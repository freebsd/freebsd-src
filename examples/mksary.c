/*
 * mksary.c for libdivsufsort
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
void
print_help(const char *progname, int status) {
  fprintf(stderr,
          "mksary, a simple suffix array builder, version %s.\n",
          divsufsort_version());
  fprintf(stderr, "usage: %s INFILE OUTFILE\n\n", progname);
  exit(status);
}

int
main(int argc, const char *argv[]) {
  FILE *fp, *ofp;
  const char *fname, *ofname;
  sauchar_t *T;
  saidx_t *SA;
  LFS_OFF_T n;
  clock_t start, finish;
  saint_t needclose = 3;

  /* Check arguments. */
  if((argc == 1) ||
     (strcmp(argv[1], "-h") == 0) ||
     (strcmp(argv[1], "--help") == 0)) { print_help(argv[0], EXIT_SUCCESS); }
  if(argc != 3) { print_help(argv[0], EXIT_FAILURE); }

  /* Open a file for reading. */
  if(strcmp(argv[1], "-") != 0) {
#if HAVE_FOPEN_S
    if(fopen_s(&fp, fname = argv[1], "rb") != 0) {
#else
    if((fp = LFS_FOPEN(fname = argv[1], "rb")) == NULL) {
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

  /* Open a file for writing. */
  if(strcmp(argv[2], "-") != 0) {
#if HAVE_FOPEN_S
    if(fopen_s(&ofp, ofname = argv[2], "wb") != 0) {
#else
    if((ofp = LFS_FOPEN(ofname = argv[2], "wb")) == NULL) {
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
    if(0x7fffffff <= n) {
      fprintf(stderr, "%s: Input file `%s' is too big.\n", argv[0], fname);
      exit(EXIT_FAILURE);
    }
  } else {
    fprintf(stderr, "%s: Cannot fseek `%s': ", argv[0], fname);
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  /* Allocate 5blocksize bytes of memory. */
  T = (sauchar_t *)malloc((size_t)n * sizeof(sauchar_t));
  SA = (saidx_t *)malloc((size_t)n * sizeof(saidx_t));
  if((T == NULL) || (SA == NULL)) {
    fprintf(stderr, "%s: Cannot allocate memory.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Read n bytes of data. */
  if(fread(T, sizeof(sauchar_t), (size_t)n, fp) != (size_t)n) {
    fprintf(stderr, "%s: %s `%s': ",
      argv[0],
      (ferror(fp) || !feof(fp)) ? "Cannot read from" : "Unexpected EOF in",
      fname);
    perror(NULL);
    exit(EXIT_FAILURE);
  }
  if(needclose & 1) { fclose(fp); }

  /* Construct the suffix array. */
  fprintf(stderr, "%s: %" PRIdOFF_T " bytes ... ", fname, n);
  start = clock();
  if(divsufsort(T, SA, (saidx_t)n) != 0) {
    fprintf(stderr, "%s: Cannot allocate memory.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  finish = clock();
  fprintf(stderr, "%.4f sec\n", (double)(finish - start) / (double)CLOCKS_PER_SEC);

  /* Write the suffix array. */
  if(fwrite(SA, sizeof(saidx_t), (size_t)n, ofp) != (size_t)n) {
    fprintf(stderr, "%s: Cannot write to `%s': ", argv[0], ofname);
    perror(NULL);
    exit(EXIT_FAILURE);
  }
  if(needclose & 2) { fclose(ofp); }

  /* Deallocate memory. */
  free(SA);
  free(T);

  return 0;
}
