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
     
#include "refer.h"
#include "refid.h"
#include "ref.h"
#include "token.h"

static const char *find_day(const char *, const char *, const char **);
static int find_month(const char *start, const char *end);
static void abbreviate_names(string &);

#define DEFAULT_ARTICLES "the\000a\000an"
     
string articles(DEFAULT_ARTICLES, sizeof(DEFAULT_ARTICLES));

// Multiple occurrences of fields are separated by FIELD_SEPARATOR.
const char FIELD_SEPARATOR = '\0';

const char MULTI_FIELD_NAMES[] = "AE";
const char *AUTHOR_FIELDS = "AQ";

enum { OTHER, JOURNAL_ARTICLE, BOOK, ARTICLE_IN_BOOK, TECH_REPORT, BELL_TM };

const char *reference_types[] = {
  "other",
  "journal-article",
  "book",
  "article-in-book",
  "tech-report",
  "bell-tm",
};

static string temp_fields[256];

reference::reference(const char *start, int len, reference_id *ridp)
: no(-1), field(0), nfields(0), h(0), merged(0), label_ptr(0),
  computed_authors(0), last_needed_author(-1), nauthors(-1)
{
  for (int i = 0; i < 256; i++)
    field_index[i] = NULL_FIELD_INDEX;
  if (ridp)
    rid = *ridp;
  if (start == 0)
    return;
  if (len <= 0)
    return;
  const char *end = start + len;
  const char *ptr = start;
  assert(*ptr == '%');
  while (ptr < end) {
    if (ptr + 1 < end && ptr[1] != '\0'
	&& ((ptr[1] != '%' && ptr[1] == annotation_field)
	    || (ptr + 2 < end && ptr[1] == '%' && ptr[2] != '\0'
		&& discard_fields.search(ptr[2]) < 0))) {
      if (ptr[1] == '%')
	ptr++;
      string &f = temp_fields[(unsigned char)ptr[1]];
      ptr += 2;
      while (ptr < end && csspace(*ptr))
	ptr++;
      for (;;) {
	for (;;) {
	  if (ptr >= end) {
	    f += '\n';
	    break;
	  }
	  f += *ptr;
	  if (*ptr++ == '\n')
	    break;
	}
	if (ptr >= end || *ptr == '%')
	  break;
      }
    }
    else if (ptr + 1 < end && ptr[1] != '\0' && ptr[1] != '%'
	     && discard_fields.search(ptr[1]) < 0) {
      string &f = temp_fields[(unsigned char)ptr[1]];
      if (f.length() > 0) {
	if (strchr(MULTI_FIELD_NAMES, ptr[1]) != 0)
	  f += FIELD_SEPARATOR;
	else
	  f.clear();
      }
      ptr += 2;
      if (ptr < end) {
	if (*ptr == ' ')
	  ptr++;
	for (;;) {
	  const char *p = ptr;
	  while (ptr < end && *ptr != '\n')
	    ptr++;
	  // strip trailing white space
	  const char *q = ptr;
	  while (q > p && q[-1] != '\n' && csspace(q[-1]))
	    q--;
	  while (p < q)
	    f += *p++;
	  if (ptr >= end)
	    break;
	  ptr++;
	  if (ptr >= end)
	    break;
	  if (*ptr == '%')
	    break;
	  f += ' ';
	}
      }
    }
    else {
      // skip this field
      for (;;) {
	while (ptr < end && *ptr++ != '\n')
	  ;
	if (ptr >= end || *ptr == '%')
	  break;
      }
    }
  }
  for (i = 0; i < 256; i++)
    if (temp_fields[i].length() > 0)
      nfields++;
  field = new string[nfields];
  int j = 0;
  for (i = 0; i < 256; i++)
    if (temp_fields[i].length() > 0) {
      field[j].move(temp_fields[i]);
      if (abbreviate_fields.search(i) >= 0)
	abbreviate_names(field[j]);
      field_index[i] = j;
      j++;
    }
}

