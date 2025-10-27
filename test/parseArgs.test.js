const { parseCliArguments, DEFAULT_THREAD_COUNT } = require("../src/cli/parseArgs");

describe("parseCliArguments", () => {
  test("returns defaults when no flags provided", () => {
    const argv = ["node", "NFTMarkerCreator.js"];

    const result = parseCliArguments(argv);

    expect(result.inputProvided).toBe(false);
    expect(result.outputProvided).toBe(false);
    expect(result.inputPath).toBeNull();
    expect(result.outputPath).toBeNull();
    expect(result.needsInputPrompt).toBe(true);
    expect(result.threadCount).toBe(DEFAULT_THREAD_COUNT);
    expect(result.threadedRequested).toBe(false);
    expect(result.params).toEqual([]);
    expect(result.noConf).toBe(false);
    expect(result.withDemo).toBe(false);
    expect(result.onlyConfidence).toBe(false);
    expect(result.isZFT).toBe(false);
  });

  test("captures flag booleans, paths, and leftover params", () => {
    const argv = [
      "node",
      "NFTMarkerCreator.js",
      "-I",
      "./image.png",
      "-O",
      "./out",
      "-NoConf",
      "-Demo",
      "-zft",
      "-onlyConfidence",
      "--extra-option",
    ];

    const result = parseCliArguments(argv);

    expect(result.inputProvided).toBe(true);
    expect(result.inputPath).toBe("./image.png");
    expect(result.outputProvided).toBe(true);
    expect(result.outputPath).toBe("./out");
    expect(result.needsInputPrompt).toBe(false);
    expect(result.noConf).toBe(true);
    expect(result.withDemo).toBe(true);
    expect(result.onlyConfidence).toBe(true);
    expect(result.isZFT).toBe(true);
    expect(result.params).toEqual(["--extra-option"]);
  });

  test("tracks threaded flag without explicit count", () => {
    const argv = ["node", "NFTMarkerCreator.js", "--threaded"];

    const result = parseCliArguments(argv);

    expect(result.threadedRequested).toBe(true);
    expect(result.threadCount).toBe(DEFAULT_THREAD_COUNT);
    expect(result.params).toEqual(["--threaded"]);
  });

  test("parses explicit thread count", () => {
    const argv = ["node", "NFTMarkerCreator.js", "--threaded", "8"];

    const result = parseCliArguments(argv);

    expect(result.threadedRequested).toBe(true);
    expect(result.threadCount).toBe(8);
    expect(result.params).toEqual(["--threaded", "8"]);
  });

  test("ignores non-numeric thread count value but preserves parameter", () => {
    const argv = ["node", "NFTMarkerCreator.js", "--threaded", "abc", "--foo"];

    const result = parseCliArguments(argv);

    expect(result.threadedRequested).toBe(true);
    expect(result.threadCount).toBe(DEFAULT_THREAD_COUNT);
    expect(result.params).toEqual(["--threaded", "abc", "--foo"]);
  });
});
