/* lang.c -- language-dependent support.
   $Id: lang.c,v 1.8 2003/05/01 00:05:27 karl Exp $

   Copyright (C) 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

#include "system.h"
#include "cmds.h"
#include "lang.h"
#include "makeinfo.h"
#include "xml.h"

/* Current document encoding.  */
encoding_code_type document_encoding_code = no_encoding;

/* Current language code; default is English.  */
language_code_type language_code = en;

static iso_map_type us_ascii_map [] = {{NULL, 0, 0}}; /* ASCII map is trivial */

/* Translation table between HTML and ISO Codes.  The last item is
   hopefully the Unicode. It might be possible that those Unicodes are
   not correct, cause I didn't check them. kama */
static iso_map_type iso8859_1_map [] = {
  { "nbsp",   0xA0, 0x00A0 },
  { "iexcl",  0xA1, 0x00A1 },
  { "cent",   0xA2, 0x00A2 },
  { "pound",  0xA3, 0x00A3 },
  { "curren", 0xA4, 0x00A4 },
  { "yen",    0xA5, 0x00A5 },
  { "brkbar", 0xA6, 0x00A6 },
  { "sect",   0xA7, 0x00A7 },
  { "uml",    0xA8, 0x00A8 },
  { "copy",   0xA9, 0x00A9 },
  { "ordf",   0xAA, 0x00AA },
  { "laquo",  0xAB, 0x00AB },
  { "not",    0xAC, 0x00AC },
  { "shy",    0xAD, 0x00AD },
  { "reg",    0xAE, 0x00AE },
  { "hibar",  0xAF, 0x00AF },
  { "deg",    0xB0, 0x00B0 },
  { "plusmn", 0xB1, 0x00B1 },
  { "sup2",   0xB2, 0x00B2 },
  { "sup3",   0xB3, 0x00B3 },
  { "acute",  0xB4, 0x00B4 },
  { "micro",  0xB5, 0x00B5 },
  { "para",   0xB6, 0x00B6 },
  { "middot", 0xB7, 0x00B7 },
  { "cedil",  0xB8, 0x00B8 },
  { "sup1",   0xB9, 0x00B9 },
  { "ordm",   0xBA, 0x00BA },
  { "raquo",  0xBB, 0x00BB },
  { "frac14", 0xBC, 0x00BC },
  { "frac12", 0xBD, 0x00BD },
  { "frac34", 0xBE, 0x00BE },
  { "iquest", 0xBF, 0x00BF },
  { "Agrave", 0xC0, 0x00C0 },
  { "Aacute", 0xC1, 0x00C1 },
  { "Acirc",  0xC2, 0x00C2 },
  { "Atilde", 0xC3, 0x00C3 },
  { "Auml",   0xC4, 0x00C4 },
  { "Aring",  0xC5, 0x00C5 },
  { "AElig",  0xC6, 0x00C6 },
  { "Ccedil", 0xC7, 0x00C7 },
  { "Ccedil", 0xC7, 0x00C7 },
  { "Egrave", 0xC8, 0x00C8 },
  { "Eacute", 0xC9, 0x00C9 },
  { "Ecirc",  0xCA, 0x00CA },
  { "Euml",   0xCB, 0x00CB },
  { "Igrave", 0xCC, 0x00CC },
  { "Iacute", 0xCD, 0x00CD },
  { "Icirc",  0xCE, 0x00CE },
  { "Iuml",   0xCF, 0x00CF },
  { "ETH",    0xD0, 0x00D0 },
  { "Ntilde", 0xD1, 0x00D1 },
  { "Ograve", 0xD2, 0x00D2 },
  { "Oacute", 0xD3, 0x00D3 },
  { "Ocirc",  0xD4, 0x00D4 },
  { "Otilde", 0xD5, 0x00D5 },
  { "Ouml",   0xD6, 0x00D6 },
  { "times",  0xD7, 0x00D7 },
  { "Oslash", 0xD8, 0x00D8 },
  { "Ugrave", 0xD9, 0x00D9 },
  { "Uacute", 0xDA, 0x00DA },
  { "Ucirc",  0xDB, 0x00DB },
  { "Uuml",   0xDC, 0x00DC },
  { "Yacute", 0xDD, 0x00DD },
  { "THORN",  0xDE, 0x00DE },
  { "szlig",  0xDF, 0x00DF },
  { "agrave", 0xE0, 0x00E0 },
  { "aacute", 0xE1, 0x00E1 },
  { "acirc",  0xE2, 0x00E2 },
  { "atilde", 0xE3, 0x00E3 },
  { "auml",   0xE4, 0x00E4 },
  { "aring",  0xE5, 0x00E5 },
  { "aelig",  0xE6, 0x00E6 },
  { "ccedil", 0xE7, 0x00E7 },
  { "egrave", 0xE8, 0x00E8 },
  { "eacute", 0xE9, 0x00E9 },
  { "ecirc",  0xEA, 0x00EA },
  { "euml",   0xEB, 0x00EB },
  { "igrave", 0xEC, 0x00EC },
  { "iacute", 0xED, 0x00ED },
  { "icirc",  0xEE, 0x00EE },
  { "iuml",   0xEF, 0x00EF },
  { "eth",    0xF0, 0x00F0 },
  { "ntilde", 0xF1, 0x00F1 },
  { "ograve", 0xF2, 0x00F2 },
  { "oacute", 0xF3, 0x00F3 },
  { "ocirc",  0xF4, 0x00F4 },
  { "otilde", 0xF5, 0x00F5 },
  { "ouml",   0xF6, 0x00F6 },
  { "divide", 0xF7, 0x00F7 },
  { "oslash", 0xF8, 0x00F8 },
  { "ugrave", 0xF9, 0x00F9 },
  { "uacute", 0xFA, 0x00FA },
  { "ucirc",  0xFB, 0x00FB },
  { "uuml",   0xFC, 0x00FC },
  { "yacute", 0xFD, 0x00FD },
  { "thorn",  0xFE, 0x00FE },
  { "yuml",   0xFF, 0x00FF },
  { NULL, 0, 0 }
};



