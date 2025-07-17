/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
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

extern int erase_char, erase2_char, kill_char;
extern int sigs;
extern int quit_if_one_screen;
extern int one_screen;
extern int sc_width;
extern int sc_height;
extern char *kent;
extern int swindow;
extern int jump_sline;
extern lbool quitting;
extern int wscroll;
extern int top_scroll;
extern int ignore_eoi;
extern int hshift;
extern int bs_mode;
extern int proc_backspace;
extern int show_attn;
extern int less_is_more;
extern int chopline;
extern POSITION highest_hilite;
extern char *every_first_cmd;
extern char version[];
extern struct scrpos initial_scrpos;
extern IFILE curr_ifile;
extern void *ml_search;
extern void *ml_examine;
extern int wheel_lines;
extern int def_search_type;
extern lbool search_wrapped;
extern int no_paste;
extern lbool pasting;
extern int no_edit_warn;
extern POSITION soft_eof;
#if SHELL_ESCAPE || PIPEC
extern void *ml_shell;
#endif
#if EDITOR
extern constant char *editproto;
#endif
#if OSC8_LINK
extern char *osc8_uri;
#endif
extern int shift_count;
extern int forw_prompt;
extern int incr_search;
extern int full_screen;
#if MSDOS_COMPILER==WIN32C
extern int utf_mode;
extern unsigned less_acp;
#endif

#if SHELL_ESCAPE
static char *shellcmd = NULL;   /* For holding last shell command for "!!" */
#endif
static int mca;                 /* The multicharacter command (action) */
static int search_type;         /* The previous type of search */
static int last_search_type;    /* Type of last executed search */
static LINENUM number;          /* The number typed by the user */
static long fraction;           /* The fractional part of the number */
static struct loption *curropt;
static lbool opt_lower;
static int optflag;
static lbool optgetname;
static POSITION bottompos;
static int save_hshift;
static int save_bs_mode;
static int save_proc_backspace;
static int screen_trashed_value = 0;
static lbool literal_char = FALSE;
static lbool ignoring_input = FALSE;
#if HAVE_TIME
static time_type ignoring_input_time;
#endif
#if PIPEC
static char pipec;
#endif

/* Stack of ungotten chars (via ungetcc) */
struct ungot {
	struct ungot *ug_next;
	char ug_char;
	lbool ug_end_command;
};
static struct ungot* ungot = NULL;

static void multi_search(constant char *pattern, int n, int silent);

/*
 * Move the cursor to start of prompt line before executing a command.
 * This looks nicer if the command takes a long time before
 * updating the screen.
 */
public void cmd_exec(void)
{
	clear_attn();
	clear_bot();
	flush();
}

/*
 * Indicate we are reading a multi-character command.
 */
static void set_mca(int action)
{
	mca = action;
	clear_bot();
	clear_cmd();
}

/*
 * Indicate we are not reading a multi-character command.
 */
static void clear_mca(void)
{
	if (mca == 0)
		return;
	mca = 0;
}

/*
 * Set up the display to start a new multi-character command.
 */
static void start_mca(int action, constant char *prompt, void *mlist, int cmdflags)
{
	set_mca(action);
	cmd_putstr(prompt);
	set_mlist(mlist, cmdflags);
}

public int in_mca(void)
{
	return (mca != 0 && mca != A_PREFIX);
}

/*
 * Set up the display to start a new search command.
 */
static void mca_search1(void)
{
	int i;

#if HILITE_SEARCH
	if (search_type & SRCH_FILTER)
		set_mca(A_FILTER);
	else 
#endif
	if (search_type & SRCH_FORW)
		set_mca(A_F_SEARCH);
	else
		set_mca(A_B_SEARCH);

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
	if (search_type & SRCH_WRAP)
		cmd_putstr("Wrap ");
	for (i = 1; i <= NUM_SEARCH_COLORS; i++)
	{
		if (search_type & SRCH_SUBSEARCH(i))
		{
			char buf[INT_STRLEN_BOUND(int)+8];
			SNPRINTF1(buf, sizeof(buf), "Sub-%d ", i);
			cmd_putstr(buf);
		}
	}
	if (literal_char)
		cmd_putstr("Lit ");

#if HILITE_SEARCH
	if (search_type & SRCH_FILTER)
		cmd_putstr("&/");
	else 
#endif
	if (search_type & SRCH_FORW)
		cmd_putstr("/");
	else
		cmd_putstr("?");
	forw_prompt = 0;
}

static void mca_search(void)
{
	mca_search1();
	set_mlist(ml_search, 0);
}

/*
 * Set up the display to start a new toggle-option command.
 */
static void mca_opt_toggle(void)
{
	int no_prompt = (optflag & OPT_NO_PROMPT);
	int flag = (optflag & ~OPT_NO_PROMPT);
	constant char *dash = (flag == OPT_NO_TOGGLE) ? "_" : "-";
	
	set_mca(A_OPT_TOGGLE);
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
	forw_prompt = 0;
	set_mlist(NULL, CF_OPTION);
}

/*
 * Execute a multicharacter command.
 */