reference::~reference()
{
  if (nfields > 0)
    ad_delete(nfields) field;
}

// ref is the inline, this is the database ref

void reference::merge(reference &ref)
{
  int i;
  for (i = 0; i < 256; i++)
    if (field_index[i] != NULL_FIELD_INDEX)
      temp_fields[i].move(field[field_index[i]]);
  for (i = 0; i < 256; i++)
    if (ref.field_index[i] != NULL_FIELD_INDEX)
      temp_fields[i].move(ref.field[ref.field_index[i]]);
  for (i = 0; i < 256; i++)
    field_index[i] = NULL_FIELD_INDEX;
  int old_nfields = nfields;
  nfields = 0;
  for (i = 0; i < 256; i++)
    if (temp_fields[i].length() > 0)
      nfields++;
  if (nfields != old_nfields) {
    if (old_nfields > 0)
      ad_delete(old_nfields) field;
    field = new string[nfields];
  }
  int j = 0;
  for (i = 0; i < 256; i++)
    if (temp_fields[i].length() > 0) {
      field[j].move(temp_fields[i]);
      field_index[i] = j;
      j++;
    }
  merged = 1;
}

void reference::insert_field(unsigned char c, string &s)
{
  assert(s.length() > 0);
  if (field_index[c] != NULL_FIELD_INDEX) {
    field[field_index[c]].move(s);
    return;
  }
  assert(field_index[c] == NULL_FIELD_INDEX);
  string *old_field = field;
  field = new string[nfields + 1];
  int pos = 0;
  for (int i = 0; i < int(c); i++)
    if (field_index[i] != NULL_FIELD_INDEX)
      pos++;
  for (i = 0; i < pos; i++)
    field[i].move(old_field[i]);
  field[pos].move(s);
  for (i = pos; i < nfields; i++)
    field[i + 1].move(old_field[i]);
  if (nfields > 0)
    ad_delete(nfields) old_field;
  nfields++;
  field_index[c] = pos;
  for (i = c + 1; i < 256; i++)
    if (field_index[i] != NULL_FIELD_INDEX)
      field_index[i] += 1;
}

void reference::delete_field(unsigned char c)
{
  if (field_index[c] == NULL_FIELD_INDEX)
    return;
  string *old_field = field;
  field = new string[nfields - 1];
  for (int i = 0; i < int(field_index[c]); i++)
    field[i].move(old_field[i]);
  for (i = field_index[c]; i < nfields - 1; i++)
    field[i].move(old_field[i + 1]);
  if (nfields > 0)
    ad_delete(nfields) old_field;
  nfields--;
  field_index[c] = NULL_FIELD_INDEX;
  for (i = c + 1; i < 256; i++)
    if (field_index[i] != NULL_FIELD_INDEX)
      field_index[i] -= 1;
}
    
void reference::compute_hash_code()
{
  if (!rid.is_null())
    h = rid.hash();
  else {
    h = 0;
    for (int i = 0; i < nfields; i++)
      if (field[i].length() > 0) {
	h <<= 4;
	h ^= hash_string(field[i].contents(), field[i].length());
      }
  }
}

void reference::set_number(int n)
{
  no = n;
}

const char SORT_SEP = '\001';
const char SORT_SUB_SEP = '\002';
const char SORT_SUB_SUB_SEP = '\003';

// sep specifies additional word separators

void sortify_words(const char *s, const char *end, const char *sep,
		   string &result)
{
  int non_empty = 0;
  int need_separator = 0;
  for (;;) {
    const char *token_start = s;
    if (!get_token(&s, end))
      break;
    if ((s - token_start == 1
	 && (*token_start == ' '
	     || *token_start == '\n'
	     || (sep && *token_start != '\0'
		 && strchr(sep, *token_start) != 0)))
	|| (s - token_start == 2
	    && token_start[0] == '\\' && token_start[1] == ' ')) {
      if (non_empty)
	need_separator = 1;
    }
    else {
      const token_info *ti = lookup_token(token_start, s);
      if (ti->sortify_non_empty(token_start, s)) {
	if (need_separator) {
	  result += ' ';
	  need_separator = 0;
	}
	ti->sortify(token_start, s, result);
	non_empty = 1;
      }
    }
  }
}

