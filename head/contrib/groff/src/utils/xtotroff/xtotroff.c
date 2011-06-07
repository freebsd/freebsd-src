/*
 * xtotroff
 *
 * convert X font metrics into troff font metrics
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#define __GETOPT_PREFIX groff_
#include <getopt.h>

#include "XFontName.h"
#include "DviChar.h"

#define charWidth(fi,c) \
	  ((fi)->per_char[(c) - (fi)->min_char_or_byte2].width)
#define charHeight(fi,c) \
	  ((fi)->per_char[(c) - (fi)->min_char_or_byte2].ascent)
#define charDepth(fi,c) \
	  ((fi)->per_char[(c) - (fi)->min_char_or_byte2].descent)
#define charLBearing(fi,c) \
	  ((fi)->per_char[(c) - (fi)->min_char_or_byte2].lbearing)
#define charRBearing(fi,c) \
	  ((fi)->per_char[(c) - (fi)->min_char_or_byte2].rbearing)

extern const char *Version_string;
static char *program_name;

Display *dpy;
unsigned resolution = 75;
unsigned point_size = 10;

int charExists(XFontStruct * fi, int c)
{
  XCharStruct *p;

  /* `c' is always >= 0 */
  if ((unsigned int) c < fi->min_char_or_byte2
      || (unsigned int) c > fi->max_char_or_byte2)
    return 0;
  p = fi->per_char + (c - fi->min_char_or_byte2);
  return p->lbearing != 0 || p->rbearing != 0 || p->width != 0
	 || p->ascent != 0 || p->descent != 0 || p->attributes != 0;
}

/* Canonicalize the font name by replacing scalable parts by *s. */

static int CanonicalizeFontName(char *font_name, char *canon_font_name)
{
  unsigned int attributes;
  XFontName parsed;

  if (!XParseFontName(font_name, &parsed, &attributes)) {
    fprintf(stderr, "not a standard name: %s\n", font_name);
    return 0;
  }

  attributes &= ~(FontNamePixelSize | FontNameAverageWidth
		  | FontNamePointSize
		  | FontNameResolutionX | FontNameResolutionY);
  XFormatFontName(&parsed, attributes, canon_font_name);
  return 1;
}

static int
FontNamesAmbiguous(const char *font_name, char **names, int count)
{
  char name1[2048], name2[2048];
  int i;

  if (count == 1)
    return 0;

  for (i = 0; i < count; i++) {
    if (!CanonicalizeFontName(names[i], i == 0 ? name1 : name2)) {
      fprintf(stderr, "bad font name: %s\n", names[i]);
      return 1;
    }
    if (i > 0 && strcmp(name1, name2) != 0) {
      fprintf(stderr, "ambiguous font name: %s\n", font_name);
      fprintf(stderr, "  matches %s\n", names[0]);
      fprintf(stderr, "  and %s\n", names[i]);
      return 1;
    }
  }
  return 0;
}

