/* lang.c -- language depend behaviour (startpoint)
   $Id: lang.c,v 1.11 1999/07/13 21:16:29 karl Exp $

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

#include "system.h"
#include "cmds.h"
#include "lang.h"
#include "makeinfo.h"

/* Current document encoding.  */
char *document_encoding = NULL;

/* Current language code; default is English.  */
language_code_type language_code = en;

language_struct language_table[] = {
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
  get_rest_of_line (1, &lang_arg);

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



/* @documentencoding.  Set global.  */
void
cm_documentencoding ()
{
  get_rest_of_line (1, &document_encoding);
}



/* Accent commands that take explicit arguments and don't have any
   special HTML support.  */

void
cm_accent (arg)
    int arg;
{
  if (arg == START)
    {
      /* Must come first to avoid ambiguity with overdot.  */
      if (strcmp (command, "udotaccent") == 0)      /* underdot */
        add_char ('.');
    }
  else if (arg == END)
    {
      if (strcmp (command, "=") == 0)               /* macron */
        add_word (html ? "&macr;" : "=");
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
        add_word (html ? "&lt;" : "<");
    }
}

/* Common routine for the accent characters that have support in HTML.
   If the character being accented is in the HTML_SUPPORTED set, then
   produce &CHTML_SOLO;, for example, &Auml; for an A-umlaut.  If not in
   HTML_SUPPORTED, just produce &HTML_SOLO;X for the best we can do with
   at an X-umlaut.  Finally, if not producing HTML, just use SINGLE, a
   character such as " which is the best plain text representation we
   can manage.  If HTML_SOLO_STANDALONE is zero the given HTML_SOLO
   does not exist as valid standalone character in HTML.  */

static void
cm_accent_generic (arg, start, end, html_supported, single,
                   html_solo_standalone, html_solo)
     int arg, start, end;
     char *html_supported;
     int single;
     int html_solo_standalone;
     char *html_solo;
{
  if (html)
    {
      static int valid_html_accent;

      if (arg == START)
	{ /* If HTML has good support for this character, use it.  */
	  if (strchr (html_supported, curchar ()))
	    { /* Yes; start with an ampersand.  The character itself
	         will be added later in read_command (makeinfo.c).  */
	      add_char ('&');
              valid_html_accent = 1;
            }
	  else
	    { /* No special HTML support, so produce standalone char.  */
	      valid_html_accent = 0;
	      if (html_solo_standalone)
		{
		  add_char ('&');
		  add_word (html_solo);
		  add_char (';');
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
  else if (arg == END)
    { /* Not producing HTML, so just use the normal character.  */
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
  cm_accent_generic (arg, start, end, "AOano", '~', 0, "tilde");
}



/* Non-English letters/characters that don't insert themselves.  */
void
cm_special_char (arg)
{
  if (arg == START)
    {
      if ((*command == 'L' || *command == 'l'
           || *command == 'O' || *command == 'o')
          && command[1] == 0)
        { /* Lslash lslash Oslash oslash.
             Lslash and lslash aren't supported in HTML.  */
          if (html && (command[0] == 'O' || command[0] == 'o'))
            add_word_args ("&%cslash;", command[0]);
          else
            add_word_args ("/%c", command[0]);
        }
      else if (strcmp (command, "exclamdown") == 0)
        add_word (html ? "&iexcl;" : "!");
      else if (strcmp (command, "pounds") == 0)
        add_word (html ? "&pound;" : "#");
      else if (strcmp (command, "questiondown") == 0)
        add_word (html ? "&iquest;" : "?");
      else if (strcmp (command, "AE") == 0)
        add_word (html ? "&AElig;" : command);
      else if (strcmp (command, "ae") == 0)
        add_word (html ? "&aelig;" : command);
      else if (strcmp (command, "OE") == 0)
        add_word (html ? "&#140;" : command);
      else if (strcmp (command, "oe") == 0)
        add_word (html ? "&#156;" : command);
      else if (strcmp (command, "AA") == 0)
        add_word (html ? "&Aring;" : command);
      else if (strcmp (command, "aa") == 0)
        add_word (html ? "&aring;" : command);
      else if (strcmp (command, "ss") == 0)
        add_word (html ? "&szlig;" : command);
      else
        line_error ("cm_special_char internal error: command=@%s", command);
    }
}

/* Dotless i or j.  */
void
cm_dotless (arg, start, end)
    int arg, start, end;
{
  if (arg == END)
    {
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
}