void sortify_word(const char *s, const char *end, string &result)
{
  for (;;) {
    const char *token_start = s;
    if (!get_token(&s, end))
      break;
    const token_info *ti = lookup_token(token_start, s);
    ti->sortify(token_start, s, result);
  }
}

void sortify_other(const char *s, int len, string &key)
{
  sortify_words(s, s + len, 0, key);
}

void sortify_title(const char *s, int len, string &key)
{
  const char *end = s + len;
  for (; s < end && (*s == ' ' || *s == '\n'); s++) 
    ;
  const char *ptr = s;
  for (;;) {
    const char *token_start = ptr;
    if (!get_token(&ptr, end))
      break;
    if (ptr - token_start == 1
	&& (*token_start == ' ' || *token_start == '\n'))
      break;
  }
  if (ptr < end) {
    int first_word_len = ptr - s - 1;
    const char *ae = articles.contents() + articles.length();
    for (const char *a = articles.contents();
	 a < ae;
	 a = strchr(a, '\0') + 1)
      if (first_word_len == strlen(a)) {
	for (int j = 0; j < first_word_len; j++)
	  if (a[j] != cmlower(s[j]))
	    break;
	if (j >= first_word_len) {
	  s = ptr;
	  for (; s < end && (*s == ' ' || *s == '\n'); s++)
	    ;
	  break;
	}
      }
  }
  sortify_words(s, end, 0, key);
}

void sortify_name(const char *s, int len, string &key)
{
  const char *last_name_end;
  const char *last_name = find_last_name(s, s + len, &last_name_end);
  sortify_word(last_name, last_name_end, key);
  key += SORT_SUB_SUB_SEP;
  if (last_name > s)
    sortify_words(s, last_name, ".", key);
  key += SORT_SUB_SUB_SEP;
  if (last_name_end < s + len)
    sortify_words(last_name_end, s + len, ".,", key);
}

void sortify_date(const char *s, int len, string &key)
{
  const char *year_end;
  const char *year_start = find_year(s, s + len, &year_end);
  if (!year_start) {
    // Things without years are often `forthcoming', so it makes sense
    // that they sort after things with explicit years.
    key += 'A';
    sortify_words(s, s + len, 0, key);
    return;
  }
  int n = year_end - year_start;
  while (n < 4) {
    key += '0';
    n++;
  }
  while (year_start < year_end)
    key += *year_start++;
  int m = find_month(s, s + len);
  if (m < 0)
    return;
  key += 'A' + m;
  const char *day_end;
  const char *day_start = find_day(s, s + len, &day_end);
  if (!day_start)
    return;
  if (day_end - day_start == 1)
    key += '0';
  while (day_start < day_end)
    key += *day_start++;
}

// SORT_{SUB,SUB_SUB}_SEP can creep in from use of @ in label specification.

void sortify_label(const char *s, int len, string &key)
{
  const char *end = s + len;
  for (;;) {
    for (const char *ptr = s;
	 ptr < end && *ptr != SORT_SUB_SEP && *ptr != SORT_SUB_SUB_SEP;
	 ptr++)
      ;
    if (ptr > s)
      sortify_words(s, ptr, 0, key);
    s = ptr;
    if (s >= end)
      break;
    key += *s++;
  }
}

