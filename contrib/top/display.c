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

/*
 *  This file contains the routines that display information on the screen.
 *  Each section of the screen has two routines:  one for initially writing
 *  all constant and dynamic text, and one for only updating the text that
 *  changes.  The prefix "i_" is used on all the "initial" routines and the
 *  prefix "u_" is used for all the "updating" routines.
 *
 *  ASSUMPTIONS:
 *        None of the "i_" routines use any of the termcap capabilities.
 *        In this way, those routines can be safely used on terminals that
 *        have minimal (or nonexistant) terminal capabilities.
 *
 *        The routines are called in this order:  *_loadave, i_timeofday,
 *        *_procstates, *_cpustates, *_memory, *_message, *_header,
 *        *_process, u_endscreen.
 */

#include "os.h"
#include <ctype.h>
#include <time.h>

#include "screen.h"		/* interface to screen package */
#include "layout.h"		/* defines for screen position layout */
#include "display.h"
#include "top.h"
#include "top.local.h"
#include "boolean.h"
#include "machine.h"		/* we should eliminate this!!! */
#include "utils.h"

#ifdef DEBUG
FILE *debug;
#endif

/* imported from screen.c */
extern int overstrike;

static int lmpid = 0;
static int last_hi = 0;		/* used in u_process and u_endscreen */
static int lastline = 0;
static int display_width = MAX_COLS;

#define lineindex(l) ((l)*display_width)

char *printable();

/* things initialized by display_init and used thruout */

/* buffer of proc information lines for display updating */
char *screenbuf = NULL;

static char **procstate_names;
static char **cpustate_names;
static char **memory_names;

static int num_procstates;
static int num_cpustates;
static int num_memory;

static int *lprocstates;
static int *lcpustates;
static int *lmemory;

static int *cpustate_columns;
static int cpustate_total_length;

static enum { OFF, ON, ERASE } header_status = ON;

static int string_count();
static void summary_format();
static void line_update();

int display_resize()

{
    register int lines;

    /* first, deallocate any previous buffer that may have been there */
    if (screenbuf != NULL)
    {
	free(screenbuf);
    }

    /* calculate the current dimensions */
    /* if operating in "dumb" mode, we only need one line */
    lines = smart_terminal ? screen_length - Header_lines : 1;

    /* we don't want more than MAX_COLS columns, since the machine-dependent
       modules make static allocations based on MAX_COLS and we don't want
       to run off the end of their buffers */
    display_width = screen_width;
    if (display_width >= MAX_COLS)
    {
	display_width = MAX_COLS - 1;
    }

    /* now, allocate space for the screen buffer */
    screenbuf = (char *)malloc(lines * display_width);
    if (screenbuf == (char *)NULL)
    {
	/* oops! */
	return(-1);
    }

    /* return number of lines available */
    /* for dumb terminals, pretend like we can show any amount */
    return(smart_terminal ? lines : Largest);
}

int display_init(statics)

struct statics *statics;

{
    register int lines;
    register char **pp;
    register int *ip;
    register int i;

    /* call resize to do the dirty work */
    lines = display_resize();

    /* only do the rest if we need to */
    if (lines > -1)
    {
	/* save pointers and allocate space for names */
	procstate_names = statics->procstate_names;
	num_procstates = string_count(procstate_names);
	lprocstates = (int *)malloc(num_procstates * sizeof(int));

	cpustate_names = statics->cpustate_names;
	num_cpustates = string_count(cpustate_names);
	lcpustates = (int *)malloc(num_cpustates * sizeof(int));
	cpustate_columns = (int *)malloc(num_cpustates * sizeof(int));

	memory_names = statics->memory_names;
	num_memory = string_count(memory_names);
	lmemory = (int *)malloc(num_memory * sizeof(int));

	/* calculate starting columns where needed */
	cpustate_total_length = 0;
	pp = cpustate_names;
	ip = cpustate_columns;
	while (*pp != NULL)
	{
	    *ip++ = cpustate_total_length;
	    if ((i = strlen(*pp++)) > 0)
	    {
		cpustate_total_length += i + 8;
	    }
	}
    }