static void exec_mca(void)
{
	constant char *cbuf;

	cmd_exec();
	cbuf = get_cmdbuf();
	if (cbuf == NULL)
		return;

	switch (mca)
	{
	case A_F_SEARCH:
	case A_B_SEARCH:
		multi_search(cbuf, (int) number, 0);
		break;
#if HILITE_SEARCH
	case A_FILTER:
		search_type ^= SRCH_NO_MATCH;
		set_filter_pattern(cbuf, search_type);
		soft_eof = NULL_POSITION;
		break;
#endif
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
		toggle_option(curropt, opt_lower, cbuf, optflag);
		curropt = NULL;
		break;
	case A_F_BRACKET:
		match_brac(cbuf[0], cbuf[1], 1, (int) number);
		break;
	case A_B_BRACKET:
		match_brac(cbuf[1], cbuf[0], 0, (int) number);
		break;
#if EXAMINE
	case A_EXAMINE: {
		char *p;
		if (!secure_allow(SF_EXAMINE))
			break;
		p = save(cbuf);
		edit_list(p);
		free(p);
#if TAGS
		/* If tag structure is loaded then clean it up. */
		cleantags();
#endif
		break; }
#endif
#if SHELL_ESCAPE
	case A_SHELL: {
		/*
		 * !! just uses whatever is in shellcmd.
		 * Otherwise, copy cmdbuf to shellcmd,
		 * expanding any special characters ("%" or "#").
		 */
		constant char *done_msg = (*cbuf == CONTROL('P')) ? NULL : "!done";
		if (done_msg == NULL)
			++cbuf;
		if (*cbuf != '!')
		{
			if (shellcmd != NULL)
				free(shellcmd);
			shellcmd = fexpand(cbuf);
		}
		if (!secure_allow(SF_SHELL))
			break;
		if (shellcmd == NULL)
			shellcmd = "";
		lsystem(shellcmd, done_msg);
		break; }
	case A_PSHELL: {
		constant char *done_msg = (*cbuf == CONTROL('P')) ? NULL : "#done";
		if (done_msg == NULL)
			++cbuf;
		if (!secure_allow(SF_SHELL))
			break;
		lsystem(pr_expand(cbuf), done_msg);
		break; }
#endif
#if PIPEC
	case A_PIPE: {
		constant char *done_msg = (*cbuf == CONTROL('P')) ? NULL : "|done";
		if (done_msg == NULL)
			++cbuf;
		if (!secure_allow(SF_PIPE))
			break;
		(void) pipe_mark(pipec, cbuf);
		if (done_msg != NULL)
			error(done_msg, NULL_PARG);
		break; }
#endif
	}
}

/*
 * Is a character an erase or kill char?
 */
static lbool is_erase_char(char c)
{
	return (c == erase_char || c == erase2_char || c == kill_char);
}

/*
 * Is a character a carriage return or newline?
 */
static lbool is_newline_char(char c)
{
	return (c == '\n' || c == '\r');
}

/*
 * Handle the first char of an option (after the initial dash).
 */
static int mca_opt_first_char(char c)
{
	int no_prompt = (optflag & OPT_NO_PROMPT);
	int flag = (optflag & ~OPT_NO_PROMPT);
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
			optflag = no_prompt | ((flag == OPT_UNSET) ?
				OPT_TOGGLE : OPT_UNSET);
			mca_opt_toggle();
			return (MCA_MORE);
		case '!':
			/* "-!" = SET */
			optflag = no_prompt | ((flag == OPT_SET) ?
				OPT_TOGGLE : OPT_SET);
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
	/* Char was not handled here. */
	return (NO_MCA);
}

/*
 * Add a char to a long option name.
 * See if we've got a match for an option name yet.
 * If so, display the complete name and stop 
 * accepting chars until user hits RETURN.
 */
static int mca_opt_nonfirst_char(char c)
{
	constant char *p;
	constant char *oname;
	lbool ambig;
	struct loption *was_curropt;

	if (curropt != NULL)
	{
		/* Already have a match for the name. */
		if (is_erase_char(c))
			return (MCA_DONE);
		/* {{ Checking for TAB here is ugly.
		 *    Also doesn't extend well -- can't do BACKTAB this way
		 *    because it's a multichar sequence. }} */
		if (c != '\t') 
			return (MCA_MORE);
	}
	/*
	 * Add char to cmd buffer and try to match
	 * the option name.
	 */
	if (cmd_char(c) == CC_QUIT)
		return (MCA_DONE);
	p = get_cmdbuf();
	if (p == NULL || p[0] == '\0')
		return (MCA_MORE);
	opt_lower = ASCII_IS_LOWER(p[0]);
	was_curropt = curropt;
	curropt = findopt_name(&p, &oname, &ambig);
	if (curropt != NULL)
	{
		if (was_curropt == NULL)
		{
			/*
			 * Got a match.
			 * Remember the option and
			 * display the full option name.
			 */
			cmd_reset();
			mca_opt_toggle();
			cmd_setstring(oname, !opt_lower);
		}
	} else if (!ambig)
	{
		bell();
	}
	return (MCA_MORE);
}

/*
 * Handle a char of an option toggle command.
 */
static int mca_opt_char(char c)
{
	PARG parg;

	/*
	 * This may be a short option (single char),
	 * or one char of a long option name,
	 * or one char of the option parameter.
	 */
	if (curropt == NULL && cmdbuf_empty())
	{
		int ret = mca_opt_first_char(c);
		if (ret != NO_MCA)
			return (ret);
	}
	if (optgetname)
	{
		/* We're getting a long option name.  */
		if (!is_newline_char(c) && c != '=')
			return (mca_opt_nonfirst_char(c));
		if (curropt == NULL)
		{
			parg.p_string = get_cmdbuf();
			if (parg.p_string == NULL)
				return (MCA_MORE);
			error("There is no --%s option", &parg);
			return (MCA_DONE);
		}
		optgetname = FALSE;
		cmd_reset();
	} else
	{
		if (is_erase_char(c))
			return (NO_MCA);
		if (curropt != NULL)
			/* We're getting the option parameter. */
			return (NO_MCA);
		curropt = findopt(c);
		if (curropt == NULL)
		{
			parg.p_string = propt(c);
			error("There is no %s option", &parg);
			return (MCA_DONE);
		}
		opt_lower = ASCII_IS_LOWER(c);
	}
	/*
	 * If the option which was entered does not take a 
	 * parameter, toggle the option immediately,
	 * so user doesn't have to hit RETURN.
	 */
	if ((optflag & ~OPT_NO_PROMPT) != OPT_TOGGLE ||
	    !opt_has_param(curropt))
	{
		toggle_option(curropt, opt_lower, "", optflag);
		return (MCA_DONE);
	}
	/*
	 * Display a prompt appropriate for the option parameter.
	 */
	start_mca(A_OPT_TOGGLE, opt_prompt(curropt), NULL, CF_OPTION);
	return (MCA_MORE);
}

