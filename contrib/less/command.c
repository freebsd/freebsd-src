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
 * User-level command processor.
 */

#include "less.h"
#if MSDOS_COMPILER==WIN32C
#include <windows.h>
#endif
#include "position.h"
#include "option.h"
#include "cmd.h"

extern int erase_char, kill_char;
extern int sigs;
extern int quit_at_eof;
extern int quit_if_one_screen;
extern int squished;
extern int hit_eof;
extern int sc_width;
extern int sc_height;
extern int swindow;
extern int jump_sline;
extern int quitting;
extern int wscroll;
extern int top_scroll;
extern int ignore_eoi;
extern int secure;
extern int hshift;
extern int show_attn;
extern char *every_first_cmd;
extern char *curr_altfilename;
extern char version[];
extern struct scrpos initial_scrpos;
extern IFILE curr_ifile;
extern void constant *ml_search;
extern void constant *ml_examine;
#if SHELL_ESCAPE || PIPEC
extern void constant *ml_shell;
#endif
#if EDITOR
extern char *editor;
extern char *editproto;
#endif
extern int screen_trashed;	/* The screen has been overwritten */
extern int shift_count;

static char ungot[UNGOT_SIZE];
static char *ungotp = NULL;
#if SHELL_ESCAPE
static char *shellcmd = NULL;	/* For holding last shell command for "!!" */
#endif
static int mca;			/* The multicharacter command (action) */
static int search_type;		/* The previous type of search */
static LINENUM number;		/* The number typed by the user */
static char optchar;
static int optflag;
static int optgetname;
static POSITION bottompos;
#if PIPEC
static char pipec;
#endif

static void multi_search();

/*
 * Move the cursor to lower left before executing a command.
 * This looks nicer if the command takes a long time before
 * updating the screen.
 */
	static void
cmd_exec()
{
	clear_attn();
	lower_left();
	flush();
}

/*
 * Set up the display to start a new multi-character command.
 */
	static void
start_mca(action, prompt, mlist, cmdflags)
	int action;
	char *prompt;
	void *mlist;
	int cmdflags;
{
	mca = action;
	clear_cmd();
	cmd_putstr(prompt);
	set_mlist(mlist, cmdflags);
}

	public int
in_mca()
{
	return (mca != 0 && mca != A_PREFIX);
}

/*
 * Set up the display to start a new search command.
 */
	static void
mca_search()
{
	if (search_type & SRCH_FORW)
		mca = A_F_SEARCH;
	else
		mca = A_B_SEARCH;

	clear_cmd();

	if (search_type & SRCH_NO_MATCH)
		cmd_putstr("Non-match ");
	if (search_type & SRCH_FIRST_FILE)
		cmd_putstr("First-file ");
	if (search_type & SRCH_PAST_EOF)
		cmd_putstr("EOF-ignore ");
	if (search_type & SRCH_NO_MOVE)
		cmd_putstr("Keep-pos ");
	if (search_type & SRCH_NO_REGEX)
		cmd_putstr("Regex-off ");

	if (search_type & SRCH_FORW)
		cmd_putstr("/");
	else
		cmd_putstr("?");
	set_mlist(ml_search, 0);
}

/*
 * Set up the display to start a new toggle-option command.
 */
	static void
mca_opt_toggle()
{
	int no_prompt;
	int flag;
	char *dash;
	
	no_prompt = (optflag & OPT_NO_PROMPT);
	flag = (optflag & ~OPT_NO_PROMPT);
	dash = (flag == OPT_NO_TOGGLE) ? "_" : "-";

	mca = A_OPT_TOGGLE;
	clear_cmd();
	cmd_putstr(dash);
	if (optgetname)
		cmd_putstr(dash);
	if (no_prompt)
		cmd_putstr("(P)");
	switch (flag)
	{
	case OPT_UNSET:
		cmd_putstr("+");
		break;
	case OPT_SET:
		cmd_putstr("!");
		break;
	}
	set_mlist(NULL, 0);
}

/*
 * Execute a multicharacter command.
 */
	static void
