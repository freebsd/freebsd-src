#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Header: /a/cvs/386BSD/src/libexec/crond/entry.c,v 1.1.1.1 1993/06/12 14:55:03 rgrimes Exp $";
#endif

/* vix 26jan87 [RCS'd; rest of log is in RCS file]
 * vix 01jan87 [added line-level error recovery]
 * vix 31dec86 [added /step to the from-to range, per bob@acornrc]
 * vix 30dec86 [written]
 */


/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 */


#include "cron.h"

typedef	enum
	{e_none, e_minute, e_hour, e_dom, e_month, e_dow, e_cmd, e_timespec}
	ecode_e;
static char *ecodes[] =
	{
		"no error",
		"bad minute",
		"bad hour",
		"bad day-of-month",
		"bad month",
		"bad day-of-week",
		"bad command",
		"bad time specifier"
	};

void
free_entry(e)
	entry	*e;
{
	int	free();

	(void) free(e->cmd);
	(void) free(e);
}


entry *
load_entry(file, error_func)
	FILE	*file;
	void	(*error_func)();
{
	/* this function reads one crontab entry -- the next -- from a file.
	 * it skips any leading blank lines, ignores comments, and returns
	 * EOF if for any reason the entry can't be read and parsed.
	 *
	 * the entry IS parsed here, btw.
	 *
	 * syntax:
	 *	minutes hours doms months dows cmd\n
	 */

	extern int	free();
	extern char	*malloc(), *savestr();
	extern void	unget_char();
	static char	get_list();

	ecode_e	ecode = e_none;
	entry	*e;
	int	ch;
	void	skip_comments();
	char	cmd[MAX_COMMAND];

	e = (entry *) calloc(sizeof(entry), sizeof(char));

	Debug(DPARS, ("load_entry()...about to eat comments\n"))

	skip_comments(file);

	ch = get_char(file);

	/* ch is now the first useful character of a useful line.
	 * it may be an @special or it may be the first character
	 * of a list of minutes.
	 */

	if (ch == '@')
	{
		/* all of these should be flagged and load-limited; i.e.,
		 * instead of @hourly meaning "0 * * * *" it should mean
		 * "close to the front of every hour but not 'til the
		 * system load is low".  Problems are: how do you know
		 * what "low" means? (save me from /etc/crond.conf!) and:
		 * how to guarantee low variance (how low is low?), which
		 * means how to we run roughly every hour -- seems like
		 * we need to keep a history or let the first hour set
		 * the schedule, which means we aren't load-limited
		 * anymore.  too much for my overloaded brain. (vix, jan90)
		 * HINT
		 */
		ch = get_string(cmd, MAX_COMMAND, file, " \t\n");
		if (!strcmp("reboot", cmd)) {
			e->flags |= WHEN_REBOOT;
		} else if (!strcmp("yearly", cmd) || !strcmp("annually", cmd)){
			bit_set(e->minute, 0);
			bit_set(e->hour, 0);
			bit_set(e->dom, 0);
			bit_set(e->month, 0);
			bit_nset(e->dow, 0, (LAST_DOW-FIRST_DOW+1));
		} else if (!strcmp("monthly", cmd)) {
			bit_set(e->minute, 0);
			bit_set(e->hour, 0);
			bit_set(e->dom, 0);
			bit_nset(e->month, 0, (LAST_MONTH-FIRST_MONTH+1));
			bit_nset(e->dow, 0, (LAST_DOW-FIRST_DOW+1));
		} else if (!strcmp("weekly", cmd)) {
			bit_set(e->minute, 0);
			bit_set(e->hour, 0);
			bit_nset(e->dom, 0, (LAST_DOM-FIRST_DOM+1));
			bit_nset(e->month, 0, (LAST_MONTH-FIRST_MONTH+1));
			bit_set(e->dow, 0);
		} else if (!strcmp("daily", cmd) || !strcmp("midnight", cmd)) {
			bit_set(e->minute, 0);
			bit_set(e->hour, 0);
			bit_nset(e->dom, 0, (LAST_DOM-FIRST_DOM+1));
			bit_nset(e->month, 0, (LAST_MONTH-FIRST_MONTH+1));
			bit_nset(e->dow, 0, (LAST_DOW-FIRST_DOW+1));
		} else if (!strcmp("hourly", cmd)) {
			bit_set(e->minute, 0);
			bit_set(e->hour, (LAST_HOUR-FIRST_HOUR+1));
			bit_nset(e->dom, 0, (LAST_DOM-FIRST_DOM+1));
			bit_nset(e->month, 0, (LAST_MONTH-FIRST_MONTH+1));
			bit_nset(e->dow, 0, (LAST_DOW-FIRST_DOW+1));
		} else {
			ecode = e_timespec;
			goto eof;
		}
	}
	else
	{
		Debug(DPARS, ("load_entry()...about to parse numerics\n"))

		ch = get_list(e->minute, FIRST_MINUTE, LAST_MINUTE,
				PPC_NULL, ch, file);
		if (ch == EOF)
		{
			ecode = e_minute;
			goto eof;
		}

		/* hours
		 */

		ch = get_list(e->hour, FIRST_HOUR, LAST_HOUR,
				PPC_NULL, ch, file);
		if (ch == EOF)
		{
			ecode = e_hour;
			goto eof;
		}

		/* DOM (days of month)
		 */

		if (ch == '*') e->flags |= DOM_STAR;
		ch = get_list(e->dom, FIRST_DOM, LAST_DOM, PPC_NULL, ch, file);
		if (ch == EOF)
		{
			ecode = e_dom;
			goto eof;
		}

		/* month
		 */

		ch = get_list(e->month, FIRST_MONTH, LAST_MONTH,
				MonthNames, ch, file);
		if (ch == EOF)
		{
			ecode = e_month;
			goto eof;
		}

		/* DOW (days of week)
		 */

		if (ch == '*') e->flags |= DOW_STAR;
		ch = get_list(e->dow, FIRST_DOW, LAST_DOW,
				DowNames, ch, file);
		if (ch == EOF)
		{
			ecode = e_dow;
			goto eof;
		}
	}

	/* make sundays equivilent */
	if (bit_test(e->dow, 0) || bit_test(e->dow, 7))
	{
		bit_set(e->dow, 0);
		bit_set(e->dow, 7);
	}

	Debug(DPARS, ("load_entry()...about to parse command\n"))

	/* ch is first character of a command.  everything up to the next
	 * \n or EOF is part of the command... too bad we don't know in
	 * advance how long it will be, since we need to malloc a string
	 * for it... so, we limit it to MAX_COMMAND
	 */ 
	unget_char(ch, file);
	ch = get_string(cmd, MAX_COMMAND, file, "\n");

	/* a file without a \n before the EOF is rude, so we'll complain...
	 */
	if (ch == EOF)
	{
		ecode = e_cmd;
		goto eof;
	}

	/* got the command in the 'cmd' string; save it in *e.
	 */
	e->cmd = savestr(cmd);

	Debug(DPARS, ("load_entry()...returning successfully\n"))

	/* success, fini, return pointer to the entry we just created...
	 */
	return e;

eof:	/* if we want to return EOF, we have to jump down here and
	 * free the entry we've been building.
	 *
	 * now, in some cases, a parse routine will have returned EOF to
	 * indicate an error, but the file is not actually done.  since, in
	 * that case, we only want to skip the line with the error on it,
	 * we'll do that here.
	 *
	 * many, including the author, see what's below as evil programming
	 * practice: since I didn't want to change the structure of this
	 * whole function to support this error recovery, I recurse.  Cursed!
	 * (At least it's tail-recursion, as if it matters in C - vix/8feb88)
	 * I'm seriously considering using (another) GOTO...   argh!
	 * (this does not get less disgusting over time.  vix/15nov88)
	 */

	(void) free(e);

	if (feof(file))
		return NULL;

	if (error_func)
		(*error_func)(ecodes[(int)ecode]);
	do  {ch = get_char(file);}
	while (ch != EOF && ch != '\n');
	if (ch == EOF)
		return NULL;
	return load_entry(file, error_func);
}


