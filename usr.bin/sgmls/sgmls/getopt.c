/* getopt.c -
   getopt() for those systems that don't have it.

   Derived from comp.sources.unix/volume3/att_getopt.
   Modified by James Clark (jjc@jclark.com).
*/

#include "config.h"

#ifndef HAVE_GETOPT

#include "std.h"
#include "getopt.h"

#ifdef SWITCHAR
#include <dos.h>
#endif

int	opterr = 1;
int	optind = 1;
int	optopt;
char *optarg;

#ifndef OPTION_CHAR
#define OPTION_CHAR '-'
#endif

int getopt(argc, argv, opts)
int argc;
char **argv;
char *opts;
{
#ifdef SWITCHAR
	union REGS regs;
	static char switchar = '\0';
#endif
	static int sp = 1;
	register int c;
	register char *cp;
	char *message;
#ifdef SWITCHAR
	if (switchar == '\0') {
		regs.x.ax = 0x3700;
		intdos(&regs, &regs);
		if (!regs.x.cflag)
			switchar = regs.h.dl;
		else
			switchar = '/';
	}
#endif
	if (sp == 1) {
		if (optind >= argc)
			return EOF;
		if ((
#ifdef SWITCHAR
			argv[optind][0] != switchar &&
#endif
			argv[optind][0] != OPTION_CHAR) || argv[optind][1] == '\0') {
#ifdef REORDER_ARGS
			int i;
			for (i = optind; i < argc; i++)
				if ((
#ifdef SWITCHAR
					 argv[i][0] == switchar ||
#endif
					 argv[i][0] == OPTION_CHAR) && argv[i][1] != '\0')
					break;
			if (i < argc) {
				c = argv[i][1];
#ifdef CASE_INSENSITIVE_OPTIONS
				if (isupper(c))
					c = tolower(c);
#endif
				if (c != ':' && c != OPTION_CHAR && (cp = strchr(opts, c)) != NULL
					&& cp[1] == ':' && argv[i][2] == 0 && i < argc - 1) {
					int j;
					char *temp1 = argv[i];
					char *temp2 = argv[i+1];
					for (j = i - 1; j >= optind; j--)
						argv[j+2] = argv[j];
					argv[optind] = temp1;
					argv[optind+1] = temp2;
				}
				else {
					int j;
					char *temp = argv[i];
					for (j = i - 1; j >= optind; j--)
						argv[j+1] = argv[j];
					argv[optind] = temp;
				}
			}
			else
#endif
				return EOF;
		}
		if ((argv[optind][0] == OPTION_CHAR && argv[optind][1] == OPTION_CHAR
				  && argv[optind][2] == '\0')
#ifdef SWITCHAR
			|| (argv[optind][0] == switchar && argv[optind][1] == switchar
				&& argv[optind][2] == '\0')
#endif
			) {
			optind++;
			return(EOF);
		}
	}
	optopt = c = argv[optind][sp];
#ifdef CASE_INSENSITIVE_OPTIONS
	if (
#ifdef USE_ISASCII
		isascii(c) &&
#endif /* USE_ISASCII */
		isupper((unsigned char)c))
		optopt = c = tolower((unsigned char)c);
#endif /* CASE_INSENSITIVE_OPTIONS */
	if (c == ':' || (cp = strchr(opts, c)) == NULL) {
		if (argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		message = ": illegal option -- ";
		goto bad;
	}
	if (*++cp == ':') {
		if (argv[optind][sp+1] != '\0')
			optarg = &argv[optind++][sp+1];
		else if (++optind >= argc) {
			sp = 1;
			message = ": option requires an argument -- ";
			goto bad;
		}
		else
			optarg = argv[optind++];
		sp = 1;
	}
	else {
		if (argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return c;
bad:
	if (opterr) {
		fputs(argv[0], stderr);
		fputs(message, stderr);
		fputc(optopt, stderr);
		fputc('\n', stderr);
	}
	return '?';
}

#endif /* not HAVE_GETOPT */

/*
Local Variables:
c-indent-level: 4
c-continued-statement-offset: 4
c-brace-offset: 4
c-argdecl-indent: 4
c-label-offset: -4
tab-width: 4
End:
*/

