/* Diacritics processing for a few character codes.
   Copyright (C) 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
   Francois Pinard <pinard@iro.umontreal.ca>, 1988.

   All this file is a temporary hack, waiting for locales in GNU.
*/

extern const char diacrit_base[]; /* characters without diacritics */
extern const char diacrit_diac[]; /* diacritic code for each character */

/* Returns CHR without its diacritic.  CHR is known to be alphabetic.  */
#define tobase(chr) (diacrit_base[(unsigned char) (chr)])

/* Returns a diacritic code for CHR.  CHR is known to be alphabetic.  */
#define todiac(chr) (diacrit_diac[(unsigned char) (chr)])

