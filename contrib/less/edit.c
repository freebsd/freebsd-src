/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


#include "less.h"
#include "position.h"
#if HAVE_STAT
#include <sys/stat.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if OS2 || __MVS__ || (defined WIFSIGNALED && defined WTERMSIG)
#include <signal.h>
#endif

public int fd0 = 0;

extern lbool new_file;
extern char *every_first_cmd;
extern int force_open;
extern int is_tty;
extern int sigs;
extern int hshift;
extern int want_filesize;
extern int consecutive_nulls;
extern int modelines;
extern int show_preproc_error;
extern IFILE curr_ifile;
extern IFILE old_ifile;
extern struct scrpos initial_scrpos;
extern void *ml_examine;
extern POSITION soft_eof;
#if SPACES_IN_FILENAMES
extern char openquote;
extern char closequote;
#endif

#if LOGFILE
extern int logfile;
extern int force_logfile;
extern char *namelogfile;
#endif

#if HAVE_STAT_INO
public dev_t curr_dev;
public ino_t curr_ino;
#endif

/*
 * Textlist functions deal with a list of words separated by spaces.
 * init_textlist sets up a textlist structure.
 * forw_textlist uses that structure to iterate thru the list of
 * words, returning each one as a standard null-terminated string.
 * back_textlist does the same, but runs thru the list backwards.
 */
public void init_textlist(struct textlist *tlist, mutable char *str)
{
	char *s;
#if SPACES_IN_FILENAMES
	lbool meta_quoted = FALSE;
	lbool delim_quoted = FALSE;
	constant char *esc = get_meta_escape();
	size_t esclen = strlen(esc);
#endif
	
	tlist->string = skipsp(str);
	tlist->endstring = tlist->string + strlen(tlist->string);
	for (s = str;  s < tlist->endstring;  s++)
	{
#if SPACES_IN_FILENAMES
		if (meta_quoted)
		{
			meta_quoted = 0;
		} else if (esclen > 0 && s + esclen < tlist->endstring &&
		           strncmp(s, esc, esclen) == 0)
		{
			meta_quoted = 1;
			s += esclen - 1;
		} else if (delim_quoted)
		{
			if (*s == closequote)
				delim_quoted = 0;
		} else /* (!delim_quoted) */
		{
			if (*s == openquote)
				delim_quoted = 1;
			else if (*s == ' ')
				*s = '\0';
		}
#else
		if (*s == ' ')
			*s = '\0';
#endif
	}
}

public constant char * forw_textlist(struct textlist *tlist, constant char *prev)
{
	constant char *s;
	
	/*
	 * prev == NULL means return the first word in the list.
	 * Otherwise, return the word after "prev".
	 */
	if (prev == NULL)
		s = tlist->string;
	else
		s = prev + strlen(prev);
	if (s >= tlist->endstring)
		return (NULL);
	while (*s == '\0')
		s++;
	if (s >= tlist->endstring)
		return (NULL);
	return (s);
}

public constant char * back_textlist(struct textlist *tlist, constant char *prev)
{
	constant char *s;
	
	/*
	 * prev == NULL means return the last word in the list.
	 * Otherwise, return the word before "prev".
	 */
	if (prev == NULL)
		s = tlist->endstring;
	else if (prev <= tlist->string)
		return (NULL);
	else
		s = prev - 1;
	while (*s == '\0')
		s--;
	if (s <= tlist->string)
		return (NULL);
	while (s[-1] != '\0' && s > tlist->string)
		s--;
	return (s);
}

/*
 * Parse a single option setting in a modeline.
 */
static void modeline_option(constant char *str, size_t opt_len)
{
	struct mloption { constant char *opt_name; void (*opt_func)(constant char*,size_t); };
	struct mloption options[] = {
		{ "ts=",         set_tabs },
		{ "tabstop=",    set_tabs },
		{ NULL, NULL }
	};
	struct mloption *opt;
	for (opt = options;  opt->opt_name != NULL;  opt++)
	{
		size_t name_len = strlen(opt->opt_name);
		if (opt_len > name_len && strncmp(str, opt->opt_name, name_len) == 0)
		{
			(*opt->opt_func)(str + name_len, opt_len - name_len);
			break;
		}
	}
}

/*
 * String length, terminated by option separator (space or colon).
 * Space/colon can be escaped with backspace.
 */
