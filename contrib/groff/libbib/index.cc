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
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "posix.h"
#include "lib.h"
#include "cset.h"
#include "cmap.h"
#include "errarg.h"
#include "error.h"

#include "refid.h"
#include "search.h"
#include "index.h"
#include "defs.h"

// Interface to mmap.
extern "C" {
  void *mapread(int fd, int len);
  int unmap(void *, int len);
}

#if 0
const 
#endif
int minus_one = -1;

int verify_flag = 0;

struct word_list;

class index_search_item : public search_item {
  search_item *out_of_date_files;
  index_header header;
  char *buffer;
  void *map_addr;
  int map_len;
  tag *tags;
  int *table;
  int *lists;
  char *pool;
  char *key_buffer;
  char *filename_buffer;
  int filename_buflen;
  char **common_words_table;
  int common_words_table_size;
  const char *ignore_fields;
  time_t mtime;

  const char *do_verify();
  const int *search1(const char **pp, const char *end);
  const int *search(const char *ptr, int length, int **temp_listp);
  const char *munge_filename(const char *);
  void read_common_words_file();
  void add_out_of_date_file(int fd, const char *filename, int fid);
public:
  index_search_item(const char *, int);
  ~index_search_item();
  int load(int fd);
  search_item_iterator *make_search_item_iterator(const char *);
  int verify();
  void check_files();
  int next_filename_id() const;
  friend class index_search_item_iterator;
};

class index_search_item_iterator : public search_item_iterator {
  index_search_item *indx;
  search_item_iterator *out_of_date_files_iter;
  search_item *next_out_of_date_file;
  const int *found_list;
  int *temp_list;
  char *buf;
  int buflen;
  linear_searcher searcher;
  char *query;
  int get_tag(int tagno, const linear_searcher &, const char **, int *,
	      reference_id *);
public:
  index_search_item_iterator(index_search_item *, const char *);
  ~index_search_item_iterator();
  int next(const linear_searcher &, const char **, int *, reference_id *);
};


index_search_item::index_search_item(const char *filename, int fid)
: search_item(filename, fid), out_of_date_files(0), key_buffer(0),
  filename_buffer(0), filename_buflen(0), common_words_table(0),
  map_addr(0), map_len(0), buffer(0)
{
}

index_search_item::~index_search_item()
{
  if (buffer)
    free(buffer);
  if (map_addr) {
    if (unmap(map_addr, map_len) < 0)
      error("unmap: %1", strerror(errno));
  }
  while (out_of_date_files) {
    search_item *tem = out_of_date_files;
    out_of_date_files = out_of_date_files->next;
    delete tem;
  }
  a_delete filename_buffer;
  a_delete key_buffer;
  if (common_words_table) {
    for (int i = 0; i < common_words_table_size; i++)
      a_delete common_words_table[i];
    a_delete common_words_table;
  }
}

class file_closer {
  int *fdp;
public:
  file_closer(int &fd) : fdp(&fd) { }
  ~file_closer() { close(*fdp); }
};
 
// Tell the compiler that a variable is intentionally unused.
inline void unused(void *) { }

int index_search_item::load(int fd)
{
  file_closer fd_closer(fd);	// close fd on return
  unused(&fd_closer);
  struct stat sb;
  if (fstat(fd, &sb) < 0) {
    error("can't fstat `%1': %2", name, strerror(errno));
    return 0;
  }
  if (!S_ISREG(sb.st_mode)) {
    error("`%1' is not a regular file", name);
    return 0;
  }
  mtime = sb.st_mtime;
  int size = int(sb.st_size);
  char *addr;
  map_addr = mapread(fd, size);
  if (map_addr) {
    addr = (char *)map_addr;
    map_len = size;
  }
  else {
    addr = buffer = (char *)malloc(size);
    if (buffer == 0) {
      error("can't allocate buffer for `%1'", name);
      return 0;
    }
    char *ptr = buffer;
    int bytes_to_read = size;
    while (bytes_to_read > 0) {
      int nread = read(fd, ptr, bytes_to_read);
      if (nread == 0) {
	error("unexpected EOF on `%1'", name);
	return 0;
      }
      if (nread < 0) {
	error("read error on `%1': %2", name, strerror(errno));
	return 0;
      }
      bytes_to_read -= nread;
      ptr += nread;
    }
  }
  header = *(index_header *)addr;
  if (header.magic != INDEX_MAGIC) {
    error("`%1' is not an index file: wrong magic number", name);
    return 0;
  }
  if (header.version != INDEX_VERSION) {
    error("version number in `%1' is wrong: was %2, should be %3",
	  name, header.version, INDEX_VERSION);
    return 0;
  }
  int sz = (header.tags_size * sizeof(tag)
	    + header.lists_size * sizeof(int)
	    + header.table_size * sizeof(int)
	    + header.strings_size
	    + sizeof(header));
  if (sz != size) {
    error("size of `%1' is wrong: was %2, should be %3",
	  name, size, sz);
    return 0;
  }
  tags = (tag *)(addr + sizeof(header));
  lists = (int *)(tags + header.tags_size);
  table = (int *)(lists + header.lists_size);
  pool = (char *)(table + header.table_size);
  ignore_fields = strchr(strchr(pool, '\0') + 1, '\0') + 1;
  key_buffer = new char[header.truncate];
  read_common_words_file();
  return 1;
}

