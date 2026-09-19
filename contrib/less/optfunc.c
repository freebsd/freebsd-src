/*
 * Copyright (C) 1984-2026  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Handling functions for command line options.
 *
 * Most options are handled by the generic code in option.c.
 * But all string options, and a few non-string options, require
 * special handling specific to the particular option.
 * This special processing is done by the "handling functions" in this file.
 *
 * Each handling function is passed a "type" and, if it is a string
 * option, the string which should be "assigned" to the option.
 * The type may be one of:
 *      INIT    The option is being initialized from the command line.
 *      TOGGLE  The option is being changed from within the program.
 *      QUERY   The setting of the option is merely being queried.
 */

#include "less.h"
#include "option.h"
#include "position.h"

extern int bufspace;
extern int pr_type;
extern lbool plusoption;
extern int swindow;
extern int sc_width;
extern int sc_height;
extern lbool dohelp;
extern char openquote;
extern char closequote;
extern char *prproto[];
extern char *eprproto[];
extern char *eqproto;
extern char *hproto;
extern char *wproto;
extern char *every_first_cmd;
extern IFILE curr_ifile;
extern char version[];
extern int jump_sline_arg;
extern int jump_sline;
extern long jump_sline_fraction;
extern int shift_count;
extern long shift_count_fraction;
extern int match_shift;
extern long match_shift_fraction;
extern LWCHAR rscroll_char;
extern int rscroll_attr;
extern int emouse;
extern int mouse_reverse;
extern int xmouse;
extern int wheel_lines;
extern lbool less_is_more;
extern int linenum_width;
extern int status_col_width;
extern int use_color;
extern int want_filesize;
extern int header_lines;
extern int header_cols;
extern int def_search_type;
extern int chopline;
extern int tabstops[];
extern int ntabstops;
extern int tabdefault;
extern int no_paste;
extern int hilite_target;
extern char intr_char;
extern int utf_mode;
extern int nosearch_header_lines;
extern int nosearch_header_cols;
extern POSITION header_start_pos;
extern char *init_header;
extern char *first_cmd_at_prompt;
extern char *autosave;
#if LOGFILE
extern char *namelogfile;
extern lbool force_logfile;
extern int logfile;
#endif
#if TAGS
public char *tagoption = NULL;
extern char *tags;
extern char ztags[];
#endif
#if LESSTEST
extern constant char *ttyin_name;
extern lbool is_tty;
#endif /*LESSTEST*/
#if MSDOS_COMPILER
extern int nm_fg_color, nm_bg_color, nm_attr;
extern int bo_fg_color, bo_bg_color, bo_attr;
extern int ul_fg_color, ul_bg_color, ul_attr;
extern int so_fg_color, so_bg_color, so_attr;
extern int bl_fg_color, bl_bg_color, bl_attr;
extern int sgr_mode;
#if MSDOS_COMPILER==WIN32C
#ifndef COMMON_LVB_UNDERSCORE
#define COMMON_LVB_UNDERSCORE 0x8000
#endif
#ifndef COMMON_LVB_REVERSE_VIDEO
#define COMMON_LVB_REVERSE_VIDEO 0x4000
#endif
#endif
#endif


#if LOGFILE
/*
 * Handler for -o option.
 */
public void opt_o(int type, constant char *s)
{
	PARG parg;
	char *filename;

	if (!secure_allow(SF_LOGFILE))
	{
		error(LM(log_file_support_is_not_available), NULL_PARG);
		return;
	}
	switch (type)
	{
	case INIT:
		namelogfile = save(s);
		break;
	case TOGGLE:
		if (ch_getflags() & CH_CANSEEK)
		{
			error(LM(Input_is_not_a_pipe), NULL_PARG);
			return;
		}
		if (logfile >= 0)
		{
			error(LM(Log_file_is_already_in_use), NULL_PARG);
			return;
		}
		s = skipspc(s);
		if (namelogfile != NULL)
			free(namelogfile);
		filename = lglob(s);
		namelogfile = shell_unquote(filename);
		free(filename);
		use_logfile(namelogfile);
		sync_logfile();
		break;
	case QUERY:
		if (logfile < 0)
			error(LM(No_log_file), NULL_PARG);
		else
		{
			parg.p_string = namelogfile;
			error(LM(Log_file_X), &parg);
		}
		break;
	}
}

/*
 * Handler for -O option.
 */
public void opt__O(int type, constant char *s)
{
	force_logfile = TRUE;
	opt_o(type, s);
}
#endif

static void toggle_fraction(int *num, long *frac, constant char *s, constant char *printopt, lbool neg_ok, void (*calc)(void))
{
	if (s == NULL)
	{
	} else if (*s == '.')
	{
		s++;
		if (!getfraction(&s, frac))
		{
			PARG parg;
			parg.p_string = printopt;
			error(LM(Invalid_fraction_in_X), &parg);
			return;
		}
	} else
	{
		if (!getnumc(&s, printopt, neg_ok, num))
			return;
		*frac = -1;
	}
	(*calc)();
}