static size_t modeline_option_len(constant char *str)
{
	lbool esc = FALSE;
	constant char *s;
	for (s = str;  *s != '\0';  s++)
	{
		if (esc)
			esc = FALSE;
		else if (*s == '\\')
			esc = TRUE;
		else if (*s == ' ' || *s == ':') /* separator */
			break;
	}
	return ptr_diff(s, str);
}

/*
 * Parse colon- or space-separated option settings in a modeline.
 */
static void modeline_options(constant char *str, char end_char)
{
	for (;;)
	{
		size_t opt_len;
		str = skipspc(str);
		if (*str == '\0' || *str == end_char)
			break;
		opt_len = modeline_option_len(str);
		modeline_option(str, opt_len);
		str += opt_len;
		if (*str != '\0')
			str += 1; /* skip past the separator */
	}
}

/*
 * See if there is a modeline string in a line.
 */
static void check_modeline(constant char *line)
{
#if HAVE_STRSTR
	static constant char *pgms[] = { "less:", "vim:", "vi:", "ex:", NULL };
	constant char **pgm;
	for (pgm = pgms;  *pgm != NULL;  ++pgm)
	{
		constant char *pline = line;
		for (;;)
		{
			constant char *str;
			pline = strstr(pline, *pgm);
			if (pline == NULL) /* pgm is not in this line */
				break;
			str = skipspc(pline + strlen(*pgm));
			if (pline == line || pline[-1] == ' ')
			{
				if (strncmp(str, "set ", 4) == 0)
					modeline_options(str+4, ':');
				else if (pgm != &pgms[0]) /* "less:" requires "set" */
					modeline_options(str, '\0');
				break;
			}
			/* Continue searching the rest of the line. */
			pline = str;
		}
	}
#endif /* HAVE_STRSTR */
}

/*
 * Read lines from start of file and check if any are modelines.
 */
static void check_modelines(void)
{
	POSITION pos = ch_zero();
	int i;
	for (i = 0;  i < modelines;  i++)
	{
		constant char *line;
		size_t line_len;
		if (ABORT_SIGS())
			return;
		pos = forw_raw_line(pos, &line, &line_len);
		if (pos == NULL_POSITION)
			break;
		check_modeline(line);
	}
}

/*
 * Close a pipe opened via popen.
 */
static void close_pipe(FILE *pipefd)
{
	int status;
	char *p;
	PARG parg;

	if (pipefd == NULL)
		return;
#if OS2
	/*
	 * The pclose function of OS/2 emx sometimes fails.
	 * Send SIGINT to the piped process before closing it.
	 */
	kill(pipefd->_pid, SIGINT);
#endif
	status = pclose(pipefd);
	if (status == -1)
	{
		/* An internal error in 'less', not a preprocessor error.  */
		p = errno_message("pclose");
		parg.p_string = p;
		error("%s", &parg);
		free(p);
		return;
	}
	if (!show_preproc_error)
		return;
#if defined WIFEXITED && defined WEXITSTATUS
	if (WIFEXITED(status))
	{
		int s = WEXITSTATUS(status);
		if (s != 0)
		{
			parg.p_int = s;
			error("Input preprocessor failed (status %d)", &parg);
		}
		return;
	}
#endif
#if defined WIFSIGNALED && defined WTERMSIG
	if (WIFSIGNALED(status))
	{
		int sig = WTERMSIG(status);
		if (sig != SIGPIPE || ch_length() != NULL_POSITION)
		{
			parg.p_string = signal_message(sig);
			error("Input preprocessor terminated: %s", &parg);
		}
		return;
	}
#endif
	if (status != 0)
	{
		parg.p_int = status;
		error("Input preprocessor exited with status %x", &parg);
	}
}

/*
 * Drain and close an input pipe if needed.
 */
public void close_altpipe(IFILE ifile)
{
	FILE *altpipe = get_altpipe(ifile);
	if (altpipe != NULL && !(ch_getflags() & CH_KEEPOPEN))
	{
		close_pipe(altpipe);
		set_altpipe(ifile, NULL);
	}
}

/*
 * Check for error status from the current altpipe.
 * May or may not close the pipe.
 */
