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

#include "posix.h"
#include "lib.h"
#include "errarg.h"
#include "error.h"
#include "stringclass.h"
#include "cset.h"
#include "cmap.h"

#include "defs.h"
#include "index.h"

extern "C" {
  // Sun's stdlib.h fails to declare this.
  char *mktemp(char *);
}

#define DEFAULT_HASH_TABLE_SIZE 997
#define TEMP_INDEX_TEMPLATE "indxbibXXXXXX"

// (2^n - MALLOC_OVERHEAD) should be a good argument for malloc().

#define MALLOC_OVERHEAD 16

#ifdef BLOCK_SIZE
#undef BLOCK_SIZE
#endif

const int BLOCK_SIZE = ((1024 - MALLOC_OVERHEAD - sizeof(struct block *)
			 - sizeof(int)) / sizeof(int));
struct block {
  block *next;
  int used;
  int v[BLOCK_SIZE];
  
  block(block *p = 0) : next(p), used(0) { }
};

struct block;

union table_entry {
  block *ptr;
  int count;
};

struct word_list {
  word_list *next;
  char *str;
  int len;
  word_list(const char *, int, word_list *);
};

table_entry *hash_table;
int hash_table_size = DEFAULT_HASH_TABLE_SIZE;
// We make this the same size as hash_table so we only have to do one
// mod per key.
static word_list **common_words_table = 0;
char *key_buffer;

FILE *indxfp;
int ntags = 0;
string filenames;
char *temp_index_file = 0;

const char *ignore_fields = "XYZ";
const char *common_words_file = COMMON_WORDS_FILE;
int n_ignore_words = 100;
int truncate_len = 6;
int shortest_len = 3;
int max_keys_per_item = 100;

static void usage();
static void write_hash_table();
static void init_hash_table();
static void read_common_words_file();
static int store_key(char *s, int len);
static void possibly_store_key(char *s, int len);
static int do_whole_file(const char *filename);
static int do_file(const char *filename);
static void store_reference(int filename_index, int pos, int len);
static void check_integer_arg(char opt, const char *arg, int min, int *res);
static void store_filename(const char *);
static void fwrite_or_die(const void *ptr, int size, int nitems, FILE *fp);
static char *get_cwd();

