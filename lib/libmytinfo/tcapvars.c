/*
 * tcapvars.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:21
 *
 */

#include "defs.h"
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo tcapvars.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

char PC;
short ospeed;
char *UP, *BC;

void
_figure_termcap() {
#if defined(USE_SGTTY) || defined(USE_TERMIO) || defined(USE_TERMIOS)
#ifdef USE_TERMIOS
	extern speed_t _baud_tbl[];
#else
#ifdef USE_SMALLMEM
	extern unsigned short _baud_tbl[];
#else
	extern long _baud_tbl[];
#endif
#endif
	cur_term->padch = PC;
	cur_term->baudrate = _baud_tbl[ospeed];
#endif
}