public void check_altpipe_error(void)
{
	if (!show_preproc_error)
		return;
	if (curr_ifile != NULL_IFILE && get_altfilename(curr_ifile) != NULL)
		close_altpipe(curr_ifile);
}

/*
 * Close the current input file.
 */
static void close_file(void)
{
	struct scrpos scrpos;
	constant char *altfilename;
	
	if (curr_ifile == NULL_IFILE)
		return;

	/*
	 * Save the current position so that we can return to
	 * the same position if we edit this file again.
	 */
	get_scrpos(&scrpos, TOP);
	if (scrpos.pos != NULL_POSITION)
	{
		store_pos(curr_ifile, &scrpos);
		lastmark();
	}
	/*
	 * Close the file descriptor, unless it is a pipe.
	 */
	ch_close();
	/*
	 * If we opened a file using an alternate name,
	 * do special stuff to close it.
	 */
	altfilename = get_altfilename(curr_ifile);
	if (altfilename != NULL)
	{
		close_altpipe(curr_ifile);
		close_altfile(altfilename, get_filename(curr_ifile));
		set_altfilename(curr_ifile, NULL);
	}
	curr_ifile = NULL_IFILE;
#if HAVE_STAT_INO
	curr_ino = curr_dev = 0;
#endif
}

/*
 * Edit a new file (given its name).
 * Filename == "-" means standard input.
 * Filename == NULL means just close the current file.
 */
public int edit(constant char *filename)
{
	if (filename == NULL)
		return (edit_ifile(NULL_IFILE));
	return (edit_ifile(get_ifile(filename, curr_ifile)));
}
	
/*
 * Clean up what edit_ifile did before error return.
 */
static int edit_error(constant char *filename, constant char *alt_filename, void *altpipe, IFILE ifile)
{
	if (alt_filename != NULL)
	{
		close_pipe(altpipe);
		close_altfile(alt_filename, filename);
		free((char*)alt_filename); /* FIXME: WTF? */
	}
	del_ifile(ifile);
	/*
	 * Re-open the current file.
	 */
	if (curr_ifile == ifile)
	{
		/*
		 * Whoops.  The "current" ifile is the one we just deleted.
		 * Just give up.
		 */
		quit(QUIT_ERROR);
	}
	return (1);
}

/*
 * Edit a new file (given its IFILE).
 * ifile == NULL means just close the current file.
 */
