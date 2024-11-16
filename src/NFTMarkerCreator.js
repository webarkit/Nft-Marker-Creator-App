const path = require("path");
const fs = require("fs");
const sharp = require("sharp");
const { prompt } = require("enquirer");
const Module = require("../build/NftMarkerCreator_wasm.js");

// GLOBAL VARs
const params = [];

const validImageExt = [".jpg", ".jpeg", ".png"];

let srcImage;

let outputPath = "output/";

let buffer;

const foundInputPath = {
  b: false,
  i: -1,
};

const foundOutputPath = {
  b: false,
  i: -1,
};

let noConf = false;
let withDemo = false;
let onlyConfidence = false;
let isZFT = false;

const imageData = {
  sizeX: 0,
  sizeY: 0,
  nc: 0,
  dpi: 0,
  array: [],
};

Module.onRuntimeInitialized = async function () {
  console.log("arguments...: ", process.argv);

  for (let j = 2; j < process.argv.length; j++) {
    if (process.argv[j].indexOf("-i") !== -1 || process.argv[j].indexOf("-I") !== -1) {
      foundInputPath.b = true;
      foundInputPath.i = j + 1;
      j++;
    } else if (process.argv[j] === "-NoConf") {
      noConf = true;
    } else if (process.argv[j] === "-Demo") {
      withDemo = true;
    } else if (process.argv[j] === "-zft") {
      isZFT = true;
    } else if (process.argv[j] === "-onlyConfidence") {
      onlyConfidence = true;
    } else if (process.argv[j].indexOf("-o") !== -1 || process.argv[j].indexOf("-O") !== -1) {
      foundOutputPath.b = true;
      foundOutputPath.i = j + 1;
      j++;
    } else {
      console.log(process.argv[j]);
      params.push(process.argv[j]);
    }
  }

  if (!foundInputPath.b) {
    const response = await prompt({
      type: "input",
      name: "inputPath",
      message: "Image path not present, to continue provide a path to image:",
    });
    srcImage = response.inputPath;
  } else {
    srcImage = process.argv[foundInputPath.i];
  }

  if (!srcImage) {
    console.log("\nERROR: No image in INPUT command!\n e.g:(-i /PATH/TO/IMAGE)\n");
    process.exit(1);
  } else {
    console.log("checking file path...: ", srcImage);
    if (!srcImage.startsWith("/")) {
      // Relative path
      srcImage = path.join(__dirname, srcImage);
    }

    console.log("image path is: ", srcImage);
    // srcImage = path.join(__dirname, process.argv[foundInputPath.i]);
  }

  if (foundOutputPath.b) {
    outputPath = process.argv[foundOutputPath.i];

    if (!outputPath.startsWith("/")) {
      // relative path
      outputPath = path.join(__dirname, outputPath);
      // outputPath = '/' + outputPath;
    }

    if (!outputPath.endsWith("/")) {
      outputPath += "/";
    }

    console.log("Set output path: " + outputPath);
  }

  let fileNameWithExt = path.basename(srcImage);
  let fileName = path.parse(fileNameWithExt).name;
  let extName = path.parse(fileNameWithExt).ext;

  let foundExt = false;
  for (let ext in validImageExt) {
    if (extName.toLowerCase() === validImageExt[ext]) {
      foundExt = true;
      break;
    }
  }

  if (!foundExt) {
    console.log("\nERROR: Invalid image TYPE!\n Valid types:(jpg,JPG,jpeg,JPEG,png,PNG)\n");
    process.exit(1);
  }

  if (!fs.existsSync(srcImage)) {
    console.log("\nERROR: Not possible to read image, probably invalid image PATH!\n");
    process.exit(1);
  } else {
    buffer = fs.readFileSync(srcImage);
  }

  console.log("Check output path: " + outputPath);
  if (!fs.existsSync(outputPath)) {
    fs.mkdirSync(outputPath);
  }

  if (
    extName.toLowerCase() === ".jpg" ||
    extName.toLowerCase() === ".jpeg" ||
    extName.toLowerCase() === ".png"
  ) {
    await processImage(buffer);
  }

  let confidence = calculateQuality();

  let txt = " - - - - - ";
  if (confidence.l !== 0) {
    let str = txt.split(" ");
    str.pop();
    str.shift();
    for (let i = 0; i < parseInt(confidence.l); i++) {
      str[i] = " *";
    }
    str.push(" ");
    txt = str.join("");
  }

  if (onlyConfidence) {
    console.log("%f", confidence.l);
    process.exit(0);
  }

  console.log(
    "\nConfidence level: [" + txt + "] %f/5 || Entropy: %f || Current max: 5.17 min: 4.6\n",
    confidence.l,
    confidence.e,
  );

  if (noConf) {
    await askToContinue();
  }

  let paramStr = params.join(" ");

  console.log("Write Success");

  console.log("Continue to Create NftDataSet..");
  Module.createNftDataSet(
      imageData.array,
    imageData.dpi,
    imageData.sizeX,
    imageData.sizeY,
    imageData.nc,
      paramStr
  );
  console.log("Create NFT Dataset complete...");

  let filenameIset = "tempFilename.iset";
  let filenameFset = "tempFilename.fset";
  let filenameFset3 = "tempFilename.fset3";

  let ext = ".iset";
  let ext2 = ".fset";
  let ext3 = ".fset3";

  let content = Module.FS.readFile(filenameIset);
  let contentFset = Module.FS.readFile(filenameFset);
  let contentFset3 = Module.FS.readFile(filenameFset3);

  if (isZFT) {
    console.log("CREATING ZFT FILE");
    let iset = Buffer.from(content.buffer);
    let fset = Buffer.from(contentFset.buffer);
    let fset3 = Buffer.from(contentFset3.buffer);

    let obj = {
      iset: iset.toString("hex"),
      fset: fset.toString("hex"),
      fset3: fset3.toString("hex"),
    };

    let strObj = JSON.stringify(obj);

    let StrBufferZip = Module._malloc(strObj.length + 1);
    Module.stringToUTF8(strObj, StrBufferZip);

    Module._compressZip(StrBufferZip, strObj.length);

    let contentBin = Module.FS.readFile("tempBinFile.bin");

    // fs.writeFileSync(path.join(__dirname, '/output/') + fileName + ".zft", contentBin);
    fs.writeFileSync(outputPath + fileName + ".zft", contentBin);

    Module._free(StrBufferZip);

    if (withDemo) {
      console.log("\nFinished marker creation!\nNow configuring demo! \n");

      const markerDir = path.join(__dirname, "./../demo/public/marker/");

      if (!fs.existsSync(markerDir)) {
        fs.mkdirSync(markerDir);
      }
      let demoHTML = fs.readFileSync(path.join(__dirname, "./../demo/nft.html")).toString("utf8").split("\n");
      addNewMarker(demoHTML, fileName);
      let newHTML = demoHTML.join("\n");

      fs.writeFileSync(path.join(__dirname, "./../demo/nft.html"), newHTML, { encoding: "utf8", flag: "w" });

      const files = fs.readdirSync(markerDir);
      for (const file of files) {
        fs.unlink(path.join(markerDir, file), (err) => {
          if (err) throw err;
        });
      }

      fs.writeFileSync(markerDir + fileName + ".zft", contentBin);

      console.log("Finished!\nTo run demo use: 'npm run demo'");
    }
  } else {
    console.log("CREATING ISET, FSET AND FSET3 FILES");
    // fs.writeFileSync(path.join(__dirname, outputPath) + fileName + ext, content);
    // fs.writeFileSync(path.join(__dirname, outputPath) + fileName + ext2, contentFset);
    // fs.writeFileSync(path.join(__dirname, outputPath) + fileName + ext3, contentFset3);
    fs.writeFileSync(outputPath + fileName + ext, content);
    fs.writeFileSync(outputPath + fileName + ext2, contentFset);
    fs.writeFileSync(outputPath + fileName + ext3, contentFset3);

    if (withDemo) {
      console.log("\nFinished marker creation!\nNow configuring demo! \n");

      const markerDir = path.join(__dirname, "./../demo/public/marker/");

      if (!fs.existsSync(markerDir)) {
        fs.mkdirSync(markerDir);
      }
      let demoHTML = fs.readFileSync(path.join(__dirname, "./../demo/nft.html")).toString("utf8").split("\n");
      addNewMarker(demoHTML, fileName);
      let newHTML = demoHTML.join("\n");

      fs.writeFileSync(path.join(__dirname, "./../demo/nft.html"), newHTML, { encoding: "utf8", flag: "w" });

      const files = fs.readdirSync(markerDir);
      for (const file of files) {
        fs.unlink(path.join(markerDir, file), (err) => {
          if (err) throw err;
        });
      }

      fs.writeFileSync(markerDir + fileName + ext, content);
      fs.writeFileSync(markerDir + fileName + ext2, contentFset);
      fs.writeFileSync(markerDir + fileName + ext3, contentFset3);

      console.log("Finished!\nTo run demo use: 'npm run demo'");
    }
  }

  process.exit(0);
};

