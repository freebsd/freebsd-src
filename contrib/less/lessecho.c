/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * lessecho [-ox] [-cx] [-pn] [-dn] [-a] file ...
 * Simply echos its filename arguments on standard output.
 * But any argument containing spaces is enclosed in quotes.
 *
 * -ox	Specifies "x" to be the open quote character.
 * -cx	Specifies "x" to be the close quote character.
 * -pn	Specifies "n" to be the open quote character, as an integer.
 * -dn	Specifies "n" to be the close quote character, as an integer.
 * -a	Specifies that all arguments are to be quoted.
 *	The default is that only arguments containing spaces are quoted.
 */

#include "less.h"

static char *version = "$Revision: 1.6 $";

static int quote_all = 0;
static char openquote = '"';
static char closequote = '"';

	static void
pr_usage()
{
	fprintf(stderr,
		"usage: lessecho [-ox] [-cx] [-pn] [-dn] [-a] file ...\n");
}

	static void
pr_version()
{
	char *p;
	char buf[10];
	char *pbuf = buf;

	for (p = version;  *p != ' ';  p++)
		if (*p == '\0')
			return;
	for (p++;  *p != '$' && *p != ' ' && *p != '\0';  p++)
		*pbuf++ = *p;
	*pbuf = '\0';
	printf("%s\n", buf);
}

	static void
pr_error(s)
	char *s;
{
	fprintf(stderr, "%s\n", s);
	exit(1);
}

	static long
lstrtol(s, radix, pend)
	char *s;
	int radix;
	char **pend;
{
	int v;
	int neg = 0;
	long n = 0;

	/* Skip leading white space. */
	while (*s == ' ' || *s == '\t')
		s++;

	/* Check for a leading + or -. */
	if (*s == '-')
	{
		neg = 1;
		s++;
	} else if (*s == '+')
	{
		s++;
	}

	/* Determine radix if caller does not specify. */
	if (radix == 0)
	{
		radix = 10;
		if (*s == '0')
		{
			switch (*++s)
			{
			case 'x':
				radix = 16;
				s++;
				break;
			default:
				radix = 8;
				break;
			}
		}
	}

	/* Parse the digits of the number. */
	for (;;)
	{
		if (*s >= '0' && *s <= '9')
			v = *s - '0';
		else if (*s >= 'a' && *s <= 'f')
			v = *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'F')
			v = *s - 'A' + 10;
		else
			break;
		if (v >= radix)
			break;
		n = n * radix + v;
		s++;
	}

	if (pend != NULL)
	{
		/* Skip trailing white space. */
		while (*s == ' ' || *s == '\t')
			s++;
		*pend = s;
	}
	if (neg)
		return (-n);
	return (n);
}


#if !HAVE_STRCHR
	char *
strchr(s, c)
	char *s;
	int c;
{
	for ( ;  *s != '\0';  s++)
		if (*s == c)
			return (s);
	if (c == '\0')
		return (s);
	return (NULL);
}
#endif

	int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *arg;
	char *s;
	int no_more_options;

	no_more_options = 0;
	while (--argc > 0)
	{
		arg = *++argv;
		if (*arg != '-' || no_more_options)
			break;
		switch (*++arg)
		{
		case 'a':
			quote_all = 1;
			break;
		case 'o':
			openquote = *++arg;
			break;
		case 'c':
			closequote = *++arg;
			break;
		case 'p':
			openquote = lstrtol(++arg, 0, &s);
			if (s == arg)
				pr_error("Missing number after -O");
			break;
		case 'd':
			closequote = lstrtol(++arg, 0, &s);
			if (s == arg)
				pr_error("Missing number after -C");
			break;
		case '?':
			pr_usage();
			return (0);
		case '-':
			if (*++arg == '\0')
			{
				no_more_options = 1;
				break;
			}
			if (strcmp(arg, "version") == 0)
			{
				pr_version();
				return (0);
			}
			if (strcmp(arg, "help") == 0)
			{
				pr_usage();
				return (0);
			}
			pr_error("Invalid option after --");
		default:
			pr_error("Invalid option letter");
		}
	}

	while (argc-- > 0)
	{
		arg = *argv++;
		if (quote_all || strchr(arg, ' ') != NULL)
			printf("%c%s%c", openquote, arg, closequote);
		else
			printf("%s", arg);
		if (argc > 0)
			printf(" ");
		else
			printf("\n");
	}
	return (0);
}
