/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Routines to mess around with filenames (and files).
 * Much of this is very OS dependent.
 */

#include "less.h"
#include "lglob.h"
#if MSDOS_COMPILER
#include <dos.h>
#if MSDOS_COMPILER==WIN32C && !defined(_MSC_VER)
#include <dir.h>
#endif
#if MSDOS_COMPILER==DJGPPC
#include <glob.h>
#include <dir.h>
#define _MAX_PATH      PATH_MAX
#endif
#endif
#ifdef _OSK
#include <rbf.h>
#ifndef _OSK_MWC32
#include <modes.h>
#endif
#endif

#if HAVE_STAT
#include <sys/stat.h>
#ifndef S_ISDIR
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)
#endif
#endif

extern int force_open;
extern int use_lessopen;
extern int ctldisp;
extern int utf_mode;
extern IFILE curr_ifile;
extern IFILE old_ifile;
#if SPACES_IN_FILENAMES
extern char openquote;
extern char closequote;
#endif
#if HAVE_STAT_INO
extern ino_t curr_ino;
extern dev_t curr_dev;
#endif

/*
 * Remove quotes around a filename.
 */
public char * shell_unquote(constant char *str)
{
	char *name;
	char *p;

	name = p = (char *) ecalloc(strlen(str)+1, sizeof(char));
	if (*str == openquote)
	{
		str++;
		while (*str != '\0')
		{
			if (*str == closequote)
			{
				if (str[1] != closequote)
					break;
				str++;
			}
			*p++ = *str++;
		}
	} else
	{
		constant char *esc = get_meta_escape();
		size_t esclen = strlen(esc);
		while (*str != '\0')
		{
			if (esclen > 0 && strncmp(str, esc, esclen) == 0)
				str += esclen;
			*p++ = *str++;
		}
	}
	*p = '\0';
	return (name);
}

/*
 * Get the shell's escape character.
 */
public constant char * get_meta_escape(void)
{
	constant char *s;

	s = lgetenv("LESSMETAESCAPE");
	if (s == NULL)
		s = DEF_METAESCAPE;
	return (s);
}

/*
 * Get the characters which the shell considers to be "metacharacters".
 */
static constant char * metachars(void)
{
	static constant char *mchars = NULL;

	if (mchars == NULL)
	{
		mchars = lgetenv("LESSMETACHARS");
		if (mchars == NULL)
			mchars = DEF_METACHARS;
	}
	return (mchars);
}

/*
 * Is this a shell metacharacter?
 */
static lbool metachar(char c)
{
	return (strchr(metachars(), c) != NULL);
}

/*
 * Must use quotes rather than escape char for this metachar?
 */
static lbool must_quote(char c)
{
	/* {{ Maybe the set of must_quote chars should be configurable? }} */
	return (c == '\n'); 
}

/*
 * Insert a backslash before each metacharacter in a string.
 */
public char * shell_quoten(constant char *s, size_t slen)
{
	constant char *p;
	char *np;
	char *newstr;
	size_t len;
	constant char *esc = get_meta_escape();
	size_t esclen = strlen(esc);
	lbool use_quotes = FALSE;
	lbool have_quotes = FALSE;

	/*
	 * Determine how big a string we need to allocate.
	 */
	len = 1; /* Trailing null byte */
	for (p = s;  p < s + slen;  p++)
	{
		len++;
		if (*p == openquote || *p == closequote)
			have_quotes = TRUE;
		if (metachar(*p))
		{
			if (esclen == 0)
			{
				/*
				 * We've got a metachar, but this shell 
				 * doesn't support escape chars.  Use quotes.
				 */
				use_quotes = TRUE;
			} else if (must_quote(*p))
			{
				len += 3; /* open quote + char + close quote */
			} else
			{
				/*
				 * Allow space for the escape char.
				 */
				len += esclen;
			}
		}
	}
	if (use_quotes)
	{
		if (have_quotes)
			/*
			 * We can't quote a string that contains quotes.
			 */
			return (NULL);
		len = slen + 3;
	}
	/*
	 * Allocate and construct the new string.
	 */
	newstr = np = (char *) ecalloc(len, sizeof(char));
	if (use_quotes)
	{
		SNPRINTF4(newstr, len, "%c%.*s%c", openquote, (int) slen, s, closequote);
	} else
	{
		constant char *es = s + slen;
		while (s < es)
		{
			if (!metachar(*s))
			{
				*np++ = *s++;
			} else if (must_quote(*s))
			{
				/* Surround the char with quotes. */
				*np++ = openquote;
				*np++ = *s++;
				*np++ = closequote;
			} else
			{
				/* Insert an escape char before the char. */
				strcpy(np, esc);
				np += esclen;
				*np++ = *s++;
			}
		}
		*np = '\0';
	}
	return (newstr);
}

