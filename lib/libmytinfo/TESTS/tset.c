/*
 * tset.c
 * 
 * By Ross Ridge
 * Public Domain
 * 92/02/19 18:53:12
 *
 */

#define NOTLIB

#include "defs.h"

const char SCCSid[] = "@(#) mytinfo tset.c 3.3 92/02/19 public domain, By Ross Ridge";

#define SINGLE
#include <term.h>

#include <ctype.h>

#if defined(USE_TERMIO) || defined(USE_TERMIOS)

#ifndef ICANON
#ifdef USE_TERMIO
#include <termio.h>
#else
#include <termios.h>
#endif
#endif

#undef USE_SUSPEND
#ifdef VSUSP
#define USE_SUSPEND
#endif

#define USE_INTERRUPT

#if defined(USE_TERMIO) && !defined(TCSETS)
#undef USE_SUSPEND
#endif

#else /* USE_TERMIO */
#ifdef USE_SGTTY

#ifndef CBREAK
#include <sgtty.h>
#endif

#undef USE_INTERRUPT
#ifdef TIOCGETC
#define USE_INTERRUPT
#endif

#undef USE_SUSPEND
#ifdef TIOCGLTC
#define USE_SUSPEND
#endif

#undef USE_NEWBSDTTY
#ifdef TIOCSETD
#ifdef NTTYDISC
#define USE_NEWBSDTTY
#endif
#endif

#else /* USE_SGTTY */

#undef USE_SUSPEND
#define USE_NOTTY

#endif /* else USE_SGTTY */
#endif /* else USE_TERMIO */

#ifndef key_interrupt_char
#undef USE_INTERRUPT
#endif

#ifndef key_suspend_char
#undef USE_SUSPEND
#endif

char *term;
int asked = 0;
long baudrate;

#ifndef USE_STDLIB
#ifdef USE_PROTOTYPES
long atol(char const *);
#else
long atol();
#endif
#endif

#define OUTPUT_TERM	1
#define OUTPUT_SHELL	2
#define OUTPUT_TERMCAP	3

#define PUTS(s)		tputs(s, 1, putch)
#define PUTCHAR(c)	putc(c, stderr)
#define FLUSH		fflush(stderr)

#ifdef USE_SMALLMEM
extern unsigned short _baud_tbl[];
#else
extern long _baud_tbl[];
#endif

extern void (*cleanup)();

static void
clean(e)
int e; {
	return;
}

static int
#ifdef USE_PROTOTYPES
putch(char c)
#else
putch(c)
int c;
#endif
{
	return putc(c, stderr);
}

void
set_term(name, query)
char *name;
int query; {
	static char buf[MAX_LINE];
	char *s, *t;

	if (query) {
		fprintf(stderr, "TERM = (%s) ", name);
		fflush(stderr);
		asked = 1;
		if (fgets(buf, sizeof(buf), stdin) != NULL) {
			s = buf;
			while(*s != '\0' && isspace(*s))
				s++;
			t = s;
			while(*s != '\0' && !isspace(*s))
				s++;
			*s = '\0';
			if (*t != '\0') {
				term = t;
				return;
			}
		}
	}
	term = strcpy(buf, name);
}

int
map_term(map, ask)
char *map;
int ask; {
	char *type;
	char test = 0;
	char *baud = NULL;

	type = map;
#ifndef USE_NOTTY
	while(*map && strchr(">@<!:?", *map) == NULL)
		map++;
	if (*map != '\0' && *map != ':' && *map != '?') {
		if (*map == '!') {
			switch(*++map) {
			case '>': test = 'g'; break;
			case '@': test = 'n'; break;
			case '<': test = 'l'; break;
			default:
				quit(-1, "bad argument to 'm' switch");
			}
		} else
			test = *map;
		*map++ = '\0';
		baud = map;
		while(*map && *map != ':')
			map++;
	}

#else
	while(*map && *map != ':' && *map != '?')
		map++;
#endif

	if (*map == ':')
		*map++ = '\0';
	if (*map == '?') {
		ask = 1;
		*map++ = '\0';
	}
	if (*map == '\0') 
		quit(-1, "bad argument to 'm' switch");

	if (type[0] != '\0' && strcmp(type, term) != 0) {
		return 0;
	}

#ifndef USE_NOTTY
	switch(test) {
	case '>':
		if (baudrate <= atol(baud))
			return 0;
		break;
	case '<':
		if (baudrate >= atol(baud))
			return 0;
		break;
	case '@':
		if (baudrate != atol(baud))
			return 0;
		break;
	case 'l':
		if (baudrate < atol(baud))
			return 0;
		break;
	case 'g':
		if (baudrate > atol(baud))
			return 0;
		break;
	case 'n':
		if (baudrate == atol(baud))
			return 0;
		break;
	}
#endif

	set_term(map, ask);
	return 1;
}

