// -*- C++ -*-
/* Copyright (C) 1994, 2000, 2001, 2003, 2004 Free Software Foundation, Inc.
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

/*
TODO
devise new names for useful characters
option to specify symbol sets to look in
put filename in error messages (or fix lib)
*/

#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include "assert.h"
#include "posix.h"
#include "errarg.h"
#include "error.h"
#include "cset.h"
#include "nonposix.h"
#include "unicode.h"

extern "C" const char *Version_string;
extern const char *hp_msl_to_unicode_code(const char *);

#define SIZEOF(v) (sizeof(v)/sizeof(v[0]))
#define equal(a, b) (strcmp(a, b) == 0)
// only valid if is_uname(c) has returned true
#define is_decomposed(c) strchr(c, '_')

#define NO       0
#define YES      1

#define MSL      0
#define SYMSET   1
#define UNICODE  2

#define UNNAMED "---"

static double multiplier = 3.0;	// make Agfa-based unitwidth an integer

inline
int scale(int n)
{
  return int(n * multiplier + 0.5);
}

// tags in TFM file

enum tag_type {
  min_tag = 400,
  type_tag = 400,
  copyright_tag = 401,
  comment_tag = 402,
  charcode_tag = 403,		// MSL for Intellifont, Unicode for TrueType
  symbol_set_tag = 404,
  unique_identifier_tag = 405,
  inches_per_point_tag = 406,
  nominal_point_size_tag = 407,
  design_units_per_em_tag = 408,
  posture_tag = 409,
  type_structure_tag = 410,
  stroke_weight_tag = 411,
  spacing_tag = 412,
  slant_tag = 413,
  appearance_width_tag = 414,
  serif_style_tag = 415,
  font_name_tag = 417,
  typeface_source_tag = 418,
  average_width_tag = 419,
  max_width_tag = 420,
  word_spacing_tag = 421,
  recommended_line_spacing_tag = 422,
  cap_height_tag = 423,
  x_height_tag = 424,
  max_ascent_tag = 425,
  max_descent_tag = 426,
  lower_ascent_tag = 427,
  lower_descent_tag = 428,
  underscore_depth_tag = 429,
  underscore_thickness_tag = 430,
  uppercase_accent_height_tag = 431,
  lowercase_accent_height_tag = 432,
  width_tag = 433,
  vertical_escapement_tag = 434,
  left_extent_tag = 435,
  right_extent_tag = 436,
  ascent_tag = 437,
  descent_tag = 438,
  pair_kern_tag = 439,
  sector_kern_tag = 440,
  track_kern_tag = 441,
  typeface_tag = 442,
  panose_tag = 443,
  max_tag = 443
};

const char *tag_name[] = {
  "Symbol Set",
  "Font Type"		// MSL for Intellifont, Unicode for TrueType
};

// types in TFM file
enum {
  BYTE_TYPE = 1,
  ASCII_TYPE = 2,		// NUL-terminated string
  USHORT_TYPE = 3,
  LONG_TYPE = 4,		// unused
  RATIONAL_TYPE = 5,		// 8-byte numerator + 8-byte denominator
  SIGNED_BYTE_TYPE = 16,	// unused
  SIGNED_SHORT_TYPE = 17,
  SIGNED_LONG_TYPE = 18		// unused
};

typedef unsigned char byte;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned int uint32;

class File {
public:
  File(const char *);
  void skip(int n);
  byte get_byte();
  uint16 get_uint16();
  uint32 get_uint32();
  uint32 get_uint32(char *orig);
  void seek(uint32 n);
private:
  unsigned char *buf_;
  const unsigned char *ptr_;
  const unsigned char *end_;
};

struct entry {
  char present;
  uint16 type;
  uint32 count;
  uint32 value;
  char orig_value[4];
  entry() : present(0) { }
};

struct char_info {
  uint16 charcode;
  uint16 width;
  int16 ascent;
  int16 descent;
  int16 left_extent;
  uint16 right_extent;
  uint16 symbol_set;
  unsigned char code;
};

const uint16 NO_GLYPH = 0xffff;
const uint16 NO_SYMBOL_SET = 0;

struct name_list {
  char *name;
  name_list *next;
  name_list(const char *s, name_list *p) : name(strsave(s)), next(p) { }
  ~name_list() { a_delete name; }
};

struct symbol_set {
  uint16 select;
  uint16 index[256];
};

#define SYMBOL_SET(n, c) ((n) * 32 + ((c) - 64))

uint16 text_symbol_sets[] = {
  SYMBOL_SET(19, 'U'),		// Windows Latin 1 ("ANSI", code page 1252)
  SYMBOL_SET(9, 'E'),		// Windows Latin 2, Code Page 1250
  SYMBOL_SET(5, 'T'),		// Code Page 1254
  SYMBOL_SET(7, 'J'),		// Desktop
  SYMBOL_SET(6, 'J'),		// Microsoft Publishing
  SYMBOL_SET(0, 'N'),		// Latin 1 (subset of 19U,
				// so we should never get here)
  SYMBOL_SET(2, 'N'),		// Latin 2 (subset of 9E,
				// so we should never get here)
  SYMBOL_SET(8, 'U'),		// HP Roman 8
  SYMBOL_SET(10, 'J'),		// PS Standard
  SYMBOL_SET(9, 'U'),		// Windows 3.0 "ANSI"
  SYMBOL_SET(1, 'U'),		// U.S. Legal

  SYMBOL_SET(12, 'J'),		// MC Text
  SYMBOL_SET(10, 'U'),		// PC Code Page 437
  SYMBOL_SET(11, 'U'),		// PC Code Page 437N
  SYMBOL_SET(17, 'U'),		// PC Code Page 852
  SYMBOL_SET(12, 'U'),		// PC Code Page 850
  SYMBOL_SET(9, 'T'),		// PC Code Page 437T
  0
};