public char * shell_quote(constant char *s)
{
	return shell_quoten(s, strlen(s));
}

/*
 * Return a pathname that points to a specified file in a specified directory.
 * Return NULL if the file does not exist in the directory.
 */
public char * dirfile(constant char *dirname, constant char *filename, int must_exist)
{
	char *pathname;
	size_t len;
	int f;

	if (dirname == NULL || *dirname == '\0')
		return (NULL);
	/*
	 * Construct the full pathname.
	 */
	len = strlen(dirname) + strlen(filename) + 2;
	pathname = (char *) calloc(len, sizeof(char));
	if (pathname == NULL)
		return (NULL);
	SNPRINTF3(pathname, len, "%s%s%s", dirname, PATHNAME_SEP, filename);
	if (must_exist)
	{
		/*
		 * Make sure the file exists.
		 */
		f = open(pathname, OPEN_READ);
		if (f < 0)
		{
			free(pathname);
			pathname = NULL;
		} else
		{
			close(f);
		}
	}
	return (pathname);
}

/*
 * Return the full pathname of the given file in the "home directory".
 */
public char * homefile(constant char *filename)
{
	char *pathname;

	/* Try $HOME/filename. */
	pathname = dirfile(lgetenv("HOME"), filename, 1);
	if (pathname != NULL)
		return (pathname);
#if OS2
	/* Try $INIT/filename. */
	pathname = dirfile(lgetenv("INIT"), filename, 1);
	if (pathname != NULL)
		return (pathname);
#endif
#if (MSDOS_COMPILER && MSDOS_COMPILER!=WIN32C) || OS2
	/* Look for the file anywhere on search path. */
	pathname = (char *) ecalloc(_MAX_PATH, sizeof(char));
#if MSDOS_COMPILER==DJGPPC
	{
		char *res = searchpath(filename);
		if (res == 0)
			*pathname = '\0';
		else
			strcpy(pathname, res);
	}
#else
	_searchenv(filename, "PATH", pathname);
#endif
	if (*pathname != '\0')
		return (pathname);
	free(pathname);
#endif
	return (NULL);
}

typedef struct xcpy { char *dest; size_t copied; } xcpy;

static void xcpy_char(xcpy *xp, char ch)
{
	if (xp->dest != NULL) *(xp->dest)++ = ch; 
	xp->copied++;
}

static void xcpy_filename(xcpy *xp, constant char *str)
{
	/* If filename contains spaces, quote it 
	 * to prevent edit_list from splitting it. */
	lbool quote = (strchr(str, ' ') != NULL);
	if (quote)
		xcpy_char(xp, openquote);
	for (;  *str != '\0';  str++)
		xcpy_char(xp, *str);
	if (quote)
		xcpy_char(xp, closequote);
}

static size_t fexpand_copy(constant char *fr, char *to)
{
	xcpy xp;
	xp.copied = 0;
	xp.dest = to;

	for (;  *fr != '\0';  fr++)
	{
		lbool expand = FALSE;
		switch (*fr)
		{
		case '%':
		case '#':
			if (fr[1] == *fr)
			{
				/* Two identical chars. Output just one. */
				fr += 1;
			} else 
			{
				/* Single char. Expand to a (quoted) file name. */
				expand = TRUE;
			}
			break;
		default:
			break;
		}
		if (expand)
		{
			IFILE ifile = (*fr == '%') ? curr_ifile : (*fr == '#') ? old_ifile : NULL_IFILE;
			if (ifile == NULL_IFILE)
				xcpy_char(&xp, *fr);
			else
				xcpy_filename(&xp, get_filename(ifile));
		} else
		{
			xcpy_char(&xp, *fr);
		}
	}
	xcpy_char(&xp, '\0');
	return xp.copied;
}