async function processImage(buf) {
  const image = sharp(buf);
  await image
    .metadata()
    .then(async (metadata) => {
      if (metadata.density) {
        imageData.dpi = metadata.density;
      } else {
        console.log("\nWARNING: No DPI value found! Using 150 as default value!\n");
        imageData.dpi = 150;
      }
      if (metadata.width) {
        imageData.sizeX = metadata.width;
      } else {
        await metadataWidth();
      }
      if (metadata.height) {
        imageData.sizeY = metadata.height;
      } else {
        await metadataHeigth();
      }
      if (metadata.channels) {
        imageData.nc = metadata.channels;
      } else {
        await metadataChannels();
      }
      return image.raw().toBuffer();
    })
    .then((data) => {
      let dt = data.buffer;

      let verifyColorSpace = detectColorSpace(dt);

      if (verifyColorSpace === 1) {
        console.log("Color Space is 1 channel!");
      } else if (verifyColorSpace === 3) {
        console.log("Color Space is 3 channels!");
      }

      let uint = new Uint8Array(dt);
      if (imageData.nc === verifyColorSpace) {
        console.log("Color Space is equal to metadata.channels!");
      } else {
        console.log("Color Space is not equal to metadata.channels!");
        //process.exit(1);
      }
      imageData.nc = verifyColorSpace;
      imageData.array = uint;
    })
    .catch(function (err) {
      console.error("Error extracting metadata: " + err);
      process.exit(1);
    });
}

