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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* I have tried to incorporate the changes needed for TeX 3.0 tfm files,
but I haven't tested them. */

/* Groff requires more font metric information than TeX.  The reason
for this is that TeX has separate Math Italic fonts, whereas groff
uses normal italic fonts for math.  The two additional pieces of
information required by groff correspond to the two arguments to the
math_fit() macro in the Metafont programs for the CM fonts. In the
case of a font for which math_fitting is false, these two arguments
are normally ignored by Metafont. We need to get hold of these two
parameters and put them in the groff font file.

We do this by loading this definition after cmbase when creating cm.base.

def ignore_math_fit(expr left_adjustment,right_adjustment) =
 special "adjustment";
 numspecial left_adjustment*16/designsize;
 numspecial right_adjustment*16/designsize;
 enddef;

This puts the two arguments to the math_fit macro into the gf file.
(They will appear in the gf file immediately before the character to
which they apply.)  We then create a gf file using this cm.base.  Then
we run tfmtodit and specify this gf file with the -g option.

This need only be done for a font for which math_fitting is false;
When it's true, the left_correction and subscript_correction should
both be zero. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include "lib.h"
#include "errarg.h"
#include "error.h"
#include "assert.h"
#include "cset.h"
#include "nonposix.h"

/* Values in the tfm file should be multiplied by this. */

#define MULTIPLIER 1

struct char_info_word {
  unsigned char width_index;
  char height_index;
  char depth_index;
  char italic_index;
  char tag;
  unsigned char remainder;
};

struct lig_kern_command {
  unsigned char skip_byte;
  unsigned char next_char;
  unsigned char op_byte;
  unsigned char remainder;
};

class tfm {
  int bc;
  int ec;
  int nw;
  int nh;
  int nd;
  int ni;
  int nl;
  int nk;
  int np;
  int cs;
  int ds;
  char_info_word *char_info;
  int *width;
  int *height;
  int *depth;
  int *italic;
  lig_kern_command *lig_kern;
  int *kern;
  int *param;
public:
  tfm();
  ~tfm();
  int load(const char *);
  int contains(int);
  int get_width(int);
  int get_height(int);
  int get_depth(int);
  int get_italic(int);
  int get_param(int, int *);
  int get_checksum();
  int get_design_size();
  int get_lig(unsigned char, unsigned char, unsigned char *);
  friend class kern_iterator;
};

class kern_iterator {
  tfm *t;
  int c;
  int i;
public:
  kern_iterator(tfm *);
  int next(unsigned char *c1, unsigned char *c2, int *k);
};


kern_iterator::kern_iterator(tfm *p)
: t(p), c(t->bc), i(-1)
{
}

int kern_iterator::next(unsigned char *c1, unsigned char *c2, int *k)
{
  for (; c <= t->ec; c++)
    if (t->char_info[c - t->bc].tag == 1) {
      if (i < 0) {
	i = t->char_info[c - t->bc].remainder;
	if (t->lig_kern[i].skip_byte > 128)
	  i = (256*t->lig_kern[i].op_byte
		   + t->lig_kern[i].remainder);
      }
      for (;;) {
	int skip = t->lig_kern[i].skip_byte;
	if (skip <= 128 && t->lig_kern[i].op_byte >= 128) {
	  *c1 = c;
	  *c2 = t->lig_kern[i].next_char;
	  *k = t->kern[256*(t->lig_kern[i].op_byte - 128)
		       + t->lig_kern[i].remainder];
	  if (skip == 128) {
	    c++;
	    i = -1;
	  }
	  else
	    i += skip + 1;
	  return 1;
	}
	if (skip >= 128)
	  break;
	i += skip + 1;
      }
      i = -1;
    }
  return 0;
}
	  
tfm::tfm()
: char_info(0), width(0), height(0), depth(0), italic(0), lig_kern(0),
  kern(0), param(0)
{
}

int tfm::get_lig(unsigned char c1, unsigned char c2, unsigned char *cp)
{
  if (contains(c1) && char_info[c1 - bc].tag == 1) {
    int i = char_info[c1 - bc].remainder;
    if (lig_kern[i].skip_byte > 128)
      i = 256*lig_kern[i].op_byte + lig_kern[i].remainder;
    for (;;) {
      int skip = lig_kern[i].skip_byte;
      if (skip > 128)
	break;
      // We are only interested in normal ligatures, for which
      // op_byte == 0.
      if (lig_kern[i].op_byte == 0
	  && lig_kern[i].next_char == c2) {
	*cp = lig_kern[i].remainder;
	return 1;
      }
      if (skip == 128)
	break;
      i += skip + 1;
    }
  }
  return 0;
}