/* Date: Mon, 31 Mar 2003 00:19:28 +0200
   From: Wojciech Polak <polak@gnu.org>
...
 * Primary Polish site for ogonki is http://www.agh.edu.pl/ogonki/,
   but it's only in Polish language (it has some interesting links).

 * A general site about ISO 8859-2 at http://nl.ijs.si/gnusl/cee/iso8859-2.html

 * ISO 8859-2 Character Set at http://nl.ijs.si/gnusl/cee/charset.html
   This site provides almost all information about iso-8859-2,
   including the character table!!! (must see!)

 * ISO 8859-2 and even HTML entities !!! (must see!)
   http://people.ssh.fi/mtr/genscript/88592.txt

 * (minor) http://www.agh.edu.pl/ogonki/plchars.html
   One more table, this time it includes even information about Polish
   characters in Unicode.
*/

static iso_map_type iso8859_2_map [] = {
  { "nbsp",	0xA0, 0x00A0 }, /* NO-BREAK SPACE */
  { "",	0xA1, 0x0104 }, /* LATIN CAPITAL LETTER A WITH OGONEK */
  { "",	0xA2, 0x02D8 }, /* BREVE */
  { "",	0xA3, 0x0141 }, /* LATIN CAPITAL LETTER L WITH STROKE */
  { "curren",	0xA4, 0x00A4 }, /* CURRENCY SIGN */
  { "",	0xA5, 0x013D }, /* LATIN CAPITAL LETTER L WITH CARON */
  { "",	0xA6, 0x015A }, /* LATIN CAPITAL LETTER S WITH ACUTE */
  { "sect",	0xA7, 0x00A7 }, /* SECTION SIGN */
  { "uml",	0xA8, 0x00A8 }, /* DIAERESIS */
  { "",	0xA9, 0x0160 }, /* LATIN CAPITAL LETTER S WITH CARON */
  { "",	0xAA, 0x015E }, /* LATIN CAPITAL LETTER S WITH CEDILLA */
  { "",	0xAB, 0x0164 }, /* LATIN CAPITAL LETTER T WITH CARON */
  { "",	0xAC, 0x0179 }, /* LATIN CAPITAL LETTER Z WITH ACUTE */
  { "shy",	0xAD, 0x00AD }, /* SOFT HYPHEN */
  { "",	0xAE, 0x017D }, /* LATIN CAPITAL LETTER Z WITH CARON */
  { "",	0xAF, 0x017B }, /* LATIN CAPITAL LETTER Z WITH DOT ABOVE */
  { "deg",	0xB0, 0x00B0 }, /* DEGREE SIGN */
  { "",	0xB1, 0x0105 }, /* LATIN SMALL LETTER A WITH OGONEK */
  { "",	0xB2, 0x02DB }, /* OGONEK */
  { "",	0xB3, 0x0142 }, /* LATIN SMALL LETTER L WITH STROKE */
  { "acute",	0xB4, 0x00B4 }, /* ACUTE ACCENT */
  { "",	0xB5, 0x013E }, /* LATIN SMALL LETTER L WITH CARON */
  { "",	0xB6, 0x015B }, /* LATIN SMALL LETTER S WITH ACUTE */
  { "",	0xB7, 0x02C7 }, /* CARON (Mandarin Chinese third tone) */
  { "cedil",	0xB8, 0x00B8 }, /* CEDILLA */
  { "",	0xB9, 0x0161 }, /* LATIN SMALL LETTER S WITH CARON */
  { "",	0xBA, 0x015F }, /* LATIN SMALL LETTER S WITH CEDILLA */
  { "",	0xBB, 0x0165 }, /* LATIN SMALL LETTER T WITH CARON */
  { "",	0xBC, 0x017A }, /* LATIN SMALL LETTER Z WITH ACUTE */
  { "",	0xBD, 0x02DD }, /* DOUBLE ACUTE ACCENT */
  { "",	0xBE, 0x017E }, /* LATIN SMALL LETTER Z WITH CARON */
  { "",	0xBF, 0x017C }, /* LATIN SMALL LETTER Z WITH DOT ABOVE */
  { "",	0xC0, 0x0154 }, /* LATIN CAPITAL LETTER R WITH ACUTE */
  { "",	0xC1, 0x00C1 }, /* LATIN CAPITAL LETTER A WITH ACUTE */
  { "",	0xC2, 0x00C2 }, /* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
  { "",	0xC3, 0x0102 }, /* LATIN CAPITAL LETTER A WITH BREVE */
  { "",	0xC4, 0x00C4 }, /* LATIN CAPITAL LETTER A WITH DIAERESIS */
  { "",	0xC5, 0x0139 }, /* LATIN CAPITAL LETTER L WITH ACUTE */
  { "",	0xC6, 0x0106 }, /* LATIN CAPITAL LETTER C WITH ACUTE */
  { "",	0xC7, 0x00C7 }, /* LATIN CAPITAL LETTER C WITH CEDILLA */
  { "",	0xC8, 0x010C }, /* LATIN CAPITAL LETTER C WITH CARON */
  { "",	0xC9, 0x00C9 }, /* LATIN CAPITAL LETTER E WITH ACUTE */
  { "",	0xCA, 0x0118 }, /* LATIN CAPITAL LETTER E WITH OGONEK */
  { "",	0xCB, 0x00CB }, /* LATIN CAPITAL LETTER E WITH DIAERESIS */
  { "",	0xCC, 0x011A }, /* LATIN CAPITAL LETTER E WITH CARON */
  { "",	0xCD, 0x00CD }, /* LATIN CAPITAL LETTER I WITH ACUTE */
  { "",	0xCE, 0x00CE }, /* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
  { "",	0xCF, 0x010E }, /* LATIN CAPITAL LETTER D WITH CARON */
  { "",	0xD0, 0x0110 }, /* LATIN CAPITAL LETTER D WITH STROKE */
  { "",	0xD1, 0x0143 }, /* LATIN CAPITAL LETTER N WITH ACUTE */
  { "",	0xD2, 0x0147 }, /* LATIN CAPITAL LETTER N WITH CARON */
  { "",	0xD3, 0x00D3 }, /* LATIN CAPITAL LETTER O WITH ACUTE */
  { "",	0xD4, 0x00D4 }, /* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
  { "",	0xD5, 0x0150 }, /* LATIN CAPITAL LETTER O WITH DOUBLE ACUTE */
  { "",	0xD6, 0x00D6 }, /* LATIN CAPITAL LETTER O WITH DIAERESIS */
  { "times",	0xD7, 0x00D7 }, /* MULTIPLICATION SIGN */
  { "",	0xD8, 0x0158 }, /* LATIN CAPITAL LETTER R WITH CARON */
  { "",	0xD9, 0x016E }, /* LATIN CAPITAL LETTER U WITH RING ABOVE */
  { "",	0xDA, 0x00DA }, /* LATIN CAPITAL LETTER U WITH ACUTE */
  { "",	0xDB, 0x0170 }, /* LATIN CAPITAL LETTER U WITH DOUBLE ACUTE */
  { "",	0xDC, 0x00DC }, /* LATIN CAPITAL LETTER U WITH DIAERESIS */
  { "",	0xDD, 0x00DD }, /* LATIN CAPITAL LETTER Y WITH ACUTE */
  { "",	0xDE, 0x0162 }, /* LATIN CAPITAL LETTER T WITH CEDILLA */
  { "",	0xDF, 0x00DF }, /* LATIN SMALL LETTER SHARP S (German) */
  { "",	0xE0, 0x0155 }, /* LATIN SMALL LETTER R WITH ACUTE */
  { "",	0xE1, 0x00E1 }, /* LATIN SMALL LETTER A WITH ACUTE */
  { "",	0xE2, 0x00E2 }, /* LATIN SMALL LETTER A WITH CIRCUMFLEX */
  { "",	0xE3, 0x0103 }, /* LATIN SMALL LETTER A WITH BREVE */
  { "",	0xE4, 0x00E4 }, /* LATIN SMALL LETTER A WITH DIAERESIS */
  { "",	0xE5, 0x013A }, /* LATIN SMALL LETTER L WITH ACUTE */
  { "",	0xE6, 0x0107 }, /* LATIN SMALL LETTER C WITH ACUTE */
  { "",	0xE7, 0x00E7 }, /* LATIN SMALL LETTER C WITH CEDILLA */
  { "",	0xE8, 0x010D }, /* LATIN SMALL LETTER C WITH CARON */
  { "",	0xE9, 0x00E9 }, /* LATIN SMALL LETTER E WITH ACUTE */
  { "",	0xEA, 0x0119 }, /* LATIN SMALL LETTER E WITH OGONEK */
  { "",	0xEB, 0x00EB }, /* LATIN SMALL LETTER E WITH DIAERESIS */
  { "",	0xEC, 0x011B }, /* LATIN SMALL LETTER E WITH CARON */
  { "",	0xED, 0x00ED }, /* LATIN SMALL LETTER I WITH ACUTE */
  { "",	0xEE, 0x00EE }, /* LATIN SMALL LETTER I WITH CIRCUMFLEX */
  { "",	0xEF, 0x010F }, /* LATIN SMALL LETTER D WITH CARON */
  { "",	0xF0, 0x0111 }, /* LATIN SMALL LETTER D WITH STROKE */
  { "",	0xF1, 0x0144 }, /* LATIN SMALL LETTER N WITH ACUTE */
  { "",	0xF2, 0x0148 }, /* LATIN SMALL LETTER N WITH CARON */
  { "",	0xF3, 0x00F3 }, /* LATIN SMALL LETTER O WITH ACUTE */
  { "",	0xF4, 0x00F4 }, /* LATIN SMALL LETTER O WITH CIRCUMFLEX */
  { "",	0xF5, 0x0151 }, /* LATIN SMALL LETTER O WITH DOUBLE ACUTE */
  { "",	0xF6, 0x00F6 }, /* LATIN SMALL LETTER O WITH DIAERESIS */
  { "divide",	0xF7, 0x00F7 }, /* DIVISION SIGN */
  { "",	0xF8, 0x0159 }, /* LATIN SMALL LETTER R WITH CARON */
  { "",	0xF9, 0x016F }, /* LATIN SMALL LETTER U WITH RING ABOVE */
  { "",	0xFA, 0x00FA }, /* LATIN SMALL LETTER U WITH ACUTE */
  { "",	0xFB, 0x0171 }, /* LATIN SMALL LETTER U WITH DOUBLE ACUTE */
  { "",	0xFC, 0x00FC }, /* LATIN SMALL LETTER U WITH DIAERESIS */
  { "",	0xFD, 0x00FD }, /* LATIN SMALL LETTER Y WITH ACUTE */
  { "",	0xFE, 0x0163 }, /* LATIN SMALL LETTER T WITH CEDILLA */
  { "",	0xFF, 0x02D9 }, /* DOT ABOVE (Mandarin Chinese light tone) */
  { NULL, 0, 0 }
};

