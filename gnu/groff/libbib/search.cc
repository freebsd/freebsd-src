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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "posix.h"
#include "lib.h"
#include "errarg.h"
#include "error.h"

#include "refid.h"
#include "search.h"

int linear_truncate_len = 6;
const char *linear_ignore_fields = "XYZ";

search_list::search_list()
: list(0), niterators(0), next_fid(1)
{
}

search_list::~search_list()
{
  assert(niterators == 0);
  while (list) {
    search_item *tem = list->next;
    delete list;
    list = tem;
  }
}

void search_list::add_file(const char *filename, int silent)
{
  search_item *p = make_index_search_item(filename, next_fid);
  if (!p) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
      if (!silent)
	error("can't open `%1': %2", filename, strerror(errno));
    }
    else
      p = make_linear_search_item(fd, filename, next_fid);
  }
  if (p) {
    for (search_item **pp = &list; *pp; pp = &(*pp)->next)
      ;
    *pp = p;
    next_fid = p->next_filename_id();
  }
}

int search_list::nfiles() const
{
  int n = 0;
  for (search_item *ptr = list; ptr; ptr = ptr->next)
    n++;
  return n;
}

search_list_iterator::search_list_iterator(search_list *p, const char *q)
: list(p), ptr(p->list), iter(0), query(strsave(q)),
  searcher(q, strlen(q), linear_ignore_fields, linear_truncate_len)
{
  list->niterators += 1;
}

search_list_iterator::~search_list_iterator()
{
  list->niterators -= 1;
  a_delete query;
  delete iter;
}

int search_list_iterator::next(const char **pp, int *lenp, reference_id *ridp)
{
  while (ptr) {
    if (iter == 0)
      iter = ptr->make_search_item_iterator(query);
    if (iter->next(searcher, pp, lenp, ridp))
      return 1;
    delete iter;
    iter = 0;
    ptr = ptr->next;
  }
  return 0;
}

search_item::search_item(const char *nm, int fid)
: next(0), name(strsave(nm)), filename_id(fid)
{
}

search_item::~search_item()
{
  a_delete name;
}

int search_item::is_named(const char *nm) const
{
  return strcmp(name, nm) == 0;
}

int search_item::next_filename_id() const
{
  return filename_id + 1;
}

search_item_iterator::~search_item_iterator()
{
}