int
conv_char(s)
char *s; {
	if (s[0] == '^' && s[1] >= '@' && s[1] < '\177')
		return s[1] & 31;
	else if (s[0] == '^' && s[1] == '?')
		return '\177';
	else if (s[0] != '\0')
		return s[0];
	else
		return -2;
}

char *
expand_char(c)
int c; {
	static char buf[5];

	if (c < 0 || c > 127) {
		sprintf(buf, "\\%03o", c & 0177);
	} else if (c == 127) {
		return "DEL";
	} else if (c < 32) {
		buf[0] = '^';
		buf[1] = c + 64;
		buf[2] = '\0';
	} else {
		buf[0] = c;
		buf[1] = '\0';
	}

	return buf;
}

#define START 	1
#define COPY	2
#define ESCAPE	3

void
compress_buf(buf)
char *buf; {
	char *s, *d;
	int state = START;

	d = s = buf;

	while(*s) {
		switch(state) {
		case START:
			if (isspace(*s) || *s == ':') {
				s++;
				break;
			}
			state = COPY;
			/* FALLTHROUGH */
		case COPY:
			switch(*s) {
			case '^':
			case '\\':
				state = ESCAPE;
				break;
			case ':':
				state = START;
				break;
			}
			*d++ = *s++;
			break;

		case ESCAPE:
			*d++ = *s++;
			state = COPY;
			break;
		}
	}
}

static void
usage(e)
int e; {
#ifdef USE_ANSIC
	fprintf(stderr, "usage: %s [-] [-"
#define ARG(s)	s
#else
	fprintf(stderr, "usage: %s [-] [-", prg_name);
#define ARG(s)	fputs(s, stderr);
#endif
			ARG("l")
#ifndef USE_NOTTY
#ifdef USE_NEWBSDTTY
			ARG("n")
#endif
#endif
			ARG("rsAI")
#ifndef USE_NOTTY
			ARG("Q")
#endif
			ARG("S")
#ifndef USE_NOTTY
			ARG("T")
#endif
			ARG("]")
#ifndef USE_NOTTY
			ARG(" [-e[c]] [-E[c]] [-k[c]]")
#ifdef USE_INTERRUPT
			ARG(" [-i[c]]")
#endif
#ifdef USE_SUSPEND
			ARG(" [-z[c]]")
#endif
#endif
			ARG("\n\t[-m ident")
#ifndef USE_NOTTY
			ARG("[[!][<@>]speed]")
#endif
			ARG(":[?]term] [term]\n")
#ifdef USE_ANSIC
			, prg_name);
#endif

#undef ARG

	return;
}

