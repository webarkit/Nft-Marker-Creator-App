#include "zlib/zlib.h"
#include <AR/ar.h>

static const char *zipname = "/tempBinFile.bin";

int compressZip(char *src, int srclen) {
  FILE *fp;

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

  printf("Compressed size is: %lu\n", strlen(b));

  fp = fopen(zipname, "wb");
  fwrite(b, defstream.total_out, 1, fp);
  fclose(fp);

  delete[] b;

  return 0;
}
