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

This project is based on the original **NFT Marker Creator** by [Carnaux](https://github.com/Carnaux/NFT-Marker-Creator) but has been updated to work with the latest versions of Node and NPM, and also to work with the latest version of Emscripten, plus other improvements.

**Now with threading support!!** Read more in _Run the app_ section

## Quick start ⚡

```bash
cd src
node NFTMarkerCreator.js -i PATH/TO/IMAGE --threaded <number_of_threads>
```

Example:

```bash
cd src
node NFTMarkerCreator.js -i pinball.jpg --threaded 8
```

Outputs land in `src/output`. See the sections below for setup, flags, and optional demo steps.

## Prerequisites ✅

- Tested on Windows 11 (PowerShell 5.1) and Ubuntu 22.04 LTS (bash)
- Git 2.39+
- Node.js v20.17.0 (`.nvmrc`) and npm 10+
- nvm 0.39+ (optional but recommended for managing Node versions)
- Docker 24+ (required only for the containerised build flow)

## Node app

### Installation 📦

1. Clone this repository (or download the latest prebuilt `NFTMarkerCreator.js` and `build/` bundle from the [GitHub Releases page](https://github.com/webarkit/Nft-Marker-Creator-App/releases)).
2. Install Node via nvm (recommended):
   - `nvm install`
   - `nvm use 20`
3. Install dependencies: `npm install`

### Run the app 🚀

1. Place the image you want to convert inside the `src` folder (either directly or inside a subfolder).
2. Generate the marker:

```bash
 cd src
 node NFTMarkerCreator.js -i PATH/TO/IMAGE
```

Example:

```bash
cd src
node NFTMarkerCreator.js -i pinball.jpg
```

The output files are saved in the `src/output` directory. Use `--threaded <count>` to speed up processing; start with the number of physical CPU cores and adjust based on thermals and available memory (e.g., laptops may peak at 4 threads, workstations can push higher):

```bash
cd src
node NFTMarkerCreator.js -i PATH/TO/IMAGE --threaded <number_of_threads>
```

Example:

```bash
cd src
node NFTMarkerCreator.js -i pinball.jpg --threaded 4
```

### CLI flags ⚙️

Example: `node NFTMarkerCreator.js -i image.png -level=4 -min_thresh=8`

| Flag                       | Description                                             | Default        |
| -------------------------- | ------------------------------------------------------- | -------------- |
| `-zft`                     | Create only the `.zft` file                             | —              |
| `-noConf`                  | Skip the confidence confirmation prompt                 | —              |
| `-Demo`                    | Generate the demo configuration alongside the marker    | —              |
| `-level=n`                 | Feature density preset from 0 (few) to 4 (many)         | `2`            |
| `-sd_thresh=<value>`       | Override standard deviation threshold                   | Auto           |
| `-max_thresh=<value>`      | Override maximum feature threshold                      | Auto           |
| `-min_thresh=<value>`      | Override minimum feature threshold                      | Auto           |
| `-leveli=n`                | Initial feature density preset from 0 to 3              | `1`            |
| `-feature_density=<value>` | Manual feature density multiplier                       | Auto           |
| `-dpi=f`                   | Force a DPI value instead of the embedded JPEG DPI      | Image metadata |
| `-max_dpi=<value>`         | Cap the maximum DPI processed for multi-scale markers   | Auto           |
| `-min_dpi=<value>`         | Floor the minimum DPI processed for multi-scale markers | Auto           |
| `-background`              | Run detached in the background (macOS/Linux only)       | —              |
| `--help`, `-h`, `-?`       | Print CLI usage information                             | —              |
| `--threaded <n>`           | Run feature extraction using `n` threads                | `1`            |
| `-nofset` / `-fset`        | Disable or force generation of the `.fset` file         | Enabled        |
| `-nofset3` / `-fset3`      | Disable or force generation of the `.fset3` file        | Enabled        |

### Demo 🧪 (optional)

- `npm run demo`
- Open [http://localhost:3000/](http://localhost:3000/)
- Add `-Demo` to the marker command to generate demo assets automatically: `node NFTMarkerCreator.js -i image.png -Demo`

## Additional resources 📚

- [Marker quality tips](https://github.com/Carnaux/NFT-Marker-Creator/wiki/Creating-good-markers) — guidance on preparing images that track reliably

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

Linux/macOS:

```bash
docker run -dit --name nft-app -v "$(pwd):/src" webarkit/nft-marker-creator-app:0.2.5 bash
```

Windows (PowerShell):

```powershell
docker run -dit --name nft-app -v "${PWD}:/src" webarkit/nft-marker-creator-app:0.2.5 bash
```

With the docker container generate the NFT marker:

`docker exec nft-app node ../Nft-Marker-Creator-App/src/NFTMarkerCreator.js -I /src/pinball.jpg`

remember to prepend the `-I /src/<path to your image>`

## Build

Build emscripten files with docker:

Linux/macOS:

```bash
docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) -e "EMSCRIPTEN=/emsdk/upstream/emscripten" emscripten/emsdk:3.1.69 npm run build-local
```

Windows (PowerShell):

```powershell
docker run --rm -v "${PWD}:/src" -e "EMSCRIPTEN=/emsdk/upstream/emscripten" emscripten/emsdk:3.1.69 npm run build-local
```

or better create a docker container and run the build command inside it:

Linux/macOS:

```bash
docker run -dit --name emscripten-nft-marker-creator-app -v $(pwd):/src emscripten/emsdk:3.1.69 bash
docker exec emscripten-nft-marker-creator-app npm run build-local
```

Windows (PowerShell):

```powershell
docker run -dit --name emscripten-nft-marker-creator-app -v "${PWD}:/src" emscripten/emsdk:3.1.69 bash
docker exec emscripten-nft-marker-creator-app npm run build-local
```

In VSCode you can run the `setup-docker` and `build-docker` command inside package.json.

## Planned Features

- [x] Multi threading support to speed up the creation of the markers.
- [ ] Python version of the project and app.