static void query_fraction(int value, long fraction, constant char *int_msg, constant char *frac_msg)
{
	PARG parg;

	if (fraction < 0)
	{
		parg.p_int = value;
		error(int_msg, &parg);
	} else
	{
		char buf[INT_STRLEN_BOUND(long)+2];
		size_t len;
		SNPRINTF1(buf, sizeof(buf), ".%06ld", fraction);
		len = strlen(buf);
		while (len > 2 && buf[len-1] == '0')
			len--;
		buf[len] = '\0';
		parg.p_string = buf;
		error(frac_msg, &parg);
	}
}

/*
 * Handlers for -j option.
 */
public void opt_j(int type, constant char *s)
{
	switch (type)
	{
	case INIT:
	case TOGGLE:
		toggle_fraction(&jump_sline_arg, &jump_sline_fraction,
			s, "-j", TRUE, calc_jump_sline);
		break;
	case QUERY:
		query_fraction(jump_sline_arg, jump_sline_fraction,
			LM(Position_target_at_screen_line_X), LM(Position_target_at_screen_position_X));
		break;
	}
}

public void calc_jump_sline(void)
{
	jump_sline = jump_sline_arg;
	/* If jump_sline_fraction is set, calculate jump_sline from it. */
	if (jump_sline_fraction >= 0)
		jump_sline = (int) muldiv(sc_height, jump_sline_fraction, NUM_FRAC_DENOM);
	/* Negative jump_sline means relative to bottom of screen. */
	if (jump_sline < 0)
		jump_sline += sc_height;
	/* If jump_sline is obscured by header, move it after the header. */
	if (jump_sline <= header_lines)
		jump_sline = header_lines + 1;
	/* If jump_sline is past bottom of screen, move it to the bottom. */
	if (jump_sline >= sc_height)
		jump_sline = sc_height - 1;
}

/*
 * Handlers for -# option.
 */
public void opt_shift(int type, constant char *s)
{
	switch (type)
	{
	case INIT:
	case TOGGLE:
		toggle_fraction(&shift_count, &shift_count_fraction,
			s, "-#", FALSE, calc_shift_count);
		break;
	case QUERY:
		query_fraction(shift_count, shift_count_fraction,
			LM(Horizontal_shift_X_columns), LM(Horizontal_shift_X_of_screen_width));
		break;
	}
}

public void calc_shift_count(void)
{
	if (shift_count_fraction < 0)
		return;
	shift_count = (int) muldiv(sc_width, shift_count_fraction, NUM_FRAC_DENOM);
}

#if USERFILE
public void opt_k(int type, constant char *s)
{
	PARG parg;

	switch (type)
	{
	case INIT:
		if (lesskey(s, FALSE))
		{
			parg.p_string = s;
			error(LM(Cannot_use_lesskey_file_X), &parg);
		}
		break;
	}
}

#if HAVE_LESSKEYSRC 
public void opt_ks(int type, constant char *s)
{
	PARG parg;

	switch (type)
	{
	case INIT:
		if (lesskey_src(s, FALSE))
		{
			parg.p_string = s;
			error(LM(Cannot_use_lesskey_source_file_X), &parg);
		}
		break;
	}
}

public void opt_kc(int type, constant char *s)
{
	switch (type)
	{
	case INIT:
		if (lesskey_content(s, FALSE))
		{
			error(LM(Error_in_lesskey_content), NULL_PARG);
		}
		break;
	}
}

#endif /* HAVE_LESSKEYSRC */
#endif /* USERFILE */

/*
 * Handler for -S option.
 */
public void opt__S(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case TOGGLE:
		pos_rehead(TRUE);
		break;
	}
}

#if TAGS
/*
 * Handler for -t option.
 */
public void opt_t(int type, constant char *s)
{
	IFILE save_ifile;
	POSITION pos;

	switch (type)
	{
	case INIT:
		tagoption = save(s);
		/* Do the rest in main() */
		break;
	case TOGGLE:
		if (!secure_allow(SF_TAGS))
		{
			error(LM(tags_support_is_not_available), NULL_PARG);
			break;
		}
		findtag(skipspc(s));
		save_ifile = save_curr_ifile();
		/*
		 * Try to open the file containing the tag
		 * and search for the tag in that file.
		 */
		if (edit_tagfile() || (pos = tagsearch()) == NULL_POSITION)
		{
			/* Failed: reopen the old file. */
			reedit_ifile(save_ifile);
			break;
		}
		unsave_ifile(save_ifile);
		jump_loc(pos, jump_sline);
		break;
	}
}

/*
 * Handler for -T option.
 */
