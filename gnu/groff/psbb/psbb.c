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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif

struct bounding_box {
  int llx, lly, urx, ury;
};

#ifdef __STDC__
const char *do_file(FILE *, struct bounding_box *);
int parse_bounding_box(char *, struct bounding_box *);
#else
#define const /* as nothing */
const char *do_file();
int parse_bounding_box();
#endif

int main(argc, argv)
int argc;
char **argv;
{
  FILE *fp;
  const char *message;
  struct bounding_box bb;
  if (argc != 2) {
    fprintf(stderr, "usage: %s filename\n", argv[0]);
    exit(3);
  }
  errno = 0;
  fp = fopen(argv[1], "r");
  if (fp == NULL) {
    fprintf(stderr, "%s: can't open `%s': ", argv[0], argv[1]);
    perror((char *)NULL);
    exit(2);
  }
  message = do_file(fp, &bb);
  if (message) {
    fprintf(stderr, "%s: ", argv[0]);
    fprintf(stderr, message, argv[1]);
    putc('\n', stderr);
    exit(1);
  }
  printf("%d %d %d %d\n", bb.llx, bb.lly, bb.urx, bb.ury);
  exit(0);
}
      
/* If the bounding box was found return NULL, and store the bounding box
in bb. If the bounding box was not found return a string suitable for
giving to printf with the filename as an argument saying why not. */

const char *do_file(fp, bb)
FILE *fp;
struct bounding_box *bb;
{
  int bb_at_end = 0;
  char buf[256];
  if (!fgets(buf, sizeof(buf), fp))
    return "%s is empty";
  if (strncmp("%!PS-Adobe-", buf, 11) != 0)
    return "%s is not conforming";
  while (fgets(buf, sizeof(buf), fp) != 0) {
    if (buf[0] != '%' || buf[1] != '%'
	|| strncmp(buf + 2, "EndComments", 11) == 0)
      break;
    if (strncmp(buf + 2, "BoundingBox:", 12) == 0) {
      int res = parse_bounding_box(buf + 14, bb);
      if (res == 1)
	return NULL;
      else if (res == 2) {
	bb_at_end = 1;
	break;
      }
      else
	return "the arguments to the %%%%BoundingBox comment in %s are bad";
    }
  }
  if (bb_at_end) {
    long offset;
    int last_try = 0;
    /* in the trailer, the last BoundingBox comment is significant */
    for (offset = 512; !last_try; offset *= 2) {
      int had_trailer = 0;
      int got_bb = 0;
      if (offset > 32768 || fseek(fp, -offset, 2) == -1) {
	last_try = 1;
	if (fseek(fp, 0L, 0) == -1)
	  break;
      }
      while (fgets(buf, sizeof(buf), fp) != 0) {
	if (buf[0] == '%' && buf[1] == '%') {
	  if (!had_trailer) {
	    if (strncmp(buf + 2, "Trailer", 7) == 0)
	      had_trailer = 1;
	  }
	  else {
	    if (strncmp(buf + 2, "BoundingBox:", 12) == 0) {
	      int res = parse_bounding_box(buf + 14, bb);
	      if (res == 1)
		got_bb = 1;
	      else if (res == 2)
		return "`(atend)' not allowed in trailer";
	      else
		return "the arguments to the %%%%BoundingBox comment in %s are bad";
	    }
	  }
	}
      }
      if (got_bb)
	return NULL;
    }
  }
  return "%%%%BoundingBox comment not found in %s";
}

/* Parse the argument to a %%BoundingBox comment. Return 1 if it
contains 4 numbers, 2 if it contains (atend), 0 otherwise. */

int parse_bounding_box(p, bb)
char *p;
struct bounding_box *bb;
{
  if (sscanf(p, "%d %d %d %d",
	     &bb->llx, &bb->lly, &bb->urx, &bb->ury) == 4)
    return 1;
  else {
    /* The Document Structuring Conventions say that the numbers
       should be integers. Unfortunately some broken applications
       get this wrong. */
    double x1, x2, x3, x4;
    if (sscanf(p, "%lf %lf %lf %lf", &x1, &x2, &x3, &x4) == 4) {
      bb->llx = (int)x1;
      bb->lly = (int)x2;
      bb->urx = (int)x3;
      bb->ury = (int)x4;
      return 1;
    }
    else {
      for (; *p == ' ' || *p == '\t'; p++)
	;
      if (strncmp(p, "(atend)", 7) == 0) {
	return 2;
      }
    }
  }
  return 0;
}

