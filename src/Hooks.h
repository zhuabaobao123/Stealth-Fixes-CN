#pragma once

namespace Hooks
{
	// Replaces StealthFixes.dll's GetProcAddress import with a shim that hands
	// back translated wrappers for the SMF imgui exports the plugin uses. Safe
	// to call repeatedly; returns true once the import is patched.
	bool Install();

	bool Installed();
}