public void opt__T(int type, constant char *s)
{
	PARG parg;
	char *filename;

	switch (type)
	{
	case INIT:
		tags = save(s);
		break;
	case TOGGLE:
		s = skipspc(s);
		if (tags != NULL && tags != ztags)
			free(tags);
		filename = lglob(s);
		tags = shell_unquote(filename);
		free(filename);
		break;
	case QUERY:
		parg.p_string = tags;
		error(LM(Tags_file_X), &parg);
		break;
	}
}
#endif

/*
 * Handler for -p option.
 */
public void opt_p(int type, constant char *s)
{
	switch (type)
	{
	case INIT:
		/*
		 * Unget a command for the specified string.
		 */
		if (less_is_more)
		{
			/*
			 * In "more" mode, the -p argument is a command,
			 * not a search string, so we don't need a slash.
			 */
			every_first_cmd = save(s);
		} else
		{
			plusoption = TRUE;
			 /*
			  * {{ This won't work if the "/" command is
			  *    changed or invalidated by a .lesskey file. }}
			  */
			ungetsc("/");
			ungetsc(s);
			ungetcc_end_command();
		}
		break;
	}
}

/*
 * Handler for -P option.
 */
public void opt__P(int type, constant char *s)
{
	char **proto;
	PARG parg;

	switch (type)
	{
	case INIT:
	case TOGGLE:
		/*
		 * Figure out which prototype string should be changed.
		 */
		switch (*s)
		{
		case 's':  proto = &prproto[PR_SHORT];  s++;    break;
		case 'm':  proto = &prproto[PR_MEDIUM]; s++;    break;
		case 'M':  proto = &prproto[PR_LONG];   s++;    break;
		case '=':  proto = &eqproto;            s++;    break;
		case 'h':  proto = &hproto;             s++;    break;
		case 'w':  proto = &wproto;             s++;    break;
		default:   proto = &prproto[PR_SHORT];          break;
		}
		free(*proto);
		*proto = save(s);
		break;
	case QUERY:
		parg.p_string = prproto[pr_type];
		switch (pr_type)
		{
		case PR_MEDIUM: error(LM(Prompt_medium_X), &parg);  break;
		case PR_LONG:   error(LM(Prompt_long_X), &parg);    break;
		default:        error(LM(Prompt_short_X), &parg);   break;
		}
		break;
	}
}

/*
 * Handler for --end-prompt option.
 */
public void opt_end_prompt(int type, constant char *s)
{
	char **pend;
	PARG parg;

	switch (type)
	{
	case INIT:
	case TOGGLE:
		switch (*s)
		{
		case 's':  pend = &eprproto[PR_SHORT];  s++;    break;
		case 'm':  pend = &eprproto[PR_MEDIUM]; s++;    break;
		case 'M':  pend = &eprproto[PR_LONG];   s++;    break;
		default:   pend = &eprproto[PR_SHORT];          break;
		}
		if (*pend != NULL)
			free(*pend);
		*pend = (strcmp(s, "-") == 0) ? NULL : save(s);
		break;
	case QUERY:
		parg.p_string = eprproto[pr_type];
		if (parg.p_string == NULL)
			parg.p_string = LM(nothing);
		switch (pr_type)
		{
		case PR_MEDIUM: error(LM(Print_after_medium_prompt_X), &parg);  break;
		case PR_LONG:   error(LM(Print_after_long_prompt_X), &parg);    break;
		default:        error(LM(Print_after_short_prompt_X), &parg);   break;
		}
		break;
	}
}

/*
 * Handler for --autosave option.
 */
public void opt_autosave(int type, constant char *s)
{
	PARG parg;

	switch (type)
	{
	case INIT:
	case TOGGLE:
		if (s == NULL)
			s = "-";
		if (autosave != NULL)
			free(autosave);
		autosave = save(s);
		break;
	case QUERY:
		parg.p_string = (autosave != NULL) ? autosave : "-";
		error(LM(Autosave_actions_X), &parg);
		break;
	}
}

/*
 * Handler for the -b option.
 */
	/*ARGSUSED*/
public void opt_b(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case INIT:
	case TOGGLE:
		/*
		 * Set the new number of buffers.
		 */
		ch_setbufspace((ssize_t) bufspace);
		break;
	case QUERY:
		break;
	}
}

/*
 * Handler for the -i option.
 */
	/*ARGSUSED*/
public void opt_i(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case TOGGLE:
		chg_caseless();
		break;
	case QUERY:
	case INIT:
		break;
	}
}

/*
 * Handler for the -V option.
 */
	/*ARGSUSED*/