int
main(argc, argv)
int argc;
char **argv; {
	char *s;
	int i, j, r, c;
	int ask = 0;
#ifndef USE_NOTTY
	int erase_char = -1;
	int erase_if_bs = 0;
	int kill_char = -1;
#ifdef USE_INTERRUPT
	int interrupt_char = -1;
#endif
#ifdef USE_SUSPEND
	int suspend_char = -1;
#endif
#ifdef USE_NEWBSDTTY
	int newbsdtty = 0;
#endif
#endif /* !USE_NOTTY */
	int output_type = 0;
	int no_term_init = 0;
	int term_type_is = 0;
	int no_set_to = 0;
	int reset = 0;
	int matched = 0;
	int expand_tabs = -1;
	int is_csh;
#ifndef USE_NOTTTY
#if defined(USE_TERMIOS) || defined(USE_TERMIO) && defined(TCSETS)
	struct termios tty;
#else
#ifdef USE_TERMIO
	struct termio tty;
#else
#ifdef USE_SGTTY
	struct sgttyb tty;
#ifdef USE_INTERRUPT
	struct tchars tty2;
#endif
#ifdef USE_SUSPEND
	struct ltchars tty3;
#endif
#endif /* USE_SGTTY */
#endif /* else USE_TERMIO */
#endif
#endif /* !USE_NOTTY */
	struct term_path *path;
	char buf[MAX_BUF];
	FILE *f;
	int datatype;

	prg_name = argv[0];
	s = strrchr(prg_name, '/');
	if (s != NULL && *++s != '\0') {
		if (strcmp(s, "reset") == 0) {
			reset = 1;
		}
		prg_name = s;
	}

	cleanup = clean;

#ifndef USE_NOTTY
#ifdef USE_TERMIOS
	if (tcgetattr(2, &tty) == -1)
		quit(errno, "tcgetattr failed");
	baudrate = cfgetospeed(&tty);
#else
#ifdef USE_TERMIO
#ifdef TCSETS
	if (ioctl(2, TCGETS, &tty) == -1)
#else
	if (ioctl(2, TCGETA, &tty) == -1)
#endif
	{
		quit(errno, "ioctl failed");
	}
	baudrate = _baud_tbl[tty.c_cflag & CBAUD];
#else
#ifdef USE_SGTTY
	if (gtty(2, &tty) == -1) {
		quit(errno, "gtty failed");
	}
	baudrate = _baud_tbl[tty.sg_ospeed];
#ifdef USE_INTERRUPT
	if (ioctl(2, TIOCGETC, &tty2) == -1) {
		quit(errno, "ioctl failed");
	}
#endif
#ifdef USE_SUSPEND
	if (ioctl(2, TIOCGLTC, &tty3) == -1) {
		quit(errno, "ioctl failed");
	}
#endif
#endif /* USE_SGTTY */
#endif /* else USE_TERMIO */
#endif
#endif /* !USE_NOTTY */

	term = getenv("TERM");

	cleanup = usage;

	for(i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (term == NULL) {
				term = argv[i];
			}
			continue;
		}
		s = argv[i] + 1;
		if (*s == '\0') {
			output_type = OUTPUT_TERM;
			continue;
		}
		while (*s != '\0') {
			switch(*s++) {
			case 'A':
				ask = 1;
				continue;
#ifndef USE_NOTTY
			case 'E':
				erase_if_bs = 1;
				/* FALLTHROUGH */
			case 'e':
				erase_char = conv_char(s);
				break;
			case 'k':
				kill_char = conv_char(s);
				break;
#ifdef USE_INTERRUPT
			case 'i':
				interrupt_char = conv_char(s);
				break;
#endif
#ifdef USE_SUSPEND
			case 'z':
				suspend_char = conv_char(s);
				break;
#endif
			case 'T':
				erase_char = kill_char
#ifdef USE_INTERRUPT
					= interrupt_char
#endif
#ifdef USE_SUSPEND
					= suspend_char
#endif
					= -2;
				continue;
#ifdef USE_NEWBSDTTY
			case 'n':
				newbsdtty = 1;
				continue;
#endif
			case 'Q':
				no_set_to = 1;
				continue;
#endif /* !USE_NOTTY */
			case 'l':
			case 'I':
				no_term_init = 1;
				continue;
			case 'r':
				term_type_is = 1;
				continue;
			case 's':
				output_type = OUTPUT_SHELL;
				continue;
			case 'S':
				output_type = OUTPUT_TERMCAP;
				continue;
			case 'm':
				if (*s == '\0') {
					if (i == argc - 1) {
						quit(-1, "'m' switch requires an argument.");
					}
					s = argv[++i];
				}
				if (!matched) {
					matched = map_term(s, ask);
				}
				break;
			default:
				quit(-1, "unknown switch '%c'", s[-1]);
				break;
			}
			break;
		}
	}
	
	cleanup = clean;

	path = _buildpath("$MYTERMINFO", 2,
			  "$TERMINFO", 2,
			  "$TERMCAP", 1,
#ifdef TERMINFODIR
			  TERMINFODIR, 0,
#endif
#ifdef TERMCAPFILE
			  TERMCAPFILE, 0,
#endif
#ifdef TERMINFOSRC
			  TERMINFOSRC, 0,
#endif
			  NULL, -1);

	if (path == NULL) {
		quit(-1, "malloc error");
	}

	do {
		if (term == NULL) {
			term = "unknown";
		}

		if (ask && !asked) {
			set_term(term, 1);
		}

		datatype = _fillterm(term, path, buf);

		switch(datatype) {
		case -3:
			quit(-1, "malloc error");
			/* NOTREACHED */
		case -2:
			quit(-1, "database in bad format");
			/* NOTREACHED */
		case -1:
			/* quit(-1, "database not found"); */
		case 0:
			if (ask || asked) {
				fprintf(stderr, "terminal type '%s' unknown.\n",
					term);
				term = NULL;
				ask = 1;
				asked = 0;
			} else {
				quit(-1, "terminal type '%s' unknown.\n", term);
			}
			break;
		case 1:
		case 2:
		case 3:
			break;
		default:
			quit(-1, "oops...");
		}
	} while(term == NULL);

	_delpath(path);

	cur_term->baudrate = baudrate;

	if (!no_term_init) {

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
				if (carriage_return != NULL) {
					PUTS(carriage_return);
				} else {
					PUTCHAR('\r');
				}
				PUTS(clear_all_tabs);
				PUTS(set_tab);
				for(i = 8; i < columns - 1; i += 8) {
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
				if (carriage_return != NULL) {
					PUTS(carriage_return);
				} else {
					PUTCHAR('\r');
				}
				expand_tabs = 0;
				FLUSH;
			} else {
				expand_tabs = 1;
			}
		} else {
			expand_tabs = 0;
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
	}

	if (term_type_is) {
		fprintf(stderr, "Terminal type is %s\n", term);
	}

