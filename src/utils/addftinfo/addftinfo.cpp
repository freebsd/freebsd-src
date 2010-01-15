// -*- C++ -*-
/* Copyright (C) 1989-1992, 2000, 2001 Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include "lib.h"

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "errarg.h"
#include "error.h"
#include "stringclass.h"
#include "cset.h"
#include "guess.h"

extern "C" const char *Version_string;

static void usage(FILE *stream);
static void usage();
static void version();
static void convert_font(const font_params &, FILE *, FILE *);

typedef int font_params::*param_t;

static struct {
  const char *name;
  param_t par;
} param_table[] = {
  { "x-height", &font_params::x_height },
  { "fig-height", &font_params::fig_height },
  { "asc-height", &font_params::asc_height },
  { "body-height", &font_params::body_height },
  { "cap-height", &font_params::cap_height },
  { "comma-depth", &font_params::comma_depth },
  { "desc-depth", &font_params::desc_depth },
  { "body-depth", &font_params::body_depth },
};

// These are all in thousandths of an em.
// These values are correct for PostScript Times Roman.

#define DEFAULT_X_HEIGHT 448
#define DEFAULT_FIG_HEIGHT 676
#define DEFAULT_ASC_HEIGHT 682
#define DEFAULT_BODY_HEIGHT 676
#define DEFAULT_CAP_HEIGHT 662
#define DEFAULT_COMMA_DEPTH 143
#define DEFAULT_DESC_DEPTH 217
#define DEFAULT_BODY_DEPTH 177

int main(int argc, char **argv)
{
  program_name = argv[0];
  int i;
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-v") || !strcmp(argv[i],"--version"))
      version();
    if (!strcmp(argv[i],"--help")) {
      usage(stdout);
      exit(0);
    }
  }
  if (argc < 4)
    usage();
  int resolution;
  if (sscanf(argv[argc-3], "%d", &resolution) != 1)
    usage();
  if (resolution <= 0)
    fatal("resolution must be > 0");
  int unitwidth;
  if (sscanf(argv[argc-2], "%d", &unitwidth) != 1)
    usage();
  if (unitwidth <= 0)
    fatal("unitwidth must be > 0");
  font_params param;
  const char *font = argv[argc-1];
  param.italic = (font[0] != '\0' && strchr(font, '\0')[-1] == 'I');
  param.em = (resolution*unitwidth)/72;
  param.x_height = DEFAULT_X_HEIGHT;
  param.fig_height = DEFAULT_FIG_HEIGHT;
  param.asc_height = DEFAULT_ASC_HEIGHT;
  param.body_height = DEFAULT_BODY_HEIGHT;
  param.cap_height = DEFAULT_CAP_HEIGHT;
  param.comma_depth = DEFAULT_COMMA_DEPTH;
  param.desc_depth = DEFAULT_DESC_DEPTH;
  param.body_depth = DEFAULT_BODY_DEPTH;
  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    if (argv[i][1] == '-' && argv[i][2] == '\0') {
      i++;
      break;
    }
    if (i + 1 >= argc)
      usage();
    size_t j;
    for (j = 0;; j++) {
      if (j >= sizeof(param_table)/sizeof(param_table[0]))
	fatal("parameter `%1' not recognized", argv[i] + 1);
      if (strcmp(param_table[j].name, argv[i] + 1) == 0)
	break;
    }
    if (sscanf(argv[i+1], "%d", &(param.*(param_table[j].par))) != 1)
      fatal("invalid argument `%1'", argv[i+1]);
    i++;
  }    
  if (argc - i != 3)
    usage();
  errno = 0;
  FILE *infp = fopen(font, "r");
  if (infp == 0)
    fatal("can't open `%1': %2", font, strerror(errno));
  convert_font(param, infp, stdout);
  return 0;
}

static void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [-v] [-param value] ... "
		  "resolution unitwidth font\n",
	  program_name);
}
static void usage()
{
  usage(stderr);
  exit(1);
}

static void version()
{
  printf("GNU addftinfo (groff) version %s\n", Version_string);
  exit(0);
}

static int get_line(FILE *fp, string *p)
{
  int c;
  p->clear();
  while ((c = getc(fp)) != EOF) {
    *p += char(c);
    if (c == '\n')
      break;
  }
  return p->length() > 0;
}
  
static void convert_font(const font_params &param, FILE *infp, FILE *outfp)
{
  string s;
  while (get_line(infp, &s)) {
    put_string(s, outfp);
    if (s.length() >= 8
	&& strncmp(&s[0], "charset", 7))
      break;
  }
  while (get_line(infp, &s)) {
    s += '\0';
    string name;
    const char *p = s.contents();
    while (csspace(*p))
      p++;
    while (*p != '\0' && !csspace(*p))
      name += *p++;
    while (csspace(*p))
      p++;
    for (const char *q = s.contents(); q < p; q++)
      putc(*q, outfp);
    char *next;
    char_metric metric;
    metric.width = (int)strtol(p, &next, 10);
    if (next != p) {
      printf("%d", metric.width);
      p = next;
      metric.type = (int)strtol(p, &next, 10);
      if (next != p) {
	name += '\0';
	guess(name.contents(), param, &metric);
	if (metric.sk == 0) {
	  if (metric.left_ic == 0) {
	    if (metric.ic == 0) {
	      if (metric.depth == 0) {
		if (metric.height != 0)
		  printf(",%d", metric.height);
	      }
	      else
		printf(",%d,%d", metric.height, metric.depth);
	    }
	    else
	      printf(",%d,%d,%d", metric.height, metric.depth, metric.ic);
	  }
	  else
	    printf(",%d,%d,%d,%d", metric.height, metric.depth, metric.ic,
		   metric.left_ic);
	}
	else
	  printf(",%d,%d,%d,%d,%d", metric.height, metric.depth, metric.ic,
		 metric.left_ic, metric.sk);
      }
    }
    fputs(p, outfp);
  }
}

