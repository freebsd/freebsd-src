/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * The option table.
 */

#include "less.h"
#include "option.h"

/*
 * Variables controlled by command line options.
 */
public int quiet;		/* Should we suppress the audible bell? */
public int how_search;		/* Where should forward searches start? */
public int top_scroll;		/* Repaint screen from top?
				   (alternative is scroll from bottom) */
public int pr_type;		/* Type of prompt (short, medium, long) */
public int bs_mode;		/* How to process backspaces */
public int know_dumb;		/* Don't complain about dumb terminals */
public int quit_at_eof;		/* Quit after hitting end of file twice */
public int quit_if_one_screen;	/* Quit if EOF on first screen */
public int squeeze;		/* Squeeze multiple blank lines into one */
public int tabstop;		/* Tab settings */
public int back_scroll;		/* Repaint screen on backwards movement */
public int forw_scroll;		/* Repaint screen on forward movement */
public int caseless;		/* Do "caseless" searches */
public int linenums;		/* Use line numbers */
public int cbufs;		/* Current number of buffers */
public int autobuf;		/* Automatically allocate buffers as needed */
public int ctldisp;		/* Send control chars to screen untranslated */
public int force_open;		/* Open the file even if not regular file */
public int swindow;		/* Size of scrolling window */
public int jump_sline;		/* Screen line of "jump target" */
public int chopline;		/* Truncate displayed lines at screen width */
public int no_init;		/* Disable sending ti/te termcap strings */
public int twiddle;             /* Show tildes after EOF */
public int show_attn;		/* Hilite first unread line */
public int shift_count;		/* Number of positions to shift horizontally */
public int status_col;		/* Display a status column */
#if HILITE_SEARCH
public int hilite_search;	/* Highlight matched search patterns? */
#endif

/*
 * Long option names.
 */
static struct optname a_optname      = { "search-skip-screen",   NULL };
static struct optname b_optname      = { "buffers",              NULL };
static struct optname B__optname     = { "auto-buffers",         NULL };
static struct optname c_optname      = { "clear-screen",         NULL };
static struct optname d_optname      = { "dumb",                 NULL };
#if MSDOS_COMPILER
static struct optname D__optname     = { "color",                NULL };
#endif
static struct optname e_optname      = { "quit-at-eof",          NULL };
static struct optname f_optname      = { "force",                NULL };
static struct optname F__optname     = { "quit-if-one-screen",   NULL };
#if HILITE_SEARCH
static struct optname g_optname      = { "hilite-search",        NULL };
#endif
static struct optname h_optname      = { "max-back-scroll",      NULL };
static struct optname i_optname      = { "ignore-case",          NULL };
static struct optname j_optname      = { "jump-target",          NULL };
static struct optname J__optname     = { "status-column",        NULL };
#if USERFILE
static struct optname k_optname      = { "lesskey-file",         NULL };
#endif
static struct optname m_optname      = { "long-prompt",          NULL };
static struct optname n_optname      = { "line-numbers",         NULL };
#if LOGFILE
static struct optname o_optname      = { "log-file",             NULL };
static struct optname O__optname     = { "LOG-FILE",             NULL };
#endif
static struct optname p_optname      = { "pattern",              NULL };
static struct optname P__optname     = { "prompt",               NULL };
static struct optname q2_optname     = { "silent",               NULL };
static struct optname q_optname      = { "quiet",                &q2_optname };
static struct optname r_optname      = { "raw-control-chars",    NULL };
static struct optname s_optname      = { "squeeze-blank-lines",  NULL };
static struct optname S__optname     = { "chop-long-lines",      NULL };
#if TAGS
static struct optname t_optname      = { "tag",                  NULL };
static struct optname T__optname     = { "tag-file",             NULL };
#endif
static struct optname u_optname      = { "underline-special",    NULL };
static struct optname V__optname     = { "version",              NULL };
static struct optname w_optname      = { "hilite-unread",        NULL };
static struct optname x_optname      = { "tabs",                 NULL };
static struct optname X__optname     = { "no-init",              NULL };
static struct optname y_optname      = { "max-forw-scroll",      NULL };
static struct optname z_optname      = { "window",               NULL };
static struct optname quote_optname  = { "quotes",               NULL };
static struct optname tilde_optname  = { "tilde",                NULL };
static struct optname query_optname  = { "help",                 NULL };
static struct optname pound_optname  = { "shift",                NULL };


/*
 * Table of all options and their semantics.
 */
static struct option option[] =
{
	{ 'a', &a_optname,
		BOOL, OPT_OFF, &how_search, NULL,
		"Search includes displayed screen",
		"Search skips displayed screen",
		NULL
	},

