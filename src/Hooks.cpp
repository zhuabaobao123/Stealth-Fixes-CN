#include "PCH.h"

#include "Hooks.h"
#include "I18n.h"

// StealthFixes resolves the SKSE Menu Framework cimgui exports on its own, with
// GetProcAddress, and caches them per widget. The DLL itself is closed source,
// so the only place to reach the text is the pointer it gets back: we replace
// its GetProcAddress import with a shim that returns our own wrappers for the
// exports that carry text, and forwards everything else untouched.
//
// The original DLL is never modified on disk, and removing this plugin restores
// the English menu.

namespace
{
	using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);

	struct ImVec2
	{
		float x;
		float y;
	};

	// cimgui signatures. On x64 the calling convention is the same for all of
	// them, so a plain function pointer is exact enough.
	using TextVFn = void (*)(const char*, va_list);
	using LabelFn = void (*)(const char*);
	using ButtonFn = bool (*)(const char*, ImVec2);
	using CheckboxFn = bool (*)(const char*, bool*);
	using HeaderFn = bool (*)(const char*, int);
	using SliderFloatFn = bool (*)(const char*, float*, float, float, const char*, int);
	using SliderIntFn = bool (*)(const char*, int*, int, int, const char*, int);

	constexpr std::size_t kFormatBufferSize = 4096;
	constexpr std::size_t kMaxShims = 8;

	struct ShimEntry
	{
		const char* name;
		void* function;
	};

	std::atomic<bool> g_installed{ false };
	HMODULE g_stealthFixes{ nullptr };
	HMODULE g_menuFramework{ nullptr };
	GetProcAddressFn g_realGetProcAddress{ nullptr };

	ShimEntry g_shims[kMaxShims]{};
	std::size_t g_shimCount{ 0 };

	TextVFn g_textWrapped{ nullptr };
	TextVFn g_setTooltip{ nullptr };
	LabelFn g_separatorText{ nullptr };
	ButtonFn g_button{ nullptr };
	CheckboxFn g_checkbox{ nullptr };
	HeaderFn g_collapsingHeader{ nullptr };
	SliderFloatFn g_sliderFloat{ nullptr };
	SliderIntFn g_sliderInt{ nullptr };

	// The original takes its text through va_arg. Hand it "%s" plus a one-slot
	// va_list that holds our translated pointer.
	void CallTranslated(TextVFn a_real, const char* a_fmt, va_list a_args)
	{
		if (!a_real) {
			return;
		}
		if (!a_fmt) {
			a_real(a_fmt, a_args);
			return;
		}

		char buffer[kFormatBufferSize];
		va_list copy;
		va_copy(copy, a_args);
		const int length = std::vsnprintf(buffer, sizeof(buffer), a_fmt, copy);
		va_end(copy);

		if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(buffer)) {
			a_real(a_fmt, a_args);
			return;
		}

		const char* translated = I18n::Translate(buffer);
		if (!translated) {
			a_real(a_fmt, a_args);
			return;
		}

		static_assert(sizeof(va_list) == sizeof(std::uintptr_t));
		std::uintptr_t slot = reinterpret_cast<std::uintptr_t>(translated);
		va_list fake = reinterpret_cast<va_list>(&slot);
		a_real("%s", fake);
	}

	void ShimTextWrappedV(const char* a_fmt, va_list a_args)
	{
		CallTranslated(g_textWrapped, a_fmt, a_args);
	}

	void ShimSetTooltipV(const char* a_fmt, va_list a_args)
	{
		CallTranslated(g_setTooltip, a_fmt, a_args);
	}

	void ShimSeparatorText(const char* a_label)
	{
		g_separatorText(I18n::TranslateOrSelf(a_label));
	}

	bool ShimButton(const char* a_label, ImVec2 a_size)
	{
		return g_button(I18n::TranslateOrSelf(a_label), a_size);
	}

	bool ShimCheckbox(const char* a_label, bool* a_value)
	{
		return g_checkbox(I18n::TranslateOrSelf(a_label), a_value);
	}

	bool ShimCollapsingHeader(const char* a_label, int a_flags)
	{
		return g_collapsingHeader(I18n::TranslateOrSelf(a_label), a_flags);
	}

	bool ShimSliderFloat(const char* a_label, float* a_value, float a_min, float a_max, const char* a_format, int a_flags)
	{
		return g_sliderFloat(
			I18n::TranslateOrSelf(a_label),
			a_value,
			a_min,
			a_max,
			I18n::TranslateOrSelf(a_format),
			a_flags);
	}

	bool ShimSliderInt(const char* a_label, int* a_value, int a_min, int a_max, const char* a_format, int a_flags)
	{
		return g_sliderInt(
			I18n::TranslateOrSelf(a_label),
			a_value,
			a_min,
			a_max,
			I18n::TranslateOrSelf(a_format),
			a_flags);
	}

	FARPROC WINAPI GetProcAddressShim(HMODULE a_module, LPCSTR a_name)
	{
		// Ordinal lookups pass a small integer in the pointer, so only a real
		// string can be compared.
		if (a_name && !IS_INTRESOURCE(a_name) && a_module && a_module == g_menuFramework) {
			for (std::size_t i = 0; i < g_shimCount; ++i) {
				if (std::strcmp(a_name, g_shims[i].name) == 0) {
					return reinterpret_cast<FARPROC>(g_shims[i].function);
				}
			}
		}
		return g_realGetProcAddress(a_module, a_name);
	}

	std::uintptr_t* FindIatSlot(HMODULE a_module, std::string_view a_function)
	{
		if (!a_module) {
			return nullptr;
		}

		const auto base = reinterpret_cast<std::uint8_t*>(a_module);
		const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
			return nullptr;
		}

		const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE) {
			return nullptr;
		}

		const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (directory.VirtualAddress == 0 || directory.Size == 0) {
			return nullptr;
		}

		const auto descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
		for (auto descriptor = descriptors; descriptor->Name != 0; ++descriptor) {
			const auto nameThunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(
				base + (descriptor->OriginalFirstThunk ? descriptor->OriginalFirstThunk : descriptor->FirstThunk));
			auto iatThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);

			for (std::size_t i = 0; nameThunk[i].u1.AddressOfData != 0; ++i) {
				if (IMAGE_SNAP_BY_ORDINAL64(nameThunk[i].u1.Ordinal)) {
					continue;
				}
				const auto imported = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + nameThunk[i].u1.AddressOfData);
				if (a_function == imported->Name) {
					return reinterpret_cast<std::uintptr_t*>(&iatThunk[i].u1.Function);
				}
			}
		}

		return nullptr;
	}

	void AddShim(const char* a_name, void* a_shim, void* a_original)
	{
		if (!a_original) {
			SKSE::log::warn("SKSE Menu Framework has no export {}, left untranslated", a_name);
			return;
		}
		if (g_shimCount >= kMaxShims) {
			return;
		}
		g_shims[g_shimCount++] = ShimEntry{ a_name, a_shim };
	}

	void ResolveOriginals()
	{
		const auto resolve = [](const char* a_name) -> void* {
			return reinterpret_cast<void*>(g_realGetProcAddress(g_menuFramework, a_name));
		};

		g_textWrapped = reinterpret_cast<TextVFn>(resolve("igTextWrappedV"));
		g_setTooltip = reinterpret_cast<TextVFn>(resolve("igSetTooltipV"));
		g_separatorText = reinterpret_cast<LabelFn>(resolve("igSeparatorText"));
		g_button = reinterpret_cast<ButtonFn>(resolve("igButton"));
		g_checkbox = reinterpret_cast<CheckboxFn>(resolve("igCheckbox"));
		g_collapsingHeader = reinterpret_cast<HeaderFn>(resolve("igCollapsingHeader_TreeNodeFlags"));
		g_sliderFloat = reinterpret_cast<SliderFloatFn>(resolve("igSliderFloat"));
		g_sliderInt = reinterpret_cast<SliderIntFn>(resolve("igSliderInt"));

		g_shimCount = 0;
		AddShim("igTextWrappedV", reinterpret_cast<void*>(&ShimTextWrappedV), reinterpret_cast<void*>(g_textWrapped));
		AddShim("igSetTooltipV", reinterpret_cast<void*>(&ShimSetTooltipV), reinterpret_cast<void*>(g_setTooltip));
		AddShim("igSeparatorText", reinterpret_cast<void*>(&ShimSeparatorText), reinterpret_cast<void*>(g_separatorText));
		AddShim("igButton", reinterpret_cast<void*>(&ShimButton), reinterpret_cast<void*>(g_button));
		AddShim("igCheckbox", reinterpret_cast<void*>(&ShimCheckbox), reinterpret_cast<void*>(g_checkbox));
		AddShim("igCollapsingHeader_TreeNodeFlags", reinterpret_cast<void*>(&ShimCollapsingHeader), reinterpret_cast<void*>(g_collapsingHeader));
		AddShim("igSliderFloat", reinterpret_cast<void*>(&ShimSliderFloat), reinterpret_cast<void*>(g_sliderFloat));
		AddShim("igSliderInt", reinterpret_cast<void*>(&ShimSliderInt), reinterpret_cast<void*>(g_sliderInt));
	}
}

