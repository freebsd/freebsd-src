/*
 * tputs.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/06/04 11:41:15
 *
 * Output a terminal capability string with any needed padding
 *
 */

#include "defs.h"
#include <term.h>

#include <ctype.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo tputs.c 3.3 92/06/04 public domain, By Ross Ridge";
#endif

#ifdef TEST
#define def_prog_mode()	(OK)
#define _norm_output()	((void)(0))
#define _lit_output()	(1)
#endif

/*
 * BITSPERCHAR (as actually transmitted over a serial line) is usually 10
 * (not 8). 1 stop bit, 1 start bit, 7 data bits, and one parity bit.
 */

#define BITSPERCHAR	10

#ifdef USE_FAKE_STDIO
#undef putchar
#endif

#ifdef USE_PROTOTYPES
#define PUTCHAR(c) (outc == (int (*)(int)) NULL ? (putchar(c)):(*outc)(c))
#else
#define PUTCHAR(c) (outc == (int (*)()) NULL ? (putchar(c)):(*outc)(c))
#endif

int
tputs(sp, count, outc)
register const char *sp;
int count;
#ifdef USE_PROTOTYPES
register int (*outc)(int);
#else
register int (*outc)();
#endif
{
	register int l;
	register long cnt;
	int must_pad, multiply_pad;
	int forced_lit = 0;

	/* some programmes expect this behaviour from tputs */
	if (sp == NULL) {
#ifdef DEBUG
		fprintf(stderr, "tputs: NULL\n");	
#endif
		return 0;
	}

	if (cur_term->termcap) {
		_figure_termcap();
	}

	while(*sp != '\0') {
		switch(*sp) {
		case '\\':
			switch(*++sp) {
			case 'n': PUTCHAR('\n'); sp++; break;
			case 'b': PUTCHAR('\b'); sp++; break;
			case 't': PUTCHAR('\t'); sp++; break;
			case 'r': PUTCHAR('\r'); sp++; break;
			case 'f': PUTCHAR('\f'); sp++; break;
			case 'l': PUTCHAR('\012'); sp++; break;
			case 's': PUTCHAR(' '); sp++; break;
			case 'e': case 'E': PUTCHAR('\033'); sp++; break;

			case '^':
			case '\\':
			case ',':
			case ':':
			case '\'':
			case '$':
				PUTCHAR(*sp++);
				break;

			case '0':
				if (*(sp + 1) < '0' || *(sp + 1) > '7') {
					PUTCHAR('\200'); /* I'd prefer \0 */
					sp++;
					break;
				}
				;/* FALLTHROUGH */
			case '1': case '2': case '3': case '4':
			case '5': case '6': case '7':
				l = *sp++ - '0';
				if (*sp >= '0' && *sp <= '7') {
					l = l * 8 + (*sp++ - '0');
					if (*sp >= '0' && *sp <= '7')
						l = l * 8 + (*sp++ - '0');
				}
				PUTCHAR(l);
				break;

			case '\0':
				PUTCHAR('\\');
				break;

			case '@':
				if (!forced_lit)
					forced_lit = _lit_output();
				sp++;
				break;

			default:
				PUTCHAR('\\');
				PUTCHAR(*sp++);
				break;
			}
			break;
		case '^':
			if (*++sp == '\0')
				break;
			l = *sp - '@';
			if (l > 31)
				l -= 32;
			if (l < 0 || l > 31) {
				PUTCHAR('^');
				PUTCHAR(*sp++);
			} else {
				PUTCHAR(l);
				sp++;
			}
			break;
		case '$':
			if (*++sp != '<') {
				PUTCHAR('$');
				break;
			}
			must_pad = 0;
			multiply_pad = 0;
			l = 0;
			sp++;
			while (isdigit(*sp))
				l = l * 10 + (*sp++ - '0');
			l *= 10;
			if (*sp == '.') {
				sp++;
				if (isdigit(*sp))
					l += *sp++ - '0';
			}
			if (*sp == '/') {
				must_pad = 1;
				if (*++sp == '*') {
					multiply_pad = 1;
					sp++;
				}
			} else if (*sp == '*') {
				multiply_pad = 1;
				if (*++sp == '/') {
					must_pad = 1;
					sp++;
				}
			}
			if (*sp != '>') {
				PUTCHAR('p');
				PUTCHAR('a');
				PUTCHAR('d');
				PUTCHAR('?');
				break;
			}
			sp++;
#ifdef TEST
			printf("\nl = %d", l);
#endif
			if (cur_term->pad || must_pad) {
				cnt = ((long) l * cur_term->baudrate
				       * (multiply_pad ? count : 1) 
				       + (10000L * BITSPERCHAR / 2L))
				      / (10000L * BITSPERCHAR);
#ifdef TEST
				printf("; cnt = %ld\n", cnt);
#endif
				while(cnt--)
					PUTCHAR(cur_term->padch);
			}
#ifdef TEST
			printf("\n");
#endif
			break;
		default:
			PUTCHAR(*sp++);
		}
	}
	if (forced_lit)
		_norm_output();
	return OK;
}

int
putp(str)
char *str; {
#ifdef USE_PROTOTYPES
	return(tputs(str, 1,(int (*)(int)) NULL));
#else
	return(tputs(str, 1,(int (*)()) NULL));
#endif
}

#ifdef TEST

TERMINAL test_term, *cur_term = &test_term;

int
#ifdef USE_PROTOTYPES
putch(char c)
#else
putch(c)
char c;
#endif
{
	if (c & 0x80) {
		printf("\\%03o", c);
	} else if (c < 32) {
		printf("^%c", c + '@');
	} else if (c == 127) {
		printf("^?");
	} else {
		putchar(c);
	}
	return 0;
}

char line[MAX_LINE];

int
main(argc, argv)
int argc;
char **argv; {
	test_term.termcap = 0;
	test_term.baudrate = 1200;
	test_term.pad = 0;
	test_term.padch = 0;
	if (argc > 1) 
		test_term.baudrate = atoi(argv[1]);
	if (argc > 2)
		test_term.padch = argv[2][0];
	if (argc > 3)
		test_term.pad = 1;

	putchar('\n');

	while(fgets(line, sizeof(line), stdin) != NULL) {
		line[strlen(line)-1] = '\0';
		tputs(line, 7, putch);
		putchar('\n');
	}
	return 0;
}
#endif