    /* return number of lines available */
    return(lines);
}

i_loadave(mpid, avenrun)

int mpid;
double *avenrun;

{
    register int i;

    /* i_loadave also clears the screen, since it is first */
    clear();

    /* mpid == -1 implies this system doesn't have an _mpid */
    if (mpid != -1)
    {
	printf("last pid: %5d;  ", mpid);
    }

    printf("load averages");

    for (i = 0; i < 3; i++)
    {
	printf("%c %5.2f",
	    i == 0 ? ':' : ',',
	    avenrun[i]);
    }
    lmpid = mpid;
}

u_loadave(mpid, avenrun)

int mpid;
double *avenrun;

{
    register int i;

    if (mpid != -1)
    {
	/* change screen only when value has really changed */
	if (mpid != lmpid)
	{
	    Move_to(x_lastpid, y_lastpid);
	    printf("%5d", mpid);
	    lmpid = mpid;
	}

	/* i remembers x coordinate to move to */
	i = x_loadave;
    }
    else
    {
	i = x_loadave_nompid;
    }

    /* move into position for load averages */
    Move_to(i, y_loadave);

    /* display new load averages */
    /* we should optimize this and only display changes */
    for (i = 0; i < 3; i++)
    {
	printf("%s%5.2f",
	    i == 0 ? "" : ", ",
	    avenrun[i]);
    }
}

i_timeofday(tod)

time_t *tod;

{
    /*
     *  Display the current time.
     *  "ctime" always returns a string that looks like this:
     *  
     *	Sun Sep 16 01:03:52 1973
     *      012345678901234567890123
     *	          1         2
     *
     *  We want indices 11 thru 18 (length 8).
     */

    if (smart_terminal)
    {
	Move_to(screen_width - 8, 0);
    }
    else
    {
	fputs("    ", stdout);
    }
#ifdef DEBUG
    {
	char *foo;
	foo = ctime(tod);
	fputs(foo, stdout);
    }
#endif
    printf("%-8.8s\n", &(ctime(tod)[11]));
    lastline = 1;
}

static int ltotal = 0;
static char procstates_buffer[MAX_COLS];

/*
 *  *_procstates(total, brkdn, names) - print the process summary line
 *
 *  Assumptions:  cursor is at the beginning of the line on entry
 *		  lastline is valid
 */

i_procstates(total, brkdn)

int total;
int *brkdn;

{
    register int i;

    /* write current number of processes and remember the value */
    printf("%d processes:", total);
    ltotal = total;

    /* put out enough spaces to get to column 15 */
    i = digits(total);
    while (i++ < 4)
    {
	putchar(' ');
    }

    /* format and print the process state summary */
    summary_format(procstates_buffer, brkdn, procstate_names);
    fputs(procstates_buffer, stdout);

    /* save the numbers for next time */
    memcpy(lprocstates, brkdn, num_procstates * sizeof(int));
}

u_procstates(total, brkdn)

int total;
int *brkdn;

{
    static char new[MAX_COLS];
    register int i;

    /* update number of processes only if it has changed */
    if (ltotal != total)
    {
	/* move and overwrite */
#if (x_procstate == 0)
	Move_to(x_procstate, y_procstate);
#else
	/* cursor is already there...no motion needed */
	/* assert(lastline == 1); */
#endif
	printf("%d", total);

	/* if number of digits differs, rewrite the label */
	if (digits(total) != digits(ltotal))
	{
	    fputs(" processes:", stdout);
	    /* put out enough spaces to get to column 15 */
	    i = digits(total);
	    while (i++ < 4)
	    {
		putchar(' ');
	    }
	    /* cursor may end up right where we want it!!! */
	}

	/* save new total */
	ltotal = total;
    }

    /* see if any of the state numbers has changed */
    if (memcmp(lprocstates, brkdn, num_procstates * sizeof(int)) != 0)
    {
	/* format and update the line */
	summary_format(new, brkdn, procstate_names);
	line_update(procstates_buffer, new, x_brkdn, y_brkdn);
	memcpy(lprocstates, brkdn, num_procstates * sizeof(int));
    }
}

