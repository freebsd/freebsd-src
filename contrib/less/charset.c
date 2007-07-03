/*
 * Copyright (C) 1984-2007  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Functions to define the character set
 * and do things specific to the character set.
 */

#include "less.h"
#if HAVE_LOCALE
#include <locale.h>
#include <ctype.h>
#include <langinfo.h>
#endif

#include "charset.h"

public int utf_mode = 0;

/*
 * Predefined character sets,
 * selected by the LESSCHARSET environment variable.
 */
struct charset {
	char *name;
	int *p_flag;
	char *desc;
} charsets[] = {
	{ "ascii",		NULL,       "8bcccbcc18b95.b" },
	{ "utf-8",		&utf_mode,  "8bcccbcc18b95.b126.bb" },
	{ "iso8859",		NULL,       "8bcccbcc18b95.33b." },
	{ "latin3",		NULL,       "8bcccbcc18b95.33b5.b8.b15.b4.b12.b18.b12.b." },
	{ "arabic",		NULL,       "8bcccbcc18b95.33b.3b.7b2.13b.3b.b26.5b19.b" },
	{ "greek",		NULL,       "8bcccbcc18b95.33b4.2b4.b3.b35.b44.b" },
	{ "greek2005",		NULL,       "8bcccbcc18b95.33b14.b35.b44.b" },
	{ "hebrew",		NULL,       "8bcccbcc18b95.33b.b29.32b28.2b2.b" },
	{ "koi8-r",		NULL,       "8bcccbcc18b95.b." },
	{ "KOI8-T",		NULL,       "8bcccbcc18b95.b8.b6.b8.b.b.5b7.3b4.b4.b3.b.b.3b." },
	{ "georgianps",		NULL,       "8bcccbcc18b95.3b11.4b12.2b." },
	{ "tcvn",		NULL,       "b..b...bcccbccbbb7.8b95.b48.5b." },
	{ "TIS-620",		NULL,       "8bcccbcc18b95.b.4b.11b7.8b." },
	{ "next",		NULL,       "8bcccbcc18b95.bb125.bb" },
	{ "dos",		NULL,       "8bcccbcc12bc5b95.b." },
	{ "windows-1251",	NULL,       "8bcccbcc12bc5b95.b24.b." },
	{ "windows-1252",	NULL,       "8bcccbcc12bc5b95.b.b11.b.2b12.b." },
	{ "windows-1255",	NULL,       "8bcccbcc12bc5b95.b.b8.b.5b9.b.4b." },
	{ "ebcdic",		NULL,       "5bc6bcc7bcc41b.9b7.9b5.b..8b6.10b6.b9.7b9.8b8.17b3.3b9.7b9.8b8.6b10.b.b.b." },
	{ "IBM-1047",		NULL,       "4cbcbc3b9cbccbccbb4c6bcc5b3cbbc4bc4bccbc191.b" },
	{ NULL, NULL, NULL }
};

/*
 * Support "locale charmap"/nl_langinfo(CODESET) values, as well as others.
 */
struct cs_alias {
	char *name;
	char *oname;
} cs_aliases[] = {
	{ "UTF-8",		"utf-8" },
	{ "ANSI_X3.4-1968",	"ascii" },
	{ "US-ASCII",		"ascii" },
	{ "latin1",		"iso8859" },
	{ "ISO-8859-1",		"iso8859" },
	{ "latin9",		"iso8859" },
	{ "ISO-8859-15",	"iso8859" },
	{ "latin2",		"iso8859" },
	{ "ISO-8859-2",		"iso8859" },
	{ "ISO-8859-3",		"latin3" },
	{ "latin4",		"iso8859" },
	{ "ISO-8859-4",		"iso8859" },
	{ "cyrillic",		"iso8859" },
	{ "ISO-8859-5",		"iso8859" },
	{ "ISO-8859-6",		"arabic" },
	{ "ISO-8859-7",		"greek" },
	{ "IBM9005",		"greek2005" },
	{ "ISO-8859-8",		"hebrew" },
	{ "latin5",		"iso8859" },
	{ "ISO-8859-9",		"iso8859" },
	{ "latin6",		"iso8859" },
	{ "ISO-8859-10",	"iso8859" },
	{ "latin7",		"iso8859" },
	{ "ISO-8859-13",	"iso8859" },
	{ "latin8",		"iso8859" },
	{ "ISO-8859-14",	"iso8859" },
	{ "latin10",		"iso8859" },
	{ "ISO-8859-16",	"iso8859" },
	{ "IBM437",		"dos" },
	{ "EBCDIC-US",		"ebcdic" },
	{ "IBM1047",		"IBM-1047" },
	{ "KOI8-R",		"koi8-r" },
	{ "KOI8-U",		"koi8-r" },
	{ "GEORGIAN-PS",	"georgianps" },
	{ "TCVN5712-1", 	"tcvn" },
	{ "NEXTSTEP",		"next" },
	{ "windows",		"windows-1252" }, /* backward compatibility */
	{ "CP1251",		"windows-1251" },
	{ "CP1252",		"windows-1252" },
	{ "CP1255",		"windows-1255" },
	{ NULL, NULL }
};

#define	IS_BINARY_CHAR	01
#define	IS_CONTROL_CHAR	02

static char chardef[256];
static char *binfmt = NULL;
static char *utfbinfmt = NULL;
public int binattr = AT_STANDOUT;


/*
 * Define a charset, given a description string.
 * The string consists of 256 letters,
 * one for each character in the charset.
 * If the string is shorter than 256 letters, missing letters
 * are taken to be identical to the last one.
 * A decimal number followed by a letter is taken to be a 
 * repetition of the letter.
 *
 * Each letter is one of:
 *	. normal character
 *	b binary character
 *	c control character
 */
	static void
ichardef(s)
	char *s;
{
	register char *cp;
	register int n;
	register char v;

	n = 0;
	v = 0;
	cp = chardef;
	while (*s != '\0')
	{
		switch (*s++)
		{
		case '.':
			v = 0;
			break;
		case 'c':
			v = IS_CONTROL_CHAR;
			break;
		case 'b':
			v = IS_BINARY_CHAR|IS_CONTROL_CHAR;
			break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = (10 * n) + (s[-1] - '0');
			continue;

		default:
			error("invalid chardef", NULL_PARG);
			quit(QUIT_ERROR);
			/*NOTREACHED*/
		}

		do
		{
			if (cp >= chardef + sizeof(chardef))
			{
				error("chardef longer than 256", NULL_PARG);
				quit(QUIT_ERROR);
				/*NOTREACHED*/
			}
			*cp++ = v;
		} while (--n > 0);
		n = 0;
	}

	while (cp < chardef + sizeof(chardef))
		*cp++ = v;
}

/*
 * Define a charset, given a charset name.
 * The valid charset names are listed in the "charsets" array.
 */
	static int
