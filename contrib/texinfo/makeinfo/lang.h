/* lang.h -- declarations for language codes etc.
   $Id: lang.h,v 1.7 2001/09/11 18:04:29 karl Exp $

   Copyright (C) 1999, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Originally written by Karl Heinz Marbaise <kama@hippo.fido.de>.  */

#ifndef LANG_H
#define LANG_H

/* The language code which can be changed through @documentlanguage
 * Actually we don't currently support this (may be in the future) ;-)
 * These code are the ISO-639 two letter codes.
 */
typedef enum
{
  aa,  ab,  af,  am,  ar,  as,  ay,  az,
  ba,  be,  bg,  bh,  bi,  bn,  bo,  br,
  ca,  co,  cs,  cy,
  da,  de,  dz,
  el,  en,  eo,  es,  et,  eu,
  fa,  fi,  fj,  fo,  fr,  fy,
  ga,  gd,  gl,  gn,  gu,
  ha,  he,  hi,  hr,  hu,  hy,
  ia,  id,  ie,  ik,  is,  it,  iu,
  ja,  jw,
  ka,  kk,  kl,  km,  kn,  ko,  ks,  ku,  ky,
  la,  ln,  lo,  lt,  lv,
  mg,  mi,  mk,  ml,  mn,  mo,  mr,  ms,  mt,  my,
  na,  ne,  nl,  no,
  oc,  om,  or,
  pa,  pl,  ps,  pt,
  qu,
  rm,  rn,  ro,  ru,  rw,
  sa,  sd,  sg,  sh,  si,  sk,  sl,  sm,  sn,  so,  sq,  sr,  ss,  st,  su,  sv,  sw,
  ta,  te,  tg,  th,  ti,  tk,  tl,  tn,  to,  tr,  ts,  tt,  tw,
  ug,  uk,  ur,  uz,
  vi,  vo,
  wo,
  xh,
  yi,  yo,
  za,  zh,  zu,
  last_language_code
} language_code_type;

/* The current language code.  */
extern language_code_type language_code;


/* Information for each language.  */
typedef struct
{
  language_code_type lc; /* language code as enum type */
  char *abbrev;          /* two letter language code */
  char *desc;            /* full name for language code */
} language_type;

extern language_type language_table[];



/* The document encoding. This is usefull if we working e.g.
 * with german Texinfo so we can produce correct german umlaut
 * while creating output (--no-headers ASCII like).
 */
typedef enum {
    no_encoding,
    ISO_8859_1,    /* default for en, de, */
    ISO_8859_2,    /* actualy not supported like the rest below */
    ISO_8859_3,
    ISO_8859_4,
    ISO_8859_5,
    ISO_8859_6,
    ISO_8859_7,
    ISO_8859_8,
    ISO_8859_9,
    ISO_8859_10,
    ISO_8859_11,
    ISO_8859_12,
    ISO_8859_13,
    ISO_8859_14,
    ISO_8859_15,
    last_encoding_code
} encoding_code_type;

/* The current document encoding, or null if not set.  */
extern encoding_code_type document_encoding_code;


/* Maps an HTML abbreviation to ISO and Unicode codes for a given code.  */

typedef unsigned short int unicode_t; /* should be 16 bits */
typedef unsigned char byte_t;

typedef struct
{
  char *html;        /* HTML equivalent like umlaut auml => &auml; */
  byte_t bytecode;   /* 8-Bit Code (ISO 8859-1,...) */
  unicode_t unicode; /* Unicode in U+ convention */
} iso_map_type;

/* Information about the document encoding. */
typedef struct
{
  encoding_code_type ec; /* document encoding type (see above enum) */
  char *ecname;          /* encoding name like ISO-8859-1 */
  iso_map_type *isotab;  /* address of ISO translation table */
} encoding_type;

/* Table with all the encoding codes that we recognize.  */
extern encoding_type encoding_table[];


/* The commands.  */
extern void cm_documentlanguage (), cm_documentencoding ();

/* Accents, other non-English characters.  */
void cm_accent (), cm_special_char (), cm_dotless ();

extern void cm_accent_umlaut (), cm_accent_acute (), cm_accent_cedilla (),
  cm_accent_hat (), cm_accent_grave (), cm_accent_tilde ();

#endif /* not LANG_H */