/*
 *  *_cpustates(states, names) - print the cpu state percentages
 *
 *  Assumptions:  cursor is on the PREVIOUS line
 */

static int cpustates_column;

/* cpustates_tag() calculates the correct tag to use to label the line */

char *cpustates_tag()

{
    register char *use;

    static char *short_tag = "CPU: ";
    static char *long_tag = "CPU states: ";

    /* if length + strlen(long_tag) >= screen_width, then we have to
       use the shorter tag (we subtract 2 to account for ": ") */
    if (cpustate_total_length + (int)strlen(long_tag) - 2 >= screen_width)
    {
	use = short_tag;
    }
    else
    {
	use = long_tag;
    }

    /* set cpustates_column accordingly then return result */
    cpustates_column = strlen(use);
    return(use);
}

i_cpustates(states)

register int *states;

{
    register int i = 0;
    register int value;
    register char **names = cpustate_names;
    register char *thisname;

    /* print tag and bump lastline */
    printf("\n%s", cpustates_tag());
    lastline++;

    /* now walk thru the names and print the line */
    while ((thisname = *names++) != NULL)
    {
	if (*thisname != '\0')
	{
	    /* retrieve the value and remember it */
	    value = *states++;

	    /* if percentage is >= 1000, print it as 100% */
	    printf((value >= 1000 ? "%s%4.0f%% %s" : "%s%4.1f%% %s"),
		   i++ == 0 ? "" : ", ",
		   ((float)value)/10.,
		   thisname);
	}
    }

    /* copy over values into "last" array */
    memcpy(lcpustates, states, num_cpustates * sizeof(int));
}

u_cpustates(states)

register int *states;

{
    register int value;
    register char **names = cpustate_names;
    register char *thisname;
    register int *lp;
    register int *colp;

    Move_to(cpustates_column, y_cpustates);
    lastline = y_cpustates;
    lp = lcpustates;
    colp = cpustate_columns;

    /* we could be much more optimal about this */
    while ((thisname = *names++) != NULL)
    {
	if (*thisname != '\0')
	{
	    /* did the value change since last time? */
	    if (*lp != *states)
	    {
		/* yes, move and change */
		Move_to(cpustates_column + *colp, y_cpustates);
		lastline = y_cpustates;

		/* retrieve value and remember it */
		value = *states;

		/* if percentage is >= 1000, print it as 100% */
		printf((value >= 1000 ? "%4.0f" : "%4.1f"),
		       ((double)value)/10.);

		/* remember it for next time */
		*lp = value;
	    }
	}

	/* increment and move on */
	lp++;
	states++;
	colp++;
    }
}

z_cpustates()

{
    register int i = 0;
    register char **names = cpustate_names;
    register char *thisname;
    register int *lp;

    /* show tag and bump lastline */
    printf("\n%s", cpustates_tag());
    lastline++;

    while ((thisname = *names++) != NULL)
    {
	if (*thisname != '\0')
	{
	    printf("%s    %% %s", i++ == 0 ? "" : ", ", thisname);
	}
    }

    /* fill the "last" array with all -1s, to insure correct updating */
    lp = lcpustates;
    i = num_cpustates;
    while (--i >= 0)
    {
	*lp++ = -1;
    }
}

/*
 *  *_memory(stats) - print "Memory: " followed by the memory summary string
 *
 *  Assumptions:  cursor is on "lastline"
 *                for i_memory ONLY: cursor is on the previous line
 */

char memory_buffer[MAX_COLS];

i_memory(stats)

int *stats;

{
    fputs("\nMemory: ", stdout);
    lastline++;

    /* format and print the memory summary */
    summary_format(memory_buffer, stats, memory_names);
    fputs(memory_buffer, stdout);
}

u_memory(stats)

int *stats;

{
    static char new[MAX_COLS];

    /* format the new line */
    summary_format(new, stats, memory_names);
    line_update(memory_buffer, new, x_mem, y_mem);
}