exec_mca()
{
	register char *cbuf;

	cmd_exec();
	cbuf = get_cmdbuf();

	switch (mca)
	{
	case A_F_SEARCH:
	case A_B_SEARCH:
		multi_search(cbuf, (int) number);
		break;
	case A_FIRSTCMD:
		/*
		 * Skip leading spaces or + signs in the string.
		 */
		while (*cbuf == '+' || *cbuf == ' ')
			cbuf++;
		if (every_first_cmd != NULL)
			free(every_first_cmd);
		if (*cbuf == '\0')
			every_first_cmd = NULL;
		else
			every_first_cmd = save(cbuf);
		break;
	case A_OPT_TOGGLE:
		toggle_option(optchar, cbuf, optflag);
		optchar = '\0';
		break;
	case A_F_BRACKET:
		match_brac(cbuf[0], cbuf[1], 1, (int) number);
		break;
	case A_B_BRACKET:
		match_brac(cbuf[1], cbuf[0], 0, (int) number);
		break;
#if EXAMINE
	case A_EXAMINE:
		if (secure)
			break;
		edit_list(cbuf);
#if TAGS
		/* If tag structure is loaded then clean it up. */
		cleantags();
#endif
		break;
#endif
#if SHELL_ESCAPE
	case A_SHELL:
		/*
		 * !! just uses whatever is in shellcmd.
		 * Otherwise, copy cmdbuf to shellcmd,
		 * expanding any special characters ("%" or "#").
		 */
		if (*cbuf != '!')
		{
			if (shellcmd != NULL)
				free(shellcmd);
			shellcmd = fexpand(cbuf);
		}

		if (secure)
			break;
		if (shellcmd == NULL)
			lsystem("", "!done");
		else
			lsystem(shellcmd, "!done");
		break;
#endif
#if PIPEC
	case A_PIPE:
		if (secure)
			break;
		(void) pipe_mark(pipec, cbuf);
		error("|done", NULL_PARG);
		break;
#endif
	}
}

/*
 * Add a character to a multi-character command.
 */
	static int