public int edit_ifile(IFILE ifile)
{
	int f;
	int answer;
	int chflags;
	constant char *filename;
	constant char *open_filename;
	char *alt_filename;
	void *altpipe;
	IFILE was_curr_ifile;
	char *p;
	PARG parg;
	ssize_t nread = 0;

	if (ifile == curr_ifile)
	{
		/*
		 * Already have the correct file open.
		 */
		return (0);
	}
	new_file = TRUE;

	if (ifile != NULL_IFILE)
	{
		/*
		 * See if LESSOPEN specifies an "alternate" file to open.
		 */
		filename = get_filename(ifile);
		altpipe = get_altpipe(ifile);
		if (altpipe != NULL)
		{
			/*
			 * File is already open.
			 * chflags and f are not used by ch_init if ifile has 
			 * filestate which should be the case if we're here. 
			 * Set them here to avoid uninitialized variable warnings.
			 */
			chflags = 0; 
			f = -1;
			alt_filename = get_altfilename(ifile);
			open_filename = (alt_filename != NULL) ? alt_filename : filename;
		} else
		{
			if (strcmp(filename, FAKE_HELPFILE) == 0 ||
				strcmp(filename, FAKE_EMPTYFILE) == 0)
				alt_filename = NULL;
			else
				alt_filename = open_altfile(filename, &f, &altpipe);

			open_filename = (alt_filename != NULL) ? alt_filename : filename;

			chflags = 0;
			if (altpipe != NULL)
			{
				/*
				 * The alternate "file" is actually a pipe.
				 * f has already been set to the file descriptor of the pipe
				 * in the call to open_altfile above.
				 * Keep the file descriptor open because it was opened 
				 * via popen(), and pclose() wants to close it.
				 */
				chflags |= CH_POPENED;
				if (strcmp(filename, "-") == 0)
					chflags |= CH_KEEPOPEN;
			} else if (strcmp(filename, "-") == 0)
			{
				/* 
				 * Use standard input.
				 * Keep the file descriptor open because we can't reopen it.
				 */
				f = fd0;
				chflags |= CH_KEEPOPEN;
				/*
				 * Must switch stdin to BINARY mode.
				 */
				SET_BINARY(f);
#if MSDOS_COMPILER==DJGPPC
				/*
				 * Setting stdin to binary by default causes
				 * Ctrl-C to not raise SIGINT.  We must undo
				 * that side-effect.
				 */
				__djgpp_set_ctrl_c(1);
#endif
			} else if (strcmp(open_filename, FAKE_EMPTYFILE) == 0)
			{
				f = -1;
				chflags |= CH_NODATA;
			} else if (strcmp(open_filename, FAKE_HELPFILE) == 0)
			{
				f = -1;
				chflags |= CH_HELPFILE;
			} else if ((p = bad_file(open_filename)) != NULL)
			{
				/*
				 * It looks like a bad file.  Don't try to open it.
				 */
				parg.p_string = p;
				error("%s", &parg);
				free(p);
				return edit_error(filename, alt_filename, altpipe, ifile);
			} else if ((f = iopen(open_filename, OPEN_READ)) < 0)
			{
				/*
				 * Got an error trying to open it.
				 */
				char *p = errno_message(filename);
				parg.p_string = p;
				error("%s", &parg);
				free(p);
				return edit_error(filename, alt_filename, altpipe, ifile);
			} else 
			{
				chflags |= CH_CANSEEK;
				if (bin_file(f, &nread) && !force_open && !opened(ifile))
				{
					/*
					 * Looks like a binary file.  
					 * Ask user if we should proceed.
					 */
					parg.p_string = filename;
					answer = query("\"%s\" may be a binary file.  See it anyway? ", &parg);
					if (answer != 'y' && answer != 'Y')
					{
						close(f);
						return edit_error(filename, alt_filename, altpipe, ifile);
					}
				}
			}
		}
		if (!force_open && f >= 0 && isatty(f))
		{
			PARG parg;
			parg.p_string = filename;
			error("%s is a terminal (use -f to open it)", &parg);
			return edit_error(filename, alt_filename, altpipe, ifile);
		}
	}

#if LOGFILE
	end_logfile();
#endif
	was_curr_ifile = save_curr_ifile();
	if (curr_ifile != NULL_IFILE)
	{
		int was_helpfile = (ch_getflags() & CH_HELPFILE);
		close_file();
		if (was_helpfile && held_ifile(was_curr_ifile) <= 1)
		{
			/*
			 * Don't keep the help file in the ifile list.
			 */
			del_ifile(was_curr_ifile);
			was_curr_ifile = NULL_IFILE;
		}
	}
	unsave_ifile(was_curr_ifile);

	if (ifile == NULL_IFILE)
	{
		/*
		 * No new file to open.
		 * (Don't set old_ifile, because if you call edit_ifile(NULL),
		 *  you're supposed to have saved curr_ifile yourself,
		 *  and you'll restore it if necessary.)
		 */
		return (0);
	}

	/*
	 * Set up the new ifile.
	 * Get the saved position for the file.
	 */
	curr_ifile = ifile;
	soft_eof = NULL_POSITION;
	set_altfilename(curr_ifile, alt_filename);
	set_altpipe(curr_ifile, altpipe);
	set_open(curr_ifile); /* File has been opened */
	get_pos(curr_ifile, &initial_scrpos);
	ch_init(f, chflags, nread);
	consecutive_nulls = 0;
	check_modelines();

	if (!(chflags & CH_HELPFILE))
	{
		if (was_curr_ifile != NULL_IFILE)
			old_ifile = was_curr_ifile;
#if LOGFILE
		if (namelogfile != NULL && is_tty)
			use_logfile(namelogfile);
#endif
#if HAVE_STAT_INO
		/* Remember the i-number and device of the opened file. */
		if (strcmp(open_filename, "-") != 0)
		{
			struct stat statbuf;
			int r = stat(open_filename, &statbuf);
			if (r == 0)
			{
				curr_ino = statbuf.st_ino;
				curr_dev = statbuf.st_dev;
			}
		}
#endif
		if (every_first_cmd != NULL)
		{
			ungetsc(every_first_cmd);
			ungetcc_end_command();
		}
	}

	flush();

	if (is_tty)
	{
		/*
		 * Output is to a real tty.
		 */

		/*
		 * Indicate there is nothing displayed yet.
		 */
		pos_clear();
		clr_linenum();
#if HILITE_SEARCH
		clr_hilite();
#endif
		undo_osc8();
		hshift = 0;
		if (strcmp(filename, FAKE_HELPFILE) && strcmp(filename, FAKE_EMPTYFILE))
		{
			char *qfilename = shell_quote(filename);
			cmd_addhist(ml_examine, qfilename, 1);
			free(qfilename);
		}
		if (want_filesize)
			scan_eof();
		set_header(ch_zero());
	}
	return (0);
}