/*
 * Expand a string, substituting any "%" with the current filename,
 * and any "#" with the previous filename.
 * But a string of N "%"s is just replaced with N-1 "%"s.
 * Likewise for a string of N "#"s.
 * {{ This is a lot of work just to support % and #. }}
 */
public char * fexpand(constant char *s)
{
	size_t n;
	char *e;

	/*
	 * Make one pass to see how big a buffer we 
	 * need to allocate for the expanded string.
	 */
	n = fexpand_copy(s, NULL);
	e = (char *) ecalloc(n, sizeof(char));

	/*
	 * Now copy the string, expanding any "%" or "#".
	 */
	fexpand_copy(s, e);
	return (e);
}


#if TAB_COMPLETE_FILENAME

/*
 * Return a blank-separated list of filenames which "complete"
 * the given string.
 */
public char * fcomplete(constant char *s)
{
	char *fpat;
	char *qs;
	char *uqs;

	/* {{ Is this needed? lglob calls secure_allow. }} */
	if (!secure_allow(SF_GLOB))
		return (NULL);
	/*
	 * Complete the filename "s" by globbing "s*".
	 */
#if MSDOS_COMPILER && (MSDOS_COMPILER == MSOFTC || MSDOS_COMPILER == BORLANDC)
	/*
	 * But in DOS, we have to glob "s*.*".
	 * But if the final component of the filename already has
	 * a dot in it, just do "s*".  
	 * (Thus, "FILE" is globbed as "FILE*.*", 
	 *  but "FILE.A" is globbed as "FILE.A*").
	 */
	{
		constant char *slash;
		size_t len;
		for (slash = s+strlen(s)-1;  slash > s;  slash--)
			if (*slash == *PATHNAME_SEP || *slash == '/')
				break;
		len = strlen(s) + 4;
		fpat = (char *) ecalloc(len, sizeof(char));
		if (strchr(slash, '.') == NULL)
			SNPRINTF1(fpat, len, "%s*.*", s);
		else
			SNPRINTF1(fpat, len, "%s*", s);
	}
#else
	{
	size_t len = strlen(s) + 2;
	fpat = (char *) ecalloc(len, sizeof(char));
	SNPRINTF1(fpat, len, "%s*", s);
	}
#endif
	qs = lglob(fpat);
	uqs = shell_unquote(qs);
	if (strcmp(uqs, fpat) == 0)
	{
		/*
		 * The filename didn't expand.
		 */
		free(qs);
		qs = NULL;
	}
	free(uqs);
	free(fpat);
	return (qs);
}
#endif

/*
 * Try to determine if a file is "binary".
 * This is just a guess, and we need not try too hard to make it accurate.
 *
 * The number of bytes read is returned to the caller, because it will
 * be used later to compare to st_size from stat(2) to see if the file
 * is lying about its size.
 */
public int bin_file(int f, ssize_t *n)
{
	int bin_count = 0;
	char data[256];
	constant char* p;
	constant char* edata;

	if (!seekable(f))
		return (0);
	if (less_lseek(f, (less_off_t)0, SEEK_SET) == BAD_LSEEK)
		return (0);
	*n = read(f, data, sizeof(data));
	if (*n <= 0)
		return (0);
	edata = &data[*n];
	for (p = data;  p < edata;  )
	{
		if (utf_mode && !is_utf8_well_formed(p, (int) ptr_diff(edata,p)))
		{
			bin_count++;
			utf_skip_to_lead(&p, edata);
		} else 
		{
			LWCHAR c = step_charc(&p, +1, edata);
			struct ansi_state *pansi;
			if (ctldisp == OPT_ONPLUS && (pansi = ansi_start(c)) != NULL)
			{
				skip_ansi(pansi, c, &p, edata);
				ansi_done(pansi);
			} else if (binary_char(c))
				bin_count++;
		}
	}
	/*
	 * Call it a binary file if there are more than 5 binary characters
	 * in the first 256 bytes of the file.
	 */
	return (bin_count > 5);
}

