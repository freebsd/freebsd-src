
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
 *	lib_trace.c - Tracing/Debugging routines
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include "curses.priv.h"
#include <nterm.h>

#if defined(BRAINDEAD)
extern int errno;
#endif

int _tracing = 0;  

static int	tracefd;

void _tracef(char *fmt, ...);

void _init_trace()
{
static int	been_here = 0;

	if (! been_here) {
	   	been_here = 1;

	   	if ((tracefd = creat("trace", 0644)) < 0) {
			write(2, "curses: Can't open 'trace' file: ", 33);
			write(2, strerror(errno), strlen(strerror(errno)));
			write(2, "\n", 1);
			exit(1);
	   	}
	   	_tracef("TRACING NCURSES version %s", NCURSES_VERSION);
	}
}


void traceon()
{

   	_tracing = 1;
}


void traceoff()
{

   	_tracing = 0;
}

char *_traceattr(int newmode)
{
static char	buf[BUFSIZ];
struct {unsigned int val; char *name;}
names[] =
    {
	{A_STANDOUT,	"A_STANDOUT, ",},
	{A_UNDERLINE,	"A_UNDERLINE, ",},	
	{A_REVERSE,	"A_REVERSE, ",},
	{A_BLINK,	"A_BLINK, ",},
	{A_DIM,		"A_DIM, ",},
	{A_BOLD,		"A_BOLD, ",},	
	{A_ALTCHARSET,	"A_ALTCHARSET, ",},	
	{A_INVIS,	"A_INVIS, ",},
	{A_PROTECT,	"A_PROTECT, ",},
	{A_CHARTEXT,	"A_CHARTEXT, ",},	
	{A_NORMAL,	"A_NORMAL, ",},
    },
colors[] =
    {
	{COLOR_BLACK,	"COLOR_BLACK",},
	{COLOR_RED,	"COLOR_RED",},
	{COLOR_GREEN,	"COLOR_GREEN",},
	{COLOR_YELLOW,	"COLOR_YELLOW",},
	{COLOR_BLUE,	"COLOR_BLUE",},
	{COLOR_MAGENTA,	"COLOR_MAGENTA",},
	{COLOR_CYAN,	"COLOR_CYAN",},
	{COLOR_WHITE,	"COLOR_WHITE",},
    },
    *sp;

	strcpy(buf, "{");
	for (sp = names; sp->val; sp++)
	    if (newmode & sp->val)
		strcat(buf, sp->name);
	if (newmode & A_COLOR)
	{
	    int pairnum = PAIR_NUMBER(newmode);

	    (void) sprintf(buf + strlen(buf),
					"COLOR_PAIR(%d) = (%s, %s), ",
					pairnum,
			   		colors[BG(color_pairs[pairnum])].name,
					colors[FG(color_pairs[pairnum])].name
			   );
	}
	if ((newmode & A_ATTRIBUTES) == 0)
	    strcat(buf,"A_NORMAL, ");
	if (buf[strlen(buf) - 2] == ',')
	    buf[strlen(buf) - 2] = '\0';
	return(strcat(buf,"}"));
}

static char *visbuf(const char *buf)
/* visibilize a given string */
{
    static char vbuf[BUFSIZ];
    char *tp = vbuf;

    while (*buf)
    {
	if (isprint(*buf) || *buf == ' ')
	    *tp++ = *buf++;
	else if (*buf == '\n')
	{
	    *tp++ = '\\'; *tp++ = 'n';
	    buf++;
	}
	else if (*buf == '\r')
	{
	    *tp++ = '\\'; *tp++ = 'r';
	    buf++;
	}
	else if (*buf == '\b')
	{
	    *tp++ = '\\'; *tp++ = 'b';
	    buf++;
	}
	else if (*buf == '\033')
	{
	    *tp++ = '\\'; *tp++ = 'e';
	    buf++;
	}
	else if (*buf < ' ')
	{
	    *tp++ = '\\'; *tp++ = '^'; *tp++ = '@' + *buf;
	    buf++;
	}
	else
	{
	    (void) sprintf(tp, "\\0x%02x", *buf++);
	    tp += strlen(tp);
	}
    }
    *tp++ = '\0';
    return(vbuf);
}

void
_tracef(char *fmt, ...)
{
va_list ap;
char buffer[256];
char *vp;

	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	vp = visbuf(buffer);
	write(tracefd, vp, strlen(vp));
	write(tracefd, "\n", 1);
}