static char
get_list(bits, low, high, names, ch, file)
	bitstr_t	*bits;		/* one bit per flag, default=FALSE */
	int		low, high;	/* bounds, impl. offset for bitstr */
	char		*names[];	/* NULL or *[] of names for these elements */
	int		ch;		/* current character being processed */
	FILE		*file;		/* file being read */
{
	static char	get_range();
	register int	done;

	/* we know that we point to a non-blank character here;
	 * must do a Skip_Blanks before we exit, so that the
	 * next call (or the code that picks up the cmd) can
	 * assume the same thing.
	 */

	Debug(DPARS|DEXT, ("get_list()...entered\n"))

	/* list = "*" | range {"," range}
	 */
	
	if (ch == '*')
	{
		/* '*' means 'all elements'.
		 */
		bit_nset(bits, 0, (high-low+1));
		goto exit;
	}

	/* clear the bit string, since the default is 'off'.
	 */
	bit_nclear(bits, 0, (high-low+1));

	/* process all ranges
	 */
	done = FALSE;
	while (!done)
	{
		ch = get_range(bits, low, high, names, ch, file);
		if (ch == ',')
			ch = get_char(file);
		else
			done = TRUE;
	}

exit:	/* exiting.  skip to some blanks, then skip over the blanks.
	 */
	Skip_Nonblanks(ch, file)
	Skip_Blanks(ch, file)

	Debug(DPARS|DEXT, ("get_list()...exiting w/ %02x\n", ch))

	return ch;
}