int tfm::contains(int i)
{
  return i >= bc && i <= ec && char_info[i - bc].width_index != 0;
}

int tfm::get_width(int i)
{
  return width[char_info[i - bc].width_index];
}

int tfm::get_height(int i)
{
  return height[char_info[i - bc].height_index];
}

int tfm::get_depth(int i)
{
  return depth[char_info[i - bc].depth_index];
}

int tfm::get_italic(int i)
{
  return italic[char_info[i - bc].italic_index];
}

int tfm::get_param(int i, int *p)
{
  if (i <= 0 || i > np)
    return 0;
  else {
    *p = param[i - 1];
    return 1;
  }
}

int tfm::get_checksum()
{
  return cs;
}

int tfm::get_design_size()
{
  return ds;
}

tfm::~tfm()
{
  a_delete char_info;
  a_delete width;
  a_delete height;
  a_delete depth;
  a_delete italic;
  a_delete lig_kern;
  a_delete kern;
  a_delete param;
}
  
int read2(unsigned char *&s)
{
  int n;
  n = *s++ << 8;
  n |= *s++;
  return n;
}

int read4(unsigned char *&s)
{
  int n;
  n = *s++ << 24;
  n |= *s++ << 16;
  n |= *s++ << 8;
  n |= *s++;
  return n;
}


int tfm::load(const char *file)
{
  errno = 0;
  FILE *fp = fopen(file, FOPEN_RB);
  if (!fp) {
    error("can't open `%1': %2", file, strerror(errno));
    return 0;
  }
  int c1 = getc(fp);
  int c2 = getc(fp);
  if (c1 == EOF || c2 == EOF) {
    fclose(fp);
    error("unexpected end of file on `%1'", file);
    return 0;
  }
  int lf = (c1 << 8) + c2;
  int toread = lf*4 - 2;
  unsigned char *buf = new unsigned char[toread];
  if (fread(buf, 1, toread, fp) != toread) {
    if (feof(fp))
      error("unexpected end of file on `%1'", file);
    else
      error("error on file `%1'", file);
    a_delete buf;
    fclose(fp);
    return 0;
  }
  fclose(fp);
  if (lf < 6) {
    error("bad tfm file `%1': impossibly short", file);
    a_delete buf;
    return 0;
  }
  unsigned char *ptr = buf;
  int lh = read2(ptr);
  bc = read2(ptr);
  ec = read2(ptr);
  nw = read2(ptr);
  nh = read2(ptr);
  nd = read2(ptr);
  ni = read2(ptr);
  nl = read2(ptr);
  nk = read2(ptr);
  int ne = read2(ptr);
  np = read2(ptr);
  if (6 + lh + (ec - bc + 1) + nw + nh + nd + ni + nl + nk + ne + np != lf) {
    error("bad tfm file `%1': lengths do not sum", file);
    a_delete buf;
    return 0;
  }
  if (lh < 2) {
    error("bad tfm file `%1': header too short", file);
    a_delete buf;
    return 0;
  }
  char_info = new char_info_word[ec - bc + 1];
  width = new int[nw];
  height = new int[nh];
  depth = new int[nd];
  italic = new int[ni];
  lig_kern = new lig_kern_command[nl];
  kern = new int[nk];
  param = new int[np];
  int i;
  cs = read4(ptr);
  ds = read4(ptr);
  ptr += (lh-2)*4;
  for (i = 0; i < ec - bc + 1; i++) {
    char_info[i].width_index = *ptr++;
    unsigned char tem = *ptr++;
    char_info[i].depth_index = tem & 0xf;
    char_info[i].height_index = tem >> 4;
    tem = *ptr++;
    char_info[i].italic_index = tem >> 2;
    char_info[i].tag = tem & 3;
    char_info[i].remainder = *ptr++;
  }
  for (i = 0; i < nw; i++)
    width[i] = read4(ptr);
  for (i = 0; i < nh; i++)
    height[i] = read4(ptr);
  for (i = 0; i < nd; i++)
    depth[i] = read4(ptr);
  for (i = 0; i < ni; i++)
    italic[i] = read4(ptr);
  for (i = 0; i < nl; i++) {
    lig_kern[i].skip_byte = *ptr++;
    lig_kern[i].next_char = *ptr++;
    lig_kern[i].op_byte = *ptr++;
    lig_kern[i].remainder = *ptr++;
  }
  for (i = 0; i < nk; i++)
    kern[i] = read4(ptr);
  ptr += ne*4;
  for (i = 0; i < np; i++)
    param[i] = read4(ptr);
  assert(ptr == buf + lf*4 - 2);
  a_delete buf;
  return 1;
}

