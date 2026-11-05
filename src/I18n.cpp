#include "PCH.h"

#include <nlohmann/json.hpp>

#include "I18n.h"

namespace
{
	constexpr auto kTranslationFile = L"StealthFixesCN.json";

	std::unordered_map<std::string, std::string> g_table;

	// Every text the plugin asked for that has no entry. Logged once each so a
	// missing line shows up in the log without spamming it every frame.
	std::unordered_set<std::string> g_missing;

	// One line proving the shims are actually reached in game.
	std::atomic<bool> g_firstHitLogged{ false };

	std::filesystem::path PluginFolder()
	{
		std::wstring buffer(MAX_PATH, L'\0');
		HMODULE self = nullptr;
		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&I18n::Load),
				&self)) {
			return {};
		}

		for (;;) {
			const auto length = GetModuleFileNameW(self, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (length == 0) {
				return {};
			}
			if (length < buffer.size() - 1) {
				buffer.resize(length);
				break;
			}
			buffer.resize(buffer.size() * 2);
		}

		return std::filesystem::path(buffer).parent_path();
	}

	// A text made only of format specifiers, digits and punctuation ("%d",
	// "%.2f") has no words to translate; asking for an entry would just be log
	// noise every time a slider renders. The conversion letter of a specifier
	// ("%d", "%.2f") is not a word, so specifiers are skipped first.
	bool IsAsciiAlpha(unsigned char a_c)
	{
		return (a_c >= 'A' && a_c <= 'Z') || (a_c >= 'a' && a_c <= 'z');
	}

	bool HasWords(const char* a_text)
	{
		for (const char* it = a_text; *it; ++it) {
			const auto c = static_cast<unsigned char>(*it);
			if (c >= 0x80) {
				return true;
			}
			if (c == '%') {
				++it;
				while (*it && *it != '%' && !IsAsciiAlpha(static_cast<unsigned char>(*it))) {
					++it;
				}
				continue;
			}
			if (IsAsciiAlpha(c)) {
				return true;
			}
		}
		return false;
	}

	void NoteMissing(const char* a_text)
	{
		if (!a_text || !*a_text) {
			return;
		}
		if (!HasWords(a_text)) {
			return;
		}
		if (g_missing.size() >= 500) {
			return;
		}
		if (g_missing.emplace(a_text).second) {
			SKSE::log::warn("no translation: {}", a_text);
		}
	}
}

bool I18n::Load()
{
	g_table.clear();

	const auto folder = PluginFolder();
	if (folder.empty()) {
		SKSE::log::error("could not resolve the plugin folder");
		return false;
	}

	const auto path = folder / kTranslationFile;
	std::ifstream file{ path };
	if (!file.is_open()) {
		SKSE::log::error("could not open {}", path.string());
		return false;
	}

	nlohmann::json document;
	try {
		document = nlohmann::json::parse(file);
	} catch (const std::exception& e) {
		SKSE::log::error("could not parse {}: {}", path.string(), e.what());
		return false;
	}

	if (!document.is_object()) {
		SKSE::log::error("{} must contain a JSON object", path.string());
		return false;
	}

	for (const auto& [key, value] : document.items()) {
		if (!value.is_string()) {
			SKSE::log::warn("entry '{}' is not a string, skipped", key);
			continue;
		}
		g_table.emplace(key, value.get<std::string>());
	}

	SKSE::log::info("loaded {} translations from {}", g_table.size(), path.string());
	return true;
}

const char* I18n::Translate(const char* a_text)
{
	if (!a_text) {
		return nullptr;
	}

	const auto it = g_table.find(a_text);
	if (it == g_table.end()) {
		NoteMissing(a_text);
		return nullptr;
	}
	if (!g_firstHitLogged.exchange(true)) {
		SKSE::log::info("first translated text: {} -> {}", a_text, it->second);
	}
	return it->second.c_str();
}

const char* I18n::TranslateOrSelf(const char* a_text)
{
	const auto translated = Translate(a_text);
	return translated ? translated : a_text;
}

bool I18n::Ready()
{
	return !g_table.empty();
}