const char *index_search_item::do_verify()
{
  if (tags == 0)
    return "not loaded";
  if (lists[header.lists_size - 1] >= 0)
    return "last list element not negative";
  int i;
  for (i = 0; i < header.table_size; i++) {
    int li = table[i];
    if (li >= header.lists_size)
      return "bad list index";
    if (li >= 0) {
      for (int *ptr = lists + li; *ptr >= 0; ptr++) {
	if (*ptr >= header.tags_size)
	  return "bad tag index";
	if (*ptr >= ptr[1] && ptr[1] >= 0)
	  return "list not ordered";
      }
    }
  }
  for (i = 0; i < header.tags_size; i++) {
    if (tags[i].filename_index >= header.strings_size)
      return "bad index in tags";
    if (tags[i].length < 0)
      return "bad length in tags";
    if (tags[i].start < 0)
      return "bad start in tags";
  }
  if (pool[header.strings_size - 1] != '\0')
    return "last character in pool not nul";
  return 0;
}

int index_search_item::verify()
{
  const char *reason = do_verify();
  if (!reason)
    return 1;
  error("`%1' is bad: %2", name, reason);
  return 0;
}

int index_search_item::next_filename_id() const
{
  return filename_id + header.strings_size + 1;
}

search_item_iterator *index_search_item::make_search_item_iterator(
  const char *query)
{
  return new index_search_item_iterator(this, query);
}

search_item *make_index_search_item(const char *filename, int fid)
{
  char *index_filename = new char[strlen(filename) + sizeof(INDEX_SUFFIX)];
  strcpy(index_filename, filename);
  strcat(index_filename, INDEX_SUFFIX);
  int fd = open(index_filename, O_RDONLY);
  if (fd < 0)
    return 0;
  index_search_item *item = new index_search_item(index_filename, fid);
  a_delete index_filename;
  if (!item->load(fd)) {
    close(fd);
    delete item;
    return 0;
  }
  else if (verify_flag && !item->verify()) {
    delete item;
    return 0;
  }
  else {
    item->check_files();
    return item;
  }
}


index_search_item_iterator::index_search_item_iterator(index_search_item *ind,
						       const char *q)
: indx(ind), buf(0), buflen(0), temp_list(0), query(strsave(q)),
  searcher(q, strlen(q), ind->ignore_fields, ind->header.truncate),
  out_of_date_files_iter(0), next_out_of_date_file(0)
{
  found_list = indx->search(q, strlen(q), &temp_list);
  if (!found_list) {
    found_list = &minus_one;
    warning("all keys would have been discarded in constructing index `%1'",
	    indx->name);
  }
}

index_search_item_iterator::~index_search_item_iterator()
{
  a_delete temp_list;
  a_delete buf;
  a_delete query;
  delete out_of_date_files_iter;
}

