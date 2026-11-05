# Stealth Fixes 汉化（游戏内菜单）

Stealth Fixes（Nexus 195418）游戏内菜单的中文翻译。原 StealthFixes.dll 未做任何修改：
汉化由一个伴生 SKSE 插件 StealthFixesCN.dll 在运行时完成，译文放在外置 JSON 中。

## 安装

1. 用 MO2 安装本压缩包（或直接把“潜行修复 - 汉化”文件夹放进 mods 目录）。
2. 在 MO2 中启用“潜行修复 - 汉化”，排序放在“潜行修复 - Stealth Fixes - SKSE”之后。
3. 正常启动游戏，按 F1 打开 SKSE 菜单框架，进入“潜行修复 → 修复项”。

## 还原英文

- 菜单：删除或禁用本模组即可，原 DLL 一直是英文原文。
- 只想改某条译文：编辑 SKSE/Plugins/StealthFixesCN.json（UTF-8，格式为 "英文原文": "中文"），重启游戏。

## 说明

- 需要 SKSE Menu Framework 3.x（用到它的 RenameSection 导出）。
- 支持 SE / AE（1.5.x / 1.6.x）。
- 日志：文档\My Games\Skyrim Special Edition\SKSE\StealthFixesCN.log
- 原模组的 StealthFixes.ini 注释已另行汉化，原文件备份为 StealthFixes.ini.bak（本压缩包不含该 INI）。

## 实现

- 不改 StealthFixes.dll：运行时把它导入表里的 `GetProcAddress` 换成 shim，只有当它向 SKSE Menu Framework 索取带文字的 imgui 导出时，才返回同签名的翻译包装；其余调用原样转发。
- `igTextWrappedV` / `igSetTooltipV` 先用 `vsnprintf` 还原实际文本再查表，命中则以 `"%s"` + 自造 va_list 回调原函数；label/格式化串逐条精确查表。
- 菜单标题用 SMF 的 `RenameSection` 改为「潜行修复 / 修复项」。
- 译文为外置 JSON（149 条），删掉即还原英文；缺译时保留英文并写一条日志。

## 源码与构建

本仓库只含汉化补丁源码，不含游戏本体、StealthFixes.dll 或 SKSE 的任何文件。

需要：CMake 3.21+、Ninja、MSVC（VS 2022/2026）、vcpkg（设置 `VCPKG_ROOT`），以及 CommonLibSSE-NG 源码树（默认 `E:/CommonLibSSE-NG-alandtse`，可在 `CMakeLists.txt` 的 `COMMONLIB_SSE_FOLDER` 改）。

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --parallel
```

产物 `build/StealthFixesCN.dll`。发布时与 `data/StealthFixesCN.json` 一起放进 `Data/SKSE/Plugins/`（压缩包要镜像 `Data/` 结构）。

## 许可与源码

Copyright (C) 2026 zhuabaobao123

本程序以 GPL-3.0-or-later 发布：可再分发、可修改，但须保留同一许可，且不提供任何担保。
完整许可见 `LICENSE.txt` / `COPYING.txt`，CommonLibSSE-NG 的附加例外见 `EXCEPTIONS.md`。

源码：https://github.com/zhuabaobao123/Stealth-Fixes-CN （压缩包内 `SOURCE/` 也带一份完整源码）。
