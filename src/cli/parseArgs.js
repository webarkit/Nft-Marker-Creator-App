const DEFAULT_THREAD_COUNT = 1;

function parseCliArguments(argv) {
  if (!Array.isArray(argv)) {
    throw new TypeError("argv must be an array");
  }

  const result = {
    params: [],
    inputPath: null,
    outputPath: null,
    inputProvided: false,
    outputProvided: false,
    needsInputPrompt: true,
    noConf: false,
    withDemo: false,
    onlyConfidence: false,
    isZFT: false,
    threadCount: DEFAULT_THREAD_COUNT,
    threadedRequested: false,
  };

  for (let index = 2; index < argv.length; index++) {
    const arg = argv[index];

    if (arg === "-i" || arg === "-I") {
      result.inputProvided = true;
      const nextValue = argv[index + 1];
      if (typeof nextValue === "string") {
        result.inputPath = nextValue;
        index += 1;
      }
    } else if (arg === "-NoConf") {
      result.noConf = true;
    } else if (arg === "-Demo") {
      result.withDemo = true;
    } else if (arg === "-zft") {
      result.isZFT = true;
    } else if (arg === "-onlyConfidence") {
      result.onlyConfidence = true;
    } else if (arg === "-o" || arg === "-O") {
      result.outputProvided = true;
      const nextValue = argv[index + 1];
      if (typeof nextValue === "string") {
        result.outputPath = nextValue;
        index += 1;
      }
    } else if (arg === "--threaded") {
      result.threadedRequested = true;
      result.params.push(arg);
      const nextValue = argv[index + 1];
      if (typeof nextValue === "string" && !nextValue.startsWith("-")) {
        const parsed = parseInt(nextValue, 10);
        if (!Number.isNaN(parsed) && parsed > 0) {
          result.threadCount = parsed;
        }
        result.params.push(nextValue);
        index += 1;
      }
    } else {
      result.params.push(arg);
    }
  }

  if (result.inputProvided && typeof result.inputPath === "string") {
    result.needsInputPrompt = false;
  }

  return result;
}

module.exports = {
  parseCliArguments,
  DEFAULT_THREAD_COUNT,
};
