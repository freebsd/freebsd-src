/*
 * Copyright (C) 1984-2005  Mark Nudelman
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
	/*
	 * Try using the codeset name as the charset name.
	 */
	s = nl_langinfo(CODESET);
	if (icharset(s, 1))
		return;
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

static struct wchar_range comp_table[] = {
	{0x300,0x357}, {0x35d,0x36f}, {0x483,0x486}, {0x488,0x489}, 
	{0x591,0x5a1}, {0x5a3,0x5b9}, {0x5bb,0x5bd}, {0x5bf,0x5bf}, 
	{0x5c1,0x5c2}, {0x5c4,0x5c4}, {0x610,0x615}, {0x64b,0x658}, 
	{0x670,0x670}, {0x6d6,0x6dc}, {0x6de,0x6e4}, {0x6e7,0x6e8}, 
	{0x6ea,0x6ed}, {0x711,0x711}, {0x730,0x74a}, {0x7a6,0x7b0}, 
	{0x901,0x902}, {0x93c,0x93c}, {0x941,0x948}, {0x94d,0x94d}, 
	{0x951,0x954}, {0x962,0x963}, {0x981,0x981}, {0x9bc,0x9bc}, 
	{0x9c1,0x9c4}, {0x9cd,0x9cd}, {0x9e2,0x9e3}, {0xa01,0xa02}, 
	{0xa3c,0xa3c}, {0xa41,0xa42}, {0xa47,0xa48}, {0xa4b,0xa4d}, 
	{0xa70,0xa71}, {0xa81,0xa82}, {0xabc,0xabc}, {0xac1,0xac5}, 
	{0xac7,0xac8}, {0xacd,0xacd}, {0xae2,0xae3}, {0xb01,0xb01}, 
	{0xb3c,0xb3c}, {0xb3f,0xb3f}, {0xb41,0xb43}, {0xb4d,0xb4d}, 
	{0xb56,0xb56}, {0xb82,0xb82}, {0xbc0,0xbc0}, {0xbcd,0xbcd}, 
	{0xc3e,0xc40}, {0xc46,0xc48}, {0xc4a,0xc4d}, {0xc55,0xc56}, 
	{0xcbc,0xcbc}, {0xcbf,0xcbf}, {0xcc6,0xcc6}, {0xccc,0xccd}, 
	{0xd41,0xd43}, {0xd4d,0xd4d}, {0xdca,0xdca}, {0xdd2,0xdd4}, 
	{0xdd6,0xdd6}, {0xe31,0xe31}, {0xe34,0xe3a}, {0xe47,0xe4e}, 
	{0xeb1,0xeb1}, {0xeb4,0xeb9}, {0xebb,0xebc}, {0xec8,0xecd}, 
	{0xf18,0xf19}, {0xf35,0xf35}, {0xf37,0xf37}, {0xf39,0xf39}, 
	{0xf71,0xf7e}, {0xf80,0xf84}, {0xf86,0xf87}, {0xf90,0xf97}, 
	{0xf99,0xfbc}, {0xfc6,0xfc6}, {0x102d,0x1030}, {0x1032,0x1032}, 
	{0x1036,0x1037}, {0x1039,0x1039}, {0x1058,0x1059}, 
	{0x1712,0x1714}, {0x1732,0x1734}, {0x1752,0x1753}, 
	{0x1772,0x1773}, {0x17b7,0x17bd}, {0x17c6,0x17c6}, 
	{0x17c9,0x17d3}, {0x17dd,0x17dd}, {0x180b,0x180d}, 
	{0x18a9,0x18a9}, {0x1920,0x1922}, {0x1927,0x1928}, 
	{0x1932,0x1932}, {0x1939,0x193b}, {0x20d0,0x20ea}, 
	{0x302a,0x302f}, {0x3099,0x309a}, {0xfb1e,0xfb1e}, 
	{0xfe00,0xfe0f}, {0xfe20,0xfe23}, {0x1d167,0x1d169}, 
	{0x1d17b,0x1d182}, {0x1d185,0x1d18b}, {0x1d1aa,0x1d1ad}, 
	{0xe0100,0xe01ef}, 
};

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
 */