static int MapFont(char *font_name, const char *troff_name)
{
  XFontStruct *fi;
  int count;
  char **names;
  FILE *out;
  unsigned int c;
  unsigned int attributes;
  XFontName parsed;
  int j, k;
  DviCharNameMap *char_map;
  char encoding[256];
  char *s;
  int wid;
  char name_string[2048];

  if (!XParseFontName(font_name, &parsed, &attributes)) {
    fprintf(stderr, "not a standard name: %s\n", font_name);
    return 0;
  }

  attributes &= ~(FontNamePixelSize | FontNameAverageWidth);
  attributes |= FontNameResolutionX;
  attributes |= FontNameResolutionY;
  attributes |= FontNamePointSize;
  parsed.ResolutionX = resolution;
  parsed.ResolutionY = resolution;
  parsed.PointSize = point_size * 10;
  XFormatFontName(&parsed, attributes, name_string);

  names = XListFonts(dpy, name_string, 100000, &count);
  if (count < 1) {
    fprintf(stderr, "bad font name: %s\n", font_name);
    return 0;
  }

  if (FontNamesAmbiguous(font_name, names, count))
    return 0;

  XParseFontName(names[0], &parsed, &attributes);
  sprintf(encoding, "%s-%s", parsed.CharSetRegistry,
	  parsed.CharSetEncoding);
  for (s = encoding; *s; s++)
    if (isupper(*s))
      *s = tolower(*s);
  char_map = DviFindMap(encoding);
  if (!char_map) {
    fprintf(stderr, "not a standard encoding: %s\n", encoding);
    return 0;
  }

  fi = XLoadQueryFont(dpy, names[0]);
  if (!fi) {
    fprintf(stderr, "font does not exist: %s\n", names[0]);
    return 0;
  }

  printf("%s -> %s\n", names[0], troff_name);

  {				/* Avoid race while opening file */
    int fd;
    (void) unlink(troff_name);
    fd = open(troff_name, O_WRONLY | O_CREAT | O_EXCL, 0600);
    out = fdopen(fd, "w");
  }

  if (!out) {
    perror(troff_name);
    return 0;
  }
  fprintf(out, "name %s\n", troff_name);
  if (!strcmp(char_map->encoding, "adobe-fontspecific"))
    fprintf(out, "special\n");
  if (charExists(fi, ' ')) {
    int w = charWidth(fi, ' ');
    if (w > 0)
      fprintf(out, "spacewidth %d\n", w);
  }
  fprintf(out, "charset\n");
  for (c = fi->min_char_or_byte2; c <= fi->max_char_or_byte2; c++) {
    const char *name = DviCharName(char_map, c, 0);
    if (charExists(fi, c)) {
      int param[5];

      wid = charWidth(fi, c);

      fprintf(out, "%s\t%d", name ? name : "---", wid);
      param[0] = charHeight(fi, c);
      param[1] = charDepth(fi, c);
      param[2] = 0;		/* charRBearing (fi, c) - wid */
      param[3] = 0;		/* charLBearing (fi, c) */
      param[4] = 0;		/* XXX */
      for (j = 0; j < 5; j++)
	if (param[j] < 0)
	  param[j] = 0;
      for (j = 4; j >= 0; j--)
	if (param[j] != 0)
	  break;
      for (k = 0; k <= j; k++)
	fprintf(out, ",%d", param[k]);
      fprintf(out, "\t0\t0%o\n", c);

      if (name) {
	for (k = 1; DviCharName(char_map, c, k); k++) {
	  fprintf(out, "%s\t\"\n", DviCharName(char_map, c, k));
	}
      }
    }
  }
  XUnloadFont(dpy, fi->fid);
  fclose(out);
  return 1;
}

static void usage(FILE *stream)
{
  fprintf(stream,
	  "usage: %s [-r resolution] [-s pointsize] FontMap\n",
	  program_name);
}

int main(int argc, char **argv)
{
  char troff_name[1024];
  char font_name[1024];
  char line[1024];
  char *a, *b, c;
  FILE *map;
  int opt;
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };

  program_name = argv[0];

  while ((opt = getopt_long(argc, argv, "gr:s:v", long_options,
			    NULL)) != EOF) {
    switch (opt) {
    case 'g':
      /* unused; just for compatibility */
      break;
    case 'r':
      sscanf(optarg, "%u", &resolution);
      break;
    case 's':
      sscanf(optarg, "%u", &point_size);
      break;
    case 'v':
      printf("xtotroff (groff) version %s\n", Version_string);
      exit(0);
      break;
    case CHAR_MAX + 1: /* --help */
      usage(stdout);
      exit(0);
      break;
    case '?':
      usage(stderr);
      exit(1);
      break;
    }
  }
  if (argc - optind != 1) {
    usage(stderr);
    exit(1);
  }

  dpy = XOpenDisplay(0);
  if (!dpy) {
    fprintf(stderr, "Can't connect to the X server.\n");
    fprintf(stderr,
	    "Make sure the DISPLAY environment variable is set correctly.\n");
    exit(1);
  }

  map = fopen(argv[optind], "r");
  if (map == NULL) {
    perror(argv[optind]);
    exit(1);
  }

  while (fgets(line, sizeof(line), map)) {
    for (a = line, b = troff_name; *a; a++, b++) {
      c = (*b = *a);
      if (c == ' ' || c == '\t')
	break;
    }
    *b = '\0';
    while (*a && (*a == ' ' || *a == '\t'))
      ++a;
    for (b = font_name; *a; a++, b++)
      if ((*b = *a) == '\n')
	break;
    *b = '\0';
    if (!MapFont(font_name, troff_name))
      exit(1);
  }
  exit(0);
}
