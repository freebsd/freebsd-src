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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "lib.h"
#include "errarg.h"
#include "error.h"

#include "defs.h"
#include "refid.h"
#include "search.h"

static void usage()
{
  fprintf(stderr, "usage: %s [-nv] [-p database] [-i XYZ] [-t N] keys ...\n",
	  program_name);
  exit(1);
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int search_default = 1;
  search_list list;
  int opt;
  while ((opt = getopt(argc, argv, "nvVi:t:p:")) != EOF)
    switch (opt) {
    case 'V':
      verify_flag = 1;
      break;
    case 'n':
      search_default = 0;
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
	extern const char *version_string;
	fprintf(stderr, "GNU lkbib version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case 'p':
      list.add_file(optarg);
      break;
    case '?':
      usage();
    default:
      assert(0);
    }
  if (optind >= argc)
    usage();
  char *filename = getenv("REFER");
  if (filename)
    list.add_file(filename);
  else if (search_default)
    list.add_file(DEFAULT_INDEX, 1);
  if (list.nfiles() == 0)
    fatal("no databases");
  int total_len = 0;
  int i;
  for (i = optind; i < argc; i++)
    total_len += strlen(argv[i]);
  total_len += argc - optind - 1 + 1; // for spaces and '\0'
  char *buffer = new char[total_len];
  char *ptr = buffer;
  for (i = optind; i < argc; i++) {
    if (i > optind)
      *ptr++ = ' ';
    strcpy(ptr, argv[i]);
    ptr = strchr(ptr, '\0');
  }
  search_list_iterator iter(&list, buffer);
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
  return !count;
}
