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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */


#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "posix.h"
#include "lib.h"
#include "errarg.h"
#include "error.h"
#include "cset.h"
#include "cmap.h"

#include "refid.h"
#include "search.h"

class file_buffer {
  char *buffer;
  char *bufend;
public:
  file_buffer();
  ~file_buffer();
  int load(int fd, const char *filename);
  const char *get_start() const;
  const char *get_end() const;
};

typedef unsigned char uchar;

static uchar map[256];
static uchar inv_map[256][3];

struct map_init {
  map_init();
};

static map_init the_map_init;

map_init::map_init()
{
  for (int i = 0; i < 256; i++)
    map[i] = csalnum(i) ? cmlower(i) : '\0';
  for (i = 0; i < 256; i++) {
    if (cslower(i)) {
      inv_map[i][0] = i;
      inv_map[i][1] = cmupper(i);
      inv_map[i][2] = '\0';
    }
    else if (csdigit(i)) {
      inv_map[i][0] = i;
      inv_map[i][1] = 0;
    }
    else
      inv_map[i][0] = '\0';
  }
}


class bmpattern {
  char *pat;
  int len;
  int delta[256];
public:
  bmpattern(const char *pattern, int pattern_length);
  ~bmpattern();
  const char *search(const char *p, const char *end) const;
  int length() const;
};

bmpattern::bmpattern(const char *pattern, int pattern_length)
: len(pattern_length)
{
  pat = new char[len];
  int i;
  for (i = 0; i < len; i++)
    pat[i] = map[uchar(pattern[i])];
  for (i = 0; i < 256; i++)
    delta[i] = len;
  for (i = 0; i < len; i++)
    for (const unsigned char *inv = inv_map[uchar(pat[i])]; *inv; inv++)
      delta[*inv] = len - i - 1;
}

const char *bmpattern::search(const char *buf, const char *end) const
{
  int buflen = end - buf;
  if (len > buflen)
    return 0;
  const char *strend;
  if (buflen > len*4)
    strend = end - len*4;
  else
    strend = buf;
  const char *k = buf + len - 1;
  const int *del = delta;
  const char *pattern = pat;
  for (;;) {
    while (k < strend) {
      int t = del[uchar(*k)];
      if (!t)
	break;
      k += t;
      k += del[uchar(*k)];
      k += del[uchar(*k)];
    }
    while (k < end && del[uchar(*k)] != 0)
      k++;
    if (k == end)
      break;
    int j = len - 1;
    const char *s = k;
    for (;;) {
      if (j == 0)
	return s;
      if (map[uchar(*--s)] != pattern[--j])
	break;
    }
    k++;
  }
  return 0;
}

bmpattern::~bmpattern()
{
  a_delete pat;
}

inline int bmpattern::length() const
{
  return len;
}


static const char *find_end(const char *bufend, const char *p);

const char *linear_searcher::search_and_check(const bmpattern *key,
  const char *buf, const char *bufend, const char **start) const
{
  assert(buf[-1] == '\n');
  assert(bufend[-1] == '\n');
  const char *ptr = buf;
  for (;;) {
    const char *found = key->search(ptr, bufend);
    if (!found)
      break;
    if (check_match(buf, bufend, found, key->length(), &ptr, start))
      return found;
  }
  return 0;
}

static const char *skip_field(const char *end, const char *p)
{
  for (;;)
    if (*p++ == '\n') {
      if (p == end || *p == '%')
	break;
      for (const char *q = p; *q == ' ' || *q == '\t'; q++)
	;
      if (*q == '\n')
	break;
      p = q + 1;
    }
  return p;
}

static const char *find_end(const char *bufend, const char *p)
{
  for (;;)
    if (*p++ == '\n') {
      if (p == bufend)
	break;
      for (const char *q = p; *q == ' ' || *q == '\t'; q++)
	;
      if (*q == '\n')
	break;
      p = q + 1;
    }
  return p;
}


