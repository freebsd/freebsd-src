/*
 * Copyright (C) 1984-2002  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Process command line options.
 *
 * Each option is a single letter which controls a program variable.
 * The options have defaults which may be changed via
 * the command line option, toggled via the "-" command, 
 * or queried via the "_" command.
 */

#include "less.h"
#include "option.h"

static struct loption *pendopt;
public int plusoption = FALSE;

static char *propt();
static char *optstring();
static int flip_triple();

extern int screen_trashed;
extern char *every_first_cmd;

/* 
 * Scan an argument (either from the command line or from the 
 * LESS environment variable) and process it.
 */
	public void
scan_option(s)
	char *s;
{
	register struct loption *o;
	register int optc;
	char *optname;
	char *printopt;
	char *str;
	int set_default;
	int lc;
	int err;
	PARG parg;

	if (s == NULL)
		return;

	/*
	 * If we have a pending option which requires an argument,
	 * handle it now.
	 * This happens if the previous option was, for example, "-P"
	 * without a following string.  In that case, the current
	 * option is simply the argument for the previous option.
	 */
	if (pendopt != NULL)
	{
		switch (pendopt->otype & OTYPE)
		{
		case STRING:
			(*pendopt->ofunc)(INIT, s);
			break;
		case NUMBER:
			printopt = propt(pendopt->oletter);
			*(pendopt->ovar) = getnum(&s, printopt, (int*)NULL);
			break;
		}
		pendopt = NULL;
		return;
	}

	set_default = FALSE;
	optname = NULL;

	while (*s != '\0')
	{
		/*
		 * Check some special cases first.
		 */
		switch (optc = *s++)
		{
		case ' ':
		case '\t':
		case END_OPTION_STRING:
			continue;
		case '-':
			/*
			 * "--" indicates an option name instead of a letter.
			 */
			if (*s == '-')
			{
				optname = ++s;
				break;
			}
			/*
			 * "-+" means set these options back to their defaults.
			 * (They may have been set otherwise by previous 
			 * options.)
			 */
			set_default = (*s == '+');
			if (set_default)
				s++;
			continue;
		case '+':
			/*
			 * An option prefixed by a "+" is ungotten, so 
			 * that it is interpreted as less commands 
			 * processed at the start of the first input file.
			 * "++" means process the commands at the start of
			 * EVERY input file.
			 */
			plusoption = TRUE;
			s = optstring(s, &str, propt('+'), NULL);
			if (*str == '+')
				every_first_cmd = save(++str);
			else
				ungetsc(str);
			continue;
		case '0':  case '1':  case '2':  case '3':  case '4':
		case '5':  case '6':  case '7':  case '8':  case '9':
			/*
			 * Special "more" compatibility form "-<number>"
			 * instead of -z<number> to set the scrolling 
			 * window size.
			 */
			s--;
			optc = 'z';
			break;
		}

		/*
		 * Not a special case.
		 * Look up the option letter in the option table.
		 */
		err = 0;
		if (optname == NULL)
		{
			printopt = propt(optc);
			lc = SIMPLE_IS_LOWER(optc);
			o = findopt(optc);
		} else
		{
			printopt = optname;
			lc = SIMPLE_IS_LOWER(optname[0]);
			o = findopt_name(&optname, NULL, &err);
			s = optname;
			optname = NULL;
			if (*s == '\0' || *s == ' ')
			{
				/*
				 * The option name matches exactly.
				 */
				;
			} else if (*s == '=')
			{
				/*
				 * The option name is followed by "=value".
				 */
				if (o != NULL &&
				    (o->otype & OTYPE) != STRING &&
				    (o->otype & OTYPE) != NUMBER)
				{
					parg.p_string = printopt;
					error("The %s option should not be followed by =",
						&parg);
					quit(QUIT_ERROR);
				}
				s++;
			} else
			{
				/*
				 * The specified name is longer than the
				 * real option name.
				 */
				o = NULL;
			}
		}
		if (o == NULL)
		{
			parg.p_string = printopt;
			if (err == OPT_AMBIG)
				error("%s is an ambiguous abbreviation (\"less --help\" for help)",
					&parg);
			else
				error("There is no %s option (\"less --help\" for help)",
					&parg);
			quit(QUIT_ERROR);
		}

		str = NULL;
		switch (o->otype & OTYPE)
		{
		case BOOL:
			if (set_default)
				*(o->ovar) = o->odefault;
			else
				*(o->ovar) = ! o->odefault;
			break;
		case TRIPLE:
			if (set_default)
				*(o->ovar) = o->odefault;
			else
				*(o->ovar) = flip_triple(o->odefault, lc);
			break;
		case STRING:
			if (*s == '\0')
			{
				/*
				 * Set pendopt and return.
				 * We will get the string next time
				 * scan_option is called.
				 */
				pendopt = o;
				return;
			}
			/*
			 * Don't do anything here.
			 * All processing of STRING options is done by 
			 * the handling function.
			 */
			while (*s == ' ')
				s++;
			s = optstring(s, &str, printopt, o->odesc[1]);
			break;
		case NUMBER:
			if (*s == '\0')
			{
				pendopt = o;
				return;
			}
			*(o->ovar) = getnum(&s, printopt, (int*)NULL);
			break;
		}
		/*
		 * If the option has a handling function, call it.
		 */
		if (o->ofunc != NULL)
			(*o->ofunc)(INIT, str);
	}
}

