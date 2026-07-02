cmake_minimum_required(VERSION 3.18)
project(nft_marker_creator LANGUAGES C CXX)

# C++17 (not the WASM build's C++11): the vendored Eigen in FreakMatcher uses
# C++14+ features (e.g. std::integer_sequence) that fail to compile with modern
# native clang/GCC at C++11. Our own code is C++11-compatible, so bumping is safe.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(pybind11 CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

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

target_include_directories(_core PRIVATE
  ${WAK}/include
  ${REPO}/emscripten
  ${WSRC}/KPM/FreakMatcher
)
# HAVE_THREADING enables the bounded worker pool in markerCreator.cpp.
# _LARGEFILE64_SOURCE/_FILE_OFFSET_BITS: minizip ioapi.c uses fopen64/fseeko64/
# ftello64, which glibc only declares with these (needed on manylinux).
target_compile_definitions(_core PRIVATE HAVE_NFT HAVE_THREADING
  _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
target_link_libraries(_core PRIVATE JPEG::JPEG ZLIB::ZLIB Threads::Threads)

install(TARGETS _core DESTINATION nft_marker_creator)