uint16 special_symbol_sets[] = {
  SYMBOL_SET(8, 'M'),		// Math 8
  SYMBOL_SET(5, 'M'),		// PS Math
  SYMBOL_SET(15, 'U'),		// Pi font
  SYMBOL_SET(13, 'J'),		// Ventura International
  SYMBOL_SET(19, 'M'),		// Symbol font
  SYMBOL_SET(579, 'L'),		// Wingdings
  0
};

entry tags[max_tag + 1 - min_tag];

char_info *char_table;
uint32 nchars = 0;

unsigned int charcode_name_table_size = 0;
name_list **charcode_name_table = NULL;

symbol_set *symbol_set_table;
unsigned int n_symbol_sets;

static int debug_flag = NO;
static int special_flag = NO;	// not a special font
static int italic_flag = NO;	// don't add italic correction
static int italic_sep;
static int all_flag = NO;	// don't include glyphs not in mapfile
static int quiet_flag = NO;	// don't suppress warnings about symbols not found

static char *hp_msl_to_ucode_name(int);
static char *unicode_to_ucode_name(int);
static int is_uname(char *);
static char *show_symset(unsigned int);
static void usage(FILE *);
static void usage();
static const char *xbasename(const char *);
static void read_tags(File &);
static int check_type();
static void check_units(File &, const int, double *, double *);
static int read_map(const char *, const int);
static void require_tag(tag_type);
static void dump_ascii(File &, tag_type);
static void dump_tags(File &);
static void dump_symbol_sets(File &);
static void dump_symbols(int);
static void output_font_name(File &);
static void output_spacewidth();
static void output_pclweight();
static void output_pclproportional();
static void read_and_output_pcltypeface(File &);
static void output_pclstyle();
static void output_slant();
static void output_ligatures();
static void read_symbol_sets(File &);
static void read_and_output_kernpairs(File &);
static void output_charset(const int);
static void read_char_table(File &);

inline
entry &tag_info(tag_type t)
{
  return tags[t - min_tag];
}

int
main(int argc, char **argv)
{
  program_name = argv[0];

  int opt;
  int res = 1200;		// PCL unit of measure for cursor moves
  int scalesize = 4;		// LaserJet 4 only allows 1/4 point increments
  int unitwidth = 6350;
  double ppi;			// points per inch
  double upem;			// design units per em

  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((opt = getopt_long(argc, argv, "adsqvi:", long_options, NULL)) != EOF) {
    switch (opt) {
    case 'a':
      all_flag = YES;
      break;
    case 'd':
      debug_flag = YES;
      break;
    case 's':
      special_flag = YES;
      break;
    case 'i':
      italic_flag = YES;
      italic_sep = atoi(optarg);	// design units
      break;
    case 'q':
      quiet_flag = YES;		// suppress warnings about symbols not found
      break;
    case 'v':
      printf("GNU hpftodit (groff) version %s\n", Version_string);
      exit(0);
      break;
    case CHAR_MAX + 1: // --help
      usage(stdout);
      exit(0);
      break;
    case '?':
      usage();
      break;
    default:
      assert(0);
    }
  }

  if (debug_flag && argc - optind < 1)
    usage();
  else if (!debug_flag && argc - optind != 3)
    usage();
  File f(argv[optind]);
  read_tags(f);
  int tfm_type = check_type();
  if (debug_flag)
    dump_tags(f);
  if (!debug_flag && !read_map(argv[optind + 1], tfm_type))
    exit(1);
  else if (debug_flag && argc - optind > 1)
    read_map(argv[optind + 1], tfm_type);
  current_filename = NULL;
  current_lineno = -1;		// no line numbers
  if (!debug_flag && !equal(argv[optind + 2], "-"))
    if (freopen(argv[optind + 2], "w", stdout) == NULL)
      fatal("cannot open `%1': %2", argv[optind + 2], strerror(errno));
  current_filename = argv[optind];

  check_units(f, tfm_type, &ppi, &upem);
  if (tfm_type == UNICODE)	// don't calculate for Intellifont TFMs
    multiplier = double(res) / upem / ppi * unitwidth / scalesize;
  if (italic_flag)
    // convert from thousandths of an em to design units
    italic_sep = int(italic_sep * upem / 1000 + 0.5);

  read_char_table(f);
  if (nchars == 0)
    fatal("no characters");

  if (!debug_flag) {
    output_font_name(f);
    printf("name %s\n", xbasename(argv[optind + 2]));
    if (special_flag)
      printf("special\n");
    output_spacewidth();
    output_slant();
    read_and_output_pcltypeface(f);
    output_pclproportional();
    output_pclweight();
    output_pclstyle();
  }
  read_symbol_sets(f);
  if (debug_flag)
    dump_symbols(tfm_type);
  else {
    output_ligatures();
    read_and_output_kernpairs(f);
    output_charset(tfm_type);
  }
  return 0;
}

static void
usage(FILE *stream)
{
  fprintf(stream,
	  "usage: %s [-s] [-a] [-q] [-i n] tfm_file map_file output_font\n"
	  "       %s -d tfm_file [map_file]\n",
	  program_name, program_name);
}

