/*
 * Copyright (C) 1984-2002  Mark Nudelman
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
#endif

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
	{ "ascii",	NULL,       "8bcccbcc18b95.b" },
	{ "dos",	NULL,       "8bcccbcc12bc5b223.b" },
	{ "ebcdic",	NULL,       "5bc6bcc7bcc41b.9b7.9b5.b..8b6.10b6.b9.7b9.8b8.17b3.3b9.7b9.8b8.6b10.b.b.b." },
	{ "IBM-1047",	NULL,       "4cbcbc3b9cbccbccbb4c6bcc5b3cbbc4bc4bccbc191.b" },
	{ "iso8859",	NULL,       "8bcccbcc18b95.33b." },
	{ "koi8-r",	NULL,       "8bcccbcc18b95.b128." },
	{ "next",	NULL,       "8bcccbcc18b95.bb125.bb" },
	{ "utf-8",	&utf_mode,  "8bcccbcc18b." },
	{ NULL, NULL, NULL }
};

struct cs_alias {
	char *name;
	char *oname;
} cs_aliases[] = {
	{ "latin1",	"iso8859" },
	{ "latin9",	"iso8859" },
	{ NULL, NULL }
};

#define	IS_BINARY_CHAR	01
#define	IS_CONTROL_CHAR	02

static char chardef[256];
static char *binfmt = NULL;
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
icharset(name)
	register char *name;
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

	error("invalid charset name", NULL_PARG);
	quit(QUIT_ERROR);
	/*NOTREACHED*/
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

	setlocale(LC_ALL, "");
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
 * Define the printing format for control chars.
 */
   	public void
setbinfmt(s)
	char *s;
{
	if (s == NULL || *s == '\0')
		s = "*s<%X>";
	/*
	 * Select the attributes if it starts with "*".
	 */
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
	binfmt = s;
}

/*
 * Initialize charset data structures.
 */
	public void
init_charset()
{
	register char *s;

	s = lgetenv("LESSBINFMT");
	setbinfmt(s);
	
	/*
	 * See if environment variable LESSCHARSET is defined.
	 */
	s = lgetenv("LESSCHARSET");
	if (icharset(s))
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

#if HAVE_STRSTR
	/*
	 * Check whether LC_ALL, LC_CTYPE or LANG look like UTF-8 is used.
	 */
	if ((s = lgetenv("LC_ALL")) != NULL ||
	    (s = lgetenv("LC_CTYPE")) != NULL ||
	    (s = lgetenv("LANG")) != NULL)
	{
		if (strstr(s, "UTF-8") != NULL || strstr(s, "utf-8") != NULL)
			if (icharset("utf-8"))
				return;
	}
#endif

#if HAVE_LOCALE
	/*
	 * Use setlocale.
	 */
	ilocale();
#else
#if MSDOS_COMPILER
	/*
	 * Default to "dos".
	 */
	(void) icharset("dos");
#else
	/*
	 * Default to "latin1".
	 */
	(void) icharset("latin1");
#endif
#endif
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
	static char buf[8];

	c &= 0377;
	if (!control_char(c))
		sprintf(buf, "%c", c);
	else if (c == ESC)
		sprintf(buf, "ESC");
#if IS_EBCDIC_HOST
	else if (!binary_char(c) && c < 64)
		sprintf(buf, "^%c",
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
  		sprintf(buf, "^%c", c ^ 0100);
#endif
	else
		sprintf(buf, binfmt, c);
	return (buf);
}
