/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
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
public lbool plusoption = FALSE;

static constant char *optstring(constant char *s, char **p_str, constant char *printopt, constant char *validchars);
static int flip_triple(int val, lbool lc);

extern int less_is_more;
extern int quit_at_eof;
extern char *every_first_cmd;
extern int opt_use_backslash;
extern int ctldisp;

/*
 * Return a printable description of an option.
 */
static constant char * opt_desc(struct loption *o)
{
	static char buf[OPTNAME_MAX + 10];
	if (o->oletter == OLETTER_NONE)
		SNPRINTF1(buf, sizeof(buf), "--%s", o->onames->oname);
	else
		SNPRINTF2(buf, sizeof(buf), "-%c (--%s)", o->oletter, o->onames->oname);
	return (buf);
}

/*
 * Return a string suitable for printing as the "name" of an option.
 * For example, if the option letter is 'x', just return "-x".
 */
public constant char * propt(char c)
{
	static char buf[MAX_PRCHAR_LEN+2];

	sprintf(buf, "-%s", prchar((LWCHAR) c));
	return (buf);
}

/* 
 * Scan an argument (either from the command line or from the 
 * LESS environment variable) and process it.
 */
public void scan_option(constant char *s, lbool is_env)
{
	struct loption *o;
	char optc;
	constant char *optname;
	constant char *printopt;
	char *str;
	lbool set_default;
	lbool lc;
	lbool ambig;
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
		if (!(pendopt->otype & O_UNSUPPORTED))
		{
			switch (pendopt->otype & OTYPE)
			{
			case O_STRING:
				(*pendopt->ofunc)(INIT, s);
				break;
			case O_NUMBER:
				printopt = opt_desc(pendopt);
				*(pendopt->ovar) = getnumc(&s, printopt, NULL);
				break;
			}
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
				optname = ++s;
			/*
			 * "-+" or "--+" means set these options back to their defaults.
			 * (They may have been set otherwise by previous options.)
			 */
			set_default = (*s == '+');
			if (set_default)
				s++;
			if (optname != NULL)
			{
				optname = s;
				break;
			}
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
			if (s == NULL)
				return;
			if (*str == '+')
			{
				if (every_first_cmd != NULL)
					free(every_first_cmd);
				every_first_cmd = save(str+1);
			} else
			{
				ungetsc(str);
				ungetcc_end_command();
			}
			free(str);
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
		case 'n':
			if (less_is_more)
				optc = 'z';
			break;
		}

		/*
		 * Not a special case.
		 * Look up the option letter in the option table.
		 */
		ambig = FALSE;
		if (optname == NULL)
		{
			printopt = propt(optc);
			lc = ASCII_IS_LOWER(optc);
			o = findopt(optc);
		} else
		{
			printopt = optname;
			lc = ASCII_IS_LOWER(optname[0]);
			o = findopt_name(&optname, NULL, &ambig);
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
				    (o->otype & OTYPE) != O_STRING &&
				    (o->otype & OTYPE) != O_NUMBER)
				{
					parg.p_string = printopt;
					error("The %s option should not be followed by =",
						&parg);
					return;
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
			if (ambig)
				error("%s is an ambiguous abbreviation (\"less --help\" for help)",
					&parg);
			else
				error("There is no %s option (\"less --help\" for help)",
					&parg);
			return;
		}

		str = NULL;
		switch (o->otype & OTYPE)
		{
		case O_BOOL:
			if (o->otype & O_UNSUPPORTED)
				break;
			if (o->ovar != NULL)
			{
				if (set_default)
					*(o->ovar) = o->odefault;
				else
					*(o->ovar) = ! o->odefault;
			}
			break;
		case O_TRIPLE:
			if (o->otype & O_UNSUPPORTED)
				break;
			if (o->ovar != NULL)
			{
				if (set_default)
					*(o->ovar) = o->odefault;
				else if (is_env && o->ovar == &ctldisp)
					/* If -r appears in an env var, treat it as -R. */
					*(o->ovar) = OPT_ONPLUS;
				else
					*(o->ovar) = flip_triple(o->odefault, lc);
			}
			break;
		case O_STRING:
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
			if (s == NULL)
				return;
			break;
		case O_NUMBER:
			if (*s == '\0')
			{
				pendopt = o;
				return;
			}
			if (o->otype & O_UNSUPPORTED)
				break;
			*(o->ovar) = getnumc(&s, printopt, NULL);
			break;
		}
		/*
		 * If the option has a handling function, call it.
		 */
		if (o->ofunc != NULL && !(o->otype & O_UNSUPPORTED))
			(*o->ofunc)(INIT, str);
		if (str != NULL)
			free(str);
	}
}

