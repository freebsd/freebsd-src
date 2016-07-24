/*
 * bwt.c for libdivsufsort
 * Copyright (c) 2003-2008 Yuta Mori All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MEMORY_H
# include <memory.h>
#endif
#if HAVE_STDDEF_H
# include <stddef.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_IO_H && HAVE_FCNTL_H
# include <io.h>
# include <fcntl.h>
#endif
#include <time.h>
#include <divsufsort.h>
#include "lfs.h"


static
size_t
write_int(FILE *fp, saidx_t n) {
  unsigned char c[4];
  c[0] = (unsigned char)((n >>  0) & 0xff), c[1] = (unsigned char)((n >>  8) & 0xff),
  c[2] = (unsigned char)((n >> 16) & 0xff), c[3] = (unsigned char)((n >> 24) & 0xff);
  return fwrite(c, sizeof(unsigned char), 4, fp);
}

static
void
print_help(const char *progname, int status) {
  fprintf(stderr,
          "bwt, a burrows-wheeler transform program, version %s.\n",
          divsufsort_version());
  fprintf(stderr, "usage: %s [-b num] INFILE OUTFILE\n", progname);
  fprintf(stderr, "  -b num    set block size to num MiB [1..512] (default: 32)\n\n");
  exit(status);
}

int
main(int argc, const char *argv[]) {
  FILE *fp, *ofp;
  const char *fname, *ofname;
  sauchar_t *T;
  saidx_t *SA;
  LFS_OFF_T n;
  size_t m;
  saidx_t pidx;
  clock_t start,finish;
  saint_t i, blocksize = 32, needclose = 3;

  /* Check arguments. */
  if((argc == 1) ||
     (strcmp(argv[1], "-h") == 0) ||
     (strcmp(argv[1], "--help") == 0)) { print_help(argv[0], EXIT_SUCCESS); }
  if((argc != 3) && (argc != 5)) { print_help(argv[0], EXIT_FAILURE); }
  i = 1;
  if(argc == 5) {
    if(strcmp(argv[i], "-b") != 0) { print_help(argv[0], EXIT_FAILURE); }
    blocksize = atoi(argv[i + 1]);
    if(blocksize < 0) { blocksize = 1; }
    else if(512 < blocksize) { blocksize = 512; }
    i += 2;
  }
  blocksize <<= 20;

  /* Open a file for reading. */
  if(strcmp(argv[i], "-") != 0) {
#if HAVE_FOPEN_S
    if(fopen_s(&fp, fname = argv[i], "rb") != 0) {
#else
    if((fp = LFS_FOPEN(fname = argv[i], "rb")) == NULL) {
#endif
      fprintf(stderr, "%s: Cannot open file `%s': ", argv[0], fname);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
  } else {
#if HAVE__SETMODE && HAVE__FILENO
    if(_setmode(_fileno(stdin), _O_BINARY) == -1) {
      fprintf(stderr, "%s: Cannot set mode: ", argv[0]);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
#endif
    fp = stdin;
    fname = "stdin";
    needclose ^= 1;
  }
  i += 1;

  /* Open a file for writing. */
  if(strcmp(argv[i], "-") != 0) {
#if HAVE_FOPEN_S
    if(fopen_s(&ofp, ofname = argv[i], "wb") != 0) {
#else
    if((ofp = LFS_FOPEN(ofname = argv[i], "wb")) == NULL) {
#endif
      fprintf(stderr, "%s: Cannot open file `%s': ", argv[0], ofname);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
  } else {
#if HAVE__SETMODE && HAVE__FILENO
    if(_setmode(_fileno(stdout), _O_BINARY) == -1) {
      fprintf(stderr, "%s: Cannot set mode: ", argv[0]);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
#endif
    ofp = stdout;
    ofname = "stdout";
    needclose ^= 2;
  }

  /* Get the file size. */
  if(LFS_FSEEK(fp, 0, SEEK_END) == 0) {
    n = LFS_FTELL(fp);
    rewind(fp);
    if(n < 0) {
      fprintf(stderr, "%s: Cannot ftell `%s': ", argv[0], fname);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
    if(0x20000000L < n) { n = 0x20000000L; }
    if((blocksize == 0) || (n < blocksize)) { blocksize = (saidx_t)n; }
  } else if(blocksize == 0) { blocksize = 32 << 20; }

  /* Allocate 5blocksize bytes of memory. */
  T = (sauchar_t *)malloc(blocksize * sizeof(sauchar_t));
  SA = (saidx_t *)malloc(blocksize * sizeof(saidx_t));
  if((T == NULL) || (SA == NULL)) {
    fprintf(stderr, "%s: Cannot allocate memory.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Write the blocksize. */
  if(write_int(ofp, blocksize) != 4) {
    fprintf(stderr, "%s: Cannot write to `%s': ", argv[0], ofname);
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "  BWT (blocksize %" PRIdSAINT_T ") ... ", blocksize);
  start = clock();
  for(n = 0; 0 < (m = fread(T, sizeof(sauchar_t), blocksize, fp)); n += m) {
    /* Burrows-Wheeler Transform. */
    pidx = divbwt(T, T, SA, m);
    if(pidx < 0) {
      fprintf(stderr, "%s (bw_transform): %s.\n",
        argv[0],
        (pidx == -1) ? "Invalid arguments" : "Cannot allocate memory");
      exit(EXIT_FAILURE);
    }

    /* Write the bwted data. */
    if((write_int(ofp, pidx) != 4) ||
       (fwrite(T, sizeof(sauchar_t), m, ofp) != m)) {
      fprintf(stderr, "%s: Cannot write to `%s': ", argv[0], ofname);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
  }
  if(ferror(fp)) {
    fprintf(stderr, "%s: Cannot read from `%s': ", argv[0], fname);
    perror(NULL);
    exit(EXIT_FAILURE);
  }
  finish = clock();
  fprintf(stderr, "%" PRIdOFF_T " bytes: %.4f sec\n",
    n, (double)(finish - start) / (double)CLOCKS_PER_SEC);

  /* Close files */
  if(needclose & 1) { fclose(fp); }
  if(needclose & 2) { fclose(ofp); }

  /* Deallocate memory. */
  free(SA);
  free(T);

  return 0;
}
