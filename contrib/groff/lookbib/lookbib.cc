// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "errarg.h"
#include "error.h"
#include "lib.h"
#include "cset.h"

#include "refid.h"
#include "search.h"

extern "C" {
  int isatty(int);
}

static void usage()
{
  fprintf(stderr, "usage: %s [-v] [-i XYZ] [-t N] database ...\n",
	  program_name);
  exit(1);
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int opt;
  while ((opt = getopt(argc, argv, "vVi:t:")) != EOF)
    switch (opt) {
    case 'V':
      verify_flag = 1;
      break;
    case 'i':
      linear_ignore_fields = optarg;
      break;
    case 't':
      {
	char *ptr;
	long n = strtol(optarg, &ptr, 10);
	if (n == 0 && ptr == optarg) {
	  error("bad integer `%1' in `t' option", optarg);
	  break;
	}
	if (n < 1)
	  n = 1;
	linear_truncate_len = int(n);
	break;
      }
    case 'v':
      {
	extern const char *Version_string;
	fprintf(stderr, "GNU lookbib version %s\n", Version_string);
	fflush(stderr);
	break;
      }
    case '?':
      usage();
    default:
      assert(0);
    }
  if (optind >= argc)
    usage();
  search_list list;
  for (int i = optind; i < argc; i++)
    list.add_file(argv[i]);
  if (list.nfiles() == 0)
    fatal("no databases");
  char line[1024];
  int interactive = isatty(fileno(stdin));
  for (;;) {
    if (interactive) {
      fputs("> ", stderr);
      fflush(stderr);
    }
    if (!fgets(line, sizeof(line), stdin))
      break;
    char *ptr = line;
    while (csspace(*ptr))
      ptr++;
    if (*ptr == '\0')
      continue;
    search_list_iterator iter(&list, line);
    const char *start;
    int len;
    int count;
    for (count = 0; iter.next(&start, &len); count++) {
      if (fwrite(start, 1, len, stdout) != len)
	fatal("write error on stdout: %1", strerror(errno));
      // Can happen for last reference in file.
      if (start[len - 1] != '\n')
	putchar('\n');
      putchar('\n');
    }
    fflush(stdout);
    if (interactive) {
      fprintf(stderr, "%d found\n", count);
      fflush(stderr);
    }
  }
  if (interactive)
    putc('\n', stderr);
  return 0;
}