static void
usage()
{
  usage(stderr);
  exit(1);
}

File::File(const char *s)
{
  // We need to read the file in binary mode because hpftodit relies
  // on byte counts.
  int fd = open(s, O_RDONLY | O_BINARY);
  if (fd < 0)
    fatal("cannot open `%1': %2", s, strerror(errno));
  current_filename = s;
  struct stat sb;
  if (fstat(fd, &sb) < 0)
    fatal("cannot stat: %1", strerror(errno));
  if (!S_ISREG(sb.st_mode))
    fatal("not a regular file");
  buf_ = new unsigned char[sb.st_size];
  long nread = read(fd, buf_, sb.st_size);
  if (nread < 0)
    fatal("read error: %1", strerror(errno));
  if (nread != sb.st_size)
    fatal("read unexpected number of bytes");
  ptr_ = buf_;
  end_ = buf_ + sb.st_size;
}

void
File::skip(int n)
{
  if (end_ - ptr_ < n)
    fatal("unexpected end of file");
  ptr_ += n;
}

void
File::seek(uint32 n)
{
  if (uint32(end_ - buf_) < n)
    fatal("unexpected end of file");
  ptr_ = buf_ + n;
}

byte
File::get_byte()
{
  if (ptr_ >= end_)
    fatal("unexpected end of file");
  return *ptr_++;
}

uint16
File::get_uint16()
{
  if (end_ - ptr_ < 2)
    fatal("unexpected end of file");
  uint16 n = *ptr_++;
  return n + (*ptr_++ << 8);
}

uint32
File::get_uint32()
{
  if (end_ - ptr_ < 4)
    fatal("unexpected end of file");
  uint32 n = *ptr_++;
  for (int i = 0; i < 3; i++)
    n += *ptr_++ << (i + 1)*8;
  return n;
}

uint32
File::get_uint32(char *orig)
{
  if (end_ - ptr_ < 4)
    fatal("unexpected end of file");
  unsigned char v = *ptr_++;
  uint32 n = v;
  orig[0] = v;
  for (int i = 1; i < 4; i++) {
    v = *ptr_++;
    orig[i] = v;
    n += v << i*8;
  }
  return n;
}

static void
read_tags(File &f)
{
  if (f.get_byte() != 'I' || f.get_byte() != 'I')
    fatal("not an Intel format TFM file");
  f.skip(6);
  uint16 ntags = f.get_uint16();
  entry dummy;
  for (uint16 i = 0; i < ntags; i++) {
    uint16 tag = f.get_uint16();
    entry *p;
    if (min_tag <= tag && tag <= max_tag)
      p = tags + (tag - min_tag);
    else
      p = &dummy;
    p->present = 1;
    p->type = f.get_uint16();
    p->count = f.get_uint32();
    p->value = f.get_uint32(p->orig_value);
  }
}

static int
check_type()
{
  require_tag(type_tag);
  int tfm_type = tag_info(type_tag).value;
  switch (tfm_type) {
    case MSL:
    case UNICODE:
      break;
    case SYMSET:
      fatal("cannot handle Symbol Set TFM files");
      break;
    default:
      fatal("unknown type tag %1", tfm_type);
  }
  return tfm_type;
}

static void
check_units(File &f, const int tfm_type, double *ppi, double *upem)
{
  require_tag(design_units_per_em_tag);
  f.seek(tag_info(design_units_per_em_tag).value);
  uint32 num = f.get_uint32();
  uint32 den = f.get_uint32();
  if (tfm_type == MSL && (num != 8782 || den != 1))
    fatal("design units per em != 8782/1");
  *upem = double(num) / den;
  require_tag(inches_per_point_tag);
  f.seek(tag_info(inches_per_point_tag).value);
  num = f.get_uint32();
  den = f.get_uint32();
  if (tfm_type == MSL && (num != 100 || den != 7231))
    fatal("inches per point not 100/7231");
  *ppi = double(den) / num;
}

static void
require_tag(tag_type t)
{
  if (!tag_info(t).present)
    fatal("tag %1 missing", int(t));
}

// put a human-readable font name in the file
static void
output_font_name(File &f)
{
  char *p;

  if (!tag_info(font_name_tag).present)
    return;
  int count = tag_info(font_name_tag).count;
  char *font_name = new char[count];

  if (count > 4) {	// value is a file offset to the string
    f.seek(tag_info(font_name_tag).value);
    int n = count;
    p = font_name;
    while (--n)
      *p++ = f.get_byte();
  }
  else			// orig_value contains the string
    sprintf(font_name, "%.*s",
	    count, tag_info(font_name_tag).orig_value);

  // remove any trailing space
  p = font_name + count - 1;
  while (csspace(*--p))
    ;
  *(p + 1) = '\0';
  printf("# %s\n", font_name);
  delete font_name;
}

static void
output_spacewidth()
{
  require_tag(word_spacing_tag);
  printf("spacewidth %d\n", scale(tag_info(word_spacing_tag).value));
}

