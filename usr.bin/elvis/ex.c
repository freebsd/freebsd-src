/* ex.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains the code for reading ex commands. */

#include "config.h"
#include "ctype.h"
#include "vi.h"

/* This data type is used to describe the possible argument combinations */
typedef short ARGT;
#define FROM	1		/* allow a linespec */
#define	TO	2		/* allow a second linespec */
#define BANG	4		/* allow a ! after the command name */
#define EXTRA	8		/* allow extra args after command name */
#define XFILE	16		/* expand wildcards in extra part */
#define NOSPC	32		/* no spaces allowed in the extra part */
#define	DFLALL	64		/* default file range is 1,$ */
#define DFLNONE	128		/* no default file range */
#define NODFL	256		/* do not default to the current file name */
#define EXRCOK	512		/* can be in a .exrc file */
#define NL	1024		/* if mode!=MODE_EX, then write a newline first */
#define PLUS	2048		/* allow a line number, as in ":e +32 foo" */
#define ZERO	4096		/* allow 0 to be given as a line number */
#define NOBAR	8192		/* treat following '|' chars as normal */
#define FILES	(XFILE + EXTRA)	/* multiple extra files allowed */
#define WORD1	(EXTRA + NOSPC)	/* one extra word allowed */
#define FILE1	(FILES + NOSPC)	/* 1 file allowed, defaults to current file */
#define NAMEDF	(FILE1 + NODFL)	/* 1 file allowed, defaults to "" */
#define NAMEDFS	(FILES + NODFL)	/* multiple files allowed, default is "" */
#define RANGE	(FROM + TO)	/* range of linespecs allowed */
#define NONE	0		/* no args allowed at all */

/* This array maps ex command names to command codes. The order in which
 * command names are listed below is significant -- ambiguous abbreviations
 * are always resolved to be the first possible match.  (e.g. "r" is taken
 * to mean "read", not "rewind", because "read" comes before "rewind")
 */
