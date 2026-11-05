#include "PCH.h"

#include "Hooks.h"
#include "I18n.h"

namespace
{
	void SetupLog()
	{
		const auto logsFolder = SKSE::log::log_directory();
		if (!logsFolder) {
			SKSE::stl::report_and_fail("no log directory");
		}

		const auto path = *logsFolder / "StealthFixesCN.log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
		auto logger = std::make_shared<spdlog::logger>("log", std::move(sink));
		spdlog::set_default_logger(std::move(logger));
		spdlog::set_level(spdlog::level::info);
		spdlog::flush_on(spdlog::level::info);
	}

	// The menu page is registered by StealthFixes on kPostPostLoad, so at
	// kDataLoaded the section exists. Rename queues a mutation; the framework
	// applies it on the render thread. If the section is somehow not there yet,
	// retry once per frame for a while instead of giving up on the first try.
	namespace Menu
	{
		using RenameSectionFn = bool (*)(const char*, const char*);

		enum class Step
		{
			kChild,
			kRoot,
			kDone,
			kFailed
		};

		constexpr int kMaxAttempts = 600;

		Step g_step{ Step::kChild };
		int g_attempts{ 0 };
		RenameSectionFn g_renameSection{ nullptr };

		bool Resolve()
		{
			if (g_renameSection) {
				return true;
			}

			const auto framework = GetModuleHandleW(L"SKSEMenuFramework.dll");
			if (!framework) {
				g_step = Step::kFailed;
				SKSE::log::warn("SKSE Menu Framework is not loaded; the menu keeps its English names");
				return false;
			}

			g_renameSection = reinterpret_cast<RenameSectionFn>(::GetProcAddress(framework, "RenameSection"));
			if (!g_renameSection) {
				g_step = Step::kFailed;
				SKSE::log::error("SKSE Menu Framework has no RenameSection export; the menu keeps its English names");
				return false;
			}
			return true;
		}

		bool TryRename()
		{
			if (g_step == Step::kDone || g_step == Step::kFailed) {
				return true;
			}
			if (!Resolve()) {
				return true;
			}

			// Child first: renaming the root would change the child's path.
			if (g_step == Step::kChild) {
				if (!g_renameSection("Stealth Fixes/Fixes", "修复项")) {
					return false;
				}
				g_step = Step::kRoot;
			}

			if (g_step == Step::kRoot) {
				if (!g_renameSection("Stealth Fixes", "潜行修复")) {
					return false;
				}
				g_step = Step::kDone;
				SKSE::log::info("renamed the Stealth Fixes menu section");
			}

			return true;
		}

		void Tick();

		void QueueNext()
		{
			if (const auto tasks = SKSE::GetTaskInterface()) {
				tasks->AddTask([]() { Tick(); });
			}
		}

		void Tick()
		{
			if (TryRename()) {
				return;
			}
			if (++g_attempts >= kMaxAttempts) {
				g_step = Step::kFailed;
				SKSE::log::warn("gave up renaming the Stealth Fixes menu after {} attempts", g_attempts);
				return;
			}
			QueueNext();
		}

		void Begin()
		{
			// Renaming the page without a translation table would leave a
			// Chinese section name over English content.
			if (!I18n::Ready()) {
				return;
			}
			if (!TryRename()) {
				QueueNext();
			}
		}
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			Hooks::Install();
			break;
		case SKSE::MessagingInterface::kPostPostLoad:
			Hooks::Install();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Hooks::Install();
			Menu::Begin();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SetupLog();
	SKSE::log::info("StealthFixesCN loading");

	// log = true would replace SetupLog's logger with CommonLib's own.
	SKSE::Init(a_skse, SKSE::InitInfo{ .log = false });

	if (I18n::Load()) {
		Hooks::Install();
	} else {
		SKSE::log::warn("no translation table, Stealth Fixes is left in English");
	}

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		SKSE::log::error("failed to register the message listener");
		return false;
	}

	SKSE::log::info("StealthFixesCN loaded");
	return true;
}
