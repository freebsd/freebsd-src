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
#define _MAX_PATH	PATH_MAX
#endif
#endif
#ifdef _OSK
#include <rbf.h>
#ifndef _OSK_MWC32
#include <modes.h>
#endif
#endif
#if OS2
#include <signal.h>
#endif

#if HAVE_STAT
#include <sys/stat.h>
#ifndef S_ISDIR
#define	S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#endif
#endif


extern int force_open;
extern int secure;
extern IFILE curr_ifile;
extern IFILE old_ifile;
#if SPACES_IN_FILENAMES
extern char openquote;
extern char closequote;
#endif

/*
 * Remove quotes around a filename.
 */
	public char *
unquote_file(str)
	char *str;
{
#if SPACES_IN_FILENAMES
	char *name;
	char *p;

	if (*str != openquote)
		return (save(str));
	name = (char *) ecalloc(strlen(str), sizeof(char));
	strcpy(name, str+1);
	p = name + strlen(name) - 1;
	if (*p == closequote)
		*p = '\0';
	return (name);
#else
	return (save(str));
#endif
}

/*
 * Return a pathname that points to a specified file in a specified directory.
 * Return NULL if the file does not exist in the directory.
 */
	static char *
dirfile(dirname, filename)
	char *dirname;
	char *filename;
{
	char *pathname;
	char *qpathname;
	int f;

	if (dirname == NULL || *dirname == '\0')
		return (NULL);
	/*
	 * Construct the full pathname.
	 */
	pathname = (char *) calloc(strlen(dirname) + strlen(filename) + 2, 
					sizeof(char));
	if (pathname == NULL)
		return (NULL);
	sprintf(pathname, "%s%s%s", dirname, PATHNAME_SEP, filename);
	/*
	 * Make sure the file exists.
	 */
	qpathname = unquote_file(pathname);
	f = open(qpathname, OPEN_READ);
	if (f < 0)
	{
		free(pathname);
		pathname = NULL;
	} else
	{
		close(f);
	}
	free(qpathname);
	return (pathname);
}

/*
 * Return the full pathname of the given file in the "home directory".
 */
	public char *
homefile(filename)
	char *filename;
{
	register char *pathname;

	/*
	 * Try $HOME/filename.
	 */
	pathname = dirfile(lgetenv("HOME"), filename);
	if (pathname != NULL)
		return (pathname);
#if OS2
	/*
	 * Try $INIT/filename.
	 */
	pathname = dirfile(lgetenv("INIT"), filename);
	if (pathname != NULL)
		return (pathname);
#endif
#if MSDOS_COMPILER || OS2
	/*
	 * Look for the file anywhere on search path.
	 */
	pathname = (char *) calloc(_MAX_PATH, sizeof(char));
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

/*
 * Expand a string, substituting any "%" with the current filename,
 * and any "#" with the previous filename.
 * But a string of N "%"s is just replaced with N-1 "%"s.
 * Likewise for a string of N "#"s.
 * {{ This is a lot of work just to support % and #. }}
 */
	public char *
fexpand(s)
	char *s;
{
	register char *fr, *to;
	register int n;
	register char *e;
	IFILE ifile;

#define	fchar_ifile(c) \
	((c) == '%' ? curr_ifile : \
	 (c) == '#' ? old_ifile : NULL_IFILE)

	/*
	 * Make one pass to see how big a buffer we 
	 * need to allocate for the expanded string.
	 */
	n = 0;
	for (fr = s;  *fr != '\0';  fr++)
	{
		switch (*fr)
		{
		case '%':
		case '#':
			if (fr > s && fr[-1] == *fr)
			{
				/*
				 * Second (or later) char in a string
				 * of identical chars.  Treat as normal.
				 */
				n++;
			} else if (fr[1] != *fr)
			{
				/*
				 * Single char (not repeated).  Treat specially.
				 */
				ifile = fchar_ifile(*fr);
				if (ifile == NULL_IFILE)
					n++;
				else
					n += strlen(get_filename(ifile));
			}
			/*
			 * Else it is the first char in a string of
			 * identical chars.  Just discard it.
			 */
			break;
		default:
			n++;
			break;
		}
	}

	e = (char *) ecalloc(n+1, sizeof(char));

	/*
	 * Now copy the string, expanding any "%" or "#".
	 */
	to = e;
	for (fr = s;  *fr != '\0';  fr++)
	{
		switch (*fr)
		{
		case '%':
		case '#':
			if (fr > s && fr[-1] == *fr)
			{
				*to++ = *fr;
			} else if (fr[1] != *fr)
			{
				ifile = fchar_ifile(*fr);
				if (ifile == NULL_IFILE)
					*to++ = *fr;
				else
				{
					strcpy(to, get_filename(ifile));
					to += strlen(to);
				}
			}
			break;
		default:
			*to++ = *fr;
			break;
		}
	}
	*to = '\0';
	return (e);
}

#if TAB_COMPLETE_FILENAME

/*
 * Return a blank-separated list of filenames which "complete"
 * the given string.
 */
	public char *
fcomplete(s)
	char *s;
{
	char *fpat;

	if (secure)
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
		char *slash;
		for (slash = s+strlen(s)-1;  slash > s;  slash--)
			if (*slash == *PATHNAME_SEP || *slash == '/')
				break;
		fpat = (char *) ecalloc(strlen(s)+4, sizeof(char));
		if (strchr(slash, '.') == NULL)
			sprintf(fpat, "%s*.*", s);
		else
			sprintf(fpat, "%s*", s);
	}
#else
	fpat = (char *) ecalloc(strlen(s)+2, sizeof(char));
	sprintf(fpat, "%s*", s);
#endif
	s = lglob(fpat);
	if (strcmp(s,fpat) == 0)
	{
		/*
		 * The filename didn't expand.
		 */
		free(s);
		s = NULL;
	}
	free(fpat);
	return (s);
}
#endif