static struct
{
	char	*name;	/* name of the command */
	CMD	code;	/* enum code of the command */
	void	(*fn)();/* function which executes the command */
	ARGT	argt;	/* command line arguments permitted/needed/used */
}
	cmdnames[] =
{   /*	cmd name	cmd code	function	arguments */
	{"print",	CMD_PRINT,	cmd_print,	RANGE+NL	},

	{"append",	CMD_APPEND,	cmd_append,	FROM+ZERO+BANG	},
#ifdef DEBUG
	{"bug",		CMD_DEBUG,	cmd_debug,	RANGE+BANG+EXTRA+NL},
#endif
	{"change",	CMD_CHANGE,	cmd_append,	RANGE+BANG	},
	{"delete",	CMD_DELETE,	cmd_delete,	RANGE+WORD1	},
	{"edit",	CMD_EDIT,	cmd_edit,	BANG+FILE1+PLUS	},
	{"file",	CMD_FILE,	cmd_file,	NAMEDF		},
	{"global",	CMD_GLOBAL,	cmd_global,	RANGE+BANG+EXTRA+DFLALL+NOBAR},
	{"insert",	CMD_INSERT,	cmd_append,	FROM+BANG	},
	{"join",	CMD_INSERT,	cmd_join,	RANGE+BANG	},
	{"k",		CMD_MARK,	cmd_mark,	FROM+WORD1	},
	{"list",	CMD_LIST,	cmd_print,	RANGE+NL	},
	{"move",	CMD_MOVE,	cmd_move,	RANGE+EXTRA	},
	{"next",	CMD_NEXT,	cmd_next,	BANG+NAMEDFS	},
	{"Next",	CMD_PREVIOUS,	cmd_next,	BANG		},
	{"quit",	CMD_QUIT,	cmd_xit,	BANG		},
	{"read",	CMD_READ,	cmd_read,	FROM+ZERO+NAMEDF},
	{"substitute",	CMD_SUBSTITUTE,	cmd_substitute,	RANGE+EXTRA	},
	{"to",		CMD_COPY,	cmd_move,	RANGE+EXTRA	},
	{"undo",	CMD_UNDO,	cmd_undo,	NONE		},
	{"vglobal",	CMD_VGLOBAL,	cmd_global,	RANGE+EXTRA+DFLALL+NOBAR},
	{"write",	CMD_WRITE,	cmd_write,	RANGE+BANG+FILE1+DFLALL},
	{"xit",		CMD_XIT,	cmd_xit,	BANG+NL		},
	{"yank",	CMD_YANK,	cmd_delete,	RANGE+WORD1	},

	{"!",		CMD_BANG,	cmd_shell,	EXRCOK+RANGE+NAMEDFS+DFLNONE+NL+NOBAR},
	{"#",		CMD_NUMBER,	cmd_print,	RANGE+NL	},
	{"<",		CMD_SHIFTL,	cmd_shift,	RANGE		},
	{">",		CMD_SHIFTR,	cmd_shift,	RANGE		},
	{"=",		CMD_EQUAL,	cmd_file,	RANGE		},
	{"&",		CMD_SUBAGAIN,	cmd_substitute,	RANGE		},
#ifndef NO_AT
	{"@",		CMD_AT,		cmd_at,		EXTRA		},
#endif

#ifndef NO_ABBR
	{"abbreviate",	CMD_ABBR,	cmd_map,	EXRCOK+BANG+EXTRA},
#endif
	{"args",	CMD_ARGS,	cmd_args,	EXRCOK+NAMEDFS	},
#ifndef NO_ERRLIST
	{"cc",		CMD_CC,		cmd_make,	BANG+FILES	},
#endif
	{"cd",		CMD_CD,		cmd_cd,		EXRCOK+BANG+NAMEDF},
	{"copy",	CMD_COPY,	cmd_move,	RANGE+EXTRA	},
#ifndef NO_DIGRAPH
	{"digraph",	CMD_DIGRAPH,	cmd_digraph,	EXRCOK+BANG+EXTRA},
#endif
#ifndef NO_ERRLIST
	{"errlist",	CMD_ERRLIST,	cmd_errlist,	BANG+NAMEDF	},
#endif
	{"ex",		CMD_EDIT,	cmd_edit,	BANG+FILE1	},
	{"mark",	CMD_MARK,	cmd_mark,	FROM+WORD1	},
#ifndef NO_MKEXRC
	{"mkexrc",	CMD_MKEXRC,	cmd_mkexrc,	NAMEDF		},
#endif
	{"number",	CMD_NUMBER,	cmd_print,	RANGE+NL	},
#ifndef NO_TAGSTACK
	{"pop",		CMD_POP,	cmd_pop,	BANG+WORD1	},
#endif
	{"put",		CMD_PUT,	cmd_put,	FROM+ZERO+WORD1	},
	{"set",		CMD_SET,	cmd_set,	EXRCOK+EXTRA	},
	{"shell",	CMD_SHELL,	cmd_shell,	NL		},
	{"source",	CMD_SOURCE,	cmd_source,	EXRCOK+NAMEDF	},
#ifdef SIGTSTP
	{"stop",	CMD_STOP,	cmd_suspend,	NONE		},
#endif
	{"tag",		CMD_TAG,	cmd_tag,	BANG+WORD1	},
	{"version",	CMD_VERSION,	cmd_version,	EXRCOK+NONE	},
	{"visual",	CMD_VISUAL,	cmd_edit,	BANG+NAMEDF	},
	{"wq",		CMD_WQUIT,	cmd_xit,	NL		},

#ifdef DEBUG
	{"debug",	CMD_DEBUG,	cmd_debug,	RANGE+BANG+EXTRA+NL},
	{"validate",	CMD_VALIDATE,	cmd_validate,	BANG+NL		},
#endif
	{"chdir",	CMD_CD,		cmd_cd,		EXRCOK+BANG+NAMEDF},
#ifndef NO_COLOR
	{"color",	CMD_COLOR,	cmd_color,	EXRCOK+EXTRA	},
#endif
#ifndef NO_ERRLIST
	{"make",	CMD_MAKE,	cmd_make,	BANG+NAMEDFS	},
#endif
	{"map",		CMD_MAP,	cmd_map,	EXRCOK+BANG+EXTRA},
	{"previous",	CMD_PREVIOUS,	cmd_next,	BANG		},
	{"rewind",	CMD_REWIND,	cmd_next,	BANG		},
#ifdef SIGTSTP
	{"suspend",	CMD_SUSPEND,	cmd_suspend,	NONE		},
#endif
	{"unmap",	CMD_UNMAP,	cmd_map,	EXRCOK+BANG+EXTRA},
#ifndef NO_ABBR
	{"unabbreviate",CMD_UNABBR,	cmd_map,	EXRCOK+WORD1	},
#endif

	{(char *)0}
};