/*
 *  *_message() - print the next pending message line, or erase the one
 *                that is there.
 *
 *  Note that u_message is (currently) the same as i_message.
 *
 *  Assumptions:  lastline is consistent
 */

/*
 *  i_message is funny because it gets its message asynchronously (with
 *	respect to screen updates).
 */

static char next_msg[MAX_COLS + 5];
static int msglen = 0;
/* Invariant: msglen is always the length of the message currently displayed
   on the screen (even when next_msg doesn't contain that message). */

i_message()

{
    while (lastline < y_message)
    {
	fputc('\n', stdout);
	lastline++;
    }
    if (next_msg[0] != '\0')
    {
	standout(next_msg);
	msglen = strlen(next_msg);
	next_msg[0] = '\0';
    }
    else if (msglen > 0)
    {
	(void) clear_eol(msglen);
	msglen = 0;
    }
}

u_message()

{
    i_message();
}

static int header_length;

/*
 *  *_header(text) - print the header for the process area
 *
 *  Assumptions:  cursor is on the previous line and lastline is consistent
 */

i_header(text)

char *text;

{
    header_length = strlen(text);
    if (header_status == ON)
    {
	putchar('\n');
	fputs(text, stdout);
	lastline++;
    }
    else if (header_status == ERASE)
    {
	header_status = OFF;
    }
}

/*ARGSUSED*/
u_header(text)

char *text;		/* ignored */

{
    if (header_status == ERASE)
    {
	putchar('\n');
	lastline++;
	clear_eol(header_length);
	header_status = OFF;
    }
}

/*
 *  *_process(line, thisline) - print one process line
 *
 *  Assumptions:  lastline is consistent
 */

i_process(line, thisline)

int line;
char *thisline;

{
    register char *p;
    register char *base;

    /* make sure we are on the correct line */
    while (lastline < y_procs + line)
    {
	putchar('\n');
	lastline++;
    }

    /* truncate the line to conform to our current screen width */
    thisline[display_width] = '\0';

    /* write the line out */
    fputs(thisline, stdout);

    /* copy it in to our buffer */
    base = smart_terminal ? screenbuf + lineindex(line) : screenbuf;
    p = strecpy(base, thisline);

    /* zero fill the rest of it */
    memzero(p, display_width - (p - base));
}

u_process(line, newline)

int line;
char *newline;

{
    register char *optr;
    register int screen_line = line + Header_lines;
    register char *bufferline;

    /* remember a pointer to the current line in the screen buffer */
    bufferline = &screenbuf[lineindex(line)];

    /* truncate the line to conform to our current screen width */
    newline[display_width] = '\0';

    /* is line higher than we went on the last display? */
    if (line >= last_hi)
    {
	/* yes, just ignore screenbuf and write it out directly */
	/* get positioned on the correct line */
	if (screen_line - lastline == 1)
	{
	    putchar('\n');
	    lastline++;
	}
	else
	{
	    Move_to(0, screen_line);
	    lastline = screen_line;
	}

	/* now write the line */
	fputs(newline, stdout);

	/* copy it in to the buffer */
	optr = strecpy(bufferline, newline);

	/* zero fill the rest of it */
	memzero(optr, display_width - (optr - bufferline));
    }
    else
    {
	line_update(bufferline, newline, 0, line + Header_lines);
    }
}

u_endscreen(hi)

register int hi;

{
    register int screen_line = hi + Header_lines;
    register int i;

    if (smart_terminal)
    {
	if (hi < last_hi)
	{
	    /* need to blank the remainder of the screen */
	    /* but only if there is any screen left below this line */
	    if (lastline + 1 < screen_length)
	    {
		/* efficiently move to the end of currently displayed info */
		if (screen_line - lastline < 5)
		{
		    while (lastline < screen_line)
		    {
			putchar('\n');
			lastline++;
		    }
		}
		else
		{
		    Move_to(0, screen_line);
		    lastline = screen_line;
		}

		if (clear_to_end)
		{
		    /* we can do this the easy way */
		    putcap(clear_to_end);
		}
		else
		{
		    /* use clear_eol on each line */
		    i = hi;
		    while ((void) clear_eol(strlen(&screenbuf[lineindex(i++)])), i < last_hi)
		    {
			putchar('\n');
		    }
		}
	    }
	}
	last_hi = hi;

	/* move the cursor to a pleasant place */
	Move_to(x_idlecursor, y_idlecursor);
	lastline = y_idlecursor;
    }
    else
    {
	/* separate this display from the next with some vertical room */
	fputs("\n\n", stdout);
    }
}

