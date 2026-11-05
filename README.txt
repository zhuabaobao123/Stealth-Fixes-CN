Stealth Fixes - Simplified Chinese menu translation
===================================================

Copyright (C) 2026 zhuabaobao123

License
-------
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

The full license text is included in LICENSE.txt and COPYING.txt in this
archive. The GPL-3.0 text is also available at:
https://www.gnu.org/licenses/gpl-3.0.txt

This plugin links CommonLibSSE-NG (GPL-3.0-or-later) and is therefore released
under the same license. EXCEPTIONS.md carries the CommonLibSSE-NG modding
exception.

Source code
-----------
All source code is included in the SOURCE folder of this archive and is also
published at:
https://github.com/zhuabaobao123/Stealth-Fixes-CN

What this mod does
------------------
A companion SKSE plugin that translates the in-game menu of "Stealth Fixes"
(Nexus 195418). The original StealthFixes.dll is not modified: the plugin
replaces its GetProcAddress import at runtime and returns translated wrappers
for the text-carrying SKSE Menu Framework imgui exports.

Translation data lives in SKSE/Plugins/StealthFixesCN.json. Delete that file or
disable this mod to get the original English menu back.

Requirements
------------
- SKSE64 (SE 1.5.97 / AE 1.6.x)
- SKSE Menu Framework 3.x
- Stealth Fixes (Nexus 195418)

Installation
------------
Install with Mod Organizer 2 (or extract into Data/) and enable this mod after
"Stealth Fixes". Press F1 in game and open "Stealth Fixes -> Fixes".

Build
-----
See README.md / SOURCE/README.md. Requires CMake 3.21+, Ninja, MSVC and vcpkg,
plus a CommonLibSSE-NG source tree.

Credits
-------
- Stealth Fixes by its original author.
- SKSE Menu Framework by QTR-Modding.
- CommonLibSSE-NG by alandtse and contributors.
- This translation patch: zhuabaobao123.
