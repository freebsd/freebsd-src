/* opts.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains the code that manages the run-time options -- The 
 * values that can be modified via the "set" command.
 */

#include "config.h"
#include "vi.h"
#include "ctype.h"
#ifndef NULL
#define NULL (char *)0
#endif

/* maximum width to permit for strings, including ="" */
#define MAXWIDTH 20

/* These are the default values of all options */
char	o_autoindent[1] =	{FALSE};
char	o_autoprint[1] =	{TRUE};
char	o_autotab[1] =		{TRUE};
char	o_autowrite[1] = 	{FALSE};
char	o_columns[3] =		{80, 32, ~0};
char	o_directory[30] =	TMPDIR;
char	o_edcompatible[1] =	{FALSE};
char	o_equalprg[80] =	{"fmt"};
char	o_errorbells[1] =	{TRUE};
char	o_exrefresh[1] =	{TRUE};
char	o_ignorecase[1] =	{FALSE};
char	o_keytime[3] =		{2, 0, 50};
char	o_keywordprg[80] =	{KEYWORDPRG};
char	o_lines[3] =		{25, 2, 96};
char	o_list[1] =		{FALSE};
char	o_number[1] =		{FALSE};
char	o_readonly[1] =		{FALSE};
char	o_remap[1] =		{TRUE};
char	o_report[3] =		{5, 1, 127};
char	o_scroll[3] =		{12, 1, 127};
char	o_shell[60] =		SHELL;
char	o_shiftwidth[3] =	{8, 1, ~0};
char	o_sidescroll[3] =	{8, 1, 40};
char	o_sync[1] =		{NEEDSYNC};
char	o_tabstop[3] =		{8, 1, 40};
char	o_term[30] =		"?";
char	o_flash[1] =		{TRUE};
char	o_warn[1] =		{TRUE};
char	o_wrapscan[1] =		{TRUE};

#ifndef CRUNCH
char	o_beautify[1] =		{FALSE};
char	o_exrc[1] =		{FALSE};
char	o_mesg[1] =		{TRUE};
char	o_more[1] =		{TRUE};
char	o_nearscroll[3] =	{15, 0, ~0};
char	o_novice[1] =		{FALSE};
char	o_prompt[1] =		{TRUE};
char	o_taglength[3] =	{0, 0, 30};
char	o_tags[256] =		{"tags"};
char	o_terse[1] =		{FALSE};
char	o_window[3] =		{0, 1, 24};
char	o_wrapmargin[3] =	{0, 0, ~0};
char	o_writeany[1] =		{FALSE};
#endif

#ifndef NO_ERRLIST
char	o_cc[30] =		{CC_COMMAND};
char	o_make[30] =		{MAKE_COMMAND};
#endif

#ifndef NO_CHARATTR
char	o_charattr[1] =		{FALSE};
#endif

#ifndef NO_DIGRAPH
char	o_digraph[1] =		{FALSE};
char	o_flipcase[80]
# ifdef CS_IBMPC
	= {"\207\200\201\232\202\220\204\216\206\217\221\222\224\231\244\245"}
# endif
# ifdef CS_LATIN1
	/* initialized by initopts() */
# endif
	;
#endif

#ifndef NO_SENTENCE
char	o_hideformat[1] =	{FALSE};
#endif

#ifndef NO_EXTENSIONS
char	o_inputmode[1] =	{FALSE};
char	o_ruler[1] =		{FALSE};
#endif

#ifndef NO_MAGIC
char	o_magic[1] =		{TRUE};
#endif

#ifndef NO_MODELINES
char	o_modelines[1] =	{FALSE};
#endif

#ifndef NO_SENTENCE
char	o_paragraphs[30] =	"PPppIPLPQP";
char	o_sections[30] =	"NHSHSSSEse";
#endif

#if MSDOS
char	o_pcbios[1] =		{TRUE};
#endif

#ifndef NO_SHOWMATCH
char	o_showmatch[1] =	{FALSE};
#endif

#ifndef	NO_SHOWMODE
char	o_smd[1] =		{FALSE};
#endif

