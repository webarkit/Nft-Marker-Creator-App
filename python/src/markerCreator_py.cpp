// pybind11 binding for the native NFT marker creator core.
// Parallel to emscripten/markerCreator_bindings.cpp (the WASM binding); this one
// is compiled by the native (Python) build. The core createNftDataSet is defined
// in emscripten/markerCreator.cpp, compiled as a pure-C++ translation unit.
#include <pybind11/pybind11.h>
#include <string>
#include <AR/ar.h> // ARUint8

// Defined in markerCreator.cpp (its Emscripten binding is #ifdef'd out natively).
int createNftDataSet(ARUint8 *imageIn, float dpiIn, int xsizeIn, int ysizeIn,
                     int ncIn, char *cmdStr);

namespace py = pybind11;

// `image` is raw interleaved pixel data of length xsize*ysize*nc.
// Writes tempFilename.iset/.fset/.fset3 to the current working directory.
static int create_nft_dataset(py::bytes image, float dpi, int xsize, int ysize,
                              int nc, std::string cmd)
{
    std::string data = image;   // owns a mutable copy of the pixel bytes
    return createNftDataSet(reinterpret_cast<ARUint8 *>(&data[0]), dpi, xsize,
                            ysize, nc, &cmd[0]);
}

PYBIND11_MODULE(_core, m)
{
    m.doc() = "Native NFT marker creator (WebARKitLib) — pybind11 binding";
    m.def("create_nft_dataset", &create_nft_dataset, py::arg("image"),
          py::arg("dpi"), py::arg("xsize"), py::arg("ysize"), py::arg("nc"),
          py::arg("cmd") = "");
}
