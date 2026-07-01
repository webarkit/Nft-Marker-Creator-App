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

# Repo layout: this file is in python/, the C/C++ sources live under emscripten/.
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(WAK ${REPO}/emscripten/WebARKitLib)
set(WSRC ${WAK}/lib/SRC)

# Same source list the WASM build (tools/makem.js) compiles, minus the
# emscripten binding (we provide a pybind11 one) and markerCompress (.zft only).
set(AR_SOURCES
  ${WSRC}/ARUtil/log.c
  ${WSRC}/ARUtil/file_utils.c
  # thread_sub.c defines threadEndSignal etc. referenced by AR2/tracking2d.c.
  # makem.js omits it (emscripten links leniently); native linking is strict.
  ${WSRC}/ARUtil/thread_sub.c
  # unzip.c/ioapi.c (minizip) provide unz* symbols referenced by file_utils.c;
  # crypt.c/zip.c provide the crypt + zip* writer symbols they pull in.
  ${WSRC}/ARUtil/unzip.c
  ${WSRC}/ARUtil/ioapi.c
  ${WSRC}/ARUtil/crypt.c
  ${WSRC}/ARUtil/zip.c
)

# AR core (libAR) + ICP (libARICP). ARToolKit5's genTexData links the full
# libAR/libARICP; native strict linking needs the matrix/param/util/pattern/icp
# symbols that the AR2 tracker files reference. Exclude paramGL.c (needs OpenGL).
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
  ${CMAKE_CURRENT_SOURCE_DIR}/src/markerCreator_py.cpp
  ${REPO}/emscripten/markerCreator.cpp
  ${AR_SOURCES} ${AR2_SOURCES} ${KPM_SOURCES} ${ARCORE_SOURCES}
)

target_include_directories(_core PRIVATE
  ${WAK}/include
  ${REPO}/emscripten
  ${WSRC}/KPM/FreakMatcher
)
# HAVE_THREADING enables the bounded worker pool in markerCreator.cpp. Native
# threads need no PTHREAD_POOL_SIZE / COOP-COEP dance (unlike the WASM build);
# pthread is already linked below. The `threads=` Python arg controls the count.
target_compile_definitions(_core PRIVATE HAVE_NFT HAVE_THREADING)
target_link_libraries(_core PRIVATE JPEG::JPEG ZLIB::ZLIB Threads::Threads)

install(TARGETS _core DESTINATION nft_marker_creator)