void reference::compute_sort_key()
{
  if (sort_fields.length() == 0)
    return;
  sort_fields += '\0';
  const char *sf = sort_fields.contents();
  while (*sf != '\0') {
    if (sf > sort_fields)
      sort_key += SORT_SEP;
    char f = *sf++;
    int n = 1;
    if (*sf == '+') {
      n = INT_MAX;
      sf++;
    }
    else if (csdigit(*sf)) {
      char *ptr;
      long l = strtol(sf, &ptr, 10);
      if (l == 0 && ptr == sf)
	;
      else {
	sf = ptr;
	if (l < 0) {
	  n = 1;
	}
	else {
	  n = int(l);
	}
      }
    }
    if (f == '.')
      sortify_label(label.contents(), label.length(), sort_key);
    else if (f == AUTHOR_FIELDS[0])
      sortify_authors(n, sort_key);
    else
      sortify_field(f, n, sort_key);
  }
  sort_fields.set_length(sort_fields.length() - 1);
}

void reference::sortify_authors(int n, string &result) const
{
  for (const char *p = AUTHOR_FIELDS; *p != '\0'; p++)
    if (contains_field(*p)) {
      sortify_field(*p, n, result);
      return;
    }
  sortify_field(AUTHOR_FIELDS[0], n, result);
}

void reference::canonicalize_authors(string &result) const
{
  int len = result.length();
  sortify_authors(INT_MAX, result);
  if (result.length() > len)
    result += SORT_SUB_SEP;
}

void reference::sortify_field(unsigned char f, int n, string &result) const
{
  typedef void (*sortify_t)(const char *, int, string &);
  sortify_t sortifier = sortify_other;
  switch (f) {
  case 'A':
  case 'E':
    sortifier = sortify_name;
    break;
  case 'D':
    sortifier = sortify_date;
    break;
  case 'B':
  case 'J':
  case 'T':
    sortifier = sortify_title;
    break;
  }
  int fi = field_index[(unsigned char)f];
  if (fi != NULL_FIELD_INDEX) {
    string &str = field[fi];
    const char *start = str.contents();
    const char *end = start + str.length();
    for (int i = 0; i < n && start < end; i++) {
      const char *p = start;
      while (start < end && *start != FIELD_SEPARATOR)
	start++;
      if (i > 0)
	result += SORT_SUB_SEP;
      (*sortifier)(p, start - p, result);
      if (start < end)
	start++;
    }
  }
}

int compare_reference(const reference &r1, const reference &r2)
{
  assert(r1.no >= 0);
  assert(r2.no >= 0);
  const char *s1 = r1.sort_key.contents();
  int n1 = r1.sort_key.length();
  const char *s2 = r2.sort_key.contents();
  int n2 = r2.sort_key.length();
  for (; n1 > 0 && n2 > 0; --n1, --n2, ++s1, ++s2)
    if (*s1 != *s2)
      return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
  if (n2 > 0)
    return -1;
  if (n1 > 0)
    return 1;
  return r1.no - r2.no;
}

int same_reference(const reference &r1, const reference &r2)
{
  if (!r1.rid.is_null() && r1.rid == r2.rid)
    return 1;
  if (r1.h != r2.h)
    return 0;
  if (r1.nfields != r2.nfields)
    return 0;
  int i = 0; 
  for (i = 0; i < 256; i++)
    if (r1.field_index != r2.field_index)
      return 0;
  for (i = 0; i < r1.nfields; i++)
    if (r1.field[i] != r2.field[i])
      return 0;
  return 1;
}

const char *find_last_name(const char *start, const char *end,
			   const char **endp)
{
  const char *ptr = start;
  const char *last_word = start;
  for (;;) {
    const char *token_start = ptr;
    if (!get_token(&ptr, end))
      break;
    if (ptr - token_start == 1) {
      if (*token_start == ',') {
	*endp = token_start;
	return last_word;
      }
      else if (*token_start == ' ' || *token_start == '\n') {
	if (ptr < end && *ptr != ' ' && *ptr != '\n')
	  last_word = ptr;
      }
    }
  }
  *endp = end;
  return last_word;
}