async function extractMetadata(buf) {
  return await sharp(buf)
    .metadata()
    .then(function (metadata) {
      if (metadata.density) {
        imageData.dpi = metadata.density;
      } else {
        console.log("\nWARNING: No DPI value found! Using 150 as default value!\n");
        imageData.dpi = 150;
      }
      imageData.sizeX = metadata.width;
      imageData.sizeY = metadata.height;
      imageData.nc = metadata.channels;
    })
    .catch(function (err) {
      console.error("Error extracting metadata: " + err);
      process.exit(1);
    });
}

function detectColorSpace(arr) {
  let target = parseInt(arr.length / 4);

  let counter = 0;

  for (let j = 0; j < arr.length; j += 4) {
    let r = arr[j];
    let g = arr[j + 1];
    let b = arr[j + 2];

    if (r === g && r === b) {
      counter++;
    }
  }

  if (target === counter) {
    return 1;
  } else {
    return 3;
  }
}

function rgbaToRgb(arr) {
  let newArr = [];
  let BGColor = {
    R: 255,
    G: 255,
    B: 255,
  };

  for (let i = 0; i < arr.length; i += 4) {
    let r = parseInt(255 * ((1 - arr[i + 3]) * BGColor.R + arr[i + 3] * arr[i]));
    let g = parseInt(255 * ((1 - arr[i + 3]) * BGColor.G + arr[i + 3] * arr[i + 1]));
    let b = parseInt(255 * ((1 - arr[i + 3]) * BGColor.B + arr[i + 3] * arr[i + 2]));

    newArr.push(r);
    newArr.push(g);
    newArr.push(b);
  }
  return newArr;
}