#ifndef NO_TAGSTACK
char	o_tagstack[1] =		{TRUE};
#endif



/* The following describes the names & types of all options */
#define BOOL	0
#define	NUM	1
#define	STR	2
#define SET	0x01	/* this option has had its value altered */
#define CANSET	0x02	/* this option can be set at any time */
#define RCSET	0x06	/* this option can be set in a .exrc file only */
#define NOSAVE	0x0a	/* this option should never be saved by mkexrc */
#define WSET	0x20	/* is this the "window" size option? */
#define MR	0x40	/* does this option affect the way text is displayed? */
struct
{
	char	*name;	/* name of an option */
	char	*nm;	/* short name of an option */
	char	type;	/* type of an option */
	char	flags;	/* boolean: has this option been set? */
	char	*value;	/* value */
}
	opts[] =
{
	/* name			type	flags		value */
	{ "autoindent",	"ai",	BOOL,	CANSET,		o_autoindent	},
	{ "autoprint",	"ap",	BOOL,	CANSET,		o_autoprint	},
	{ "autotab",	"at",	BOOL,	CANSET,		o_autotab	},
	{ "autowrite",	"aw",	BOOL,	CANSET,		o_autowrite	},
#ifndef CRUNCH
	{ "beautify",	"bf",	BOOL,	CANSET,		o_beautify	},
#endif
#ifndef NO_ERRLIST
	{ "cc",		"cc",	STR,	CANSET,		o_cc		},
#endif
#ifndef NO_CHARATTR
	{ "charattr",	"ca",	BOOL,	CANSET|MR,	o_charattr	},
#endif
	{ "columns",	"co",	NUM,	SET|NOSAVE|MR,	o_columns	},
#ifndef NO_DIGRAPH
	{ "digraph",	"dig",	BOOL,	CANSET,		o_digraph	},
#endif
	{ "directory",	"dir",	STR,	RCSET,		o_directory	},
	{ "edcompatible","ed",	BOOL,	CANSET,		o_edcompatible	},
	{ "equalprg",	"ep",	STR,	CANSET,		o_equalprg	},
	{ "errorbells",	"eb",	BOOL,	CANSET,		o_errorbells	},
#ifndef CRUNCH
	{ "exrc",	"exrc",	BOOL,	CANSET,		o_exrc		},
#endif
	{ "exrefresh",	"er",	BOOL,	CANSET,		o_exrefresh	},
	{ "flash",	"vbell",BOOL,	CANSET,		o_flash		},
#ifndef NO_DIGRAPH
	{ "flipcase",	"fc",	STR,	CANSET,		o_flipcase	},
#endif
#ifndef NO_SENTENCE
	{ "hideformat",	"hf",	BOOL,	CANSET|MR,	o_hideformat	},
#endif
	{ "ignorecase",	"ic",	BOOL,	CANSET,		o_ignorecase	},
#ifndef NO_EXTENSIONS
	{ "inputmode",	"im",	BOOL,	CANSET,		o_inputmode	},
#endif
	{ "keytime",	"kt",	NUM,	CANSET,		o_keytime	},
	{ "keywordprg",	"kp",	STR,	CANSET,		o_keywordprg	},
	{ "lines",	"ls",	NUM,	SET|NOSAVE|MR,	o_lines		},
	{ "list",	"li",	BOOL,	CANSET|MR,	o_list		},
#ifndef NO_MAGIC
	{ "magic",	"ma",	BOOL,	CANSET,		o_magic		},
#endif
#ifndef NO_ERRLIST
	{ "make",	"mk",	STR,	CANSET,		o_make		},
#endif
#ifndef CRUNCH
	{ "mesg",	"me",	BOOL,	CANSET,		o_mesg		},
#endif
#ifndef NO_MODELINES
	{ "modelines",	"ml",	BOOL,	CANSET,		o_modelines	},
#endif
#ifndef CRUNCH
	{ "more",	"mo",	BOOL,	CANSET,		o_more		},
	{ "nearscroll",	"ns",	NUM,	CANSET,		o_nearscroll	},
	{ "novice",	"nov",	BOOL,	CANSET,		o_novice	},
#endif
	{ "number",	"nu",	BOOL,	CANSET|MR,	o_number	},
#ifndef NO_SENTENCE
	{ "paragraphs",	"para",	STR,	CANSET,		o_paragraphs	},
#endif
#if MSDOS
	{ "pcbios",	"pc",	BOOL,	SET|NOSAVE,	o_pcbios	},
#endif
#ifndef CRUNCH
	{ "prompt",	"pr",	BOOL,	CANSET,		o_prompt	},
#endif
	{ "readonly",	"ro",	BOOL,	CANSET,		o_readonly	},
	{ "remap",	"remap",BOOL,	CANSET,		o_remap		},
	{ "report",	"re",	NUM,	CANSET,		o_report	},
#ifndef NO_EXTENSIONS
	{ "ruler",	"ru",	BOOL,	CANSET,		o_ruler		},
#endif
	{ "scroll",	"sc",	NUM,	CANSET,		o_scroll	},
#ifndef NO_SENTENCE
	{ "sections",	"sect",	STR,	CANSET,		o_sections	},
#endif
	{ "shell",	"sh",	STR,	CANSET,		o_shell		},
#ifndef NO_SHOWMATCH
	{ "showmatch",	"sm",	BOOL,	CANSET,		o_showmatch	},
#endif
#ifndef	NO_SHOWMODE
	{ "showmode",	"smd",	BOOL,	CANSET,		o_smd		},
#endif
	{ "shiftwidth",	"sw",	NUM,	CANSET,		o_shiftwidth	},
	{ "sidescroll",	"ss",	NUM,	CANSET,		o_sidescroll	},
	{ "sync",	"sy",	BOOL,	CANSET,		o_sync		},
	{ "tabstop",	"ts",	NUM,	CANSET|MR,	o_tabstop	},
#ifndef CRUNCH
	{ "taglength",	"tl",	NUM,	CANSET,		o_taglength	},
	{ "tags",	"tag",	STR,	CANSET,		o_tags		},
#endif
#ifndef NO_TAGSTACK
	{ "tagstack",	"tgs",	BOOL,	CANSET,		o_tagstack	},
#endif
	{ "term",	"te",	STR,	SET,		o_term		},
#ifndef CRUNCH
	{ "terse",	"tr",	BOOL,	CANSET,		o_terse		},
	{ "timeout",	"to",	BOOL,	CANSET,		o_keytime	},
#endif
#ifndef CRUNCH
	{ "window",	"wi",	NUM,	CANSET|MR|WSET,	o_window	},
	{ "wrapmargin",	"wm",	NUM,	CANSET,		o_wrapmargin	},
#endif
	{ "wrapscan",	"ws",	BOOL,	CANSET,		o_wrapscan	},
#ifndef CRUNCH
	{ "writeany",	"wr",	BOOL,	CANSET,		o_writeany	},
#endif
	{ NULL, NULL, 0, CANSET, NULL }
};