#ifndef USE_NOTTY
#if defined(USE_TERMIO) || defined(USE_TERMIOS)

#ifdef USE_TERMIO
	if (expand_tabs == 0 && (tty.c_cflag & TABDLY) == TAB3) {
		tty.c_cflag = tty.c_cflag & ~TABDLY;
	} else if (expand_tabs == 1) {
		tty.c_cflag = (tty.c_cflag & ~TABDLY) | TAB3;
	}
#else
	if (!expand_tabs)
		tty.c_oflag &= ~OXTABS;
	else
		tty.c_oflag |= OXTABS;
#endif

#define SETTO(v, m, t, c) (c != tty.c_cc[v] ? (tty.c_cc[v] = c,		  \
			   !no_set_to ? fprintf(stderr, "%s set to %s\n", t, \
					     expand_char(c)) : 0) : 0)

#endif
#ifdef USE_SGTTY
	
#ifdef USE_NEWBSDTTY
	if (newbsdtty) {
		if (ioctl(2, TIOCSETD, NTTYDISC) == -1) {
			quit(errno, "Can't switch tty to new line disc.");
		}
	}
#endif

	if (expand_tabs == 0) {
		tty.sg_flags &= ~XTABS;
	} else if (expand_tabs == 1) {
		tty.sg_flags |= XTABS;
	}

#define SETTO(v, m, t, c) (c != m ? (m = c,				  \
			   !no_set_to ? fprintf(stderr, "%s set to %s\n", t, \
					     expand_char(c)) : 0) : 0)