void abbreviate_name(const char *ptr, const char *end, string &result)
{
  const char *last_name_end;
  const char *last_name_start = find_last_name(ptr, end, &last_name_end);
  int need_period = 0;
  for (;;) {
    const char *token_start = ptr;
    if (!get_token(&ptr, last_name_start))
      break;
    const token_info *ti = lookup_token(token_start, ptr);
    if (need_period) {
      if ((ptr - token_start == 1 && *token_start == ' ')
	  || (ptr - token_start == 2 && token_start[0] == '\\'
	      && token_start[1] == ' '))
	continue;
      if (ti->is_upper())
	result += period_before_initial;
      else
	result += period_before_other;
      need_period = 0;
    }
    result.append(token_start, ptr - token_start);
    if (ti->is_upper()) {
      const char *lower_ptr = ptr;
      int first_token = 1;
      for (;;) {
	token_start = ptr;
	if (!get_token(&ptr, last_name_start))
	  break;
	if ((ptr - token_start == 1 && *token_start == ' ')
	    || (ptr - token_start == 2 && token_start[0] == '\\'
		&& token_start[1] == ' '))
	  break;
	ti = lookup_token(token_start, ptr);
	if (ti->is_hyphen()) {
	  const char *ptr1 = ptr;
	  if (get_token(&ptr1, last_name_start)) {
	    ti = lookup_token(ptr, ptr1);
	    if (ti->is_upper()) {
	      result += period_before_hyphen;
	      result.append(token_start, ptr1 - token_start);
	      ptr = ptr1;
	    }
	  }
	}
	else if (ti->is_upper()) {
	  // MacDougal -> MacD.
	  result.append(lower_ptr, ptr - lower_ptr);
	  lower_ptr = ptr;
	  first_token = 1;
	}
	else if (first_token && ti->is_accent()) {
	  result.append(token_start, ptr - token_start);
	  lower_ptr = ptr;
	}
	first_token = 0;
      }
      need_period = 1;
    }
  }
  if (need_period)
    result += period_before_last_name;
  result.append(last_name_start, end - last_name_start);
}

static void abbreviate_names(string &result)
{
  string str;
  str.move(result);
  const char *ptr = str.contents();
  const char *end = ptr + str.length();
  while (ptr < end) {
    const char *name_end = (char *)memchr(ptr, FIELD_SEPARATOR, end - ptr);
    if (name_end == 0)
      name_end = end;
    abbreviate_name(ptr, name_end, result);
    if (name_end >= end)
      break;
    ptr = name_end + 1;
    result += FIELD_SEPARATOR;
  }
}

void reverse_name(const char *ptr, const char *name_end, string &result)
{
  const char *last_name_end;
  const char *last_name_start = find_last_name(ptr, name_end, &last_name_end);
  result.append(last_name_start, last_name_end - last_name_start);
  while (last_name_start > ptr
	 && (last_name_start[-1] == ' ' || last_name_start[-1] == '\n'))
    last_name_start--;
  if (last_name_start > ptr) {
    result += ", ";
    result.append(ptr, last_name_start - ptr);
  }
  if (last_name_end < name_end)
    result.append(last_name_end, name_end - last_name_end);
}

void reverse_names(string &result, int n)
{
  if (n <= 0)
    return;
  string str;
  str.move(result);
  const char *ptr = str.contents();
  const char *end = ptr + str.length();
  while (ptr < end) {
    if (--n < 0) {
      result.append(ptr, end - ptr);
      break;
    }
    const char *name_end = (char *)memchr(ptr, FIELD_SEPARATOR, end - ptr);
    if (name_end == 0)
      name_end = end;
    reverse_name(ptr, name_end, result);
    if (name_end >= end)
      break;
    ptr = name_end + 1;
    result += FIELD_SEPARATOR;
  }
}

// Return number of field separators.