	{ 'b', &b_optname,
		NUMBER, 10, &cbufs, opt_b, 
		"Buffers: ",
		"%d buffers",
		NULL
	},
	{ 'B', &B__optname,
		BOOL, OPT_ON, &autobuf, NULL,
		"Don't automatically allocate buffers",
		"Automatically allocate buffers when needed",
		NULL
	},
	{ 'c', &c_optname,
		TRIPLE, OPT_OFF, &top_scroll, NULL,
		"Repaint by scrolling from bottom of screen",
		"Repaint by clearing each line",
		"Repaint by painting from top of screen"
	},
	{ 'd', &d_optname,
		BOOL|NO_TOGGLE, OPT_OFF, &know_dumb, NULL,
		"Assume intelligent terminal",
		"Assume dumb terminal",
		NULL
	},
#if MSDOS_COMPILER
	{ 'D', &D__optname,
		STRING|REPAINT|NO_QUERY, 0, NULL, opt_D,
		"color desc: ", NULL, NULL
	},
#endif
	{ 'e', &e_optname,
		TRIPLE, OPT_OFF, &quit_at_eof, NULL,
		"Don't quit at end-of-file",
		"Quit at end-of-file",
		"Quit immediately at end-of-file"
	},
	{ 'f', &f_optname,
		BOOL, OPT_OFF, &force_open, NULL,
		"Open only regular files",
		"Open even non-regular files",
		NULL
	},
	{ 'F', &F__optname,
		BOOL, OPT_OFF, &quit_if_one_screen, NULL,
		"Don't quit if end-of-file on first screen",
		"Quit if end-of-file on first screen",
		NULL
	},
#if HILITE_SEARCH
	{ 'g', &g_optname,
		TRIPLE|HL_REPAINT, OPT_ONPLUS, &hilite_search, NULL,
		"Don't highlight search matches",
		"Highlight matches for previous search only",
		"Highlight all matches for previous search pattern",
	},
#endif
	{ 'h', &h_optname,
		NUMBER, -1, &back_scroll, NULL,
		"Backwards scroll limit: ",
		"Backwards scroll limit is %d lines",
		NULL
	},
	{ 'i', &i_optname,
		TRIPLE|HL_REPAINT, OPT_OFF, &caseless, opt_i,
		"Case is significant in searches",
		"Ignore case in searches",
		"Ignore case in searches and in patterns"
	},
	{ 'j', &j_optname,
		NUMBER, 1, &jump_sline, NULL,
		"Target line: ",
		"Position target at screen line %d",
		NULL
	},
	{ 'J', &J__optname,
		BOOL|REPAINT, OPT_OFF, &status_col, NULL,
		"Don't display a status column",
		"Display a status column",
		NULL
	},
#if USERFILE
	{ 'k', &k_optname,
		STRING|NO_TOGGLE|NO_QUERY, 0, NULL, opt_k,
		NULL, NULL, NULL
	},
#endif
	{ 'l', NULL,
		STRING|NO_TOGGLE|NO_QUERY, 0, NULL, opt_l,
		NULL, NULL, NULL
	},
	{ 'm', &m_optname,
		TRIPLE, OPT_OFF, &pr_type, NULL,
		"Short prompt",
		"Medium prompt",
		"Long prompt"
	},
	{ 'n', &n_optname,
		TRIPLE|REPAINT, OPT_ON, &linenums, NULL,
		"Don't use line numbers",
		"Use line numbers",
		"Constantly display line numbers"
	},
#if LOGFILE
	{ 'o', &o_optname,
		STRING, 0, NULL, opt_o,
		"log file: ", NULL, NULL
	},
	{ 'O', &O__optname,
		STRING, 0, NULL, opt__O,
		"Log file: ", NULL, NULL
	},
#endif
	{ 'p', &p_optname,
		STRING|NO_TOGGLE|NO_QUERY, 0, NULL, opt_p,
		NULL, NULL, NULL
	},
	{ 'P', &P__optname,
		STRING, 0, NULL, opt__P,
		"prompt: ", NULL, NULL
	},
	{ 'q', &q_optname,
		TRIPLE, OPT_OFF, &quiet, NULL,
		"Ring the bell for errors AND at eof/bof",
		"Ring the bell for errors but not at eof/bof",
		"Never ring the bell"
	},
	{ 'r', &r_optname,
		TRIPLE|REPAINT, OPT_OFF, &ctldisp, NULL,
		"Display control characters as ^X",
		"Display control characters directly",
		"Display control characters directly, processing ANSI sequences"
	},
	{ 's', &s_optname,
		BOOL|REPAINT, OPT_OFF, &squeeze, NULL,
		"Display all blank lines",
		"Squeeze multiple blank lines",
		NULL
	},
	{ 'S', &S__optname,
		BOOL|REPAINT, OPT_OFF, &chopline, NULL,
		"Fold long lines",
		"Chop long lines",
		NULL
	},
#if TAGS
	{ 't', &t_optname,
		STRING|NO_QUERY, 0, NULL, opt_t,
		"tag: ", NULL, NULL
	},
	{ 'T', &T__optname,
		STRING, 0, NULL, opt__T,
		"tags file: ", NULL, NULL
	},
#endif
	{ 'u', &u_optname,
		TRIPLE|REPAINT, OPT_OFF, &bs_mode, NULL,
		"Display underlined text in underline mode",
		"Backspaces cause overstrike",
		"Print backspace as ^H"
	},
	{ 'V', &V__optname,
		NOVAR, 0, NULL, opt__V,
		NULL, NULL, NULL
	},
	{ 'w', &w_optname,
		TRIPLE|REPAINT, OPT_OFF, &show_attn, NULL,
		"Don't highlight first unread line",
		"Highlight first unread line after forward-screen",
		"Highlight first unread line after any forward movement",
	},
	{ 'x', &x_optname,
		NUMBER|REPAINT, 8, &tabstop, NULL,
		"Tab stops: ",
		"Tab stops every %d spaces", 
		NULL
	},
	{ 'X', &X__optname,
		BOOL|NO_TOGGLE, OPT_OFF, &no_init, NULL,
		"Send init/deinit strings to terminal",
		"Don't use init/deinit strings",
		NULL
	},
	{ 'y', &y_optname,
		NUMBER, -1, &forw_scroll, NULL,
		"Forward scroll limit: ",
		"Forward scroll limit is %d lines",
		NULL
	},
	{ 'z', &z_optname,
		NUMBER, -1, &swindow, NULL,
		"Scroll window size: ",
		"Scroll window size is %d lines",
		NULL
	},
	{ '"', &quote_optname,
		STRING, 0, NULL, opt_quote,
		"quotes: ", NULL, NULL
	},
	{ '~', &tilde_optname,
		BOOL|REPAINT, OPT_ON, &twiddle, NULL,
		"Don't show tildes after end of file",
		"Show tildes after end of file",
		NULL
	},
	{ '?', &query_optname,
		NOVAR, 0, NULL, opt_query,
		NULL, NULL, NULL
	},
	{ '#', &pound_optname,
		NUMBER, 0, &shift_count, NULL,
		"Horizontal shift: ",
		"Horizontal shift %d positions",
		NULL
	},
	{ '\0', NULL, NOVAR, 0, NULL, NULL, NULL, NULL, NULL }
};