class gf {
  int left[256];
  int right[256];
  static int sread4(int *p, FILE *fp);
  static int uread3(int *p, FILE *fp);
  static int uread2(int *p, FILE *fp);
  static int skip(int n, FILE *fp);
public:
  gf();
  int load(const char *file);
  int get_left_adjustment(int i) { return left[i]; }
  int get_right_adjustment(int i) { return right[i]; }
};

gf::gf()
{
  for (int i = 0; i < 256; i++)
    left[i] = right[i] = 0;
}

int gf::load(const char *file)
{
  enum {
    paint_0 = 0,
    paint1 = 64,
    boc = 67,
    boc1 = 68,
    eoc = 69,
    skip0 = 70,
    skip1 = 71,
    new_row_0 = 74,
    xxx1 = 239,
    yyy = 243,
    no_op = 244,
    pre = 247,
    post = 248
  };
  int got_an_adjustment = 0;
  int pending_adjustment = 0;
  int left_adj, right_adj;
  const int gf_id_byte = 131;
  errno = 0;
  FILE *fp = fopen(file, FOPEN_RB);
  if (!fp) {
    error("can't open `%1': %2", file, strerror(errno));
    return 0;
  }
  if (getc(fp) != pre || getc(fp) != gf_id_byte) {
    error("bad gf file");
    return 0;
  }
  int n = getc(fp);
  if (n == EOF)
    goto eof;
  if (!skip(n, fp))
    goto eof;
  for (;;) {
    int op = getc(fp);
    if (op == EOF)
      goto eof;
    if (op == post)
      break;
    if ((op >= paint_0 && op <= paint_0 + 63)
	|| (op >= new_row_0 && op <= new_row_0 + 164))
      continue;
    switch (op) {
    case no_op:
    case eoc:
    case skip0:
      break;
    case paint1:
    case skip1:
      if (!skip(1, fp))
	goto eof;
      break;
    case paint1 + 1:
    case skip1 + 1:
      if (!skip(2, fp))
	goto eof;
      break;
    case paint1 + 2:
    case skip1 + 2:
      if (!skip(3, fp))
	goto eof;
      break;
    case boc:
      {
	int code;
	if (!sread4(&code, fp))
	  goto eof;
	if (pending_adjustment) {
	  pending_adjustment = 0;
	  left[code & 0377] = left_adj;
	  right[code & 0377] = right_adj;
	}
	if (!skip(20, fp))
	  goto eof;
	break;
      }
    case boc1:
      {
	int code = getc(fp);
	if (code == EOF)
	  goto eof;
	if (pending_adjustment) {
	  pending_adjustment = 0;
	  left[code] = left_adj;
	  right[code] = right_adj;
	}
	if (!skip(4, fp))
	  goto eof;
	break;
      }
    case xxx1:
      {
	int len = getc(fp);
	if (len == EOF)
	  goto eof;
	char buf[256];
	if (fread(buf, 1, len, fp) != len)
	  goto eof;
	if (len == 10 /* strlen("adjustment") */
	    && memcmp(buf, "adjustment", len) == 0) {
	  int c = getc(fp);
	  if (c != yyy) {
	    if (c != EOF)
	      ungetc(c, fp);
	    break;
	  }
	  if (!sread4(&left_adj, fp))
	    goto eof;
	  c = getc(fp);
	  if (c != yyy) {
	    if (c != EOF)
	      ungetc(c, fp);
	    break;
	  }
	  if (!sread4(&right_adj, fp))
	    goto eof;
	  got_an_adjustment = 1;
	  pending_adjustment = 1;
	}
	break;
      }
    case xxx1 + 1:
      if (!uread2(&n, fp) || !skip(n, fp))
	goto eof;
      break;
    case xxx1 + 2:
      if (!uread3(&n, fp) || !skip(n, fp))
	goto eof;
      break;
    case xxx1 + 3:
      if (!sread4(&n, fp) || !skip(n, fp))
	goto eof;
      break;
    case yyy:
      if (!skip(4, fp))
	goto eof;
      break;
    default:
      fatal("unrecognized opcode `%1'", op);
      break;
    }
  }
  if (!got_an_adjustment)
    warning("no adjustment specials found in gf file");
  return 1;
 eof:
  error("unexpected end of file");
  return 0;
}

int gf::sread4(int *p, FILE *fp)
{
  *p = getc(fp);
  if (*p >= 128)
    *p -= 256;
  *p <<= 8;
  *p |= getc(fp);
  *p <<= 8;
  *p |= getc(fp);
  *p <<= 8;
  *p |= getc(fp);
  return !ferror(fp) && !feof(fp);
}