/* This function parses a search pattern - given a pointer to a / or ?,
 * it replaces the ending / or ? with a \0, and returns a pointer to the
 * stuff that came after the pattern.
 */
char	*parseptrn(ptrn)
	REG char	*ptrn;
{
	REG char 	*scan;

	for (scan = ptrn + 1;
	     *scan && *scan != *ptrn;
	     scan++)
	{
		/* allow backslashed versions of / and ? in the pattern */
		if (*scan == '\\' && scan[1] != '\0')
		{
			scan++;
		}
	}
	if (*scan)
	{
		*scan++ = '\0';
	}

	return scan;
}


/* This function parses a line specifier for ex commands */
char *linespec(s, markptr)
	REG char	*s;		/* start of the line specifier */
	MARK		*markptr;	/* where to store the mark's value */
{
	long		num;
	REG char	*t;

	/* parse each ;-delimited clause of this linespec */
	do
	{
		/* skip an initial ';', if any */
		if (*s == ';')
		{
			s++;
		}

		/* skip leading spaces */
		while (isspace(*s))
		{
			s++;
		}

		/* dot means current position */
		if (*s == '.')
		{
			s++;
			*markptr = cursor;
		}
		/* '$' means the last line */
		else if (*s == '$')
		{
			s++;
			*markptr = MARK_LAST;
		}
		/* digit means an absolute line number */
		else if (isdigit(*s))
		{
			for (num = 0; isdigit(*s); s++)
			{
				num = num * 10 + *s - '0';
			}
			*markptr = MARK_AT_LINE(num);
		}
		/* appostrophe means go to a set mark */
		else if (*s == '\'')
		{
			s++;
			*markptr = m_tomark(cursor, 1L, (int)*s);
			s++;
		}
		/* slash means do a search */
		else if (*s == '/' || *s == '?')
		{
			/* put a '\0' at the end of the search pattern */
			t = parseptrn(s);

			/* search for the pattern */
			*markptr &= ~(BLKSIZE - 1);
			if (*s == '/')
			{
				pfetch(markline(*markptr));
				if (plen > 0)
					*markptr += plen - 1;
				*markptr = m_fsrch(*markptr, s);
			}
			else
			{
				*markptr = m_bsrch(*markptr, s);
			}

			/* adjust command string pointer */
			s = t;
		}

		/* if linespec was faulty, quit now */
		if (!*markptr)
		{
			return s;
		}

		/* maybe add an offset */
		t = s;
		if (*t == '-' || *t == '+')
		{
			s++;
			for (num = 0; isdigit(*s); s++)
			{
				num = num * 10 + *s - '0';
			}
			if (num == 0)
			{
				num = 1;
			}
			*markptr = m_updnto(*markptr, num, *t);
		}
	} while (*s == ';' || *s == '+' || *s == '-');

	/* protect against invalid line numbers */
	num = markline(*markptr);
	if (num < 1L || num > nlines)
	{
		msg("Invalid line number -- must be from 1 to %ld", nlines);
		*markptr = MARK_UNSET;
	}

	return s;
}