/*
 * Try to determine if a file is "binary".
 * This is just a guess, and we need not try too hard to make it accurate.
 */
	public int
bin_file(f)
	int f;
{
	int i;
	int n;
	unsigned char data[64];

	if (!seekable(f))
		return (0);
	if (lseek(f, (off_t)0, 0) == BAD_LSEEK)
		return (0);
	n = read(f, data, sizeof(data));
	for (i = 0;  i < n;  i++)
		if (binary_char(data[i]))
			return (1);
	return (0);
}

/*
 * Try to determine the size of a file by seeking to the end.
 */
	static POSITION
seek_filesize(f)
	int f;
{
	off_t spos;

	spos = lseek(f, (off_t)0, 2);
	if (spos == BAD_LSEEK)
		return (NULL_POSITION);
	return ((POSITION) spos);
}

/*
 * Read a string from a file.
 * Return a pointer to the string in memory.
 */
	static char *
readfd(fd)
	FILE *fd;
{
	int len;
	int ch;
	char *buf;
	char *p;
	
	/* 
	 * Make a guess about how many chars in the string
	 * and allocate a buffer to hold it.
	 */
	len = 100;
	buf = (char *) ecalloc(len, sizeof(char));
	for (p = buf;  ;  p++)
	{
		if ((ch = getc(fd)) == '\n' || ch == EOF)
			break;
		if (p - buf >= len-1)
		{
			/*
			 * The string is too big to fit in the buffer we have.
			 * Allocate a new buffer, twice as big.
			 */
			len *= 2;
			*p = '\0';
			p = (char *) ecalloc(len, sizeof(char));
			strcpy(p, buf);
			free(buf);
			buf = p;
			p = buf + strlen(buf);
		}
		*p = ch;
	}
	*p = '\0';
	return (buf);
}

#if HAVE_SHELL

/*
 * Get the shell's escape character.
 */
	static char *
get_meta_escape()
{
	char *s;

	s = lgetenv("LESSMETAESCAPE");
	if (s == NULL)
		s = DEF_METAESCAPE;
	return (s);
}

/*
 * Is this a shell metacharacter?
 */
	static int
metachar(c)
	char c;
{
	static char *metachars = NULL;

	if (metachars == NULL)
	{
		metachars = lgetenv("LESSMETACHARS");
		if (metachars == NULL)
			metachars = DEF_METACHARS;
	}
	return (strchr(metachars, c) != NULL);
}