/* This function initializes certain options from environment variables, etc. */
void initopts()
{
	char	*val;
	int	i;

	/* set some stuff from environment variables */
#if MSDOS
	if (val = getenv("COMSPEC")) /* yes, ASSIGNMENT! */
#else
	if (val = getenv("SHELL")) /* yes, ASSIGNMENT! */
#endif
	{
		strcpy(o_shell, val);
	}

	strcpy(o_term, termtype);
#if MSDOS
	if (strcmp(termtype, "pcbios"))
	{
		o_pcbios[0] = FALSE;
	}
	else
	{
		o_pcbios[0] = TRUE;
	}
#endif

#if AMIGA || MSDOS || TOS
	if ((val = getenv("TMP")) /* yes, ASSIGNMENT! */
	||  (val = getenv("TEMP")))
		strcpy(o_directory, val);
#endif

#ifndef CRUNCH
	if ((val = getenv("LINES")) && atoi(val) > 4) /* yes, ASSIGNMENT! */
	{
		LINES = atoi(val);
	}
	if ((val = getenv("COLUMNS")) && atoi(val) > 30) /* yes, ASSIGNMENT! */
	{
		COLS = atoi(val);
	}
#endif
	*o_lines = LINES;
	*o_columns = COLS;
	*o_scroll = LINES / 2 - 1;
#ifndef CRUNCH
	if (o_window[0] == 0)
	{
		o_window[0] = o_window[2] = *o_lines;
	}
	*o_nearscroll = *o_lines;
#endif

	/* disable the flash option if we don't know how to do a flash */
	if (!has_VB)
	{
		for (i = 0; opts[i].value != o_flash; i++)
		{
		}
		opts[i].flags &= ~CANSET;
		*o_flash = FALSE;
	}

#ifndef NO_DIGRAPH
# ifdef CS_LATIN1
	for (i = 0, val = o_flipcase; i < 32; i++)
	{
		/* leave out the multiply/divide symbols */
		if (i == 23)
			continue;

		/* add lower/uppercase pair */
		*val++ = i + 0xe0;
		*val++ = i + 0xc0;
	}
	*val = '\0';
# endif /* CS_LATIN1 */

	/* initialize the ctype package */
	_ct_init(o_flipcase);
#else
	_ct_init("");
#endif /* not NO_DIGRAPH */
}