/*
 * Normalize search type.
 */
public int norm_search_type(int st)
{
	/* WRAP and PAST_EOF are mutually exclusive. */
	if ((st & (SRCH_PAST_EOF|SRCH_WRAP)) == (SRCH_PAST_EOF|SRCH_WRAP))
		st ^= SRCH_PAST_EOF;
	return st;
}

/*
 * Handle a char of a search command.
 */
static int mca_search_char(char c)
{
	int flag = 0;

	/*
	 * Certain characters as the first char of 
	 * the pattern have special meaning:
	 *      !  Toggle the NO_MATCH flag
	 *      *  Toggle the PAST_EOF flag
	 *      @  Toggle the FIRST_FILE flag
	 */
	if (!cmdbuf_empty() || literal_char)
	{
		literal_char = FALSE;
		return (NO_MCA);
	}

	switch (c)
	{
	case '*':
		if (less_is_more)
			break;
	case CONTROL('E'): /* ignore END of file */
		if (mca != A_FILTER)
			flag = SRCH_PAST_EOF;
		search_type &= ~SRCH_WRAP;
		break;
	case '@':
		if (less_is_more)
			break;
	case CONTROL('F'): /* FIRST file */
		if (mca != A_FILTER)
			flag = SRCH_FIRST_FILE;
		break;
	case CONTROL('K'): /* KEEP position */
		if (mca != A_FILTER)
			flag = SRCH_NO_MOVE;
		break;
	case CONTROL('S'): { /* SUBSEARCH */
		char buf[INT_STRLEN_BOUND(int)+24];
		SNPRINTF1(buf, sizeof(buf), "Sub-pattern (1-%d):", NUM_SEARCH_COLORS);
		clear_bot();
		cmd_putstr(buf);
		flush();
		c = getcc();
		if (c >= '1' && c <= '0'+NUM_SEARCH_COLORS)
			flag = SRCH_SUBSEARCH(c-'0');
		else
			flag = -1; /* calls mca_search() below to repaint */
		break; }
	case CONTROL('W'): /* WRAP around */
		if (mca != A_FILTER)
			flag = SRCH_WRAP;
		break;
	case CONTROL('R'): /* Don't use REGULAR EXPRESSIONS */
		flag = SRCH_NO_REGEX;
		break;
	case CONTROL('N'): /* NOT match */
	case '!':
		flag = SRCH_NO_MATCH;
		break;
	case CONTROL('L'):
		literal_char = TRUE;
		flag = -1;
		break;
	}

	if (flag != 0)
	{
		if (flag != -1)
			search_type = norm_search_type(search_type ^ flag);
		mca_search();
		return (MCA_MORE);
	}
	return (NO_MCA);
}

/*
 * Handle a character of a multi-character command.
 */
static int mca_char(char c)
{
	int ret;

	switch (mca)
	{
	case 0:
		/*
		 * We're not in a multicharacter command.
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
		if ((c >= '0' && c <= '9') || c == '.')
			break;
		switch (editchar(c, ECF_PEEK|ECF_NOHISTORY|ECF_NOCOMPLETE|ECF_NORIGHTLEFT))
		{
		case A_NOACTION:
			/*
			 * Ignore this char and get another one.
			 */
			return (MCA_MORE);
		case A_INVALID:
			/*
			 * Not part of the number.
			 * End the number and treat this char 
			 * as a normal command character.
			 */
			number = cmd_int(&fraction);
			clear_mca();
			cmd_accept();
			return (NO_MCA);
		}
		break;

	case A_OPT_TOGGLE:
		ret = mca_opt_char(c);
		if (ret != NO_MCA)
			return (ret);
		break;

	case A_F_SEARCH:
	case A_B_SEARCH:
	case A_FILTER:
		ret = mca_search_char(c);
		if (ret != NO_MCA)
			return (ret);
		break;

	default:
		/* Other multicharacter command. */
		break;
	}

	/*
	 * The multichar command is terminated by a newline.
	 */
	if (is_newline_char(c))
	{
		if (pasting && no_paste)
		{
			/* Ignore pasted input after (and including) the first newline */
			start_ignoring_input();
			return (MCA_MORE);
		}
		/* Execute the command. */
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

	switch (mca)
	{
	case A_F_BRACKET:
	case A_B_BRACKET:
		if (len_cmdbuf() >= 2)
		{
			/*
			 * Special case for the bracket-matching commands.
			 * Execute the command after getting exactly two
			 * characters from the user.
			 */
			exec_mca();
			return (MCA_DONE);
		}
		break;
	case A_F_SEARCH:
	case A_B_SEARCH:
		if (incr_search)
		{
			/* Incremental search: do a search after every input char. */
			int st = (search_type & (SRCH_FORW|SRCH_BACK|SRCH_NO_MATCH|SRCH_NO_REGEX|SRCH_NO_MOVE|SRCH_WRAP|SRCH_SUBSEARCH_ALL));
			ssize_t save_updown;
			constant char *pattern = get_cmdbuf();
			if (pattern == NULL)
				return (MCA_MORE);
			/*
			 * Must save updown_match because mca_search
			 * reinits it. That breaks history scrolling.
			 * {{ This is ugly. mca_search probably shouldn't call set_mlist. }}
			 */
			save_updown = save_updown_match();
			cmd_exec();
			if (*pattern == '\0')
			{
				/* User has backspaced to an empty pattern. */
				undo_search(1);
			} else
			{
				if (search(st | SRCH_INCR, pattern, 1) != 0)
					/* No match, invalid pattern, etc. */
					undo_search(1);
			}
			/* Redraw the search prompt and search string. */
			if (is_screen_trashed() || !full_screen)
			{
				clear();
				repaint();
			}
			mca_search1();
			restore_updown_match(save_updown);
			cmd_repaint(NULL);
		}
		break;
	}

	/*
	 * Need another character.
	 */
	return (MCA_MORE);
}