/*
 * Toggle command line flags from within the program.
 * Used by the "-" and "_" commands.
 * how_toggle may be:
 *	OPT_NO_TOGGLE	just report the current setting, without changing it.
 *	OPT_TOGGLE	invert the current setting
 *	OPT_UNSET	set to the default value
 *	OPT_SET		set to the inverse of the default value
 */
	public void
toggle_option(c, s, how_toggle)
	int c;
	char *s;
	int how_toggle;
{
	register struct loption *o;
	register int num;
	int no_prompt;
	int err;
	PARG parg;

	no_prompt = (how_toggle & OPT_NO_PROMPT);
	how_toggle &= ~OPT_NO_PROMPT;

	/*
	 * Look up the option letter in the option table.
	 */
	o = findopt(c);
	if (o == NULL)
	{
		parg.p_string = propt(c);
		error("There is no %s option", &parg);
		return;
	}

	if (how_toggle == OPT_TOGGLE && (o->otype & NO_TOGGLE))
	{
		parg.p_string = propt(c);
		error("Cannot change the %s option", &parg);
		return;
	} 

	if (how_toggle == OPT_NO_TOGGLE && (o->otype & NO_QUERY))
	{
		parg.p_string = propt(c);
		error("Cannot query the %s option", &parg);
		return;
	} 

	/*
	 * Check for something which appears to be a do_toggle
	 * (because the "-" command was used), but really is not.
	 * This could be a string option with no string, or
	 * a number option with no number.
	 */
	switch (o->otype & OTYPE)
	{
	case STRING:
	case NUMBER:
		if (how_toggle == OPT_TOGGLE && *s == '\0')
			how_toggle = OPT_NO_TOGGLE;
		break;
	}

#if HILITE_SEARCH
	if (how_toggle != OPT_NO_TOGGLE && (o->otype & HL_REPAINT))
		repaint_hilite(0);
#endif

	/*
	 * Now actually toggle (change) the variable.
	 */
	if (how_toggle != OPT_NO_TOGGLE)
	{
		switch (o->otype & OTYPE)
		{
		case BOOL:
			/*
			 * Boolean.
			 */
			switch (how_toggle)
			{
			case OPT_TOGGLE:
				*(o->ovar) = ! *(o->ovar);
				break;
			case OPT_UNSET:
				*(o->ovar) = o->odefault;
				break;
			case OPT_SET:
				*(o->ovar) = ! o->odefault;
				break;
			}
			break;
		case TRIPLE:
			/*
			 * Triple:
			 *	If user gave the lower case letter, then switch 
			 *	to 1 unless already 1, in which case make it 0.
			 *	If user gave the upper case letter, then switch
			 *	to 2 unless already 2, in which case make it 0.
			 */
			switch (how_toggle)
			{
			case OPT_TOGGLE:
				*(o->ovar) = flip_triple(*(o->ovar), 
						islower(c));
				break;
			case OPT_UNSET:
				*(o->ovar) = o->odefault;
				break;
			case OPT_SET:
				*(o->ovar) = flip_triple(o->odefault,
						islower(c));
				break;
			}
			break;
		case STRING:
			/*
			 * String: don't do anything here.
			 *	The handling function will do everything.
			 */
			switch (how_toggle)
			{
			case OPT_SET:
			case OPT_UNSET:
				error("Cannot use \"-+\" or \"--\" for a string option",
					NULL_PARG);
				return;
			}
			break;
		case NUMBER:
			/*
			 * Number: set the variable to the given number.
			 */
			switch (how_toggle)
			{
			case OPT_TOGGLE:
				num = getnum(&s, NULL, &err);
				if (!err)
					*(o->ovar) = num;
				break;
			case OPT_UNSET:
				*(o->ovar) = o->odefault;
				break;
			case OPT_SET:
				error("Can't use \"-!\" for a numeric option",
					NULL_PARG);
				return;
			}
			break;
		}
	}

	/*
	 * Call the handling function for any special action 
	 * specific to this option.
	 */
	if (o->ofunc != NULL)
		(*o->ofunc)((how_toggle==OPT_NO_TOGGLE) ? QUERY : TOGGLE, s);

#if HILITE_SEARCH
	if (how_toggle != OPT_NO_TOGGLE && (o->otype & HL_REPAINT))
		chg_hilite();
#endif

	if (!no_prompt)
	{
		/*
		 * Print a message describing the new setting.
		 */
		switch (o->otype & OTYPE)
		{
		case BOOL:
		case TRIPLE:
			/*
			 * Print the odesc message.
			 */
			error(o->odesc[*(o->ovar)], NULL_PARG);
			break;
		case NUMBER:
			/*
			 * The message is in odesc[1] and has a %d for 
			 * the value of the variable.
			 */
			parg.p_int = *(o->ovar);
			error(o->odesc[1], &parg);
			break;
		case STRING:
			/*
			 * Message was already printed by the handling function.
			 */
			break;
		}
	}

	if (how_toggle != OPT_NO_TOGGLE && (o->otype & REPAINT))
		screen_trashed = TRUE;
}