display_header(t)

int t;

{
    if (t)
    {
	header_status = ON;
    }
    else if (header_status == ON)
    {
	header_status = ERASE;
    }
}

/*VARARGS2*/
new_message(type, msgfmt, a1, a2, a3)

int type;
char *msgfmt;
caddr_t a1, a2, a3;

{
    register int i;

    /* first, format the message */
    (void) sprintf(next_msg, msgfmt, a1, a2, a3);

    if (msglen > 0)
    {
	/* message there already -- can we clear it? */
	if (!overstrike)
	{
	    /* yes -- write it and clear to end */
	    i = strlen(next_msg);
	    if ((type & MT_delayed) == 0)
	    {
		type & MT_standout ? standout(next_msg) :
		                     fputs(next_msg, stdout);
		(void) clear_eol(msglen - i);
		msglen = i;
		next_msg[0] = '\0';
	    }
	}
    }
    else
    {
	if ((type & MT_delayed) == 0)
	{
	    type & MT_standout ? standout(next_msg) : fputs(next_msg, stdout);
	    msglen = strlen(next_msg);
	    next_msg[0] = '\0';
	}
    }
}

clear_message()

{
    if (clear_eol(msglen) == 1)
    {
	putchar('\r');
    }
}

readline(buffer, size, numeric)

char *buffer;
int  size;
int  numeric;

{
    register char *ptr = buffer;
    register char ch;
    register char cnt = 0;
    register char maxcnt = 0;

    /* allow room for null terminator */
    size -= 1;

    /* read loop */
    while ((fflush(stdout), read(0, ptr, 1) > 0))
    {
	/* newline means we are done */
	if ((ch = *ptr) == '\n')
	{
	    break;
	}

	/* handle special editing characters */
	if (ch == ch_kill)
	{
	    /* kill line -- account for overstriking */
	    if (overstrike)
	    {
		msglen += maxcnt;
	    }

	    /* return null string */
	    *buffer = '\0';
	    putchar('\r');
	    return(-1);
	}
	else if (ch == ch_erase)
	{
	    /* erase previous character */
	    if (cnt <= 0)
	    {
		/* none to erase! */
		putchar('\7');
	    }
	    else
	    {
		fputs("\b \b", stdout);
		ptr--;
		cnt--;
	    }
	}
	/* check for character validity and buffer overflow */
	else if (cnt == size || (numeric && !isdigit(ch)) ||
		!isprint(ch))
	{
	    /* not legal */
	    putchar('\7');
	}
	else
	{
	    /* echo it and store it in the buffer */
	    putchar(ch);
	    ptr++;
	    cnt++;
	    if (cnt > maxcnt)
	    {
		maxcnt = cnt;
	    }
	}
    }

    /* all done -- null terminate the string */
    *ptr = '\0';

    /* account for the extra characters in the message area */
    /* (if terminal overstrikes, remember the furthest they went) */
    msglen += overstrike ? maxcnt : cnt;

    /* return either inputted number or string length */
    putchar('\r');
    return(cnt == 0 ? -1 : numeric ? atoi(buffer) : cnt);
}

/* internal support routines */

static int string_count(pp)

register char **pp;

{
    register int cnt;

    cnt = 0;
    while (*pp++ != NULL)
    {
	cnt++;
    }
    return(cnt);
}

static void summary_format(str, numbers, names)

char *str;
int *numbers;
register char **names;