int join_fields(string &f)
{
  const char *ptr = f.contents();
  int len = f.length();
  int nfield_seps = 0;
  for (int j = 0; j < len; j++)
    if (ptr[j] == FIELD_SEPARATOR)
      nfield_seps++;
  if (nfield_seps == 0)
    return 0;
  string temp;
  int field_seps_left = nfield_seps;
  for (j = 0; j < len; j++) {
    if (ptr[j] == FIELD_SEPARATOR) {
      if (nfield_seps == 1)
	temp += join_authors_exactly_two;
      else if (--field_seps_left == 0)
	temp += join_authors_last_two;
      else
	temp += join_authors_default;
    }
    else
      temp += ptr[j];
  }
  f = temp;
  return nfield_seps;
}

void uppercase(const char *start, const char *end, string &result)
{
  for (;;) {
    const char *token_start = start;
    if (!get_token(&start, end))
      break;
    const token_info *ti = lookup_token(token_start, start);
    ti->upper_case(token_start, start, result);
  }
}

void lowercase(const char *start, const char *end, string &result)
{
  for (;;) {
    const char *token_start = start;
    if (!get_token(&start, end))
      break;
    const token_info *ti = lookup_token(token_start, start);
    ti->lower_case(token_start, start, result);
  }
}

void capitalize(const char *ptr, const char *end, string &result)
{
  int in_small_point_size = 0;
  for (;;) {
    const char *start = ptr;
    if (!get_token(&ptr, end))
      break;
    const token_info *ti = lookup_token(start, ptr);
    const char *char_end = ptr;
    int is_lower = ti->is_lower();
    if ((is_lower || ti->is_upper()) && get_token(&ptr, end)) {
      const token_info *ti2 = lookup_token(char_end, ptr);
      if (!ti2->is_accent())
	ptr = char_end;
    }
    if (is_lower) {
      if (!in_small_point_size) {
	result += "\\s-2";
	in_small_point_size = 1;
      }
      ti->upper_case(start, char_end, result);
      result.append(char_end, ptr - char_end);
    }
    else {
      if (in_small_point_size) {
	result += "\\s+2";
	in_small_point_size = 0;
      }
      result.append(start, ptr - start);
    }
  }
  if (in_small_point_size)
    result += "\\s+2";
}

void capitalize_field(string &str)
{
  string temp;
  capitalize(str.contents(), str.contents() + str.length(), temp);
  str.move(temp);
}

int is_terminated(const char *ptr, const char *end)
{
  const char *last_token = end;
  for (;;) {
    const char *p = ptr;
    if (!get_token(&ptr, end))
      break;
    last_token = p;
  }
  return end - last_token == 1
    && (*last_token == '.' || *last_token == '!' || *last_token == '?');
}

void reference::output(FILE *fp)
{
  fputs(".]-\n", fp);
  for (int i = 0; i < 256; i++)
    if (field_index[i] != NULL_FIELD_INDEX && i != annotation_field) {
      string &f = field[field_index[i]];
      if (!csdigit(i)) {
	int j = reverse_fields.search(i);
	if (j >= 0) {
	  int n;
	  int len = reverse_fields.length();
	  if (++j < len && csdigit(reverse_fields[j])) {
	    n = reverse_fields[j] - '0';
	    for (++j; j < len && csdigit(reverse_fields[j]); j++)
	      // should check for overflow
	      n = n*10 + reverse_fields[j] - '0';
	  }
	  else 
	    n = INT_MAX;
	  reverse_names(f, n);
	}
      }
      int is_multiple = join_fields(f) > 0;
      if (capitalize_fields.search(i) >= 0)
	capitalize_field(f);
      if (memchr(f.contents(), '\n', f.length()) == 0) {
	fprintf(fp, ".ds [%c ", i);
	if (f[0] == ' ' || f[0] == '\\' || f[0] == '"')
	  putc('"', fp);
	put_string(f, fp);
	putc('\n', fp);
      }
      else {
	fprintf(fp, ".de [%c\n", i);
	put_string(f, fp);
	fputs("..\n", fp);
      }
      if (i == 'P') {
	int multiple_pages = 0;
	if (f.length() > 0 && memchr(f.contents(), '-', f.length()) != 0)
	  multiple_pages = 1;
	fprintf(fp, ".nr [P %d\n", multiple_pages);
      }
      else if (i == 'E')
	fprintf(fp, ".nr [E %d\n", is_multiple);
    }
  for (const char *p = "TAO"; *p; p++) {
    int fi = field_index[(unsigned char)*p];
    if (fi != NULL_FIELD_INDEX) {
      string &f = field[fi];
      fprintf(fp, ".nr [%c %d\n", *p,
	      is_terminated(f.contents(), f.contents() + f.length()));
    }
  }
  int t = classify();
  fprintf(fp, ".][ %d %s\n", t, reference_types[t]);
  if (annotation_macro.length() > 0 && annotation_field >= 0
      && field_index[annotation_field] != NULL_FIELD_INDEX) {
    putc('.', fp);
    put_string(annotation_macro, fp);
    putc('\n', fp);
    put_string(field[field_index[annotation_field]], fp);
  }
}