public void opt__V(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case TOGGLE:
	case QUERY:
		dispversion();
		break;
	case INIT:
		set_output(1, TRUE); /* Force output to stdout per GNU standard for --version output. */
		putstr("less ");
		putstr(version);
		putstr(" (");
		putstr(pattern_lib_name());
		putstr(" regular expressions)\n");
		{
			char constant *copyright = 
				"Copyright (C) 1984-2026  Mark Nudelman\n\n";
			putstr(copyright);
		}
		if (version[strlen(version)-1] == 'x')
		{
			putstr("** This is an EXPERIMENTAL build of the 'less' software,\n");
			putstr("** and may not function correctly.\n");
			putstr("** Obtain release builds from the web page below.\n\n");
		}
#if LESSTEST
		putstr("This build supports LESSTEST.\n");
#endif /*LESSTEST*/
		putstr("less comes with NO WARRANTY, to the extent permitted by law.\n");
		putstr("For information about the terms of redistribution,\n");
		putstr("see the file named README in the less distribution.\n");
		putstr("Home page: https://greenwoodsoftware.com/less\n");
		quit(QUIT_OK);
		break;
	}
}

#if MSDOS_COMPILER
/*
 * Parse an MSDOS color descriptor.
 */
static void colordesc(constant char *s, int *fg_color, int *bg_color, int *dattr)
{
	int fg, bg;
	CHAR_ATTR attr;
	if (parse_color(s, &fg, &bg, &attr) == CT_NULL)
	{
		PARG p;
		p.p_string = s;
		error(LM(Invalid_color_string_X), &p);
	} else
	{
		*fg_color = fg;
		*bg_color = bg;
		*dattr = 0;
#if MSDOS_COMPILER==WIN32C
		if (attr & CATTR_UNDERLINE)
			*dattr |= COMMON_LVB_UNDERSCORE;
		if (attr & CATTR_STANDOUT)
			*dattr |= COMMON_LVB_REVERSE_VIDEO;
#endif
	}
}
#endif

static int color_from_namechar(char namechar)
{
	switch (namechar)
	{
	case 'B': return AT_COLOR_BIN;
	case 'C': return AT_COLOR_CTRL;
	case 'E': return AT_COLOR_ERROR;
	case 'H': return AT_COLOR_HEADER;
	case 'J': return AT_COLOR_TARGET;
	case 'M': return AT_COLOR_MARK;
	case 'N': return AT_COLOR_LINENUM;
	case 'O': return AT_COLOR_OSC8;
	case 'P': return AT_COLOR_PROMPT;
	case 'R': return AT_COLOR_RSCROLL;
	case 'S': return AT_COLOR_SEARCH;
	case 'T': return AT_COLOR_TILDE;
	case 'W': case 'A': return AT_COLOR_ATTN;
	case 'n': return AT_NORMAL;
	case 's': return AT_STANDOUT;
	case 'd': return AT_BOLD;
	case 'u': return AT_UNDERLINE;
	case 'k': return AT_BLINK;
	default:
		if (namechar >= '1' && namechar <= '0'+NUM_SEARCH_COLORS)
			return AT_COLOR_SUBSEARCH(namechar-'0');
		return -1;
	}
}

/*
 * Handler for the -D option.
 */
	/*ARGSUSED*/
public void opt_D(int type, constant char *s)
{
	PARG p;
	int attr;

	switch (type)
	{
	case INIT:
	case TOGGLE:
#if MSDOS_COMPILER
		if (*s == 'a')
		{
			sgr_mode = !sgr_mode;
			if (type == TOGGLE)
			{
				p.p_string = (sgr_mode) ? LM(on) : LM(off);
				error(LM(SGR_mode_is_X), &p);
			}
			break;
		}
#endif
		attr = color_from_namechar(s[0]);
		if (attr < 0)
		{
			p.p_char = s[0];
			error(LM(Invalid_color_specifier_X), &p);
			return;
		}
		if (!use_color && (attr & AT_COLOR))
		{
			error(LM(Set__use_color_before_changing_colors), NULL_PARG);
			return;
		}
		s++;
#if MSDOS_COMPILER
		if (!(attr & AT_COLOR))
		{
			switch (attr)
			{
			case AT_NORMAL:
				colordesc(s, &nm_fg_color, &nm_bg_color, &nm_attr);
				break;
			case AT_BOLD:
				colordesc(s, &bo_fg_color, &bo_bg_color, &bo_attr);
				break;
			case AT_UNDERLINE:
				colordesc(s, &ul_fg_color, &ul_bg_color, &ul_attr);
				break;
			case AT_BLINK:
				colordesc(s, &bl_fg_color, &bl_bg_color, &bl_attr);
				break;
			case AT_STANDOUT:
				colordesc(s, &so_fg_color, &so_bg_color, &so_attr);
				break;
			}
			if (type == TOGGLE)
			{
				init_win_colors();
				at_enter(AT_STANDOUT);
				at_exit();
			}
		} else
#endif
		if (set_color_map(attr, s) < 0)
		{
			p.p_string = s;
			error(LM(Invalid_color_string_X), &p);
			return;
		}
		break;
	}
}