bool Hooks::Install()
{
	if (g_installed.load(std::memory_order_acquire)) {
		return true;
	}

	// Without a table every shim would only add a lookup and a log line.
	if (!I18n::Ready()) {
		return false;
	}

	if (!g_stealthFixes) {
		g_stealthFixes = GetModuleHandleW(L"StealthFixes.dll");
	}
	if (!g_stealthFixes) {
		return false;
	}

	if (!g_menuFramework) {
		g_menuFramework = GetModuleHandleW(L"SKSEMenuFramework.dll");
	}
	if (!g_menuFramework) {
		SKSE::log::warn("SKSE Menu Framework is not loaded; Stealth Fixes has no menu to translate");
		return false;
	}

	const auto slot = FindIatSlot(g_stealthFixes, "GetProcAddress");
	if (!slot) {
		SKSE::log::error("could not find GetProcAddress in StealthFixes.dll's import table");
		return false;
	}

	const auto hookAddress = reinterpret_cast<std::uintptr_t>(&GetProcAddressShim);
	if (*slot == hookAddress) {
		g_installed.store(true, std::memory_order_release);
		return true;
	}

	if (!g_realGetProcAddress) {
		const auto kernel32 = GetModuleHandleW(L"kernel32.dll");
		g_realGetProcAddress = reinterpret_cast<GetProcAddressFn>(::GetProcAddress(kernel32, "GetProcAddress"));
	}
	if (!g_realGetProcAddress) {
		SKSE::log::error("could not resolve the real GetProcAddress");
		return false;
	}

	ResolveOriginals();
	if (g_shimCount == 0) {
		SKSE::log::error("none of the SKSE Menu Framework exports could be resolved");
		return false;
	}

	DWORD oldProtect = 0;
	if (!VirtualProtect(slot, sizeof(std::uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect)) {
		SKSE::log::error("VirtualProtect failed on the GetProcAddress import ({})", GetLastError());
		return false;
	}
	const auto previous = *slot;
	*slot = hookAddress;
	DWORD ignored = 0;
	VirtualProtect(slot, sizeof(std::uintptr_t), oldProtect, &ignored);
	FlushInstructionCache(GetCurrentProcess(), slot, sizeof(std::uintptr_t));

	if (*slot != hookAddress) {
		SKSE::log::error("the GetProcAddress import did not keep the shim");
		return false;
	}

	SKSE::log::info(
		"patched StealthFixes' GetProcAddress import ({:x} -> {:x}), {} text shims active",
		previous,
		hookAddress,
		g_shimCount);

	g_installed.store(true, std::memory_order_release);
	return true;
}

bool Hooks::Installed()
{
	return g_installed.load(std::memory_order_acquire);
}