mca_char(c)
	int c;
{
	char *p;
	int flag;
	char buf[3];
	PARG parg;

	switch (mca)
	{
	case 0:
		/*
		 * Not in a multicharacter command.
		 */
		return (NO_MCA);

	case A_PREFIX:
		/*
		 * In the prefix of a command.
		 * This not considered a multichar command
		 * (even tho it uses cmdbuf, etc.).
		 * It is handled in the commands() switch.
		 */
		return (NO_MCA);

	case A_DIGIT:
		/*
		 * Entering digits of a number.
		 * Terminated by a non-digit.
		 */
		if ((c < '0' || c > '9') && 
		  editchar(c, EC_PEEK|EC_NOHISTORY|EC_NOCOMPLETE|EC_NORIGHTLEFT) == A_INVALID)
		{
			/*
			 * Not part of the number.
			 * Treat as a normal command character.
			 */
			number = cmd_int();
			mca = 0;
			cmd_accept();
			return (NO_MCA);
		}
		break;

	case A_OPT_TOGGLE:
		/*
		 * Special case for the TOGGLE_OPTION command.
		 * If the option letter which was entered is a
		 * single-char option, execute the command immediately,
		 * so user doesn't have to hit RETURN.
		 * If the first char is + or -, this indicates
		 * OPT_UNSET or OPT_SET respectively, instead of OPT_TOGGLE.
		 * "--" begins inputting a long option name.
		 */
		if (optchar == '\0' && len_cmdbuf() == 0)
		{
			flag = (optflag & ~OPT_NO_PROMPT);
			if (flag == OPT_NO_TOGGLE)
			{
				switch (c)
				{
				case '_':
					/* "__" = long option name. */
					optgetname = TRUE;
					mca_opt_toggle();
					return (MCA_MORE);
				}
			} else
			{
				switch (c)
				{
				case '+':
					/* "-+" = UNSET. */
					optflag = (flag == OPT_UNSET) ?
						OPT_TOGGLE : OPT_UNSET;
					mca_opt_toggle();
					return (MCA_MORE);
				case '!':
					/* "-!" = SET */
					optflag = (flag == OPT_SET) ?
						OPT_TOGGLE : OPT_SET;
					mca_opt_toggle();
					return (MCA_MORE);
				case CONTROL('P'):
					optflag ^= OPT_NO_PROMPT;
					mca_opt_toggle();
					return (MCA_MORE);
				case '-':
					/* "--" = long option name. */
					optgetname = TRUE;
					mca_opt_toggle();
					return (MCA_MORE);
				}
			}
		}
		if (optgetname)
		{
			/*
			 * We're getting a long option name.
			 * See if we've matched an option name yet.
			 * If so, display the complete name and stop 
			 * accepting chars until user hits RETURN.
			 */
			struct loption *o;
			char *oname;
			int lc;

			if (c == '\n' || c == '\r')
			{
				/*
				 * When the user hits RETURN, make sure
				 * we've matched an option name, then
				 * pretend he just entered the equivalent
				 * option letter.
				 */
				if (optchar == '\0')
				{
					parg.p_string = get_cmdbuf();
					error("There is no --%s option", &parg);
					return (MCA_DONE);
				}
				optgetname = FALSE;
				cmd_reset();
				c = optchar;
			} else
			{
				if (optchar != '\0')
				{
					/*
					 * Already have a match for the name.
					 * Don't accept anything but erase/kill.
					 */
					if (c == erase_char || c == kill_char)
						return (MCA_DONE);
					return (MCA_MORE);
				}
				/*
				 * Add char to cmd buffer and try to match
				 * the option name.
				 */
				if (cmd_char(c) == CC_QUIT)
					return (MCA_DONE);
				p = get_cmdbuf();
				lc = islower(p[0]);
				o = findopt_name(&p, &oname, NULL);
				if (o != NULL)
				{
					/*
					 * Got a match.
					 * Remember the option letter and
					 * display the full option name.
					 */
					optchar = o->oletter;
					if (!lc && islower(optchar))
						optchar = toupper(optchar);
					cmd_reset();
					mca_opt_toggle();
					for (p = oname;  *p != '\0';  p++)
					{
						c = *p;
						if (!lc && islower(c))
							c = toupper(c);
						if (cmd_char(c) != CC_OK)
							return (MCA_DONE);
					}
				}
				return (MCA_MORE);
			}
		} else
		{
			if (c == erase_char || c == kill_char)
				break;
			if (optchar != '\0')
				/* We already have the option letter. */
				break;
		}

		optchar = c;
		if ((optflag & ~OPT_NO_PROMPT) != OPT_TOGGLE ||
		    single_char_option(c))
		{
			toggle_option(c, "", optflag);
			return (MCA_DONE);
		}
		/*
		 * Display a prompt appropriate for the option letter.
		 */
		if ((p = opt_prompt(c)) == NULL)
		{
			buf[0] = '-';
			buf[1] = c;
			buf[2] = '\0';
			p = buf;
		}
		start_mca(A_OPT_TOGGLE, p, (void*)NULL, 0);
		return (MCA_MORE);

	case A_F_SEARCH:
	case A_B_SEARCH:
		/*
		 * Special case for search commands.
		 * Certain characters as the first char of 
		 * the pattern have special meaning:
		 *	!  Toggle the NO_MATCH flag
		 *	*  Toggle the PAST_EOF flag
		 *	@  Toggle the FIRST_FILE flag
		 */
		if (len_cmdbuf() > 0)
			/*
			 * Only works for the first char of the pattern.
			 */
			break;

		flag = 0;
		switch (c)
		{
		case CONTROL('E'): /* ignore END of file */
		case '*':
			flag = SRCH_PAST_EOF;
			break;
		case CONTROL('F'): /* FIRST file */
		case '@':
			flag = SRCH_FIRST_FILE;
			break;
		case CONTROL('K'): /* KEEP position */
			flag = SRCH_NO_MOVE;
			break;
		case CONTROL('R'): /* Don't use REGULAR EXPRESSIONS */
			flag = SRCH_NO_REGEX;
			break;
		case CONTROL('N'): /* NOT match */
		case '!':
			flag = SRCH_NO_MATCH;
			break;
		}
		if (flag != 0)
		{
			search_type ^= flag;
			mca_search();
			return (MCA_MORE);
		}
		break;
	}

	/*
	 * Any other multicharacter command
	 * is terminated by a newline.
	 */
	if (c == '\n' || c == '\r')
	{
		/*
		 * Execute the command.
		 */
		exec_mca();
		return (MCA_DONE);
	}

	/*
	 * Append the char to the command buffer.
	 */
	if (cmd_char(c) == CC_QUIT)
		/*
		 * Abort the multi-char command.
		 */
		return (MCA_DONE);

	if ((mca == A_F_BRACKET || mca == A_B_BRACKET) && len_cmdbuf() >= 2)
	{
		/*
		 * Special case for the bracket-matching commands.
		 * Execute the command after getting exactly two
		 * characters from the user.
		 */
		exec_mca();
		return (MCA_DONE);
	}

	/*
	 * Need another character.
	 */
	return (MCA_MORE);
}

/*
 * Make sure the screen is displayed.
 */
	static void
make_display()
{
	/*
	 * If nothing is displayed yet, display starting from initial_scrpos.
	 */
	if (empty_screen())
	{
		if (initial_scrpos.pos == NULL_POSITION)
			/*
			 * {{ Maybe this should be:
			 *    jump_loc(ch_zero(), jump_sline);
			 *    but this behavior seems rather unexpected 
			 *    on the first screen. }}
			 */
			jump_loc(ch_zero(), 1);
		else
			jump_loc(initial_scrpos.pos, initial_scrpos.ln);
	} else if (screen_trashed)
	{
		int save_top_scroll;
		save_top_scroll = top_scroll;
		top_scroll = 1;
		repaint();
		top_scroll = save_top_scroll;
	}
}

