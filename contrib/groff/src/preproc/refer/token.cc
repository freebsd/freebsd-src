// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2001 Free Software Foundation, Inc.
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

#include "refer.h"
#include "token.h"

#define TOKEN_TABLE_SIZE 1009
// I believe in Icelandic thorn sorts after z.
#define THORN_SORT_KEY "{"

struct token_table_entry {
  const char *tok;
  token_info ti;
  token_table_entry();
};

token_table_entry token_table[TOKEN_TABLE_SIZE];
int ntokens = 0;

static void skip_name(const char **ptr, const char *end)
{
  if (*ptr < end) {
    switch (*(*ptr)++) {
    case '(':
      if (*ptr < end) {
	*ptr += 1;
	if (*ptr < end)
	  *ptr += 1;
      }
      break;
    case '[':
      while (*ptr < end)
	if (*(*ptr)++ == ']')
	  break;
      break;
    }
  }
}

int get_token(const char **ptr, const char *end)
{
  if (*ptr >= end)
    return 0;
  char c = *(*ptr)++;
  if (c == '\\' && *ptr < end) {
    switch (**ptr) {
    default:
      *ptr += 1;
      break;
    case '(':
    case '[':
      skip_name(ptr, end);
      break;
    case '*':
    case 'f':
      *ptr += 1;
      skip_name(ptr, end);
      break;
    }
  }
  return 1;
}

token_info::token_info()
: type(TOKEN_OTHER), sort_key(0), other_case(0)
{
}

void token_info::set(token_type t, const char *sk, const char *oc)
{
  assert(oc == 0 || t == TOKEN_UPPER || t == TOKEN_LOWER);
  type = t;
  sort_key = sk;
  other_case = oc;
}

void token_info::sortify(const char *start, const char *end, string &result)
     const
{
  if (sort_key)
    result += sort_key;
  else if (type == TOKEN_UPPER || type == TOKEN_LOWER) {
    for (; start < end; start++)
      if (csalpha(*start))
	result += cmlower(*start);
  }
}

int token_info::sortify_non_empty(const char *start, const char *end) const
{
  if (sort_key)
    return *sort_key != '\0';
  if (type != TOKEN_UPPER && type != TOKEN_LOWER)
    return 0;
  for (; start < end; start++)
    if (csalpha(*start))
      return 1;
  return 0;
}


void token_info::lower_case(const char *start, const char *end,
			    string &result) const
{
  if (type != TOKEN_UPPER) {
    while (start < end)
      result += *start++;
  }
  else if (other_case)
    result += other_case;
  else {
    while (start < end)
      result += cmlower(*start++);
  }
}

void token_info::upper_case(const char *start, const char *end,
			    string &result) const
{
  if (type != TOKEN_LOWER) {
    while (start < end)
      result += *start++;
  }
  else if (other_case)
    result += other_case;
  else {
    while (start < end)
      result += cmupper(*start++);
  }
}

token_table_entry::token_table_entry()
: tok(0)
{
}

static void store_token(const char *tok, token_type typ,
			const char *sk = 0, const char *oc = 0)
{
  unsigned n = hash_string(tok, strlen(tok)) % TOKEN_TABLE_SIZE;
  for (;;) {
    if (token_table[n].tok == 0) {
      if (++ntokens == TOKEN_TABLE_SIZE)
	assert(0);
      token_table[n].tok = tok;
      break;
    }
    if (strcmp(tok, token_table[n].tok) == 0)
      break;
    if (n == 0)
      n = TOKEN_TABLE_SIZE - 1;
    else
      --n;
  }
  token_table[n].ti.set(typ, sk, oc);
}


token_info default_token_info;

const token_info *lookup_token(const char *start, const char *end)
{
  unsigned n = hash_string(start, end - start) % TOKEN_TABLE_SIZE;
  for (;;) {
    if (token_table[n].tok == 0)
      break;
    if (strlen(token_table[n].tok) == size_t(end - start)
	&& memcmp(token_table[n].tok, start, end - start) == 0)
      return &(token_table[n].ti);
    if (n == 0)
      n = TOKEN_TABLE_SIZE - 1;
    else
      --n;
  }
  return &default_token_info;
}

static void init_ascii()
{
  const char *p;
  for (p = "abcdefghijklmnopqrstuvwxyz"; *p; p++) {
    char buf[2];
    buf[0] = *p;
    buf[1] = '\0';
    store_token(strsave(buf), TOKEN_LOWER);
    buf[0] = cmupper(buf[0]);
    store_token(strsave(buf), TOKEN_UPPER);
  }
  for (p = "0123456789"; *p; p++) {
    char buf[2];
    buf[0] = *p;
    buf[1] = '\0';
    const char *s = strsave(buf);
    store_token(s, TOKEN_OTHER, s);
  }
  for (p = ".,:;?!"; *p; p++) {
    char buf[2];
    buf[0] = *p;
    buf[1] = '\0';
    store_token(strsave(buf), TOKEN_PUNCT);
  }
  store_token("-", TOKEN_HYPHEN);
}

static void store_letter(const char *lower, const char *upper,
		  const char *sort_key = 0)
{
  store_token(lower, TOKEN_LOWER, sort_key, upper);
  store_token(upper, TOKEN_UPPER, sort_key, lower);
}

static void init_letter(unsigned char uc_code, unsigned char lc_code,
		 const char *sort_key)
{
  char lbuf[2];
  lbuf[0] = lc_code;
  lbuf[1] = 0;
  char ubuf[2];
  ubuf[0] = uc_code;
  ubuf[1] = 0;
  store_letter(strsave(lbuf), strsave(ubuf), sort_key);
}