/*
 * Try to determine the size of a file by seeking to the end.
 */
static POSITION seek_filesize(int f)
{
	less_off_t spos;

	spos = less_lseek(f, (less_off_t)0, SEEK_END);
	if (spos == BAD_LSEEK)
		return (NULL_POSITION);
	return ((POSITION) spos);
}

#if HAVE_POPEN
/*
 * Read a string from a file.
 * Return a pointer to the string in memory.
 */
public char * readfd(FILE *fd)
{
	struct xbuffer xbuf;
	xbuf_init(&xbuf);
	for (;;)
	{
		int ch;
		if ((ch = getc(fd)) == '\n' || ch == EOF)
			break;
		xbuf_add_char(&xbuf, (char) ch);
	}
	xbuf_add_char(&xbuf, '\0');
	return (char *) xbuf.data;
}

/*
 * Execute a shell command.
 * Return a pointer to a pipe connected to the shell command's standard output.
 */
static FILE * shellcmd(constant char *cmd)
{
	FILE *fd;

#if HAVE_SHELL
	constant char *shell;

	shell = lgetenv("SHELL");
	if (!isnullenv(shell))
	{
		char *scmd;
		char *esccmd;

		/*
		 * Read the output of <$SHELL -c cmd>.  
		 * Escape any metacharacters in the command.
		 */
		esccmd = shell_quote(cmd);
		if (esccmd == NULL)
		{
			fd = popen(cmd, "r");
		} else
		{
			size_t len = strlen(shell) + strlen(esccmd) + 5;
			scmd = (char *) ecalloc(len, sizeof(char));
			SNPRINTF3(scmd, len, "%s %s %s", shell, shell_coption(), esccmd);
			free(esccmd);
			fd = popen(scmd, "r");
			free(scmd);
		}
	} else
#endif
	{
		fd = popen(cmd, "r");
	}
	/*
	 * Redirection in `popen' might have messed with the
	 * standard devices.  Restore binary input mode.
	 */
	SET_BINARY(0);
	return (fd);
}

#endif /* HAVE_POPEN */


/*
 * Expand a filename, doing any system-specific metacharacter substitutions.
 */
