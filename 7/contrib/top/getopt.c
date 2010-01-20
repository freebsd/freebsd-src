/*
 * "getopt" routine customized for top.
 */

/*
 * Many modern-day Unix implementations already have this function
 * in libc.  The standard "getopt" is perfectly sufficient for top's
 * needs.  If such a function exists in libc then you certainly don't
 * need to compile this one in.  To prevent this function from being 
 * compiled, define "HAVE_GETOPT".  This is usually done in the "CFLAGS"
 * line of the corresponding machine module.
 */

/*
 * This empty declaration exists solely to placate overexhuberant C
 * compilers that like to warn you about content-free files.
 */
static void __empty();

#ifndef HAVE_GETOPT

/*LINTLIBRARY*/

#include "os.h"
#ifndef NULL
#define NULL	0
#endif
#ifndef EOF
#define EOF	(-1)
#endif
#define ERR(s, c)	if(opterr){\
	extern int write();\
	char errbuf[2];\
	errbuf[0] = c; errbuf[1] = '\n';\
	(void) write(2, argv[0], strlen(argv[0]));\
	(void) write(2, s, strlen(s));\
	(void) write(2, errbuf, 2);}


int	opterr = 1;
int	optind = 1;
int	optopt;
char	*optarg;

int
getopt(argc, argv, opts)
int	argc;
char	**argv, *opts;
{
	static int sp = 1;
	register int c;
	register char *cp;

	if(sp == 1)
		if(optind >= argc ||
		   argv[optind][0] != '-' || argv[optind][1] == '\0')
			return(EOF);
		else if(strcmp(argv[optind], "--") == 0) {
			optind++;
			return(EOF);
		}
	optopt = c = argv[optind][sp];
	if(c == ':' || (cp=strchr(opts, c)) == NULL) {
		ERR(": unknown option, -", c);
		if(argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		return('?');
	}
	if(*++cp == ':') {
		if(argv[optind][sp+1] != '\0')
			optarg = &argv[optind++][sp+1];
		else if(++optind >= argc) {
			ERR(": argument missing for -", c);
			sp = 1;
			return('?');
		} else
			optarg = argv[optind++];
		sp = 1;
	} else {
		if(argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return(c);
}
#endif /* HAVE_GETOPT */