void reference::print_sort_key_comment(FILE *fp)
{
  fputs(".\\\"", fp);
  put_string(sort_key, fp);
  putc('\n', fp);
}

const char *find_year(const char *start, const char *end, const char **endp)
{
  for (;;) {
    while (start < end && !csdigit(*start))
      start++;
    const char *ptr = start;
    if (start == end)
      break;
    while (ptr < end && csdigit(*ptr))
      ptr++;
    if (ptr - start == 4 || ptr - start == 3
	|| (ptr - start == 2
	    && (start[0] >= '4' || (start[0] == '3' && start[1] >= '2')))) {
      *endp = ptr;
      return start;
    }
    start = ptr;
  }
  return 0;
}

static const char *find_day(const char *start, const char *end,
			    const char **endp)
{
  for (;;) {
    while (start < end && !csdigit(*start))
      start++;
    const char *ptr = start;
    if (start == end)
      break;
    while (ptr < end && csdigit(*ptr))
      ptr++;
    if ((ptr - start == 1 && start[0] != '0')
	|| (ptr - start == 2 &&
	    (start[0] == '1'
	     || start[0] == '2'
	     || (start[0] == '3' && start[1] <= '1')
	     || (start[0] == '0' && start[1] != '0')))) {
      *endp = ptr;
      return start;
    }
    start = ptr;
  }
  return 0;
}

static int find_month(const char *start, const char *end)
{
  static const char *months[] = {
    "january",
    "february",
    "march",
    "april",
    "may",
    "june",
    "july",
    "august",
    "september",
    "october",
    "november",
    "december",
  };
  for (;;) {
    while (start < end && !csalpha(*start))
      start++;
    const char *ptr = start;
    if (start == end)
      break;
    while (ptr < end && csalpha(*ptr))
      ptr++;
    if (ptr - start >= 3) {
      for (int i = 0; i < sizeof(months)/sizeof(months[0]); i++) {
	const char *q = months[i];
	const char *p = start;
	for (; p < ptr; p++, q++)
	  if (cmlower(*p) != *q)
	    break;
	if (p >= ptr)
	  return i;
      }
    }
    start = ptr;
  }
  return -1;
}

int reference::contains_field(char c) const
{
  return field_index[(unsigned char)c] != NULL_FIELD_INDEX;
}

int reference::classify()
{
  if (contains_field('J'))
    return JOURNAL_ARTICLE;
  if (contains_field('B'))
    return ARTICLE_IN_BOOK;
  if (contains_field('G'))
    return TECH_REPORT;
  if (contains_field('R'))
    return TECH_REPORT;
  if (contains_field('I'))
    return BOOK;
  if (contains_field('M'))
    return BELL_TM;
  return OTHER;
}

const char *reference::get_year(const char **endp) const
{
  if (field_index['D'] != NULL_FIELD_INDEX) {
    string &date = field[field_index['D']];
    const char *start = date.contents();
    const char *end = start + date.length();
    return find_year(start, end, endp);
  }
  else
    return 0;
}