int gf::uread3(int *p, FILE *fp)
{
  *p = getc(fp);
  *p <<= 8;
  *p |= getc(fp);
  *p <<= 8;
  *p |= getc(fp);
  return !ferror(fp) && !feof(fp);
}

int gf::uread2(int *p, FILE *fp)
{
  *p = getc(fp);
  *p <<= 8;
  *p |= getc(fp);
  return !ferror(fp) && !feof(fp);
}

int gf::skip(int n, FILE *fp)
{
  while (--n >= 0)
    if (getc(fp) == EOF)
      return 0;
  return 1;
}


struct char_list {
  char *ch;
  char_list *next;
  char_list(const char *, char_list * = 0);
};

char_list::char_list(const char *s, char_list *p) : ch(strsave(s)), next(p)
{
}


int read_map(const char *file, char_list **table)
{
  errno = 0;
  FILE *fp = fopen(file, "r");
  if (!fp) {
    error("can't open `%1': %2", file, strerror(errno));
    return 0;
  }
  for (int i = 0; i < 256; i++)
    table[i] = 0;
  char buf[512];
  int lineno = 0;
  while (fgets(buf, int(sizeof(buf)), fp)) {
    lineno++;
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
      error("%1:%2: bad map file", file, lineno);
      fclose(fp);
      return 0;
    }
    if (n < 0 || n > 255) {
      error("%1:%2: code out of range", file, lineno);
      fclose(fp);
      return 0;
    }
    ptr = strtok(0, " \n\t");
    if (!ptr) {
      error("%1:%2: missing names", file, lineno);
      fclose(fp);
      return 0;
    }
    for (; ptr; ptr = strtok(0, " \n\t"))
      table[n] = new char_list(ptr, table[n]);
  }
  fclose(fp);
  return 1;
}


/* Every character that can participate in a ligature appears in the
lig_chars table. `ch' gives the full-name of the character, `name'
gives the groff name of the character, `i' gives its index in
the encoding, which is filled in later  (-1 if it does not appear). */

struct {
  const char *ch;
  int i;
} lig_chars[] = {
  { "f", -1 },
  { "i", -1 },
  { "l", -1 },
  { "ff", -1 },
  { "fi", -1 },
  { "fl", -1 },
  { "Fi", -1 },
  { "Fl", -1 },
};

// Indices into lig_chars[].

enum { CH_f, CH_i, CH_l, CH_ff, CH_fi, CH_fl, CH_ffi, CH_ffl };

// Each possible ligature appears in this table.

struct {
  unsigned char c1, c2, res;
  const char *ch;
} lig_table[] = {
  { CH_f, CH_f, CH_ff, "ff" },
  { CH_f, CH_i, CH_fi, "fi" },
  { CH_f, CH_l, CH_fl, "fl" },
  { CH_ff, CH_i, CH_ffi, "ffi" },
  { CH_ff, CH_l, CH_ffl, "ffl" },
  };

static void usage(FILE *stream);
  