/*
 * Display the appropriate prompt.
 */
	static void
prompt()
{
	register char *p;

	if (ungotp != NULL && ungotp > ungot)
	{
		/*
		 * No prompt necessary if commands are from 
		 * ungotten chars rather than from the user.
		 */
		return;
	}

	/*
	 * Make sure the screen is displayed.
	 */
	make_display();
	bottompos = position(BOTTOM_PLUS_ONE);

	/*
	 * If the -E flag is set and we've hit EOF on the last file, quit.
	 */
	if ((quit_at_eof == OPT_ONPLUS || quit_if_one_screen) &&
	    hit_eof && !(ch_getflags() & CH_HELPFILE) && 
	    next_ifile(curr_ifile) == NULL_IFILE)
		quit(QUIT_OK);
	quit_if_one_screen = FALSE;
#if 0 /* This doesn't work well because some "te"s clear the screen. */
	/*
	 * If the -e flag is set and we've hit EOF on the last file,
	 * and the file is squished (shorter than the screen), quit.
	 */
	if (quit_at_eof && squished &&
	    next_ifile(curr_ifile) == NULL_IFILE)
		quit(QUIT_OK);
#endif

#if MSDOS_COMPILER==WIN32C
	/* 
	 * In Win32, display the file name in the window title.
	 */
	if (!(ch_getflags() & CH_HELPFILE))
		SetConsoleTitle(pr_expand("Less?f - %f.", 0));
#endif
	/*
	 * Select the proper prompt and display it.
	 */
	clear_cmd();
	p = pr_string();
	if (p == NULL)
		putchr(':');
	else
	{
		so_enter();
		putstr(p);
		so_exit();
	}
}

/*
 * Display the less version message.
 */
	public void
dispversion()
{
	PARG parg;

	parg.p_string = version;
	error("less %s", &parg);
}

/*
 * Get command character.
 * The character normally comes from the keyboard,
 * but may come from ungotten characters
 * (characters previously given to ungetcc or ungetsc).
 */
	public int
getcc()
{
	if (ungotp == NULL)
		/*
		 * Normal case: no ungotten chars, so get one from the user.
		 */
		return (getchr());

	if (ungotp > ungot)
		/*
		 * Return the next ungotten char.
		 */
		return (*--ungotp);

	/*
	 * We have just run out of ungotten chars.
	 */
	ungotp = NULL;
	if (len_cmdbuf() == 0 || !empty_screen())
		return (getchr());
	/*
	 * Command is incomplete, so try to complete it.
	 */
	switch (mca)
	{
	case A_DIGIT:
		/*
		 * We have a number but no command.  Treat as #g.
		 */
		return ('g');

	case A_F_SEARCH:
	case A_B_SEARCH:
		/*
		 * We have "/string" but no newline.  Add the \n.
		 */
		return ('\n'); 

	default:
		/*
		 * Some other incomplete command.  Let user complete it.
		 */
		return (getchr());
	}
}

/*
 * "Unget" a command character.
 * The next getcc() will return this character.
 */
	public void
ungetcc(c)
	int c;
{
	if (ungotp == NULL)
		ungotp = ungot;
	if (ungotp >= ungot + sizeof(ungot))
	{
		error("ungetcc overflow", NULL_PARG);
		quit(QUIT_ERROR);
	}
	*ungotp++ = c;
}

/*
 * Unget a whole string of command characters.
 * The next sequence of getcc()'s will return this string.
 */
	public void
ungetsc(s)
	char *s;
{
	register char *p;

	for (p = s + strlen(s) - 1;  p >= s;  p--)
		ungetcc(*p);
}

/*
 * Search for a pattern, possibly in multiple files.
 * If SRCH_FIRST_FILE is set, begin searching at the first file.
 * If SRCH_PAST_EOF is set, continue the search thru multiple files.
 */
	static void
