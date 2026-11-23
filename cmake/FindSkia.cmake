# FindSkia.cmake: import a Skia tree built by scripts/bootstrap_skia.sh.
#
# Inputs (cache variables):
#   ROADOS_SKIA_DIR   Skia checkout              (default: <project>/third_party/skia)
#   ROADOS_SKIA_OUT   GN output directory         (default: <skia>/out/Release-<os>-<cpu>)
#
# Provides the INTERFACE target Skia::Skia (core + svg + skshaper + skresources
# + bundled codecs). Public preprocessor defines are read from `gn desc`, so
# consumers always compile with exactly the configuration Skia was built with.

set(ROADOS_SKIA_DIR "${PROJECT_SOURCE_DIR}/third_party/skia" CACHE PATH "Skia checkout")

# Map the CMake target platform to bootstrap_skia.sh's output naming.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_sk_os iossim)
  else()
    set(_sk_os ios)
  endif()
  set(_sk_cpu "${CMAKE_OSX_ARCHITECTURES}")
elseif(ANDROID)
  set(_sk_os android)
  set(_sk_cpu "${CMAKE_ANDROID_ARCH_ABI}")
elseif(APPLE)
  set(_sk_os mac)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _sk_cpu)
elseif(UNIX)
  set(_sk_os linux)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _sk_cpu)
else()
  set(_sk_os win)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _sk_cpu)
endif()
if(_sk_cpu MATCHES "^(arm64|aarch64|arm64-v8a)$")
  set(_sk_cpu arm64)
elseif(_sk_cpu MATCHES "^(x86_64|amd64)$")
  set(_sk_cpu x64)
endif()
set(ROADOS_SKIA_OUT "${ROADOS_SKIA_DIR}/out/Release-${_sk_os}-${_sk_cpu}" CACHE PATH "Skia GN output directory")

set(_sk_libs svg skshaper skresources skia skcms png zlib expat)
if(_sk_os STREQUAL "android")
  list(APPEND _sk_libs freetype2)  # bundled FreeType for the Android font manager
endif()
set(_sk_missing "")
foreach(_l IN LISTS _sk_libs)
  if(NOT EXISTS "${ROADOS_SKIA_OUT}/lib${_l}.a")
    list(APPEND _sk_missing "lib${_l}.a")
  endif()
endforeach()

if(_sk_missing OR NOT EXISTS "${ROADOS_SKIA_DIR}/include/core/SkCanvas.h")
  set(Skia_FOUND FALSE)
  if(Skia_FIND_REQUIRED)
    message(FATAL_ERROR
      "Skia not found in ${ROADOS_SKIA_OUT} (missing: ${_sk_missing}).\n"
      "Run:  scripts/bootstrap_skia.sh Release")
  endif()
  return()
endif()

# Public defines, straight from the GN build graph.
set(_sk_gn "${ROADOS_SKIA_DIR}/bin/gn")
set(_sk_defines "")
foreach(_cfg //:skia_public //modules/svg:public_config //modules/skshaper:public_config)
  execute_process(
    COMMAND "${_sk_gn}" desc "${ROADOS_SKIA_OUT}" "${_cfg}" defines
    WORKING_DIRECTORY "${ROADOS_SKIA_DIR}"
    OUTPUT_VARIABLE _out RESULT_VARIABLE _rc ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "gn desc ${_cfg} failed in ${ROADOS_SKIA_OUT}")
  endif()
  string(REPLACE "\n" ";" _out "${_out}")
  list(APPEND _sk_defines ${_out})
endforeach()
list(REMOVE_DUPLICATES _sk_defines)

# SK_DEBUG/SK_RELEASE changes class layouts in Skia headers: it must match the build.
file(READ "${ROADOS_SKIA_OUT}/args.gn" _sk_args)
if(_sk_args MATCHES "is_debug *= *true")
  list(APPEND _sk_defines SK_DEBUG)
else()
  list(APPEND _sk_defines SK_RELEASE)
endif()

add_library(Skia::Skia INTERFACE IMPORTED GLOBAL)
foreach(_l IN LISTS _sk_libs)
  add_library(Skia::${_l} STATIC IMPORTED GLOBAL)
  set_target_properties(Skia::${_l} PROPERTIES IMPORTED_LOCATION "${ROADOS_SKIA_OUT}/lib${_l}.a")
  target_link_libraries(Skia::Skia INTERFACE Skia::${_l})
endforeach()
target_include_directories(Skia::Skia SYSTEM INTERFACE "${ROADOS_SKIA_DIR}")
target_compile_definitions(Skia::Skia INTERFACE ${_sk_defines})

if(_sk_os STREQUAL "ios" OR _sk_os STREQUAL "iossim")
  target_link_libraries(Skia::Skia INTERFACE
    "-framework CoreFoundation" "-framework CoreGraphics" "-framework CoreText")
elseif(APPLE)
  target_link_libraries(Skia::Skia INTERFACE
    "-framework CoreFoundation" "-framework CoreGraphics" "-framework CoreText"
    "-framework CoreServices" "-framework ApplicationServices" "-framework AppKit")
elseif(ANDROID)
  target_link_libraries(Skia::Skia INTERFACE log ${CMAKE_DL_LIBS})
elseif(UNIX)
  find_package(Freetype REQUIRED)
  find_package(Fontconfig REQUIRED)
  find_package(Threads REQUIRED)
  target_link_libraries(Skia::Skia INTERFACE Freetype::Freetype Fontconfig::Fontconfig Threads::Threads ${CMAKE_DL_LIBS})
endif()

execute_process(COMMAND git -C "${ROADOS_SKIA_DIR}" rev-parse --short=12 HEAD
  OUTPUT_VARIABLE Skia_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
set(Skia_FOUND TRUE)
message(STATUS "Skia: ${ROADOS_SKIA_OUT} (rev ${Skia_REVISION})")