int linear_searcher::check_match(const char *buf, const char *bufend,
				 const char *match, int matchlen,
				 const char **cont, const char **start) const
{
  *cont = match + 1;
  // The user is required to supply only the first truncate_len characters
  // of the key.  If truncate_len <= 0, he must supply all the key.
  if ((truncate_len <= 0 || matchlen < truncate_len)
      && map[uchar(match[matchlen])] != '\0')
    return 0;

  // The character before the match must not be an alphanumeric
  // character (unless the alphanumeric character follows one or two
  // percent characters at the beginning of the line), nor must it be
  // a percent character at the beginning of a line, nor a percent
  // character following a percent character at the beginning of a
  // line.

  switch (match - buf) {
  case 0:
    break;
  case 1:
    if (match[-1] == '%' || map[uchar(match[-1])] != '\0')
      return 0;
    break;
  case 2:
    if (map[uchar(match[-1])] != '\0' && match[-2] != '%')
      return 0;
    if (match[-1] == '%'
	&& (match[-2] == '\n' || match[-2] == '%'))
      return 0;
    break;
  default:
    if (map[uchar(match[-1])] != '\0'
	&& !(match[-2] == '%'
	     && (match[-3] == '\n'
		 || (match[-3] == '%' && match[-4] == '\n'))))
      return 0;
    if (match[-1] == '%'
	&& (match[-2] == '\n'
	    || (match[-2] == '%' && match[-3] == '\n')))
      return 0;
  }
    
  const char *p = match;
  int had_percent = 0;
  for (;;) {
    if (*p == '\n') {
      if (!had_percent && p[1] == '%') {
	if (p[2] != '\0' && strchr(ignore_fields, p[2]) != 0) {
	  *cont = skip_field(bufend, match + matchlen);
	  return 0;
	}
	if (!start)
	  break;
	had_percent = 1;
      }
      if (p <= buf) {
	if (start)
	  *start = p + 1;
	return 1;
      }
      for (const char *q = p - 1; *q == ' ' || *q == '\t'; q--)
	;
      if (*q == '\n') {
	if (start)
	  *start = p + 1;
	break;
      }
      p = q;
    }
    p--;
  }
  return 1;
}

file_buffer::file_buffer()
: buffer(0), bufend(0)
{
}

file_buffer::~file_buffer()
{
  a_delete buffer;
}

const char *file_buffer::get_start() const
{
  return buffer ? buffer + 4 : 0;
}

const char *file_buffer::get_end() const
{
  return bufend;
}

int file_buffer::load(int fd, const char *filename)
{
  struct stat sb;
  if (fstat(fd, &sb) < 0)
    error("can't fstat `%1': %2", filename, strerror(errno));
  else if ((sb.st_mode & S_IFMT) != S_IFREG)
    error("`%1' is not a regular file", filename);
  else {
    // We need one character extra at the beginning for an additional newline
    // used as a sentinel.  We get 4 instead so that the read buffer will be
    // word-aligned.  This seems to make the read slightly faster.  We also
    // need one character at the end also for an addional newline used as a
    // sentinel.
    int size = int(sb.st_size);
    buffer = new char[size + 4 + 1];
    int nread = read(fd, buffer + 4, size);
    if (nread < 0)
      error("error reading `%1': %2", filename, strerror(errno));
    else if (nread != size)
      error("size of `%1' decreased", filename);
    else {
      char c;
      nread = read(fd, &c, 1);
      if (nread != 0)
	error("size of `%1' increased", filename);
      else if (memchr(buffer + 4, '\0', size < 1024 ? size : 1024) != 0)
	error("database `%1' is a binary file", filename);
      else {
	close(fd);
	buffer[3] = '\n';
	bufend = buffer + 4 + size;
	if (bufend[-1] != '\n')
	  *bufend++ = '\n';
	return 1;
      }
    }
    a_delete buffer;
    buffer = 0;
  }
  close(fd);
  return 0;
}