const char *reference::get_field(unsigned char c, const char **endp) const
{
  if (field_index[c] != NULL_FIELD_INDEX) {
    string &f = field[field_index[c]];
    const char *start = f.contents();
    *endp = start + f.length();
    return start;
  }
  else
    return 0;
}

const char *reference::get_date(const char **endp) const
{
  return get_field('D', endp);
}

const char *nth_field(int i, const char *start, const char **endp)
{
  while (--i >= 0) {
    start = (char *)memchr(start, FIELD_SEPARATOR, *endp - start);
    if (!start)
      return 0;
    start++;
  }
  const char *e = (char *)memchr(start, FIELD_SEPARATOR, *endp - start);
  if (e)
    *endp = e;
  return start;
}

const char *reference::get_author(int i, const char **endp) const
{
  for (const char *f = AUTHOR_FIELDS; *f != '\0'; f++) {
    const char *start = get_field(*f, endp);
    if (start) {
      if (strchr(MULTI_FIELD_NAMES, *f) != 0)
	return nth_field(i, start, endp);
      else if (i == 0)
	return start;
      else
	return 0;
    }
  }
  return 0;
}

const char *reference::get_author_last_name(int i, const char **endp) const
{
  for (const char *f = AUTHOR_FIELDS; *f != '\0'; f++) {
    const char *start = get_field(*f, endp);
    if (start) {
      if (strchr(MULTI_FIELD_NAMES, *f) != 0) {
	start = nth_field(i, start, endp);
	if (!start)
	  return 0;
      }
      if (*f == 'A')
	return find_last_name(start, *endp, endp);
      else
	return start;
    }
  }
  return 0;
}

void reference::set_date(string &d)
{
  if (d.length() == 0)
    delete_field('D');
  else
    insert_field('D', d);
}

int same_year(const reference &r1, const reference &r2)
{
  const char *ye1;
  const char *ys1 = r1.get_year(&ye1);
  const char *ye2;
  const char *ys2 = r2.get_year(&ye2);
  if (ys1 == 0) {
    if (ys2 == 0)
      return same_date(r1, r2);
    else
      return 0;
  }
  else if (ys2 == 0)
    return 0;
  else if (ye1 - ys1 != ye2 - ys2)
    return 0;
  else
    return memcmp(ys1, ys2, ye1 - ys1) == 0;
}

int same_date(const reference &r1, const reference &r2)
{
  const char *e1;
  const char *s1 = r1.get_date(&e1);
  const char *e2;
  const char *s2 = r2.get_date(&e2);
  if (s1 == 0)
    return s2 == 0;
  else if (s2 == 0)
    return 0;
  else if (e1 - s1 != e2 - s2)
    return 0;
  else
    return memcmp(s1, s2, e1 - s1) == 0;
}

const char *reference::get_sort_field(int i, int si, int ssi,
				      const char **endp) const
{
  const char *start = sort_key.contents();
  const char *end = start + sort_key.length();
  if (i < 0) {
    *endp = end;
    return start;
  }
  while (--i >= 0) {
    start = (char *)memchr(start, SORT_SEP, end - start);
    if (!start)
      return 0;
    start++;
  }
  const char *e = (char *)memchr(start, SORT_SEP, end - start);
  if (e)
    end = e;
  if (si < 0) {
    *endp = end;
    return start;
  }
  while (--si >= 0) {
    start = (char *)memchr(start, SORT_SUB_SEP, end - start);
    if (!start)
      return 0;
    start++;
  }
  e = (char *)memchr(start, SORT_SUB_SEP, end - start);
  if (e)
    end = e;
  if (ssi < 0) {
    *endp = end;
    return start;
  }
  while (--ssi >= 0) {
    start = (char *)memchr(start, SORT_SUB_SUB_SEP, end - start);
    if (!start)
      return 0;
    start++;
  }
  e = (char *)memchr(start, SORT_SUB_SUB_SEP, end - start);
  if (e)
    end = e;
  *endp = end;
  return start;
}