/* This function lists the current values of all options */
void dumpopts(all)
	int	all;	/* boolean: dump all options, or just set ones? */
{
#ifndef NO_OPTCOLS
	int	i, j, k;
	char	nbuf[4];	/* used for converting numbers to ASCII */
	int	widths[5];	/* width of each column, including gap */
	int	ncols;		/* number of columns */
	int	nrows;		/* number of options per column */
	int	nset;		/* number of options to be output */
	int	width;		/* width of a particular option */
	int	todump[60];	/* indicies of options to be dumped */

	/* step 1: count the number of set options */
	for (nset = i = 0; opts[i].name; i++)
	{
		if (all || (opts[i].flags & SET))
		{
			todump[nset++] = i;
		}
	}

	/* step two: try to use as many columns as possible */
	for (ncols = (nset > 5 ? 5 : nset); ncols > 1; ncols--)
	{
		/* how many would go in this column? */
		nrows = (nset + ncols - 1) / ncols;

		/* figure out the width of each column */
		for (i = 0; i < ncols; i++)
		{
			widths[i] = 0;
			for (j = 0, k = nrows * i; j < nrows && k < nset; j++, k++)
			{
				/* figure out the width of a particular option */
				switch (opts[todump[k]].type)
				{
				  case BOOL:
					if (!*opts[todump[k]].value)
						width = 2;
					else
						width = 0;
					break;

				  case STR:
					width = 3 + strlen(opts[todump[k]].value);
					if (width > MAXWIDTH)
						width = MAXWIDTH;
					break;

				  case NUM:
					width = 4;
					break;
				}
				width += strlen(opts[todump[k]].name);

				/* if this is the widest so far, widen col */
				if (width > widths[i])
				{
					widths[i] = width;
				}
			}

		}

		/* if the total width is narrow enough, then use it */
		for (width = -2, i = 0; i < ncols; i++)
		{
			width += widths[i] + 2;
		}
		if (width < COLS - 1)
		{
			break;
		}
	}

	/* step 3: output the columns */
	nrows = (nset + ncols - 1) / ncols;
	for (i = 0; i < nrows; i++)
	{
		for (j = 0; j < ncols; j++)
		{
			/* if we hit the end of the options, quit */
			k = i + j * nrows;
			if (k >= nset)
			{
				break;
			}

			/* output this option's value */
			width = 0;
			switch (opts[todump[k]].type)
			{
			  case BOOL:
				if (!*opts[todump[k]].value)
				{
					qaddch('n');
					qaddch('o');
					width = 2;
				}
				qaddstr(opts[todump[k]].name);
				width += strlen(opts[todump[k]].name);
				break;

			  case NUM:
				sprintf(nbuf, "%-3d", UCHAR(*opts[todump[k]].value));
				qaddstr(opts[todump[k]].name);
				qaddch('=');
				qaddstr(nbuf);
				width = 4 + strlen(opts[todump[k]].name);
				break;

			  case STR:
				qaddstr(opts[todump[k]].name);
				qaddch('=');
				qaddch('"');
				strcpy(tmpblk.c, opts[todump[k]].value);
				width = 3 + strlen(tmpblk.c);
				if (width > MAXWIDTH)
				{
					width = MAXWIDTH;
					strcpy(tmpblk.c + MAXWIDTH - 6, "...");
				}
				qaddstr(tmpblk.c);
				qaddch('"');
				width += strlen(opts[todump[k]].name);
				break;
			}

			/* pad the field to the correct size */
			if (k + nrows <= nset)
			{
				while (width < widths[j] + 2)
				{
					qaddch(' ');
					width++;
				}
			}
		}
		addch('\n');
		exrefresh();
	}
#else
	int	i;
	int	col;
	char	nbuf[4];

	for (i = col = 0; opts[i].name; i++)
	{
		/* if not set and not all, ignore this option */
		if (!all && !(opts[i].flags & SET))
		{
			continue;
		}

		/* align this option in one of the columns */
		if (col > 52)
		{
			addch('\n');
			col = 0;
		}
		else if (col > 26)
		{
			while (col < 52)
			{
				qaddch(' ');
				col++;
			}
		}
		else if (col > 0)
		{
			while (col < 26)
			{
				qaddch(' ');
				col++;
			}
		}

		switch (opts[i].type)
		{
		  case BOOL:
			if (!*opts[i].value)
			{
				qaddch('n');
				qaddch('o');
				col += 2;
			}
			qaddstr(opts[i].name);
			col += strlen(opts[i].name);
			break;

		  case NUM:
			sprintf(nbuf, "%-3d", UCHAR(*opts[i].value));
			qaddstr(opts[i].name);
			qaddch('=');
			qaddstr(nbuf);
			col += 4 + strlen(opts[i].name);
			break;

		  case STR:
			qaddstr(opts[i].name);
			qaddch('=');
			qaddch('"');
			qaddstr(opts[i].value);
			qaddch('"');
			col += 3 + strlen(opts[i].name) + strlen(opts[i].value);
			break;
		}
		exrefresh();
	}
	if (col > 0)
	{
		addch('\n');
		exrefresh();
	}
#endif
}