/*
 * Insert a backslash before each metacharacter in a string.
 */
	public char *
esc_metachars(s)
	char *s;
{
	char *p;
	char *newstr;
	int len;
	char *esc;
	int esclen;

	/*
	 * Determine how big a string we need to allocate.
	 */
	esc = get_meta_escape();
	esclen = strlen(esc);
	len = 1; /* Trailing null byte */
	for (p = s;  *p != '\0';  p++)
	{
		len++;
		if (metachar(*p))
		{
			if (*esc == '\0')
			{
				/*
				 * We've got a metachar, but this shell 
				 * doesn't support escape chars.  Give up.
				 */
				return (NULL);
			}
			/*
			 * Allow space for the escape char.
			 */
			len += esclen;
		}
	}
	/*
	 * Allocate and construct the new string.
	 */
	newstr = p = (char *) ecalloc(len, sizeof(char));
	while (*s != '\0')
	{
		if (metachar(*s))
		{
			/*
			 * Add the escape char.
			 */
			strcpy(p, esc);
			p += esclen;
		}
		*p++ = *s++;
	}
	*p = '\0';
	return (newstr);
}

#else /* HAVE_SHELL */

	public char *
esc_metachars(s)
	char *s;
{
	return (save(s));
}

#endif /* HAVE_SHELL */


#if HAVE_POPEN

FILE *popen();

/*
 * Execute a shell command.
 * Return a pointer to a pipe connected to the shell command's standard output.
 */
	static FILE *
shellcmd(cmd)
	char *cmd;
{
	FILE *fd;

#if HAVE_SHELL
	char *shell;

	shell = lgetenv("SHELL");
	if (shell != NULL && *shell != '\0')
	{
		char *scmd;
		char *esccmd;

		/*
		 * Try to escape any metacharacters in the command.
		 * If we can't do that, just put the command in quotes.
		 * (But that doesn't work well if the command itself 
		 * contains quotes.)
		 */
		if ((esccmd = esc_metachars(cmd)) == NULL)
		{
			/*
			 * Cannot escape the metacharacters, so use quotes.
			 * Read the output of <$SHELL -c "cmd">.
			 */
			scmd = (char *) ecalloc(strlen(shell) + strlen(cmd) + 7,
						sizeof(char));
			sprintf(scmd, "%s -c \"%s\"", shell, cmd);
		} else
		{
			/*
			 * Read the output of <$SHELL -c cmd>.  
			 * No quotes; use the escaped cmd.
			 */
			scmd = (char *) ecalloc(strlen(shell) + strlen(esccmd) + 5,
						sizeof(char));
			sprintf(scmd, "%s -c %s", shell, esccmd);
			free(esccmd);
		}
		fd = popen(scmd, "r");
		free(scmd);
	} else
#endif
	{
		fd = popen(cmd, "r");
		/*
		 * Redirection in `popen' might have messed with the
		 * standard devices.  Restore binary input mode.
		 */
		SET_BINARY(0);
	}
	return (fd);
}

#endif /* HAVE_POPEN */


/*
 * Expand a filename, doing any system-specific metacharacter substitutions.
 */
	public char *