public char * lglob(constant char *afilename)
{
	char *gfilename;
	char *filename = fexpand(afilename);

	if (!secure_allow(SF_GLOB))
		return (filename);

#ifdef DECL_GLOB_LIST
{
	/*
	 * The globbing function returns a list of names.
	 */
	size_t length;
	char *p;
	char *qfilename;
	DECL_GLOB_LIST(list)

	GLOB_LIST(filename, list);
	if (GLOB_LIST_FAILED(list))
	{
		return (filename);
	}
	length = 1; /* Room for trailing null byte */
	for (SCAN_GLOB_LIST(list, p))
	{
		INIT_GLOB_LIST(list, p);
		qfilename = shell_quote(p);
		if (qfilename != NULL)
		{
			length += strlen(qfilename) + 1;
			free(qfilename);
		}
	}
	gfilename = (char *) ecalloc(length, sizeof(char));
	for (SCAN_GLOB_LIST(list, p))
	{
		INIT_GLOB_LIST(list, p);
		qfilename = shell_quote(p);
		if (qfilename != NULL)
		{
			sprintf(gfilename + strlen(gfilename), "%s ", qfilename);
			free(qfilename);
		}
	}
	/*
	 * Overwrite the final trailing space with a null terminator.
	 */
	*--p = '\0';
	GLOB_LIST_DONE(list);
}
#else
#ifdef DECL_GLOB_NAME
{
	/*
	 * The globbing function returns a single name, and
	 * is called multiple times to walk thru all names.
	 */
	char *p;
	size_t len;
	size_t n;
	char *pfilename;
	char *qfilename;
	DECL_GLOB_NAME(fnd,drive,dir,fname,ext,handle)
	
	GLOB_FIRST_NAME(filename, &fnd, handle);
	if (GLOB_FIRST_FAILED(handle))
	{
		return (filename);
	}

	_splitpath(filename, drive, dir, fname, ext);
	len = 100;
	gfilename = (char *) ecalloc(len, sizeof(char));
	p = gfilename;
	do {
		n = strlen(drive) + strlen(dir) + strlen(fnd.GLOB_NAME) + 1;
		pfilename = (char *) ecalloc(n, sizeof(char));
		SNPRINTF3(pfilename, n, "%s%s%s", drive, dir, fnd.GLOB_NAME);
		qfilename = shell_quote(pfilename);
		free(pfilename);
		if (qfilename != NULL)
		{
			n = strlen(qfilename);
			while (p - gfilename + n + 2 >= len)
			{
				/*
				 * No room in current buffer.
				 * Allocate a bigger one.
				 */
				len *= 2;
				*p = '\0';
				p = (char *) ecalloc(len, sizeof(char));
				strcpy(p, gfilename);
				free(gfilename);
				gfilename = p;
				p = gfilename + strlen(gfilename);
			}
			strcpy(p, qfilename);
			free(qfilename);
			p += n;
			*p++ = ' ';
		}
	} while (GLOB_NEXT_NAME(handle, &fnd) == 0);

	/*
	 * Overwrite the final trailing space with a null terminator.
	 */
	*--p = '\0';
	GLOB_NAME_DONE(handle);
}
#else
#if HAVE_POPEN
{
	/*
	 * We get the shell to glob the filename for us by passing
	 * an "echo" command to the shell and reading its output.
	 */
	FILE *fd;
	constant char *s;
	constant char *lessecho;
	char *cmd;
	constant char *esc;
	char *qesc;
	size_t len;

	esc = get_meta_escape();
	if (strlen(esc) == 0)
		esc = "-";
	qesc = shell_quote(esc);
	if (qesc == NULL)
	{
		return (filename);
	}
	lessecho = lgetenv("LESSECHO");
	if (isnullenv(lessecho))
		lessecho = "lessecho";
	/*
	 * Invoke lessecho, and read its output (a globbed list of filenames).
	 */
	len = strlen(lessecho) + strlen(filename) + (7*strlen(metachars())) + 24;
	cmd = (char *) ecalloc(len, sizeof(char));
	SNPRINTF4(cmd, len, "%s -p0x%x -d0x%x -e%s ", lessecho,
		(unsigned char) openquote, (unsigned char) closequote, qesc);
	free(qesc);
	for (s = metachars();  *s != '\0';  s++)
		sprintf(cmd + strlen(cmd), "-n0x%x ", (unsigned char) *s);
	sprintf(cmd + strlen(cmd), "-- %s", filename);
	fd = shellcmd(cmd);
	free(cmd);
	if (fd == NULL)
	{
		/*
		 * Cannot create the pipe.
		 * Just return the original (fexpanded) filename.
		 */
		return (filename);
	}
	gfilename = readfd(fd);
	pclose(fd);
	if (*gfilename == '\0')
	{
		free(gfilename);
		return (filename);
	}
}
#else
	/*
	 * No globbing functions at all.  Just use the fexpanded filename.
	 */
	gfilename = save(filename);
#endif
#endif
#endif
	free(filename);
	return (gfilename);
}

/*
 * Does path not represent something in the file system?
 */
public lbool is_fake_pathname(constant char *path)
{
	return (strcmp(path, "-") == 0 ||
	        strcmp(path, FAKE_HELPFILE) == 0 || strcmp(path, FAKE_EMPTYFILE) == 0);
}

/*
 * Return canonical pathname.
 */
public char * lrealpath(constant char *path)
{
	if (!is_fake_pathname(path))
	{
#if HAVE_REALPATH
		/*
		 * Not all systems support the POSIX.1-2008 realpath() behavior
		 * of allocating when passing a NULL argument. And PATH_MAX is
		 * not required to be defined, or might contain an exceedingly
		 * big value. We assume that if it is not defined (such as on
		 * GNU/Hurd), then realpath() accepts NULL.
		 */
#ifndef PATH_MAX
		char *rpath;

		rpath = realpath(path, NULL);
		if (rpath != NULL)
			return (rpath);
#else
		char rpath[PATH_MAX];
		if (realpath(path, rpath) != NULL)
			return (save(rpath));
#endif
#endif
	}
	return (save(path));
}

