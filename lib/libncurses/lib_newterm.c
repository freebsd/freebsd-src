
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_newterm.c
**
** 	The newterm() function.
**
*/

#include <stdlib.h>
#include "curses.priv.h"
#include <nterm.h>
#ifdef SVR4_ACTION
#define _POSIX_SOURCE
#endif
#include <signal.h>

#ifdef MYTINFO
int LINES, COLS;
#endif

static void cleanup(int sig)
{

	if (sig == SIGSEGV)
		fprintf(stderr, "Got a segmentation violation signal, cleaning up and exiting\n");
	endwin();
	exit(1);
}

WINDOW *stdscr, *curscr, *newscr;
SCREEN *SP;

struct ripoff_t
{
	int	line;
	int	(*hook)();
}
rippedoff[5], *rsp = rippedoff;

SCREEN * newterm(char *term, FILE *ofp, FILE *ifp)
{
sigaction_t act;
int	errret;
int	stolen, topstolen;

#ifdef TRACE
	_init_trace();
	T(("newterm(%s,%x,%x) called", term, ofp, ifp));
#endif

#ifdef MYTINFO
	if (setupterm(term, fileno(ofp), &errret) != OK)
	    	return NULL;
	COLS = cur_term->true_columns;
	LINES = cur_term->true_lines;
#else
	if (setupterm(term, fileno(ofp), &errret) != 1)
	    	return NULL;
#endif

	if ((SP = (SCREEN *) malloc(sizeof *SP)) == NULL)
	    	return NULL;

	if (ofp == stdout && ifp == stdin) {
	    	SP->_ofp       = stdout;
	    	SP->_ifp       = stdin;
	} else {
	    	SP->_ofp       = ofp;
	    	SP->_ifp       = ofp;
	}
	SP->_term      	= cur_term;
	SP->_cursrow   	= -1;
	SP->_curscol   	= -1;
	SP->_keytry    	= UNINITIALISED;
	SP->_nl        	= TRUE;
	SP->_raw       	= FALSE;
	SP->_cbreak    	= FALSE;
	SP->_echo      	= TRUE;
	SP->_nlmapping 	= TRUE;
	SP->_fifohead	= -1;
	SP->_fifotail 	= 0;
	SP->_fifopeek	= 0;

	if (enter_ca_mode)
	    	putp(enter_ca_mode);

	init_acs(); 

	T(("creating newscr"));
	if ((newscr = newwin(lines, columns, 0, 0)) == (WINDOW *)NULL)
	    	return(NULL);

	T(("creating curscr"));
	if ((curscr = newwin(lines, columns, 0, 0)) == (WINDOW *)NULL)
	    	return(NULL);

	SP->_newscr = newscr;
	SP->_curscr = curscr;

	newscr->_clear = TRUE;
	curscr->_clear = FALSE;

	stolen = topstolen = 0;
	for (rsp = rippedoff; rsp->line; rsp++)
	{
		if (rsp->hook)
			if (rsp->line < 0)
				rsp->hook(newwin(1,COLS, LINES-1,0), COLS);
			else
				rsp->hook(newwin(1,COLS, topstolen++,0), COLS);
		--LINES;
		stolen++;
	}

	act.sa_handler = tstp;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGTSTP, &act, NULL);
	act.sa_handler = cleanup;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);
#if 0
	sigaction(SIGSEGV, &act, NULL);
#endif
	if (stdscr == NULL)
	    	if ((stdscr = newwin(lines - stolen, columns, topstolen, 0)) == NULL)
			return(NULL);

	T(("newterm returns %x", SP));

	return(SP);
}

int
ripoffline(int line, int (*init)(WINDOW *, int))
{
    if (line == 0)
	return(OK);

    if (rsp >= rippedoff + sizeof(rippedoff)/sizeof(rippedoff[0]))
	return(ERR);

    rsp->line = line;
    rsp->hook = init;
    rsp++;

    return(OK);
}