/*
 * Discard any buffered file data.
 */
static void clear_buffers(void)
{
	if (!(ch_getflags() & CH_CANSEEK))
		return;
	ch_flush();
	clr_linenum();
#if HILITE_SEARCH
	clr_hilite();
#endif
}

public void screen_trashed_num(int trashed)
{
	screen_trashed_value = trashed;
}

public void screen_trashed(void)
{
	screen_trashed_num(1);
}

public int is_screen_trashed(void)
{
	return screen_trashed_value;
}

/*
 * Make sure the screen is displayed.
 */
static void make_display(void)
{
	/*
	 * If not full_screen, we can't rely on scrolling to fill the screen.
	 * We need to clear and repaint screen before any change.
	 */
	if (!full_screen && !(quit_if_one_screen && one_screen))
		clear();
	/*
	 * If nothing is displayed yet, display starting from initial_scrpos.
	 */
	if (empty_screen())
	{
		if (initial_scrpos.pos == NULL_POSITION)
			jump_loc(ch_zero(), 1);
		else
			jump_loc(initial_scrpos.pos, initial_scrpos.ln);
	} else if (is_screen_trashed() || !full_screen)
	{
		int save_top_scroll = top_scroll;
		int save_ignore_eoi = ignore_eoi;
		top_scroll = 1;
		ignore_eoi = 0;
		if (is_screen_trashed() == 2)
		{
			/* Special case used by ignore_eoi: re-open the input file
			 * and jump to the end of the file. */
			reopen_curr_ifile();
			jump_forw();
		}
		repaint();
		top_scroll = save_top_scroll;
		ignore_eoi = save_ignore_eoi;
	}
}

/*
 * Display the appropriate prompt.
 */
static void prompt(void)
{
	constant char *p;

	if (ungot != NULL && !ungot->ug_end_command)
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
	 * If we've hit EOF on the last file and the -E flag is set, quit.
	 */
	if (get_quit_at_eof() == OPT_ONPLUS &&
	    eof_displayed(FALSE) && !(ch_getflags() & CH_HELPFILE) && 
	    next_ifile(curr_ifile) == NULL_IFILE)
		quit(QUIT_OK);

	/*
	 * If the entire file is displayed and the -F flag is set, quit.
	 */
	if (quit_if_one_screen &&
	    entire_file_displayed() && !(ch_getflags() & CH_HELPFILE) && 
	    next_ifile(curr_ifile) == NULL_IFILE)
		quit(QUIT_OK);
	quit_if_one_screen = FALSE; /* only get one chance at this */

#if MSDOS_COMPILER==WIN32C
	/* 
	 * In Win32, display the file name in the window title.
	 */
	if (!(ch_getflags() & CH_HELPFILE))
	{
		WCHAR w[MAX_PATH+16];
		p = pr_expand("Less?f - %f.");
		MultiByteToWideChar(less_acp, 0, p, -1, w, countof(w));
		SetConsoleTitleW(w);
	}
#endif

	/*
	 * Select the proper prompt and display it.
	 */
	/*
	 * If the previous action was a forward movement, 
	 * don't clear the bottom line of the display;
	 * just print the prompt since the forward movement guarantees 
	 * that we're in the right position to display the prompt.
	 * Clearing the line could cause a problem: for example, if the last
	 * line displayed ended at the right screen edge without a newline,
	 * then clearing would clear the last displayed line rather than
	 * the prompt line.
	 */
	if (!forw_prompt)
		clear_bot();
	clear_cmd();
	forw_prompt = 0;
	p = pr_string();
#if HILITE_SEARCH
	if (is_filtering())
		putstr("& ");
#endif
	if (search_wrapped)
	{
		if (search_type & SRCH_BACK)
			error("Search hit top; continuing at bottom", NULL_PARG);
		else
			error("Search hit bottom; continuing at top", NULL_PARG);
		search_wrapped = FALSE;
	}
#if OSC8_LINK
	if (osc8_uri != NULL)
	{
		PARG parg;
		parg.p_string = osc8_uri;
		error("Link: %s", &parg);
		free(osc8_uri);
		osc8_uri = NULL;
	}
#endif
	if (p == NULL || *p == '\0')
	{
		at_enter(AT_NORMAL|AT_COLOR_PROMPT);
		putchr(':');
		at_exit();
	} else
	{
#if MSDOS_COMPILER==WIN32C
		WCHAR w[MAX_PATH*2];
		char  a[MAX_PATH*2];
		MultiByteToWideChar(less_acp, 0, p, -1, w, countof(w));
		WideCharToMultiByte(utf_mode ? CP_UTF8 : GetConsoleOutputCP(),
		                    0, w, -1, a, sizeof(a), NULL, NULL);
		p = a;
#endif
		load_line(p);
		put_line(FALSE);
	}
	clear_eol();
}

/*
 * Display the less version message.
 */
