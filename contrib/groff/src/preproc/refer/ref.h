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

struct label_info;

enum label_type { NORMAL_LABEL, SHORT_LABEL };
const int N_LABEL_TYPES = 2;

struct substring_position {
  int start;
  int length;
  substring_position() : start(-1) { }
};

class int_set {
  string v;
public:
  int_set() { }
  void set(int i);
  int get(int i) const;
};

class reference {
private:
  unsigned h;
  reference_id rid;
  int merged;
  string sort_key;
  int no;
  string *field;
  int nfields;
  unsigned char field_index[256];
  enum { NULL_FIELD_INDEX = 255 };
  string label;
  substring_position separator_pos;
  string short_label;
  substring_position short_separator_pos;
  label_info *label_ptr;
  string authors;
  int computed_authors;
  int last_needed_author;
  int nauthors;
  int_set last_name_unambiguous;

  int contains_field(char) const;
  void insert_field(unsigned char, string &s);
  void delete_field(unsigned char);
  void set_date(string &);
  const char *get_sort_field(int i, int si, int ssi, const char **endp) const;
  int merge_labels_by_parts(reference **, int, label_type, string &);
  int merge_labels_by_number(reference **, int, label_type, string &);
public:
  reference(const char * = 0, int = -1, reference_id * = 0);
  ~reference();
  void output(FILE *);
  void print_sort_key_comment(FILE *);
  void set_number(int);
  int get_number() const { return no; }
  unsigned hash() const { return h; }
  const string &get_label(label_type type) const;
  const substring_position &get_separator_pos(label_type) const;
  int is_merged() const { return merged; }
  void compute_sort_key();
  void compute_hash_code();
  void pre_compute_label();
  void compute_label();
  void immediate_compute_label();
  int classify();
  void merge(reference &);
  int merge_labels(reference **, int, label_type, string &);
  int get_nauthors() const;
  void need_author(int);
  void set_last_name_unambiguous(int);
  void sortify_authors(int, string &) const;
  void canonicalize_authors(string &) const;
  void sortify_field(unsigned char, int, string &) const;
  const char *get_author(int, const char **) const;
  const char *get_author_last_name(int, const char **) const;
  const char *get_date(const char **) const;
  const char *get_year(const char **) const;
  const char *get_field(unsigned char, const char **) const;
  const label_info *get_label_ptr() const { return label_ptr; }
  const char *get_authors(const char **) const;
  // for sorting
  friend int compare_reference(const reference &r1, const reference &r2);
  // for merging
  friend int same_reference(const reference &, const reference &);
  friend int same_year(const reference &, const reference &);
  friend int same_date(const reference &, const reference &);
  friend int same_author_last_name(const reference &, const reference &, int);
  friend int same_author_name(const reference &, const reference &, int);
};

const char *find_year(const char *, const char *, const char **);
const char *find_last_name(const char *, const char *, const char **);

const char *nth_field(int i, const char *start, const char **endp);

void capitalize(const char *ptr, const char *end, string &result);
void reverse_name(const char *ptr, const char *end, string &result);
void uppercase(const char *ptr, const char *end, string &result);
void lowercase(const char *ptr, const char *end, string &result);
void abbreviate_name(const char *ptr, const char *end, string &result);