encoding_type encoding_table[] = {
  { no_encoding, "(no encoding)", NULL },
  { US_ASCII,    "US-ASCII",    us_ascii_map },
  { ISO_8859_1,  "ISO-8859-1",  (iso_map_type *) iso8859_1_map },
  { ISO_8859_2,  "ISO-8859-2",  (iso_map_type *) iso8859_2_map },
  { ISO_8859_3,  "ISO-8859-3",  NULL },
  { ISO_8859_4,  "ISO-8859-4",  NULL },
  { ISO_8859_5,  "ISO-8859-5",  NULL },
  { ISO_8859_6,  "ISO-8859-6",  NULL },
  { ISO_8859_7,  "ISO-8859-7",  NULL },
  { ISO_8859_8,  "ISO-8859-8",  NULL },
  { ISO_8859_9,  "ISO-8859-9",  NULL },
  { ISO_8859_10, "ISO-8859-10", NULL },
  { ISO_8859_11, "ISO-8859-11", NULL },
  { ISO_8859_12, "ISO-8859-12", NULL },
  { ISO_8859_13, "ISO-8859-13", NULL },
  { ISO_8859_14, "ISO-8859-14", NULL },
  { ISO_8859_15, "ISO-8859-15", NULL },
  { last_encoding_code, NULL, NULL }
};