icharset(name, no_error)
	register char *name;
	int no_error;
{
	register struct charset *p;
	register struct cs_alias *a;

	if (name == NULL || *name == '\0')
		return (0);

	/* First see if the name is an alias. */
	for (a = cs_aliases;  a->name != NULL;  a++)
	{
		if (strcmp(name, a->name) == 0)
		{
			name = a->oname;
			break;
		}
	}

	for (p = charsets;  p->name != NULL;  p++)
	{
		if (strcmp(name, p->name) == 0)
		{
			ichardef(p->desc);
			if (p->p_flag != NULL)
				*(p->p_flag) = 1;
			return (1);
		}
	}

	if (!no_error) {
		error("invalid charset name", NULL_PARG);
		quit(QUIT_ERROR);
	}
	return (0);
}

#if HAVE_LOCALE
/*
 * Define a charset, given a locale name.
 */
	static void
ilocale()
{
	register int c;

	for (c = 0;  c < (int) sizeof(chardef);  c++)
	{
		if (isprint(c))
			chardef[c] = 0;
		else if (iscntrl(c))
			chardef[c] = IS_CONTROL_CHAR;
		else
			chardef[c] = IS_BINARY_CHAR|IS_CONTROL_CHAR;
	}
}
#endif

/*
 * Define the printing format for control (or binary utf) chars.
 */
   	static void
setbinfmt(s, fmtvarptr, default_fmt)
	char *s;
	char **fmtvarptr;
	char *default_fmt;
{
	if (s && utf_mode)
	{
		/* It would be too hard to account for width otherwise.  */
		char *t = s;
		while (*t)
		{
			if (*t < ' ' || *t > '~')
			{
				s = default_fmt;
				goto attr;
			}
			t++;
		}
	}

	/* %n is evil */
	if (s == NULL || *s == '\0' ||
	    (*s == '*' && (s[1] == '\0' || s[2] == '\0' || strchr(s + 2, 'n'))) ||
	    (*s != '*' && strchr(s, 'n')))
		s = default_fmt;

	/*
	 * Select the attributes if it starts with "*".
	 */
 attr:
	if (*s == '*')
	{
		switch (s[1])
		{
		case 'd':  binattr = AT_BOLD;      break;
		case 'k':  binattr = AT_BLINK;     break;
		case 's':  binattr = AT_STANDOUT;  break;
		case 'u':  binattr = AT_UNDERLINE; break;
		default:   binattr = AT_NORMAL;    break;
		}
		s += 2;
	}
	*fmtvarptr = s;
}

/*
 *
 */
	static void
set_charset()
{
	char *s;

	/*
	 * See if environment variable LESSCHARSET is defined.
	 */
	s = lgetenv("LESSCHARSET");
	if (icharset(s, 0))
		return;

	/*
	 * LESSCHARSET is not defined: try LESSCHARDEF.
	 */
	s = lgetenv("LESSCHARDEF");
	if (s != NULL && *s != '\0')
	{
		ichardef(s);
		return;
	}

#if HAVE_LOCALE
#ifdef CODESET
	/*
	 * Try using the codeset name as the charset name.
	 */
	s = nl_langinfo(CODESET);
	if (icharset(s, 1))
		return;
#endif
#endif

#if HAVE_STRSTR
	/*
	 * Check whether LC_ALL, LC_CTYPE or LANG look like UTF-8 is used.
	 */
	if ((s = lgetenv("LC_ALL")) != NULL ||
	    (s = lgetenv("LC_CTYPE")) != NULL ||
	    (s = lgetenv("LANG")) != NULL)
	{
		if (   strstr(s, "UTF-8") != NULL || strstr(s, "utf-8") != NULL
		    || strstr(s, "UTF8")  != NULL || strstr(s, "utf8")  != NULL)
			if (icharset("utf-8", 1))
				return;
	}
#endif

#if HAVE_LOCALE
	/*
	 * Get character definitions from locale functions,
	 * rather than from predefined charset entry.
	 */
	ilocale();
#if MSDOS_COMPILER
	/*
	 * Default to "dos".
	 */
	(void) icharset("dos", 1);
#else
	/*
	 * Default to "latin1".
	 */
	(void) icharset("latin1", 1);
#endif
#endif
}

/*
 * Initialize charset data structures.
 */
	public void
init_charset()
{
	char *s;

#if HAVE_LOCALE
	setlocale(LC_ALL, "");
#endif

	set_charset();

	s = lgetenv("LESSBINFMT");
	setbinfmt(s, &binfmt, "*s<%02X>");
	
	s = lgetenv("LESSUTFBINFMT");
	setbinfmt(s, &utfbinfmt, "<U+%04lX>");
}

/*
 * Is a given character a "binary" character?
 */
	public int
binary_char(c)
	unsigned char c;
{
	c &= 0377;
	return (chardef[c] & IS_BINARY_CHAR);
}

/*
 * Is a given character a "control" character?
 */
	public int
control_char(c)
	int c;
{
	c &= 0377;
	return (chardef[c] & IS_CONTROL_CHAR);
}

/*
 * Return the printable form of a character.
 * For example, in the "ascii" charset '\3' is printed as "^C".
 */
	public char *
prchar(c)
	int c;
{
	/* {{ This buffer can be overrun if LESSBINFMT is a long string. }} */
	static char buf[32];

	c &= 0377;
	if ((c < 128 || !utf_mode) && !control_char(c))
		SNPRINTF1(buf, sizeof(buf), "%c", c);
	else if (c == ESC)
		strcpy(buf, "ESC");
#if IS_EBCDIC_HOST
	else if (!binary_char(c) && c < 64)
		SNPRINTF1(buf, sizeof(buf), "^%c",
		/*
		 * This array roughly inverts CONTROL() #defined in less.h,
	 	 * and should be kept in sync with CONTROL() and IBM-1047.
 	 	 */
		"@ABC.I.?...KLMNO"
		"PQRS.JH.XY.."
		"\\]^_"
		"......W[.....EFG"
		"..V....D....TU.Z"[c]);
#else
  	else if (c < 128 && !control_char(c ^ 0100))
  		SNPRINTF1(buf, sizeof(buf), "^%c", c ^ 0100);
#endif
	else
		SNPRINTF1(buf, sizeof(buf), binfmt, c);
	return (buf);
}

/*
 * Return the printable form of a UTF-8 character.
 */
	public char *
prutfchar(ch)
	LWCHAR ch;
{
	static char buf[32];

	if (ch == ESC)
		strcpy(buf, "ESC");
  	else if (ch < 128 && control_char(ch))
	{
		if (!control_char(ch ^ 0100))
			SNPRINTF1(buf, sizeof(buf), "^%c", ((char) ch) ^ 0100);
		else
			SNPRINTF1(buf, sizeof(buf), binfmt, (char) ch);
	} else if (is_ubin_char(ch))
		SNPRINTF1(buf, sizeof(buf), utfbinfmt, ch);
	else
	{
		int len;
		if (ch >= 0x80000000)
		{
			len = 3;
			ch = 0xFFFD;
		} else
		{
			len =   (ch < 0x80) ? 1
			      : (ch < 0x800) ? 2
			      : (ch < 0x10000) ? 3
			      : (ch < 0x200000) ? 4
			      : (ch < 0x4000000) ? 5
			      : 6;
		}
		buf[len] = '\0';
		if (len == 1)
			*buf = (char) ch;
		else
		{
			*buf = ((1 << len) - 1) << (8 - len);
			while (--len > 0)
			{
				buf[len] = (char) (0x80 | (ch & 0x3F));
				ch >>= 6;
			}
			*buf |= ch;
		}
	}
	return (buf);
}

