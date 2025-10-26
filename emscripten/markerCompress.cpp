#include "zlib/zlib.h"
#include <AR/ar.h>
#include <fstream>

static const char *zipname = "/tempBinFile.bin";

int compressZip(char *src, int srclen) {
  char *b = new char[srclen];

  printf("Uncompressed size is: %lu", strlen(src));
  printf("\n----------\n");

  z_stream defstream;
  defstream.zalloc = Z_NULL;
  defstream.zfree = Z_NULL;
  defstream.opaque = Z_NULL;
  defstream.avail_in = (uInt)srclen;
  defstream.next_in = (Bytef *)src;
  defstream.avail_out = (uInt)srclen;
  defstream.next_out = (Bytef *)b;

  deflateInit(&defstream, Z_BEST_COMPRESSION);
  deflate(&defstream, Z_FINISH);
  deflateEnd(&defstream);

  printf("Compressed size is: %zu\n", (size_t)defstream.total_out);

  std::ofstream outFile(zipname, std::ios::binary);
  outFile.write(b, defstream.total_out);
  outFile.close();

  delete[] b;

  return 0;
}