lglob(filename)
	char *filename;
{
	char *gfilename;
	char *ofilename;

	ofilename = fexpand(filename);
	if (secure)
		return (ofilename);
	filename = unquote_file(ofilename);

#ifdef DECL_GLOB_LIST
{
	/*
	 * The globbing function returns a list of names.
	 */
	int length;
	char *p;
	DECL_GLOB_LIST(list)

	GLOB_LIST(filename, list);
	if (GLOB_LIST_FAILED(list))
	{
		free(filename);
		return (ofilename);
	}
	length = 1; /* Room for trailing null byte */
	for (SCAN_GLOB_LIST(list, p))
	{
		INIT_GLOB_LIST(list, p);
	  	length += strlen(p) + 1;
#if SPACES_IN_FILENAMES
		if (strchr(p, ' ') != NULL)
			length += 2; /* Allow for quotes */
#endif
	}
	gfilename = (char *) ecalloc(length, sizeof(char));
	for (SCAN_GLOB_LIST(list, p))
	{
		INIT_GLOB_LIST(list, p);
#if SPACES_IN_FILENAMES
		if (strchr(p, ' ') != NULL)
			sprintf(gfilename + strlen(gfilename), "%c%s%c ",
				openquote, p, closequote);
		else
#endif
			sprintf(gfilename + strlen(gfilename), "%s ", p);
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
	register char *p;
	register int len;
	register int n;
#if SPACES_IN_FILENAMES
	register int spaces_in_file;
#endif
	DECL_GLOB_NAME(fnd,drive,dir,fname,ext,handle)
	
	GLOB_FIRST_NAME(filename, &fnd, handle);
	if (GLOB_FIRST_FAILED(handle))
	{
		free(filename);
		return (ofilename);
	}

	_splitpath(filename, drive, dir, fname, ext);
	len = 100;
	gfilename = (char *) ecalloc(len, sizeof(char));
	p = gfilename;
	do {
		n = strlen(drive) + strlen(dir) + strlen(fnd.GLOB_NAME) + 1;
#if SPACES_IN_FILENAMES
		spaces_in_file = 0;
		if (strchr(fnd.GLOB_NAME, ' ') != NULL ||
		    strchr(filename, ' ') != NULL)
		{
			spaces_in_file = 1;
			n += 2;
		}
#endif
		while (p - gfilename + n+2 >= len)
		{
			/*
			 * No room in current buffer.  Allocate a bigger one.
			 */
			len *= 2;
			*p = '\0';
			p = (char *) ecalloc(len, sizeof(char));
			strcpy(p, gfilename);
			free(gfilename);
			gfilename = p;
			p = gfilename + strlen(gfilename);
		}
#if SPACES_IN_FILENAMES
		if (spaces_in_file)
			sprintf(p, "%c%s%s%s%c ", openquote, 
				drive, dir, fnd.GLOB_NAME, closequote);
		else
#endif
			sprintf(p, "%s%s%s ", drive, dir, fnd.GLOB_NAME);
		p += n;
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
	char *s;
	char *lessecho;
	char *cmd;

	lessecho = lgetenv("LESSECHO");
	if (lessecho == NULL || *lessecho == '\0')
		lessecho = "lessecho";
	s = esc_metachars(filename);
	if (s == NULL)
	{
		/*
		 * There may be dangerous metachars in this name.
		 * We can't risk passing it to the shell.
		 * {{ For example, do "!;TAB" when the first file 
		 *    in the dir is named "rm". }}
		 */
		free(filename);
		return (ofilename);
	}
	/*
	 * Invoke lessecho, and read its output (a globbed list of filenames).
	 */
	cmd = (char *) ecalloc(strlen(lessecho) + strlen(s) + 24, sizeof(char));
	sprintf(cmd, "%s -p0x%x -d0x%x -- %s", 
		lessecho, openquote, closequote, s);
	fd = shellcmd(cmd);
	free(s);
	free(cmd);
	if (fd == NULL)
	{
		/*
		 * Cannot create the pipe.
		 * Just return the original (fexpanded) filename.
		 */
		free(filename);
		return (ofilename);
	}
	gfilename = readfd(fd);
	pclose(fd);
	if (*gfilename == '\0')
	{
		free(gfilename);
		free(filename);
		return (ofilename);
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
	free(ofilename);
	return (gfilename);
}

/*
 * See if we should open a "replacement file" 
 * instead of the file we're about to open.
 */
	public char *
open_altfile(filename, pf, pfd)
	char *filename;
	int *pf;
	void **pfd;
{
#if !HAVE_POPEN
	return (NULL);
#else
	char *lessopen;
	char *gfilename;
	char *cmd;
	FILE *fd;
#if HAVE_FILENO
	int returnfd = 0;
#endif
	
	if (secure)
		return (NULL);
	ch_ungetchar(-1);
	if ((lessopen = lgetenv("LESSOPEN")) == NULL)
		return (NULL);
	if (strcmp(filename, "-") == 0)
		return (NULL);
	if (*lessopen == '|')
	{
		/*
		 * If LESSOPEN starts with a |, it indicates 
		 * a "pipe preprocessor".
		 */
#if HAVE_FILENO
		lessopen++;
		returnfd = 1;
#else
		error("LESSOPEN pipe is not supported", NULL_PARG);
		return (NULL);
#endif
	}

	gfilename = esc_metachars(filename);
	if (gfilename == NULL)
	{
		/*
		 * Cannot escape metacharacters.
		 */
		return (NULL);
	}
	cmd = (char *) ecalloc(strlen(lessopen) + strlen(gfilename) + 2, 
			sizeof(char));
	sprintf(cmd, lessopen, gfilename);
	fd = shellcmd(cmd);
	free(gfilename);
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
		int f;
		char c;

		/*
		 * Read one char to see if the pipe will produce any data.
		 * If it does, push the char back on the pipe.
		 */
		f = fileno(fd);
		SET_BINARY(f);
		if (read(f, &c, 1) != 1)
		{
			/*
			 * Pipe is empty.  This means there is no alt file.
			 */
			pclose(fd);
			return (NULL);
		}
		ch_ungetchar(c);
		*pfd = (void *) fd;
		*pf = f;
		return (save("-"));
	}
#endif
	gfilename = readfd(fd);
	pclose(fd);
	if (*gfilename == '\0')
		/*
		 * Pipe is empty.  This means there is no alt file.
		 */
		return (NULL);
	return (gfilename);
#endif /* HAVE_POPEN */
}

/*
 * Close a replacement file.
 */
	public void
close_altfile(altfilename, filename, pipefd)
	char *altfilename;
	char *filename;
	void *pipefd;
{
#if HAVE_POPEN
	char *lessclose;
	char *gfilename;
	char *galtfilename;
	FILE *fd;
	char *cmd;
	
	if (secure)
		return;
	if (pipefd != NULL)
	{
#if OS2
		/*
		 * The pclose function of OS/2 emx sometimes fails.
		 * Send SIGINT to the piped process before closing it.
		 */
		kill(((FILE*)pipefd)->_pid, SIGINT);
#endif
		pclose((FILE*) pipefd);
	}
	if ((lessclose = lgetenv("LESSCLOSE")) == NULL)
	     	return;
	gfilename = esc_metachars(filename);
	if (gfilename == NULL)
	{
		return;
	}
	galtfilename = esc_metachars(altfilename);
	if (galtfilename == NULL)
	{
		free(gfilename);
		return;
	}
	cmd = (char *) ecalloc(strlen(lessclose) + strlen(gfilename) + 
			strlen(galtfilename) + 2, sizeof(char));
	sprintf(cmd, lessclose, gfilename, galtfilename);
	fd = shellcmd(cmd);
	free(galtfilename);
	free(gfilename);
	free(cmd);
	if (fd != NULL)
		pclose(fd);
#endif
}
		
/*
 * Is the specified file a directory?
 */
	public int
is_dir(filename)
	char *filename;
{
	int isdir = 0;

	filename = unquote_file(filename);
#if HAVE_STAT
{
	int r;
	struct stat statbuf;

	r = stat(filename, &statbuf);
	isdir = (r >= 0 && S_ISDIR(statbuf.st_mode));
}
#else
#ifdef _OSK
{
	register int f;

	f = open(filename, S_IREAD | S_IFDIR);
	if (f >= 0)
		close(f);
	isdir = (f >= 0);
}
#endif
#endif
	free(filename);
	return (isdir);
}

/*
 * Returns NULL if the file can be opened and
 * is an ordinary file, otherwise an error message
 * (if it cannot be opened or is a directory, etc.)
 */
	public char *
bad_file(filename)
	char *filename;
{
	register char *m = NULL;

	filename = unquote_file(filename);
	if (is_dir(filename))
	{
		static char is_dir[] = " is a directory";

		m = (char *) ecalloc(strlen(filename) + sizeof(is_dir), 
			sizeof(char));
		strcpy(m, filename);
		strcat(m, is_dir);
	} else
	{
#if HAVE_STAT
		int r;
		struct stat statbuf;

		r = stat(filename, &statbuf);
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
	free(filename);
	return (m);
}

/*
 * Return the size of a file, as cheaply as possible.
 * In Unix, we can stat the file.
 */
	public POSITION
filesize(f)
	int f;
{
#if HAVE_STAT
	struct stat statbuf;

	if (fstat(f, &statbuf) >= 0)
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

