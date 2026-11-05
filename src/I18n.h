#pragma once

namespace I18n
{
	// Loads StealthFixesCN.json from the folder the DLL lives in. Returns false
	// when the file is missing or unreadable; the plugin then simply leaves the
	// original English text in place.
	bool Load();

	// Exact lookup. Returns nullptr when the text has no entry.
	const char* Translate(const char* a_text);

	// Exact lookup with a fallback to the caller's own pointer.
	const char* TranslateOrSelf(const char* a_text);

	// True when a non-empty table is loaded. The hooks and the menu rename are
	// pointless without it, so they are skipped when this is false.
	bool Ready();
}