int main(int argc, char **argv)
{
  program_name = argv[0];
  int special_flag = 0;
  int skewchar = -1;
  int opt;
  const char *gf_file = 0;
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((opt = getopt_long(argc, argv, "svg:k:", long_options, NULL)) != EOF)
    switch (opt) {
    case 'g':
      gf_file = optarg;
      break;
    case 's':
      special_flag = 1;
      break;
    case 'k':
      {
	char *ptr;
	long n = strtol(optarg, &ptr, 0);
	if ((n == 0 && ptr == optarg)
	    || *ptr != '\0'
	    || n < 0
	    || n > UCHAR_MAX)
	  error("invalid skewchar");
	else
	  skewchar = (int)n;
	break;
      }
    case 'v':
      {
	extern const char *Version_string;
	printf("GNU tfmtodit (groff) version %s\n", Version_string);
	exit(0);
	break;
      }
    case CHAR_MAX + 1: // --help
      usage(stdout);
      exit(0);
      break;
    case '?':
      usage(stderr);
      exit(1);
      break;
    case EOF:
      assert(0);
    }
  if (argc - optind != 3) {
    usage(stderr);
    exit(1);
  }
  gf g;
  if (gf_file) {
    if (!g.load(gf_file))
      return 1;
  }
  const char *tfm_file = argv[optind];
  const char *map_file = argv[optind + 1];
  const char *font_file = argv[optind + 2];
  tfm t;
  if (!t.load(tfm_file))
    return 1;
  char_list *table[256];
  if (!read_map(map_file, table))
    return 1;
  errno = 0;
  if (!freopen(font_file, "w", stdout)) {
    error("can't open `%1' for writing: %2", font_file, strerror(errno));
    return 1;
  }
  printf("name %s\n", font_file);
  if (special_flag)
    fputs("special\n", stdout);
  char *internal_name = strsave(argv[optind]);
  int len = strlen(internal_name);
  if (len > 4 && strcmp(internal_name + len - 4, ".tfm") == 0)
    internal_name[len - 4] = '\0';
  // DIR_SEPS[] are possible directory separator characters, see nonposix.h.
  // We want the rightmost separator of all possible ones.
  // Example: d:/foo\\bar.
  const char *s = strrchr(internal_name, DIR_SEPS[0]), *s1;
  const char *sep = &DIR_SEPS[1];
  while (*sep)
    {
      s1 = strrchr(internal_name, *sep);
      if (s1 && (!s || s1 > s))
	s = s1;
      sep++;
    }
  printf("internalname %s\n", s ? s + 1 : internal_name);
  int n;
  if (t.get_param(2, &n)) {
    if (n > 0)
      printf("spacewidth %d\n", n*MULTIPLIER);
  }
  if (t.get_param(1, &n) && n != 0)
    printf("slant %f\n", atan2(n/double(1<<20), 1.0)*180.0/PI);
  int xheight;
  if (!t.get_param(5, &xheight))
    xheight = 0;
  int i;
  // Print the list of ligatures.
  // First find the indices of each character that can participate in
  // a ligature.
  for (i = 0; i < 256; i++)
    for (int j = 0; j < sizeof(lig_chars)/sizeof(lig_chars[0]); j++)
      for (char_list *p = table[i]; p; p = p->next)
	if (strcmp(lig_chars[j].ch, p->ch) == 0)
	  lig_chars[j].i = i;
  // For each possible ligature, if its participants all exist,
  // and it appears as a ligature in the tfm file, include in
  // the list of ligatures.
  int started = 0;
  for (i = 0; i < sizeof(lig_table)/sizeof(lig_table[0]); i++) {
    int i1 = lig_chars[lig_table[i].c1].i;
    int i2 = lig_chars[lig_table[i].c2].i;
    int r = lig_chars[lig_table[i].res].i;
    if (i1 >= 0 && i2 >= 0 && r >= 0) {
      unsigned char c;
      if (t.get_lig(i1, i2, &c) && c == r) {
	if (!started) {
	  started = 1;
	  fputs("ligatures", stdout);
	}
	printf(" %s", lig_table[i].ch);
      }
    }
  }
  if (started)
    fputs(" 0\n", stdout);
  printf("checksum %d\n", t.get_checksum());
  printf("designsize %d\n", t.get_design_size());
  // Now print out the kerning information.
  int had_kern = 0;
  kern_iterator iter(&t);
  unsigned char c1, c2;
  int k;
  while (iter.next(&c1, &c2, &k))
    if (c2 != skewchar) {
      k *= MULTIPLIER;
      char_list *q = table[c2];
      for (char_list *p1 = table[c1]; p1; p1 = p1->next)
	for (char_list *p2 = q; p2; p2 = p2->next) {
	  if (!had_kern) {
	    printf("kernpairs\n");
	    had_kern = 1;
	  }
	  printf("%s %s %d\n", p1->ch, p2->ch, k);
	}
    }
  printf("charset\n");
  char_list unnamed("---");
  for (i = 0; i < 256; i++) 
    if (t.contains(i)) {
      char_list *p = table[i] ? table[i] : &unnamed;
      int m[6];
      m[0] = t.get_width(i);
      m[1] = t.get_height(i);
      m[2] = t.get_depth(i);
      m[3] = t.get_italic(i);
      m[4] = g.get_left_adjustment(i);
      m[5] = g.get_right_adjustment(i);
      printf("%s\t%d", p->ch, m[0]*MULTIPLIER);
      int j;
      for (j = int(sizeof(m)/sizeof(m[0])) - 1; j > 0; j--)
	if (m[j] != 0)
	  break;
      for (k = 1; k <= j; k++)
	printf(",%d", m[k]*MULTIPLIER);
      int type = 0;
      if (m[2] > 0)
	type = 1;
      if (m[1] > xheight)
	type += 2;
      printf("\t%d\t%04o\n", type, i);
      for (p = p->next; p; p = p->next)
	printf("%s\t\"\n", p->ch);
    }
  return 0;
}

static void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [-sv] [-g gf_file] [-k skewchar] tfm_file map_file font\n",
	  program_name);
}