/*
 * Toggle command line flags from within the program.
 * Used by the "-" and "_" commands.
 * how_toggle may be:
 *      OPT_NO_TOGGLE   just report the current setting, without changing it.
 *      OPT_TOGGLE      invert the current setting
 *      OPT_UNSET       set to the default value
 *      OPT_SET         set to the inverse of the default value
 */
public void toggle_option(struct loption *o, lbool lower, constant char *s, int how_toggle)
{
	int num;
	int no_prompt;
	lbool err;
	PARG parg;

	no_prompt = (how_toggle & OPT_NO_PROMPT);
	how_toggle &= ~OPT_NO_PROMPT;

	if (o == NULL)
	{
		error("No such option", NULL_PARG);
		return;
	}

	if (how_toggle == OPT_TOGGLE && (o->otype & O_NO_TOGGLE))
	{
		parg.p_string = opt_desc(o);
		error("Cannot change the %s option", &parg);
		return;
	}

	if (how_toggle == OPT_NO_TOGGLE && (o->otype & O_NO_QUERY))
	{
		parg.p_string = opt_desc(o);
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
	case O_STRING:
	case O_NUMBER:
		if (how_toggle == OPT_TOGGLE && *s == '\0')
			how_toggle = OPT_NO_TOGGLE;
		break;
	}

#if HILITE_SEARCH
	if (how_toggle != OPT_NO_TOGGLE && (o->otype & O_HL_REPAINT))
		repaint_hilite(FALSE);
#endif

	/*
	 * Now actually toggle (change) the variable.
	 */
	if (how_toggle != OPT_NO_TOGGLE)
	{
		switch (o->otype & OTYPE)
		{
		case O_BOOL:
			/*
			 * Boolean.
			 */
			if (o->ovar != NULL)
			{
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
			}
			break;
		case O_TRIPLE:
			/*
			 * Triple:
			 *      If user gave the lower case letter, then switch 
			 *      to 1 unless already 1, in which case make it 0.
			 *      If user gave the upper case letter, then switch
			 *      to 2 unless already 2, in which case make it 0.
			 */
			if (o->ovar != NULL)
			{
				switch (how_toggle)
				{
				case OPT_TOGGLE:
					*(o->ovar) = flip_triple(*(o->ovar), lower);
					break;
				case OPT_UNSET:
					*(o->ovar) = o->odefault;
					break;
				case OPT_SET:
					*(o->ovar) = flip_triple(o->odefault, lower);
					break;
				}
			}
			break;
		case O_STRING:
			/*
			 * String: don't do anything here.
			 *      The handling function will do everything.
			 */
			switch (how_toggle)
			{
			case OPT_SET:
			case OPT_UNSET:
				error("Cannot use \"-+\" or \"-!\" for a string option",
					NULL_PARG);
				return;
			}
			break;
		case O_NUMBER:
			/*
			 * Number: set the variable to the given number.
			 */
			switch (how_toggle)
			{
			case OPT_TOGGLE:
				num = getnumc(&s, NULL, &err);
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
	if (how_toggle != OPT_NO_TOGGLE && (o->otype & O_HL_REPAINT))
		chg_hilite();
#endif

	if (!no_prompt)
	{
		/*
		 * Print a message describing the new setting.
		 */
		switch (o->otype & OTYPE)
		{
		case O_BOOL:
		case O_TRIPLE:
			/*
			 * Print the odesc message.
			 */
			if (o->ovar != NULL)
				error(o->odesc[*(o->ovar)], NULL_PARG);
			break;
		case O_NUMBER:
			/*
			 * The message is in odesc[1] and has a %d for 
			 * the value of the variable.
			 */
			parg.p_int = *(o->ovar);
			error(o->odesc[1], &parg);
			break;
		case O_STRING:
			/*
			 * Message was already printed by the handling function.
			 */
			break;
		}
	}

	if (how_toggle != OPT_NO_TOGGLE && (o->otype & O_REPAINT))
		screen_trashed();
}

/*
 * "Toggle" a triple-valued option.
 */
static int flip_triple(int val, lbool lc)
{
	if (lc)
		return ((val == OPT_ON) ? OPT_OFF : OPT_ON);
	else
		return ((val == OPT_ONPLUS) ? OPT_OFF : OPT_ONPLUS);
}

/*
 * Determine if an option takes a parameter.
 */
public int opt_has_param(struct loption *o)
{
	if (o == NULL)
		return (0);
	if (o->otype & (O_BOOL|O_TRIPLE|O_NOVAR|O_NO_TOGGLE))
		return (0);
	return (1);
}

/*
 * Return the prompt to be used for a given option letter.
 * Only string and number valued options have prompts.
 */
public constant char * opt_prompt(struct loption *o)
{
	if (o == NULL || (o->otype & (O_STRING|O_NUMBER)) == 0)
		return ("?");
	return (o->odesc[0]);
}

/*
 * If the specified option can be toggled, return NULL.
 * Otherwise return an appropriate error message.
 */
public constant char * opt_toggle_disallowed(int c)
{
	switch (c)
	{
	case 'o':
		if (ch_getflags() & CH_CANSEEK)
			return "Input is not a pipe";
		break;
	}
	return NULL;
}

/*
 * Return whether or not there is a string option pending;
 * that is, if the previous option was a string-valued option letter 
 * (like -P) without a following string.
 * In that case, the current option is taken to be the string for
 * the previous option.
 */
public lbool isoptpending(void)
{
	return (pendopt != NULL);
}

/*
 * Print error message about missing string.
 */
static void nostring(constant char *printopt)
{
	PARG parg;
	parg.p_string = printopt;
	error("Value is required after %s", &parg);
}

/*
 * Print error message if a STRING type option is not followed by a string.
 */
public void nopendopt(void)
{
	nostring(opt_desc(pendopt));
}

/*
 * Scan to end of string or to an END_OPTION_STRING character.
 * In the latter case, replace the char with a null char.
 * Return a pointer to the remainder of the string, if any.
 * validchars is of the form "[-][.]d[,]".
 *   "-" means an optional leading "-" is allowed
 *   "." means an optional leading "." is allowed (after any "-")
 *   "d" indicates a string of one or more digits (0-9)
 *   "," indicates a comma-separated list of digit strings is allowed
 *   "s" means a space char terminates the argument
 */
static constant char * optstring(constant char *s, char **p_str, constant char *printopt, constant char *validchars)
{
	constant char *p;
	char *out;

	if (*s == '\0')
	{
		nostring(printopt);
		return (NULL);
	}
	/* Alloc could be more than needed, but not worth trimming. */
	*p_str = (char *) ecalloc(strlen(s)+1, sizeof(char));
	out = *p_str;

	for (p = s;  *p != '\0';  p++)
	{
		if (opt_use_backslash && *p == '\\' && p[1] != '\0')
		{
			/* Take next char literally. */
			++p;
		} else 
		{
			if (validchars != NULL)
			{
				if (validchars[0] == 's')
				{
					if (*p == ' ')
						break;
				} else if (*p == '-')
				{
					if (validchars[0] != '-')
						break;
					++validchars;
				} else if (*p == '.')
				{
					if (validchars[0] == '-')
						++validchars;
					if (validchars[0] != '.')
						break;
					++validchars;
				} else if (*p == ',')
				{
					if (validchars[0] == '\0' || validchars[1] != ',')
						break;
				} else if (*p >= '0' && *p <= '9')
				{
					while (validchars[0] == '-' || validchars[0] == '.')
						++validchars;
					if (validchars[0] != 'd')
						break;
				} else
					break;
			}
			if (*p == END_OPTION_STRING)
				/* End of option string. */
				break;
		}
		*out++ = *p;
	}
	*out = '\0';
	return (p);
}

/*
 */
static int num_error(constant char *printopt, lbool *errp, lbool overflow)
{
	PARG parg;

	if (errp != NULL)
	{
		*errp = TRUE;
		return (-1);
	}
	if (printopt != NULL)
	{
		parg.p_string = printopt;
		error((overflow
		       ? "Number too large in '%s'"
		       : "Number is required after %s"),
		      &parg);
	}
	return (-1);
}

/*
 * Translate a string into a number.
 * Like atoi(), but takes a pointer to a char *, and updates
 * the char * to point after the translated number.
 */
public int getnumc(constant char **sp, constant char *printopt, lbool *errp)
{
	constant char *s = *sp;
	int n;
	lbool neg;

	s = skipspc(s);
	neg = FALSE;
	if (*s == '-')
	{
		neg = TRUE;
		s++;
	}
	if (*s < '0' || *s > '9')
		return (num_error(printopt, errp, FALSE));

	n = lstrtoic(s, sp, 10);
	if (n < 0)
		return (num_error(printopt, errp, TRUE));
	if (errp != NULL)
		*errp = FALSE;
	if (neg)
		n = -n;
	return (n);
}

public int getnum(char **sp, constant char *printopt, lbool *errp)
{
	constant char *cs = *sp;
	int r = getnumc(&cs, printopt, errp);
	*sp = (char *) cs;
	return r;
}

/*
 * Translate a string into a fraction, represented by the part of a
 * number which would follow a decimal point.
 * The value of the fraction is returned as parts per NUM_FRAC_DENOM.
 * That is, if "n" is returned, the fraction intended is n/NUM_FRAC_DENOM.
 */
public long getfraction(constant char **sp, constant char *printopt, lbool *errp)
{
	constant char *s;
	long frac = 0;
	int fraclen = 0;

	s = skipspc(*sp);
	if (*s < '0' || *s > '9')
		return (num_error(printopt, errp, FALSE));

	for ( ;  *s >= '0' && *s <= '9';  s++)
	{
		if (NUM_LOG_FRAC_DENOM <= fraclen)
			continue;
		frac = (frac * 10) + (*s - '0');
		fraclen++;
	}
	while (fraclen++ < NUM_LOG_FRAC_DENOM)
		frac *= 10;
	*sp = s;
	if (errp != NULL)
		*errp = FALSE;
	return (frac);
}

/*
 * Set the UNSUPPORTED bit in every option listed
 * in the LESS_UNSUPPORT environment variable.
 */
public void init_unsupport(void)
{
	constant char *s = lgetenv("LESS_UNSUPPORT");
	if (isnullenv(s))
		return;
	for (;;)
	{
		struct loption *opt;
		s = skipspc(s);
		if (*s == '\0') break;
		if (*s == '-' && *++s == '\0') break;
		if (*s == '-') /* long option name */
		{
			++s;
			opt = findopt_name(&s, NULL, NULL);
		} else /* short (single-char) option */
		{
			opt = findopt(*s);
			if (opt != NULL) ++s;
		}
		if (opt != NULL)
			opt->otype |= O_UNSUPPORTED;
	}
}

/*
 * Get the value of the -e flag.
 */
public int get_quit_at_eof(void)
{
	if (!less_is_more)
		return quit_at_eof;
	/* When less_is_more is set, the -e flag semantics are different. */
	return quit_at_eof ? OPT_ONPLUS : OPT_ON;
}
