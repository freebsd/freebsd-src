/*
 * caps.c
 * 
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:29:45
 *
 * caps [-c | -t] [term]
 *
 * -c		use termcap names instead of terminfo variable names
 * -t		use terminfo capnames instead of variables names
 * term 	name of terminal to use
 *
 * prints out all the capabilities given the specified terminal. If no
 * terminal is given, it is taken from the environment variable TERM.
 *
 */

#define NOTLIB
#include "defs.h"
#include <term.h>

const char SCCSid[] = "@(#) mytinfo caps.c 3.2 92/02/01 public domain, By Ross Ridge";

/* output a string in a human readable format */
void
putstr(s)
char *s; {
	while(*s != '\0') {
		switch(*s) {
		case '\n': printf("\\n"); break;
		case '\b': printf("\\b"); break;
		case '\t': printf("\\t"); break;
		case '\r': printf("\\r"); break;
		case '\f': printf("\\f"); break;
		case ' ': printf("\\s"); break;
		case '\177': printf("^?"); break;
		case '\200': printf("\\0"); break;
		default:
			if (*s > 0 && *s < 32)
				printf("^%c", *s + 64);
			else if (*s < 0)
				printf("\\%03o", *s & 0xff);
			else
				putchar(*s);
			break;
		}
		s++;
	}
}

void
do_cleanup(e)
int e; {
	fprintf(stderr, "usage: %s [-c | -t ] [terminal]\n", prg_name);
	return;
}

int
main(argc, argv)
int argc;
char **argv; {
	int names = 0;
	register int i;
	int flag, num;
	char *str;

	prg_name = argv[0];
	cleanup = do_cleanup;

	if (argc > 3)
		quit(-1, "argument count");

	if (argc == 1)
		setupterm(NULL, 2, (int *) 0);
	else if (argc == 2) {
		if (argv[1][0] != '-')
		setupterm(argv[1], 2, (int *) 0);
	else {
			if (argv[1][1] == 'c')
				names = 2;
			else if (argv[1][1] == 't')
				names = 1;
			else
				quit(-1, "unknown switch '%c'", argv[1][1]);
			setupterm(NULL, 2, (int *) 0);
		}
	} else {
		if (argv[1][0] != '-')
			quit(-1, "bad switch");
		if (argv[1][1] == 'c')
			names = 2;
		else if (argv[1][1] == 't')
			names = 1;
		else
			quit(-1, "unknown switch '%c'", argv[1][1]);
		setupterm(argv[2], 2, (int *) 0);

	}

	fflush(stderr);
	fflush(stdout);
	printf("\n");
#ifdef _CUR_TERM
	printf("%s: %s\n", cur_term->name, cur_term->name_all);
	printf("pad: %d xon: %d termcap: %d\n",
	        cur_term->pad, cur_term->xon, cur_term->termcap);
	printf("true_columns: %d true_lines: %d baudrate: %lu\n",
		cur_term->true_columns, cur_term->true_lines,
		(unsigned long) cur_term->baudrate);
	printf("\n");
#endif

	printf("Booleans:\n");
	for(i = 0; boolnames[i] != NULL; i++) {
#ifdef _CUR_TERM
		flag = cur_term->bools[i];
#else
		flag = tigetflag(boolnames[i]);
#endif
		if (flag != -1 && flag != 0) {
			switch(names) {
			case 0:
				printf("  %s\n", boolfnames[i]);
				break;
			case 1:
				printf("  %s\n", boolnames[i]);
				break;
			case 2:
				printf("  %s\n", boolcodes[i]);
				break;
			}
		}
	}

	printf("\nNumerics:\n");
	for(i = 0; numnames[i] != NULL; i++) {
		num = tigetnum(numnames[i]);
		if (num != -2 && num != -1) {
			switch(names) {
			case 0:
				printf("  %-32s: %d\n", numfnames[i], num);
				break;
			case 1:
				printf("  %-5s: %d\n", numnames[i], num);
				break;
			case 2:
				printf("  %-2s: %d\n", numcodes[i], num);
				break;
			}
		}
	}
	printf("\nStrings:\n");
	for(i = 0; strnames[i] != NULL; i++) {
		str = tigetstr(strnames[i]);
		if (str != (char *) -1 && str != (char *) 0) {
			switch(names) {
			case 0:
				printf("  %-32s: ", strfnames[i]);
				break;
			case 1:
				printf("  %-5s: ", strnames[i]);
				break;
			case 2:
				printf("  %-2s: ", strcodes[i]);
				break;
			}
			putstr(str);
			putchar('\n');
		}
	}
	return 0;
}