/*
 * Edit a space-separated list of files.
 * For each filename in the list, enter it into the ifile list.
 * Then edit the first one.
 */
public int edit_list(char *filelist)
{
	IFILE save_ifile;
	constant char *good_filename;
	constant char *filename;
	char *gfilelist;
	constant char *gfilename;
	char *qfilename;
	struct textlist tl_files;
	struct textlist tl_gfiles;

	save_ifile = save_curr_ifile();
	good_filename = NULL;
	
	/*
	 * Run thru each filename in the list.
	 * Try to glob the filename.  
	 * If it doesn't expand, just try to open the filename.
	 * If it does expand, try to open each name in that list.
	 */
	init_textlist(&tl_files, filelist);
	filename = NULL;
	while ((filename = forw_textlist(&tl_files, filename)) != NULL)
	{
		gfilelist = lglob(filename);
		init_textlist(&tl_gfiles, gfilelist);
		gfilename = NULL;
		while ((gfilename = forw_textlist(&tl_gfiles, gfilename)) != NULL)
		{
			qfilename = shell_unquote(gfilename);
			if (edit(qfilename) == 0 && good_filename == NULL)
				good_filename = get_filename(curr_ifile);
			free(qfilename);
		}
		free(gfilelist);
	}
	/*
	 * Edit the first valid filename in the list.
	 */
	if (good_filename == NULL)
	{
		unsave_ifile(save_ifile);
		return (1);
	}
	if (get_ifile(good_filename, curr_ifile) == curr_ifile)
	{
		/*
		 * Trying to edit the current file; don't reopen it.
		 */
		unsave_ifile(save_ifile);
		return (0);
	}
	reedit_ifile(save_ifile);
	return (edit(good_filename));
}

/*
 * Edit the first file in the command line (ifile) list.
 */
public int edit_first(void)
{
	if (nifile() == 0)
		return (edit_stdin());
	curr_ifile = NULL_IFILE;
	return (edit_next(1));
}

/*
 * Edit the last file in the command line (ifile) list.
 */
public int edit_last(void)
{
	curr_ifile = NULL_IFILE;
	return (edit_prev(1));
}


/*
 * Edit the n-th next or previous file in the command line (ifile) list.
 */
static int edit_istep(IFILE h, int n, int dir)
{
	IFILE next;

	/*
	 * Skip n filenames, then try to edit each filename.
	 */
	for (;;)
	{
		next = (dir > 0) ? next_ifile(h) : prev_ifile(h);
		if (--n < 0)
		{
			if (edit_ifile(h) == 0)
				break;
		}
		if (next == NULL_IFILE)
		{
			/*
			 * Reached end of the ifile list.
			 */
			return (1);
		}
		if (ABORT_SIGS())
		{
			/*
			 * Interrupt breaks out, if we're in a long
			 * list of files that can't be opened.
			 */
			return (1);
		}
		h = next;
	} 
	/*
	 * Found a file that we can edit.
	 */
	return (0);
}

static int edit_inext(IFILE h, int n)
{
	return (edit_istep(h, n, +1));
}

public int edit_next(int n)
{
	return edit_istep(curr_ifile, n, +1);
}

static int edit_iprev(IFILE h, int n)
{
	return (edit_istep(h, n, -1));
}

public int edit_prev(int n)
{
	return edit_istep(curr_ifile, n, -1);
}

/*
 * Edit a specific file in the command line (ifile) list.
 */
public int edit_index(int n)
{
	IFILE h;

	h = NULL_IFILE;
	do
	{
		if ((h = next_ifile(h)) == NULL_IFILE)
		{
			/*
			 * Reached end of the list without finding it.
			 */
			return (1);
		}
	} while (get_index(h) != n);

	return (edit_ifile(h));
}