static void
read_symbol_sets(File &f)
{
  uint32 symbol_set_dir_length = tag_info(symbol_set_tag).count;
  uint16 *symbol_set_selectors;
  n_symbol_sets = symbol_set_dir_length/14;
  symbol_set_table = new symbol_set[n_symbol_sets];
  unsigned int i;

  for (i = 0; i < nchars; i++)
    char_table[i].symbol_set = NO_SYMBOL_SET;

  for (i = 0; i < n_symbol_sets; i++) {
    f.seek(tag_info(symbol_set_tag).value + i*14);
    (void)f.get_uint32();		// offset to symbol set name
    uint32 off1 = f.get_uint32();	// offset to selection string
    uint32 off2 = f.get_uint32();	// offset to symbol set index array

    f.seek(off1);
    uint16 kind = 0;			// HP-GL "Kind 1" symbol set value
    unsigned int j;
    for (j = 0; j < off2 - off1; j++) {
      unsigned char c = f.get_byte();
      if ('0' <= c && c <= '9')		// value
	kind = kind*10 + (c - '0');
      else if ('A' <= c && c <= 'Z')	// terminator
	kind = kind*32 + (c - 64);
    }
    symbol_set_table[i].select = kind;
    for (j = 0; j < 256; j++)
      symbol_set_table[i].index[j] = f.get_uint16();
  }

  symbol_set_selectors = (special_flag ? special_symbol_sets
				       : text_symbol_sets);
  for (i = 0; symbol_set_selectors[i] != 0; i++) {
    unsigned int j;
    for (j = 0; j < n_symbol_sets; j++)
      if (symbol_set_table[j].select == symbol_set_selectors[i])
	break;
    if (j < n_symbol_sets) {
      for (int k = 0; k < 256; k++) {
	uint16 idx = symbol_set_table[j].index[k];
	if (idx != NO_GLYPH
	    && char_table[idx].symbol_set == NO_SYMBOL_SET) {
	  char_table[idx].symbol_set = symbol_set_table[j].select;
	  char_table[idx].code = k;
	}
      }
    }
  }

  if (all_flag)
    return;

  symbol_set_selectors = (special_flag ? text_symbol_sets
				       : special_symbol_sets);
  for (i = 0; symbol_set_selectors[i] != 0; i++) {
    unsigned int j;
    for (j = 0; j < n_symbol_sets; j++)
      if (symbol_set_table[j].select == symbol_set_selectors[i])
	break;
    if (j < n_symbol_sets) {
      for (int k = 0; k < 256; k++) {
	uint16 idx = symbol_set_table[j].index[k];
	if (idx != NO_GLYPH
	    && char_table[idx].symbol_set == NO_SYMBOL_SET) {
	  char_table[idx].symbol_set = symbol_set_table[j].select;
	  char_table[idx].code = k;
	}
      }
    }
  }
  return;
}

static void
read_char_table(File &f)
{
  require_tag(charcode_tag);
  nchars = tag_info(charcode_tag).count;
  char_table = new char_info[nchars];

  f.seek(tag_info(charcode_tag).value);
  uint32 i;
  for (i = 0; i < nchars; i++)
    char_table[i].charcode = f.get_uint16();

  require_tag(width_tag);
  f.seek(tag_info(width_tag).value);
  for (i = 0; i < nchars; i++)
    char_table[i].width = f.get_uint16();

  require_tag(ascent_tag);
  f.seek(tag_info(ascent_tag).value);
  for (i = 0; i < nchars; i++) {
    char_table[i].ascent = f.get_uint16();
    if (char_table[i].ascent < 0)
      char_table[i].ascent = 0;
  }

  require_tag(descent_tag);
  f.seek(tag_info(descent_tag).value);
  for (i = 0; i < nchars; i++) {
    char_table[i].descent = f.get_uint16();
    if (char_table[i].descent > 0)
      char_table[i].descent = 0;
  }

  require_tag(left_extent_tag);
  f.seek(tag_info(left_extent_tag).value);
  for (i = 0; i < nchars; i++)
    char_table[i].left_extent = int16(f.get_uint16());

  require_tag(right_extent_tag);
  f.seek(tag_info(right_extent_tag).value);
  for (i = 0; i < nchars; i++)
    char_table[i].right_extent = f.get_uint16();
}

static void
output_pclweight()
{
  require_tag(stroke_weight_tag);
  int stroke_weight = tag_info(stroke_weight_tag).value;
  int pcl_stroke_weight;
  if (stroke_weight < 128)
    pcl_stroke_weight = -3;
  else if (stroke_weight == 128)
    pcl_stroke_weight = 0;
  else if (stroke_weight <= 145)
    pcl_stroke_weight = 1;
  else if (stroke_weight <= 179)
    pcl_stroke_weight = 3;
  else
    pcl_stroke_weight = 4;
  printf("pclweight %d\n", pcl_stroke_weight);
}

static void
output_pclproportional()
{
  require_tag(spacing_tag);
  printf("pclproportional %d\n", tag_info(spacing_tag).value == 0);
}

static void
read_and_output_pcltypeface(File &f)
{
  printf("pcltypeface ");
  require_tag(typeface_tag);
  if (tag_info(typeface_tag).count > 4) {
    f.seek(tag_info(typeface_tag).value);
    for (uint32 i = 0; i < tag_info(typeface_tag).count; i++) {
      unsigned char c = f.get_byte();
      if (c == '\0')
	break;
      putchar(c);
    }
  }
  else
    printf("%.4s", tag_info(typeface_tag).orig_value);
  printf("\n");
}

static void
output_pclstyle()
{
  unsigned pcl_style = 0;
  // older tfms don't have the posture tag
  if (tag_info(posture_tag).present) {
    if (tag_info(posture_tag).value)
      pcl_style |= 1;
  }
  else {
    require_tag(slant_tag);
    if (tag_info(slant_tag).value != 0)
      pcl_style |= 1;
  }
  require_tag(appearance_width_tag);
  if (tag_info(appearance_width_tag).value < 100) // guess
    pcl_style |= 4;
  printf("pclstyle %d\n", pcl_style);
}