/*
 * Get the length of a UTF-8 character in bytes.
 */
	public int
utf_len(ch)
	char ch;
{
	if ((ch & 0x80) == 0)
		return 1;
	if ((ch & 0xE0) == 0xC0)
		return 2;
	if ((ch & 0xF0) == 0xE0)
		return 3;
	if ((ch & 0xF8) == 0xF0)
		return 4;
	if ((ch & 0xFC) == 0xF8)
		return 5;
	if ((ch & 0xFE) == 0xFC)
		return 6;
	/* Invalid UTF-8 encoding. */
	return 1;
}

/*
 * Is a UTF-8 character well-formed?
 */
	public int
is_utf8_well_formed(s)
	unsigned char *s;
{
	int i;
	int len;

	if (IS_UTF8_INVALID(s[0]))
		return (0);

	len = utf_len((char) s[0]);
	if (len == 1)
		return (1);
	if (len == 2)
	{
		if (s[0] < 0xC2)
		    return (0);
	} else
	{
		unsigned char mask;
		mask = (~((1 << (8-len)) - 1)) & 0xFF;
		if (s[0] == mask && (s[1] & mask) == 0x80)
			return (0);
	}

	for (i = 1;  i < len;  i++)
		if (!IS_UTF8_TRAIL(s[i]))
			return (0);
	return (1);
}

/*
 * Get the value of a UTF-8 character.
 */
	public LWCHAR
get_wchar(p)
	char *p;
{
	switch (utf_len(p[0]))
	{
	case 1:
	default:
		return (LWCHAR)
			(p[0] & 0xFF);
	case 2:
		return (LWCHAR) (
			((p[0] & 0x1F) << 6) |
			(p[1] & 0x3F));
	case 3:
		return (LWCHAR) (
			((p[0] & 0x0F) << 12) |
			((p[1] & 0x3F) << 6) |
			(p[2] & 0x3F));
	case 4:
		return (LWCHAR) (
			((p[0] & 0x07) << 18) |
			((p[1] & 0x3F) << 12) | 
			((p[2] & 0x3F) << 6) | 
			(p[3] & 0x3F));
	case 5:
		return (LWCHAR) (
			((p[0] & 0x03) << 24) |
			((p[1] & 0x3F) << 18) | 
			((p[2] & 0x3F) << 12) | 
			((p[3] & 0x3F) << 6) | 
			(p[4] & 0x3F));
	case 6:
		return (LWCHAR) (
			((p[0] & 0x01) << 30) |
			((p[1] & 0x3F) << 24) | 
			((p[2] & 0x3F) << 18) | 
			((p[3] & 0x3F) << 12) | 
			((p[4] & 0x3F) << 6) | 
			(p[5] & 0x3F));
	}
}

/*
 * Step forward or backward one character in a string.
 */
	public LWCHAR
step_char(pp, dir, limit)
	char **pp;
	signed int dir;
	char *limit;
{
	LWCHAR ch;
	char *p = *pp;

	if (!utf_mode)
	{
		/* It's easy if chars are one byte. */
		if (dir > 0)
			ch = (LWCHAR) ((p < limit) ? *p++ : 0);
		else
			ch = (LWCHAR) ((p > limit) ? *--p : 0);
	} else if (dir > 0)
	{
		if (p + utf_len(*p) > limit)
			ch = 0;
		else
		{
			ch = get_wchar(p);
			p++;
			while (IS_UTF8_TRAIL(*p))
				p++;
		}
	} else
	{
		while (p > limit && IS_UTF8_TRAIL(p[-1]))
			p--;
		if (p > limit)
			ch = get_wchar(--p);
		else
			ch = 0;
	}
	*pp = p;
	return ch;
}

/*
 * Unicode characters data
 */
struct wchar_range { LWCHAR first, last; };

/*
 * Characters with general category values
 *	Mn: Mark, Nonspacing
 *	Me: Mark, Enclosing
 * Last synched with
 *	<http://www.unicode.org/Public/5.0.0/ucd/UnicodeData-5.0.0d7.txt>
 *	dated 2005-11-30T00:58:48Z
 */
