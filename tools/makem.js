/*
 * Simple script for running emcc on ARToolKit
 * @author zz85 github.com/zz85
 */

let exec = require("child_process").exec,
  path = require("path"),
  fs = require("fs");

const HAVE_NFT = 1;

const EMSCRIPTEN_ROOT = process.env.EMSCRIPTEN;
const WEBARKITLIB_ROOT = process.env.WEBARKITLIB_ROOT || "../emscripten/WebARKitLib";

if (!EMSCRIPTEN_ROOT) {
  console.log("\nWarning: EMSCRIPTEN environment variable not found.");
  console.log(
    'If you get a "command not found" error,\ndo `source <path to emsdk>/emsdk_env.sh` and try again.',
  );
}

const EMCC = EMSCRIPTEN_ROOT ? path.resolve(EMSCRIPTEN_ROOT, "emcc") : "emcc";
const EMPP = EMSCRIPTEN_ROOT ? path.resolve(EMSCRIPTEN_ROOT, "em++") : "em++";
const OPTIMIZE_FLAGS = " -Oz "; // -Oz for smallest size
const MEM = 256 * 1024 * 1024; // 64MB

const SOURCE_PATH = path.resolve(__dirname, "../emscripten/") + "/";
const OUTPUT_PATH = path.resolve(__dirname, "../build/") + "/";

const BUILD_WASM_FILE = "NftMarkerCreator_wasm.js";
const BUILD_WASM_TD_FILE = "NftMarkerCreator_wasm.thread.js";
const BUILD_WASM_ES6_FILE = "NftMarkerCreator_ES6_wasm.js";
const BUILD_MIN_FILE = "NftMarkerCreator.min.js";

// prettier-ignore
let MAIN_SOURCES = [
    'markerCreator.cpp',
    'markerCompress.cpp'
];

MAIN_SOURCES = MAIN_SOURCES.map(function (src) {
  return path.resolve(SOURCE_PATH, src);
}).join(" ");

// prettier-ignore
let ar_sources = [
    'ARUtil/log.c',
    'ARUtil/file_utils.c',
].map(function (src) {
    return path.resolve(__dirname, WEBARKITLIB_ROOT + '/lib/SRC/', src);
});

const ar2_sources = [
  "handle.c",
  "imageSet.c",
  "jpeg.c",
  "marker.c",
  "featureMap.c",
  "featureSet.c",
  "selectTemplate.c",
  "surface.c",
  "tracking.c",
  "tracking2d.c",
  "matching.c",
  "matching2.c",
  "template.c",
  "searchPoint.c",
  "coord.c",
  "util.c",
].map(function (src) {
  return path.resolve(__dirname, WEBARKITLIB_ROOT + "/lib/SRC/AR2/", src);
});

const kpm_sources = [
  "KPM/kpmHandle.cpp",
  "KPM/kpmRefDataSet.cpp",
  "KPM/kpmMatching.cpp",
  "KPM/kpmResult.cpp",
  "KPM/kpmUtil.cpp",
  "KPM/kpmFopen.c",
  "KPM/FreakMatcher/detectors/DoG_scale_invariant_detector.cpp",
  "KPM/FreakMatcher/detectors/gaussian_scale_space_pyramid.cpp",
  "KPM/FreakMatcher/detectors/gradients.cpp",
  //"KPM/FreakMatcher/detectors/harris.cpp",
  "KPM/FreakMatcher/detectors/orientation_assignment.cpp",
  "KPM/FreakMatcher/detectors/pyramid.cpp",
  "KPM/FreakMatcher/facade/visual_database_facade.cpp",
  "KPM/FreakMatcher/matchers/hough_similarity_voting.cpp",
  "KPM/FreakMatcher/matchers/freak.cpp",
  "KPM/FreakMatcher/framework/date_time.cpp",
  "KPM/FreakMatcher/framework/image.cpp",
  "KPM/FreakMatcher/framework/logger.cpp",
  "KPM/FreakMatcher/framework/timers.cpp",
].map(function (src) {
  return path.resolve(__dirname, WEBARKITLIB_ROOT + "/lib/SRC/", src);
});