/*
 */
public void set_tabs(constant char *s, size_t len)
{
	int i;
	constant char *es = s + len;
	/* Start at 1 because tabstops[0] is always zero. */
	for (i = 1;  i < TABSTOP_MAX;  )
	{
		int n = 0;
		lbool v = FALSE;
		while (s < es && *s == ' ')
			s++;
		for (; s < es && *s >= '0' && *s <= '9'; s++)
		{
			v = v || ckd_mul(&n, n, 10);
			v = v || ckd_add(&n, n, *s - '0');
		}
		if (!v && n > tabstops[i-1])
			tabstops[i++] = n;
		while (s < es && *s == ' ')
			s++;
		if (s == es || *s++ != ',')
			break;
	}
	if (i < 2)
		return;
	ntabstops = i;
	tabdefault = tabstops[ntabstops-1] - tabstops[ntabstops-2];
}

/*
 * Handler for the -x option.
 */
public void opt_x(int type, constant char *s)
{
	size_t msglen;
	char *msg;
	int i;
	PARG p;

	switch (type)
	{
	case INIT:
	case TOGGLE:
		set_tabs(s, strlen(s));
		break;
	case QUERY:
		msglen = strlen(LM(Tab_stops)) + strlen(LM(and_then)) + strlen(LM(every_X_spaces)) + ((INT_STRLEN_BOUND(int)+1)*ntabstops) + 2;
		msg = ecalloc(msglen, sizeof(char));
		strcpy(msg, LM(Tab_stops));
		if (ntabstops > 2)
		{
			for (i = 1;  i < ntabstops;  i++)
			{
				if (i > 1)
					strcat(msg, ",");
				sprintf(msg+strlen(msg), "%d", tabstops[i]);
			}
			strcat(msg, " ");
			strcat(msg, LM(and_then));
		}
		sprintf(msg+strlen(msg), LM(every_X_spaces), tabdefault);
		p.p_string = msg;
		error("%s", &p);
		free(msg);
		break;
	}
}


/*
 * Handler for the -" option.
 */
public void opt_quote(int type, constant char *s)
{
	char buf[3];
	PARG parg;

	switch (type)
	{
	case INIT:
	case TOGGLE:
		if (s[0] == '\0')
		{
			openquote = closequote = '\0';
			break;
		}
		if (s[1] != '\0' && s[2] != '\0')
		{
			error(LM(dq_must_be_followed_by_1_or_2_chars), NULL_PARG);
			return;
		}
		openquote = s[0];
		if (s[1] == '\0')
			closequote = openquote;
		else
			closequote = s[1];
		break;
	case QUERY:
		buf[0] = openquote;
		buf[1] = closequote;
		buf[2] = '\0';
		parg.p_string = buf;
		error(LM(quotes_X), &parg);
		break;
	}
}

/*
 * Handler for the --rscroll option.
 */
	/*ARGSUSED*/
public void opt_rscroll(int type, constant char *s)
{
	PARG p;

	switch (type)
	{
	case INIT:
	case TOGGLE: {
		constant char *fmt;
		int attr = AT_STANDOUT;
		setfmt(s, &fmt, &attr, "*s>", FALSE);
		if (strcmp(fmt, "-") == 0)
		{
			rscroll_char = 0;
		} else
		{
			rscroll_attr = attr|AT_COLOR_RSCROLL;
			if (*fmt == '\0')
				rscroll_char = '>';
			else
			{
				LWCHAR ch = step_charc(&fmt, +1, fmt+strlen(fmt));
				if (pwidth(ch, rscroll_attr, 0, 0) > 1)
					error(LM(cannot_set_rscroll_to_a_wide_character), NULL_PARG);
				else
					rscroll_char = ch;
			}
		}
		break; }
	case QUERY: {
		p.p_string = rscroll_char == 0 ? "-" : utf_mode ? prutfchar(rscroll_char) : prchar(rscroll_char);
		error(LM(rscroll_character_is_X), &p);
		break; }
	}
}

/*
 * "-?" means display a help message.
 * If from the command line, exit immediately.
 */
	/*ARGSUSED*/
public void opt_query(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case QUERY:
	case TOGGLE:
		error(LM(Use_h_for_help), NULL_PARG);
		break;
	case INIT:
		dohelp = TRUE;
	}
}

	/*ARGSUSED*/
public void opt_match_shift(int type, constant char *s)
{
	switch (type)
	{
	case INIT:
	case TOGGLE:
		toggle_fraction(&match_shift, &match_shift_fraction,
			s, "--match-shift", FALSE, calc_match_shift);
		break;
	case QUERY:
		query_fraction(match_shift, match_shift_fraction,
			LM(Search_match_shift_is_X), LM(Search_match_shift_is_X_of_screen_width));
		break;
	}
}

