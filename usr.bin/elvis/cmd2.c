/* cmd2.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains some of the commands - mostly ones that change text */

#include "config.h"
#include "ctype.h"
#include "vi.h"
#ifdef REGEX
# include <regex.h>
#else
# include "regexp.h"
#endif
#if TOS
# include <stat.h>
#else
# if OSK
#  include "osk.h"
# else
#  if AMIGA
#   include "amistat.h"
#  else
#   include <sys/stat.h>
#  endif
# endif
#endif

#ifdef REGEX
int patlock = 0;	 /* lock substitute pattern */
#endif

/*ARGSUSED*/
void cmd_substitute(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;	/* rest of the command line */
{
	char	*line;	/* a line from the file */
#ifdef REGEX
	static regex_t *ore = NULL;	/* old regex */
	regex_t *optpat();
	regex_t *re = NULL;
	regmatch_t rm[SE_MAX];
	char *startp, *endp;
	int n;
#else
	regexp	*re;	/* the compiled search expression */
#endif
	char	*eol;
	char	*subst;	/* the substitution string */
	static char *osubst;
	char	*opt;	/* substitution options */
	long	l;	/* a line number */
	char	*s, *d;	/* used during subtitutions */
	char	*conf;	/* used during confirmation */
	long	chline;	/* # of lines changed */
	long	chsub;	/* # of substitutions made */
	static	optp;	/* boolean option: print when done? */
	static	optg;	/* boolean option: substitute globally in line? */
	static	optc;	/* boolean option: confirm before subst? */
#ifndef CRUNCH
	long	oldnlines;
#endif


	/* for now, assume this will fail */
	rptlines = -1L;

	if (cmd == CMD_SUBAGAIN || !*extra)
	{
#ifndef NO_MAGIC
		if (*o_magic)
			subst = "~";
		else
#endif
		subst = "\\~";
#ifdef REGEX
		/* get the previous substitute pattern; not necessarily
		 * the previous pattern.
		 */
		if ((re = ore) == NULL)
			msg("RE error: no previous pattern");
#else
		re = regcomp("");
#endif

		/* if visual "&", then turn off the "p" and "c" options */
		if (bang)
		{
			optp = optc = FALSE;
		}
	}
	else /* CMD_SUBSTITUTE */
	{
		/* make sure we got a search pattern */
		if (*extra == ' ' || *extra == '\n')
		{
			msg("Usage: s/regular expression/new text/");
			return;
		}

		/* parse & compile the search pattern */
		subst = parseptrn(extra);
#ifdef REGEX
		if (re = optpat(extra + 1))
			patlock = 1;
		else
			return;
		if (re != ore && ore) {
			regfree(ore);
			free(ore);
		}
		ore = re;
#else
		re = regcomp(extra + 1);
#endif
	}

	/* abort if RE error -- error message already given by regcomp() */
	if (!re)
	{
		return;
	}

	if (cmd == CMD_SUBSTITUTE)
	{
		/* parse the substitution string & find the option string */
		for (opt = subst; *opt && *opt != *extra; opt++)
		{
			if (*opt == '\\' && opt[1])
			{
				opt++;
			}
		}
		if (*opt)
		{
			*opt++ = '\0';
		}

		/* analyse the option string */
		if (!*o_edcompatible)
		{
			optp = optg = optc = FALSE;
		}
		while (*opt)
		{
			switch (*opt++)
			{
			  case 'p':	optp = !optp;	break;
			  case 'g':	optg = !optg;	break;
			  case 'c':	optc = !optc;	break;
			  case ' ':
			  case '\t':			break;
			  default:
				msg("Subst options are p, c, and g -- not %c", opt[-1]);
				return;
			}
		}
	}

	/* if "c" or "p" flag was given, and we're in visual mode, then NEWLINE */
	if ((optc || optp) && mode == MODE_VI)
	{
		addch('\n');
		exrefresh();
	}

	ChangeText
	{
		/* reset the change counters */
		chline = chsub = 0L;

		/* for each selected line */
		for (l = markline(frommark); l <= markline(tomark); l++)
		{
			/* fetch the line */
			line = fetchline(l);
			eol = line + strlen(line);

			/* if it contains the search pattern... */
#ifdef REGEX
			if (!regexec(re, line, SE_MAX, rm, 0))
#else
			if (regexec(re, line, TRUE))
#endif
			{
				/* increment the line change counter */
				chline++;

				/* initialize the pointers */
				s = line;
				d = tmpblk.c;

				/* do once or globally ... */
				do
				{
#ifdef REGEX
					startp = s + rm[0].rm_so;
					endp = s + rm[0].rm_eo;
#endif
#ifndef CRUNCH
					/* confirm, if necessary */
					if (optc)
					{
#ifdef REGEX
						for (conf = line; conf < startp; conf++)
#else
						for (conf = line; conf < re->startp[0]; conf++)
#endif
							addch(*conf);
						standout();
#ifdef REGEX
						for ( ; conf < endp; conf++)
#else
						for ( ; conf < re->endp[0]; conf++)
#endif
							addch(*conf);
						standend();
						for (; *conf; conf++)
							addch(*conf);
						addch('\n');
						exrefresh();
						if (getkey(0) != 'y')
						{
							/* copy accross the original chars */
#ifdef REGEX
							while (s <  endp)
#else
							while (s < re->endp[0])
#endif
								*d++ = *s++;

							/* skip to next match on this line, if any */
							goto Continue;
						}
					}
#endif /* not CRUNCH */

					/* increment the substitution change counter */
					chsub++;

					/* copy stuff from before the match */
#ifdef REGEX
					while (s < startp)
#else
					while (s < re->startp[0])
#endif
					{
						*d++ = *s++;
					}

					/* substitute for the matched part */
#ifdef REGEX
					regsub(rm, startp, endp, subst, d);
#else
					regsub(re, subst, d);
#endif
#ifdef REGEX
					s = endp;
#else
					s = re->endp[0];
#endif
					d += strlen(d);

Continue:
;
#ifndef REGEX
					/* if this regexp could conceivably match
					 * a zero-length string, then require at
					 * least 1 unmatched character between
					 * matches.
					 */
					if (re->minlen == 0)
					{
						if (!*s)
							break;
						*d++ = *s++;
					}
#endif

				} 
#ifdef REGEX
				while (*s && optg && rm[0].rm_eo && !regexec(re, s, SE_MAX, rm, REG_NOTBOL));
				if (eol - s > 0 && !rm[0].rm_eo && optg) {
					msg("RE error: line too long");
					return;
				}
#else
				while (optg && regexec(re, s, FALSE));
#endif

				/* copy stuff from after the match */
				while (*d++ = *s++)	/* yes, ASSIGNMENT! */
				{
				}

#ifndef CRUNCH
				/* NOTE: since the substitution text is allowed to have ^Ms which are
				 * translated into newlines, it is possible that the number of lines
				 * in the file will increase after each line has been substituted.
				 * we need to adjust for this.
				 */
				oldnlines = nlines;
#endif

				/* replace the old version of the line with the new */
				d[-1] = '\n';
				d[0] = '\0';
				change(MARK_AT_LINE(l), MARK_AT_LINE(l + 1), tmpblk.c);

#ifndef CRUNCH
				l += nlines - oldnlines;
				tomark += MARK_AT_LINE(nlines - oldnlines);
#endif

				/* if supposed to print it, do so */
				if (optp)
				{
					addstr(tmpblk.c);
					exrefresh();
				}

				/* move the cursor to that line */
				cursor = MARK_AT_LINE(l);
			}
		}
	}

	/* free the regexp */
#ifndef REGEX
	_free_(re);
#endif

	/* if done from within a ":g" command, then finish silently */
	if (doingglobal)
	{
		rptlines = chline;
		rptlabel = "changed";
		return;
	}

	/* Reporting */
	if (chsub == 0)
	{
		msg("Substitution failed");
	}
	else if (chline >= *o_report)
	{
		msg("%ld substitutions on %ld lines", chsub, chline);
	}
	rptlines = 0L;
}




/*ARGSUSED*/
void cmd_delete(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	MARK	curs2;	/* an altered form of the cursor */

	/* choose your cut buffer */
	if (*extra == '"')
	{
		extra++;
	}
	if (*extra)
	{
		cutname(*extra);
	}

	/* make sure we're talking about whole lines here */
	frommark = frommark & ~(BLKSIZE - 1);
	tomark = (tomark & ~(BLKSIZE - 1)) + BLKSIZE;

	/* yank the lines */
	cut(frommark, tomark);

	/* if CMD_DELETE then delete the lines */
	if (cmd != CMD_YANK)
	{
		curs2 = cursor;
		ChangeText
		{
			/* delete the lines */
			delete(frommark, tomark);
		}
		if (curs2 > tomark)
		{
			cursor = curs2 - tomark + frommark;
		}
		else if (curs2 > frommark)
		{
			cursor = frommark;
		}
	}
}


/*ARGSUSED*/
void cmd_append(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	long	l;	/* line counter */

#ifndef CRUNCH
	/* if '!' then toggle auto-indent */
	if (bang)
	{
		*o_autoindent = !*o_autoindent;
	}
#endif

	ChangeText
	{
		/* if we're doing a change, delete the old version */
		if (cmd == CMD_CHANGE)
		{
			/* delete 'em */
			cmd_delete(frommark, tomark, cmd, bang, extra);
		}

		/* new lines start at the frommark line, or after it */
		l = markline(frommark);
		if (cmd == CMD_APPEND)
		{
 			l++;
		}

		/* get lines until no more lines, or "." line, and insert them */
		while (vgets('\0', tmpblk.c, BLKSIZE) >= 0)
		{
			addch('\n');
			if (!strcmp(tmpblk.c, "."))
			{
				break;
			}

			strcat(tmpblk.c, "\n");
			add(MARK_AT_LINE(l), tmpblk.c);
			l++;
		}
	}

	/* on the odd chance that we're calling this from vi mode ... */
	redraw(MARK_UNSET, FALSE);
}


/*ARGSUSED*/
void cmd_put(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	/* choose your cut buffer */
	if (*extra == '"')
	{
		extra++;
	}
	if (*extra)
	{
		cutname(*extra);
	}

	/* paste it */
	ChangeText
	{
		cursor = paste(frommark, TRUE, FALSE);
	}
}


/*ARGSUSED*/
void cmd_join(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	long	l;
	char	*scan;
	int	len;	/* length of the new line */

	/* if only one line is specified, assume the following one joins too */
	if (markline(frommark) == nlines)
	{
		msg("Nothing to join with this line");
		return;
	}
	if (markline(frommark) == markline(tomark))
	{
		tomark += BLKSIZE;
	}

	/* get the first line */
	l = markline(frommark);
	strcpy(tmpblk.c, fetchline(l));
	len = strlen(tmpblk.c);

	/* build the longer line */
	while (++l <= markline(tomark))
	{
		/* get the next line */
		scan = fetchline(l);

		/* remove any leading whitespace */
		while (*scan == '\t' || *scan == ' ')
		{
			scan++;
		}

		/* see if the line will fit */
		if (strlen(scan) + len + 3 > (unsigned)BLKSIZE)
		{
			msg("Can't join -- the resulting line would be too long");
			return;
		}

		/* catenate it, with a space (or two) in between */
		if (!bang)
		{
			if (len >= 1)
			{
				if (tmpblk.c[len - 1] == '.'
				 || tmpblk.c[len - 1] == '?'
				 || tmpblk.c[len - 1] == '!')
				{
					 tmpblk.c[len++] = ' ';
					 tmpblk.c[len++] = ' ';
				}
				else if (tmpblk.c[len - 1] != ' ')
				{
					 tmpblk.c[len++] = ' ';
				}
			}
		}
		strcpy(tmpblk.c + len, scan);
		len += strlen(scan);
	}
	tmpblk.c[len++] = '\n';
	tmpblk.c[len] = '\0';

	/* make the change */
	ChangeText
	{
		frommark &= ~(BLKSIZE - 1);
		tomark &= ~(BLKSIZE - 1);
		tomark += BLKSIZE;
		change(frommark, tomark, tmpblk.c);
	}

	/* Reporting... */
	rptlines = markline(tomark) - markline(frommark) - 1L;
	rptlabel = "joined";
}



/*ARGSUSED*/
void cmd_shift(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	long	l;	/* line number counter */
	int	oldidx;	/* number of chars previously used for indent */
	int	newidx;	/* number of chars in the new indent string */
	int	oldcol;	/* previous indent amount */
	int	newcol;	/* new indent amount */
	char	*text;	/* pointer to the old line's text */

	ChangeText
	{
		/* for each line to shift... */
		for (l = markline(frommark); l <= markline(tomark); l++)
		{
			/* get the line - ignore empty lines unless ! mode */
			text = fetchline(l);
			if (!*text && !bang)
				continue;

			/* calc oldidx and oldcol */
			for (oldidx = 0, oldcol = 0;
			     text[oldidx] == ' ' || text[oldidx] == '\t';
			     oldidx++)
			{
				if (text[oldidx] == ' ')
				{
					oldcol += 1;
				}
				else
				{
					oldcol += *o_tabstop - (oldcol % *o_tabstop);
				}
			}

			/* calc newcol */
			if (cmd == CMD_SHIFTR)
			{
				newcol = oldcol + (*o_shiftwidth & 0xff);
			}
			else
			{
				newcol = oldcol - (*o_shiftwidth & 0xff);
				if (newcol < 0)
					newcol = 0;
			}

			/* if no change, then skip to next line */
			if (oldcol == newcol)
				continue;

			/* build a new indent string */
			newidx = 0;
			if (*o_autotab)
			{
				while (newcol >= *o_tabstop)
				{
					tmpblk.c[newidx++] = '\t';
					newcol -= *o_tabstop;
				}
			}
			while (newcol > 0)
			{
				tmpblk.c[newidx++] = ' ';
				newcol--;
			}
			tmpblk.c[newidx] = '\0';

			/* change the old indent string into the new */
			change(MARK_AT_LINE(l), MARK_AT_LINE(l) + oldidx, tmpblk.c);
		}
	}

	/* Reporting... */
	rptlines = markline(tomark) - markline(frommark) + 1L;
	if (cmd == CMD_SHIFTR)
	{
		rptlabel = ">ed";
	}
	else
	{
		rptlabel = "<ed";
	}
}


/*ARGSUSED*/
void cmd_read(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	int	fd, rc;	/* used while reading from the file */
	char	*scan;	/* used for finding NUL characters */
	int	hadnul;	/* boolean: any NULs found? */
	int	addnl;	/* boolean: forced to add newlines? */
	int	len;	/* number of chars in current line */
	long	lines;	/* number of lines in current block */
	struct stat statb;

	/* special case: if ":r !cmd" then let the filter() function do it */
	if (extra[0] == '!')
	{
		filter(frommark, MARK_UNSET, extra + 1, TRUE);
		return;
	}

	/* open the file */
	fd = open(extra, O_RDONLY);
	if (fd < 0)
	{
		msg("Can't open \"%s\"", extra);
		return;
	}

#ifndef CRUNCH
	if (stat(extra, &statb) < 0)
	{
		msg("Can't stat \"%s\"", extra);
	}
# if TOS
	if (statb.st_mode & S_IJDIR)
# else
#  if OSK
	if (statb.st_mode & S_IFDIR)
#  else
	if ((statb.st_mode & S_IFMT) != S_IFREG)
#  endif
# endif
	{
		msg("\"%s\" is not a regular file", extra);
		return;
	}
#endif /* not CRUNCH */

	/* get blocks from the file, and add them */
	ChangeText
	{
		/* insertion starts at the line following frommark */
		tomark = frommark = (frommark | (BLKSIZE - 1L)) + 1L;
		len = 0;
		hadnul = addnl = FALSE;

		/* add an extra newline, so partial lines at the end of
		 * the file don't trip us up
		 */
		add(tomark, "\n");

		/* for each chunk of text... */
		while ((rc = tread(fd, tmpblk.c, BLKSIZE - 1)) > 0)
		{
			/* count newlines, convert NULs, etc. ... */
			for (lines = 0, scan = tmpblk.c; rc > 0; rc--, scan++)
			{
				/* break up long lines */
				if (*scan != '\n' && len + 2 > BLKSIZE)
				{
					*scan = '\n';
					addnl = TRUE;
				}

				/* protect against NUL chars in file */
				if (!*scan)
				{
					*scan = 0x80;
					hadnul = TRUE;
				}

				/* starting a new line? */
				if (*scan == '\n')
				{
					/* reset length at newline */
					len = 0;
					lines++;
				}
				else
				{
					len++;
				}
			}

			/* add the text */
			*scan = '\0';
			add(tomark, tmpblk.c);
			tomark += MARK_AT_LINE(lines) + len - markidx(tomark);
		}

		/* if partial last line, then retain that first newline */
		if (len > 0)
		{
			msg("Last line had no newline");
			tomark += BLKSIZE; /* <- for the rptlines calc */
		}
		else /* delete that first newline */
		{
			delete(tomark, (tomark | (BLKSIZE - 1L)) + 1L);
		}
	}

	/* close the file */
	close(fd);

	/* Reporting... */
	rptlines = markline(tomark) - markline(frommark);
	rptlabel = "read";
	if (mode == MODE_EX)
	{
		cursor = (tomark & ~BLKSIZE) - BLKSIZE;
	}
	else
	{
		cursor = frommark;
	}

	if (addnl)
		msg("Newlines were added to break up long lines");
	if (hadnul)
		msg("NULs were converted to 0x80");
}



/*ARGSUSED*/
void cmd_undo(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	undo();
}


/* print the selected lines */
/*ARGSUSED*/
void cmd_print(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	REG char	*scan;
	REG long	l;
	REG int		col;

	for (l = markline(frommark); l <= markline(tomark); l++)
	{
		/* display a line number, if CMD_NUMBER */
		if (cmd == CMD_NUMBER)
		{
			sprintf(tmpblk.c, "%6ld  ", l);
			qaddstr(tmpblk.c);
			col = 8;
		}
		else
		{
			col = 0;
		}

		/* get the next line & display it */
		for (scan = fetchline(l); *scan; scan++)
		{
			/* expand tabs to the proper width */
			if (*scan == '\t' && cmd != CMD_LIST)
			{
				do
				{
					qaddch(' ');
					col++;
				} while (col % *o_tabstop != 0);
			}
			else if (*scan >= 1 && *scan < ' ' || *scan == '\177')
			{
				qaddch('^');
				qaddch(*scan ^ 0x40);
				col += 2;
			}
			else if ((*scan & 0x80) && cmd == CMD_LIST)
			{
				sprintf(tmpblk.c, "\\%03o", UCHAR(*scan));
				qaddstr(tmpblk.c);
				col += 4;
			}
			else
			{
				qaddch(*scan);
				col++;
			}

			/* wrap at the edge of the screen */
			if (!has_AM && col >= COLS)
			{
				addch('\n');
				col -= COLS;
			}
		}
		if (cmd == CMD_LIST)
		{
			qaddch('$');
		}
		addch('\n');
		exrefresh();
	}

	/* leave the cursor on the last line printed */
	cursor = tomark;
}


/* move or copy selected lines */
/*ARGSUSED*/
void cmd_move(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	MARK	destmark;

	/* parse the destination linespec.  No defaults.  Line 0 is okay */
	destmark = cursor;
	if (!strcmp(extra, "0"))
	{
		destmark = 0L;
	}
	else if (linespec(extra, &destmark) == extra || !destmark)
	{
		msg("invalid destination address");
		return;
	}

	/* flesh the marks out to encompass whole lines */
	frommark &= ~(BLKSIZE - 1);
	tomark = (tomark & ~(BLKSIZE - 1)) + BLKSIZE;
	destmark = (destmark & ~(BLKSIZE - 1)) + BLKSIZE;

	/* make sure the destination is valid */
	if (cmd == CMD_MOVE && destmark >= frommark && destmark < tomark)
	{
		msg("invalid destination address");
	}

	/* Do it */
	ChangeText
	{
		/* save the text to a cut buffer */
		cutname('\0');
		cut(frommark, tomark);

		/* if we're not copying, delete the old text & adjust destmark */
		if (cmd != CMD_COPY)
		{
			delete(frommark, tomark);
			if (destmark >= frommark)
			{
				destmark -= (tomark - frommark);
			}
		}

		/* add the new text */
		paste(destmark, FALSE, FALSE);
	}

	/* move the cursor to the last line of the moved text */
	cursor = destmark + (tomark - frommark) - BLKSIZE;
	if (cursor < MARK_FIRST || cursor >= MARK_LAST + BLKSIZE)
	{
		cursor = MARK_LAST;
	}

	/* Reporting... */
	rptlabel = ( (cmd == CMD_COPY) ? "copied" : "moved" );
}



/* execute EX commands from a file */
/*ARGSUSED*/
void cmd_source(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	/* must have a filename */
	if (!*extra)
	{
		msg("\"source\" requires a filename");
		return;
	}

	doexrc(extra);
}


#ifndef NO_AT
/*ARGSUSED*/
void cmd_at(frommark, tomark, cmd, bang, extra)
	MARK	frommark;
	MARK	tomark;
	CMD	cmd;
	int	bang;
	char	*extra;
{
	static	nest = FALSE;
	int	result;
	char	buf[MAXRCLEN];

	/* don't allow nested macros */
	if (nest)
	{
		msg("@ macros can't be nested");
		return;
	}
	nest = TRUE;

	/* require a buffer name */
	if (*extra == '"')
		extra++;
	if (!*extra || !isascii(*extra) ||!islower(*extra))
	{
		msg("@ requires a cut buffer name (a-z)");
	}

	/* get the contents of the buffer */
	result = cb2str(*extra, buf, (unsigned)(sizeof buf));
	if (result <= 0)
	{
		msg("buffer \"%c is empty", *extra);
	}
	else if (result >= sizeof buf)
	{
		msg("buffer \"%c is too large to execute", *extra);
	}
	else
	{
		/* execute the contents of the buffer as ex commands */
		exstring(buf, result, '\\');
	}

	nest = FALSE;
}
#endif