#if HAVE_POPEN
/*
 * Return number of %s escapes in a string.
 * Return a large number if there are any other % escapes besides %s.
 */
static int num_pct_s(constant char *lessopen)
{
	int num = 0;

	while (*lessopen != '\0')
	{
		if (*lessopen == '%')
		{
			if (lessopen[1] == '%')
				++lessopen;
			else if (lessopen[1] == 's')
				++num;
			else
				return (999);
		}
		++lessopen;
	}
	return (num);
}
#endif

/*
 * See if we should open a "replacement file" 
 * instead of the file we're about to open.
 */
public char * open_altfile(constant char *filename, int *pf, void **pfd)
{
#if !HAVE_POPEN
	return (NULL);
#else
	constant char *lessopen;
	char *qfilename;
	char *cmd;
	size_t len;
	FILE *fd;
#if HAVE_FILENO
	int returnfd = 0;
#endif
	
	if (!secure_allow(SF_LESSOPEN))
		return (NULL);
	if (!use_lessopen)
		return (NULL);
	ch_ungetchar(-1);
	if ((lessopen = lgetenv("LESSOPEN")) == NULL)
		return (NULL);
	while (*lessopen == '|')
	{
		/*
		 * If LESSOPEN starts with a |, it indicates 
		 * a "pipe preprocessor".
		 */
#if !HAVE_FILENO
		error("LESSOPEN pipe is not supported", NULL_PARG);
		return (NULL);
#else
		lessopen++;
		returnfd++;
#endif
	}
	if (*lessopen == '-')
	{
		/*
		 * Lessopen preprocessor will accept "-" as a filename.
		 */
		lessopen++;
	} else
	{
		if (strcmp(filename, "-") == 0)
			return (NULL);
	}
	if (num_pct_s(lessopen) != 1)
	{
		error("LESSOPEN ignored: must contain exactly one %%s", NULL_PARG);
		return (NULL);
	}

	qfilename = shell_quote(filename);
	len = strlen(lessopen) + strlen(qfilename) + 2;
	cmd = (char *) ecalloc(len, sizeof(char));
	SNPRINTF1(cmd, len, lessopen, qfilename);
	free(qfilename);
	fd = shellcmd(cmd);
	free(cmd);
	if (fd == NULL)
	{
		/*
		 * Cannot create the pipe.
		 */
		return (NULL);
	}
#if HAVE_FILENO
	if (returnfd)
	{
		unsigned char c;
		int f;

		/*
		 * The alt file is a pipe. Read one char 
		 * to see if the pipe will produce any data.
		 * If it does, push the char back on the pipe.
		 */
		f = fileno(fd);
		SET_BINARY(f);
		if (read(f, &c, 1) != 1)
		{
			/*
			 * Pipe is empty.
			 * If more than 1 pipe char was specified,
			 * the exit status tells whether the file itself 
			 * is empty, or if there is no alt file.
			 * If only one pipe char, just assume no alt file.
			 */
			int status = pclose(fd);
			if (returnfd > 1 && status == 0) {
				/* File is empty. */
				*pfd = NULL;
				*pf = -1;
				return (save(FAKE_EMPTYFILE));
			}
			/* No alt file. */
			return (NULL);
		}
		/* Alt pipe contains data, so use it. */
		ch_ungetchar(c);
		*pfd = (void *) fd;
		*pf = f;
		return (save("-"));
	}
#endif
	/* The alt file is a regular file. Read its name from LESSOPEN. */
	cmd = readfd(fd);
	pclose(fd);
	if (*cmd == '\0')
	{
		/*
		 * Pipe is empty.  This means there is no alt file.
		 */
		free(cmd);
		return (NULL);
	}
	return (cmd);
#endif /* HAVE_POPEN */
}

/*
 * Close a replacement file.
 */