public void calc_match_shift(void)
{
	if (match_shift_fraction < 0)
		return;
	match_shift = (int) muldiv(sc_width, match_shift_fraction, NUM_FRAC_DENOM);
}

/*
 * Handler for the --emouse option.
 */
	/*ARGSUSED*/
public void opt_emouse(int type, constant char *s)
{
	/* Order of entries matters for QUERY.
	 * Combinations must come after their components. */
	static struct csl_bitmap_def emouse_defs[] = {
		{ "hscroll",   EMOUSE_HSCROLL },
		{ "vscroll",   EMOUSE_VSCROLL },
		{ "hdrag",     EMOUSE_HDRAG },
		{ "vdrag",     EMOUSE_VDRAG },
		{ "lclick",    EMOUSE_LCLICK },
		{ "rclick",    EMOUSE_RCLICK },
		{ "scroll",    EMOUSE_HSCROLL|EMOUSE_VSCROLL },
		{ "drag",      EMOUSE_HDRAG|EMOUSE_VDRAG },
		{ "hmove",     EMOUSE_HSCROLL|EMOUSE_HDRAG },
		{ "vmove",     EMOUSE_VSCROLL|EMOUSE_VDRAG },
		{ "move",      EMOUSE_VSCROLL|EMOUSE_VDRAG|EMOUSE_HSCROLL|EMOUSE_HDRAG },
		{ "click",     EMOUSE_LCLICK|EMOUSE_RCLICK },
		{ "all",       EMOUSE_VSCROLL|EMOUSE_VDRAG|EMOUSE_HSCROLL|EMOUSE_HDRAG|EMOUSE_LCLICK|EMOUSE_RCLICK },
	};

	switch (type)
	{
	case INIT:
	case TOGGLE: {
		lbool was_mouse_enabled = (emouse != 0);
		if (s == NULL || strcmp(s, "-") == 0)
			emouse = 0;
		else
			emouse = parse_csl_bitmap(s,
			    emouse_defs, countof(emouse_defs), "--emouse");
		if (type == TOGGLE && was_mouse_enabled != (emouse != 0))
		{
			if (emouse == 0)
				deinit_mouse();
			else
				init_mouse();
		}
		break; }
	case QUERY: {
		char buf[128];
		char *bp = buf;
		char *ebuf = buf + sizeof(buf);
		int qmouse = emouse;
		PARG parg;
		int i;

		for (i = countof(emouse_defs)-1;  i >= 0;  i--)
		{
			int bit = emouse_defs[i].bit_value;
			if ((qmouse & bit) == bit)
			{
				if (bp > buf && bp+1 < ebuf)
					*bp++ = ',';
				strncpy(bp, emouse_defs[i].bit_name, ptr_diff(ebuf, bp));
				bp += strlen(bp);
				qmouse &= ~bit;
			}
		}
		*bp = '\0';
		parg.p_string = buf;
		if (buf[0] == '\0')
			error(LM(Ignore_mouse_input), NULL_PARG);
		else
			error(LM(Mouse_features_enabled_X), &parg);
		break; }
	}
}

/*
 * Handler for the --mouse option.
 */
	/*ARGSUSED*/
public void opt_mouse(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case INIT:
	case TOGGLE:
		if (emouse == 0)
		{
			opt_emouse(type, "vmove,click");
			mouse_reverse = (xmouse == OPT_ONPLUS);
		} else
		{
			opt_emouse(type, "-");
			xmouse = 0;
		}
		if (type == INIT)
			break;
		/*FALLTHRU*/
	case QUERY: /* odesc[] entries are NULL so we can do the QUERY here */
		if (emouse == (EMOUSE_VSCROLL|EMOUSE_VDRAG|EMOUSE_LCLICK|EMOUSE_RCLICK))
		{
			if (mouse_reverse)
				error(LM(Use_the_mouse_for_scrolling_vertically_reverse), NULL_PARG);
			else
				error(LM(Use_the_mouse_for_scrolling_vertically), NULL_PARG);
		} else
			opt_emouse(QUERY, NULL);
		break;
	}
}

/*
 * Handler for the --wheel-lines option.
 */
	/*ARGSUSED*/
public void opt_wheel_lines(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case INIT:
	case TOGGLE:
		if (wheel_lines <= 0)
			wheel_lines = default_wheel_lines();
		break;
	case QUERY:
		break;
	}
}

/*
 * Handler for the --line-number-width option.
 */
	/*ARGSUSED*/
public void opt_linenum_width(int type, constant char *s)
{
	PARG parg;

	(void)s;
	switch (type)
	{
	case INIT:
	case TOGGLE:
		if (linenum_width > MAX_LINENUM_WIDTH)
		{
			parg.p_int = MAX_LINENUM_WIDTH;
			error(LM(Line_number_width_must_not_be_larger_than_X), &parg);
			linenum_width = MIN_LINENUM_WIDTH;
		} 
		break;
	case QUERY:
		break;
	}
}