language_type language_table[] = {
  { aa, "aa", "Afar" },
  { ab, "ab", "Abkhazian" },
  { af, "af", "Afrikaans" },
  { am, "am", "Amharic" },
  { ar, "ar", "Arabic" },
  { as, "as", "Assamese" },
  { ay, "ay", "Aymara" },
  { az, "az", "Azerbaijani" },
  { ba, "ba", "Bashkir" },
  { be, "be", "Byelorussian" },
  { bg, "bg", "Bulgarian" },
  { bh, "bh", "Bihari" },
  { bi, "bi", "Bislama" },
  { bn, "bn", "Bengali; Bangla" },
  { bo, "bo", "Tibetan" },
  { br, "br", "Breton" },
  { ca, "ca", "Catalan" },
  { co, "co", "Corsican" },
  { cs, "cs", "Czech" },
  { cy, "cy", "Welsh" },
  { da, "da", "Danish" },
  { de, "de", "German" },
  { dz, "dz", "Bhutani" },
  { el, "el", "Greek" },
  { en, "en", "English" },
  { eo, "eo", "Esperanto" },
  { es, "es", "Spanish" },
  { et, "et", "Estonian" },
  { eu, "eu", "Basque" },
  { fa, "fa", "Persian" },
  { fi, "fi", "Finnish" },
  { fj, "fj", "Fiji" },
  { fo, "fo", "Faroese" },
  { fr, "fr", "French" },
  { fy, "fy", "Frisian" },
  { ga, "ga", "Irish" },
  { gd, "gd", "Scots Gaelic" },
  { gl, "gl", "Galician" },
  { gn, "gn", "Guarani" },
  { gu, "gu", "Gujarati" },
  { ha, "ha", "Hausa" },
  { he, "he", "Hebrew" } /* (formerly iw) */,
  { hi, "hi", "Hindi" },
  { hr, "hr", "Croatian" },
  { hu, "hu", "Hungarian" },
  { hy, "hy", "Armenian" },
  { ia, "ia", "Interlingua" },
  { id, "id", "Indonesian" } /* (formerly in) */,
  { ie, "ie", "Interlingue" },
  { ik, "ik", "Inupiak" },
  { is, "is", "Icelandic" },
  { it, "it", "Italian" },
  { iu, "iu", "Inuktitut" },
  { ja, "ja", "Japanese" },
  { jw, "jw", "Javanese" },
  { ka, "ka", "Georgian" },
  { kk, "kk", "Kazakh" },
  { kl, "kl", "Greenlandic" },
  { km, "km", "Cambodian" },
  { kn, "kn", "Kannada" },
  { ko, "ko", "Korean" },
  { ks, "ks", "Kashmiri" },
  { ku, "ku", "Kurdish" },
  { ky, "ky", "Kirghiz" },
  { la, "la", "Latin" },
  { ln, "ln", "Lingala" },
  { lo, "lo", "Laothian" },
  { lt, "lt", "Lithuanian" },
  { lv, "lv", "Latvian, Lettish" },
  { mg, "mg", "Malagasy" },
  { mi, "mi", "Maori" },
  { mk, "mk", "Macedonian" },
  { ml, "ml", "Malayalam" },
  { mn, "mn", "Mongolian" },
  { mo, "mo", "Moldavian" },
  { mr, "mr", "Marathi" },
  { ms, "ms", "Malay" },
  { mt, "mt", "Maltese" },
  { my, "my", "Burmese" },
  { na, "na", "Nauru" },
  { ne, "ne", "Nepali" },
  { nl, "nl", "Dutch" },
  { no, "no", "Norwegian" },
  { oc, "oc", "Occitan" },
  { om, "om", "(Afan) Oromo" },
  { or, "or", "Oriya" },
  { pa, "pa", "Punjabi" },
  { pl, "pl", "Polish" },
  { ps, "ps", "Pashto, Pushto" },
  { pt, "pt", "Portuguese" },
  { qu, "qu", "Quechua" },
  { rm, "rm", "Rhaeto-Romance" },
  { rn, "rn", "Kirundi" },
  { ro, "ro", "Romanian" },
  { ru, "ru", "Russian" },
  { rw, "rw", "Kinyarwanda" },
  { sa, "sa", "Sanskrit" },
  { sd, "sd", "Sindhi" },
  { sg, "sg", "Sangro" },
  { sh, "sh", "Serbo-Croatian" },
  { si, "si", "Sinhalese" },
  { sk, "sk", "Slovak" },
  { sl, "sl", "Slovenian" },
  { sm, "sm", "Samoan" },
  { sn, "sn", "Shona" },
  { so, "so", "Somali" },
  { sq, "sq", "Albanian" },
  { sr, "sr", "Serbian" },
  { ss, "ss", "Siswati" },
  { st, "st", "Sesotho" },
  { su, "su", "Sundanese" },
  { sv, "sv", "Swedish" },
  { sw, "sw", "Swahili" },
  { ta, "ta", "Tamil" },
  { te, "te", "Telugu" },
  { tg, "tg", "Tajik" },
  { th, "th", "Thai" },
  { ti, "ti", "Tigrinya" },
  { tk, "tk", "Turkmen" },
  { tl, "tl", "Tagalog" },
  { tn, "tn", "Setswana" },
  { to, "to", "Tonga" },
  { tr, "tr", "Turkish" },
  { ts, "ts", "Tsonga" },
  { tt, "tt", "Tatar" },
  { tw, "tw", "Twi" },
  { ug, "ug", "Uighur" },
  { uk, "uk", "Ukrainian" },
  { ur, "ur", "Urdu" },
  { uz, "uz", "Uzbek" },
  { vi, "vi", "Vietnamese" },
  { vo, "vo", "Volapuk" },
  { wo, "wo", "Wolof" },
  { xh, "xh", "Xhosa" },
  { yi, "yi", "Yiddish" } /* (formerly ji) */,
  { yo, "yo", "Yoruba" },
  { za, "za", "Zhuang" },
  { zh, "zh", "Chinese" },
  { zu, "zu", "Zulu" },
  { last_language_code, NULL, NULL }
};



