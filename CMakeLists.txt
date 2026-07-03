cmake_minimum_required(VERSION 3.18)
project(nft_marker_creator LANGUAGES C CXX)

# C++17 (not the WASM build's C++11): the vendored Eigen in FreakMatcher uses
# C++14+ features (e.g. std::integer_sequence) that fail to compile with modern
# native clang/GCC at C++11. Our own code is C++11-compatible, so bumping is safe.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The vendored ARToolKit code crashes at import under clang-21's default Release
# flags (-O3 -DNDEBUG). Set the Release flags directly (unambiguous, unlike
# appending via target options) to -O2; keep asserts (no NDEBUG). GNU/clang only
# (MSVC uses /O2 and is unaffected).
if(NOT MSVC)
  set(CMAKE_C_FLAGS_RELEASE "-O2")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2")
endif()

find_package(pybind11 CONFIG REQUIRED)

# JPEG + zlib. Linux/macOS use system libraries (manylinux/auditwheel bundle
# them). Windows CI runners have neither, so fetch and STATICALLY link both — a
# self-contained _core.pyd needs no delvewheel/DLL bundling.
if(WIN32)
  include(FetchContent)
  # Some vendored deps still declare cmake_minimum_required < 3.5, which CMake 4
  # rejects; this keeps them building.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

  # zlib integrates via add_subdirectory (FetchContent) fine.
  set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1)
  FetchContent_MakeAvailable(zlib)
  set(NFT_ZLIB_TARGET zlibstatic)

  # libjpeg-turbo refuses add_subdirectory (its CMakeLists errors out), so build
  # it as a standalone ExternalProject at build time and consume the installed
  # static lib via an imported target.
  include(ExternalProject)
  set(_jpeg_prefix ${CMAKE_BINARY_DIR}/_deps/jpeg-install)
  set(_jpeg_lib ${_jpeg_prefix}/lib/jpeg-static.lib)
  set(_jpeg_inc ${_jpeg_prefix}/include)
  ExternalProject_Add(jpeg_ext
    GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
    GIT_TAG 3.0.4
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${_jpeg_prefix}
      -DENABLE_SHARED=OFF -DENABLE_STATIC=ON
      -DWITH_TURBOJPEG=OFF -DWITH_SIMD=OFF  # WITH_SIMD=OFF avoids a NASM dep
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    BUILD_BYPRODUCTS ${_jpeg_lib})
  file(MAKE_DIRECTORY ${_jpeg_inc})
  add_library(nft_jpeg STATIC IMPORTED GLOBAL)
  set_target_properties(nft_jpeg PROPERTIES
    IMPORTED_LOCATION ${_jpeg_lib}
    INTERFACE_INCLUDE_DIRECTORIES ${_jpeg_inc})
  set(NFT_JPEG_TARGET nft_jpeg)
  set(NFT_JPEG_EXTPROJ jpeg_ext)  # _core must wait for this to build

  set(NFT_DEP_INCLUDES ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
else()
  find_package(JPEG REQUIRED)
  find_package(ZLIB REQUIRED)
  find_package(Threads REQUIRED)
  set(NFT_JPEG_TARGET JPEG::JPEG)
  set(NFT_ZLIB_TARGET ZLIB::ZLIB)
endif()

set(REPO ${CMAKE_CURRENT_SOURCE_DIR})

# WebARKitLib (AR2/KPM/AR/ARICP/ARUtil sources). Use the in-repo submodule when
# present (dev / CI checkout with submodules); otherwise fetch the pinned release
# so a source build from the sdist works without the submodule.
if(EXISTS ${REPO}/emscripten/WebARKitLib/lib/SRC/AR2/imageSet.c)
  set(WAK ${REPO}/emscripten/WebARKitLib)
  message(STATUS "nft_marker_creator: using in-repo WebARKitLib submodule")
else()
  include(FetchContent)
  FetchContent_Declare(
    webarkitlib
    GIT_REPOSITORY https://github.com/webarkit/WebARKitLib.git
    GIT_TAG 0.8.0
  )
  # Only the sources are needed; do not add_subdirectory WebARKitLib's own build.
  FetchContent_GetProperties(webarkitlib)
  if(NOT webarkitlib_POPULATED)
    FetchContent_Populate(webarkitlib)
  endif()
  set(WAK ${webarkitlib_SOURCE_DIR})
  message(STATUS "nft_marker_creator: fetched WebARKitLib 0.8.0")
endif()
set(WSRC ${WAK}/lib/SRC)

# ARUtil: log + thread_sub (threadEndSignal, referenced by AR2/tracking2d.c) +
# file_utils (defines cat(), called by AR/arPattLoad.c) and its minizip chain.
# ioapi.c hardcodes glibc fopen64/fseeko64/ftello64 -> needs _LARGEFILE64_SOURCE
# (set below). Linux-first source set; macOS/Windows will need a leaner one.
set(AR_SOURCES
  ${WSRC}/ARUtil/log.c
  ${WSRC}/ARUtil/thread_sub.c
  ${WSRC}/ARUtil/file_utils.c
  ${WSRC}/ARUtil/unzip.c
  ${WSRC}/ARUtil/ioapi.c
  ${WSRC}/ARUtil/crypt.c
  ${WSRC}/ARUtil/zip.c
)

# AR core (libAR) + ICP (libARICP): the matrix/param/util/pattern/icp symbols the
# AR2 tracker files reference. Exclude paramGL.c (needs OpenGL).
file(GLOB AR_MATRIX ${WSRC}/AR/m*.c ${WSRC}/AR/v*.c)
file(GLOB AR_PARAM ${WSRC}/AR/param*.c)
list(REMOVE_ITEM AR_PARAM ${WSRC}/AR/paramGL.c)
file(GLOB ARICP_SOURCES ${WSRC}/ARICP/icp*.c)
set(ARCORE_SOURCES
  ${AR_MATRIX} ${AR_PARAM} ${ARICP_SOURCES}
  ${WSRC}/AR/arUtil.c
  ${WSRC}/AR/arPattLoad.c
)
set(AR2_SOURCES
  ${WSRC}/AR2/handle.c ${WSRC}/AR2/imageSet.c ${WSRC}/AR2/jpeg.c
  ${WSRC}/AR2/marker.c ${WSRC}/AR2/featureMap.c ${WSRC}/AR2/featureSet.c
  ${WSRC}/AR2/selectTemplate.c ${WSRC}/AR2/surface.c ${WSRC}/AR2/tracking.c
  ${WSRC}/AR2/tracking2d.c ${WSRC}/AR2/matching.c ${WSRC}/AR2/matching2.c
  ${WSRC}/AR2/template.c ${WSRC}/AR2/searchPoint.c ${WSRC}/AR2/coord.c
  ${WSRC}/AR2/util.c
)
set(KPM_SOURCES
  ${WSRC}/KPM/kpmHandle.cpp ${WSRC}/KPM/kpmRefDataSet.cpp
  ${WSRC}/KPM/kpmMatching.cpp ${WSRC}/KPM/kpmResult.cpp ${WSRC}/KPM/kpmUtil.cpp
  ${WSRC}/KPM/kpmFopen.c
  ${WSRC}/KPM/FreakMatcher/detectors/DoG_scale_invariant_detector.cpp
  ${WSRC}/KPM/FreakMatcher/detectors/gaussian_scale_space_pyramid.cpp
  ${WSRC}/KPM/FreakMatcher/detectors/gradients.cpp
  ${WSRC}/KPM/FreakMatcher/detectors/orientation_assignment.cpp
  ${WSRC}/KPM/FreakMatcher/detectors/pyramid.cpp
  ${WSRC}/KPM/FreakMatcher/facade/visual_database_facade.cpp
  ${WSRC}/KPM/FreakMatcher/matchers/hough_similarity_voting.cpp
  ${WSRC}/KPM/FreakMatcher/matchers/freak.cpp
  ${WSRC}/KPM/FreakMatcher/framework/date_time.cpp
  ${WSRC}/KPM/FreakMatcher/framework/image.cpp
  ${WSRC}/KPM/FreakMatcher/framework/logger.cpp
  ${WSRC}/KPM/FreakMatcher/framework/timers.cpp
)

pybind11_add_module(_core
  ${REPO}/python/src/markerCreator_py.cpp
  ${REPO}/emscripten/markerCreator.cpp
  ${AR_SOURCES} ${AR2_SOURCES} ${KPM_SOURCES} ${ARCORE_SOURCES}
)

# On Windows, libjpeg-turbo is an ExternalProject; _core must wait for it.
if(NFT_JPEG_EXTPROJ)
  add_dependencies(_core ${NFT_JPEG_EXTPROJ})
endif()

target_include_directories(_core PRIVATE
  ${WAK}/include
  ${REPO}/emscripten
  ${WSRC}/KPM/FreakMatcher
  ${NFT_DEP_INCLUDES}
)
# HAVE_THREADING enables the bounded worker pool in markerCreator.cpp.
target_compile_definitions(_core PRIVATE HAVE_NFT HAVE_THREADING)
if(WIN32)
  # thread_sub.c uses Win32 threads/CRITICAL_SECTION instead of pthreads.
  # NOMINMAX: <windows.h> defines min/max macros that break std::min({...}).
  target_compile_definitions(_core PRIVATE ARUTIL_DISABLE_PTHREADS NOMINMAX)
else()
  # minizip ioapi.c uses fopen64/fseeko64/ftello64, which glibc only declares
  # with these (needed on manylinux). On Windows ioapi.h maps them to _fseeki64.
  target_compile_definitions(_core PRIVATE _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
endif()

if(NOT MSVC)
  target_compile_options(_core PRIVATE -fno-strict-aliasing -fno-strict-overflow)
  # Hide all non-pybind symbols. pybind11 sets CXX_VISIBILITY_PRESET but not
  # C_VISIBILITY_PRESET, so the vendored C symbols (AR2/jpeg/zlib/...) would stay
  # exported and could interpose over auditwheel's bundled libjpeg. Good hygiene
  # for a Python extension (only PyInit__core needs to be visible). On MSVC,
  # pybind11 uses __declspec(dllexport) so only the module entry is exported.
  set_target_properties(_core PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Link with the old 2-segment layout (-z noseparate-code). binutils' default
  # separate-code layout produces 4 PT_LOAD segments that auditwheel's patchelf
  # 0.17.2 corrupts when it grows the dynamic section (longer bundled-libjpeg
  # soname + RUNPATH), leaving DT_INIT (offset 0x25c) non-executable and crashing
  # at import. The merged layout survives patchelf's rewrite. (GNU ld only.)
  target_link_options(_core PRIVATE "LINKER:-z,noseparate-code")
endif()

target_link_libraries(_core PRIVATE ${NFT_JPEG_TARGET} ${NFT_ZLIB_TARGET})
if(NOT WIN32)
  target_link_libraries(_core PRIVATE Threads::Threads)
endif()

install(TARGETS _core DESTINATION nft_marker_creator)
