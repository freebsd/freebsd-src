/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 */

/*  This file contains the routines that interface to termcap and stty/gtty.
 *
 *  Paul Vixie, February 1987: converted to use ioctl() instead of stty/gtty.
 *
 *  I put in code to turn on the TOSTOP bit while top was running, but I
 *  didn't really like the results.  If you desire it, turn on the
 *  preprocessor variable "TOStop".   --wnl
 */

#include "os.h"
#include "top.h"

#include <sys/ioctl.h>
#ifdef CBREAK
# include <sgtty.h>
# define SGTTY
#else
# ifdef TCGETA
#  define TERMIO
#  include <termio.h>
# else
#  define TERMIOS
#  include <termios.h>
# endif
#endif
#if defined(TERMIO) || defined(TERMIOS)
# ifndef TAB3
#  ifdef OXTABS
#   define TAB3 OXTABS
#  else
#   define TAB3 0
#  endif
# endif
#endif
#include "screen.h"
#include "boolean.h"

extern char *myname;

int putstdout();

int  overstrike;
int  screen_length;
int  screen_width;
char ch_erase;
char ch_kill;
char smart_terminal;
char PC;
char *tgetstr();
char *tgoto();
char termcap_buf[1024];
char string_buffer[1024];
char home[15];
char lower_left[15];
char *clear_line;
char *clear_screen;
char *clear_to_end;
char *cursor_motion;
char *start_standout;
char *end_standout;
char *terminal_init;
char *terminal_end;
short ospeed;

#ifdef SGTTY
static struct sgttyb old_settings;
static struct sgttyb new_settings;
#endif
#ifdef TERMIO
static struct termio old_settings;
static struct termio new_settings;
#endif
#ifdef TERMIOS
static struct termios old_settings;
static struct termios new_settings;
#endif
static char is_a_terminal = No;
#ifdef TOStop
static int old_lword;
static int new_lword;
#endif

#define	STDIN	0
#define	STDOUT	1
#define	STDERR	2

init_termcap(interactive)

int interactive;

{
    char *bufptr;
    char *PCptr;
    char *term_name;
    char *getenv();
    int status;

    /* set defaults in case we aren't smart */
    screen_width = MAX_COLS;
    screen_length = 0;

    if (!interactive)
    {
	/* pretend we have a dumb terminal */
	smart_terminal = No;
	return;
    }

    /* assume we have a smart terminal until proven otherwise */
    smart_terminal = Yes;

    /* get the terminal name */
    term_name = getenv("TERM");

    /* if there is no TERM, assume it's a dumb terminal */
    /* patch courtesy of Sam Horrocks at telegraph.ics.uci.edu */
    if (term_name == NULL)
    {
	smart_terminal = No;
	return;
    }

    /* now get the termcap entry */
    if ((status = tgetent(termcap_buf, term_name)) != 1)
    {
	if (status == -1)
	{
	    fprintf(stderr, "%s: can't open termcap file\n", myname);
	}
	else
	{
	    fprintf(stderr, "%s: no termcap entry for a `%s' terminal\n",
		    myname, term_name);
	}

	/* pretend it's dumb and proceed */
	smart_terminal = No;
	return;
    }

    /* "hardcopy" immediately indicates a very stupid terminal */
    if (tgetflag("hc"))
    {
	smart_terminal = No;
	return;
    }

    /* set up common terminal capabilities */
    if ((screen_length = tgetnum("li")) <= 0)
    {
	screen_length = smart_terminal = 0;
	return;
    }

    /* screen_width is a little different */
    if ((screen_width = tgetnum("co")) == -1)
    {
	screen_width = 79;
    }
    else
    {
	screen_width -= 1;
    }

    /* terminals that overstrike need special attention */
    overstrike = tgetflag("os");

    /* initialize the pointer into the termcap string buffer */
    bufptr = string_buffer;

    /* get "ce", clear to end */
    if (!overstrike)
    {
	clear_line = tgetstr("ce", &bufptr);
    }

    /* get necessary capabilities */
    if ((clear_screen  = tgetstr("cl", &bufptr)) == NULL ||
	(cursor_motion = tgetstr("cm", &bufptr)) == NULL)
    {
	smart_terminal = No;
	return;
    }

    /* get some more sophisticated stuff -- these are optional */
    clear_to_end   = tgetstr("cd", &bufptr);
    terminal_init  = tgetstr("ti", &bufptr);
    terminal_end   = tgetstr("te", &bufptr);
    start_standout = tgetstr("so", &bufptr);
    end_standout   = tgetstr("se", &bufptr);

    /* pad character */
    PC = (PCptr = tgetstr("pc", &bufptr)) ? *PCptr : 0;

    /* set convenience strings */
    (void) strncpy(home, tgoto(cursor_motion, 0, 0), sizeof(home) - 1);
    home[sizeof(home) - 1] = '\0';
    /* (lower_left is set in get_screensize) */

    /* get the actual screen size with an ioctl, if needed */
    /* This may change screen_width and screen_length, and it always
       sets lower_left. */
    get_screensize();

    /* if stdout is not a terminal, pretend we are a dumb terminal */
#ifdef SGTTY
    if (ioctl(STDOUT, TIOCGETP, &old_settings) == -1)
    {
	smart_terminal = No;
    }
#endif
#ifdef TERMIO
    if (ioctl(STDOUT, TCGETA, &old_settings) == -1)
    {
	smart_terminal = No;
    }
#endif
#ifdef TERMIOS
    if (tcgetattr(STDOUT, &old_settings) == -1)
    {
	smart_terminal = No;
    }
#endif
}