public void dispversion(void)
{
	PARG parg;

	parg.p_string = version;
	error("less %s", &parg);
}

/*
 * Return a character to complete a partial command, if possible.
 */
static char getcc_end_command(void)
{
	int ch;
	switch (mca)
	{
	case A_DIGIT:
		/* We have a number but no command.  Treat as #g. */
		return ('g');
	case A_F_SEARCH:
	case A_B_SEARCH:
	case A_FILTER:
		/* We have "/string" but no newline.  Add the \n. */
		return ('\n'); 
	default:
		/* Some other incomplete command.  Let user complete it. */
		if (ungot != NULL)
			return ('\0');
		ch = getchr();
		if (ch < 0) ch = '\0';
		return (char) ch;
	}
}

/*
 * Get a command character from the ungotten stack.
 */
static char get_ungot(lbool *p_end_command)
{
	struct ungot *ug = ungot;
	char c = ug->ug_char;
	if (p_end_command != NULL)
		*p_end_command = ug->ug_end_command;
	ungot = ug->ug_next;
	free(ug);
	return c;
}

/*
 * Delete all ungotten characters.
 */
public void getcc_clear(void)
{
	while (ungot != NULL)
		(void) get_ungot(NULL);
}

/*
 * Get command character.
 * The character normally comes from the keyboard,
 * but may come from ungotten characters
 * (characters previously given to ungetcc or ungetsc).
 */
static char getccu(void)
{
	int c = 0;
	while (c == 0 && sigs == 0)
	{
		if (ungot == NULL)
		{
			/* Normal case: no ungotten chars.
			 * Get char from the user. */
			c = getchr();
			if (c < 0) c = '\0';
		} else
		{
			/* Ungotten chars available:
			 * Take the top of stack (most recent). */
			lbool end_command;
			c = get_ungot(&end_command);
			if (end_command)
				c = getcc_end_command();
		}
	}
	return ((char) c);
}

/*
 * Get a command character, but if we receive the orig sequence,
 * convert it to the repl sequence.
 */
static char getcc_repl(char constant *orig, char constant *repl, char (*gr_getc)(void), void (*gr_ungetc)(char))
{
	char c;
	char keys[16];
	size_t ki = 0;

	c = (*gr_getc)();
	if (orig == NULL || orig[0] == '\0')
		return c;
	for (;;)
	{
		keys[ki] = c;
		if (c != orig[ki] || ki >= sizeof(keys)-1)
		{
			/* This is not orig we have been receiving.
			 * If we have stashed chars in keys[],
			 * unget them and return the first one. */
			while (ki > 0)
				(*gr_ungetc)(keys[ki--]);
			return keys[0];
		}
		if (orig[++ki] == '\0')
		{
			/* We've received the full orig sequence.
			 * Return the repl sequence. */
			ki = strlen(repl)-1;
			while (ki > 0)
				(*gr_ungetc)(repl[ki--]);
			return repl[0];
		}
		/* We've received a partial orig sequence (ki chars of it).
		 * Get next char and see if it continues to match orig. */
		c = (*gr_getc)();
	}
}

/*
 * Get command character.
 */
public char getcc(void)
{
	/* Replace kent (keypad Enter) with a newline. */
	return getcc_repl(kent, "\n", getccu, ungetcc);
}

/*
 * "Unget" a command character.
 * The next getcc() will return this character.
 */
public void ungetcc(char c)
{
	struct ungot *ug = (struct ungot *) ecalloc(1, sizeof(struct ungot));

	ug->ug_char = c;
	ug->ug_next = ungot;
	ungot = ug;
}

/*
 * "Unget" a command character.
 * If any other chars are already ungotten, put this one after those.
 */
static void ungetcc_back1(char c, lbool end_command)
{
	struct ungot *ug = (struct ungot *) ecalloc(1, sizeof(struct ungot));
	ug->ug_char = c;
	ug->ug_end_command = end_command;
	ug->ug_next = NULL;
	if (ungot == NULL)
		ungot = ug;
	else
	{
		struct ungot *pu;
		for (pu = ungot; pu->ug_next != NULL; pu = pu->ug_next)
			continue;
		pu->ug_next = ug;
	}
}

public void ungetcc_back(char c)
{
	ungetcc_back1(c, FALSE);
}

public void ungetcc_end_command(void)
{
	ungetcc_back1('\0', TRUE);
}

/*
 * Unget a whole string of command characters.
 * The next sequence of getcc()'s will return this string.
 */
public void ungetsc(constant char *s)
{
	while (*s != '\0')
		ungetcc_back(*s++);
}

/*
 * Peek the next command character, without consuming it.
 */
public char peekcc(void)
{
	char c = getcc();
	ungetcc(c);
	return c;
}

/*
 * Search for a pattern, possibly in multiple files.
 * If SRCH_FIRST_FILE is set, begin searching at the first file.
 * If SRCH_PAST_EOF is set, continue the search thru multiple files.
 */
static void multi_search(constant char *pattern, int n, int silent)
{
	int nomore;
	IFILE save_ifile;
	lbool changed_file;

	changed_file = FALSE;
	save_ifile = save_curr_ifile();

	if ((search_type & (SRCH_FORW|SRCH_BACK)) == 0)
		search_type |= SRCH_FORW;
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
		changed_file = TRUE;
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
		last_search_type = search_type;
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
		changed_file = TRUE;
	}

	/*
	 * Didn't find it.
	 * Print an error message if we haven't already.
	 */
	if (n > 0 && !silent)
		error("Pattern not found", NULL_PARG);

	if (changed_file)
	{
		/*
		 * Restore the file we were originally viewing.
		 */
		reedit_ifile(save_ifile);
	} else
	{
		unsave_ifile(save_ifile);
	}
}