int index_search_item_iterator::next(const linear_searcher &,
				     const char **pp, int *lenp,
				     reference_id *ridp)
{
  if (found_list) {
    for (;;) {
      int tagno = *found_list;
      if (tagno == -1)
	break;
      found_list++;
      if (get_tag(tagno, searcher, pp, lenp, ridp))
	return 1;
    }
    found_list = 0;
    next_out_of_date_file = indx->out_of_date_files;
  }
  while (next_out_of_date_file) {
    if (out_of_date_files_iter == 0)
      out_of_date_files_iter
	= next_out_of_date_file->make_search_item_iterator(query);
    if (out_of_date_files_iter->next(searcher, pp, lenp, ridp))
      return 1;
    delete out_of_date_files_iter;
    out_of_date_files_iter = 0;
    next_out_of_date_file = next_out_of_date_file->next;
  }
  return 0;
}

int index_search_item_iterator::get_tag(int tagno,
					const linear_searcher &searcher,
					const char **pp, int *lenp,
					reference_id *ridp)
{
  if (tagno < 0 || tagno >= indx->header.tags_size) {
    error("bad tag number");
    return 0;
  }
  tag *tp = indx->tags + tagno;
  const char *filename = indx->munge_filename(indx->pool + tp->filename_index);
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    error("can't open `%1': %2", filename, strerror(errno));
    return 0;
  }
  struct stat sb;
  if (fstat(fd, &sb) < 0) {
    error("can't fstat: %1", strerror(errno));
    close(fd);
    return 0;
  }
  time_t mtime = sb.st_mtime;
  if (mtime > indx->mtime) {
    indx->add_out_of_date_file(fd, filename,
			       indx->filename_id + tp->filename_index);
    return 0;
  }
  int res = 0;
  FILE *fp = fdopen(fd, "r");
  if (!fp) {
    error("fdopen failed");
    close(fd);
    return 0;
  }
  if (tp->start != 0 && fseek(fp, long(tp->start), 0) < 0)
    error("can't seek on `%1': %2", filename, strerror(errno));
  else {
    int length = tp->length;
    int err = 0;
    if (length == 0) {
      struct stat sb;
      if (fstat(fileno(fp), &sb) < 0) {
	error("can't stat `%1': %2", filename, strerror(errno));
	err = 1;
      }
      else if (!S_ISREG(sb.st_mode)) {
	error("`%1' is not a regular file", filename);
	err = 1;
      }
      else
	length = int(sb.st_size);
    }
    if (!err) {
      if (length + 2 > buflen) {
	a_delete buf;
	buflen = length + 2;
	buf = new char[buflen];
      }
      if (fread(buf + 1, 1, length, fp) != length)
	error("fread on `%1' failed: %2", filename, strerror(errno));
      else {
	buf[0] = '\n';
	buf[length + 1] = '\n';
	res = searcher.search(buf + 1, buf + 2 + length, pp, lenp);
	if (res && ridp)
	  *ridp = reference_id(indx->filename_id + tp->filename_index,
			       tp->start);
      }
    }
  }
  fclose(fp);
  return res;
}

const char *index_search_item::munge_filename(const char *filename)
{
  if (filename[0] == '/')
    return filename;
  const char *cwd = pool;
  int need_slash = (cwd[0] != 0 && strchr(cwd, '\0')[-1] != '/');
  int len = strlen(cwd) + strlen(filename) + need_slash + 1;
  if (len > filename_buflen) {
    a_delete filename_buffer;
    filename_buflen = len;
    filename_buffer = new char[len];
  }
  strcpy(filename_buffer, cwd);
  if (need_slash)
    strcat(filename_buffer, "/");
  strcat(filename_buffer, filename);
  return filename_buffer;
}

const int *index_search_item::search1(const char **pp, const char *end)
{
  while (*pp < end && !csalnum(**pp))
    *pp += 1;
  if (*pp >= end)
    return 0;
  const char *start = *pp;
  while (*pp < end && csalnum(**pp))
    *pp += 1;
  int len = *pp - start;
  if (len < header.shortest)
    return 0;
  if (len > header.truncate)
    len = header.truncate;
  int is_number = 1;
  for (int i = 0; i < len; i++)
    if (csdigit(start[i]))
      key_buffer[i] = start[i];
    else {
      key_buffer[i] = cmlower(start[i]);
      is_number = 0;
    }
  if (is_number && !(len == 4 && start[0] == '1' && start[1] == '9'))
    return 0;
  unsigned hc = hash(key_buffer, len);
  if (common_words_table) {
    for (int h = hc % common_words_table_size;
	 common_words_table[h];
	 --h) {
      if (strlen(common_words_table[h]) == len
	  && memcmp(common_words_table[h], key_buffer, len) == 0)
	return 0;
      if (h == 0)
	h = common_words_table_size;
    }
  }
  int li = table[int(hc % header.table_size)];
  return li < 0 ? &minus_one : lists + li;
}