/*
 * Handler for the --status-column-width option.
 */
	/*ARGSUSED*/
public void opt_status_col_width(int type, constant char *s)
{
	PARG parg;

	(void)s;
	switch (type)
	{
	case INIT:
	case TOGGLE:
		if (status_col_width > MAX_STATUSCOL_WIDTH)
		{
			parg.p_int = MAX_STATUSCOL_WIDTH;
			error(LM(Status_column_width_must_not_be_larger_than_X), &parg);
			status_col_width = 2;
		}
		break;
	case QUERY:
		break;
	}
}

/*
 * Handler for the --file-size option.
 */
	/*ARGSUSED*/
public void opt_filesize(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case INIT:
	case TOGGLE:
		if (want_filesize && curr_ifile != NULL && ch_length() == NULL_POSITION)
			scan_eof();
		break;
	case QUERY:
		break;
	}
}

/*
 * Handler for the --cmd option.
 */
	/*ARGSUSED*/
public void opt_first_cmd_at_prompt(int type, constant char *s)
{
	switch (type)
	{
	case INIT:
	case TOGGLE:
		first_cmd_at_prompt = save(s);
		break;
	case QUERY:
		break;
	}
}

/*
 * Handler for the --intr option.
 */
	/*ARGSUSED*/
public void opt_intr(int type, constant char *s)
{
	PARG p;

	switch (type)
	{
	case INIT:
	case TOGGLE:
		intr_char = *s;
		if (intr_char == '^' && s[1] != '\0')
			intr_char = CONTROL(s[1]);
		break;
	case QUERY: {
		p.p_string = prchar((LWCHAR) intr_char);
		error(LM(interrupt_character_is_X), &p);
		break; }
	}
}

/*
 * Return the next number from a comma-separated list.
 * Return -1 if the list entry is missing or empty.
 * Updates *sp to point to the first char of the next number in the list.
 */
static lbool next_cnum(constant char **sp, constant char *printopt, mutable int *p_num)
{
	if (**sp == '\0') /* at end of line */
	{
		*p_num = -1;
		return TRUE;
	}
	if (**sp == ',') /* that's the next comma; we have an empty string */
	{
		++(*sp);
		*p_num = -1;
		return TRUE;
	}
	if (!getnumc(sp, printopt, FALSE, p_num))
		return FALSE;
	if (**sp == ',')
		++(*sp);
	return TRUE;
}

/*
 * Parse a parameter to the --header option.
 * Value is "L,C,N", where each field is a decimal number or empty.
 */
static lbool parse_header(constant char *s, int *lines, int *cols, POSITION *start_pos)
{
	int n;

	if (*s == '-')
		s = "0,0";
	if (!next_cnum(&s, "--header", &n))
		return FALSE;
	if (n >= 0) *lines = n;
	if (!next_cnum(&s, "--header", &n))
		return FALSE;
	if (n >= 0) *cols = n;
	if (!next_cnum(&s, "--header", &n))
		return FALSE;
	if (n > 0)
	{
		if (n < 1) n = 1;
		*start_pos = find_pos((LINENUM)n);
	}
	return TRUE;
}

/*
 * Handler for the --header option.
 */
	/*ARGSUSED*/
public void opt_header(int type, constant char *s)
{
	switch (type)
	{
	case INIT:
		/* Can't call parse_header now because input file is not yet opened,
		 * so find_pos won't work. */
		init_header = save(s);
		break;
	case TOGGLE: {
		int lines = header_lines;
		int cols = header_cols;
		POSITION start_pos = (type == INIT) ? ch_zero() : position(TOP);
		if (start_pos == NULL_POSITION) start_pos = ch_zero();
		if (!parse_header(s, &lines, &cols, &start_pos))
			break;
		header_lines = lines;
		header_cols = cols;
		set_header(start_pos);
		calc_jump_sline();
		break; }
	case QUERY: {
		char buf[3*INT_STRLEN_BOUND(long)+3];
		PARG parg;
		SNPRINTF3(buf, sizeof(buf), "%ld,%ld,%ld", (long) header_lines, (long) header_cols, (long) find_linenum(header_start_pos));
		parg.p_string = buf;
		error(LM(Header_is_X), &parg);
		break; }
	}
}

/*
 * Handler for the --hilite-target option.
 */
	/*ARGSUSED*/
public void opt_hilite_target(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case INIT:
		break;
	case TOGGLE:
		draw_target_attn(hilite_target != OPT_OFF);
		break;
    case QUERY:
		break;
	}
}

/*
 * Handler for the --search-options option.
 */
	/*ARGSUSED*/