#endif

	if (erase_char != -1
            && (!erase_if_bs || backspaces_with_bs
	        || (cursor_left == NULL && cursor_left[0] != '\b'))) {
		if (erase_char != -2) {
			c = erase_char;
		} else if (cursor_left == NULL || cursor_left[0] == '\0'
			 || cursor_left[1] != '\0') {
			c = '\b';
		} else {
			c = cursor_left[0];
		}
		SETTO(VERASE, tty.sg_erase, "Erase", c);
	}

	if (kill_char != -1) {
		if (kill_char != -2) {
			c = kill_char;
		} else if (key_kill_char == NULL || key_kill_char[0] == '\0'
			   || key_kill_char[1] != '\0') {
			c = '\025';
		} else {
			c = key_kill_char[0];
		}
		SETTO(VKILL, tty.sg_kill, "Kill", c);
	}

	
#ifdef USE_INTERRUPT
	if (interrupt_char != -1) {
		if (interrupt_char != -2) {
			c = interrupt_char;
		} else if (key_interrupt_char == NULL
			   || key_interrupt_char[0] == '\0'
			   || key_interrupt_char[1] != '\0') {
			c = '\177';
		} else {
			c = key_interrupt_char[0];
		}
		SETTO(VINTR, tty2.t_intrc, "Interrupt", c);
	}
#endif

#ifdef USE_SUSPEND
	if (suspend_char != -1) {
		if (suspend_char != -2) {
			c = suspend_char;
		} else if (key_suspend_char == NULL
			   || key_suspend_char[0] == '\0'
			   || key_suspend_char[1] != '\0') {
			c = '\032';
		} else {
			c = key_suspend_char[0];
		}
		SETTO(VSUSP, tty3.t_suspc, "Suspend", c);
	}
#endif

#ifdef USE_TERMIOS
	if (tcsetattr(2, TCSADRAIN, &tty) == -1)
		quit(errno, "tcsetattr failed");
#else
#ifdef USE_TERMIO
#ifdef TCSETS
	if (ioctl(2, TCSETS, &tty) == -1)
#else
	if (ioctl(2, TCSETA, &tty) == -1)
#endif
	{
		quit(errno, "ioctl failed");
	}
#else
#ifdef USE_SGTTY
	if (stty(2, &tty) == -1) {
		quit(errno, "stty failed");
	}
#ifdef USE_INTERRUPT
	if (ioctl(2, TIOCSETC, &tty2) == -1) {
		quit(errno, "ioctl failed");
	}
#endif
#ifdef USE_SUSPEND
	if (ioctl(2, TIOCSLTC, &tty3) == -1) {
		quit(errno, "ioctl failed");
	}
#endif
#endif /* USE_SGTTY */
#endif /* else USE_TERMIO */
#endif
#endif /* !USE_NOTTY */

	s = getenv("SHELL");
	r = strlen(s);

	if (r >= 3 && strcmp("csh", s + r - 3) == 0) {
		is_csh = 1;
	} else {
		is_csh = 0;
	}

	switch(output_type) {
	case OUTPUT_TERM:
		fprintf(stdout, "%s\n", term);
		break;

	case OUTPUT_TERMCAP:
		if (is_csh) {
			if (datatype == 1) {
				compress_buf(buf);
				fprintf(stdout, "%s %s", term, buf);
			} else {
				s = getenv("TERMCAP");
				if (s == NULL || *s == '\0') {
					s = ":";
				}
				fprintf(stdout, "%s %s", term, s);
			}
			break;
		}
		/* FALLTHROUGH */
	case OUTPUT_SHELL:
		if (is_csh) {
			fprintf(stdout, "set noglob;\n");
			fprintf(stdout, "setenv TERM '%s';\n", term);
			if (datatype == 1) {
				compress_buf(buf);
				fprintf(stdout, "setenv TERMCAP '%s';\n", buf);
			}
			fprintf(stdout, "unset noglob;\n");
		} else {
			fprintf(stdout, "export TERM TERMCAP;\n");
			fprintf(stdout, "TERM='%s';\n", term);
			if (datatype == 1) {
				compress_buf(buf);
				fprintf(stdout, "TERMCAP='%s';\n", buf);
			}
		}
		break;
	}
	
	return 0;
}