static struct wchar_range comp_table[] = {
	{  0x0300,  0x036F} /* Mn */, {  0x0483,  0x0486} /* Mn */,
	{  0x0488,  0x0489} /* Me */,
	{  0x0591,  0x05BD} /* Mn */, {  0x05BF,  0x05BF} /* Mn */,
	{  0x05C1,  0x05C2} /* Mn */, {  0x05C4,  0x05C5} /* Mn */,
	{  0x05C7,  0x05C7} /* Mn */, {  0x0610,  0x0615} /* Mn */,
	{  0x064B,  0x065E} /* Mn */, {  0x0670,  0x0670} /* Mn */,
	{  0x06D6,  0x06DC} /* Mn */,
	{  0x06DE,  0x06DE} /* Me */,
	{  0x06DF,  0x06E4} /* Mn */, {  0x06E7,  0x06E8} /* Mn */,
	{  0x06EA,  0x06ED} /* Mn */, {  0x0711,  0x0711} /* Mn */,
	{  0x0730,  0x074A} /* Mn */, {  0x07A6,  0x07B0} /* Mn */,
	{  0x07EB,  0x07F3} /* Mn */, {  0x0901,  0x0902} /* Mn */,
	{  0x093C,  0x093C} /* Mn */, {  0x0941,  0x0948} /* Mn */,
	{  0x094D,  0x094D} /* Mn */, {  0x0951,  0x0954} /* Mn */,
	{  0x0962,  0x0963} /* Mn */, {  0x0981,  0x0981} /* Mn */,
	{  0x09BC,  0x09BC} /* Mn */, {  0x09C1,  0x09C4} /* Mn */,
	{  0x09CD,  0x09CD} /* Mn */, {  0x09E2,  0x09E3} /* Mn */,
	{  0x0A01,  0x0A02} /* Mn */, {  0x0A3C,  0x0A3C} /* Mn */,
	{  0x0A41,  0x0A42} /* Mn */, {  0x0A47,  0x0A48} /* Mn */,
	{  0x0A4B,  0x0A4D} /* Mn */, {  0x0A70,  0x0A71} /* Mn */,
	{  0x0A81,  0x0A82} /* Mn */, {  0x0ABC,  0x0ABC} /* Mn */,
	{  0x0AC1,  0x0AC5} /* Mn */, {  0x0AC7,  0x0AC8} /* Mn */,
	{  0x0ACD,  0x0ACD} /* Mn */, {  0x0AE2,  0x0AE3} /* Mn */,
	{  0x0B01,  0x0B01} /* Mn */, {  0x0B3C,  0x0B3C} /* Mn */,
	{  0x0B3F,  0x0B3F} /* Mn */, {  0x0B41,  0x0B43} /* Mn */,
	{  0x0B4D,  0x0B4D} /* Mn */, {  0x0B56,  0x0B56} /* Mn */,
	{  0x0B82,  0x0B82} /* Mn */, {  0x0BC0,  0x0BC0} /* Mn */,
	{  0x0BCD,  0x0BCD} /* Mn */, {  0x0C3E,  0x0C40} /* Mn */,
	{  0x0C46,  0x0C48} /* Mn */, {  0x0C4A,  0x0C4D} /* Mn */,
	{  0x0C55,  0x0C56} /* Mn */, {  0x0CBC,  0x0CBC} /* Mn */,
	{  0x0CBF,  0x0CBF} /* Mn */, {  0x0CC6,  0x0CC6} /* Mn */,
	{  0x0CCC,  0x0CCD} /* Mn */, {  0x0CE2,  0x0CE3} /* Mn */,
	{  0x0D41,  0x0D43} /* Mn */, {  0x0D4D,  0x0D4D} /* Mn */,
	{  0x0DCA,  0x0DCA} /* Mn */, {  0x0DD2,  0x0DD4} /* Mn */,
	{  0x0DD6,  0x0DD6} /* Mn */, {  0x0E31,  0x0E31} /* Mn */,
	{  0x0E34,  0x0E3A} /* Mn */, {  0x0E47,  0x0E4E} /* Mn */,
	{  0x0EB1,  0x0EB1} /* Mn */, {  0x0EB4,  0x0EB9} /* Mn */,
	{  0x0EBB,  0x0EBC} /* Mn */, {  0x0EC8,  0x0ECD} /* Mn */,
	{  0x0F18,  0x0F19} /* Mn */, {  0x0F35,  0x0F35} /* Mn */,
	{  0x0F37,  0x0F37} /* Mn */, {  0x0F39,  0x0F39} /* Mn */,
	{  0x0F71,  0x0F7E} /* Mn */, {  0x0F80,  0x0F84} /* Mn */,
	{  0x0F86,  0x0F87} /* Mn */, {  0x0F90,  0x0F97} /* Mn */,
	{  0x0F99,  0x0FBC} /* Mn */, {  0x0FC6,  0x0FC6} /* Mn */,
	{  0x102D,  0x1030} /* Mn */, {  0x1032,  0x1032} /* Mn */,
	{  0x1036,  0x1037} /* Mn */, {  0x1039,  0x1039} /* Mn */,
	{  0x1058,  0x1059} /* Mn */, {  0x135F,  0x135F} /* Mn */,
	{  0x1712,  0x1714} /* Mn */, {  0x1732,  0x1734} /* Mn */,
	{  0x1752,  0x1753} /* Mn */, {  0x1772,  0x1773} /* Mn */,
	{  0x17B7,  0x17BD} /* Mn */, {  0x17C6,  0x17C6} /* Mn */,
	{  0x17C9,  0x17D3} /* Mn */, {  0x17DD,  0x17DD} /* Mn */,
	{  0x180B,  0x180D} /* Mn */, {  0x18A9,  0x18A9} /* Mn */,
	{  0x1920,  0x1922} /* Mn */, {  0x1927,  0x1928} /* Mn */,
	{  0x1932,  0x1932} /* Mn */, {  0x1939,  0x193B} /* Mn */,
	{  0x1A17,  0x1A18} /* Mn */, {  0x1B00,  0x1B03} /* Mn */,
	{  0x1B34,  0x1B34} /* Mn */, {  0x1B36,  0x1B3A} /* Mn */,
	{  0x1B3C,  0x1B3C} /* Mn */, {  0x1B42,  0x1B42} /* Mn */,
	{  0x1B6B,  0x1B73} /* Mn */, {  0x1DC0,  0x1DCA} /* Mn */,
	{  0x1DFE,  0x1DFF} /* Mn */, {  0x20D0,  0x20DC} /* Mn */,
	{  0x20DD,  0x20E0} /* Me */,
	{  0x20E1,  0x20E1} /* Mn */,
	{  0x20E2,  0x20E4} /* Me */,
	{  0x20E5,  0x20EF} /* Mn */, {  0x302A,  0x302F} /* Mn */,
	{  0x3099,  0x309A} /* Mn */, {  0xA806,  0xA806} /* Mn */,
	{  0xA80B,  0xA80B} /* Mn */, {  0xA825,  0xA826} /* Mn */,
	{  0xFB1E,  0xFB1E} /* Mn */, {  0xFE00,  0xFE0F} /* Mn */,
	{  0xFE20,  0xFE23} /* Mn */, { 0x10A01, 0x10A03} /* Mn */,
	{ 0x10A05, 0x10A06} /* Mn */, { 0x10A0C, 0x10A0F} /* Mn */,
	{ 0x10A38, 0x10A3A} /* Mn */, { 0x10A3F, 0x10A3F} /* Mn */,
	{ 0x1D167, 0x1D169} /* Mn */, { 0x1D17B, 0x1D182} /* Mn */,
	{ 0x1D185, 0x1D18B} /* Mn */, { 0x1D1AA, 0x1D1AD} /* Mn */,
	{ 0x1D242, 0x1D244} /* Mn */, { 0xE0100, 0xE01EF} /* Mn */,
};

/*
 * Special pairs, not ranges.
 */
static struct wchar_range comb_table[] = {
	{0x0644,0x0622}, {0x0644,0x0623}, {0x0644,0x0625}, {0x0644,0x0627},
};

/*
 * Characters with general category values
 *	Cc: Other, Control
 *	Cf: Other, Format
 *	Cs: Other, Surrogate
 *	Co: Other, Private Use
 *	Cn: Other, Not Assigned
 *	Zl: Separator, Line
 *	Zp: Separator, Paragraph
 * Last synched with
 *	<http://www.unicode.org/Public/5.0.0/ucd/UnicodeData-5.0.0d7.txt>
 *	dated 2005-11-30T00:58:48Z
 */