/* This function reads an ex command and executes it. */
void ex()
{
	char		cmdbuf[150];
	REG int		cmdlen;
	static long	oldline;

	significant = FALSE;
	oldline = markline(cursor);

	while (mode == MODE_EX)
	{
		/* read a line */
#ifdef CRUNCH
		cmdlen = vgets(':', cmdbuf, sizeof(cmdbuf));
#else
		cmdlen = vgets(*o_prompt ? ':' : '\0', cmdbuf, sizeof(cmdbuf));
#endif
		if (cmdlen < 0)
		{
			return;
		}

		/* if empty line, assume ".+1" */
		if (cmdlen == 0)
		{
			strcpy(cmdbuf, ".+1");
			qaddch('\r');
			clrtoeol();
		}
		else
		{
			addch('\n');
		}
		refresh();

		/* parse & execute the command */
		doexcmd(cmdbuf);

		/* handle autoprint */
		if (significant || markline(cursor) != oldline)
		{
			significant = FALSE;
			oldline = markline(cursor);
			if (*o_autoprint && mode == MODE_EX)
			{
				cmd_print(cursor, cursor, CMD_PRINT, FALSE, "");
			}
		}
	}
}

void doexcmd(cmdbuf)
	char		*cmdbuf;	/* string containing an ex command */
{
	REG char	*scan;		/* used to scan thru cmdbuf */
	MARK		frommark;	/* first linespec */
	MARK		tomark;		/* second linespec */
	REG int		cmdlen;		/* length of the command name given */
	CMD		cmd;		/* what command is this? */
	ARGT		argt;		/* argument types for this command */
	short		forceit;	/* bang version of a command? */
	REG int		cmdidx;		/* index of command */
	REG char	*build;		/* used while copying filenames */
	int		iswild;		/* boolean: filenames use wildcards? */
	int		isdfl;		/* using default line ranges? */
	int		didsub;		/* did we substitute file names for % or # */

	/* ex commands can't be undone via the shift-U command */
	U_line = 0L;

	/* permit extra colons at the start of the line */
	for (; *cmdbuf == ':'; cmdbuf++)
	{
	}

	/* ignore command lines that start with a double-quote */
	if (*cmdbuf == '"')
	{
		return;
	}
	scan = cmdbuf;

	/* parse the line specifier */
	if (nlines < 1)
	{
		/* no file, so don't allow addresses */
	}
	else if (*scan == '%')
	{
		/* '%' means all lines */
		frommark = MARK_FIRST;
		tomark = MARK_LAST;
		scan++;
	}
	else if (*scan == '0')
	{
		scan++;
		frommark = tomark = (*scan ? MARK_UNSET : MARK_FIRST);
	}
	else
	{
		frommark = cursor;
		scan = linespec(scan, &frommark);
		tomark = frommark;
		if (frommark && *scan == ',')
		{
			scan++;
			scan = linespec(scan, &tomark);
		}
		if (!tomark)
		{
			/* faulty line spec -- fault already described */
			return;
		}
		if (frommark > tomark)
		{
			msg("first address exceeds the second");
			return;
		}
	}
	isdfl = (scan == cmdbuf);

	/* skip whitespace */
	while (isspace(*scan))
	{
		scan++;
	}

	/* Figure out how long the command name is.  If no command, then the
	 * length is 0, which will match the "print" command.
	 */ 
	if (!*scan)
	{
		/* if not in ex mode, and both endpoints are at the line,
		 * then just move to the start of that line without printing
		 */
		if (mode != MODE_EX && frommark == tomark)
		{
			if (tomark != MARK_UNSET)
				cursor = tomark;
			return;
		}
		cmdlen = 0;
	}
	else if (!isalpha(*scan))
	{
		cmdlen = 1;
	}
	else
	{
		for (cmdlen = 1;
		     isalpha(scan[cmdlen]);
		     cmdlen++)
		{
		}
	}

	/* lookup the command code */
	for (cmdidx = 0;
	     cmdnames[cmdidx].name && strncmp(scan, cmdnames[cmdidx].name, cmdlen);
	     cmdidx++)
	{
	}
	argt = cmdnames[cmdidx].argt;
	cmd = cmdnames[cmdidx].code;
	if (cmd == CMD_NULL)
	{
		msg("Unknown command \"%.*s\"", cmdlen, scan);
		return;
	}

	/* !!! if the command doesn't have NOBAR set, then replace | with \0 */

	/* if the command ended with a bang, set the forceit flag */
	scan += cmdlen;
	if ((argt & BANG) && *scan == '!')
	{
		scan++;
		forceit = 1;
	}
	else
	{
		forceit = 0;
	}

	/* skip any more whitespace, to leave scan pointing to arguments */
	while (isspace(*scan))
	{
		scan++;
	}

	/* a couple of special cases for filenames */
	if (argt & XFILE)
	{
		/* if names were given, process them */
		if (*scan)
		{
			for (build = tmpblk.c, iswild = didsub = FALSE; *scan; scan++)
			{
				switch (*scan)
				{
				  case '\\':
					if (scan[1] == '\\' || scan[1] == '%' || scan[1] == '#')
					{
						*build++ = *++scan;
					}
					else
					{
						*build++ = '\\';
					}
					break;

				  case '%':
					if (!*origname)
					{
						msg("No filename to substitute for %%");
						return;
					}
					strcpy(build, origname);
					while (*build)
					{
						build++;
					}
					didsub = TRUE;
					break;

				  case '#':
					if (!*prevorig)
					{
						msg("No filename to substitute for #");
						return;
					}
					strcpy(build, prevorig);
					while (*build)
					{
						build++;
					}
					didsub = TRUE;
					break;

				  case '*':
				  case '?':
#if !(MSDOS || TOS)
				  case '[':
				  case '`':
				  case '{': /* } */
				  case '$':
				  case '~':
#endif
					*build++ = *scan;
					iswild = TRUE;
					break;

				  default:
					*build++ = *scan;
				}
			}
			*build = '\0';

			if (cmd == CMD_BANG
			 || cmd == CMD_READ && tmpblk.c[0] == '!'
			 || cmd == CMD_WRITE && tmpblk.c[0] == '!')
			{
				if (didsub)
				{
					if (mode != MODE_EX)
					{
						addch('\n');
					}
					addstr(tmpblk.c);
					addch('\n');
					exrefresh();
				}
			}
			else
			{
				if (iswild && tmpblk.c[0] != '>')
				{
					scan = wildcard(tmpblk.c);
				}
			}
		}
		else /* no names given, maybe assume origname */
		{
			if (!(argt & NODFL))
			{
				strcpy(tmpblk.c, origname);
			}
			else
			{
				*tmpblk.c = '\0';
			}
		}

		scan = tmpblk.c;
	}

	/* bad arguments? */
	if (!(argt & EXRCOK) && nlines < 1L)
	{
		msg("Can't use the \"%s\" command in a %s file", cmdnames[cmdidx].name, EXRC);
		return;
	}
	if (!(argt & (ZERO | EXRCOK)) && frommark == MARK_UNSET)
	{
		msg("Can't use address 0 with \"%s\" command.", cmdnames[cmdidx].name);
		return;
	}
	if (!(argt & FROM) && frommark != cursor && nlines >= 1L)
	{
		msg("Can't use address with \"%s\" command.", cmdnames[cmdidx].name);
		return;
	}
	if (!(argt & TO) && tomark != frommark && nlines >= 1L)
	{
		msg("Can't use a range with \"%s\" command.", cmdnames[cmdidx].name);
		return;
	}
	if (!(argt & EXTRA) && *scan)
	{
		msg("Extra characters after \"%s\" command.", cmdnames[cmdidx].name);
		return;
	}
	if ((argt & NOSPC) && !(cmd == CMD_READ && (forceit || *scan == '!')))
	{
		build = scan;
#ifndef CRUNCH
		if ((argt & PLUS) && *build == '+')
		{
			while (*build && !isspace(*build))
			{
				build++;
			}
			while (*build && isspace(*build))
			{
				build++;
			}
		}
#endif /* not CRUNCH */
		for (; *build; build++)
		{
			if (isspace(*build))
			{
				msg("Too many %s to \"%s\" command.",
					(argt & XFILE) ? "filenames" : "arguments",
					cmdnames[cmdidx].name);
				return;
			}
		}
	}

	/* some commands have special default ranges */
	if (isdfl && (argt & DFLALL))
	{
		frommark = MARK_FIRST;
		tomark = MARK_LAST;
	}
	else if (isdfl && (argt & DFLNONE))
	{
		frommark = tomark = 0L;
	}

	/* write a newline if called from visual mode */
	if ((argt & NL) && mode != MODE_EX && !exwrote)
	{
		addch('\n');
		exrefresh();
	}

	/* act on the command */
	(*cmdnames[cmdidx].fn)(frommark, tomark, cmd, forceit, scan);
}