static void
output_slant()
{
  require_tag(slant_tag);
  int slant = int16(tag_info(slant_tag).value);
  if (slant != 0)
    printf("slant %f\n", slant/100.0);
}

static void
output_ligatures()
{
  // don't use ligatures for fixed space font
  require_tag(spacing_tag);
  if (tag_info(spacing_tag).value != 0)
    return;
  static const char *ligature_names[] = {
    "fi", "fl", "ff", "ffi", "ffl"
    };

  static const char *ligature_chars[] = {
    "fi", "fl", "ff", "Fi", "Fl"
    };

  unsigned ligature_mask = 0;
  unsigned int i;
  for (i = 0; i < nchars; i++) {
    uint16 charcode = char_table[i].charcode;
    if (charcode < charcode_name_table_size
	&& char_table[i].symbol_set != NO_SYMBOL_SET) {
      for (name_list *p = charcode_name_table[charcode]; p; p = p->next)
	for (unsigned int j = 0; j < SIZEOF(ligature_chars); j++)
	  if (strcmp(p->name, ligature_chars[j]) == 0) {
	    ligature_mask |= 1 << j;
	    break;
	  }
      }
    }
  if (ligature_mask) {
    printf("ligatures");
    for (i = 0; i < SIZEOF(ligature_names); i++)
      if (ligature_mask & (1 << i))
	printf(" %s", ligature_names[i]);
    printf(" 0\n");
  }
}

static void
read_and_output_kernpairs(File &f)
{
  if (tag_info(pair_kern_tag).present) {
    printf("kernpairs\n");
    f.seek(tag_info(pair_kern_tag).value);
    uint16 n_pairs = f.get_uint16();
    for (int i = 0; i < n_pairs; i++) {
      uint16 i1 = f.get_uint16();
      uint16 i2 = f.get_uint16();
      int16 val = int16(f.get_uint16());
      if (char_table[i1].symbol_set != NO_SYMBOL_SET
	  && char_table[i2].symbol_set != NO_SYMBOL_SET
	  && char_table[i1].charcode < charcode_name_table_size
	  && char_table[i2].charcode < charcode_name_table_size) {
	for (name_list *p = charcode_name_table[char_table[i1].charcode];
	     p;
	     p = p->next)
	  for (name_list *q = charcode_name_table[char_table[i2].charcode];
	       q;
	       q = q->next)
	    if (!equal(p->name, UNNAMED) && !equal(q->name, UNNAMED))
		printf("%s %s %d\n", p->name, q->name, scale(val));
      }
    }
  }
}

static void
output_charset(const int tfm_type)
{
  require_tag(slant_tag);
  double slant_angle = int16(tag_info(slant_tag).value)*PI/18000.0;
  double slant = sin(slant_angle)/cos(slant_angle);

  if (italic_flag)
    require_tag(x_height_tag);
  require_tag(lower_ascent_tag);
  require_tag(lower_descent_tag);

  printf("charset\n");
  unsigned int i;
  for (i = 0; i < nchars; i++) {
    uint16 charcode = char_table[i].charcode;

    // the glyph is bound to one of the searched symbol sets
    if (char_table[i].symbol_set != NO_SYMBOL_SET) {
      // the character was in the map file
      if (charcode < charcode_name_table_size && charcode_name_table[charcode])
	printf("%s", charcode_name_table[charcode]->name);
      else if (!all_flag)
	continue;
      else if (tfm_type == MSL)
	printf(hp_msl_to_ucode_name(charcode));
      else
	printf(unicode_to_ucode_name(charcode));

      printf("\t%d,%d",
	     scale(char_table[i].width), scale(char_table[i].ascent));

      int depth = scale(-char_table[i].descent);
      if (depth < 0)
	depth = 0;
      int italic_correction = 0;
      int left_italic_correction = 0;
      int subscript_correction = 0;

      if (italic_flag) {
	italic_correction = scale(char_table[i].right_extent
				  - char_table[i].width
				  + italic_sep);
	if (italic_correction < 0)
	  italic_correction = 0;
	subscript_correction = int((tag_info(x_height_tag).value
				    * slant * .8) + .5);
	if (subscript_correction > italic_correction)
	  subscript_correction = italic_correction;
	left_italic_correction = scale(italic_sep
				       - char_table[i].left_extent);
      }

      if (subscript_correction != 0)
	printf(",%d,%d,%d,%d",
	       depth, italic_correction, left_italic_correction,
	       subscript_correction);
      else if (left_italic_correction != 0)
	printf(",%d,%d,%d", depth, italic_correction, left_italic_correction);
      else if (italic_correction != 0)
	printf(",%d,%d", depth, italic_correction);
      else if (depth != 0)
	printf(",%d", depth);
      // This is fairly arbitrary.  Fortunately it doesn't much matter.
      unsigned type = 0;
      if (char_table[i].ascent > int16(tag_info(lower_ascent_tag).value)*9/10)
	type |= 2;
      if (char_table[i].descent < int16(tag_info(lower_descent_tag).value)*9/10)
	type |= 1;
      printf("\t%d\t%d", type,
	     char_table[i].symbol_set*256 + char_table[i].code);

      if (tfm_type == UNICODE) {
	if (charcode >= 0xE000 && charcode <= 0xF8FF)
	  printf("\t-- HP PUA U+%04X", charcode);
	else
	  printf("\t-- U+%04X", charcode);
      }
      else
	printf("\t-- MSL %4d", charcode);
      printf(" (%3s %3d)\n",
	     show_symset(char_table[i].symbol_set), char_table[i].code);

      if (charcode < charcode_name_table_size
	  && charcode_name_table[charcode])
	for (name_list *p = charcode_name_table[charcode]->next;
	     p; p = p->next)
	  printf("%s\t\"\n", p->name);
    }
    // warnings about characters in mapfile not found in TFM
    else if (charcode < charcode_name_table_size
	     && charcode_name_table[charcode]) {
      char *name = charcode_name_table[charcode]->name;
      // don't warn about Unicode or unnamed glyphs
      //  that aren't in the the TFM file
      if (tfm_type == UNICODE && !quiet_flag && !equal(name, UNNAMED)
	  && !is_uname(name)) {
	fprintf(stderr, "%s: warning: symbol U+%04X (%s",
		program_name, charcode, name);
	for (name_list *p = charcode_name_table[charcode]->next;
	     p; p = p->next)
	  fprintf(stderr, ", %s", p->name);
	fprintf(stderr, ") not in any searched symbol set\n");
      }
      else if (!quiet_flag && !equal(name, UNNAMED) && !is_uname(name)) {
	fprintf(stderr, "%s: warning: symbol MSL %d (%s",
		program_name, charcode, name);
	for (name_list *p = charcode_name_table[charcode]->next;
	     p; p = p->next)
	  fprintf(stderr, ", %s", p->name);
	fprintf(stderr, ") not in any searched symbol set\n");
      }
    }
  }
}