static struct wchar_range ubin_table[] = {
	{  0x0000,  0x001F} /* Cc */, {  0x007F,  0x009F} /* Cc */,
#if 0
	{  0x00AD,  0x00AD} /* Cf */,
#endif
	{  0x0370,  0x0373} /* Cn */, {  0x0376,  0x0379} /* Cn */,
	{  0x037F,  0x0383} /* Cn */, {  0x038B,  0x038B} /* Cn */,
	{  0x038D,  0x038D} /* Cn */, {  0x03A2,  0x03A2} /* Cn */,
	{  0x03CF,  0x03CF} /* Cn */, {  0x0487,  0x0487} /* Cn */,
	{  0x0514,  0x0530} /* Cn */, {  0x0557,  0x0558} /* Cn */,
	{  0x0560,  0x0560} /* Cn */, {  0x0588,  0x0588} /* Cn */,
	{  0x058B,  0x0590} /* Cn */, {  0x05C8,  0x05CF} /* Cn */,
	{  0x05EB,  0x05EF} /* Cn */, {  0x05F5,  0x05FF} /* Cn */,
#if 0
	{  0x0600,  0x0603} /* Cf */,
#endif
	{  0x0604,  0x060A} /* Cn */, {  0x0616,  0x061A} /* Cn */,
	{  0x061C,  0x061D} /* Cn */, {  0x0620,  0x0620} /* Cn */,
	{  0x063B,  0x063F} /* Cn */, {  0x065F,  0x065F} /* Cn */,
#if 0
	{  0x06DD,  0x06DD} /* Cf */,
#endif
	{  0x070E,  0x070E} /* Cn */,
#if 0
	{  0x070F,  0x070F} /* Cf */,
#endif
	{  0x074B,  0x074C} /* Cn */, {  0x076E,  0x077F} /* Cn */,
	{  0x07B2,  0x07BF} /* Cn */, {  0x07FB,  0x0900} /* Cn */,
	{  0x093A,  0x093B} /* Cn */, {  0x094E,  0x094F} /* Cn */,
	{  0x0955,  0x0957} /* Cn */, {  0x0971,  0x097A} /* Cn */,
	{  0x0980,  0x0980} /* Cn */, {  0x0984,  0x0984} /* Cn */,
	{  0x098D,  0x098E} /* Cn */, {  0x0991,  0x0992} /* Cn */,
	{  0x09A9,  0x09A9} /* Cn */, {  0x09B1,  0x09B1} /* Cn */,
	{  0x09B3,  0x09B5} /* Cn */, {  0x09BA,  0x09BB} /* Cn */,
	{  0x09C5,  0x09C6} /* Cn */, {  0x09C9,  0x09CA} /* Cn */,
	{  0x09CF,  0x09D6} /* Cn */, {  0x09D8,  0x09DB} /* Cn */,
	{  0x09DE,  0x09DE} /* Cn */, {  0x09E4,  0x09E5} /* Cn */,
	{  0x09FB,  0x0A00} /* Cn */, {  0x0A04,  0x0A04} /* Cn */,
	{  0x0A0B,  0x0A0E} /* Cn */, {  0x0A11,  0x0A12} /* Cn */,
	{  0x0A29,  0x0A29} /* Cn */, {  0x0A31,  0x0A31} /* Cn */,
	{  0x0A34,  0x0A34} /* Cn */, {  0x0A37,  0x0A37} /* Cn */,
	{  0x0A3A,  0x0A3B} /* Cn */, {  0x0A3D,  0x0A3D} /* Cn */,
	{  0x0A43,  0x0A46} /* Cn */, {  0x0A49,  0x0A4A} /* Cn */,
	{  0x0A4E,  0x0A58} /* Cn */, {  0x0A5D,  0x0A5D} /* Cn */,
	{  0x0A5F,  0x0A65} /* Cn */, {  0x0A75,  0x0A80} /* Cn */,
	{  0x0A84,  0x0A84} /* Cn */, {  0x0A8E,  0x0A8E} /* Cn */,
	{  0x0A92,  0x0A92} /* Cn */, {  0x0AA9,  0x0AA9} /* Cn */,
	{  0x0AB1,  0x0AB1} /* Cn */, {  0x0AB4,  0x0AB4} /* Cn */,
	{  0x0ABA,  0x0ABB} /* Cn */, {  0x0AC6,  0x0AC6} /* Cn */,
	{  0x0ACA,  0x0ACA} /* Cn */, {  0x0ACE,  0x0ACF} /* Cn */,
	{  0x0AD1,  0x0ADF} /* Cn */, {  0x0AE4,  0x0AE5} /* Cn */,
	{  0x0AF0,  0x0AF0} /* Cn */, {  0x0AF2,  0x0B00} /* Cn */,
	{  0x0B04,  0x0B04} /* Cn */, {  0x0B0D,  0x0B0E} /* Cn */,
	{  0x0B11,  0x0B12} /* Cn */, {  0x0B29,  0x0B29} /* Cn */,
	{  0x0B31,  0x0B31} /* Cn */, {  0x0B34,  0x0B34} /* Cn */,
	{  0x0B3A,  0x0B3B} /* Cn */, {  0x0B44,  0x0B46} /* Cn */,
	{  0x0B49,  0x0B4A} /* Cn */, {  0x0B4E,  0x0B55} /* Cn */,
	{  0x0B58,  0x0B5B} /* Cn */, {  0x0B5E,  0x0B5E} /* Cn */,
	{  0x0B62,  0x0B65} /* Cn */, {  0x0B72,  0x0B81} /* Cn */,
	{  0x0B84,  0x0B84} /* Cn */, {  0x0B8B,  0x0B8D} /* Cn */,
	{  0x0B91,  0x0B91} /* Cn */, {  0x0B96,  0x0B98} /* Cn */,
	{  0x0B9B,  0x0B9B} /* Cn */, {  0x0B9D,  0x0B9D} /* Cn */,
	{  0x0BA0,  0x0BA2} /* Cn */, {  0x0BA5,  0x0BA7} /* Cn */,
	{  0x0BAB,  0x0BAD} /* Cn */, {  0x0BBA,  0x0BBD} /* Cn */,
	{  0x0BC3,  0x0BC5} /* Cn */, {  0x0BC9,  0x0BC9} /* Cn */,
	{  0x0BCE,  0x0BD6} /* Cn */, {  0x0BD8,  0x0BE5} /* Cn */,
	{  0x0BFB,  0x0C00} /* Cn */, {  0x0C04,  0x0C04} /* Cn */,
	{  0x0C0D,  0x0C0D} /* Cn */, {  0x0C11,  0x0C11} /* Cn */,
	{  0x0C29,  0x0C29} /* Cn */, {  0x0C34,  0x0C34} /* Cn */,
	{  0x0C3A,  0x0C3D} /* Cn */, {  0x0C45,  0x0C45} /* Cn */,
	{  0x0C49,  0x0C49} /* Cn */, {  0x0C4E,  0x0C54} /* Cn */,
	{  0x0C57,  0x0C5F} /* Cn */, {  0x0C62,  0x0C65} /* Cn */,
	{  0x0C70,  0x0C81} /* Cn */, {  0x0C84,  0x0C84} /* Cn */,
	{  0x0C8D,  0x0C8D} /* Cn */, {  0x0C91,  0x0C91} /* Cn */,
	{  0x0CA9,  0x0CA9} /* Cn */, {  0x0CB4,  0x0CB4} /* Cn */,
	{  0x0CBA,  0x0CBB} /* Cn */, {  0x0CC5,  0x0CC5} /* Cn */,
	{  0x0CC9,  0x0CC9} /* Cn */, {  0x0CCE,  0x0CD4} /* Cn */,
	{  0x0CD7,  0x0CDD} /* Cn */, {  0x0CDF,  0x0CDF} /* Cn */,
	{  0x0CE4,  0x0CE5} /* Cn */, {  0x0CF0,  0x0CF0} /* Cn */,
	{  0x0CF3,  0x0D01} /* Cn */, {  0x0D04,  0x0D04} /* Cn */,
	{  0x0D0D,  0x0D0D} /* Cn */, {  0x0D11,  0x0D11} /* Cn */,
	{  0x0D29,  0x0D29} /* Cn */, {  0x0D3A,  0x0D3D} /* Cn */,
	{  0x0D44,  0x0D45} /* Cn */, {  0x0D49,  0x0D49} /* Cn */,
	{  0x0D4E,  0x0D56} /* Cn */, {  0x0D58,  0x0D5F} /* Cn */,
	{  0x0D62,  0x0D65} /* Cn */, {  0x0D70,  0x0D81} /* Cn */,
	{  0x0D84,  0x0D84} /* Cn */, {  0x0D97,  0x0D99} /* Cn */,
	{  0x0DB2,  0x0DB2} /* Cn */, {  0x0DBC,  0x0DBC} /* Cn */,
	{  0x0DBE,  0x0DBF} /* Cn */, {  0x0DC7,  0x0DC9} /* Cn */,
	{  0x0DCB,  0x0DCE} /* Cn */, {  0x0DD5,  0x0DD5} /* Cn */,
	{  0x0DD7,  0x0DD7} /* Cn */, {  0x0DE0,  0x0DF1} /* Cn */,
	{  0x0DF5,  0x0E00} /* Cn */, {  0x0E3B,  0x0E3E} /* Cn */,
	{  0x0E5C,  0x0E80} /* Cn */, {  0x0E83,  0x0E83} /* Cn */,
	{  0x0E85,  0x0E86} /* Cn */, {  0x0E89,  0x0E89} /* Cn */,
	{  0x0E8B,  0x0E8C} /* Cn */, {  0x0E8E,  0x0E93} /* Cn */,
	{  0x0E98,  0x0E98} /* Cn */, {  0x0EA0,  0x0EA0} /* Cn */,
	{  0x0EA4,  0x0EA4} /* Cn */, {  0x0EA6,  0x0EA6} /* Cn */,
	{  0x0EA8,  0x0EA9} /* Cn */, {  0x0EAC,  0x0EAC} /* Cn */,
	{  0x0EBA,  0x0EBA} /* Cn */, {  0x0EBE,  0x0EBF} /* Cn */,
	{  0x0EC5,  0x0EC5} /* Cn */, {  0x0EC7,  0x0EC7} /* Cn */,
	{  0x0ECE,  0x0ECF} /* Cn */, {  0x0EDA,  0x0EDB} /* Cn */,
	{  0x0EDE,  0x0EFF} /* Cn */, {  0x0F48,  0x0F48} /* Cn */,
	{  0x0F6B,  0x0F70} /* Cn */, {  0x0F8C,  0x0F8F} /* Cn */,
	{  0x0F98,  0x0F98} /* Cn */, {  0x0FBD,  0x0FBD} /* Cn */,
	{  0x0FCD,  0x0FCE} /* Cn */, {  0x0FD2,  0x0FFF} /* Cn */,
	{  0x1022,  0x1022} /* Cn */, {  0x1028,  0x1028} /* Cn */,
	{  0x102B,  0x102B} /* Cn */, {  0x1033,  0x1035} /* Cn */,
	{  0x103A,  0x103F} /* Cn */, {  0x105A,  0x109F} /* Cn */,
	{  0x10C6,  0x10CF} /* Cn */, {  0x10FD,  0x10FF} /* Cn */,
	{  0x115A,  0x115E} /* Cn */, {  0x11A3,  0x11A7} /* Cn */,
	{  0x11FA,  0x11FF} /* Cn */, {  0x1249,  0x1249} /* Cn */,
	{  0x124E,  0x124F} /* Cn */, {  0x1257,  0x1257} /* Cn */,
	{  0x1259,  0x1259} /* Cn */, {  0x125E,  0x125F} /* Cn */,
	{  0x1289,  0x1289} /* Cn */, {  0x128E,  0x128F} /* Cn */,
	{  0x12B1,  0x12B1} /* Cn */, {  0x12B6,  0x12B7} /* Cn */,
	{  0x12BF,  0x12BF} /* Cn */, {  0x12C1,  0x12C1} /* Cn */,
	{  0x12C6,  0x12C7} /* Cn */, {  0x12D7,  0x12D7} /* Cn */,
	{  0x1311,  0x1311} /* Cn */, {  0x1316,  0x1317} /* Cn */,
	{  0x135B,  0x135E} /* Cn */, {  0x137D,  0x137F} /* Cn */,
	{  0x139A,  0x139F} /* Cn */, {  0x13F5,  0x1400} /* Cn */,
	{  0x1677,  0x167F} /* Cn */, {  0x169D,  0x169F} /* Cn */,
	{  0x16F1,  0x16FF} /* Cn */, {  0x170D,  0x170D} /* Cn */,
	{  0x1715,  0x171F} /* Cn */, {  0x1737,  0x173F} /* Cn */,
	{  0x1754,  0x175F} /* Cn */, {  0x176D,  0x176D} /* Cn */,
	{  0x1771,  0x1771} /* Cn */, {  0x1774,  0x177F} /* Cn */,
#if 0
	{  0x17B4,  0x17B5} /* Cf */,
#endif
	{  0x17DE,  0x17DF} /* Cn */, {  0x17EA,  0x17EF} /* Cn */,
	{  0x17FA,  0x17FF} /* Cn */, {  0x180F,  0x180F} /* Cn */,
	{  0x181A,  0x181F} /* Cn */, {  0x1878,  0x187F} /* Cn */,
	{  0x18AA,  0x18FF} /* Cn */, {  0x191D,  0x191F} /* Cn */,
	{  0x192C,  0x192F} /* Cn */, {  0x193C,  0x193F} /* Cn */,
	{  0x1941,  0x1943} /* Cn */, {  0x196E,  0x196F} /* Cn */,
	{  0x1975,  0x197F} /* Cn */, {  0x19AA,  0x19AF} /* Cn */,
	{  0x19CA,  0x19CF} /* Cn */, {  0x19DA,  0x19DD} /* Cn */,
	{  0x1A1C,  0x1A1D} /* Cn */, {  0x1A20,  0x1AFF} /* Cn */,
	{  0x1B4C,  0x1B4F} /* Cn */, {  0x1B7D,  0x1CFF} /* Cn */,
	{  0x1DCB,  0x1DFD} /* Cn */, {  0x1E9C,  0x1E9F} /* Cn */,
	{  0x1EFA,  0x1EFF} /* Cn */, {  0x1F16,  0x1F17} /* Cn */,
	{  0x1F1E,  0x1F1F} /* Cn */, {  0x1F46,  0x1F47} /* Cn */,
	{  0x1F4E,  0x1F4F} /* Cn */, {  0x1F58,  0x1F58} /* Cn */,
	{  0x1F5A,  0x1F5A} /* Cn */, {  0x1F5C,  0x1F5C} /* Cn */,
	{  0x1F5E,  0x1F5E} /* Cn */, {  0x1F7E,  0x1F7F} /* Cn */,
	{  0x1FB5,  0x1FB5} /* Cn */, {  0x1FC5,  0x1FC5} /* Cn */,
	{  0x1FD4,  0x1FD5} /* Cn */, {  0x1FDC,  0x1FDC} /* Cn */,
	{  0x1FF0,  0x1FF1} /* Cn */, {  0x1FF5,  0x1FF5} /* Cn */,
	{  0x1FFF,  0x1FFF} /* Cn */,
	{  0x200B,  0x200F} /* Cf */,
	{  0x2028,  0x2028} /* Zl */,
	{  0x2029,  0x2029} /* Zp */,
	{  0x202A,  0x202E} /* Cf */,
	{  0x2060,  0x2063} /* Cf */,
	{  0x2064,  0x2069} /* Cn */,
	{  0x206A,  0x206F} /* Cf */,
	{  0x2072,  0x2073} /* Cn */, {  0x208F,  0x208F} /* Cn */,
	{  0x2095,  0x209F} /* Cn */, {  0x20B6,  0x20CF} /* Cn */,
	{  0x20F0,  0x20FF} /* Cn */, {  0x214F,  0x2152} /* Cn */,
	{  0x2185,  0x218F} /* Cn */, {  0x23E8,  0x23FF} /* Cn */,
	{  0x2427,  0x243F} /* Cn */, {  0x244B,  0x245F} /* Cn */,
	{  0x269D,  0x269F} /* Cn */, {  0x26B3,  0x2700} /* Cn */,
	{  0x2705,  0x2705} /* Cn */, {  0x270A,  0x270B} /* Cn */,
	{  0x2728,  0x2728} /* Cn */, {  0x274C,  0x274C} /* Cn */,
	{  0x274E,  0x274E} /* Cn */, {  0x2753,  0x2755} /* Cn */,
	{  0x2757,  0x2757} /* Cn */, {  0x275F,  0x2760} /* Cn */,
	{  0x2795,  0x2797} /* Cn */, {  0x27B0,  0x27B0} /* Cn */,
	{  0x27BF,  0x27BF} /* Cn */, {  0x27CB,  0x27CF} /* Cn */,
	{  0x27EC,  0x27EF} /* Cn */, {  0x2B1B,  0x2B1F} /* Cn */,
	{  0x2B24,  0x2BFF} /* Cn */, {  0x2C2F,  0x2C2F} /* Cn */,
	{  0x2C5F,  0x2C5F} /* Cn */, {  0x2C6D,  0x2C73} /* Cn */,
	{  0x2C78,  0x2C7F} /* Cn */, {  0x2CEB,  0x2CF8} /* Cn */,
	{  0x2D26,  0x2D2F} /* Cn */, {  0x2D66,  0x2D6E} /* Cn */,
	{  0x2D70,  0x2D7F} /* Cn */, {  0x2D97,  0x2D9F} /* Cn */,
	{  0x2DA7,  0x2DA7} /* Cn */, {  0x2DAF,  0x2DAF} /* Cn */,
	{  0x2DB7,  0x2DB7} /* Cn */, {  0x2DBF,  0x2DBF} /* Cn */,
	{  0x2DC7,  0x2DC7} /* Cn */, {  0x2DCF,  0x2DCF} /* Cn */,
	{  0x2DD7,  0x2DD7} /* Cn */, {  0x2DDF,  0x2DFF} /* Cn */,
	{  0x2E18,  0x2E1B} /* Cn */, {  0x2E1E,  0x2E7F} /* Cn */,
	{  0x2E9A,  0x2E9A} /* Cn */, {  0x2EF4,  0x2EFF} /* Cn */,
	{  0x2FD6,  0x2FEF} /* Cn */, {  0x2FFC,  0x2FFF} /* Cn */,
	{  0x3040,  0x3040} /* Cn */, {  0x3097,  0x3098} /* Cn */,
	{  0x3100,  0x3104} /* Cn */, {  0x312D,  0x3130} /* Cn */,
	{  0x318F,  0x318F} /* Cn */, {  0x31B8,  0x31BF} /* Cn */,
	{  0x31D0,  0x31EF} /* Cn */, {  0x321F,  0x321F} /* Cn */,
	{  0x3244,  0x324F} /* Cn */, {  0x32FF,  0x32FF} /* Cn */,
	{  0x4DB6,  0x4DBF} /* Cn */, {  0x9FBC,  0x9FFF} /* Cn */,
	{  0xA48D,  0xA48F} /* Cn */, {  0xA4C7,  0xA6FF} /* Cn */,
	{  0xA71B,  0xA71F} /* Cn */, {  0xA722,  0xA7FF} /* Cn */,
	{  0xA82C,  0xA83F} /* Cn */, {  0xA878,  0xABFF} /* Cn */,
	{  0xD7A4,  0xD7FF} /* Cn */,
	{  0xD800,  0xDFFF} /* Cs */,
	{  0xE000,  0xF8FF} /* Co */,
	{  0xFA2E,  0xFA2F} /* Cn */, {  0xFA6B,  0xFA6F} /* Cn */,
	{  0xFADA,  0xFAFF} /* Cn */, {  0xFB07,  0xFB12} /* Cn */,
	{  0xFB18,  0xFB1C} /* Cn */, {  0xFB37,  0xFB37} /* Cn */,
	{  0xFB3D,  0xFB3D} /* Cn */, {  0xFB3F,  0xFB3F} /* Cn */,
	{  0xFB42,  0xFB42} /* Cn */, {  0xFB45,  0xFB45} /* Cn */,
	{  0xFBB2,  0xFBD2} /* Cn */, {  0xFD40,  0xFD4F} /* Cn */,
	{  0xFD90,  0xFD91} /* Cn */, {  0xFDC8,  0xFDEF} /* Cn */,
	{  0xFDFE,  0xFDFF} /* Cn */, {  0xFE1A,  0xFE1F} /* Cn */,
	{  0xFE24,  0xFE2F} /* Cn */, {  0xFE53,  0xFE53} /* Cn */,
	{  0xFE67,  0xFE67} /* Cn */, {  0xFE6C,  0xFE6F} /* Cn */,
	{  0xFE75,  0xFE75} /* Cn */, {  0xFEFD,  0xFEFE} /* Cn */,
	{  0xFEFF,  0xFEFF} /* Cf */,
	{  0xFF00,  0xFF00} /* Cn */, {  0xFFBF,  0xFFC1} /* Cn */,
	{  0xFFC8,  0xFFC9} /* Cn */, {  0xFFD0,  0xFFD1} /* Cn */,
	{  0xFFD8,  0xFFD9} /* Cn */, {  0xFFDD,  0xFFDF} /* Cn */,
	{  0xFFE7,  0xFFE7} /* Cn */, {  0xFFEF,  0xFFF8} /* Cn */,
	{  0xFFF9,  0xFFFB} /* Cf */,
	{  0xFFFE,  0xFFFF} /* Cn */, { 0x1000C, 0x1000C} /* Cn */,
	{ 0x10027, 0x10027} /* Cn */, { 0x1003B, 0x1003B} /* Cn */,
	{ 0x1003E, 0x1003E} /* Cn */, { 0x1004E, 0x1004F} /* Cn */,
	{ 0x1005E, 0x1007F} /* Cn */, { 0x100FB, 0x100FF} /* Cn */,
	{ 0x10103, 0x10106} /* Cn */, { 0x10134, 0x10136} /* Cn */,
	{ 0x1018B, 0x102FF} /* Cn */, { 0x1031F, 0x1031F} /* Cn */,
	{ 0x10324, 0x1032F} /* Cn */, { 0x1034B, 0x1037F} /* Cn */,
	{ 0x1039E, 0x1039E} /* Cn */, { 0x103C4, 0x103C7} /* Cn */,
	{ 0x103D6, 0x103FF} /* Cn */,
	{ 0x1049E, 0x1049F} /* Cn */, { 0x104AA, 0x107FF} /* Cn */,
	{ 0x10806, 0x10807} /* Cn */, { 0x10809, 0x10809} /* Cn */,
	{ 0x10836, 0x10836} /* Cn */, { 0x10839, 0x1083B} /* Cn */,
	{ 0x1083D, 0x1083E} /* Cn */, { 0x10840, 0x108FF} /* Cn */,
	{ 0x1091A, 0x1091E} /* Cn */, { 0x10920, 0x109FF} /* Cn */,
	{ 0x10A04, 0x10A04} /* Cn */, { 0x10A07, 0x10A0B} /* Cn */,
	{ 0x10A14, 0x10A14} /* Cn */, { 0x10A18, 0x10A18} /* Cn */,
	{ 0x10A34, 0x10A37} /* Cn */, { 0x10A3B, 0x10A3E} /* Cn */,
	{ 0x10A48, 0x10A4F} /* Cn */, { 0x10A59, 0x11FFF} /* Cn */,
	{ 0x1236F, 0x123FF} /* Cn */, { 0x12463, 0x1246F} /* Cn */,
	{ 0x12474, 0x1CFFF} /* Cn */, { 0x1D0F6, 0x1D0FF} /* Cn */,
	{ 0x1D127, 0x1D129} /* Cn */,
	{ 0x1D173, 0x1D17A} /* Cf */,
	{ 0x1D1DE, 0x1D1FF} /* Cn */, { 0x1D246, 0x1D2FF} /* Cn */,
	{ 0x1D357, 0x1D35F} /* Cn */, { 0x1D372, 0x1D3FF} /* Cn */,
	{ 0x1D455, 0x1D455} /* Cn */, { 0x1D49D, 0x1D49D} /* Cn */,
	{ 0x1D4A0, 0x1D4A1} /* Cn */, { 0x1D4A3, 0x1D4A4} /* Cn */,
	{ 0x1D4A7, 0x1D4A8} /* Cn */, { 0x1D4AD, 0x1D4AD} /* Cn */,
	{ 0x1D4BA, 0x1D4BA} /* Cn */, { 0x1D4BC, 0x1D4BC} /* Cn */,
	{ 0x1D4C4, 0x1D4C4} /* Cn */, { 0x1D506, 0x1D506} /* Cn */,
	{ 0x1D50B, 0x1D50C} /* Cn */, { 0x1D515, 0x1D515} /* Cn */,
	{ 0x1D51D, 0x1D51D} /* Cn */, { 0x1D53A, 0x1D53A} /* Cn */,
	{ 0x1D53F, 0x1D53F} /* Cn */, { 0x1D545, 0x1D545} /* Cn */,
	{ 0x1D547, 0x1D549} /* Cn */, { 0x1D551, 0x1D551} /* Cn */,
	{ 0x1D6A6, 0x1D6A7} /* Cn */, { 0x1D7CC, 0x1D7CD} /* Cn */,
	{ 0x1D800, 0x1FFFF} /* Cn */, { 0x2A6D7, 0x2F7FF} /* Cn */,
	{ 0x2FA1E, 0xE0000} /* Cn */,
	{ 0xE0001, 0xE0001} /* Cf */,
	{ 0xE0002, 0xE001F} /* Cn */,
	{ 0xE0020, 0xE007F} /* Cf */,
	{ 0xE0080, 0xE00FF} /* Cn */, { 0xE01F0, 0xEFFFF} /* Cn */,
	{ 0xF0000, 0xFFFFD} /* Co */,
	{ 0xFFFFE, 0xFFFFF} /* Cn */,
	{0x100000,0x10FFFD} /* Co */,
	{0x10FFFE,0x10FFFF} /* Cn */,
	{0x110000,0x7FFFFFFF} /* ISO 10646?? */
};