extern "C" {
  void cleanup();
  long dir_name_max(const char *);
  void catch_fatal_signals();
  void ignore_fatal_signals();
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  
  const char *basename = 0;
  typedef int (*parser_t)(const char *);
  parser_t parser = do_file;
  const char *directory = 0;
  const char *foption = 0;
  int opt;
  while ((opt = getopt(argc, argv, "c:o:h:i:k:l:t:n:c:d:f:vw")) != EOF)
    switch (opt) {
    case 'c':
      common_words_file = optarg;
      break;
    case 'd':
      directory = optarg;
      break;
    case 'f':
      foption = optarg;
      break;
    case 'h':
      check_integer_arg('h', optarg, 1, &hash_table_size);
      if (!is_prime(hash_table_size)) {
	while (!is_prime(++hash_table_size))
	  ;
	warning("%1 not prime: using %2 instead", optarg, hash_table_size);
      }
      break;
    case 'i':
      ignore_fields = optarg;
      break;
    case 'k':
      check_integer_arg('k', optarg, 1, &max_keys_per_item);
      break;
    case 'l':
      check_integer_arg('l', optarg, 0, &shortest_len);
      break;
    case 'n':
      check_integer_arg('n', optarg, 0, &n_ignore_words);
      break;
    case 'o':
      basename = optarg;
      break;
    case 't':
      check_integer_arg('t', optarg, 1, &truncate_len);
      break;
    case 'w':
      parser = do_whole_file;
      break;
    case 'v':
      {
	extern const char *version_string;
	fprintf(stderr, "GNU indxbib version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case '?':
      usage();
      break;
    default:
      assert(0);
      break;
    }
  if (optind >= argc && foption == 0)
    fatal("no files and no -f option");
  if (!directory) {
    char *path = get_cwd();
    store_filename(path);
    a_delete path;
  }
  else
    store_filename(directory);
  init_hash_table();
  store_filename(common_words_file);
  store_filename(ignore_fields);
  key_buffer = new char[truncate_len];
  read_common_words_file();
  if (!basename)
    basename = optind < argc ? argv[optind] : DEFAULT_INDEX_NAME;
  const char *p = strrchr(basename, '/');
  long name_max;
  if (p) {
    char *dir = strsave(basename);
    dir[p - basename] = '\0';
    name_max = dir_name_max(dir);
    a_delete dir;
  }
  else
    name_max = dir_name_max(".");
  const char *filename = p ? p + 1 : basename;
  if (name_max >= 0 && strlen(filename) + sizeof(INDEX_SUFFIX) - 1 > name_max)
    fatal("`%1.%2' is too long for a filename", filename, INDEX_SUFFIX);
  if (p) {
    p++;
    temp_index_file = new char[p - basename + sizeof(TEMP_INDEX_TEMPLATE)];
    memcpy(temp_index_file, basename, p - basename);
    strcpy(temp_index_file + (p - basename), TEMP_INDEX_TEMPLATE);
  }
  else {
    temp_index_file = strsave(TEMP_INDEX_TEMPLATE);
  }
  if (!mktemp(temp_index_file) || !temp_index_file[0])
    fatal("cannot create file name for temporary file");
  catch_fatal_signals();
  int fd = creat(temp_index_file, S_IRUSR|S_IRGRP|S_IROTH);
  if (fd < 0)
    fatal("can't create temporary index file: %1", strerror(errno));
  indxfp = fdopen(fd, "w");
  if (indxfp == 0)
    fatal("fdopen failed");
  if (fseek(indxfp, sizeof(index_header), 0) < 0)
    fatal("can't seek past index header: %1", strerror(errno));
  int failed = 0;
  if (foption) {
    FILE *fp = stdin;
    if (strcmp(foption, "-") != 0) {
      errno = 0;
      fp = fopen(foption, "r");
      if (!fp)
	fatal("can't open `%1': %2", foption, strerror(errno));
    }
    string path;
    int lineno = 1;
    for (;;) {
      int c;
      for (c = getc(fp); c != '\n' && c != EOF; c = getc(fp)) {
	if (c == '\0')
	  error_with_file_and_line(foption, lineno,
				   "nul character in pathname ignored");
	else
	  path += c;
      }
      if (path.length() > 0) {
	path += '\0';
	if (!(*parser)(path.contents()))
	  failed = 1;
	path.clear();
      }
      if (c == EOF)
	break;
      lineno++;
    }
    if (fp != stdin)
      fclose(fp);
  }
  for (int i = optind; i < argc; i++)
    if (!(*parser)(argv[i]))
      failed = 1;
  write_hash_table();
  if (fclose(indxfp) < 0)
    fatal("error closing temporary index file: %1", strerror(errno));
  char *index_file = new char[strlen(basename) + sizeof(INDEX_SUFFIX)];    
  strcpy(index_file, basename);
  strcat(index_file, INDEX_SUFFIX);
#ifdef HAVE_RENAME
  if (rename(temp_index_file, index_file) < 0)
    fatal("can't rename temporary index file: %1", strerror(errno));
#else /* not HAVE_RENAME */
  ignore_fatal_signals();
  if (unlink(index_file) < 0) {
    if (errno != ENOENT)
      fatal("can't unlink `%1': %2", index_file, strerror(errno));
  }
  if (link(temp_index_file, index_file) < 0)
    fatal("can't link temporary index file: %1", strerror(errno));
  if (unlink(temp_index_file) < 0)
    fatal("can't unlink temporary index file: %1", strerror(errno));
#endif /* not HAVE_RENAME */
  temp_index_file = 0;
  return failed;
}

static void usage()
{
  fprintf(stderr,
"usage: %s [-vw] [-c file] [-d dir] [-f file] [-h n] [-i XYZ] [-k n]\n"
"       [-l n] [-n n] [-o base] [-t n] [files...]\n",
	  program_name);
  exit(1);
}

static void check_integer_arg(char opt, const char *arg, int min, int *res)
{
  char *ptr;
  long n = strtol(arg, &ptr, 10);
  if (n == 0 && ptr == arg)
    error("argument to -%1 not an integer", opt);
  else if (n < min)
    error("argument to -%1 must not be less than %2", opt, min);
  else {
    if (n > INT_MAX)
      error("argument to -%1 greater than maximum integer", opt);
    else if (*ptr != '\0')
      error("junk after integer argument to -%1", opt);
    *res = int(n);
  }
}

static char *get_cwd()
{
  char *buf;
  int size = 12;

  for (;;) {
    buf = new char[size];
    if (getcwd(buf, size))
      break;
    if (errno != ERANGE)
      fatal("cannot get current working directory: %1", strerror(errno));
    a_delete buf;
    if (size == INT_MAX)
      fatal("current working directory longer than INT_MAX");
    if (size > INT_MAX/2)
      size = INT_MAX;
    else
      size *= 2;
  }
  return buf;
}

word_list::word_list(const char *s, int n, word_list *p)
: next(p), len(n)
{
  str = new char[n];
  memcpy(str, s, n);
}

static void read_common_words_file()
{
  if (n_ignore_words <= 0)
    return;
  errno = 0;
  FILE *fp = fopen(common_words_file, "r");
  if (!fp)
    fatal("can't open `%1': %2", common_words_file, strerror(errno));
  common_words_table = new word_list * [hash_table_size];
  for (int i = 0; i < hash_table_size; i++)
    common_words_table[i] = 0;
  int count = 0;
  int key_len = 0;
  for (;;) {
    int c = getc(fp);
    while (c != EOF && !csalnum(c))
      c = getc(fp);
    if (c == EOF)
      break;
    do {
      if (key_len < truncate_len)
	key_buffer[key_len++] = cmlower(c);
      c = getc(fp);
    } while (c != EOF && csalnum(c));
    if (key_len >= shortest_len) {
      int h = hash(key_buffer, key_len) % hash_table_size;
      common_words_table[h] = new word_list(key_buffer, key_len,
					    common_words_table[h]);
    }
    if (++count >= n_ignore_words)
      break;
    key_len = 0;
    if (c == EOF)
      break;
  }
  n_ignore_words = count;
  fclose(fp);
}

static int do_whole_file(const char *filename)
{
  errno = 0;
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    error("can't open `%1': %2", filename, strerror(errno));
    return 0;
  }
  int count = 0;
  int key_len = 0;
  int c;
  while ((c = getc(fp)) != EOF) {
    if (csalnum(c)) {
      key_len = 1;
      key_buffer[0] = c;
      while ((c = getc(fp)) != EOF) {
	if (!csalnum(c))
	  break;
	if (key_len < truncate_len)
	  key_buffer[key_len++] = c;
      }
      if (store_key(key_buffer, key_len)) {
	if (++count >= max_keys_per_item)
	  break;
      }
      if (c == EOF)
	break;
    }
  }
  store_reference(filenames.length(), 0, 0);
  store_filename(filename);
  fclose(fp);
  return 1;
}

static int do_file(const char *filename)
{
  errno = 0;
  FILE *fp = fopen(filename, "r");
  if (fp == 0) {
    error("can't open `%1': %2", filename, strerror(errno));
    return 0;
  }
  int filename_index = filenames.length();
  store_filename(filename);

  enum {
    START,	// at the start of the file; also in between references
    BOL,	// in the middle of a reference, at the beginning of the line
    PERCENT,	// seen a percent at the beginning of the line
    IGNORE,	// ignoring a field
    IGNORE_BOL,	// at the beginning of a line ignoring a field
    KEY,	// in the middle of a key
    DISCARD,	// after truncate_len bytes of a key
    MIDDLE	// in between keys
  } state = START;
  
  // In states START, BOL, IGNORE_BOL, space_count how many spaces at
  // the beginning have been seen.  In states PERCENT, IGNORE, KEY,
  // MIDDLE space_count must be 0.
  int space_count = 0;
  int byte_count = 0;		// bytes read
  int key_len = 0;
  int ref_start = -1;		// position of start of current reference
  for (;;) {
    int c = getc(fp);
    if (c == EOF)
      break;
    byte_count++;
    switch (state) {
    case START:
      if (c == ' ' || c == '\t') {
	space_count++;
	break;
      }
      if (c == '\n') {
	space_count = 0;
	break;
      }
      ref_start = byte_count - space_count - 1;
      space_count = 0;
      if (c == '%')
	state = PERCENT;
      else if (csalnum(c)) {
	state = KEY;
	key_buffer[0] = c;
	key_len = 1;
      }
      else
	state = MIDDLE;
      break;
    case BOL:
      switch (c) {
      case '%':
	if (space_count > 0) {
	  space_count = 0;
	  state = MIDDLE;
	}
	else
	  state = PERCENT;
	break;
      case ' ':
      case '\t':
	space_count++;
	break;
      case '\n':
	store_reference(filename_index, ref_start,
			byte_count - 1 - space_count - ref_start);
	state = START;
	space_count = 0;
	break;
      default:
	space_count = 0;
	if (csalnum(c)) {
	  state = KEY;
	  key_buffer[0] = c;
	  key_len = 1;
	}
	else
	  state = MIDDLE;
      }
      break;
    case PERCENT:
      if (strchr(ignore_fields, c) != 0)
	state = IGNORE;
      else if (c == '\n')
	state = BOL;
      else
	state = MIDDLE;
      break;
    case IGNORE:
      if (c == '\n')
	state = IGNORE_BOL;
      break;
    case IGNORE_BOL:
      switch (c) {
      case '%':
	if (space_count > 0) {
	  state = IGNORE;
	  space_count = 0;
	}
	else
	  state = PERCENT;
	break;
      case ' ':
      case '\t':
	space_count++;
	break;
      case '\n':
	store_reference(filename_index, ref_start,
			byte_count - 1 - space_count - ref_start);
	state = START;
	space_count = 0;
	break;
      default:
	space_count = 0;
	state = IGNORE;
      }
      break;
    case KEY:
      if (csalnum(c)) {
	if (key_len < truncate_len)
	  key_buffer[key_len++] = c;
	else
	  state = DISCARD;
      }
      else {
	possibly_store_key(key_buffer, key_len);
	key_len = 0;
	if (c == '\n')
	  state = BOL;
	else
	  state = MIDDLE;
      }
      break;
    case DISCARD:
      if (!csalnum(c)) {
	possibly_store_key(key_buffer, key_len);
	key_len = 0;
	if (c == '\n')
	  state = BOL;
	else
	  state = MIDDLE;
      }
      break;
    case MIDDLE:
      if (csalnum(c)) {
	state = KEY;
	key_buffer[0] = c;
	key_len = 1;
      }
      else if (c == '\n')
	state = BOL;
      break;
    default:
      assert(0);
    }
  }
  switch (state) {
  case START:
    break;
  case DISCARD:
  case KEY:
    possibly_store_key(key_buffer, key_len);
    // fall through
  case BOL:
  case PERCENT:
  case IGNORE_BOL:
  case IGNORE:
  case MIDDLE:
    store_reference(filename_index, ref_start,
		    byte_count - ref_start - space_count);
    break;
  default:
    assert(0);
  }
  fclose(fp);
  return 1;
}

static void store_reference(int filename_index, int pos, int len)
{
  tag t;
  t.filename_index = filename_index;
  t.start = pos;
  t.length = len;
  fwrite_or_die(&t, sizeof(t), 1, indxfp);
  ntags++;
}

static void store_filename(const char *fn)
{
  filenames += fn;
  filenames += '\0';
}

static void init_hash_table()
{
  hash_table = new table_entry[hash_table_size];
  for (int i = 0; i < hash_table_size; i++)
    hash_table[i].ptr = 0;
}

static void possibly_store_key(char *s, int len)
{
  static int last_tagno = -1;
  static int key_count;
  if (last_tagno != ntags) {
    last_tagno = ntags;
    key_count = 0;
  }
  if (key_count < max_keys_per_item) {
    if (store_key(s, len))
      key_count++;
  }
}

static int store_key(char *s, int len)
{
  if (len < shortest_len)
    return 0;
  int is_number = 1;
  for (int i = 0; i < len; i++)
    if (!csdigit(s[i])) {
      is_number = 0;
      s[i] = cmlower(s[i]);
    }
  if (is_number && !(len == 4 && s[0] == '1' && s[1] == '9'))
    return 0;
  int h = hash(s, len) % hash_table_size;
  if (common_words_table) {
    for (word_list *ptr = common_words_table[h]; ptr; ptr = ptr->next)
      if (len == ptr->len && memcmp(s, ptr->str, len) == 0)
	return 0;
  }
  table_entry *pp =  hash_table + h;
  if (!pp->ptr)
    pp->ptr = new block;
  else if (pp->ptr->v[pp->ptr->used - 1] == ntags)
    return 1;
  else if (pp->ptr->used >= BLOCK_SIZE)
    pp->ptr = new block(pp->ptr);
  pp->ptr->v[(pp->ptr->used)++] = ntags;
  return 1;
}

static void write_hash_table()
{
  const int minus_one = -1;
  int li = 0;
  for (int i = 0; i < hash_table_size; i++) {
    block *ptr = hash_table[i].ptr;
    if (!ptr)
      hash_table[i].count = -1;
    else {
      hash_table[i].count = li;
      block *rev = 0;
      while (ptr) {
	block *tem = ptr;
	ptr = ptr->next;
	tem->next = rev;
	rev = tem;
      }
      while (rev) {
	fwrite_or_die(rev->v, sizeof(int), rev->used, indxfp);
	li += rev->used;
	block *tem = rev;
	rev = rev->next;
	delete tem;
      }
      fwrite_or_die(&minus_one, sizeof(int), 1, indxfp);
      li += 1;
    }
  }
  if (sizeof(table_entry) == sizeof(int))
    fwrite_or_die(hash_table, sizeof(int), hash_table_size, indxfp);
  else {
    // write it out word by word
    for (int i = 0; i < hash_table_size; i++)
      fwrite_or_die(&hash_table[i].count, sizeof(int), 1, indxfp);
  }
  fwrite_or_die(filenames.contents(), 1, filenames.length(), indxfp);
  if (fseek(indxfp, 0, 0) < 0)
    fatal("error seeking on index file: %1", strerror(errno));
  index_header h;
  h.magic = INDEX_MAGIC;
  h.version = INDEX_VERSION;
  h.tags_size = ntags;
  h.lists_size = li;
  h.table_size = hash_table_size;
  h.strings_size = filenames.length();
  h.truncate = truncate_len;
  h.shortest = shortest_len;
  h.common = n_ignore_words;
  fwrite_or_die(&h, sizeof(h), 1, indxfp);
}

static void fwrite_or_die(const void *ptr, int size, int nitems, FILE *fp)
{
  if (fwrite(ptr, size, nitems, fp) != nitems)
    fatal("fwrite failed: %1", strerror(errno));
}

void fatal_error_exit()
{
  cleanup();
  exit(3);
}

extern "C" {

void cleanup()
{
  if (temp_index_file)
    unlink(temp_index_file);
}

}
