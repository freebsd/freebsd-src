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

enum token_type {
  TOKEN_OTHER,
  TOKEN_UPPER,
  TOKEN_LOWER,
  TOKEN_ACCENT,
  TOKEN_PUNCT,
  TOKEN_HYPHEN,
  TOKEN_RANGE_SEP
};

class token_info {
private:
  token_type type;
  const char *sort_key;
  const char *other_case;
public:
  token_info();
  void set(token_type, const char *sk = 0, const char *oc = 0);
  void lower_case(const char *start, const char *end, string &result) const;
  void upper_case(const char *start, const char *end, string &result) const;
  void sortify(const char *start, const char *end, string &result) const;
  int sortify_non_empty(const char *start, const char *end) const;
  int is_upper() const;
  int is_lower() const;
  int is_accent() const;
  int is_other() const;
  int is_punct() const;
  int is_hyphen() const;
  int is_range_sep() const;
};

inline int token_info::is_upper() const
{
  return type == TOKEN_UPPER;
}

inline int token_info::is_lower() const
{
  return type == TOKEN_LOWER;
}

inline int token_info::is_accent() const
{
  return type == TOKEN_ACCENT;
}

inline int token_info::is_other() const
{
  return type == TOKEN_OTHER;
}

inline int token_info::is_punct() const
{
  return type == TOKEN_PUNCT;
}

inline int token_info::is_hyphen() const
{
  return type == TOKEN_HYPHEN;
}

inline int token_info::is_range_sep() const
{
  return type == TOKEN_RANGE_SEP;
}

int get_token(const char **ptr, const char *end);
const token_info *lookup_token(const char *start, const char *end);
