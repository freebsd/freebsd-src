/*
 * mktermhead.c
 *
 * By Ross Ridge
 * Public Domain 
 * 92/06/04 11:38:57
 *
 * mktermhead [-n caps] file
 *
 * generates term.head
 *
 */

#define NOTLIB
#include "defs.h"

const char SCCSid[] = "@(#) mytinfo mktermhead.c 3.3 92/06/04 public domain, By Ross Ridge";

#define DEFAULT_CAPS	1000


int
main(argc, argv)
int argc;
char **argv; {
	FILE *f;
	int caps = DEFAULT_CAPS;
	int n;
	register int i;
	int nb, ns, nn;
	struct caplist *list;

	if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'n') {
		caps = atoi(argv[2]);
		argv += 2;
		argc -= 2;
	}

	if (argc == 1) {
		f = stdin;
	} else if (argc == 2) {
		f = fopen(argv[1], "r");
		if (f == NULL) {
			fprintf(stderr, "%s: can't open '%s'\n", argv[0],
				argv[1]);
			return 1;
		}
	} else {
		fprintf(stderr, "%s: too many arguments\n", argv[0]);
		fprintf(stderr, "usage: %s [-n caps] [file]\n", argv[0]);
		return 1;
	}

	list = (struct caplist *) malloc(caps * sizeof(struct caplist));
	if (list == NULL) {
		fprintf(stderr, "%s: malloc failed.\n", argv[0]);
		return 1;
	}

	n = readcaps(f, list, caps);
	if (n > caps) {
		fprintf(stderr, "%s: too many caps, use -n.\n", argv[0]);
		return 1;
	}
	switch(n) {
	case 0:
		fprintf(stderr, "%s: no caps in file.\n", argv[0]);
		return 1;
	case -1:
		fprintf(stderr, "%s: bad caps line.\n", argv[0]);
		return 1;
	case -2:
		fprintf(stderr, "%s: unexpected EOF.\n", argv[0]);
		return 1;
	}

	puts("/*");
	puts(" * term.h ");
	puts(" *");
	puts(" * This file was generated automatically.");
	puts(" *");
	puts(" */");
	putchar('\n');
	puts("#ifndef _TERM_H_");
	puts("#define _TERM_H_");
	putchar('\n');
	nb = 0;
	for (i = 0; i < n; i++) {
		if (list[i].type == '!') {
			printf("#define %-*s (_CUR_TERM.bools[%d])\n",
			       MAX_VARNAME, list[i].var, nb);
			nb++;
		}
	}

	nn = 0;
	for (i = 0; i < n; i++) {
		if (list[i].type == '#') {
			printf("#define %-*s (_CUR_TERM.nums[%d])\n",
			       MAX_VARNAME, list[i].var, nn);
			nn++;
		}
	}

	ns = 0;
	for (i = 0; i < n; i++) {
		if (list[i].type == '$') {
			printf("#define %-*s (_CUR_TERM.strs[%d])\n",
			       MAX_VARNAME, list[i].var, ns);
			ns++;
		}
	}

	putchar('\n');

	printf ("#define NUM_OF_BOOLS\t%d\n", nb);
	printf ("#define NUM_OF_NUMS\t%d\n", nn);
	printf ("#define NUM_OF_STRS\t%d\n", ns);

	putchar('\n');
	puts("#ifndef OVERRIDE");
	puts("#undef _USE_SGTTY");
#ifdef USE_SGTTY
	puts("#define _USE_SGTTY");
#endif
	puts("#undef _USE_TERMIO");
#ifdef USE_TERMIO
	puts("#define _USE_TERMIO");
#endif
	puts("#undef _USE_TERMIOS");
#ifdef USE_TERMIOS
	puts("#define _USE_TERMIOS");
#endif
	puts("#undef _USE_SMALLMEM");
#ifdef USE_SMALLMEM
	puts("#define _USE_SMALLMEM");
#endif
	puts("#undef _USE_PROTOTYPES");
#ifdef USE_PROTOTYPES
	puts("#define _USE_PROTOTYPES");
#endif
	puts("#undef _USE_WINSZ");
#ifdef USE_WINSZ
	puts("#define _USE_WINSZ");
#endif
	puts("#undef _USE_TERMINFO");
#ifdef USE_TERMINFO
	puts("#define _USE_TERMINFO");
#endif
	puts("#undef _USE_TERMCAP");
#ifdef USE_TERMCAP
	puts("#define _USE_TERMCAP");
#endif
	puts("#undef _MAX_CHUNK");
	printf("#define _MAX_CHUNK %d\n", MAX_CHUNK);
	puts("#endif /* OVERRIDE */");
	putchar('\n');

	return 0;
}