/*
 * Initialize each option to its default value.
 */
	public void
init_option()
{
	register struct option *o;

	for (o = option;  o->oletter != '\0';  o++)
	{
		/*
		 * Set each variable to its default.
		 */
		if (o->ovar != NULL)
			*(o->ovar) = o->odefault;
	}
}

/*
 * Find an option in the option table, given its option letter.
 */
	public struct option *
findopt(c)
	int c;
{
	register struct option *o;

	for (o = option;  o->oletter != '\0';  o++)
	{
		if (o->oletter == c)
			return (o);
		if ((o->otype & TRIPLE) && toupper(o->oletter) == c)
			return (o);
	}
	return (NULL);
}

/*
 * Find an option in the option table, given its option name.
 * p_optname is the (possibly partial) name to look for, and
 * is updated to point after the matched name.
 * p_oname if non-NULL is set to point to the full option name.
 */
	public struct option *
findopt_name(p_optname, p_oname, p_err)
	char **p_optname;
	char **p_oname;
	int *p_err;
{
	char *optname = *p_optname;
	register struct option *o;
	register struct optname *oname;
	register int len;
	int uppercase;
	struct option *maxo = NULL;
	struct optname *maxoname = NULL;
	int maxlen = 0;
	int ambig = 0;
	int exact = 0;

	/*
	 * Check all options.
	 */
	for (o = option;  o->oletter != '\0';  o++)
	{
		/*
		 * Check all names for this option.
		 */
		for (oname = o->onames;  oname != NULL;  oname = oname->onext)
		{
			/* 
			 * Try normal match first (uppercase == 0),
			 * then, then if it's a TRIPLE option,
			 * try uppercase match (uppercase == 1).
			 */
			for (uppercase = 0;  uppercase <= 1;  uppercase++)
			{
				len = sprefix(optname, oname->oname, uppercase);
				if (!exact && len == maxlen)
					/*
					 * Already had a partial match,
					 * and now there's another one that
					 * matches the same length.
					 */
					ambig = 1;
				else if (len > maxlen)
				{
					/*
					 * Found a better match than
					 * the one we had.
					 */
					maxo = o;
					maxoname = oname;
					maxlen = len;
					ambig = 0;
					exact = (len == strlen(oname->oname));
				}
				if (!(o->otype & TRIPLE))
					break;
			}
		}
	}
	if (ambig)
	{
		/*
		 * Name matched more than one option.
		 */
		if (p_err != NULL)
			*p_err = OPT_AMBIG;
		return (NULL);
	}
	*p_optname = optname + maxlen;
	if (p_oname != NULL)
		*p_oname = maxoname->oname;
	return (maxo);
}