linear_searcher::linear_searcher(const char *query, int query_len,
				 const char *ign, int trunc)
: keys(0), nkeys(0), truncate_len(trunc), ignore_fields(ign)
{
  const char *query_end = query + query_len;
  int nk = 0;
  const char *p;
  for (p = query; p < query_end; p++)
    if (map[uchar(*p)] != '\0'
	&& (p[1] == '\0' || map[uchar(p[1])] == '\0'))
      nk++;
  if (nk == 0)
    return;
  keys = new bmpattern*[nk];
  p = query;
  for (;;) {
    while (p < query_end && map[uchar(*p)] == '\0')
      p++;
    if (p == query_end)
      break;
    const char *start = p;
    while (p < query_end && map[uchar(*p)] != '\0')
      p++;
    keys[nkeys++] = new bmpattern(start, p - start);
  }
  assert(nkeys <= nk);
  if (nkeys == 0) {
    a_delete keys;
    keys = 0;
  }
}

linear_searcher::~linear_searcher()
{
  for (int i = 0; i < nkeys; i++)
    delete keys[i];
  a_delete keys;
}

int linear_searcher::search(const char *buffer, const char *bufend,
			    const char **startp, int *lengthp) const
{
  assert(bufend - buffer > 0);
  assert(buffer[-1] == '\n');
  assert(bufend[-1] == '\n');
  if (nkeys == 0)
    return 0;
  for (;;) {
    const char *refstart;
    const char *found = search_and_check(keys[0], buffer, bufend, &refstart);
    if (!found)
      break;
    const char *refend = find_end(bufend, found + keys[0]->length());
    for (int i = 1; i < nkeys; i++)
      if (!search_and_check(keys[i], refstart, refend))
	break;
    if (i >= nkeys) {
      *startp = refstart;
      *lengthp = refend - refstart;
      return 1;
    }
    buffer = refend;
  }
  return 0;
}

class linear_search_item : public search_item {
  file_buffer fbuf;
public:
  linear_search_item(const char *filename, int fid);
  ~linear_search_item();
  int load(int fd);
  search_item_iterator *make_search_item_iterator(const char *);
  friend class linear_search_item_iterator;
};

class linear_search_item_iterator : public search_item_iterator {
  linear_search_item *lsi;
  int pos;
public:
  linear_search_item_iterator(linear_search_item *, const char *query);
  ~linear_search_item_iterator();
  int next(const linear_searcher &, const char **ptr, int *lenp,
	   reference_id *ridp);
};

search_item *make_linear_search_item(int fd, const char *filename, int fid)
{
  linear_search_item *item = new linear_search_item(filename, fid);
  if (!item->load(fd)) {
    delete item;
    return 0;
  }
  else
    return item;
}

linear_search_item::linear_search_item(const char *filename, int fid)
: search_item(filename, fid)
{
}

linear_search_item::~linear_search_item()
{
}

int linear_search_item::load(int fd)
{
  return fbuf.load(fd, name);
}

search_item_iterator *linear_search_item::make_search_item_iterator(
  const char *query)
{
  return new linear_search_item_iterator(this, query);
}

linear_search_item_iterator::linear_search_item_iterator(
  linear_search_item *p, const char *)
: lsi(p), pos(0)
{
}

linear_search_item_iterator::~linear_search_item_iterator()
{
}

int linear_search_item_iterator::next(const linear_searcher &searcher,
				      const char **startp, int *lengthp,
				      reference_id *ridp)
{
  const char *bufstart = lsi->fbuf.get_start();
  const char *bufend = lsi->fbuf.get_end();
  const char *ptr = bufstart + pos;
  if (ptr < bufend && searcher.search(ptr, bufend, startp, lengthp)) {
    pos = *startp + *lengthp - bufstart;
    if (ridp)
      *ridp = reference_id(lsi->filename_id, *startp - bufstart);
    return 1;
  }
  else
    return 0;
}