function calculateQuality() {
  let gray = toGrayscale(imageData.array);
  let hist = getHistogram(gray);
  let ent = 0;
  let totSize = imageData.sizeX * imageData.sizeY;
  for (let i = 0; i < 255; i++) {
    if (hist[i] > 0) {
      let temp = (hist[i] / totSize) * Math.log(hist[i] / totSize);
      ent += temp;
    }
  }

  let entropy = (-1 * ent).toFixed(2);
  let oldRange = 5.17 - 4.6;
  let newRange = 5 - 0;
  let level = ((entropy - 4.6) * newRange) / oldRange;

  if (level > 5) {
    level = 5;
  } else if (level < 0) {
    level = 0;
  }
  return { l: level.toFixed(2), e: entropy };
}

function toGrayscale(arr) {
  let gray = [];
  for (let i = 0; i < arr.length; i += 3) {
    let avg = (arr[i] + arr[i + 1] + arr[i + 2]) / 3;
    gray.push(parseInt(avg));
  }
  return gray;
}

function getHistogram(arr) {
  let hist = [256];
  for (let i = 0; i < arr.length; i++) {
    hist[i] = 0;
  }
  for (let i = 0; i < arr.length; i++) {
    hist[arr[i]]++;
  }
  return hist;
}

function addNewMarker(text, name) {
  for (let i = 0; i < text.length; i++) {
    if (text[i].trim().includes("<script>MARKER_NAME =")) {
      text[i] = "<script>MARKER_NAME = '" + name + "'</script>";
      break;
    }
  }
}

async function askToContinue() {
  const response = await prompt({
    type: "input",
    name: "answer",
    message: "Do you want to continue? (Y/N)\n",
  });

  if (response.answer === "n") {
    console.log("\nProcess finished by the user! \n");
    process.exit(1);
  }
}

async function metadataWidth() {
  const responseToProceed = await prompt({
    type: "input",
    name: "answer",
    message: "Metadata width not present do you want to inform it? (Y/N)\n",
  });

  if (responseToProceed.answer === "n") {
    console.log("\nProcess finished by the user! \n");
    process.exit(1);
  } else {
    const responseAfterEnquiry = await prompt({
      type: "input",
      name: "width",
      message: "Inform the width: e.g 200\n",
    });
    if (responseAfterEnquiry.width) {
      imageData.sizeX = responseAfterEnquiry.width;
    }
  }
}

async function metadataHeigth() {
  const responseToProceed = await prompt({
    type: "input",
    name: "answer",
    message: "Metadata height not present do you want to inform it? (Y/N)\n",
  });

  if (responseToProceed.answer === "n") {
    console.log("\nProcess finished by the user! \n");
    process.exit(1);
  } else {
    const responseAfterEnquiry = await prompt({
      type: "input",
      name: "height",
      message: "Inform the height: e.g 400\n",
    });
    if (responseAfterEnquiry.height) {
      imageData.sizeY = responseAfterEnquiry.height;
    }
  }
}

async function metadataChannels() {
  const responseToProceed = await prompt({
    type: "input",
    name: "answer",
    message: "Metadata channels not present do you want to inform it? (Y/N)\n",
  });

  if (responseToProceed.answer === "n") {
    console.log("\nProcess finished by the user! \n");
    process.exit(1);
  } else {
    const responseAfterEnquiry = await prompt({
      type: "input",
      name: "channels",
      message: "Inform the number of channels: e.g 3\n",
    });
    if (responseAfterEnquiry.channels) {
      imageData.nc = responseAfterEnquiry.channels;
    }
  }
}