static void init_latin1()
{
  init_letter(0xc0, 0xe0, "a");
  init_letter(0xc1, 0xe1, "a");
  init_letter(0xc2, 0xe2, "a");
  init_letter(0xc3, 0xe3, "a");
  init_letter(0xc4, 0xe4, "a");
  init_letter(0xc5, 0xe5, "a");
  init_letter(0xc6, 0xe6, "ae");
  init_letter(0xc7, 0xe7, "c");
  init_letter(0xc8, 0xe8, "e");
  init_letter(0xc9, 0xe9, "e");
  init_letter(0xca, 0xea, "e");
  init_letter(0xcb, 0xeb, "e");
  init_letter(0xcc, 0xec, "i");
  init_letter(0xcd, 0xed, "i");
  init_letter(0xce, 0xee, "i");
  init_letter(0xcf, 0xef, "i");

  init_letter(0xd0, 0xf0, "d");
  init_letter(0xd1, 0xf1, "n");
  init_letter(0xd2, 0xf2, "o");
  init_letter(0xd3, 0xf3, "o");
  init_letter(0xd4, 0xf4, "o");
  init_letter(0xd5, 0xf5, "o");
  init_letter(0xd6, 0xf6, "o");
  init_letter(0xd8, 0xf8, "o");
  init_letter(0xd9, 0xf9, "u");
  init_letter(0xda, 0xfa, "u");
  init_letter(0xdb, 0xfb, "u");
  init_letter(0xdc, 0xfc, "u");
  init_letter(0xdd, 0xfd, "y");
  init_letter(0xde, 0xfe, THORN_SORT_KEY);

  store_token("\337", TOKEN_LOWER, "ss", "SS");
  store_token("\377", TOKEN_LOWER, "y", "Y");
}

static void init_two_char_letter(char l1, char l2, char u1, char u2,
				 const char *sk = 0)
{
  char buf[6];
  buf[0] = '\\';
  buf[1] = '(';
  buf[2] = l1;
  buf[3] = l2;
  buf[4] = '\0';
  const char *p = strsave(buf);
  buf[2] = u1;
  buf[3] = u2;
  store_letter(p, strsave(buf), sk);
  buf[1] = '[';
  buf[4] = ']';
  buf[5] = '\0';
  p = strsave(buf);
  buf[2] = l1;
  buf[3] = l2;
  store_letter(strsave(buf), p, sk);
  
}

static void init_special_chars()
{
  const char *p;
  for (p = "':^`~"; *p; p++)
    for (const char *q = "aeiouy"; *q; q++) {
      // Use a variable to work around bug in gcc 2.0
      char c = cmupper(*q);
      init_two_char_letter(*p, *q, *p, c);
    }
  for (p = "/l/o~n,coeaeij"; *p; p += 2) {
    // Use variables to work around bug in gcc 2.0
    char c0 = cmupper(p[0]);
    char c1 = cmupper(p[1]);
    init_two_char_letter(p[0], p[1], c0, c1);
  }
  init_two_char_letter('v', 's', 'v', 'S', "s");
  init_two_char_letter('v', 'z', 'v', 'Z', "z");
  init_two_char_letter('o', 'a', 'o', 'A', "a");
  init_two_char_letter('T', 'p', 'T', 'P', THORN_SORT_KEY);
  init_two_char_letter('-', 'd', '-', 'D');
  
  store_token("\\(ss", TOKEN_LOWER, 0, "SS");
  store_token("\\[ss]", TOKEN_LOWER, 0, "SS");

  store_token("\\(Sd", TOKEN_LOWER, "d", "\\(-D");
  store_token("\\[Sd]", TOKEN_LOWER, "d", "\\[-D]");
  store_token("\\(hy", TOKEN_HYPHEN);
  store_token("\\[hy]", TOKEN_HYPHEN);
  store_token("\\(en", TOKEN_RANGE_SEP);
  store_token("\\[en]", TOKEN_RANGE_SEP);
}

static void init_strings()
{
  char buf[6];
  buf[0] = '\\';
  buf[1] = '*';
  for (const char *p = "'`^^,:~v_o./;"; *p; p++) {
    buf[2] = *p;
    buf[3] = '\0';
    store_token(strsave(buf), TOKEN_ACCENT);
    buf[2] = '[';
    buf[3] = *p;
    buf[4] = ']';
    buf[5] = '\0';
    store_token(strsave(buf), TOKEN_ACCENT);
  }

  // -ms special letters
  store_letter("\\*(th", "\\*(Th", THORN_SORT_KEY);
  store_letter("\\*[th]", "\\*[Th]", THORN_SORT_KEY);
  store_letter("\\*(d-", "\\*(D-");
  store_letter("\\*[d-]", "\\*[D-]");
  store_letter("\\*(ae", "\\*(Ae", "ae");
  store_letter("\\*[ae]", "\\*[Ae]", "ae");
  store_letter("\\*(oe", "\\*(Oe", "oe");
  store_letter("\\*[oe]", "\\*[Oe]", "oe");

  store_token("\\*3", TOKEN_LOWER, "y", "Y");
  store_token("\\*8", TOKEN_LOWER, "ss", "SS");
  store_token("\\*q", TOKEN_LOWER, "o", "O");
}

struct token_initer {
  token_initer();
};

static token_initer the_token_initer;

token_initer::token_initer()
{
  init_ascii();
  init_latin1();
  init_special_chars();
  init_strings();
  default_token_info.set(TOKEN_OTHER);
}