public void close_altfile(constant char *altfilename, constant char *filename)
{
#if HAVE_POPEN
	constant char *lessclose;
	char *qfilename;
	char *qaltfilename;
	FILE *fd;
	char *cmd;
	size_t len;
	
	if (!secure_allow(SF_LESSOPEN))
		return;
	if ((lessclose = lgetenv("LESSCLOSE")) == NULL)
		return;
	if (num_pct_s(lessclose) > 2) 
	{
		error("LESSCLOSE ignored; must contain no more than 2 %%s", NULL_PARG);
		return;
	}
	qfilename = shell_quote(filename);
	qaltfilename = shell_quote(altfilename);
	len = strlen(lessclose) + strlen(qfilename) + strlen(qaltfilename) + 2;
	cmd = (char *) ecalloc(len, sizeof(char));
	SNPRINTF2(cmd, len, lessclose, qfilename, qaltfilename);
	free(qaltfilename);
	free(qfilename);
	fd = shellcmd(cmd);
	free(cmd);
	if (fd != NULL)
		pclose(fd);
#endif
}
		
/*
 * Is the specified file a directory?
 */
public lbool is_dir(constant char *filename)
{
	lbool isdir = FALSE;

#if HAVE_STAT
{
	int r;
	less_stat_t statbuf;

	r = less_stat(filename, &statbuf);
	isdir = (r >= 0 && S_ISDIR(statbuf.st_mode));
}
#else
#ifdef _OSK
{
	int f;

	f = open(filename, S_IREAD | S_IFDIR);
	if (f >= 0)
		close(f);
	isdir = (f >= 0);
}
#endif
#endif
	return (isdir);
}

/*
 * Returns NULL if the file can be opened and
 * is an ordinary file, otherwise an error message
 * (if it cannot be opened or is a directory, etc.)
 */
public char * bad_file(constant char *filename)
{
	char *m = NULL;

	if (!force_open && is_dir(filename))
	{
		static char is_a_dir[] = " is a directory";

		m = (char *) ecalloc(strlen(filename) + sizeof(is_a_dir), 
			sizeof(char));
		strcpy(m, filename);
		strcat(m, is_a_dir);
	} else
	{
#if HAVE_STAT
		int r;
		less_stat_t statbuf;

		r = less_stat(filename, &statbuf);
		if (r < 0)
		{
			m = errno_message(filename);
		} else if (force_open)
		{
			m = NULL;
		} else if (!S_ISREG(statbuf.st_mode))
		{
			static char not_reg[] = " is not a regular file (use -f to see it)";
			m = (char *) ecalloc(strlen(filename) + sizeof(not_reg),
				sizeof(char));
			strcpy(m, filename);
			strcat(m, not_reg);
		}
#endif
	}
	return (m);
}

/*
 * Return the size of a file, as cheaply as possible.
 * In Unix, we can stat the file.
 */
public POSITION filesize(int f)
{
#if HAVE_STAT
	less_stat_t statbuf;

	if (less_fstat(f, &statbuf) >= 0)
		return ((POSITION) statbuf.st_size);
#else
#ifdef _OSK
	long size;

	if ((size = (long) _gs_size(f)) >= 0)
		return ((POSITION) size);
#endif
#endif
	return (seek_filesize(f));
}

public lbool curr_ifile_changed(void)
{
#if HAVE_STAT_INO
	/* 
	 * If the file's i-number or device has changed,
	 * or if the file is smaller than it previously was,
	 * the file must be different.
	 */
	struct stat st;
	POSITION curr_pos = ch_tell();
	int r = stat(get_filename(curr_ifile), &st);
	if (r == 0 && (st.st_ino != curr_ino ||
		st.st_dev != curr_dev ||
		(curr_pos != NULL_POSITION && st.st_size < curr_pos)))
		return (TRUE);
#endif
	return (FALSE);
}

/*
 * 
 */
public constant char * shell_coption(void)
{
	return ("-c");
}

/*
 * Return last component of a pathname.
 */
public constant char * last_component(constant char *name)
{
	constant char *slash;

	for (slash = name + strlen(name);  slash > name; )
	{
		--slash;
		if (*slash == *PATHNAME_SEP || *slash == '/')
			return (slash + 1);
	}
	return (name);
}