/*
 * "Toggle" a triple-valued option.
 */
	static int
flip_triple(val, lc)
	int val;
	int lc;
{
	if (lc)
		return ((val == OPT_ON) ? OPT_OFF : OPT_ON);
	else
		return ((val == OPT_ONPLUS) ? OPT_OFF : OPT_ONPLUS);
}

/*
 * Return a string suitable for printing as the "name" of an option.
 * For example, if the option letter is 'x', just return "-x".
 */
	static char *
propt(c)
	int c;
{
	static char buf[8];

	sprintf(buf, "-%s", prchar(c));
	return (buf);
}

/*
 * Determine if an option is a single character option (BOOL or TRIPLE),
 * or if it a multi-character option (NUMBER).
 */
	public int
single_char_option(c)
	int c;
{
	register struct loption *o;

	o = findopt(c);
	if (o == NULL)
		return (TRUE);
	return ((o->otype & (BOOL|TRIPLE|NOVAR|NO_TOGGLE)) != 0);
}

/*
 * Return the prompt to be used for a given option letter.
 * Only string and number valued options have prompts.
 */
	public char *
opt_prompt(c)
	int c;
{
	register struct loption *o;

	o = findopt(c);
	if (o == NULL || (o->otype & (STRING|NUMBER)) == 0)
		return (NULL);
	return (o->odesc[0]);
}

/*
 * Return whether or not there is a string option pending;
 * that is, if the previous option was a string-valued option letter 
 * (like -P) without a following string.
 * In that case, the current option is taken to be the string for
 * the previous option.
 */
	public int
isoptpending()
{
	return (pendopt != NULL);
}

/*
 * Print error message about missing string.
 */
	static void
nostring(printopt)
	char *printopt;
{
	PARG parg;
	parg.p_string = printopt;
	error("Value is required after %s", &parg);
}

/*
 * Print error message if a STRING type option is not followed by a string.
 */
	public void
nopendopt()
{
	nostring(propt(pendopt->oletter));
}

/*
 * Scan to end of string or to an END_OPTION_STRING character.
 * In the latter case, replace the char with a null char.
 * Return a pointer to the remainder of the string, if any.
 */
	static char *
optstring(s, p_str, printopt, validchars)
	char *s;
	char **p_str;
	char *printopt;
	char *validchars;
{
	register char *p;

	if (*s == '\0')
	{
		nostring(printopt);
		quit(QUIT_ERROR);
	}
	*p_str = s;
	for (p = s;  *p != '\0';  p++)
	{
		if (*p == END_OPTION_STRING ||
		    (validchars != NULL && strchr(validchars, *p) == NULL))
		{
			switch (*p)
			{
			case END_OPTION_STRING:
			case ' ':  case '\t':  case '-':
				/* Replace the char with a null to terminate string. */
				*p++ = '\0';
				break;
			default:
				/* Cannot replace char; make a copy of the string. */
				*p_str = (char *) ecalloc(p-s+1, sizeof(char));
				strncpy(*p_str, s, p-s);
				(*p_str)[p-s] = '\0';
				break;
			}
			break;
		}
	}
	return (p);
}

/*
 * Translate a string into a number.
 * Like atoi(), but takes a pointer to a char *, and updates
 * the char * to point after the translated number.
 */
	public int
getnum(sp, printopt, errp)
	char **sp;
	char *printopt;
	int *errp;
{
	register char *s;
	register int n;
	register int neg;
	PARG parg;

	s = skipsp(*sp);
	neg = FALSE;
	if (*s == '-')
	{
		neg = TRUE;
		s++;
	}
	if (*s < '0' || *s > '9')
	{
		if (errp != NULL)
		{
			*errp = TRUE;
			return (-1);
		}
		if (printopt != NULL)
		{
			parg.p_string = printopt;
			error("Number is required after %s", &parg);
		}
		quit(QUIT_ERROR);
	}

	n = 0;
	while (*s >= '0' && *s <= '9')
		n = 10 * n + *s++ - '0';
	*sp = s;
	if (errp != NULL)
		*errp = FALSE;
	if (neg)
		n = -n;
	return (n);
}
