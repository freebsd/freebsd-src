/****************************************************************************
 * Copyright (c) 2013 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/*
 * Author: Thomas E. Dickey
 *
 * $Id: dots_termcap.c,v 1.7 2013/09/28 21:50:35 tom Exp $
 *
 * A simple demo of the termcap interface.
 */
#define USE_TINFO
#include <test.priv.h>

#if !defined(__MINGW32__)
#include <sys/time.h>
#endif

#if HAVE_TGETENT

#include <time.h>

#define valid(s) ((s != 0) && s != (char *)-1)

static bool interrupted = FALSE;
static long total_chars = 0;
static time_t started;

static char *t_AB;
static char *t_AF;
static char *t_cl;
static char *t_cm;
static char *t_me;
static char *t_mr;
static char *t_oc;
static char *t_op;
static char *t_ve;
static char *t_vi;

static struct {
    const char *name;
    char **value;
} my_caps[] = {

    {
	"AB", &t_AB
    },
    {
	"AF", &t_AF
    },
    {
	"cl", &t_cl
    },
    {
	"cm", &t_cm
    },
    {
	"me", &t_me
    },
    {
	"mr", &t_mr
    },
    {
	"oc", &t_oc
    },
    {
	"op", &t_op
    },
    {
	"ve", &t_ve
    },
    {
	"vi", &t_vi
    },
};

static
TPUTS_PROTO(outc, c)
{
    int rc = c;

    if (interrupted) {
	char tmp = (char) c;
	if (write(STDOUT_FILENO, &tmp, (size_t) 1) == -1)
	    rc = EOF;
    } else {
	rc = putc(c, stdout);
    }
    TPUTS_RETURN(rc);
}

static bool
outs(char *s)
{
    if (valid(s)) {
	tputs(s, 1, outc);
	return TRUE;
    }
    return FALSE;
}

static void
cleanup(void)
{
    outs(t_me);
    if (!outs(t_oc))
	outs(t_op);
    outs(t_cl);
    outs(t_ve);

    printf("\n\n%ld total chars, rate %.2f/sec\n",
	   total_chars,
	   ((double) (total_chars) / (double) (time((time_t *) 0) - started)));
}

static void
onsig(int n GCC_UNUSED)
{
    interrupted = TRUE;
}

static double
ranf(void)
{
    long r = (rand() & 077777);
    return ((double) r / 32768.);
}

static void
my_napms(int ms)
{
#if defined(__MINGW32__) || !HAVE_GETTIMEOFDAY
    Sleep(ms);
#else
    struct timeval data;
    data.tv_sec = 0;
    data.tv_usec = ms * 1000;
    select(0, NULL, NULL, NULL, &data);
#endif
}

int
main(int argc GCC_UNUSED,
     char *argv[]GCC_UNUSED)
{
    int x, y, z, p;
    int num_colors;
    int num_lines;
    int num_columns;
    double r;
    double c;
    char buffer[1024];
    char area[1024];
    char *name;

    CATCHALL(onsig);

    srand((unsigned) time(0));

    if ((name = getenv("TERM")) == 0) {
	fprintf(stderr, "TERM is not set\n");
	ExitProgram(EXIT_FAILURE);
    } else if (tgetent(buffer, name) < 0) {
	fprintf(stderr, "terminal description not found\n");
	ExitProgram(EXIT_FAILURE);
    } else {
	size_t t;
	char *ap = area;
	for (t = 0; t < SIZEOF(my_caps); ++t) {
	    *(my_caps[t].value) = tgetstr((NCURSES_CONST char *)
					  my_caps[t].name, &ap);
	}
    }

    num_colors = tgetnum("Co");
    num_lines = tgetnum("li");
    num_columns = tgetnum("co");

    outs(t_cl);
    outs(t_vi);
    if (num_colors > 1) {
	if (!valid(t_AF)
	    || !valid(t_AB)
	    || (!valid(t_oc) && !valid(t_op)))
	    num_colors = -1;
    }

    r = (double) (num_lines - 4);
    c = (double) (num_columns - 4);
    started = time((time_t *) 0);

    while (!interrupted) {
	x = (int) (c * ranf()) + 2;
	y = (int) (r * ranf()) + 2;
	p = (ranf() > 0.9) ? '*' : ' ';

	tputs(tgoto(t_cm, x, y), 1, outc);
	if (num_colors > 0) {
	    z = (int) (ranf() * num_colors);
	    if (ranf() > 0.01) {
		tputs(tgoto(t_AF, 0, z), 1, outc);
	    } else {
		tputs(tgoto(t_AB, 0, z), 1, outc);
		my_napms(1);
	    }
	} else if (valid(t_me)
		   && valid(t_mr)) {
	    if (ranf() <= 0.01) {
		outs((ranf() > 0.6)
		     ? t_mr
		     : t_me);
		my_napms(1);
	    }
	}
	outc(p);
	fflush(stdout);
	++total_chars;
    }
    cleanup();
    ExitProgram(EXIT_SUCCESS);
}
#else
int
main(int argc GCC_UNUSED,
     char *argv[]GCC_UNUSED)
{
    fprintf(stderr, "This program requires termcap\n");
    exit(EXIT_FAILURE);
}
#endif
