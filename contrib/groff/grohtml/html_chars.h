// -*- C++ -*-
/* Copyright (C) 2000 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote output.cc
 *  but it owes a huge amount of ideas and raw code from
 *  James Clark (jjc@jclark.com) grops/ps.cc.
 *
 *  html_chars.h
 *
 *  provides a diacritical character combination table for html
 */



struct diacritical_desc {
  char *mark;
  char *second_troff_char;
  char  translation;
};


static struct diacritical_desc diacritical_table[] = {
  { "ad"   , "aeiouyAEIOU" , ':'    , },      /* */
  { "ac"   , "cC"          , ','    , },      /* cedilla */
  { "aa"   , "aeiouyAEIOU" , '\''   , },      /* acute */
  { NULL   , NULL          , (char)0, },
};