static struct wchar_range ubin_table[] = {
	{  0x0000,  0x001f} /* Cc */, {  0x007f,  0x009f} /* Cc */,
#if 0
	{  0x00ad,  0x00ad} /* Cf */,
#endif
	{  0x0237,  0x024f} /* Cn */, {  0x0358,  0x035c} /* Cn */,
	{  0x0370,  0x0373} /* Cn */, {  0x0376,  0x0379} /* Cn */,
	{  0x037b,  0x037d} /* Cn */, {  0x037f,  0x0383} /* Cn */,
	{  0x038b,  0x038b} /* Cn */, {  0x038d,  0x038d} /* Cn */,
	{  0x03a2,  0x03a2} /* Cn */, {  0x03cf,  0x03cf} /* Cn */,
	{  0x03fc,  0x03ff} /* Cn */, {  0x0487,  0x0487} /* Cn */,
	{  0x04cf,  0x04cf} /* Cn */, {  0x04f6,  0x04f7} /* Cn */,
	{  0x04fa,  0x04ff} /* Cn */, {  0x0510,  0x0530} /* Cn */,
	{  0x0557,  0x0558} /* Cn */, {  0x0560,  0x0560} /* Cn */,
	{  0x0588,  0x0588} /* Cn */, {  0x058b,  0x0590} /* Cn */,
	{  0x05a2,  0x05a2} /* Cn */, {  0x05ba,  0x05ba} /* Cn */,
	{  0x05c5,  0x05cf} /* Cn */, {  0x05eb,  0x05ef} /* Cn */,
	{  0x05f5,  0x05ff} /* Cn */,
#if 0
	{  0x0600,  0x0603} /* Cf */,
#endif
	{  0x0604,  0x060b} /* Cn */, {  0x0616,  0x061a} /* Cn */,
	{  0x061c,  0x061e} /* Cn */, {  0x0620,  0x0620} /* Cn */,
	{  0x063b,  0x063f} /* Cn */, {  0x0659,  0x065f} /* Cn */,
#if 0
	{  0x06dd,  0x06dd} /* Cf */,
#endif
	{  0x070e,  0x070e} /* Cn */,
#if 0
	{  0x070f,  0x070f} /* Cf */,
#endif
	{  0x074b,  0x074c} /* Cn */, {  0x0750,  0x077f} /* Cn */,
	{  0x07b2,  0x0900} /* Cn */, {  0x093a,  0x093b} /* Cn */,
	{  0x094e,  0x094f} /* Cn */, {  0x0955,  0x0957} /* Cn */,
	{  0x0971,  0x0980} /* Cn */, {  0x0984,  0x0984} /* Cn */,
	{  0x098d,  0x098e} /* Cn */, {  0x0991,  0x0992} /* Cn */,
	{  0x09a9,  0x09a9} /* Cn */, {  0x09b1,  0x09b1} /* Cn */,
	{  0x09b3,  0x09b5} /* Cn */, {  0x09ba,  0x09bb} /* Cn */,
	{  0x09c5,  0x09c6} /* Cn */, {  0x09c9,  0x09ca} /* Cn */,
	{  0x09ce,  0x09d6} /* Cn */, {  0x09d8,  0x09db} /* Cn */,
	{  0x09de,  0x09de} /* Cn */, {  0x09e4,  0x09e5} /* Cn */,
	{  0x09fb,  0x0a00} /* Cn */, {  0x0a04,  0x0a04} /* Cn */,
	{  0x0a0b,  0x0a0e} /* Cn */, {  0x0a11,  0x0a12} /* Cn */,
	{  0x0a29,  0x0a29} /* Cn */, {  0x0a31,  0x0a31} /* Cn */,
	{  0x0a34,  0x0a34} /* Cn */, {  0x0a37,  0x0a37} /* Cn */,
	{  0x0a3a,  0x0a3b} /* Cn */, {  0x0a3d,  0x0a3d} /* Cn */,
	{  0x0a43,  0x0a46} /* Cn */, {  0x0a49,  0x0a4a} /* Cn */,
	{  0x0a4e,  0x0a58} /* Cn */, {  0x0a5d,  0x0a5d} /* Cn */,
	{  0x0a5f,  0x0a65} /* Cn */, {  0x0a75,  0x0a80} /* Cn */,
	{  0x0a84,  0x0a84} /* Cn */, {  0x0a8e,  0x0a8e} /* Cn */,
	{  0x0a92,  0x0a92} /* Cn */, {  0x0aa9,  0x0aa9} /* Cn */,
	{  0x0ab1,  0x0ab1} /* Cn */, {  0x0ab4,  0x0ab4} /* Cn */,
	{  0x0aba,  0x0abb} /* Cn */, {  0x0ac6,  0x0ac6} /* Cn */,
	{  0x0aca,  0x0aca} /* Cn */, {  0x0ace,  0x0acf} /* Cn */,
	{  0x0ad1,  0x0adf} /* Cn */, {  0x0ae4,  0x0ae5} /* Cn */,
	{  0x0af0,  0x0af0} /* Cn */, {  0x0af2,  0x0b00} /* Cn */,
	{  0x0b04,  0x0b04} /* Cn */, {  0x0b0d,  0x0b0e} /* Cn */,
	{  0x0b11,  0x0b12} /* Cn */, {  0x0b29,  0x0b29} /* Cn */,
	{  0x0b31,  0x0b31} /* Cn */, {  0x0b34,  0x0b34} /* Cn */,
	{  0x0b3a,  0x0b3b} /* Cn */, {  0x0b44,  0x0b46} /* Cn */,
	{  0x0b49,  0x0b4a} /* Cn */, {  0x0b4e,  0x0b55} /* Cn */,
	{  0x0b58,  0x0b5b} /* Cn */, {  0x0b5e,  0x0b5e} /* Cn */,
	{  0x0b62,  0x0b65} /* Cn */, {  0x0b72,  0x0b81} /* Cn */,
	{  0x0b84,  0x0b84} /* Cn */, {  0x0b8b,  0x0b8d} /* Cn */,
	{  0x0b91,  0x0b91} /* Cn */, {  0x0b96,  0x0b98} /* Cn */,
	{  0x0b9b,  0x0b9b} /* Cn */, {  0x0b9d,  0x0b9d} /* Cn */,
	{  0x0ba0,  0x0ba2} /* Cn */, {  0x0ba5,  0x0ba7} /* Cn */,
	{  0x0bab,  0x0bad} /* Cn */, {  0x0bb6,  0x0bb6} /* Cn */,
	{  0x0bba,  0x0bbd} /* Cn */, {  0x0bc3,  0x0bc5} /* Cn */,
	{  0x0bc9,  0x0bc9} /* Cn */, {  0x0bce,  0x0bd6} /* Cn */,
	{  0x0bd8,  0x0be6} /* Cn */, {  0x0bfb,  0x0c00} /* Cn */,
	{  0x0c04,  0x0c04} /* Cn */, {  0x0c0d,  0x0c0d} /* Cn */,
	{  0x0c11,  0x0c11} /* Cn */, {  0x0c29,  0x0c29} /* Cn */,
	{  0x0c34,  0x0c34} /* Cn */, {  0x0c3a,  0x0c3d} /* Cn */,
	{  0x0c45,  0x0c45} /* Cn */, {  0x0c49,  0x0c49} /* Cn */,
	{  0x0c4e,  0x0c54} /* Cn */, {  0x0c57,  0x0c5f} /* Cn */,
	{  0x0c62,  0x0c65} /* Cn */, {  0x0c70,  0x0c81} /* Cn */,
	{  0x0c84,  0x0c84} /* Cn */, {  0x0c8d,  0x0c8d} /* Cn */,
	{  0x0c91,  0x0c91} /* Cn */, {  0x0ca9,  0x0ca9} /* Cn */,
	{  0x0cb4,  0x0cb4} /* Cn */, {  0x0cba,  0x0cbb} /* Cn */,
	{  0x0cc5,  0x0cc5} /* Cn */, {  0x0cc9,  0x0cc9} /* Cn */,
	{  0x0cce,  0x0cd4} /* Cn */, {  0x0cd7,  0x0cdd} /* Cn */,
	{  0x0cdf,  0x0cdf} /* Cn */, {  0x0ce2,  0x0ce5} /* Cn */,
	{  0x0cf0,  0x0d01} /* Cn */, {  0x0d04,  0x0d04} /* Cn */,
	{  0x0d0d,  0x0d0d} /* Cn */, {  0x0d11,  0x0d11} /* Cn */,
	{  0x0d29,  0x0d29} /* Cn */, {  0x0d3a,  0x0d3d} /* Cn */,
	{  0x0d44,  0x0d45} /* Cn */, {  0x0d49,  0x0d49} /* Cn */,
	{  0x0d4e,  0x0d56} /* Cn */, {  0x0d58,  0x0d5f} /* Cn */,
	{  0x0d62,  0x0d65} /* Cn */, {  0x0d70,  0x0d81} /* Cn */,
	{  0x0d84,  0x0d84} /* Cn */, {  0x0d97,  0x0d99} /* Cn */,
	{  0x0db2,  0x0db2} /* Cn */, {  0x0dbc,  0x0dbc} /* Cn */,
	{  0x0dbe,  0x0dbf} /* Cn */, {  0x0dc7,  0x0dc9} /* Cn */,
	{  0x0dcb,  0x0dce} /* Cn */, {  0x0dd5,  0x0dd5} /* Cn */,
	{  0x0dd7,  0x0dd7} /* Cn */, {  0x0de0,  0x0df1} /* Cn */,
	{  0x0df5,  0x0e00} /* Cn */, {  0x0e3b,  0x0e3e} /* Cn */,
	{  0x0e5c,  0x0e80} /* Cn */, {  0x0e83,  0x0e83} /* Cn */,
	{  0x0e85,  0x0e86} /* Cn */, {  0x0e89,  0x0e89} /* Cn */,
	{  0x0e8b,  0x0e8c} /* Cn */, {  0x0e8e,  0x0e93} /* Cn */,
	{  0x0e98,  0x0e98} /* Cn */, {  0x0ea0,  0x0ea0} /* Cn */,
	{  0x0ea4,  0x0ea4} /* Cn */, {  0x0ea6,  0x0ea6} /* Cn */,
	{  0x0ea8,  0x0ea9} /* Cn */, {  0x0eac,  0x0eac} /* Cn */,
	{  0x0eba,  0x0eba} /* Cn */, {  0x0ebe,  0x0ebf} /* Cn */,
	{  0x0ec5,  0x0ec5} /* Cn */, {  0x0ec7,  0x0ec7} /* Cn */,
	{  0x0ece,  0x0ecf} /* Cn */, {  0x0eda,  0x0edb} /* Cn */,
	{  0x0ede,  0x0eff} /* Cn */, {  0x0f48,  0x0f48} /* Cn */,
	{  0x0f6b,  0x0f70} /* Cn */, {  0x0f8c,  0x0f8f} /* Cn */,
	{  0x0f98,  0x0f98} /* Cn */, {  0x0fbd,  0x0fbd} /* Cn */,
	{  0x0fcd,  0x0fce} /* Cn */, {  0x0fd0,  0x0fff} /* Cn */,
	{  0x1022,  0x1022} /* Cn */, {  0x1028,  0x1028} /* Cn */,
	{  0x102b,  0x102b} /* Cn */, {  0x1033,  0x1035} /* Cn */,
	{  0x103a,  0x103f} /* Cn */, {  0x105a,  0x109f} /* Cn */,
	{  0x10c6,  0x10cf} /* Cn */, {  0x10f9,  0x10fa} /* Cn */,
	{  0x10fc,  0x10ff} /* Cn */, {  0x115a,  0x115e} /* Cn */,
	{  0x11a3,  0x11a7} /* Cn */, {  0x11fa,  0x11ff} /* Cn */,
	{  0x1207,  0x1207} /* Cn */, {  0x1247,  0x1247} /* Cn */,
	{  0x1249,  0x1249} /* Cn */, {  0x124e,  0x124f} /* Cn */,
	{  0x1257,  0x1257} /* Cn */, {  0x1259,  0x1259} /* Cn */,
	{  0x125e,  0x125f} /* Cn */, {  0x1287,  0x1287} /* Cn */,
	{  0x1289,  0x1289} /* Cn */, {  0x128e,  0x128f} /* Cn */,
	{  0x12af,  0x12af} /* Cn */, {  0x12b1,  0x12b1} /* Cn */,
	{  0x12b6,  0x12b7} /* Cn */, {  0x12bf,  0x12bf} /* Cn */,
	{  0x12c1,  0x12c1} /* Cn */, {  0x12c6,  0x12c7} /* Cn */,
	{  0x12cf,  0x12cf} /* Cn */, {  0x12d7,  0x12d7} /* Cn */,
	{  0x12ef,  0x12ef} /* Cn */, {  0x130f,  0x130f} /* Cn */,
	{  0x1311,  0x1311} /* Cn */, {  0x1316,  0x1317} /* Cn */,
	{  0x131f,  0x131f} /* Cn */, {  0x1347,  0x1347} /* Cn */,
	{  0x135b,  0x1360} /* Cn */, {  0x137d,  0x139f} /* Cn */,
	{  0x13f5,  0x1400} /* Cn */, {  0x1677,  0x167f} /* Cn */,
	{  0x169d,  0x169f} /* Cn */, {  0x16f1,  0x16ff} /* Cn */,
	{  0x170d,  0x170d} /* Cn */, {  0x1715,  0x171f} /* Cn */,
	{  0x1737,  0x173f} /* Cn */, {  0x1754,  0x175f} /* Cn */,
	{  0x176d,  0x176d} /* Cn */, {  0x1771,  0x1771} /* Cn */,
	{  0x1774,  0x177f} /* Cn */,
#if 0
	{  0x17b4,  0x17b5} /* Cf */,
#endif
	{  0x17de,  0x17df} /* Cn */, {  0x17ea,  0x17ef} /* Cn */,
	{  0x17fa,  0x17ff} /* Cn */, {  0x180f,  0x180f} /* Cn */,
	{  0x181a,  0x181f} /* Cn */, {  0x1878,  0x187f} /* Cn */,
	{  0x18aa,  0x18ff} /* Cn */, {  0x191d,  0x191f} /* Cn */,
	{  0x192c,  0x192f} /* Cn */, {  0x193c,  0x193f} /* Cn */,
	{  0x1941,  0x1943} /* Cn */, {  0x196e,  0x196f} /* Cn */,
	{  0x1975,  0x19df} /* Cn */, {  0x1a00,  0x1cff} /* Cn */,
	{  0x1d6c,  0x1dff} /* Cn */, {  0x1e9c,  0x1e9f} /* Cn */,
	{  0x1efa,  0x1eff} /* Cn */, {  0x1f16,  0x1f17} /* Cn */,
	{  0x1f1e,  0x1f1f} /* Cn */, {  0x1f46,  0x1f47} /* Cn */,
	{  0x1f4e,  0x1f4f} /* Cn */, {  0x1f58,  0x1f58} /* Cn */,
	{  0x1f5a,  0x1f5a} /* Cn */, {  0x1f5c,  0x1f5c} /* Cn */,
	{  0x1f5e,  0x1f5e} /* Cn */, {  0x1f7e,  0x1f7f} /* Cn */,
	{  0x1fb5,  0x1fb5} /* Cn */, {  0x1fc5,  0x1fc5} /* Cn */,
	{  0x1fd4,  0x1fd5} /* Cn */, {  0x1fdc,  0x1fdc} /* Cn */,
	{  0x1ff0,  0x1ff1} /* Cn */, {  0x1ff5,  0x1ff5} /* Cn */,
	{  0x1fff,  0x1fff} /* Cn */, {  0x200b,  0x200f} /* Cf */,
	{  0x2028,  0x2028} /* Zl */,
	{  0x2029,  0x2029} /* Zp */,
	{  0x202a,  0x202e} /* Cf */,
	{  0x2055,  0x2056} /* Cn */, {  0x2058,  0x205e} /* Cn */,
	{  0x2060,  0x2063} /* Cf */,
	{  0x2064,  0x2069} /* Cn */,
	{  0x206a,  0x206f} /* Cf */,
	{  0x2072,  0x2073} /* Cn */, {  0x208f,  0x209f} /* Cn */,
	{  0x20b2,  0x20cf} /* Cn */, {  0x20eb,  0x20ff} /* Cn */,
	{  0x213c,  0x213c} /* Cn */, {  0x214c,  0x2152} /* Cn */,
	{  0x2184,  0x218f} /* Cn */, {  0x23d1,  0x23ff} /* Cn */,
	{  0x2427,  0x243f} /* Cn */, {  0x244b,  0x245f} /* Cn */,
	{  0x2618,  0x2618} /* Cn */, {  0x267e,  0x267f} /* Cn */,
	{  0x2692,  0x269f} /* Cn */, {  0x26a2,  0x2700} /* Cn */,
	{  0x2705,  0x2705} /* Cn */, {  0x270a,  0x270b} /* Cn */,
	{  0x2728,  0x2728} /* Cn */, {  0x274c,  0x274c} /* Cn */,
	{  0x274e,  0x274e} /* Cn */, {  0x2753,  0x2755} /* Cn */,
	{  0x2757,  0x2757} /* Cn */, {  0x275f,  0x2760} /* Cn */,
	{  0x2795,  0x2797} /* Cn */, {  0x27b0,  0x27b0} /* Cn */,
	{  0x27bf,  0x27cf} /* Cn */, {  0x27ec,  0x27ef} /* Cn */,
	{  0x2b0e,  0x2e7f} /* Cn */, {  0x2e9a,  0x2e9a} /* Cn */,
	{  0x2ef4,  0x2eff} /* Cn */, {  0x2fd6,  0x2fef} /* Cn */,
	{  0x2ffc,  0x2fff} /* Cn */, {  0x3040,  0x3040} /* Cn */,
	{  0x3097,  0x3098} /* Cn */, {  0x3100,  0x3104} /* Cn */,
	{  0x312d,  0x3130} /* Cn */, {  0x318f,  0x318f} /* Cn */,
	{  0x31b8,  0x31ef} /* Cn */, {  0x321f,  0x321f} /* Cn */,
	{  0x3244,  0x324f} /* Cn */, {  0x327e,  0x327e} /* Cn */,
	{  0x32ff,  0x32ff} /* Cn */, {  0x4db6,  0x4dbf} /* Cn */,
	{  0x9fa6,  0x9fff} /* Cn */, {  0xa48d,  0xa48f} /* Cn */,
	{  0xa4c7,  0xabff} /* Cn */, {  0xd7a4,  0xd7ff} /* Cn */,
	{  0xd800,  0xdfff} /* Cs */,
	{  0xe000,  0xf8ff} /* Co */,
	{  0xfa2e,  0xfa2f} /* Cn */, {  0xfa6b,  0xfaff} /* Cn */,
	{  0xfb07,  0xfb12} /* Cn */, {  0xfb18,  0xfb1c} /* Cn */,
	{  0xfb37,  0xfb37} /* Cn */, {  0xfb3d,  0xfb3d} /* Cn */,
	{  0xfb3f,  0xfb3f} /* Cn */, {  0xfb42,  0xfb42} /* Cn */,
	{  0xfb45,  0xfb45} /* Cn */, {  0xfbb2,  0xfbd2} /* Cn */,
	{  0xfd40,  0xfd4f} /* Cn */, {  0xfd90,  0xfd91} /* Cn */,
	{  0xfdc8,  0xfdef} /* Cn */, {  0xfdfe,  0xfdff} /* Cn */,
	{  0xfe10,  0xfe1f} /* Cn */, {  0xfe24,  0xfe2f} /* Cn */,
	{  0xfe53,  0xfe53} /* Cn */, {  0xfe67,  0xfe67} /* Cn */,
	{  0xfe6c,  0xfe6f} /* Cn */, {  0xfe75,  0xfe75} /* Cn */,
	{  0xfefd,  0xfefe} /* Cn */,
	{  0xfeff,  0xfeff} /* Cf */,
	{  0xff00,  0xff00} /* Cn */, {  0xffbf,  0xffc1} /* Cn */,
	{  0xffc8,  0xffc9} /* Cn */, {  0xffd0,  0xffd1} /* Cn */,
	{  0xffd8,  0xffd9} /* Cn */, {  0xffdd,  0xffdf} /* Cn */,
	{  0xffe7,  0xffe7} /* Cn */, {  0xffef,  0xfff8} /* Cn */,
	{  0xfff9,  0xfffb} /* Cf */,
	{  0xfffe,  0xffff} /* Cn */, { 0x1000c, 0x1000c} /* Cn */,
	{ 0x10027, 0x10027} /* Cn */, { 0x1003b, 0x1003b} /* Cn */,
	{ 0x1003e, 0x1003e} /* Cn */, { 0x1004e, 0x1004f} /* Cn */,
	{ 0x1005e, 0x1007f} /* Cn */, { 0x100fb, 0x100ff} /* Cn */,
	{ 0x10103, 0x10106} /* Cn */, { 0x10134, 0x10136} /* Cn */,
	{ 0x10140, 0x102ff} /* Cn */, { 0x1031f, 0x1031f} /* Cn */,
	{ 0x10324, 0x1032f} /* Cn */, { 0x1034b, 0x1037f} /* Cn */,
	{ 0x1039e, 0x1039e} /* Cn */, { 0x103a0, 0x103ff} /* Cn */,
	{ 0x1049e, 0x1049f} /* Cn */, { 0x104aa, 0x107ff} /* Cn */,
	{ 0x10806, 0x10807} /* Cn */, { 0x10809, 0x10809} /* Cn */,
	{ 0x10836, 0x10836} /* Cn */, { 0x10839, 0x1083b} /* Cn */,
	{ 0x1083d, 0x1083e} /* Cn */, { 0x10840, 0x1cfff} /* Cn */,
	{ 0x1d0f6, 0x1d0ff} /* Cn */, { 0x1d127, 0x1d129} /* Cn */,
	{ 0x1d173, 0x1d17a} /* Cf */,
	{ 0x1d1de, 0x1d2ff} /* Cn */, { 0x1d357, 0x1d3ff} /* Cn */,
	{ 0x1d455, 0x1d455} /* Cn */, { 0x1d49d, 0x1d49d} /* Cn */,
	{ 0x1d4a0, 0x1d4a1} /* Cn */, { 0x1d4a3, 0x1d4a4} /* Cn */,
	{ 0x1d4a7, 0x1d4a8} /* Cn */, { 0x1d4ad, 0x1d4ad} /* Cn */,
	{ 0x1d4ba, 0x1d4ba} /* Cn */, { 0x1d4bc, 0x1d4bc} /* Cn */,
	{ 0x1d4c4, 0x1d4c4} /* Cn */, { 0x1d506, 0x1d506} /* Cn */,
	{ 0x1d50b, 0x1d50c} /* Cn */, { 0x1d515, 0x1d515} /* Cn */,
	{ 0x1d51d, 0x1d51d} /* Cn */, { 0x1d53a, 0x1d53a} /* Cn */,
	{ 0x1d53f, 0x1d53f} /* Cn */, { 0x1d545, 0x1d545} /* Cn */,
	{ 0x1d547, 0x1d549} /* Cn */, { 0x1d551, 0x1d551} /* Cn */,
	{ 0x1d6a4, 0x1d6a7} /* Cn */, { 0x1d7ca, 0x1d7cd} /* Cn */,
	{ 0x1d800, 0x1ffff} /* Cn */, { 0x2a6d7, 0x2f7ff} /* Cn */,
	{ 0x2fa1e, 0xe0000} /* Cn */,
	{ 0xe0001, 0xe0001} /* Cf */,
	{ 0xe0002, 0xe001f} /* Cn */,
	{ 0xe0020, 0xe007f} /* Cf */,
	{ 0xe0080, 0xe00ff} /* Cn */, { 0xe01f0, 0xeffff} /* Cn */,
	{ 0xf0000, 0xffffd} /* Co */,
	{ 0xffffe, 0xfffff} /* Cn */,
	{0x100000,0x10fffd} /* Co */,
	{0x10fffe,0x10ffff} /* Cn */,
	{0x110000,0x7fffffff} /* ISO 10646?? */
};

/*
 * Double width characters
 *	W: East Asian Wide
 *	F: East Asian Full-width
 */
static struct wchar_range wide_table[] = {
	{  0x1100,  0x115f} /* W */, {  0x2329,  0x232a} /* W */,
	{  0x2E80,  0x2FFB} /* W */,
	{  0x3000,  0x3000} /* F */,
	{  0x3001,  0x303E} /* W */, {  0x3041,  0x4DB5} /* W */,
	{  0x4E00,  0x9FA5} /* W */, {  0xA000,  0xA4C6} /* W */,
	{  0xAC00,  0xD7A3} /* W */, {  0xF900,  0xFA6A} /* W */,
	{  0xFE30,  0xFE6B} /* W */,
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