// prettier-ignore
if (HAVE_NFT) {
    ar_sources = ar_sources
        .concat(ar2_sources)
        .concat(kpm_sources);
}

let DEFINES = " ";
if (HAVE_NFT) DEFINES += " -D HAVE_NFT ";

const TD = " -D HAVE_THREADING ";

let FLAGS = "" + OPTIMIZE_FLAGS;
FLAGS += " -Wno-warn-absolute-paths ";
FLAGS += " -s TOTAL_MEMORY=" + MEM + " ";
FLAGS += " -s ALLOW_MEMORY_GROWTH=1 ";
FLAGS += " -s USE_LIBJPEG=1";
FLAGS += " -s FORCE_FILESYSTEM=1";

let ZLIB_FLAG = " -s USE_ZLIB=1 ";

let ES6_FLAGS = "";
ES6_FLAGS +=
  " -s EXPORT_ES6=1 -s USE_ES6_IMPORT_META=0 -s MODULARIZE=1 -sENVIRONMENT=web -s EXPORT_NAME='NftMC' ";

const WASM_FLAGS = " -s WASM=1 ";

const SINGLE_FILE_FLAG = " -s SINGLE_FILE=1 ";

const BIND_FLAG = " --bind ";

const EXPORTED_FUNCTIONS =
  ' -s EXPORTED_FUNCTIONS=["_malloc,_free"] -s EXPORTED_RUNTIME_METHODS=["FS,stringToUTF8,lengthBytesUTF8"] ';

/* DEBUG FLAGS */
let DEBUG_FLAGS = " -g ";
// DEBUG_FLAGS += ' -s ASSERTIONS=2 '
DEBUG_FLAGS += " -s ASSERTIONS=1 ";
DEBUG_FLAGS += " --profiling ";
// DEBUG_FLAGS += ' -s EMTERPRETIFY_ADVISE=1 '
DEBUG_FLAGS += " -s ALLOW_MEMORY_GROWTH=1";
DEBUG_FLAGS += "  -s DEMANGLE_SUPPORT=1 ";

var INCLUDES = [
  path.resolve(__dirname, WEBARKITLIB_ROOT + "/include"),
  OUTPUT_PATH,
  SOURCE_PATH,
  path.resolve(__dirname, WEBARKITLIB_ROOT + "/lib/SRC/KPM/FreakMatcher"),
]
  .map(function (s) {
    return "-I" + s;
  })
  .join(" ");

function format(str) {
  for (let f = 1; f < arguments.length; f++) {
    str = str.replace(/{\w*}/, arguments[f]);
  }
  return str;
}

function clean_builds() {
  try {
    const stats = fs.statSync(OUTPUT_PATH);
  } catch (e) {
    fs.mkdirSync(OUTPUT_PATH);
  }

  try {
    const files = fs.readdirSync(OUTPUT_PATH);
    if (files.length > 0)
      for (let i = 0; i < files.length; i++) {
        const filePath = OUTPUT_PATH + "/" + files[i];
        if (fs.statSync(filePath).isFile()) fs.unlinkSync(filePath);
      }
  } catch (e) {
    return console.log(e);
  }
}

const compile_arlib = format(
  EMCC +
    " " +
    INCLUDES +
    " " +
    ar_sources.join(" ") +
    FLAGS +
    " " +
    ZLIB_FLAG +
    " " +
    DEFINES +
    " -r -o {OUTPUT_PATH}libar.o ",
  OUTPUT_PATH,
);

const configure_zlib = format("emcmake cmake -B emscripten/build -S emscripten/zlib ..");

const build_zlib = format("cd emscripten/build && emmake make");

