# Build the vendored Steamhammer bot as a Linux shared module.
#
# This file is integration glue and lives OUTSIDE 3rdparty/Steamhammer, so the
# upstream tree stays unmodified (see its README.upstream.md).
#
# Usage from the OpenBW repo root:
#   cmake -S 3rdparty/Steamhammer-integration -B build-steamhammer \
#         -DBWAPI_ROOT=<path to the bwapi tree that provides libBWAPI>
#   cmake --build build-steamhammer -j
# Output: build-steamhammer/libSteamhammer.so
cmake_minimum_required(VERSION 3.16)
project(SteamhammerModule LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Where the vendored bot lives, relative to this file.
set(SH_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../Steamhammer")

if(NOT BWAPI_ROOT)
  message(FATAL_ERROR "pass -DBWAPI_ROOT=<dir containing bwapi/include and libBWAPI.so>")
endif()

file(GLOB_RECURSE SH_SRC   "${SH_ROOT}/Steamhammer/Source/*.cpp")
file(GLOB_RECURSE BOSS_SRC "${SH_ROOT}/BOSS/source/*.cpp")

# Upstream excludes these two; keep the same rule (Dll.cpp is Windows-only,
# BOSS_main.cpp is a standalone harness). Dll_posix.cpp replaces Dll.cpp.
list(FILTER SH_SRC EXCLUDE REGEX "/Dll\\.cpp$")
list(FILTER BOSS_SRC EXCLUDE REGEX "/BOSS_main\\.cpp$")

# Optional debug GUI (StarCraftGUI / GUITools) is not needed to play and pulls in
# headers that are not shipped for Linux. Disable it rather than shipping a GUI
# dependency we never use.
add_definitions(-DBOSS_GUI_DISABLED)
list(FILTER BOSS_SRC EXCLUDE REGEX "/StarCraftGUI\\.cpp$")

# BOSS/source/deprecated holds a second, Windows-only copy of BOSSAssert.cpp
# (localtime_s, vsnprintf_s, SDL-based GUITools). It is not part of the bot and
# shadows the real BOSSAssert.cpp, so exclude the whole directory.
list(FILTER BOSS_SRC EXCLUDE REGEX "/deprecated/")

add_library(Steamhammer SHARED
  ${SH_SRC}
  ${BOSS_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/Dll_posix.cpp)

target_include_directories(Steamhammer PRIVATE
  "${SH_ROOT}/Steamhammer/Source"
  "${SH_ROOT}/BOSS/source"
  "${BWAPI_ROOT}/bwapi/include")

# The harness puts libBWAPI.so in <harness>/build/lib, while a standalone bwapi
# tree would have it in <bwapi>/build/lib. Accept either, or an explicit path.
if(BWAPI_LIB_PATH)
  set(BWAPI_LIB "${BWAPI_LIB_PATH}")
else()
  find_library(BWAPI_LIB NAMES BWAPILIB
    PATHS "${BWAPI_ROOT}/build/lib" "${BWAPI_ROOT}/build"
          "${BWAPI_ROOT}/../build/lib" "${BWAPI_ROOT}/../../build/lib"
    NO_DEFAULT_PATH)
endif()
if(NOT BWAPI_LIB)
  message(FATAL_ERROR "libBWAPI.so not found; pass -DBWAPI_LIB_PATH=<path to libBWAPILIB.so>")
endif()
target_link_libraries(Steamhammer PRIVATE "${BWAPI_LIB}")

# dlsym() must be able to see newAIModule/gameInit.
set_target_properties(Steamhammer PROPERTIES
  CXX_VISIBILITY_PRESET default
  VISIBILITY_INLINES_HIDDEN OFF)