public IFILE save_curr_ifile(void)
{
	if (curr_ifile != NULL_IFILE)
		hold_ifile(curr_ifile, 1);
	return (curr_ifile);
}

public void unsave_ifile(IFILE save_ifile)
{
	if (save_ifile != NULL_IFILE)
		hold_ifile(save_ifile, -1);
}

/*
 * Reedit the ifile which was previously open.
 */
public void reedit_ifile(IFILE save_ifile)
{
	IFILE next;
	IFILE prev;

	/*
	 * Try to reopen the ifile.
	 * Note that opening it may fail (maybe the file was removed),
	 * in which case the ifile will be deleted from the list.
	 * So save the next and prev ifiles first.
	 */
	unsave_ifile(save_ifile);
	next = next_ifile(save_ifile);
	prev = prev_ifile(save_ifile);
	if (edit_ifile(save_ifile) == 0)
		return;
	/*
	 * If can't reopen it, open the next input file in the list.
	 */
	if (next != NULL_IFILE && edit_inext(next, 0) == 0)
		return;
	/*
	 * If can't open THAT one, open the previous input file in the list.
	 */
	if (prev != NULL_IFILE && edit_iprev(prev, 0) == 0)
		return;
	/*
	 * If can't even open that, we're stuck.  Just quit.
	 */
	quit(QUIT_ERROR);
}

public void reopen_curr_ifile(void)
{
	IFILE save_ifile = save_curr_ifile();
	close_file();
	reedit_ifile(save_ifile);
}

/*
 * Edit standard input.
 */
public int edit_stdin(void)
{
	if (isatty(fd0))
	{
		error("Missing filename (\"less --help\" for help)", NULL_PARG);
		quit(QUIT_OK);
	}
	return (edit("-"));
}

/*
 * Copy a file directly to standard output.
 * Used if standard output is not a tty.
 */
public void cat_file(void)
{
	int c;

	while ((c = ch_forw_get()) != EOI)
		putchr(c);
	flush();
}

#if LOGFILE

#define OVERWRITE_OPTIONS "Overwrite, Append, Don't log, or Quit?"

/*
 * If the user asked for a log file and our input file
 * is standard input, create the log file.  
 * We take care not to blindly overwrite an existing file.
 */
public void use_logfile(constant char *filename)
{
	int exists;
	int answer;
	PARG parg;

	if (ch_getflags() & CH_CANSEEK)
		/*
		 * Can't currently use a log file on a file that can seek.
		 */
		return;

	/*
	 * {{ We could use access() here. }}
	 */
	exists = open(filename, OPEN_READ);
	if (exists >= 0)
		close(exists);
	exists = (exists >= 0);

	/*
	 * Decide whether to overwrite the log file or append to it.
	 * If it doesn't exist we "overwrite" it.
	 */
	if (!exists || force_logfile)
	{
		/*
		 * Overwrite (or create) the log file.
		 */
		answer = 'O';
	} else
	{
		/*
		 * Ask user what to do.
		 */
		parg.p_string = filename;
		answer = query("Warning: \"%s\" exists; "OVERWRITE_OPTIONS" ", &parg);
	}

loop:
	switch (answer)
	{
	case 'O': case 'o':
		/*
		 * Overwrite: create the file.
		 */
		logfile = creat(filename, CREAT_RW);
		break;
	case 'A': case 'a':
		/*
		 * Append: open the file and seek to the end.
		 */
		logfile = open(filename, OPEN_APPEND);
		if (less_lseek(logfile, (less_off_t)0, SEEK_END) == BAD_LSEEK)
		{
			close(logfile);
			logfile = -1;
		}
		break;
	case 'D': case 'd':
		/*
		 * Don't do anything.
		 */
		return;
	default:
		/*
		 * Eh?
		 */

		answer = query(OVERWRITE_OPTIONS" (Type \"O\", \"A\", \"D\" or \"Q\") ", NULL_PARG);
		goto loop;
	}

	if (logfile < 0)
	{
		/*
		 * Error in opening logfile.
		 */
		parg.p_string = filename;
		error("Cannot write to \"%s\"", &parg);
		return;
	}
	SET_BINARY(logfile);
}

#endif
