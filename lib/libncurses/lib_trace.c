
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
#include "terminfo.h"

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


void trace(const unsigned int tracelevel)
{
   	_tracing = tracelevel;
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
			   		colors[FG(color_pairs[pairnum])].name,
					colors[BG(color_pairs[pairnum])].name
			   );
	}
	if ((newmode & A_ATTRIBUTES) == 0)
	    strcat(buf,"A_NORMAL, ");
	if (buf[strlen(buf) - 2] == ',')
	    buf[strlen(buf) - 2] = '\0';
	return(strcat(buf,"}"));
}

char *visbuf(const char *buf)
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

char *_tracechar(const unsigned char ch)
{
    static char crep[20];
    /* 
     * We can show the actual character if it's either an ordinary printable
     * or one of the high-half characters.
     */
    if (isprint(ch) || (ch & 0x80))
    {
	crep[0] = '\'';
	crep[1] = ch;	/* necessary; printf tries too hard on metachars */
	(void) sprintf(crep + 2, "' = 0x%02x", ch);
    }
    else
	(void) sprintf(crep, "0x%02x", ch);
    return(crep);
}

void
_tracef(char *fmt, ...)
{
va_list ap;
char buffer[256];

	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	write(tracefd, buffer, strlen(buffer));
	write(tracefd, "\n", 1);
}