/*
 * Double width characters
 *	W: East Asian Wide
 *	F: East Asian Full-width
 * Unassigned code points may be included when they allow ranges to be merged.
 * Last synched with
 *	<http://www.unicode.org/Public/5.0.0/ucd/EastAsianWidth-5.0.0d2.txt>
 *	dated 2005-11-08T01:32:56Z
 */
static struct wchar_range wide_table[] = {
	{  0x1100,  0x115F} /* W */, {  0x2329,  0x232A} /* W */,
	{  0x2E80,  0x2FFB} /* W */,
	{  0x3000,  0x3000} /* F */,
	{  0x3001,  0x303E} /* W */, {  0x3041,  0x4DB5} /* W */,
	{  0x4E00,  0x9FBB} /* W */, {  0xA000,  0xA4C6} /* W */,
	{  0xAC00,  0xD7A3} /* W */, {  0xF900,  0xFAD9} /* W */,
	{  0xFE10,  0xFE19} /* W */, {  0xFE30,  0xFE6B} /* W */,
	{  0xFF01,  0xFF60} /* F */, {  0xFFE0,  0xFFE6} /* F */,
	{ 0x20000, 0x2FFFD} /* W */, { 0x30000, 0x3FFFD} /* W */,
};

	static int
is_in_table(ch, table, tsize)
	LWCHAR ch;
	struct wchar_range table[];
	int tsize;
{
	int hi;
	int lo;

	/* Binary search in the table. */
	if (ch < table[0].first)
		return 0;
	lo = 0;
	hi = tsize - 1;
	while (lo <= hi)
	{
		int mid = (lo + hi) / 2;
		if (ch > table[mid].last)
			lo = mid + 1;
		else if (ch < table[mid].first)
			hi = mid - 1;
		else
			return 1;
	}
	return 0;
}

