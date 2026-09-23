cmake_minimum_required(VERSION 3.22)

project(zg_browser VERSION 0.1.0 LANGUAGES C CXX)

if(APPLE)
   enable_language(OBJC)
   enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

include(cmake/CPM.cmake)

# ---------------------------------------------------------------------------
#  Dependencies
#
#  muscle and zg_choir are pulled in source-only (DOWNLOAD_ONLY) and compiled
#  by the small library definitions below.  That is deliberate: their own
#  CMakeLists build a pile of demos/tools/tests we don't need, and (more
#  importantly) they set ABI-relevant defines (MUSCLE_USE_PTHREADS,
#  MUSCLE_USE_CPLUSPLUS17, ...) via directory-scoped flags that would *not*
#  propagate to this app -- which would silently mismatch struct layouts.
#  Here every consumer gets exactly the same defines via the muscle target.
# ---------------------------------------------------------------------------

CPMAddPackage(
   NAME muscle
   GITHUB_REPOSITORY jfriesne/muscle
   GIT_TAG v9.93
   GIT_SHALLOW TRUE
   DOWNLOAD_ONLY YES)

CPMAddPackage(
   NAME zg_choir
   GITHUB_REPOSITORY jfriesne/zg_choir
   # No release tag matches muscle v9.93 yet; this is master as of 2026-09-03.
   GIT_TAG 0069258c0628ba54973eafa94d2dd63d4140faf3
   DOWNLOAD_ONLY YES)

CPMAddPackage(
   NAME JUCE
   GITHUB_REPOSITORY juce-framework/JUCE
   GIT_TAG 8.0.15
   GIT_SHALLOW TRUE)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
#  muscle
# ---------------------------------------------------------------------------

file(GLOB MUSCLE_SRCS CONFIGURE_DEPENDS
   "${muscle_SOURCE_DIR}/dataio/*.cpp"
   "${muscle_SOURCE_DIR}/iogateway/*.cpp"
   "${muscle_SOURCE_DIR}/message/*.cpp"
   "${muscle_SOURCE_DIR}/reflector/*.cpp"
   "${muscle_SOURCE_DIR}/regex/*.cpp"
   "${muscle_SOURCE_DIR}/syslog/*.cpp"
   "${muscle_SOURCE_DIR}/system/*.cpp"
   "${muscle_SOURCE_DIR}/util/*.cpp"
   "${muscle_SOURCE_DIR}/zlib/*.cpp"
   "${muscle_SOURCE_DIR}/zlib/zlib/contrib/minizip/ioapi.c"
   "${muscle_SOURCE_DIR}/zlib/zlib/contrib/minizip/mztools.c"
   "${muscle_SOURCE_DIR}/zlib/zlib/contrib/minizip/unzip.c"
   "${muscle_SOURCE_DIR}/zlib/zlib/contrib/minizip/zip.c")
list(FILTER MUSCLE_SRCS EXCLUDE REGEX "SSL")   # we don't link OpenSSL

add_library(muscle STATIC ${MUSCLE_SRCS})

target_include_directories(muscle SYSTEM PUBLIC "${muscle_SOURCE_DIR}")
target_compile_features(muscle PUBLIC cxx_std_17)
target_compile_definitions(muscle PUBLIC
   MUSCLE_USE_CPLUSPLUS17
   MUSCLE_ENABLE_ZLIB_ENCODING
   MUSCLE_NO_EXCEPTIONS)
target_link_libraries(muscle PUBLIC Threads::Threads)

if(WIN32)
   # No system zlib/regex on Windows, so build muscle's captive copies.  Note the
   # regex sources are named explicitly:  that directory also holds files that are
   # #included by regexec.c, and two more with a main() of their own.
   file(GLOB MUSCLE_CAPTIVE_ZLIB CONFIGURE_DEPENDS "${muscle_SOURCE_DIR}/zlib/zlib/*.c")
   target_sources(muscle PRIVATE
      ${MUSCLE_CAPTIVE_ZLIB}
      "${muscle_SOURCE_DIR}/regex/regex/regcomp.c"
      "${muscle_SOURCE_DIR}/regex/regex/regerror.c"
      "${muscle_SOURCE_DIR}/regex/regex/regexec.c"
      "${muscle_SOURCE_DIR}/regex/regex/regfree.c")
   target_include_directories(muscle PRIVATE
      "${muscle_SOURCE_DIR}/zlib/zlib" "${muscle_SOURCE_DIR}/regex/regex")
   # muscle asks for most of these via #pragma comment(lib, ...), which only MSVC
   # honours -- naming them here keeps MinGW/clang-cl builds linking too.
   target_link_libraries(muscle PUBLIC ws2_32 winmm iphlpapi version)
   target_link_libraries(muscle PRIVATE Dbghelp)