#define em_fract(a) (upem >= 0 ? double(a)/upem : 0)

static void
dump_tags(File &f)
{
  double upem = -1.0;

  printf("TFM tags\n"
	 "\n"
	 "tag# type count value\n"
	 "---------------------\n");

  for (int i = min_tag; i <= max_tag; i++) {
    enum tag_type t = tag_type(i);
    if (tag_info(t).present) {
      printf("%4d %4d %5d", i, tag_info(t).type, tag_info(t).count);
      switch (tag_info(t).type) {
      case BYTE_TYPE:
      case USHORT_TYPE:
	printf(" %5u", tag_info(t).value);
	switch (i) {
	case type_tag:
	  printf(" Font Type ");
	  switch (tag_info(t).value) {
	  case MSL:
	  case SYMSET:
	    printf("(Intellifont)");
	    break;
	  case UNICODE:
	    printf("(TrueType)");
	  }
	  break;
	case charcode_tag:
	  printf(" Number of Symbols (%u)", tag_info(t).count);
	  break;
	case symbol_set_tag:
	  printf(" Symbol Sets (%u): ",
		 tag_info(symbol_set_tag).count / 14);
	  dump_symbol_sets(f);
	  break;
	case type_structure_tag:
	  printf(" Type Structure (%u)", tag_info(t).value);
	  break;
	case stroke_weight_tag:
	  printf(" Stroke Weight (%u)", tag_info(t).value);
	  break;
	case spacing_tag:
	  printf(" Spacing ");
	  switch (tag_info(t).value) {
	  case 0:
	    printf("(Proportional)");
	    break;
	  case 1:
	    printf("(Fixed Pitch: %u DU: %.2f em)", tag_info(t).value,
		   em_fract(tag_info(t).value));
	    break;
	  }
	  break;
	case appearance_width_tag:
	  printf(" Appearance Width (%u)", tag_info(t).value);
	  break;
	case serif_style_tag:
	  printf(" Serif Style (%u)", tag_info(t).value);
	  break;
	case posture_tag:
	  printf(" Posture (%s)", tag_info(t).value == 0
				  ? "Upright"
				  : tag_info(t).value == 1
				    ? "Italic"
				    : "Alternate Italic");
	  break;
	case max_width_tag:
	  printf(" Maximum Width (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case word_spacing_tag:
	  printf(" Interword Spacing (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case recommended_line_spacing_tag:
	  printf(" Recommended Line Spacing (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case x_height_tag:
	  printf(" x-Height (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case cap_height_tag:
	  printf(" Cap Height (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case max_ascent_tag:
	  printf(" Maximum Ascent (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case lower_ascent_tag:
	  printf(" Lowercase Ascent (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case underscore_thickness_tag:
	  printf(" Underscore Thickness (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case uppercase_accent_height_tag:
	  printf(" Uppercase Accent Height (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case lowercase_accent_height_tag:
	  printf(" Lowercase Accent Height (%u DU: %.2f em)", tag_info(t).value,
		 em_fract(tag_info(t).value));
	  break;
	case width_tag:
	  printf(" Horizontal Escapement array");
	  break;
	case vertical_escapement_tag:
	  printf(" Vertical Escapement array");
	  break;
	case right_extent_tag:
	  printf(" Right Extent array");
	  break;
	case ascent_tag:
	  printf(" Character Ascent array");
	  break;
	case pair_kern_tag:
	  f.seek(tag_info(t).value);
	  printf(" Kern Pairs (%u)", f.get_uint16());
	  break;
	case panose_tag:
	  printf(" PANOSE Classification array");
	  break;
	}
	break;
      case SIGNED_SHORT_TYPE:
	printf(" %5d", int16(tag_info(t).value));
	switch (i) {
	case slant_tag:
	  printf(" Slant (%.2f degrees)", double(tag_info(t).value) / 100);
	  break;
	case max_descent_tag:
	  printf(" Maximum Descent (%d DU: %.2f em)", int16(tag_info(t).value),
		 em_fract(int16(tag_info(t).value)));
	  break;
	case lower_descent_tag:
	  printf(" Lowercase Descent (%d DU: %.2f em)", int16(tag_info(t).value),
		 em_fract(int16(tag_info(t).value)));
	  break;
	case underscore_depth_tag:
	  printf(" Underscore Depth (%d DU: %.2f em)", int16(tag_info(t).value),
		 em_fract(int16(tag_info(t).value)));
	  break;
	case left_extent_tag:
	  printf(" Left Extent array");
	  break;
	// The type of this tag has changed from SHORT to SIGNED SHORT
	// in TFM version 1.3.0.
	case ascent_tag:
	  printf(" Character Ascent array");
	  break;
	case descent_tag:
	  printf(" Character Descent array");
	  break;
	}
	break;
      case RATIONAL_TYPE:
	printf(" %5u", tag_info(t).value);
	switch (i) {
	case inches_per_point_tag:
	  printf(" Inches per Point");
	  break;
	case nominal_point_size_tag:
	  printf(" Nominal Point Size");
	  break;
	case design_units_per_em_tag:
	  printf(" Design Units per Em");
	  break;
	case average_width_tag:
	  printf(" Average Width");
	  break;
	}
	if (tag_info(t).count == 1) {
	  f.seek(tag_info(t).value);
	  uint32 num = f.get_uint32();
	  uint32 den = f.get_uint32();
	  if (i == design_units_per_em_tag)
	    upem = double(num) / den;
	  printf(" (%u/%u = %g)", num, den, double(num)/den);
	}
	break;
      case ASCII_TYPE:
	printf(" %5u ", tag_info(t).value);
	switch (i) {
	case comment_tag:
	  printf("Comment ");
	  break;
	case copyright_tag:
	  printf("Copyright ");
	  break;
	case unique_identifier_tag:
	  printf("Unique ID ");
	  break;
	case font_name_tag:
	  printf("Typeface Name ");
	  break;
	case typeface_source_tag:
	  printf("Typeface Source ");
	  break;
	case typeface_tag:
	  printf("PCL Typeface ");
	  break;
	}
	dump_ascii(f, t);
      }
      putchar('\n');
    }
  }
  putchar('\n');
}
#undef em_fract

static void
dump_ascii(File &f, tag_type t)
{
  putchar('"');
  if (tag_info(t).count > 4) {
    int count = tag_info(t).count;
    f.seek(tag_info(t).value);
    while (--count)
      printf("%c", f.get_byte());
  }
  else
    printf("%.4s", tag_info(t).orig_value);
  putchar('"');
}

static void
dump_symbol_sets(File &f)
{
  uint32 symbol_set_dir_length = tag_info(symbol_set_tag).count;
  uint32 num_symbol_sets = symbol_set_dir_length / 14;

  for (uint32 i = 0; i < num_symbol_sets; i++) {
    f.seek(tag_info(symbol_set_tag).value + i * 14);
    (void)f.get_uint32();		// offset to symbol set name
    uint32 off1 = f.get_uint32();	// offset to selection string
    uint32 off2 = f.get_uint32();	// offset to symbol set index array
    f.seek(off1);
    for (uint32 j = 0; j < off2 - off1; j++) {
      unsigned char c = f.get_byte();
      if ('0' <= c && c <= '9')
	putchar(c);
      else if ('A' <= c && c <= 'Z')
	printf(i < num_symbol_sets - 1 ? "%c," : "%c", c);
    }
  }
}

static void
dump_symbols(int tfm_type)
{
  printf("Symbols:\n"
	 "\n"
	 " glyph id#     symbol set  name(s)\n"
	 "----------------------------------\n");
  for (uint32 i = 0; i < nchars; i++) {
    uint16 charcode = char_table[i].charcode;
    if (charcode < charcode_name_table_size
	&& charcode_name_table[charcode]) {
      if (char_table[i].symbol_set != NO_SYMBOL_SET) {
	printf(tfm_type == UNICODE ? "%4d (U+%04X)   (%3s %3d)  %s"
				   : "%4d (MSL %4d) (%3s %3d)  %s",
	       i, charcode,
	       show_symset(char_table[i].symbol_set),
	       char_table[i].code,
	       charcode_name_table[charcode]->name);
	for (name_list *p = charcode_name_table[charcode]->next;
	      p; p = p->next)
	  printf(", %s", p->name);
	putchar('\n');
      }
    }
    else {
      printf(tfm_type == UNICODE ? "%4d (U+%04X)   "
				 : "%4d (MSL %4d) ",
	     i, charcode);
      if (char_table[i].symbol_set != NO_SYMBOL_SET)
	printf("(%3s %3d)",
	       show_symset(char_table[i].symbol_set), char_table[i].code);
      putchar('\n');
    }
  }
  putchar('\n');
}

static char *
show_symset(unsigned int symset)
{
   static char symset_str[8];

   sprintf(symset_str, "%d%c", symset / 32, (symset & 31) + 64);
   return symset_str;
}

static char *
hp_msl_to_ucode_name(int msl)
{
  char codestr[8];

  sprintf(codestr, "%d", msl);
  const char *ustr = hp_msl_to_unicode_code(codestr);
  if (ustr == NULL)
    ustr = UNNAMED;
  else {
    char *nonum;
    int ucode = int(strtol(ustr, &nonum, 16));
    // don't allow PUA code points as Unicode names
    if (ucode >= 0xE000 && ucode <= 0xF8FF)
      ustr = UNNAMED;
  }
  if (!equal(ustr, UNNAMED)) {
    const char *uname_decomposed = decompose_unicode(ustr);
    if (uname_decomposed)
      // 1st char is the number of components
      ustr = uname_decomposed + 1;
  }
  char *value = new char[strlen(ustr) + 1];
  sprintf(value, equal(ustr, UNNAMED) ? ustr : "u%s", ustr);
  return value;
}

static char *
unicode_to_ucode_name(int ucode)
{
  const char *ustr;
  char codestr[8];

  // don't allow PUA code points as Unicode names
  if (ucode >= 0xE000 && ucode <= 0xF8FF)
    ustr = UNNAMED;
  else {
    sprintf(codestr, "%04X", ucode);
    ustr = codestr;
  }
  if (!equal(ustr, UNNAMED)) {
    const char *uname_decomposed = decompose_unicode(ustr);
    if (uname_decomposed)
      // 1st char is the number of components
      ustr = uname_decomposed + 1;
  }
  char *value = new char[strlen(ustr) + 1];
  sprintf(value, equal(ustr, UNNAMED) ? ustr : "u%s", ustr);
  return value;
}

static int
is_uname(char *name)
{
  size_t i;
  size_t len = strlen(name);
  if (len % 5)
    return 0;

  if (name[0] != 'u')
    return 0;
  for (i = 1; i < 4; i++)
    if (!csxdigit(name[i]))
      return 0;
  for (i = 5; i < len; i++)
    if (i % 5 ? !csxdigit(name[i]) : name[i] != '_')
      return 0;

  return 1;
}

static int
read_map(const char *file, const int tfm_type)
{
  errno = 0;
  FILE *fp = fopen(file, "r");
  if (!fp) {
    error("can't open `%1': %2", file, strerror(errno));
    return 0;
  }
  current_filename = file;
  char buf[512];
  current_lineno = 0;
  char *nonum;
  while (fgets(buf, int(sizeof(buf)), fp)) {
    current_lineno++;
    char *ptr = buf;
    while (csspace(*ptr))
      ptr++;
    if (*ptr == '\0' || *ptr == '#')
      continue;
    ptr = strtok(ptr, " \n\t");
    if (!ptr)
      continue;

    int msl_code = int(strtol(ptr, &nonum, 10));
    if (*nonum != '\0') {
      if (csxdigit(*nonum))
	error("bad MSL map: got hex code (%1)", ptr);
      else if (ptr == nonum)
	error("bad MSL map: bad MSL code (%1)", ptr);
      else
	error("bad MSL map");
      fclose(fp);
      return 0;
    }

    ptr = strtok(NULL, " \n\t");
    if (!ptr)
      continue;
    int unicode = int(strtol(ptr, &nonum, 16));
    if (*nonum != '\0') {
      if (ptr == nonum)
	error("bad Unicode value (%1)", ptr);
      else
	error("bad Unicode map");
      fclose(fp);
      return 0;
    }
    if (strlen(ptr) != 4) {
      error("bad Unicode value (%1)", ptr);
      return 0;
    }

    int n = tfm_type == MSL ? msl_code : unicode;
    if (tfm_type == UNICODE && n > 0xFFFF) {
      // greatest value supported by TFM files
      error("bad Unicode value (%1): greatest value is 0xFFFF", ptr);
      fclose(fp);
      return 0;
    }
    else if (n < 0) {
      error("negative code value (%1)", ptr);
      fclose(fp);
      return 0;
    }

    ptr = strtok(NULL, " \n\t");
    if (!ptr) {					// groff name
      error("missing name(s)");
      fclose(fp);
      return 0;
    }
    // leave decomposed Unicode values alone
    else if (is_uname(ptr) && !is_decomposed(ptr))
      ptr = unicode_to_ucode_name(strtol(ptr + 1, &nonum, 16));

    if (size_t(n) >= charcode_name_table_size) {
      size_t old_size = charcode_name_table_size;
      name_list **old_table = charcode_name_table;
      charcode_name_table_size = n + 256;
      charcode_name_table = new name_list *[charcode_name_table_size];
      if (old_table) {
	memcpy(charcode_name_table, old_table, old_size*sizeof(name_list *));
	a_delete old_table;
      }
      for (size_t i = old_size; i < charcode_name_table_size; i++)
	charcode_name_table[i] = NULL;
    }

    // a '#' that isn't the first groff name begins a comment
    for (int names = 1; ptr; ptr = strtok(NULL, " \n\t")) {
      if (names++ > 1 && *ptr == '#')
	break;
      charcode_name_table[n] = new name_list(ptr, charcode_name_table[n]);
    }
  }
  fclose(fp);
  return 1;
}

static const char *
xbasename(const char *s)
{
  // DIR_SEPS[] are possible directory separator characters, see
  // nonposix.h.  We want the rightmost separator of all possible
  // ones.  Example: d:/foo\\bar.
  const char *b = strrchr(s, DIR_SEPS[0]), *b1;
  const char *sep = &DIR_SEPS[1];

  while (*sep)
    {
      b1 = strrchr(s, *sep);
      if (b1 && (!b || b1 > b))
	b = b1;
      sep++;
    }
  return b ? b + 1 : s;
}