/*
 * Forward forever, or until a highlighted line appears.
 */
static int forw_loop(int until_hilite)
{
	POSITION curr_len;

	if (ch_getflags() & CH_HELPFILE)
		return (A_NOACTION);

	cmd_exec();
	jump_forw_buffered();
	curr_len = ch_length();
	highest_hilite = until_hilite ? curr_len : NULL_POSITION;
	ignore_eoi = 1;
	while (!sigs)
	{
		if (until_hilite && highest_hilite > curr_len)
		{
			bell();
			break;
		}
		make_display();
		forward(1, FALSE, FALSE, FALSE);
	}
	ignore_eoi = 0;
	ch_set_eof();

	/*
	 * This gets us back in "F mode" after processing 
	 * a non-abort signal (e.g. window-change).  
	 */
	if (sigs && !ABORT_SIGS())
		return (until_hilite ? A_F_UNTIL_HILITE : A_F_FOREVER);

	return (A_NOACTION);
}

/*
 * Ignore subsequent (pasted) input chars.
 */
public void start_ignoring_input()
{
	ignoring_input = TRUE;
#if HAVE_TIME
	ignoring_input_time = get_time();
#endif
}

/*
 * Stop ignoring input chars.
 */
public void stop_ignoring_input()
{
	ignoring_input = FALSE;
	pasting = FALSE;
}

/*
 * Are we ignoring input chars?
 */
public lbool is_ignoring_input(int action)
{
	if (!ignoring_input)
		return FALSE;
	if (action == A_END_PASTE)
		stop_ignoring_input();
#if HAVE_TIME
	if (get_time() >= ignoring_input_time + MAX_PASTE_IGNORE_SEC)
		stop_ignoring_input();
#endif
	/*
	 * Don't ignore prefix chars so we can parse a full command
	 * (which might be A_END_PASTE).
	 */
	return (action != A_PREFIX);
}

/*
 * Main command processor.
 * Accept and execute commands until a quit command.
 */