static char
get_range(bits, low, high, names, ch, file)
	bitstr_t	*bits;		/* one bit per flag, default=FALSE */
	int		low, high;	/* bounds, impl. offset for bitstr */
	char		*names[];	/* NULL or names of elements */
	int		ch;		/* current character being processed */
	FILE		*file;		/* file being read */
{
	/* range = number | number "-" number [ "/" number ]
	 */

	static int	set_element();
	static char	get_number();
	register int	i;
	auto int	num1, num2, num3;

	Debug(DPARS|DEXT, ("get_range()...entering, exit won't show\n"))

	if (EOF == (ch = get_number(&num1, low, names, ch, file)))
		return EOF;

	if (ch != '-')
	{
		/* not a range, it's a single number.
		 */
		if (EOF == set_element(bits, low, high, num1))
			return EOF;
	}
	else
	{
		/* eat the dash
		 */
		ch = get_char(file);
		if (ch == EOF)
			return EOF;

		/* get the number following the dash
		 */
		ch = get_number(&num2, low, names, ch, file);
		if (ch == EOF)
			return EOF;

		/* check for step size
		 */
		if (ch == '/')
		{
			/* eat the slash
			 */
			ch = get_char(file);
			if (ch == EOF)
				return EOF;

			/* get the step size -- note: we don't pass the
			 * names here, because the number is not an
			 * element id, it's a step size.  'low' is
			 * sent as a 0 since there is no offset either.
			 */
			ch = get_number(&num3, 0, PPC_NULL, ch, file);
			if (ch == EOF)
				return EOF;
		}
		else
		{
			/* no step.  default==1.
			 */
			num3 = 1;
		}

		/* range. set all elements from num1 to num2, stepping
		 * by num3.  (the step is a downward-compatible extension
		 * proposed conceptually by bob@acornrc, syntactically
		 * designed then implmented by paul vixie).
		 */
		for (i = num1;  i <= num2;  i += num3)
			if (EOF == set_element(bits, low, high, i))
				return EOF;
	}
	return ch;
}


static char
get_number(numptr, low, names, ch, file)
	int	*numptr;
	int	low;
	char	*names[];
	char	ch;
	FILE	*file;
{
	char	temp[MAX_TEMPSTR], *pc;
	int	len, i, all_digits;

	/* collect alphanumerics into our fixed-size temp array
	 */
	pc = temp;
	len = 0;
	all_digits = TRUE;
	while (isalnum(ch))
	{
		if (++len >= MAX_TEMPSTR)
			return EOF;

		*pc++ = ch;

		if (!isdigit(ch))
			all_digits = FALSE;

		ch = get_char(file);
	}
	*pc = '\0';

	/* try to find the name in the name list
	 */
	if (names)
		for (i = 0;  names[i] != NULL;  i++)
		{
			Debug(DPARS|DEXT,
				("get_num, compare(%s,%s)\n", names[i], temp))
			if (!nocase_strcmp(names[i], temp))
			{
				*numptr = i+low;
				return ch;
			}
		}

	/* no name list specified, or there is one and our string isn't
	 * in it.  either way: if it's all digits, use its magnitude.
	 * otherwise, it's an error.
	 */
	if (all_digits)
	{
		*numptr = atoi(temp);
		return ch;
	}

	return EOF;
}


static int
set_element(bits, low, high, number)
	bitstr_t	*bits; 		/* one bit per flag, default=FALSE */
	int		low;
	int		high;
	int		number;
{
	Debug(DPARS|DEXT, ("set_element(?,%d,%d,%d)\n", low, high, number))

	if (number < low || number > high)
		return EOF;

	Debug(DPARS|DEXT, ("bit_set(%x,%d)\n",bits,(number-low)))
	bit_set(bits, (number-low));
	Debug(DPARS|DEXT, ("bit_set succeeded\n"))
	return OK;
}
