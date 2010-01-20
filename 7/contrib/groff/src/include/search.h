// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2004 Free Software Foundation, Inc.
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

class search_item;
class search_item_iterator;

class search_list {
public:
  search_list();
  ~search_list();
  void add_file(const char *fn, int silent = 0);
  int nfiles() const;
private:
  search_item *list;
  int niterators;
  int next_fid;
  friend class search_list_iterator;
};

class bmpattern;

class linear_searcher {
  const char *ignore_fields;
  int truncate_len;
  bmpattern **keys;
  int nkeys;
  const char *search_and_check(const bmpattern *key, const char *buf,
			       const char *bufend, const char **start = 0)
    const;
  int check_match(const char *buf, const char *bufend, const char *match,
		  int matchlen, const char **cont, const char **start)
    const;
public:
  linear_searcher(const char *query, int query_len,
		  const char *ign, int trunc);
  ~linear_searcher();
  int search(const char *buf, const char *bufend,
	     const char **startp, int *lengthp) const;
};

class search_list_iterator {
  search_list *list;
  search_item *ptr;
  search_item_iterator *iter;
  char *query;
  linear_searcher searcher;
public:
  search_list_iterator(search_list *, const char *query);
  ~search_list_iterator();
  int next(const char **, int *, reference_id * = 0);
};

class search_item {
protected:
  char *name;
  int filename_id;
public:
  search_item *next;
  search_item(const char *nm, int fid);
  virtual search_item_iterator *make_search_item_iterator(const char *) = 0;
  virtual ~search_item();
  int is_named(const char *) const;
  virtual int next_filename_id() const;
};

class search_item_iterator {
  char shut_g_plus_plus_up;
public:
  virtual ~search_item_iterator();
  virtual int next(const linear_searcher &, const char **ptr, int *lenp,
		   reference_id *) = 0;
};

search_item *make_index_search_item(const char *filename, int fid);
search_item *make_linear_search_item(int fd, const char *filename, int fid);

extern int linear_truncate_len;
extern const char *linear_ignore_fields;
extern int verify_flag;