public void commands(void)
{
	char c;
	int action;
	constant char *cbuf;
	constant char *msg;
	int newaction;
	int save_jump_sline;
	int save_search_type;
	constant char *extra;
	PARG parg;
	IFILE old_ifile;
	IFILE new_ifile;
#if TAGS
	constant char *tagfile;
#endif

	search_type = SRCH_FORW;
	wscroll = (sc_height + 1) / 2;
	newaction = A_NOACTION;

	for (;;)
	{
		clear_mca();
		cmd_accept();
		number = 0;
		curropt = NULL;

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
			extra = NULL;
			if (mca)
			{
				/*
				 * We're in a multichar command.
				 * Add the character to the command buffer
				 * and display it on the screen.
				 * If the user backspaces past the start 
				 * of the line, abort the command.
				 */
				if (cmd_char(c) == CC_QUIT || cmdbuf_empty())
					continue;
				cbuf = get_cmdbuf();
				if (cbuf == NULL)
				{
					c = getcc();
					goto again;
				}
				action = fcmd_decode(cbuf, &extra);
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
				constant char tbuf[2] = { c, '\0' };
				action = fcmd_decode(tbuf, &extra);
			}
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

		if (is_ignoring_input(action))
			continue;

		switch (action)
		{
		case A_START_PASTE:
			if (no_paste)
				start_ignoring_input();
			break;

		case A_DIGIT:
			/*
			 * First digit of a number.
			 */
			start_mca(A_DIGIT, ":", NULL, CF_QUIT_ON_ERASE);
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
			forward((int) number, FALSE, TRUE, FALSE);
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
			backward((int) number, FALSE, TRUE, FALSE);
			break;

		case A_F_LINE:
		case A_F_NEWLINE:

			/*
			 * Forward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			if (show_attn == OPT_ONPLUS && number > 1)
				set_attnpos(bottompos);
			forward((int) number, FALSE, FALSE, action == A_F_NEWLINE && !chopline);
			break;

		case A_B_LINE:
		case A_B_NEWLINE:
			/*
			 * Backward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			backward((int) number, FALSE, FALSE, action == A_B_NEWLINE && !chopline);
			break;

		case A_F_MOUSE:
			/*
			 * Forward wheel_lines lines.
			 */
			cmd_exec();
			forward(wheel_lines, FALSE, FALSE, FALSE);
			break;

		case A_B_MOUSE:
			/*
			 * Backward wheel_lines lines.
			 */
			cmd_exec();
			backward(wheel_lines, FALSE, FALSE, FALSE);
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
			forward((int) number, TRUE, FALSE, FALSE);
			break;

		case A_BF_LINE:
			/*
			 * Force backward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			backward((int) number, TRUE, FALSE, FALSE);
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
			forward((int) number, TRUE, FALSE, FALSE);
			break;

		case A_BF_SCREEN:
			/*
			 * Force backward one screen.
			 */
			if (number <= 0)
				number = get_swindow();
			cmd_exec();
			backward((int) number, TRUE, FALSE, FALSE);
			break;

		case A_F_FOREVER:
			/*
			 * Forward forever, ignoring EOF.
			 */
			if (get_altfilename(curr_ifile) != NULL)
				error("Warning: command may not work correctly when file is viewed via LESSOPEN", NULL_PARG);
			if (show_attn)
				set_attnpos(bottompos);
			newaction = forw_loop(0);
			break;

		case A_F_UNTIL_HILITE:
			newaction = forw_loop(1);
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
			forward(wscroll, FALSE, FALSE, FALSE);
			break;

		case A_B_SCROLL:
			/*
			 * Forward N lines 
			 * (default same as last 'd' or 'u' command).
			 */
			if (number > 0)
				wscroll = (int) number;
			cmd_exec();
			backward(wscroll, FALSE, FALSE, FALSE);
			break;

		case A_FREPAINT:
			/*
			 * Flush buffers, then repaint screen.
			 * Don't flush the buffers on a pipe!
			 */
			clear_buffers();
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
			 * If N <= 0, ignore jump_sline in order to avoid
			 * empty lines before the beginning of the file.
			 */
			save_jump_sline = jump_sline;
			if (number <= 0)
			{
				number = 1;
				jump_sline = 0;
			}
			cmd_exec();
			jump_back(number);
			jump_sline = save_jump_sline;
			break;

		case A_PERCENT:
			/*
			 * Go to a specified percentage into the file.
			 */
			if (number < 0)
			{
				number = 0;
				fraction = 0;
			}
			if (number > 100 || (number == 100 && fraction != 0))
			{
				number = 100;
				fraction = 0;
			}
			cmd_exec();
			jump_percent((int) number, fraction);
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

		case A_GOEND_BUF:
			/*
			 * Go to line N, default last buffered byte.
			 */
			cmd_exec();
			if (number <= 0)
				jump_forw_buffered();
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
			 * Print version number.
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
				hshift = save_hshift;
				bs_mode = save_bs_mode;
				proc_backspace = save_proc_backspace;
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
#define DO_SEARCH() \
			if (number <= 0) number = 1;    \
			mca_search();                   \
			cmd_exec();                     \
			multi_search(NULL, (int) number, 0);

		case A_F_SEARCH:
			/*
			 * Search forward for a pattern.
			 * Get the first char of the pattern.
			 */
			search_type = SRCH_FORW | def_search_type;
			if (number <= 0)
				number = 1;
			literal_char = FALSE;
			mca_search();
			c = getcc();
			goto again;

		case A_B_SEARCH:
			/*
			 * Search backward for a pattern.
			 * Get the first char of the pattern.
			 */
			search_type = SRCH_BACK | def_search_type;
			if (number <= 0)
				number = 1;
			literal_char = FALSE;
			mca_search();
			c = getcc();
			goto again;

		case A_OSC8_F_SEARCH:
#if OSC8_LINK
			cmd_exec();
			if (number <= 0)
				number = 1;
			osc8_search(SRCH_FORW, NULL, number);
#else
			error("Command not available", NULL_PARG);
#endif
			break;

		case A_OSC8_B_SEARCH:
#if OSC8_LINK
			cmd_exec();
			if (number <= 0)
				number = 1;
			osc8_search(SRCH_BACK, NULL, number);
#else
			error("Command not available", NULL_PARG);
#endif
			break;

		case A_OSC8_OPEN:
#if OSC8_LINK
			if (secure_allow(SF_OSC8_OPEN))
			{
				cmd_exec();
				osc8_open();
				break;
			}
#endif
			error("Command not available", NULL_PARG);
			break;

		case A_OSC8_JUMP:
#if OSC8_LINK
			cmd_exec();
			osc8_jump();
#else
			error("Command not available", NULL_PARG);
#endif
			break;

		case A_FILTER:
#if HILITE_SEARCH
			search_type = SRCH_FORW | SRCH_FILTER;
			literal_char = FALSE;
			mca_search();
			c = getcc();
			goto again;
#else
			error("Command not available", NULL_PARG);
			break;
#endif

		case A_AGAIN_SEARCH:
			/*
			 * Repeat previous search.
			 */
			search_type = last_search_type;
			DO_SEARCH();
			break;
		
		case A_T_AGAIN_SEARCH:
			/*
			 * Repeat previous search, multiple files.
			 */
			search_type = last_search_type | SRCH_PAST_EOF;
			DO_SEARCH();
			break;

		case A_REVERSE_SEARCH:
			/*
			 * Repeat previous search, in reverse direction.
			 */
			save_search_type = search_type = last_search_type;
			search_type = SRCH_REVERSE(search_type);
			DO_SEARCH();
			last_search_type = save_search_type;
			break;

		case A_T_REVERSE_SEARCH:
			/* 
			 * Repeat previous search, 
			 * multiple files in reverse direction.
			 */
			save_search_type = search_type = last_search_type;
			search_type = SRCH_REVERSE(search_type) | SRCH_PAST_EOF;
			DO_SEARCH();
			last_search_type = save_search_type;
			break;

		case A_UNDO_SEARCH:
		case A_CLR_SEARCH:
			/*
			 * Clear search string highlighting.
			 */
			undo_search(action == A_CLR_SEARCH);
			break;

		case A_HELP:
			/*
			 * Help.
			 */
			if (ch_getflags() & CH_HELPFILE)
				break;
			cmd_exec();
			save_hshift = hshift;
			hshift = 0;
			save_bs_mode = bs_mode;
			bs_mode = BS_SPECIAL;
			save_proc_backspace = proc_backspace;
			proc_backspace = OPT_OFF;
			(void) edit(FAKE_HELPFILE);
			break;

		case A_EXAMINE:
			/*
			 * Edit a new file.  Get the filename.
			 */
#if EXAMINE
			if (secure_allow(SF_EXAMINE))
			{
				start_mca(A_EXAMINE, "Examine: ", ml_examine, 0);
				c = getcc();
				goto again;
			}
#endif
			error("Command not available", NULL_PARG);
			break;
			
		case A_VISUAL:
			/*
			 * Invoke an editor on the input file.
			 */
#if EDITOR
			if (secure_allow(SF_EDIT))
			{
				if (ch_getflags() & CH_HELPFILE)
					break;
				if (strcmp(get_filename(curr_ifile), "-") == 0)
				{
					error("Cannot edit standard input", NULL_PARG);
					break;
				}
				if (!no_edit_warn && get_altfilename(curr_ifile) != NULL)
				{
					error("WARNING: This file was viewed via LESSOPEN", NULL_PARG);
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
				lsystem(pr_expand(editproto), NULL);
				break;
			}
#endif
			error("Command not available", NULL_PARG);
			break;

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
			cmd_exec();
			if (edit_next((int) number))
			{
				if (get_quit_at_eof() && eof_displayed(FALSE) && 
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
			cmd_exec();
			if (edit_prev((int) number))
			{
				parg.p_string = (number > 1) ? "(N-th) " : "";
				error("No %sprevious file", &parg);
			}
			break;

		case A_NEXT_TAG:
			/*
			 * Jump to the next tag in the current tag list.
			 */
#if TAGS
			if (number <= 0)
				number = 1;
			tagfile = nexttag((int) number);
			if (tagfile == NULL)
			{
				error("No next tag", NULL_PARG);
				break;
			}
			cmd_exec();
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
			/*
			 * Jump to the previous tag in the current tag list.
			 */
#if TAGS
			if (number <= 0)
				number = 1;
			tagfile = prevtag((int) number);
			if (tagfile == NULL)
			{
				error("No previous tag", NULL_PARG);
				break;
			}
			cmd_exec();
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
			cmd_exec();
			if (edit_index((int) number))
				error("No such file", NULL_PARG);
			break;

		case A_REMOVE_FILE:
			/*
			 * Remove a file from the input file list.
			 */
			if (ch_getflags() & CH_HELPFILE)
				break;
			old_ifile = curr_ifile;
			new_ifile = getoff_ifile(curr_ifile);
			cmd_exec();
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
			/*
			 * Change the setting of an  option.
			 */
			optflag = OPT_TOGGLE;
			optgetname = FALSE;
			mca_opt_toggle();
			c = getcc();
			msg = opt_toggle_disallowed(c);
			if (msg != NULL)
			{
				error(msg, NULL_PARG);
				break;
			}
			goto again;

		case A_DISP_OPTION:
			/*
			 * Report the setting of an option.
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
			start_mca(A_FIRSTCMD, "+", NULL, 0);
			c = getcc();
			goto again;

		case A_SHELL:
		case A_PSHELL:
			/*
			 * Shell escape.
			 */
#if SHELL_ESCAPE
			if (secure_allow(SF_SHELL))
			{
				start_mca(action, (action == A_SHELL) ? "!" : "#", ml_shell, 0);
				c = getcc();
				goto again;
			}
#endif
			error("Command not available", NULL_PARG);
			break;

		case A_SETMARK:
		case A_SETMARKBOT:
			/*
			 * Set a mark.
			 */
			if (ch_getflags() & CH_HELPFILE)
			{
				if (ungot != NULL)
				{
					/*
					 * Probably from a lesskey file, in which case there 
					 * is probably an ungotten letter from the "extra" string.
					 * Eat it so it is not interpreted as a command.
					 */
					(void) getcc();
				}
				break;
			}
			start_mca(A_SETMARK, "set mark: ", NULL, 0);
			c = getcc();
			if (is_erase_char(c) || is_newline_char(c))
				break;
			setmark(c, action == A_SETMARKBOT ? BOTTOM : TOP);
			repaint();
			break;

		case A_CLRMARK:
			/*
			 * Clear a mark.
			 */
			start_mca(A_CLRMARK, "clear mark: ", NULL, 0);
			c = getcc();
			if (is_erase_char(c) || is_newline_char(c))
				break;
			clrmark(c);
			repaint();
			break;

		case A_GOMARK:
			/*
			 * Jump to a marked position.
			 */
			start_mca(A_GOMARK, "goto mark: ", NULL, 0);
			c = getcc();
			if (is_erase_char(c) || is_newline_char(c))
				break;
			cmd_exec();
			gomark(c);
			break;

		case A_PIPE:
			/*
			 * Write part of the input to a pipe to a shell command.
			 */
#if PIPEC
			if (secure_allow(SF_PIPE))
			{
				start_mca(A_PIPE, "|mark: ", NULL, 0);
				c = getcc();
				if (is_erase_char(c))
					break;
				if (is_newline_char(c))
					c = '.';
				if (badmark(c))
					break;
				pipec = c;
				start_mca(A_PIPE, "!", ml_shell, 0);
				c = getcc();
				goto again;
			}
#endif
			error("Command not available", NULL_PARG);
			break;

		case A_B_BRACKET:
		case A_F_BRACKET:
			start_mca(action, "Brackets: ", NULL, 0);
			c = getcc();
			goto again;

		case A_LSHIFT:
			/*
			 * Shift view left.
			 */
			if (number > 0)
				shift_count = (int) number;
			else
				number = (shift_count > 0) ? shift_count : sc_width / 2;
			if (number > hshift)
				number = hshift;
			pos_rehead();
			hshift -= (int) number;
			screen_trashed();
			break;

		case A_RSHIFT:
			/*
			 * Shift view right.
			 */
			if (number > 0)
				shift_count = (int) number;
			else
				number = (shift_count > 0) ? shift_count : sc_width / 2;
			pos_rehead();
			hshift += (int) number;
			screen_trashed();
			break;

		case A_LLSHIFT:
			/*
			 * Shift view left to margin.
			 */
			pos_rehead();
			hshift = 0;
			screen_trashed();
			break;

		case A_RRSHIFT:
			/*
			 * Shift view right to view rightmost char on screen.
			 */
			pos_rehead();
			hshift = rrshift();
			screen_trashed();
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
				start_mca(A_PREFIX, " ", NULL, CF_QUIT_ON_ERASE);
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