/* This function executes EX commands from a file.  It returns 1 normally, or
 * 0 if the file could not be opened for reading.
 */
int doexrc(filename)
	char	*filename;	/* name of a ".exrc" file */
{
	int	fd;		/* file descriptor */
	int	len;		/* length of the ".exrc" file */

#ifdef CRUNCH
	/* small address space - we need to conserve space */

	/* !!! kludge: we use U_text as the buffer.  This has the side-effect
	 * of interfering with the shift-U visual command.  Disable shift-U.
	 */
	U_line = 0L;
#else
# if TINYSTACK
#  if TOS || MINT
	/* small stack, but big heap.  Allocate buffer from heap */
	char	*U_text = (char *)malloc(4096);
	if (!U_text)
	{
		return 0;
	}
#  else
	/* small stack - we need to conserve space */

	/* !!! kludge: we use U_text as the buffer.  This has the side-effect
	 * of interfering with the shift-U visual command.  Disable shift-U.
	 */
	U_line = 0L;
#  endif
# else
	/* This is how we would *like* to do it -- with a large buffer on the
	 * stack, so we can handle large .exrc files and also recursion.
	 */
	char	U_text[4096];
# endif
#endif

	/* open the file, read it, and close */
	fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
#if TINYSTACK && (TOS || MINT)
		free(U_text);
#endif
		return 0;
	}
