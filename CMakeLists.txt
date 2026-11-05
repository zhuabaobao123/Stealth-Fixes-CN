cmake_minimum_required(VERSION 3.21)

project(
	StealthFixesCN
	LANGUAGES C CXX
	VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime targets. Flat (SE + AE) by default; -DENABLE_SKYRIM_VR=ON for the VR variant.
set(ENABLE_SKYRIM_SE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL " " FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLib tests in consumer" FORCE)

set(COMMONLIB_SSE_FOLDER "E:/CommonLibSSE-NG-alandtse" CACHE PATH "CommonLibSSE-NG source directory")

add_subdirectory(${COMMONLIB_SSE_FOLDER} CommonLibSSE EXCLUDE_FROM_ALL)
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)
include(${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(${PROJECT_NAME}
	AUTHOR "translation patch"
	SOURCES
		src/main.cpp
		src/Hooks.cpp
		src/I18n.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_precompile_headers(${PROJECT_NAME} PRIVATE include/PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# Sources are UTF-8 (the translation table is Chinese), and the workspace path is
# non-ASCII, so MSVC must not guess the source code page.
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /Zc:__cplusplus /Zi)
	target_link_options(${PROJECT_NAME} PRIVATE /DEBUG /OPT:REF /OPT:ICF)
endif()

# Local deploy into the MO2 instance that SKYRIM_MODS_FOLDER points at. The JSON
# rides next to the DLL because that is where the plugin looks for it.
if(DEFINED ENV{SKYRIM_MODS_FOLDER})
	set(CN_MOD_DIR "$ENV{SKYRIM_MODS_FOLDER}/潜行修复 - 汉化")
	add_custom_command(
		TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CN_MOD_DIR}/SKSE/Plugins"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:${PROJECT_NAME}>"
			"${CN_MOD_DIR}/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/data/StealthFixesCN.json"
			"${CN_MOD_DIR}/SKSE/Plugins/StealthFixesCN.json"
		COMMENT "Deploying ${PROJECT_NAME} into ${CN_MOD_DIR}"
	)
endif()
