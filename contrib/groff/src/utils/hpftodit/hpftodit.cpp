// -*- C++ -*-
/* Copyright (C) 1994, 2000, 2001, 2003 Free Software Foundation, Inc.
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

/*
TODO
put human readable font name in device file
devise new names for useful characters
use --- for unnamed characters
option to specify symbol sets to look in
make it work with TrueType fonts
put filename in error messages (or fix lib)
*/

#include "lib.h"

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include "assert.h"
#include "posix.h"
#include "errarg.h"
#include "error.h"
#include "cset.h"
#include "nonposix.h"

extern "C" const char *Version_string;

#define SIZEOF(v) (sizeof(v)/sizeof(v[0]))

const int MULTIPLIER = 3;

inline
int scale(int n)
{
  return n * MULTIPLIER;
}

// tags in TFM file

enum tag_type {
  min_tag = 400,
  type_tag = 400,
  symbol_set_tag = 404,
  msl_tag = 403,
  inches_per_point_tag = 406,
  design_units_per_em_tag = 408,
  posture_tag = 409,
  stroke_weight_tag = 411,
  spacing_tag = 412,
  slant_tag = 413,
  appearance_width_tag = 414,
  word_spacing_tag = 421,
  x_height_tag = 424,
  lower_ascent_tag = 427,
  lower_descent_tag = 428,
  width_tag = 433,
  left_extent_tag = 435,
  right_extent_tag = 436,
  ascent_tag = 437,
  descent_tag = 438,
  pair_kern_tag = 439,
  typeface_tag = 442,
  max_tag = 443
  };

// types in TFM file

enum {
  ENUM_TYPE = 1,
  BYTE_TYPE = 2,
  USHORT_TYPE = 3,
  FLOAT_TYPE = 5,
  SIGNED_SHORT_TYPE = 17
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
  entry() : present(0) { }
};

struct char_info {
  uint16 msl;
  uint16 width;
  int16 ascent;
  int16 descent;
  int16 left_extent;
  uint16 right_extent;
  uint16 symbol_set;
  unsigned char code;
};

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
  SYMBOL_SET(0, 'N'),		// Latin 1
  SYMBOL_SET(6, 'J'),		// Microsoft Publishing
  SYMBOL_SET(2, 'N'),		// Latin 2
  0
  };

uint16 special_symbol_sets[] = {
  SYMBOL_SET(8, 'M'),
  SYMBOL_SET(5, 'M'),
  SYMBOL_SET(15, 'U'),
  0
  };

entry tags[max_tag + 1 - min_tag];

char_info *char_table;
uint32 nchars;

unsigned int msl_name_table_size = 0;
name_list **msl_name_table = 0;

unsigned int n_symbol_sets;
symbol_set *symbol_set_table;

static int special_flag = 0;
static int italic_flag = 0;
static int italic_sep;

static void usage(FILE *stream);
static void usage();
static const char *xbasename(const char *);
static void read_tags(File &);
static void check_type();
static void check_units(File &);
static int read_map(const char *);
static void require_tag(tag_type);
static void dump_tags(File &f);
static void output_spacewidth();
static void output_pclweight();
static void output_pclproportional();
static void read_and_output_pcltypeface(File &);
static void output_pclstyle();
static void output_slant();
static void output_ligatures();
static void read_symbol_sets(File &);
static void read_and_output_kernpairs(File &);
static void output_charset();
static void read_char_table(File &f);

inline
entry &tag_info(tag_type t)
{
  return tags[t - min_tag];
}

int main(int argc, char **argv)
{
  program_name = argv[0];

  int opt;
  int debug_flag = 0;

  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((opt = getopt_long(argc, argv, "dsvi:", long_options, NULL)) != EOF) {
    switch (opt) {
    case 'd':
      debug_flag = 1;
      break;
    case 's':
      special_flag = 1;
      break;
    case 'i':
      italic_flag = 1;
      italic_sep = atoi(optarg);
      break;
    case 'v':
      {
	printf("GNU hpftodit (groff) version %s\n", Version_string);
	exit(0);
      }
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
  if (argc - optind != 3)
    usage();
  File f(argv[optind]);
  if (!read_map(argv[optind + 1]))
    exit(1);
  current_filename = 0;
  current_lineno = -1;		// no line numbers
  if (freopen(argv[optind + 2], "w", stdout) == 0)
    fatal("cannot open `%1': %2", argv[optind + 2], strerror(errno));
  current_filename = argv[optind];
  printf("name %s\n", xbasename(argv[optind + 2]));
  if (special_flag)
    printf("special\n");
  read_tags(f);
  check_type();
  check_units(f);
  if (debug_flag)
    dump_tags(f);
  read_char_table(f);
  output_spacewidth();
  output_slant();
  read_and_output_pcltypeface(f);
  output_pclproportional();
  output_pclweight();
  output_pclstyle();
  read_symbol_sets(f);
  output_ligatures();
  read_and_output_kernpairs(f);
  output_charset();
  return 0;
}

static
void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [-s] [-i n] tfm_file map_file output_font\n",
	  program_name);
}
static
void usage()
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