multi_search(pattern, n)
	char *pattern;
	int n;
{
	register int nomore;
	IFILE save_ifile;
	int changed_file;

	changed_file = 0;
	save_ifile = save_curr_ifile();

	if (search_type & SRCH_FIRST_FILE)
	{
		/*
		 * Start at the first (or last) file 
		 * in the command line list.
		 */
		if (search_type & SRCH_FORW)
			nomore = edit_first();
		else
			nomore = edit_last();
		if (nomore)
		{
			unsave_ifile(save_ifile);
			return;
		}
		changed_file = 1;
		search_type &= ~SRCH_FIRST_FILE;
	}

	for (;;)
	{
		n = search(search_type, pattern, n);
		/*
		 * The SRCH_NO_MOVE flag doesn't "stick": it gets cleared
		 * after being used once.  This allows "n" to work after
		 * using a /@@ search.
		 */
		search_type &= ~SRCH_NO_MOVE;
		if (n == 0)
		{
			/*
			 * Found it.
			 */
			unsave_ifile(save_ifile);
			return;
		}

		if (n < 0)
			/*
			 * Some kind of error in the search.
			 * Error message has been printed by search().
			 */
			break;

		if ((search_type & SRCH_PAST_EOF) == 0)
			/*
			 * We didn't find a match, but we're
			 * supposed to search only one file.
			 */
			break;
		/*
		 * Move on to the next file.
		 */
		if (search_type & SRCH_FORW)
			nomore = edit_next(1);
		else
			nomore = edit_prev(1);
		if (nomore)
			break;
		changed_file = 1;
	}

	/*
	 * Didn't find it.
	 * Print an error message if we haven't already.
	 */
	if (n > 0)
		error("Pattern not found", NULL_PARG);

	if (changed_file)
	{
		/*
		 * Restore the file we were originally viewing.
		 */
		reedit_ifile(save_ifile);
	}
}

/*
 * Main command processor.
 * Accept and execute commands until a quit command.
 */
	public void