init_screen()

{
    /* get the old settings for safe keeping */
#ifdef SGTTY
    if (ioctl(STDOUT, TIOCGETP, &old_settings) != -1)
    {
	/* copy the settings so we can modify them */
	new_settings = old_settings;

	/* turn on CBREAK and turn off character echo and tab expansion */
	new_settings.sg_flags |= CBREAK;
	new_settings.sg_flags &= ~(ECHO|XTABS);
	(void) ioctl(STDOUT, TIOCSETP, &new_settings);

	/* remember the erase and kill characters */
	ch_erase = old_settings.sg_erase;
	ch_kill  = old_settings.sg_kill;

#ifdef TOStop
	/* get the local mode word */
	(void) ioctl(STDOUT, TIOCLGET, &old_lword);

	/* modify it */
	new_lword = old_lword | LTOSTOP;
	(void) ioctl(STDOUT, TIOCLSET, &new_lword);
#endif
	/* remember that it really is a terminal */
	is_a_terminal = Yes;

	/* send the termcap initialization string */
	putcap(terminal_init);
    }
#endif
#ifdef TERMIO
    if (ioctl(STDOUT, TCGETA, &old_settings) != -1)
    {
	/* copy the settings so we can modify them */
	new_settings = old_settings;

	/* turn off ICANON, character echo and tab expansion */
	new_settings.c_lflag &= ~(ICANON|ECHO);
	new_settings.c_oflag &= ~(TAB3);
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	(void) ioctl(STDOUT, TCSETA, &new_settings);

	/* remember the erase and kill characters */
	ch_erase = old_settings.c_cc[VERASE];
	ch_kill  = old_settings.c_cc[VKILL];

	/* remember that it really is a terminal */
	is_a_terminal = Yes;

	/* send the termcap initialization string */
	putcap(terminal_init);
    }
#endif
#ifdef TERMIOS
    if (tcgetattr(STDOUT, &old_settings) != -1)
    {
	/* copy the settings so we can modify them */
	new_settings = old_settings;

	/* turn off ICANON, character echo and tab expansion */
	new_settings.c_lflag &= ~(ICANON|ECHO);
	new_settings.c_oflag &= ~(TAB3);
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	(void) tcsetattr(STDOUT, TCSADRAIN, &new_settings);

	/* remember the erase and kill characters */
	ch_erase = old_settings.c_cc[VERASE];
	ch_kill  = old_settings.c_cc[VKILL];

	/* remember that it really is a terminal */
	is_a_terminal = Yes;

	/* send the termcap initialization string */
	putcap(terminal_init);
    }
#endif

    if (!is_a_terminal)
    {
	/* not a terminal at all---consider it dumb */
	smart_terminal = No;
    }
}