void File::skip(int n)
{
  if (end_ - ptr_ < n)
    fatal("unexpected end of file");
  ptr_ += n;
}

void File::seek(uint32 n)
{
  if ((uint32)(end_ - buf_) < n)
    fatal("unexpected end of file");
  ptr_ = buf_ + n;
}

byte File::get_byte()
{
  if (ptr_ >= end_)
    fatal("unexpected end of file");
  return *ptr_++;
}

uint16 File::get_uint16()
{
  if (end_ - ptr_ < 2)
    fatal("unexpected end of file");
  uint16 n = *ptr_++;
  return n + (*ptr_++ << 8);
}

uint32 File::get_uint32()
{
  if (end_ - ptr_ < 4)
    fatal("unexpected end of file");
  uint32 n = *ptr_++;
  for (int i = 0; i < 3; i++)
    n += *ptr_++ << (i + 1)*8;
  return n;
}

static
void read_tags(File &f)
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
    p->value = f.get_uint32();
  }
}

static
void check_type()
{
  require_tag(type_tag);
  if (tag_info(type_tag).value != 0) {
    if (tag_info(type_tag).value == 2)
      fatal("cannot handle TrueType tfm files");
    fatal("unknown type tag %1", int(tag_info(type_tag).value));
  }
}

static
void check_units(File &f)
{
  require_tag(design_units_per_em_tag);
  f.seek(tag_info(design_units_per_em_tag).value);
  uint32 num = f.get_uint32();
  uint32 den = f.get_uint32();
  if (num != 8782 || den != 1)
    fatal("design units per em != 8782/1");
  require_tag(inches_per_point_tag);
  f.seek(tag_info(inches_per_point_tag).value);
  num = f.get_uint32();
  den = f.get_uint32();
  if (num != 100 || den != 7231)
    fatal("inches per point not 100/7231");
}

static
void require_tag(tag_type t)
{
  if (!tag_info(t).present)
    fatal("tag %1 missing", int(t));
}

static
void output_spacewidth()
{
  require_tag(word_spacing_tag);
  printf("spacewidth %d\n", scale(tag_info(word_spacing_tag).value));
}

static
void read_symbol_sets(File &f)
{
  uint32 symbol_set_dir_length = tag_info(symbol_set_tag).count;
  n_symbol_sets = symbol_set_dir_length/14;
  symbol_set_table = new symbol_set[n_symbol_sets];
  unsigned int i;
  for (i = 0; i < n_symbol_sets; i++) {
    f.seek(tag_info(symbol_set_tag).value + i*14);
    (void)f.get_uint32();
    uint32 off1 = f.get_uint32();
    uint32 off2 = f.get_uint32();
    (void)f.get_uint16();		// what's this for?
    f.seek(off1);
    unsigned int j;
    uint16 kind = 0;
    for (j = 0; j < off2 - off1; j++) {
      unsigned char c = f.get_byte();
      if ('0' <= c && c <= '9')
	kind = kind*10 + (c - '0');
      else if ('A' <= c && c <= 'Z')
	kind = kind*32 + (c - 64);
    }
    symbol_set_table[i].select = kind;
    for (j = 0; j < 256; j++)
      symbol_set_table[i].index[j] = f.get_uint16();
  }
  for (i = 0; i < nchars; i++)
    char_table[i].symbol_set = NO_SYMBOL_SET;

  uint16 *symbol_set_selectors = (special_flag
				  ? special_symbol_sets
				  : text_symbol_sets);
  for (i = 0; symbol_set_selectors[i] != 0; i++) {
    unsigned int j;
    for (j = 0; j < n_symbol_sets; j++)
      if (symbol_set_table[j].select == symbol_set_selectors[i])
	break;
    if (j < n_symbol_sets) {
      for (int k = 0; k < 256; k++) {
	uint16 index = symbol_set_table[j].index[k];
	if (index != 0xffff
	    && char_table[index].symbol_set == NO_SYMBOL_SET) {
	  char_table[index].symbol_set = symbol_set_table[j].select;
	  char_table[index].code = k;
	}
      }
    }
  }
}

