/*
 * tput.c
 * 
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:39
 *
 */

#define NOTLIB

#include "defs.h"

const char SCCSid[] = "@(#) mytinfo tput.c 3.2 92/02/01 public domain, By Ross Ridge";

#define SINGLE
#include <term.h>

#include <ctype.h>

#define PUTS(s)		putp(s)
#define PUTCHAR(c)	putchar(c)
#define FLUSH		fflush(stdout)

extern void (*cleanup)();

static void
clean(e)
int e; {
	return;
}

static void
usage(e)
int e; {
	fprintf(stderr, "usage: %s [-T term] capname\n", prg_name);
	return;
}

int
main(argc, argv)
int argc;
char **argv; {
	char *s;
	int i, j, c;
	int reset;
	FILE *f;
	char *term;

	prg_name = argv[0];
	s = strrchr(prg_name, '/');
	if (s != NULL && *++s != '\0') {
		prg_name = s;
	}

	term = getenv("TERM");

	cleanup = usage;

	if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'T') {
		if (argv[1][2] == '\0' && argc > 3) {
			term = argv[2];
			argc--;
			argv++;
		} else {
			term = argv[1] + 2;
		}
		argc--;
		argv++;
	}

	if (argc != 2) {
		quit(-1, "arg count");
	}

	cleanup = clean;

	setupterm(term, 1, (int *)0);

	reset = 0;
	if (strcmp(argv[1], "reset") == 0) {
		reset = 1;
	}
	if (reset || strcmp(argv[1], "init") == 0) {

		if (init_prog != NULL) {
			system(init_prog);
		}
		FLUSH;

		if (reset && reset_1string != NULL) {
			PUTS(reset_1string);
		} else if (init_1string != NULL) {
			PUTS(init_1string);
		}
		FLUSH;

		if (reset && reset_2string != NULL) {
			PUTS(reset_2string);
		} else if (init_2string != NULL) {
			PUTS(init_2string);
		}
		FLUSH;

		if (set_lr_margin != NULL) {
			PUTS(tparm(set_lr_margin, 0, columns - 1));
		} else if (set_left_margin_parm != NULL
			   && set_right_margin_parm != NULL) {
			PUTS(tparm(set_left_margin_parm, 0));
			PUTS(tparm(set_right_margin_parm, columns - 1));
		} else if (clear_margins != NULL && set_left_margin != NULL
			   && set_right_margin != NULL) {
			PUTS(clear_margins);
			if (carriage_return != NULL) {
				PUTS(carriage_return);
			} else {
				PUTCHAR('\r');
			}
			PUTS(set_left_margin);
			if (parm_right_cursor) {
				PUTS(tparm(parm_right_cursor, columns - 1));
			} else {
				for(i = 0; i < columns - 1; i++) {
					PUTCHAR(' ');
				}
			}
			PUTS(set_right_margin);
			if (carriage_return != NULL) {
				PUTS(carriage_return);
			} else {
				PUTCHAR('\r');
			}
		}
		FLUSH;

		if (init_tabs != 8) {
			if (clear_all_tabs != NULL && set_tab != NULL) {
				for(i = 0; i < columns - 1; i += 8) {
					if (parm_right_cursor) {
						PUTS(tparm(parm_right_cursor,
						     8));
					} else {
						for(j = 0; j < 8; j++) {
							PUTCHAR(' ');
						}
					}
					PUTS(set_tab);
				}
				FLUSH;
			}
		}

		if (reset && reset_file != NULL) {
			f = fopen(reset_file, "r");
			if (f == NULL) {
				quit(errno, "Can't open reset_file: '%s'",
				     reset_file);
			}
			while((c = fgetc(f)) != EOF) {
				PUTCHAR(c);
			}
			fclose(f);
		} else if (init_file != NULL) {
			f = fopen(init_file, "r");
			if (f == NULL) {
				quit(errno, "Can't open init_file: '%s'",
				     init_file);
			}
			while((c = fgetc(f)) != EOF) {
				PUTCHAR(c);
			}
			fclose(f);
		}
		FLUSH;

		if (reset && reset_3string != NULL) {
			PUTS(reset_3string);
		} else if (init_2string != NULL) {
			PUTS(init_3string);
		}
		FLUSH;
		return 0;
	}

	s = tigetstr(argv[1]);

	if (s == (char *) -1) {
		quit(-1, "unknown capname '%s'", argv[1]);
	} else if (s == NULL) {
		return 0;
	}

	putp(s);

	return 0;
}