end_screen()

{
    /* move to the lower left, clear the line and send "te" */
    if (smart_terminal)
    {
	putcap(lower_left);
	putcap(clear_line);
	fflush(stdout);
	putcap(terminal_end);
    }

    /* if we have settings to reset, then do so */
    if (is_a_terminal)
    {
#ifdef SGTTY
	(void) ioctl(STDOUT, TIOCSETP, &old_settings);
#ifdef TOStop
	(void) ioctl(STDOUT, TIOCLSET, &old_lword);
#endif
#endif
#ifdef TERMIO
	(void) ioctl(STDOUT, TCSETA, &old_settings);
#endif
#ifdef TERMIOS
	(void) tcsetattr(STDOUT, TCSADRAIN, &old_settings);
#endif
    }
}

reinit_screen()

{
    /* install our settings if it is a terminal */
    if (is_a_terminal)
    {
#ifdef SGTTY
	(void) ioctl(STDOUT, TIOCSETP, &new_settings);
#ifdef TOStop
	(void) ioctl(STDOUT, TIOCLSET, &new_lword);
#endif
#endif
#ifdef TERMIO
	(void) ioctl(STDOUT, TCSETA, &new_settings);
#endif
#ifdef TERMIOS
	(void) tcsetattr(STDOUT, TCSADRAIN, &new_settings);
#endif
    }

    /* send init string */
    if (smart_terminal)
    {
	putcap(terminal_init);
    }
}

get_screensize()

{

#ifdef TIOCGWINSZ

    struct winsize ws;

    if (ioctl (1, TIOCGWINSZ, &ws) != -1)
    {
	if (ws.ws_row != 0)
	{
	    screen_length = ws.ws_row;
	}
	if (ws.ws_col != 0)
	{
	    screen_width = ws.ws_col - 1;
	}
    }

#else
#ifdef TIOCGSIZE

    struct ttysize ts;

    if (ioctl (1, TIOCGSIZE, &ts) != -1)
    {
	if (ts.ts_lines != 0)
	{
	    screen_length = ts.ts_lines;
	}
	if (ts.ts_cols != 0)
	{
	    screen_width = ts.ts_cols - 1;
	}
    }

#endif /* TIOCGSIZE */
#endif /* TIOCGWINSZ */

    (void) strncpy(lower_left, tgoto(cursor_motion, 0, screen_length - 1),
	sizeof(lower_left) - 1);
    lower_left[sizeof(lower_left) - 1] = '\0';
}

standout(msg)

char *msg;

{
    if (smart_terminal)
    {
	putcap(start_standout);
	fputs(msg, stdout);
	putcap(end_standout);
    }
    else
    {
	fputs(msg, stdout);
    }
}

clear()

{
    if (smart_terminal)
    {
	putcap(clear_screen);
    }
}

clear_eol(len)

int len;

{
    if (smart_terminal && !overstrike && len > 0)
    {
	if (clear_line)
	{
	    putcap(clear_line);
	    return(0);
	}
	else
	{
	    while (len-- > 0)
	    {
		putchar(' ');
	    }
	    return(1);
	}
    }
    return(-1);
}

go_home()

{
    if (smart_terminal)
    {
	putcap(home);
    }
}

/* This has to be defined as a subroutine for tputs (instead of a macro) */

putstdout(ch)

char ch;

{
    putchar(ch);
}