static
void read_char_table(File &f)
{
  require_tag(msl_tag);
  nchars = tag_info(msl_tag).count;
  char_table = new char_info[nchars];

  f.seek(tag_info(msl_tag).value);
  uint32 i;
  for (i = 0; i < nchars; i++)
    char_table[i].msl = f.get_uint16();
  
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

static
void output_pclweight()
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

static
void output_pclproportional()
{
  require_tag(spacing_tag);
  printf("pclproportional %d\n", tag_info(spacing_tag).value == 0);
}

static
void read_and_output_pcltypeface(File &f)
{
  printf("pcltypeface ");
  require_tag(typeface_tag);
  f.seek(tag_info(typeface_tag).value);
  for (uint32 i = 0; i < tag_info(typeface_tag).count; i++) {
    unsigned char c = f.get_byte();
    if (c == '\0')
      break;
    putchar(c);
  }
  printf("\n");
}

static
void output_pclstyle()
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

static
void output_slant()
{
  require_tag(slant_tag);
  int slant = int16(tag_info(slant_tag).value);
  if (slant != 0)
    printf("slant %f\n", slant/100.0);
}

static
void output_ligatures()
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
    uint16 msl = char_table[i].msl;
    if (msl < msl_name_table_size
	&& char_table[i].symbol_set != NO_SYMBOL_SET) {
      for (name_list *p = msl_name_table[msl]; p; p = p->next)
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

static
void read_and_output_kernpairs(File &f)
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
	  && char_table[i1].msl < msl_name_table_size
	  && char_table[i2].msl < msl_name_table_size) {
	for (name_list *p = msl_name_table[char_table[i1].msl];
	     p;
	     p = p->next)
	  for (name_list *q = msl_name_table[char_table[i2].msl];
	       q;
	       q = q->next)
	    printf("%s %s %d\n", p->name, q->name, scale(val));
      }
    }
  }
}

static 
void output_charset()
{
  require_tag(slant_tag);
  double slant_angle = int16(tag_info(slant_tag).value)*PI/18000.0;
  double slant = sin(slant_angle)/cos(slant_angle);

  require_tag(x_height_tag);
  require_tag(lower_ascent_tag);
  require_tag(lower_descent_tag);

  printf("charset\n");
  unsigned int i;
  for (i = 0; i < nchars; i++) {
    uint16 msl = char_table[i].msl;
    if (msl < msl_name_table_size
	&& msl_name_table[msl]) {
      if (char_table[i].symbol_set != NO_SYMBOL_SET) {
	printf("%s\t%d,%d",
	       msl_name_table[msl]->name,
	       scale(char_table[i].width),
	       scale(char_table[i].ascent));
	int depth = scale(- char_table[i].descent);
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
	if (char_table[i].ascent > (int16(tag_info(lower_ascent_tag).value)*9)/10)
	  type |= 2;
	if (char_table[i].descent < (int16(tag_info(lower_descent_tag).value)*9)/10)
	  type |= 1;
	printf("\t%d\t%d\n",
	       type,
	       char_table[i].symbol_set*256 + char_table[i].code);
	for (name_list *p = msl_name_table[msl]->next; p; p = p->next)
	  printf("%s\t\"\n", p->name);
      }
      else
	warning("MSL %1 not in any of the searched symbol sets", msl);
    }
  }
}

static
void dump_tags(File &f)
{
  int i;
  for (i = min_tag; i <= max_tag; i++) {
    enum tag_type t = tag_type(i);
    if (tag_info(t).present) {
      fprintf(stderr,
	      "%d %d %d %d\n", i, tag_info(t).type, tag_info(t).count,
	      tag_info(t).value);
      if (tag_info(t).type == FLOAT_TYPE
	  && tag_info(t).count == 1) {
	f.seek(tag_info(t).value);
	uint32 num = f.get_uint32();
	uint32 den = f.get_uint32();
	fprintf(stderr, "(%u/%u = %g)\n", num, den, (double)num/den);
      }
    }
  }
}

static
int read_map(const char *file)
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
    int n;
    if (sscanf(ptr, "%d", &n) != 1) {
      error("bad map file");
      fclose(fp);
      return 0;
    }
    if (n < 0) {
      error("negative code");
      fclose(fp);
      return 0;
    }
    if ((size_t)n >= msl_name_table_size) {
      size_t old_size = msl_name_table_size;
      name_list **old_table = msl_name_table;
      msl_name_table_size = n + 256;
      msl_name_table = new name_list *[msl_name_table_size];
      if (old_table) {
	memcpy(msl_name_table, old_table, old_size*sizeof(name_list *));
	a_delete old_table;
      }
      for (size_t i = old_size; i < msl_name_table_size; i++)
	msl_name_table[i] = 0;
    }
    ptr = strtok(0, " \n\t");
    if (!ptr) {
      error("missing names");
      fclose(fp);
      return 0;
    }
    for (; ptr; ptr = strtok(0, " \n\t"))
      msl_name_table[n] = new name_list(ptr, msl_name_table[n]);
  }
  fclose(fp);
  return 1;
}

static
const char *xbasename(const char *s)
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