static void merge(int *result, const int *s1, const int *s2)
{
  for (; *s1 >= 0; s1++) {
    while (*s2 >= 0 && *s2 < *s1)
      s2++;
    if (*s2 == *s1)
      *result++ = *s2;
  }
  *result++ = -1;
}

const int *index_search_item::search(const char *ptr, int length,
				     int **temp_listp)
{
  const char *end = ptr + length;
  if (*temp_listp) {
    a_delete *temp_listp;
    *temp_listp = 0;
  }
  const int *first_list = 0;
  while (ptr < end && (first_list = search1(&ptr, end)) == 0)
    ;
  if (!first_list)
    return 0;
  if (*first_list < 0)
    return first_list;
  const int *second_list = 0;
  while (ptr < end && (second_list = search1(&ptr, end)) == 0)
    ;
  if (!second_list)
    return first_list;
  if (*second_list < 0)
    return second_list;
  const int *p;
  for (p = first_list; *p >= 0; p++)
    ;
  int len = p - first_list;
  for (p = second_list; *p >= 0; p++)
    ;
  if (p - second_list < len)
    len = p - second_list;
  int *matches = new int[len + 1];
  merge(matches, first_list, second_list);
  while (ptr < end) {
    const int *list = search1(&ptr, end);
    if (list != 0) {
      if (*list < 0) {
	a_delete matches;
	return list;
      }
      merge(matches, matches, list);
      if (*matches < 0) {
	a_delete matches;
	return &minus_one;
      }
    }
  }
  *temp_listp = matches;
  return matches;
}

void index_search_item::read_common_words_file()
{
  if (header.common <= 0)
    return;
  const char *common_words_file = munge_filename(strchr(pool, '\0') + 1);
  errno = 0;
  FILE *fp = fopen(common_words_file, "r");
  if (!fp) {
    error("can't open `%1': %2", common_words_file, strerror(errno));
    return;
  }
  common_words_table_size = 2*header.common + 1;
  while (!is_prime(common_words_table_size))
    common_words_table_size++;
  common_words_table = new char *[common_words_table_size];
  for (int i = 0; i < common_words_table_size; i++)
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
      if (key_len < header.truncate)
	key_buffer[key_len++] = cmlower(c);
      c = getc(fp);
    } while (c != EOF && csalnum(c));
    if (key_len >= header.shortest) {
      int h = hash(key_buffer, key_len) % common_words_table_size;
      while (common_words_table[h]) {
	if (h == 0)
	  h = common_words_table_size;
	--h;
      }
      common_words_table[h] = new char[key_len + 1];
      memcpy(common_words_table[h], key_buffer, key_len);
      common_words_table[h][key_len] = '\0';
    }
    if (++count >= header.common)
      break;
    key_len = 0;
    if (c == EOF)
      break;
  }
  fclose(fp);
}

void index_search_item::add_out_of_date_file(int fd, const char *filename,
					     int fid)
{
  search_item **pp;
  for (pp = &out_of_date_files; *pp; pp = &(*pp)->next)
    if ((*pp)->is_named(filename))
      return;
  *pp = make_linear_search_item(fd, filename, fid);
  warning("`%1' modified since `%2' created", filename, name);
}

void index_search_item::check_files()
{
  const char *pool_end = pool + header.strings_size;
  for (const char *ptr = strchr(ignore_fields, '\0') + 1;
       ptr < pool_end;
       ptr = strchr(ptr, '\0') + 1) {
    const char *path = munge_filename(ptr);
    struct stat sb;
    if (stat(path, &sb) < 0)
      error("can't stat `%1': %2", path, strerror(errno));
    else if (sb.st_mtime > mtime) {
      int fd = open(path, O_RDONLY);
      if (fd < 0)
	error("can't open `%1': %2", path, strerror(errno));
      else
	add_out_of_date_file(fd, path, filename_id + (ptr - pool));
    }
  }
}