/* @documentlanguage.  Maybe we'll do something useful with this in the
   future.  For now, we just recognize it.  */
void
cm_documentlanguage ()
{
  language_code_type c;
  char *lang_arg;

  /* Read the line with the language code on it.  */
  get_rest_of_line (0, &lang_arg);

  /* Linear search is fine these days.  */
  for (c = aa; c != last_language_code; c++)
    {
      if (strcmp (lang_arg, language_table[c].abbrev) == 0)
        { /* Set current language code.  */
          language_code = c;
          break;
        }
    }

  /* If we didn't find this code, complain.  */
  if (c == last_language_code)
    warning (_("%s is not a valid ISO 639 language code"), lang_arg);

  free (lang_arg);
}



/* Search through the encoding table for the given character, returning
   its equivalent.  */

static int
cm_search_iso_map (html)
      char *html;
{
  int i;
  iso_map_type *iso = encoding_table[document_encoding_code].isotab;

  /* If no conversion table for this encoding, quit.  */
  if (!iso)
    return -1;

  for (i = 0; iso[i].html; i++)
    {
      if (strcmp (html, iso[i].html) == 0)
        return i;
    }

  return -1;
}


/* @documentencoding.  Set the translation table.  */

void
cm_documentencoding ()
{
  encoding_code_type enc;
  char *enc_arg;

  get_rest_of_line (1, &enc_arg);

  /* See if we have this encoding.  */
  for (enc = no_encoding+1; enc != last_encoding_code; enc++)
    {
      if (strcasecmp (enc_arg, encoding_table[enc].encname) == 0)
        {
          document_encoding_code = enc;
          break;
        }
    }

  /* If we didn't find this code, complain.  */
  if (enc == last_encoding_code)
    warning (_("unrecognized encoding name `%s'"), enc_arg);

  else if (encoding_table[document_encoding_code].isotab == NULL)
    warning (_("sorry, encoding `%s' not supported"), enc_arg);

  free (enc_arg);
}


