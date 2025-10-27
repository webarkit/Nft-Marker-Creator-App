const fs = require("fs");
const path = require("path");
const { execFile } = require("child_process");
const { promisify } = require("util");

const execFileAsync = promisify(execFile);

jest.setTimeout(180000);

const scriptPath = path.resolve(__dirname, "../src/NFTMarkerCreator.js");
const scriptDir = path.dirname(scriptPath);
const imagePath = path.resolve(__dirname, "pinball-test.jpg");
const relativeImagePath = path.relative(scriptDir, imagePath);
const nonThreadOutput = path.resolve(__dirname, "output-non-thread");
const threadedOutput = path.resolve(__dirname, "output-thread");
const relativeNonThreadOutput = path.relative(scriptDir, nonThreadOutput);
const relativeThreadOutput = path.relative(scriptDir, threadedOutput);
const invalidImagePath = path.resolve(__dirname, "invalid-file.txt");
const relativeInvalidPath = path.relative(scriptDir, invalidImagePath);
const missingImagePath = path.resolve(__dirname, "does-not-exist.jpg");
const relativeMissingPath = path.relative(scriptDir, missingImagePath);

const runCli = async (args, options = {}) => {
  try {
    const { stdout, stderr } = await execFileAsync(process.execPath, [scriptPath, ...args], {
      maxBuffer: 1024 * 1024,
      ...options,
    });
    return { success: true, code: 0, stdout, stderr };
  } catch (error) {
    return {
      success: false,
      code: typeof error.code === "number" ? error.code : null,
      signal: error.signal ?? null,
      stdout: error.stdout ?? "",
      stderr: error.stderr ?? "",
    };
  }
};

const cleanupDir = (dir) => {
  fs.rmSync(dir, { recursive: true, force: true });
};

beforeEach(() => {
  cleanupDir(nonThreadOutput);
  cleanupDir(threadedOutput);
  if (!fs.existsSync(invalidImagePath)) {
    fs.writeFileSync(invalidImagePath, "not-an-image");
  }
});

afterAll(() => {
  cleanupDir(nonThreadOutput);
  cleanupDir(threadedOutput);
  if (fs.existsSync(invalidImagePath)) {
    fs.unlinkSync(invalidImagePath);
  }
});

test("threaded and non-threaded runs produce identical marker files", async () => {
  const baseResult = await runCli(["-I", relativeImagePath, "-O", relativeNonThreadOutput]);

  expect(baseResult.success).toBe(true);
  expect(baseResult.stdout).toContain("Create NFT Dataset complete...");

  const threadedResult = await runCli([
    "-I",
    relativeImagePath,
    "-O",
    relativeThreadOutput,
    "--threaded",
    "4",
  ]);

  expect(threadedResult.success).toBe(true);
  expect(threadedResult.stdout).toContain("Create NFT Dataset complete...");

  const fileBase = "pinball-test";
  const extensions = ["iset", "fset", "fset3"];

  for (const ext of extensions) {
    const nonThreadFile = path.join(nonThreadOutput, `${fileBase}.${ext}`);
    const threadedFile = path.join(threadedOutput, `${fileBase}.${ext}`);

    expect(fs.existsSync(nonThreadFile)).toBe(true);
    expect(fs.existsSync(threadedFile)).toBe(true);

    const nonThreadData = fs.readFileSync(nonThreadFile);
    const threadedData = fs.readFileSync(threadedFile);

    expect(nonThreadData.equals(threadedData)).toBe(true);
  }
});

test("fails when image path does not exist", async () => {
  const result = await runCli(["-I", relativeMissingPath]);

  expect(result.success).toBe(false);
  expect(result.code).toBe(1);
  expect(result.stdout).toContain("Not possible to read image");
});

test("fails when image extension is invalid", async () => {
  const result = await runCli(["-I", relativeInvalidPath]);

  expect(result.success).toBe(false);
  expect(result.code).toBe(1);
  expect(result.stdout).toContain("Invalid image TYPE");
});