const copy_zlib = format("cp emscripten/build/libz.a {OUTPUT_PATH}libz.a", OUTPUT_PATH);

const compile_combine_min = format(
  EMCC +
    " " +
    INCLUDES +
    " " +
    " {OUTPUT_PATH}libar.o " +
    " {OUTPUT_PATH}libz.a " +
    MAIN_SOURCES +
    EXPORTED_FUNCTIONS +
    FLAGS +
    " -s WASM=0" +
    " " +
    DEFINES +
    BIND_FLAG +
    " -o {OUTPUT_PATH}{BUILD_MIN_FILE} ",
  OUTPUT_PATH,
  OUTPUT_PATH,
  OUTPUT_PATH,
  BUILD_MIN_FILE,
);

const compile_wasm = format(
  EMCC +
    " " +
    INCLUDES +
    " " +
    " {OUTPUT_PATH}libar.o " +
    " {OUTPUT_PATH}libz.a " +
    MAIN_SOURCES +
    EXPORTED_FUNCTIONS +
    FLAGS +
    WASM_FLAGS +
    SINGLE_FILE_FLAG +
    DEFINES +
    BIND_FLAG +
    " -std=c++11 " +
    " -o {OUTPUT_PATH}{BUILD_WASM_FILE} ",
  OUTPUT_PATH,
  OUTPUT_PATH,
  OUTPUT_PATH,
  BUILD_WASM_FILE,
);

const compile_wasm_es6 = format(
  EMCC +
    " " +
    INCLUDES +
    " " +
    " {OUTPUT_PATH}libar.o " +
    " {OUTPUT_PATH}libz.a " +
    MAIN_SOURCES +
    EXPORTED_FUNCTIONS +
    FLAGS +
    WASM_FLAGS +
    ES6_FLAGS +
    SINGLE_FILE_FLAG +
    DEFINES +
    BIND_FLAG +
    " -std=c++11 " +
    " -o {OUTPUT_PATH}{BUILD_WASM_ES6_FILE} ",
  OUTPUT_PATH,
  OUTPUT_PATH,
  OUTPUT_PATH,
  BUILD_WASM_ES6_FILE,
);

const compile_wasm_td = format(
  EMCC +
    " " +
    INCLUDES +
    " " +
    " {OUTPUT_PATH}libar.o " +
    " {OUTPUT_PATH}libz.a " +
    MAIN_SOURCES +
    EXPORTED_FUNCTIONS +
    FLAGS +
    WASM_FLAGS +
    SINGLE_FILE_FLAG +
    DEFINES +
    TD +
    " -std=c++11 -pthread " +
    BIND_FLAG +
    " -o {OUTPUT_PATH}{BUILD_WASM_TD_FILE} ",
  OUTPUT_PATH,
  OUTPUT_PATH,
  OUTPUT_PATH,
  BUILD_WASM_TD_FILE,
);

/*
 * Run commands
 */

function onExec(error, stdout, stderr) {
  if (stdout) console.log("stdout: " + stdout);
  if (stderr) console.log("stderr: " + stderr);
  if (error !== null) {
    console.log("exec error: " + error.code);
    process.exit(error.code);
  } else {
    runJob();
  }
}

const jobs = [];

function runJob() {
  if (!jobs.length) {
    console.log("Jobs completed");
    return;
  }

  const cmd = jobs.shift();

  if (typeof cmd === "function") {
    cmd();
    runJob();
    return;
  }

  console.log("\nRunning command: " + cmd + "\n");
  exec(cmd, onExec);
}

function addJob(job) {
  jobs.push(job);
}

addJob(clean_builds);
addJob(compile_arlib);
addJob(configure_zlib);
addJob(build_zlib);
addJob(copy_zlib);
addJob(compile_wasm);
addJob(compile_wasm_es6);
addJob(compile_wasm_td);
addJob(compile_combine_min);

runJob();
