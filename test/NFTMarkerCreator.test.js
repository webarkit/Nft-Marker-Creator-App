const { exec } = require("child_process");
const path = require("path");

test("NFTMarkerCreator should process the image", (done) => {
  const scriptPath = path.join(__dirname, "../src/NFTMarkerCreator.js");
  const imagePath = "../test/pinball-test.jpg";
  const command = `node ${scriptPath} -I ${imagePath}`;

  exec(command, (error, stdout, stderr) => {
    expect(error).toBeNull();
    expect(stdout).toContain("Create NFT Dataset complete...");
    done();
  });
}, 57000);