{
    register char *p;
    register int num;
    register char *thisname;
    register int useM = No;

    /* format each number followed by its string */
    p = str;
    while ((thisname = *names++) != NULL)
    {
	/* get the number to format */
	num = *numbers++;

	/* display only non-zero numbers */
	if (num > 0)
	{
	    /* is this number in kilobytes? */
	    if (thisname[0] == 'K')
	    {
		/* yes: format it as a memory value */
		p = strecpy(p, format_k(num));

		/* skip over the K, since it was included by format_k */
		p = strecpy(p, thisname+1);
	    }
	    else
	    {
		p = strecpy(p, itoa(num));
		p = strecpy(p, thisname);
	    }
	}

	/* ignore negative numbers, but display corresponding string */
	else if (num < 0)
	{
	    p = strecpy(p, thisname);
	}
    }

    /* if the last two characters in the string are ", ", delete them */
    p -= 2;
    if (p >= str && p[0] == ',' && p[1] == ' ')
    {
	*p = '\0';
    }
}

static void line_update(old, new, start, line)

register char *old;
register char *new;
int start;
int line;

{
    register int ch;
    register int diff;
    register int newcol = start + 1;
    register int lastcol = start;
    char cursor_on_line = No;
    char *current;

    /* compare the two strings and only rewrite what has changed */
    current = old;
#ifdef DEBUG
    fprintf(debug, "line_update, starting at %d\n", start);
    fputs(old, debug);
    fputc('\n', debug);
    fputs(new, debug);
    fputs("\n-\n", debug);
#endif

    /* start things off on the right foot		    */
    /* this is to make sure the invariants get set up right */
    if ((ch = *new++) != *old)
    {
	if (line - lastline == 1 && start == 0)
	{
	    putchar('\n');
	}
	else
	{
	    Move_to(start, line);
	}
	cursor_on_line = Yes;
	putchar(ch);
	*old = ch;
	lastcol = 1;
    }
    old++;
	
    /*
     *  main loop -- check each character.  If the old and new aren't the
     *	same, then update the display.  When the distance from the
     *	current cursor position to the new change is small enough,
     *	the characters that belong there are written to move the
     *	cursor over.
     *
     *	Invariants:
     *	    lastcol is the column where the cursor currently is sitting
     *		(always one beyond the end of the last mismatch).
     */
    do		/* yes, a do...while */
    {
	if ((ch = *new++) != *old)
	{
	    /* new character is different from old	  */
	    /* make sure the cursor is on top of this character */
	    diff = newcol - lastcol;
	    if (diff > 0)
	    {
		/* some motion is required--figure out which is shorter */
		if (diff < 6 && cursor_on_line)
		{
		    /* overwrite old stuff--get it out of the old buffer */
		    printf("%.*s", diff, &current[lastcol-start]);
		}
		else
		{
		    /* use cursor addressing */
		    Move_to(newcol, line);
		    cursor_on_line = Yes;
		}
		/* remember where the cursor is */
		lastcol = newcol + 1;
	    }
	    else
	    {
		/* already there, update position */
		lastcol++;
	    }
		
	    /* write what we need to */
	    if (ch == '\0')
	    {
		/* at the end--terminate with a clear-to-end-of-line */
		(void) clear_eol(strlen(old));
	    }
	    else
	    {
		/* write the new character */
		putchar(ch);
	    }
	    /* put the new character in the screen buffer */
	    *old = ch;
	}
	    
	/* update working column and screen buffer pointer */
	newcol++;
	old++;
	    
    } while (ch != '\0');

    /* zero out the rest of the line buffer -- MUST BE DONE! */
    diff = display_width - newcol;
    if (diff > 0)
    {
	memzero(old, diff);
    }

    /* remember where the current line is */
    if (cursor_on_line)
    {
	lastline = line;
    }
}

/*
 *  printable(str) - make the string pointed to by "str" into one that is
 *	printable (i.e.: all ascii), by converting all non-printable
 *	characters into '?'.  Replacements are done in place and a pointer
 *	to the original buffer is returned.
 */

char *printable(str)

char *str;

{
    register char *ptr;
    register char ch;

    ptr = str;
    while ((ch = *ptr) != '\0')
    {
	if (!isprint(ch))
	{
	    *ptr = '?';
	}
	ptr++;
    }
    return(str);
}
