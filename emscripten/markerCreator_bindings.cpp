#include <emscripten/bind.h>
#include <emscripten/val.h>

int createNftDataSet_em(emscripten::val imgData, float dpiIn, int xsizeIn,
                        int ysizeIn, int ncIn, std::string cmdStr) {
  std::vector<uint8_t> idata =
      emscripten::convertJSArrayToNumberVector<uint8_t>(imgData);
  return createNftDataSet(idata.data(), dpiIn, xsizeIn, ysizeIn, ncIn,
                          &cmdStr[0]);
}

int compressZip_em(std::string srcStr, int srclen) {
  return compressZip(&srcStr[0], srclen);
}

EMSCRIPTEN_BINDINGS(markerCreator_bindings) {
  emscripten::function("createNftDataSet", &createNftDataSet_em);
  emscripten::function("compressZip", &compressZip_em);
};