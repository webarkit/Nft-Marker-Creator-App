![github releases](https://flat.badgen.net/github/release/webarkit/Nft-Marker-Creator-App)
![github stars](https://flat.badgen.net/github/stars/webarkit/Nft-Marker-Creator-App)
![github forks](https://flat.badgen.net/github/forks/webarkit/Nft-Marker-Creator-App)
![npm package version](https://flat.badgen.net/npm/v/@webarkit/nft-marker-creator-app)
![docker pulls](https://flat.badgen.net/docker/pulls/webarkit/nft-marker-creator-app)
[![CI](https://github.com/webarkit/Nft-Marker-Creator-App/actions/workflows/CI.yml/badge.svg)](https://github.com/webarkit/Nft-Marker-Creator-App/actions/workflows/CI.yml)
[![Build Nft-Marker-Creator-App](https://github.com/webarkit/Nft-Marker-Creator-App/actions/workflows/build.yml/badge.svg)](https://github.com/webarkit/Nft-Marker-Creator-App/actions/workflows/main.yml)

# Nft-Marker-Creator-App

This editor creates NFT markers for **WebARKitLib** and **ARTOOLKIT 5.x**, they are compatible with jsartoolkitNFT.js, jsartoolkit5.js, artoolkit5.js, ARnft.js and AR.js.
A Node app is provided.

Check out the wiki to learn how to generate good markers! https://github.com/Carnaux/NFT-Marker-Creator/wiki/Creating-good-markers

This project is based on the original **NFT Marker Creator** by [Carnaux](https://github.com/Carnaux/NFT-Marker-Creator) but has been updated to work with the latest versions of Node and NPM, and also to work with the latest version of Emscripten, plus other improvements.

## Node app

### How to use it

1. Clone this repository.

2. Install all dependencies. First run

   `nvm install`

   it will download the node version specified in the `.nvmrc` file. You need to install nvm first if you don't have it. Then run `nvm use 18` to use the node version specified in the `.nvmrc` file.

   Then finally run

   `npm install`

3. Put the image you want inside the `src` folder. You can just paste it or you can create a folder. e.g

   - NFTmarkerCreatorApp Folder
   - NFTMarkerCreator.js
   - IMAGE.PNG :arrow_left:
   - ...

   or

   - NFTmarkerCreatorApp Folder
   - NFTMarkerCreator.js
   - FOLDER/IMAGE.PNG :arrow_left:
   - ...

4. Run it

```
cd src
node NFTMarkerCreator.js -i PATH/TO/IMAGE
```

In the end of the process an "output" folder will be created (if it does not exist) with the marker files.

You can use additional flags with the run command.

e.g `node NFTMarkerCreator.js -i image.png -level=4 -min_thresh=8`

    -zft
          Flag for creating only the zft file
    -noConf
          Disable confirmation after the confidence level
    -Demo
          Creates the demo configuration
    -level=n
         (n is an integer in range 0 (few) to 4 (many). Default 2.'
    -sd_thresh=<sd_thresh>
    -max_thresh=<max_thresh>
    -min_thresh=<min_thresh>
    -leveli=n
         (n is an integer in range 0 (few) to 3 (many). Default 1.'
    -feature_density=<feature_density>
    -dpi=f:
          Override embedded JPEG DPI value.
    -max_dpi=<max_dpi>
    -min_dpi=<min_dpi>
    -background
         Run in background, i.e. as daemon detached from controlling terminal. (macOS and Linux only.)
    --help -h -?
          Display this help

5. The generated files will be on the "output" folder.

6. (OPTIONAL) You can test your marker using the demo folder!

   - Just run `npm run demo`.

   - It should open a server at: http://localhost:3000/

   If you want to create the demo configuration when you create a marker, add `-Demo` to the command parameters.

   e.g node NFTMarkerCreator.js -i image.png -Demo

## ES6 version of the build library

The library is built with Emscripten and is located in the `build` folder. It is an ES6 module and can be imported in your project.

<!-- prettier-ignore -->
```js
<script type="module">

  import nftMC from '../build/NFTMarkerCreator_ES6_wasm.js';
  const mc = await nftMC();

</script>
```

## Create your NTS markers with our docker image

First, you need docker installed in your system, if you haven't, follow the Docker engine installation [instruction](https://docs.docker.com/engine/install/) .
Then inside the folder you want to run the docker image:

`docker run -dit --name nft-app -v "$(pwd):/src" webarkit/nft-marker-creator-app:0.2.3 bash`

With the docker container generate the NFT marker:

`docker exec nft-app node ../Nft-Marker-Creator-App/src/NFTMarkerCreator.js -I /src/pinball.jpg`

remember to prepend the `-I /src/<path to your image>`

## Build

Build emscripten files with docker:

    docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) -e "EMSCRIPTEN=/emsdk/upstream/emscripten"  emscripten/emsdk:3.1.69 npm run build-local

or better create a docker container and run the build command inside it:

    docker run -dit --name emscripten-nft-marker-creator-app -v $(pwd):/src emscripten/emsdk:3.1.69 bash
    docker exec emscripten-nft-marker-creator-app npm run build-local

In VSCode you can run the `setup-docker` and `build-docker` command inside package.json.

## Planned Features

- [ ] Multi threading support to speed up the creation of the markers.