public void opt_search_type(int type, constant char *s)
{
	int st;
	PARG parg;
	char buf[16];
	char *bp;
	int i;

	switch (type)
	{
	case INIT:
	case TOGGLE:
		st = 0;
		for (;  *s != '\0';  s++)
		{
			switch (*s)
			{
			case 'E': case 'e': case CONTROL('E'): st |= SRCH_PAST_EOF;   break;
			case 'F': case 'f': case CONTROL('F'): st |= SRCH_FIRST_FILE; break;
			case 'K': case 'k': case CONTROL('K'): st |= SRCH_NO_MOVE;    break;
			case 'N': case 'n': case CONTROL('N'): st |= SRCH_NO_MATCH;   break;
			case 'R': case 'r': case CONTROL('R'): st |= SRCH_NO_REGEX;   break;
			case 'W': case 'w': case CONTROL('W'): st |= SRCH_WRAP;       break;
			case '-': st = 0; break;
			case '^': break;
			default:
				if (*s >= '1' && *s <= '0'+NUM_SEARCH_COLORS)
				{
					st |= SRCH_SUBSEARCH(*s-'0');
					break;
				}
				parg.p_char = *s;
				error(LM(invalid_search_option_X), &parg);
				return;
			}
		}
		def_search_type = norm_search_type(st);
		break;
	case QUERY:
		bp = buf;
		if (def_search_type & SRCH_PAST_EOF)   *bp++ = 'E'; 
		if (def_search_type & SRCH_FIRST_FILE) *bp++ = 'F'; 
		if (def_search_type & SRCH_NO_MOVE)    *bp++ = 'K'; 
		if (def_search_type & SRCH_NO_MATCH)   *bp++ = 'N'; 
		if (def_search_type & SRCH_NO_REGEX)   *bp++ = 'R'; 
		if (def_search_type & SRCH_WRAP)       *bp++ = 'W'; 
		for (i = 1;  i <= NUM_SEARCH_COLORS;  i++)
			if (def_search_type & SRCH_SUBSEARCH(i))
				*bp++ = (char) ('0'+i);
		if (bp == buf)
			*bp++ = '-';
		*bp = '\0';
		parg.p_string = buf;
		error(LM(search_options_X), &parg);
		break;
	}
}

/*
 * Handler for the --no-search-headers, --no-search-header-lines
 * and --no-search-header-cols options.
 */
static void do_nosearch_headers(int type, int no_header_lines, int no_header_cols)
{
	switch (type)
	{
	case INIT:
	case TOGGLE:
		nosearch_header_lines = no_header_lines;
		nosearch_header_cols = no_header_cols;
		if (type != TOGGLE) break;
		/*FALLTHRU*/
	case QUERY:
		if (nosearch_header_lines && nosearch_header_cols)
			error(LM(Search_does_not_include_header_lines_or_columns), NULL_PARG);
		else if (nosearch_header_lines)
			error(LM(Search_includes_header_columns_but_not_header_lines), NULL_PARG);
		else if (nosearch_header_cols)
			error(LM(Search_includes_header_lines_but_not_header_columns), NULL_PARG);
		else
			error(LM(Search_includes_header_lines_and_columns), NULL_PARG);
	}
}

	/*ARGSUSED*/
public void opt_nosearch_headers(int type, constant char *s)
{
	(void)s;
	do_nosearch_headers(type, 1, 1);
}

	/*ARGSUSED*/
public void opt_nosearch_header_lines(int type, constant char *s)
{
	(void)s;
	do_nosearch_headers(type, 1, 0);
}

	/*ARGSUSED*/
public void opt_nosearch_header_cols(int type, constant char *s)
{
	(void)s;
	do_nosearch_headers(type, 0, 1);
}

	/*ARGSUSED*/
public void opt_no_paste(int type, constant char *s)
{
	(void)s;
	switch (type)
	{
	case TOGGLE:
		if (no_paste)
			init_bracketed_paste();
		else
			deinit_bracketed_paste();
		break;
	case INIT:
	case QUERY:
		break;
	}
}

#if LESSTEST
/*
 * Handler for the --tty option.
 */
	/*ARGSUSED*/
public void opt_ttyin_name(int type, constant char *s)
{
	switch (type)
	{
	case INIT:
		ttyin_name = s;
		is_tty = TRUE;
		break;
	}
}
#endif /*LESSTEST*/

public int chop_line(void)
{
	return (chopline || header_cols > 0 || header_lines > 0);
}

/*
 * Get the "screen window" size.
 */
public int get_swindow(void)
{
	if (swindow > 0)
		return (swindow);
	return (sc_height - header_lines + swindow);
}

/*
 * Should an action initiate an autosave?
 */
public lbool autosave_action(char act)
{
	return strchr(autosave, act) != NULL || strchr(autosave, '*') != NULL;
}