/*
 * Is a character a UTF-8 composing character?
 * If a composing character follows any char, the two combine into one glyph.
 */
	public int
is_composing_char(ch)
	LWCHAR ch;
{
	return is_in_table(ch, comp_table, (sizeof(comp_table) / sizeof(*comp_table)));
}

/*
 * Should this UTF-8 character be treated as binary?
 */
	public int
is_ubin_char(ch)
	LWCHAR ch;
{
	return is_in_table(ch, ubin_table, (sizeof(ubin_table) / sizeof(*ubin_table)));
}

/*
 * Is this a double width UTF-8 character?
 */
	public int
is_wide_char(ch)
	LWCHAR ch;
{
	return is_in_table(ch, wide_table, (sizeof(wide_table) / sizeof(*wide_table)));
}

/*
 * Is a character a UTF-8 combining character?
 * A combining char acts like an ordinary char, but if it follows
 * a specific char (not any char), the two combine into one glyph.
 */
	public int
is_combining_char(ch1, ch2)
	LWCHAR ch1;
	LWCHAR ch2;
{
	/* The table is small; use linear search. */
	int i;
	for (i = 0;  i < sizeof(comb_table)/sizeof(*comb_table);  i++)
	{
		if (ch1 == comb_table[i].first &&
		    ch2 == comb_table[i].last)
			return 1;
	}
	return 0;
}

