# Nft-Marker-Creator-App

## Build
Build emscripten files with docker:
docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) -e "EMSCRIPTEN=/emsdk/upstream/emscripten"  emscripten/emsdk:3.1.26 npm run build-local