#ifndef NO_MKEXRC
/* This function saves the current configuration of options to a file */
void saveopts(fd)
	int	fd;	/* file descriptor to write to */
{
	int	i;
	char	buf[256], *pos;

	/* write each set options */
	for (i = 0; opts[i].name; i++)
	{
		/* if unset or unsettable, ignore this option */
		if ((opts[i].flags & (SET|CANSET|NOSAVE)) != (SET|CANSET))
		{
			continue;
		}

		strcpy(buf, "set ");
		pos = &buf[4];
		switch (opts[i].type)
		{
		  case BOOL:
			if (!*opts[i].value)
			{
				*pos++='n';
				*pos++='o';
			}
			strcpy(pos, opts[i].name);
			strcat(pos, "\n");
			break;

		  case NUM:
			sprintf(pos, "%s=%-3d\n", opts[i].name, *opts[i].value & 0xff);
			break;

		  case STR:
			sprintf(pos, "%s=\"%s\"\n", opts[i].name, opts[i].value);
			break;
		}
		twrite(fd, buf, (unsigned)strlen(buf));
	}
}
#endif


/* This function changes the values of one or more options. */
void setopts(assignments)
	char	*assignments;	/* a string containing option assignments */
{
	char	*name;		/* name of variable in assignments */
	char	*value;		/* value of the variable */
	char	*scan;		/* used for moving through strings */
	char	*build;		/* used for copying chars from "scan" */
	char	*prefix;	/* pointer to "neg" or "no" at front of a boolean */
	int	quote;		/* boolean: inside '"' quotes? */
	int	i, j;

#ifndef CRUNCH
	/* reset the upper limit of "window" option to lines-1 */
	*o_window = *o_lines - 1;
#endif

	/* for each assignment... */
	for (name = assignments; *name; )
	{
		/* skip whitespace */
		if (*name == ' ' || *name == '\t')
		{
			name++;
			continue;
		}

		/* after the name, find the value (if any) */
		for (scan = name; isalnum(*scan); scan++)
		{
		}
		if (*scan == '=')
		{
			*scan++ = '\0';
			value = build = scan;
			for (quote = FALSE; *scan && (quote || !isspace(*scan)); scan++)
			{
				if (*scan == '"')
				{
					quote = !quote;
				}
				else if (*scan == '\\' && scan[1])
				{
					*build++ = *++scan;
				}
				else
				{
					*build++ = *scan;
				}
			}
			if (*scan)
				scan++;
			*build = '\0';
		}
		else /* no "=" so it is probably boolean... */
		{
			if (*scan)
			{
				*scan++ = '\0';
			}
			value = NULL;
			prefix = name;
#ifndef CRUNCH
			if (!strcmp(name, "novice"))
				/* don't check for a "no" prefix */;
			else
#endif
			if (prefix[0] == 'n' && prefix[1] == 'o')
				name += 2;
			else if (prefix[0] == 'n' && prefix[1] == 'e' && prefix[2] == 'g')
				name += 3;
		}

		/* find the variable */
		for (i = 0;
		     opts[i].name && strcmp(opts[i].name, name) && strcmp(opts[i].nm, name);
		     i++)
		{
		}

		/* change the variable */
		if (!opts[i].name)
		{
			/* only complain about unknown options if we're editing
			 * a file;  i.e., if we're not executing the .exrc now.
			 */
			if (tmpfd >= 0)
				msg("invalid option name \"%s\"", name);
		}
		else if ((opts[i].flags & CANSET) != CANSET)
		{
			msg("option \"%s\" can't be altered", name);
		}
		else if ((opts[i].flags & RCSET) != CANSET && nlines >= 1L)
		{
			msg("option \"%s\" can only be set in a %s file", name, EXRC);
		}
		else if (value)
		{
			switch (opts[i].type)
			{
			  case BOOL:
				msg("option \"[no]%s\" is boolean", name);
				break;

			  case NUM:
				j = atoi(value);
				if (j == 0 && *value != '0')
				{
					msg("option \"%s\" must have a numeric value", name);
				}
				else if (j < opts[i].value[1] || j > (opts[i].value[2] & 0xff))
				{
					msg("option \"%s\" must have a value between %d and %d",
						name, opts[i].value[1], opts[i].value[2] & 0xff);
				}
				else
				{
					*opts[i].value = atoi(value);
					opts[i].flags |= SET;
				}
				break;

			  case STR:
				strcpy(opts[i].value, value);
				opts[i].flags |= SET;
				break;
			}
			if (opts[i].flags & MR)
			{
				redraw(MARK_UNSET, FALSE);
			}
#ifndef CRUNCH
			if (opts[i].flags & WSET)
			{
				wset = TRUE;
			}
#endif
		}
		else /* valid option, no value */
		{
			if (opts[i].type == BOOL)
			{
				if (prefix == name)
					*opts[i].value = TRUE;
				else if (prefix[1] == 'o')
					*opts[i].value = FALSE;
				else
					*opts[i].value = !*opts[i].value;

				opts[i].flags |= SET;
				if (opts[i].flags & MR)
				{
					redraw(MARK_UNSET, FALSE);
				}
			}
			else
			{
				msg("option \"%s\" must be given a value", name);
			}
		}

		/* move on to the next option */
		name = scan;
	}

	/* special processing ... */

#ifndef CRUNCH
	/* if "novice" is set, then ":set report=1 showmode nomagic" */
	if (*o_novice)
	{
		*o_report = 1;
# ifndef NO_SHOWMODE
		*o_smd = TRUE;
# endif
# ifndef NO_MAGIC
		*o_magic = FALSE;
# endif
	}
#endif

	/* if "readonly" then set the READONLY flag for this file */
	if (*o_readonly)
	{
		setflag(file, READONLY);
	}

#ifndef NO_DIGRAPH
	/* re-initialize the ctype package */
	_ct_init(o_flipcase);
#endif /* not NO_DIGRAPH */

	/* copy o_lines and o_columns into LINES and COLS */
	LINES = (*o_lines & 255);
	COLS = (*o_columns & 255);
}