/* If html or xml output, add HTML_STR to the output.  If not html and
   the user requested encoded output, add the real 8-bit character
   corresponding to HTML_STR from the translation tables.  Otherwise,
   add INFO_STR.  */

void
add_encoded_char (html_str, info_str)
      char *html_str;
      char *info_str;
{
  if (html)
    add_word_args ("&%s;", html_str);
  else if (xml)
    xml_insert_entity (html_str);
  else if (enable_encoding)
    {
      /* Look for HTML_STR in the current translation table.  */
      int rc = cm_search_iso_map (html_str);
      if (rc >= 0)
        /* We found it, add the real character.  */
        add_char (encoding_table[document_encoding_code].isotab[rc].bytecode);
      else
        { /* We didn't find it, that seems bad.  */
          warning (_("invalid encoded character `%s'"), html_str);
          add_word (info_str);
        }
    }
  else
    add_word (info_str);
}



/* Output an accent for HTML or XML. */

static void
cm_accent_generic_html (arg, start, end, html_supported, single,
                        html_solo_standalone, html_solo)
     int arg, start, end;
     char *html_supported;
     int single;
     int html_solo_standalone;
     char *html_solo;
{
  static int valid_html_accent; /* yikes */

  if (arg == START)
    { /* If HTML has good support for this character, use it.  */
      if (strchr (html_supported, curchar ()))
        { /* Yes; start with an ampersand.  The character itself
             will be added later in read_command (makeinfo.c).  */
	  int saved_escape_html = escape_html;
	  escape_html = 0;
          valid_html_accent = 1;
          add_char ('&');
	  escape_html = saved_escape_html;
        }
      else
        {
          valid_html_accent = 0;
          if (html_solo_standalone)
            { /* No special HTML support, so produce standalone char.  */
	      if (xml)
		xml_insert_entity (html_solo);
	      else
		add_word_args ("&%s;", html_solo);
            }
          else
            /* If the html_solo does not exist as standalone character
               (namely &circ; &grave; &tilde;), then we use
               the single character version instead.  */
            add_char (single);
        }
    }
  else if (arg == END)
    { /* Only if we saw a valid_html_accent can we use the full
         HTML accent (umlaut, grave ...).  */
      if (valid_html_accent)
        {
          add_word (html_solo);
          add_char (';');
        }
    }
}