else()
   target_compile_definitions(muscle PUBLIC MUSCLE_USE_PTHREADS)
   find_package(ZLIB)
   if(ZLIB_FOUND)
      target_link_libraries(muscle PUBLIC ZLIB::ZLIB)
   else()
      file(GLOB MUSCLE_CAPTIVE_ZLIB CONFIGURE_DEPENDS "${muscle_SOURCE_DIR}/zlib/zlib/*.c")
      target_sources(muscle PRIVATE ${MUSCLE_CAPTIVE_ZLIB})
      target_include_directories(muscle PRIVATE "${muscle_SOURCE_DIR}/zlib/zlib")
   endif()
   target_link_libraries(muscle PUBLIC util)   # forkpty()
endif()

if(APPLE)
   target_link_libraries(muscle PUBLIC
      "-framework CoreFoundation" "-framework SystemConfiguration" "-framework IOKit")
endif()

# muscle is third-party code; don't let its warnings drown ours out.
if(MSVC)
   target_compile_definitions(muscle PRIVATE _CRT_SECURE_NO_WARNINGS)
   target_compile_options(muscle PRIVATE /W0)
else()
   target_compile_options(muscle PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
#  zg (zg_choir)
# ---------------------------------------------------------------------------

file(GLOB ZG_SRCS CONFIGURE_DEPENDS
   "${zg_choir_SOURCE_DIR}/src/*.cpp"
   "${zg_choir_SOURCE_DIR}/src/clocksync/*.cpp"
   "${zg_choir_SOURCE_DIR}/src/connector/*.cpp"
   "${zg_choir_SOURCE_DIR}/src/discovery/*/*.cpp"
   "${zg_choir_SOURCE_DIR}/src/messagetree/*/*.cpp"
   "${zg_choir_SOURCE_DIR}/src/udp/*.cpp"
   "${zg_choir_SOURCE_DIR}/src/private/*.cpp")

if(APPLE)
   file(GLOB ZG_OBJC_SRCS CONFIGURE_DEPENDS "${zg_choir_SOURCE_DIR}/src/private/*.mm")
   list(APPEND ZG_SRCS ${ZG_OBJC_SRCS})
endif()

add_library(zg STATIC ${ZG_SRCS})
target_include_directories(zg SYSTEM PUBLIC "${zg_choir_SOURCE_DIR}/include")
target_link_libraries(zg PUBLIC muscle)

if(APPLE)
   target_link_libraries(zg PUBLIC "-framework IOKit" "-framework Foundation")
endif()

if(MSVC)
   target_compile_definitions(zg PRIVATE _CRT_SECURE_NO_WARNINGS)
   target_compile_options(zg PRIVATE /W0)
else()
   target_compile_options(zg PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
#  The application
# ---------------------------------------------------------------------------

juce_add_gui_app(zg_browser
   PRODUCT_NAME "ZG Browser"
   COMPANY_NAME "zg_browser"
   BUNDLE_ID "org.zgchoir.zgbrowser"
   VERSION ${PROJECT_VERSION}
   NEEDS_CURL FALSE
   NEEDS_WEB_BROWSER FALSE
   # macOS 15+ requires user consent before an app may send multicast/broadcast
   # traffic on the LAN, which is exactly what ZG's discovery pings are.
   PLIST_TO_MERGE "<plist><dict><key>NSLocalNetworkUsageDescription</key><string>ZG Browser discovers ZG servers on your local network.</string></dict></plist>")

target_sources(zg_browser PRIVATE
   Source/Main.cpp
   Source/MainComponent.cpp
   Source/DiscoveryComponent.cpp
   Source/BrowserComponent.cpp
   Source/MuscleNodeSource.cpp
   Source/ZGNodeSource.cpp
   Source/NodeTreeItem.cpp
   Source/MessagePanel.cpp
   Source/SearchPanel.cpp
   Source/ZGLookAndFeel.cpp)

target_compile_definitions(zg_browser PRIVATE
   JUCE_WEB_BROWSER=0
   JUCE_USE_CURL=0
   JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:zg_browser,JUCE_PRODUCT_NAME>"
   JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:zg_browser,JUCE_VERSION>")

target_link_libraries(zg_browser PRIVATE
   zg
   juce::juce_gui_basics
   PUBLIC
   juce::juce_recommended_config_flags
   juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
#  Optional: zg_choir's own test server, handy for trying the browser out.
#  Run it (in as many copies as you like) and it shows up in the systems list.
# ---------------------------------------------------------------------------

option(ZG_BROWSER_BUILD_TEST_SERVER "Also build zg_choir's tree_server test program" OFF)
if(ZG_BROWSER_BUILD_TEST_SERVER)
   add_executable(zg_tree_server "${zg_choir_SOURCE_DIR}/tests/tree_server.cpp")
   target_link_libraries(zg_tree_server PRIVATE zg)
   if(NOT MSVC)
      target_compile_options(zg_tree_server PRIVATE -w)
   endif()
endif()