#if TINYSTACK && (TOS || MINT)
	len = tread(fd, U_text, 4096);
#else
	len = tread(fd, U_text, sizeof U_text);
#endif
	close(fd);

	/* execute the string */
	exstring(U_text, len, ctrl('V'));

#if TINYSTACK && (TOS || MINT)
	free(U_text);
#endif
	return 1;
}

/* This function executes EX commands from a string.  The commands may be
 * separated by newlines or by | characters.  It also handles quoting.
 * Each individual command is limited to 132 bytes, but the total string
 * may be longer.
 */
void exstring(buf, len, qchar)
	char	*buf;	/* the commands to execute */
	int	len;	/* the length of the string */
	int	qchar;	/* the quote character -- ^V for file, or \ for kbd */
{
	char	single[133];	/* a single command */
	char	*src, *dest;
	int	i;

	/* find & do each command */
	for (src = buf; src < &buf[len]; src++)
	{
		/* Copy a single command into single[].  Convert any quoted |
		 * into a normal |, and stop at a newline or unquoted |.
		 */
		for (dest = single, i = 0;
		     i < 132 && src < &buf[len] && *src != '\n' && *src != '|';
		     src++, i++)
		{
			if (src[0] == qchar && src[1] == '|')
			{
				src++;
			}
			*dest++ = *src;
		}
		*dest = '\0';

		/* do it */
		doexcmd(single);
	}
}
