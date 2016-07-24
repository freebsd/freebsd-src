/*
 * sasearch.c for libdivsufsort
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
#include <divsufsort.h>
#include "lfs.h"


static
void
print_help(const char *progname, int status) {
  fprintf(stderr,
          "sasearch, a simple SA-based full-text search tool, version %s\n",
          divsufsort_version());
  fprintf(stderr, "usage: %s PATTERN FILE SAFILE\n\n", progname);
  exit(status);
}

int
main(int argc, const char *argv[]) {
  FILE *fp;
  const char *P;
  sauchar_t *T;
  saidx_t *SA;
  LFS_OFF_T n;
  size_t Psize;
  saidx_t i, size, left;

  if((argc == 1) ||
     (strcmp(argv[1], "-h") == 0) ||
     (strcmp(argv[1], "--help") == 0)) { print_help(argv[0], EXIT_SUCCESS); }
  if(argc != 4) { print_help(argv[0], EXIT_FAILURE); }

  P = argv[1];
  Psize = strlen(P);

  /* Open a file for reading. */
#if HAVE_FOPEN_S
  if(fopen_s(&fp, argv[2], "rb") != 0) {
#else
  if((fp = LFS_FOPEN(argv[2], "rb")) == NULL) {
#endif
    fprintf(stderr, "%s: Cannot open file `%s': ", argv[0], argv[2]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  /* Get the file size. */
  if(LFS_FSEEK(fp, 0, SEEK_END) == 0) {
    n = LFS_FTELL(fp);
    rewind(fp);
    if(n < 0) {
      fprintf(stderr, "%s: Cannot ftell `%s': ", argv[0], argv[2]);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
  } else {
    fprintf(stderr, "%s: Cannot fseek `%s': ", argv[0], argv[2]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  /* Allocate 5n bytes of memory. */
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
      argv[2]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }
  fclose(fp);

  /* Open the SA file for reading. */
#if HAVE_FOPEN_S
  if(fopen_s(&fp, argv[3], "rb") != 0) {
#else
  if((fp = LFS_FOPEN(argv[3], "rb")) == NULL) {
#endif
    fprintf(stderr, "%s: Cannot open file `%s': ", argv[0], argv[3]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  /* Read n * sizeof(saidx_t) bytes of data. */
  if(fread(SA, sizeof(saidx_t), (size_t)n, fp) != (size_t)n) {
    fprintf(stderr, "%s: %s `%s': ",
      argv[0],
      (ferror(fp) || !feof(fp)) ? "Cannot read from" : "Unexpected EOF in",
      argv[3]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }
  fclose(fp);

  /* Search and print */
  size = sa_search(T, (saidx_t)n,
                   (const sauchar_t *)P, (saidx_t)Psize,
                   SA, (saidx_t)n, &left);
  for(i = 0; i < size; ++i) {
    fprintf(stdout, "%" PRIdSAIDX_T "\n", SA[left + i]);
  }

  /* Deallocate memory. */
  free(SA);
  free(T);

  return 0;
}