static void
cm_accent_generic_no_headers (arg, start, end, single, html_solo)
     int arg, start, end;
     int single;
     char *html_solo;
{
  if (arg == END)
    {
      if (no_encoding)
        add_char (single);
      else
        {
          int rc;
          char *buffer = xmalloc (1 + strlen (html_solo) + 1);
          buffer[0] = output_paragraph[end - 1];
          buffer[1] = 0;
          strcat (buffer, html_solo);

          rc = cm_search_iso_map (buffer);
          if (rc >= 0)
            /* A little bit tricky ;-)
               Here we replace the character which has
               been inserted in read_command with
               the value we have found in converting table
               Does there exist a better way to do this?  kama. */
            output_paragraph[end - 1]
              = encoding_table[document_encoding_code].isotab[rc].bytecode;
          else
            { /* If we didn't find a translation for this character,
                 put the single instead. E.g., &Xuml; does not exist so X&uml;
                 should be produced. */
              warning (_("%s is an invalid ISO code, using %c"),
                       buffer, single);
              add_char (single);
            }

          free (buffer);
        }
    }
}



/* Accent commands that take explicit arguments and don't have any
   special HTML support.  */

void
cm_accent (arg)
    int arg;
{
  int old_escape_html = escape_html;
  escape_html = 0;
  if (arg == START)
    {
      /* Must come first to avoid ambiguity with overdot.  */
      if (strcmp (command, "udotaccent") == 0)      /* underdot */
        add_char ('.');
    }
  else if (arg == END)
    {
      if (strcmp (command, "=") == 0)               /* macron */
        add_word ((html || xml) ? "&macr;" : "=");
      else if (strcmp (command, "H") == 0)          /* Hungarian umlaut */
        add_word ("''");
      else if (strcmp (command, "dotaccent") == 0)  /* overdot */
        add_meta_char ('.');
      else if (strcmp (command, "ringaccent") == 0) /* ring */
        add_char ('*');
      else if (strcmp (command, "tieaccent") == 0)  /* long tie */
        add_char ('[');
      else if (strcmp (command, "u") == 0)          /* breve */
        add_char ('(');
      else if (strcmp (command, "ubaraccent") == 0) /* underbar */
        add_char ('_');
      else if (strcmp (command, "v") == 0)          /* hacek/check */
        add_word ((html || xml) ? "&lt;" : "<");
    }
  escape_html = old_escape_html;
}

/* Common routine for the accent characters that have support in HTML.
   If the character being accented is in the HTML_SUPPORTED set, then
   produce &CHTML_SOLO;, for example, &Auml; for an A-umlaut.  If not in
   HTML_SUPPORTED, just produce &HTML_SOLO;X for the best we can do with
   at an X-umlaut.  If not producing HTML, just use SINGLE, a
   character such as " which is the best plain text representation we
   can manage.  If HTML_SOLO_STANDALONE is nonzero the given HTML_SOLO
   exists as valid standalone character in HTML, e.g., &uml;.  */