commands()
{
	register int c;
	register int action;
	register char *cbuf;
	int newaction;
	int save_search_type;
	char *extra;
	char tbuf[2];
	PARG parg;
	IFILE old_ifile;
	IFILE new_ifile;
	char *tagfile;

	search_type = SRCH_FORW;
	wscroll = (sc_height + 1) / 2;
	newaction = A_NOACTION;

	for (;;)
	{
		mca = 0;
		cmd_accept();
		number = 0;
		optchar = '\0';

		/*
		 * See if any signals need processing.
		 */
		if (sigs)
		{
			psignals();
			if (quitting)
				quit(QUIT_SAVED_STATUS);
		}

		/*
		 * See if window size changed, for systems that don't
		 * generate SIGWINCH.
		 */
		check_winch();

		/*
		 * Display prompt and accept a character.
		 */
		cmd_reset();
		prompt();
		if (sigs)
			continue;
		if (newaction == A_NOACTION)
			c = getcc();

	again:
		if (sigs)
			continue;

		if (newaction != A_NOACTION)
		{
			action = newaction;
			newaction = A_NOACTION;
		} else
		{
			/*
			 * If we are in a multicharacter command, call mca_char.
			 * Otherwise we call fcmd_decode to determine the
			 * action to be performed.
			 */
			if (mca)
				switch (mca_char(c))
				{
				case MCA_MORE:
					/*
					 * Need another character.
					 */
					c = getcc();
					goto again;
				case MCA_DONE:
					/*
					 * Command has been handled by mca_char.
					 * Start clean with a prompt.
					 */
					continue;
				case NO_MCA:
					/*
					 * Not a multi-char command
					 * (at least, not anymore).
					 */
					break;
				}

			/*
			 * Decode the command character and decide what to do.
			 */
			if (mca)
			{
				/*
				 * We're in a multichar command.
				 * Add the character to the command buffer
				 * and display it on the screen.
				 * If the user backspaces past the start 
				 * of the line, abort the command.
				 */
				if (cmd_char(c) == CC_QUIT || len_cmdbuf() == 0)
					continue;
				cbuf = get_cmdbuf();
			} else
			{
				/*
				 * Don't use cmd_char if we're starting fresh
				 * at the beginning of a command, because we
				 * don't want to echo the command until we know
				 * it is a multichar command.  We also don't
				 * want erase_char/kill_char to be treated
				 * as line editing characters.
				 */
				tbuf[0] = c;
				tbuf[1] = '\0';
				cbuf = tbuf;
			}
			extra = NULL;
			action = fcmd_decode(cbuf, &extra);
			/*
			 * If an "extra" string was returned,
			 * process it as a string of command characters.
			 */
			if (extra != NULL)
				ungetsc(extra);
		}
		/*
		 * Clear the cmdbuf string.
		 * (But not if we're in the prefix of a command,
		 * because the partial command string is kept there.)
		 */
		if (action != A_PREFIX)
			cmd_reset();

		switch (action)
		{
		case A_DIGIT:
			/*
			 * First digit of a number.
			 */
			start_mca(A_DIGIT, ":", (void*)NULL, CF_QUIT_ON_ERASE);
			goto again;

		case A_F_WINDOW:
			/*
			 * Forward one window (and set the window size).
			 */
			if (number > 0)
				swindow = (int) number;
			/* FALLTHRU */
		case A_F_SCREEN:
			/*
			 * Forward one screen.
			 */
			if (number <= 0)
				number = get_swindow();
			cmd_exec();
			if (show_attn)
				set_attnpos(bottompos);
			forward((int) number, 0, 1);
			break;

		case A_B_WINDOW:
			/*
			 * Backward one window (and set the window size).
			 */
			if (number > 0)
				swindow = (int) number;
			/* FALLTHRU */
		case A_B_SCREEN:
			/*
			 * Backward one screen.
			 */
			if (number <= 0)
				number = get_swindow();
			cmd_exec();
			backward((int) number, 0, 1);
			break;

		case A_F_LINE:
			/*
			 * Forward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			if (show_attn == OPT_ONPLUS && number > 1)
				set_attnpos(bottompos);
			forward((int) number, 0, 0);
			break;

		case A_B_LINE:
			/*
			 * Backward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			backward((int) number, 0, 0);
			break;

		case A_FF_LINE:
			/*
			 * Force forward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			if (show_attn == OPT_ONPLUS && number > 1)
				set_attnpos(bottompos);
			forward((int) number, 1, 0);
			break;

		case A_BF_LINE:
			/*
			 * Force backward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			backward((int) number, 1, 0);
			break;
		
		case A_FF_SCREEN:
			/*
			 * Force forward one screen.
			 */
			if (number <= 0)
				number = get_swindow();
			cmd_exec();
			if (show_attn == OPT_ONPLUS)
				set_attnpos(bottompos);
			forward((int) number, 1, 0);
			break;

		case A_F_FOREVER:
			/*
			 * Forward forever, ignoring EOF.
			 */
			if (ch_getflags() & CH_HELPFILE)
				break;
			cmd_exec();
			jump_forw();
			ignore_eoi = 1;
			hit_eof = 0;
			while (!sigs)
				forward(1, 0, 0);
			ignore_eoi = 0;
			/*
			 * This gets us back in "F mode" after processing 
			 * a non-abort signal (e.g. window-change).  
			 */
			if (sigs && !ABORT_SIGS())
				newaction = A_F_FOREVER;
			break;

		case A_F_SCROLL:
			/*
			 * Forward N lines 
			 * (default same as last 'd' or 'u' command).
			 */
			if (number > 0)
				wscroll = (int) number;
			cmd_exec();
			if (show_attn == OPT_ONPLUS)
				set_attnpos(bottompos);
			forward(wscroll, 0, 0);
			break;

		case A_B_SCROLL:
			/*
			 * Forward N lines 
			 * (default same as last 'd' or 'u' command).
			 */
			if (number > 0)
				wscroll = (int) number;
			cmd_exec();
			backward(wscroll, 0, 0);
			break;

		case A_FREPAINT:
			/*
			 * Flush buffers, then repaint screen.
			 * Don't flush the buffers on a pipe!
			 */
			if (ch_getflags() & CH_CANSEEK)
			{
				ch_flush();
				clr_linenum();
#if HILITE_SEARCH
				clr_hilite();
#endif
			}
			/* FALLTHRU */
		case A_REPAINT:
			/*
			 * Repaint screen.
			 */
			cmd_exec();
			repaint();
			break;

		case A_GOLINE:
			/*
			 * Go to line N, default beginning of file.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			jump_back(number);
			break;

		case A_PERCENT:
			/*
			 * Go to a specified percentage into the file.
			 */
			if (number < 0)
				number = 0;
			if (number > 100)
				number = 100;
			cmd_exec();
			jump_percent((int) number);
			break;

		case A_GOEND:
			/*
			 * Go to line N, default end of file.
			 */
			cmd_exec();
			if (number <= 0)
				jump_forw();
			else
				jump_back(number);
			break;

		case A_GOPOS:
			/*
			 * Go to a specified byte position in the file.
			 */
			cmd_exec();
			if (number < 0)
				number = 0;
			jump_line_loc((POSITION) number, jump_sline);
			break;

		case A_STAT:
			/*
			 * Print file name, etc.
			 */
			if (ch_getflags() & CH_HELPFILE)
				break;
			cmd_exec();
			parg.p_string = eq_message();
			error("%s", &parg);
			break;

		case A_VERSION:
			/*
			 * Print version number, without the "@(#)".
			 */
			cmd_exec();
			dispversion();
			break;

		case A_QUIT:
			/*
			 * Exit.
			 */
			if (curr_ifile != NULL_IFILE && 
			    ch_getflags() & CH_HELPFILE)
			{
				/*
				 * Quit while viewing the help file
				 * just means return to viewing the
				 * previous file.
				 */
				if (edit_prev(1) == 0)
					break;
			}
			if (extra != NULL)
				quit(*extra);
			quit(QUIT_OK);
			break;

/*
 * Define abbreviation for a commonly used sequence below.
 */
#define	DO_SEARCH()	if (number <= 0) number = 1;	\
			mca_search();			\
			cmd_exec();			\
			multi_search((char *)NULL, (int) number);


		case A_F_SEARCH:
			/*
			 * Search forward for a pattern.
			 * Get the first char of the pattern.
			 */
			search_type = SRCH_FORW;
			if (number <= 0)
				number = 1;
			mca_search();
			c = getcc();
			goto again;

		case A_B_SEARCH:
			/*
			 * Search backward for a pattern.
			 * Get the first char of the pattern.
			 */
			search_type = SRCH_BACK;
			if (number <= 0)
				number = 1;
			mca_search();
			c = getcc();
			goto again;

		case A_AGAIN_SEARCH:
			/*
			 * Repeat previous search.
			 */
			DO_SEARCH();
			break;
		
		case A_T_AGAIN_SEARCH:
			/*
			 * Repeat previous search, multiple files.
			 */
			search_type |= SRCH_PAST_EOF;
			DO_SEARCH();
			break;

		case A_REVERSE_SEARCH:
			/*
			 * Repeat previous search, in reverse direction.
			 */
			save_search_type = search_type;
			search_type = SRCH_REVERSE(search_type);
			DO_SEARCH();
			search_type = save_search_type;
			break;

		case A_T_REVERSE_SEARCH:
			/* 
			 * Repeat previous search, 
			 * multiple files in reverse direction.
			 */
			save_search_type = search_type;
			search_type = SRCH_REVERSE(search_type);
			search_type |= SRCH_PAST_EOF;
			DO_SEARCH();
			search_type = save_search_type;
			break;

		case A_UNDO_SEARCH:
			undo_search();
			break;

		case A_HELP:
			/*
			 * Help.
			 */
			if (ch_getflags() & CH_HELPFILE)
				break;
			cmd_exec();
			(void) edit(FAKE_HELPFILE);
			break;

		case A_EXAMINE:
#if EXAMINE
			/*
			 * Edit a new file.  Get the filename.
			 */
			if (secure)
			{
				error("Command not available", NULL_PARG);
				break;
			}
			start_mca(A_EXAMINE, "Examine: ", ml_examine, 0);
			c = getcc();
			goto again;
#else
			error("Command not available", NULL_PARG);
			break;
#endif
			
		case A_VISUAL:
			/*
			 * Invoke an editor on the input file.
			 */
#if EDITOR
			if (secure)
			{
				error("Command not available", NULL_PARG);
				break;
			}
			if (ch_getflags() & CH_HELPFILE)
				break;
			if (strcmp(get_filename(curr_ifile), "-") == 0)
			{
				error("Cannot edit standard input", NULL_PARG);
				break;
			}
			if (curr_altfilename != NULL)
			{
				error("Cannot edit file processed with LESSOPEN", 
					NULL_PARG);
				break;
			}
			start_mca(A_SHELL, "!", ml_shell, 0);
			/*
			 * Expand the editor prototype string
			 * and pass it to the system to execute.
			 * (Make sure the screen is displayed so the
			 * expansion of "+%lm" works.)
			 */
			make_display();
			cmd_exec();
			lsystem(pr_expand(editproto, 0), (char*)NULL);
			break;
#else
			error("Command not available", NULL_PARG);
			break;
#endif

		case A_NEXT_FILE:
			/*
			 * Examine next file.
			 */
#if TAGS
			if (ntags())
			{
				error("No next file", NULL_PARG);
				break;
			}
#endif
			if (number <= 0)
				number = 1;
			if (edit_next((int) number))
			{
				if (quit_at_eof && hit_eof && 
				    !(ch_getflags() & CH_HELPFILE))
					quit(QUIT_OK);
				parg.p_string = (number > 1) ? "(N-th) " : "";
				error("No %snext file", &parg);
			}
			break;

		case A_PREV_FILE:
			/*
			 * Examine previous file.
			 */
#if TAGS
			if (ntags())
			{
				error("No previous file", NULL_PARG);
				break;
			}
#endif
			if (number <= 0)
				number = 1;
			if (edit_prev((int) number))
			{
				parg.p_string = (number > 1) ? "(N-th) " : "";
				error("No %sprevious file", &parg);
			}
			break;

		case A_NEXT_TAG:
#if TAGS
			if (number <= 0)
				number = 1;
			tagfile = nexttag((int) number);
			if (tagfile == NULL)
			{
				error("No next tag", NULL_PARG);
				break;
			}
			if (edit(tagfile) == 0)
			{
				POSITION pos = tagsearch();
				if (pos != NULL_POSITION)
					jump_loc(pos, jump_sline);
			}
#else
			error("Command not available", NULL_PARG);
#endif
			break;

		case A_PREV_TAG:
#if TAGS
			if (number <= 0)
				number = 1;
			tagfile = prevtag((int) number);
			if (tagfile == NULL)
			{
				error("No previous tag", NULL_PARG);
				break;
			}
			if (edit(tagfile) == 0)
			{
				POSITION pos = tagsearch();
				if (pos != NULL_POSITION)
					jump_loc(pos, jump_sline);
			}
#else
			error("Command not available", NULL_PARG);
#endif
			break;

		case A_INDEX_FILE:
			/*
			 * Examine a particular file.
			 */
			if (number <= 0)
				number = 1;
			if (edit_index((int) number))
				error("No such file", NULL_PARG);
			break;

		case A_REMOVE_FILE:
			if (ch_getflags() & CH_HELPFILE)
				break;
			old_ifile = curr_ifile;
			new_ifile = getoff_ifile(curr_ifile);
			if (new_ifile == NULL_IFILE)
			{
				bell();
				break;
			}
			if (edit_ifile(new_ifile) != 0)
			{
				reedit_ifile(old_ifile);
				break;
			}
			del_ifile(old_ifile);
			break;

		case A_OPT_TOGGLE:
			optflag = OPT_TOGGLE;
			optgetname = FALSE;
			mca_opt_toggle();
			c = getcc();
			goto again;

		case A_DISP_OPTION:
			/*
			 * Report a flag setting.
			 */
			optflag = OPT_NO_TOGGLE;
			optgetname = FALSE;
			mca_opt_toggle();
			c = getcc();
			goto again;

		case A_FIRSTCMD:
			/*
			 * Set an initial command for new files.
			 */
			start_mca(A_FIRSTCMD, "+", (void*)NULL, 0);
			c = getcc();
			goto again;

		case A_SHELL:
			/*
			 * Shell escape.
			 */
#if SHELL_ESCAPE
			if (secure)
			{
				error("Command not available", NULL_PARG);
				break;
			}
			start_mca(A_SHELL, "!", ml_shell, 0);
			c = getcc();
			goto again;
#else
			error("Command not available", NULL_PARG);
			break;
#endif

		case A_SETMARK:
			/*
			 * Set a mark.
			 */
			if (ch_getflags() & CH_HELPFILE)
				break;
			start_mca(A_SETMARK, "mark: ", (void*)NULL, 0);
			c = getcc();
			if (c == erase_char || c == kill_char ||
			    c == '\n' || c == '\r')
				break;
			setmark(c);
			break;

		case A_GOMARK:
			/*
			 * Go to a mark.
			 */
			start_mca(A_GOMARK, "goto mark: ", (void*)NULL, 0);
			c = getcc();
			if (c == erase_char || c == kill_char || 
			    c == '\n' || c == '\r')
				break;
			gomark(c);
			break;

		case A_PIPE:
#if PIPEC
			if (secure)
			{
				error("Command not available", NULL_PARG);
				break;
			}
			start_mca(A_PIPE, "|mark: ", (void*)NULL, 0);
			c = getcc();
			if (c == erase_char || c == kill_char)
				break;
			if (c == '\n' || c == '\r')
				c = '.';
			if (badmark(c))
				break;
			pipec = c;
			start_mca(A_PIPE, "!", ml_shell, 0);
			c = getcc();
			goto again;
#else
			error("Command not available", NULL_PARG);
			break;
#endif

		case A_B_BRACKET:
		case A_F_BRACKET:
			start_mca(action, "Brackets: ", (void*)NULL, 0);
			c = getcc();
			goto again;

		case A_LSHIFT:
			if (number > 0)
				shift_count = number;
			else
				number = (shift_count > 0) ?
					shift_count : sc_width / 2;
			if (number > hshift)
				number = hshift;
			hshift -= number;
			screen_trashed = 1;
			break;

		case A_RSHIFT:
			if (number > 0)
				shift_count = number;
			else
				number = (shift_count > 0) ?
					shift_count : sc_width / 2;
			hshift += number;
			screen_trashed = 1;
			break;

		case A_PREFIX:
			/*
			 * The command is incomplete (more chars are needed).
			 * Display the current char, so the user knows
			 * what's going on, and get another character.
			 */
			if (mca != A_PREFIX)
			{
				cmd_reset();
				start_mca(A_PREFIX, " ", (void*)NULL,
					CF_QUIT_ON_ERASE);
				(void) cmd_char(c);
			}
			c = getcc();
			goto again;

		case A_NOACTION:
			break;

		default:
			bell();
			break;
		}
	}
}
