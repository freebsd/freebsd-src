/* lang.h -- declarations for language codes etc.
   $Id: lang.h,v 1.6 1999/03/22 20:07:34 karl Exp $

   Copyright (C) 1999 Free Software Foundation, Inc.

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

   Written by Karl Heinz Marbaise <kama@hippo.fido.de>.  */

#ifndef LANG_H
#define LANG_H

/* The langauge code which can be changed through @documentlanguage
 * Actualy Info does not support this (may be in the future) ;-)
 * Default for language code is en (english!)                kama
 * These code should ISO 639 two letter codes.
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

/* Information about all valid languages.  */
typedef struct
{
  language_code_type lc; /* language code as enum type */
  char *abbrev;          /* two letter language code */
  char *desc;            /* full name for language code */
} language_struct;
extern language_struct language_table[];

/* The encoding, or null if not set.  */
extern char *document_encoding;


/* The commands.  */
extern void cm_documentlanguage (), cm_documentencoding ();

/* Accents, other non-English characters.  */
void cm_accent (), cm_special_char (), cm_dotless ();

extern void cm_accent_umlaut (), cm_accent_acute (), cm_accent_cedilla (),
  cm_accent_hat (), cm_accent_grave (), cm_accent_tilde ();

#endif /* not LANG_H */