static void
cm_accent_generic (arg, start, end, html_supported, single,
                   html_solo_standalone, html_solo)
     int arg, start, end;
     char *html_supported;
     int single;
     int html_solo_standalone;
     char *html_solo;
{
  if (html || xml)
    cm_accent_generic_html (arg, start, end, html_supported,
                            single, html_solo_standalone, html_solo);
  else if (no_headers)
    cm_accent_generic_no_headers (arg, start, end, single, html_solo);
  else if (arg == END)
    {
      if (enable_encoding)
        /* use 8-bit if available */
        cm_accent_generic_no_headers (arg, start, end, single, html_solo);
      else
        /* use regular character */
        add_char (single);
    }
}

void
cm_accent_umlaut (arg, start, end)
     int arg, start, end;
{
  cm_accent_generic (arg, start, end, "aouAOUEeIiy", '"', 1, "uml");
}

void
cm_accent_acute (arg, start, end)
     int arg, start, end;
{
  cm_accent_generic (arg, start, end, "AEIOUYaeiouy", '\'', 1, "acute");
}

void
cm_accent_cedilla (arg, start, end)
     int arg, start, end;
{
  cm_accent_generic (arg, start, end, "Cc", ',', 1, "cedil");
}

void
cm_accent_hat (arg, start, end)
     int arg, start, end;
{
  cm_accent_generic (arg, start, end, "AEIOUaeiou", '^', 0, "circ");
}

void
cm_accent_grave (arg, start, end)
     int arg, start, end;
{
  cm_accent_generic (arg, start, end, "AEIOUaeiou", '`', 0, "grave");
}

void
cm_accent_tilde (arg, start, end)
     int arg, start, end;
{
  cm_accent_generic (arg, start, end, "ANOano", '~', 0, "tilde");
}



/* Non-English letters/characters that don't insert themselves.  */
void
cm_special_char (arg)
{
  int old_escape_html = escape_html;
  escape_html = 0;

  if (arg == START)
    {
      if ((*command == 'L' || *command == 'l'
           || *command == 'O' || *command == 'o')
          && command[1] == 0)
        { /* Lslash lslash Oslash oslash.
             Lslash and lslash aren't supported in HTML.  */
          if ((html || xml) && command[0] == 'O')
            add_encoded_char ("Oslash", "/O");
          else if ((html || xml) && command[0] == 'o')
            add_encoded_char ("oslash", "/o");
          else
            add_word_args ("/%c", command[0]);
        }
      else if (strcmp (command, "exclamdown") == 0)
        add_encoded_char ("iexcl", "!");
      else if (strcmp (command, "pounds") == 0)
        add_encoded_char ("pound" , "#");
      else if (strcmp (command, "questiondown") == 0)
        add_encoded_char ("iquest", "?");
      else if (strcmp (command, "AE") == 0)
        add_encoded_char ("AElig", command);
      else if (strcmp (command, "ae") == 0)
        add_encoded_char ("aelig",  command);
      else if (strcmp (command, "OE") == 0)
        add_encoded_char ("#140", command);
      else if (strcmp (command, "oe") == 0)
        add_encoded_char ("#156", command);
      else if (strcmp (command, "AA") == 0)
        add_encoded_char ("Aring", command);
      else if (strcmp (command, "aa") == 0)
        add_encoded_char ("aring", command);
      else if (strcmp (command, "ss") == 0)
        add_encoded_char ("szlig", command);
      else
        line_error ("cm_special_char internal error: command=@%s", command);
    }
  escape_html = old_escape_html;
}

/* Dotless i or j.  */
void
cm_dotless (arg, start, end)
    int arg, start, end;
{
  if (arg == END)
    {
      xml_no_para --;
      if (output_paragraph[start] != 'i' && output_paragraph[start] != 'j')
        /* This error message isn't perfect if the argument is multiple
           characters, but it doesn't seem worth getting right.  */
        line_error (_("%c%s expects `i' or `j' as argument, not `%c'"),
                    COMMAND_PREFIX, command, output_paragraph[start]);

      else if (end - start != 1)
        line_error (_("%c%s expects a single character `i' or `j' as argument"),
                    COMMAND_PREFIX, command);

      /* We've already inserted the `i' or `j', so nothing to do.  */
    }
  else
    xml_no_para ++;
}
