/* tmp.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains functions which create & readback a TMPFILE */


#include "config.h"
#include "vi.h"
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
#if TURBOC
# include <process.h>
#endif

#ifndef NO_MODELINES
static void do_modelines(l, stop)
	long	l;	/* line number to start at */
	long	stop;	/* line number to stop at */
{
	char	*str;	/* used to scan through the line */
	char	*start;	/* points to the start of the line */
	char	buf[80];

	/* if modelines are disabled, then do nothing */
	if (!*o_modelines)
	{
		return;
	}

	/* for each line... */
	for (; l <= stop; l++)
	{
		/* for each position in the line.. */
		for (str = fetchline(l); *str; str++)
		{
			/* if it is the start of a modeline command... */
			if ((str[0] == 'e' && str[1] == 'x'
			  || str[0] == 'v' && str[1] == 'i')
			  && str[2] == ':')
			{
				start = str += 3;

				/* find the end */
				for (str = start + strlen(start); *--str != ':'; )
				{
				}

				/* if it is a well-formed modeline, execute it */
				if (str > start && str - start < sizeof buf)
				{
					strncpy(buf, start, (int)(str - start));
					exstring(buf, str - start, '\\');
					break;
				}
			}
		}
	}
}
#endif


/* The FAIL() macro prints an error message and then exits. */
#define FAIL(why,arg)	mode = MODE_EX; msg(why, arg); endwin(); exit(9)

/* This is the name of the temp file */
static char	tmpname[80];

/* This function creates the temp file and copies the original file into it.
 * Returns if successful, or stops execution if it fails.
 */
int tmpstart(filename)
	char		*filename; /* name of the original file */
{
	int		origfd;	/* fd used for reading the original file */
	struct stat	statb;	/* stat buffer, used to examine inode */
	REG BLK		*this;	/* pointer to the current block buffer */
	REG BLK		*next;	/* pointer to the next block buffer */
	int		inbuf;	/* number of characters in a buffer */
	int		nread;	/* number of bytes read */
	REG int		j, k;
	int		i;
	long		nbytes;

	/* switching to a different file certainly counts as a change */
	changes++;
	redraw(MARK_UNSET, FALSE);

	/* open the original file for reading */
	*origname = '\0';
	if (filename && *filename)
	{
		strcpy(origname, filename);
		origfd = open(origname, O_RDONLY);
		if (origfd < 0 && errno != ENOENT)
		{
			msg("Can't open \"%s\"", origname);
			return tmpstart("");
		}
		if (origfd >= 0)
		{
			if (stat(origname, &statb) < 0)
			{
				FAIL("Can't stat \"%s\"", origname);
			}
#if TOS
			if (origfd >= 0 && (statb.st_mode & S_IJDIR))
#else
# if OSK
			if (origfd >= 0 && (statb.st_mode & S_IFDIR))
# else
			if (origfd >= 0 && (statb.st_mode & S_IFMT) != S_IFREG)
# endif
#endif
			{
				msg("\"%s\" is not a regular file", origname);
				return tmpstart("");
			}
		}
		else
		{
			stat(".", &statb);
		}
		if (origfd >= 0)
		{
			origtime = statb.st_mtime;
#if OSK
			if (*o_readonly || !(statb.st_mode &
				  ((getuid() >> 16) == 0 ? S_IOWRITE | S_IWRITE :
				  ((statb.st_gid != (getuid() >> 16) ? S_IOWRITE : S_IWRITE)))))
#endif
#if AMIGA || MSDOS
			if (*o_readonly || !(statb.st_mode & S_IWRITE))
#endif
#if TOS
# ifdef __GNUC__ 
			if (*o_readonly || !(statb.st_mode & S_IWRITE))
# else
			if (*o_readonly || (statb.st_mode & S_IJRON))
# endif
#endif
#if ANY_UNIX
			if (*o_readonly || !(statb.st_mode &
				  ((geteuid() == 0) ? 0222 :
				  ((statb.st_uid != geteuid() ? 0022 : 0200)))))
#endif
#if VMS
			if (*o_readonly)
#endif
			{
				setflag(file, READONLY);
			}
		}
		else
		{
			origtime = 0L;
		}
	}
	else
	{
		setflag(file, NOFILE);
		origfd = -1;
		origtime = 0L;
		stat(".", &statb);
	}

	/* make a name for the tmp file */
	do
	{
		tmpnum++;
#if MSDOS || TOS
		/* MS-Dos doesn't allow multiple slashes, but supports drives
		 * with current directories.
		 * This relies on TMPNAME beginning with "%s\\"!!!!
		 */
		strcpy(tmpname, o_directory);
		if ((i = strlen(tmpname)) && !strchr(":/\\", tmpname[i-1]))
			tmpname[i++]=SLASH;
		sprintf(tmpname+i, TMPNAME+3, getpid(), tmpnum);
#else
		sprintf(tmpname, TMPNAME, o_directory, getpid(), tmpnum);
#endif
	} while (access(tmpname, 0) == 0);

	/* !!! RACE CONDITION HERE - some other process with the same PID could
	 * create the temp file between the access() call and the creat() call.
	 * This could happen in a couple of ways:
	 * - different workstation may share the same temp dir via NFS.  Each
	 *   workstation could have a process with the same number.
	 * - The DOS version may be running multiple times on the same physical
	 *   machine in different virtual machines.  The DOS pid number will
	 *   be the same on all virtual machines.
	 *
	 * This race condition could be fixed by replacing access(tmpname, 0)
	 * with open(tmpname, O_CREAT|O_EXCL, 0600), if we could only be sure
	 * that open() *always* used modern UNIX semantics.
	 */

	/* create the temp file */
#if ANY_UNIX
	close(creat(tmpname, 0600));		/* only we can read it */
#else
	close(creat(tmpname, FILEPERMS));	/* anybody body can read it, alas */
#endif
	tmpfd = open(tmpname, O_RDWR | O_BINARY);
	if (tmpfd < 0)
	{
		FAIL("Can't create temp file... Does directory \"%s\" exist?", o_directory);
		return 1;
	}

	/* allocate space for the header in the file */
	if (write(tmpfd, hdr.c, (unsigned)BLKSIZE) < BLKSIZE
	 || write(tmpfd, tmpblk.c, (unsigned)BLKSIZE) < BLKSIZE)
	{
		FAIL("Error writing headers to \"%s\"", tmpname);
	}

#ifndef NO_RECYCLE
	/* initialize the block allocator */
	/* This must already be done here, before the first attempt
	 * to write to the new file! GB */
	garbage();
#endif

	/* initialize lnum[] */
	for (i = 1; i < MAXBLKS; i++)
	{
		lnum[i] = INFINITY;
	}
	lnum[0] = 0;

	/* if there is no original file, then create a 1-line file */
	if (origfd < 0)
	{
		hdr.n[0] = 0;	/* invalid inode# denotes new file */

		this = blkget(1); 	/* get the new text block */
		strcpy(this->c, "\n");	/* put a line in it */

		lnum[1] = 1L;	/* block 1 ends with line 1 */
		nlines = 1L;	/* there is 1 line in the file */
		nbytes = 1L;

		if (*origname)
		{
			msg("\"%s\" [NEW FILE]  1 line, 1 char", origname);
		}
		else
		{
			msg("\"[NO FILE]\"  1 line, 1 char");
		}
	}
	else /* there is an original file -- read it in */
	{
		nbytes = nlines = 0;

		/* preallocate 1 "next" buffer */
		i = 1;
		next = blkget(i);
		inbuf = 0;

		/* loop, moving blocks from orig to tmp */
		for (;;)
		{
			/* "next" buffer becomes "this" buffer */
			this = next;

			/* read [more] text into this block */
			nread = tread(origfd, &this->c[inbuf], BLKSIZE - 1 - inbuf);
			if (nread < 0)
			{
				close(origfd);
				close(tmpfd);
				tmpfd = -1;
				unlink(tmpname);
				FAIL("Error reading \"%s\"", origname);
			}

			/* convert NUL characters to something else */
			for (j = k = inbuf; k < inbuf + nread; k++)
			{
				if (!this->c[k])
				{
					setflag(file, HADNUL);
					this->c[j++] = 0x80;
				}
#ifndef CRUNCH
				else if (*o_beautify && this->c[k] < ' ' && this->c[k] >= 1)
				{
					if (this->c[k] == '\t'
					 || this->c[k] == '\n'
					 || this->c[k] == '\f')
					{
						this->c[j++] = this->c[k];
					}
					else if (this->c[k] == '\b')
					{
						/* delete '\b', but complain */
						setflag(file, HADBS);
					}
					/* else silently delete control char */
				}
#endif
				else
				{
					this->c[j++] = this->c[k];
				}
			}
			inbuf = j;

			/* if the buffer is empty, quit */
			if (inbuf == 0)
			{
				goto FoundEOF;
			}

#if MSDOS || TOS
/* BAH! MS text mode read fills inbuf, then compresses eliminating \r
   but leaving garbage at end of buf. The same is true for TURBOC. GB. */

			memset(this->c + inbuf, '\0', BLKSIZE - inbuf);
#endif

			/* search backward for last newline */
			for (k = inbuf; --k >= 0 && this->c[k] != '\n';)
			{
			}
			if (k++ < 0)
			{
				if (inbuf >= BLKSIZE - 1)
				{
					k = 80;
				}
				else
				{
					k = inbuf;
				}
			}

			/* allocate next buffer */
			if (i >= MAXBLKS - 2)
			{
				FAIL("File too big.  Limit is approx %ld kbytes.", MAXBLKS * BLKSIZE / 1024L);
			}
			next = blkget(++i);

			/* move fragmentary last line to next buffer */
			inbuf -= k;
			for (j = 0; k < BLKSIZE; j++, k++)
			{
				next->c[j] = this->c[k];
				this->c[k] = 0;
			}

			/* if necessary, add a newline to this buf */
			for (k = BLKSIZE - inbuf; --k >= 0 && !this->c[k]; )
			{
			}
			if (this->c[k] != '\n')
			{
				setflag(file, ADDEDNL);
				this->c[k + 1] = '\n';
			}

			/* count the lines in this block */
			for (k = 0; k < BLKSIZE && this->c[k]; k++)
			{
				if (this->c[k] == '\n')
				{
					nlines++;
				}
				nbytes++;
			}
			lnum[i - 1] = nlines;
		}
FoundEOF:

		/* if this is a zero-length file, add 1 line */
		if (nlines == 0)
		{
			this = blkget(1); 	/* get the new text block */
			strcpy(this->c, "\n");	/* put a line in it */

			lnum[1] = 1;	/* block 1 ends with line 1 */
			nlines = 1;	/* there is 1 line in the file */
			nbytes = 1;
		}

#if MSDOS || TOS
		/* each line has an extra CR that we didn't count yet */
		nbytes += nlines;
#endif

		/* report the number of lines in the file */
		msg("\"%s\" %s %ld line%s, %ld char%s",
			origname,
			(tstflag(file, READONLY) ? "[READONLY]" : ""),
			nlines,
			nlines == 1 ? "" : "s",
			nbytes,
			nbytes == 1 ? "" : "s");
	}

	/* initialize the cursor to start of line 1 */
	cursor = MARK_FIRST;

	/* close the original file */
	close(origfd);

	/* any other messages? */
	if (tstflag(file, HADNUL))
	{
		msg("This file contained NULs.  They've been changed to \\x80 chars");
	}
	if (tstflag(file, ADDEDNL))
	{
		msg("Newline characters have been inserted to break up long lines");
	}
#ifndef CRUNCH
	if (tstflag(file, HADBS))
	{
		msg("Backspace characters deleted due to ':set beautify'");
	}
#endif

	storename(origname);

#ifndef NO_MODELINES
	if (nlines > 10)
	{
		do_modelines(1L, 5L);
		do_modelines(nlines - 4L, nlines);
	}
	else
	{
		do_modelines(1L, nlines);
	}
#endif

	/* force all blocks out onto the disk, to support file recovery */
	blksync();

	return 0;
}



/* This function copies the temp file back onto an original file.
 * Returns TRUE if successful, or FALSE if the file could NOT be saved.
 */
int tmpsave(filename, bang)
	char	*filename;	/* the name to save it to */
	int	bang;		/* forced write? */
{
	int		fd;	/* fd of the file we're writing to */
	REG int		len;	/* length of a text block */
	REG BLK		*this;	/* a text block */
	long		bytes;	/* byte counter */
	REG int		i;

	/* if no filename is given, assume the original file name */
	if (!filename || !*filename)
	{
		filename = origname;
	}

	/* if still no file name, then fail */
	if (!*filename)
	{
		msg("Don't know a name for this file -- NOT WRITTEN");
		return FALSE;
	}

	/* can't rewrite a READONLY file */
	if (!strcmp(filename, origname) && tstflag(file, READONLY) && !bang)
	{
		msg("\"%s\" [READONLY] -- NOT WRITTEN", filename);
		return FALSE;
	}

	/* open the file */
	if (*filename == '>' && filename[1] == '>')
	{
		filename += 2;
		while (*filename == ' ' || *filename == '\t')
		{
			filename++;
		}
#ifdef O_APPEND
		fd = open(filename, O_WRONLY|O_APPEND);
#else
		fd = open(filename, O_WRONLY);
		lseek(fd, 0L, 2);
#endif
	}
	else
	{
		/* either the file must not exist, or it must be the original
		 * file, or we must have a bang, or "writeany" must be set.
		 */
		if (strcmp(filename, origname) && access(filename, 0) == 0 && !bang
#ifndef CRUNCH
		    && !*o_writeany
#endif
				   )
		{
			msg("File already exists - Use :w! to overwrite");
			return FALSE;
		}
#if VMS
		/* Create a new VMS version of this file. */
		{ 
		char *strrchr(), *ptr = strrchr(filename,';');
		if (ptr) *ptr = '\0';  /* Snip off any ;number in the name */
		}
#endif
		fd = creat(filename, FILEPERMS);
	}
	if (fd < 0)
	{
		msg("Can't write to \"%s\" -- NOT WRITTEN", filename);
		return FALSE;
	}

	/* write each text block to the file */
	bytes = 0L;
	for (i = 1; i < MAXBLKS && (this = blkget(i)) && this->c[0]; i++)
	{
		for (len = 0; len < BLKSIZE && this->c[len]; len++)
		{
		}
		if (twrite(fd, this->c, len) < len)
		{
			msg("Trouble writing to \"%s\"", filename);
			if (!strcmp(filename, origname))
			{
				setflag(file, MODIFIED);
			}
			close(fd);
			return FALSE;
		}
		bytes += len;
	}

	/* reset the "modified" flag, but not the "undoable" flag */
	clrflag(file, MODIFIED);
	significant = FALSE;
	if (!strcmp(origname, filename))
	{
		exitcode &= ~1;
	}

	/* report lines & characters */
#if MSDOS || TOS
	bytes += nlines; /* for the inserted carriage returns */
#endif
	msg("Wrote \"%s\"  %ld lines, %ld characters", filename, nlines, bytes);

	/* close the file */
	close(fd);

	return TRUE;
}


/* This function deletes the temporary file.  If the file has been modified
 * and "bang" is FALSE, then it returns FALSE without doing anything; else
 * it returns TRUE.
 *
 * If the "autowrite" option is set, then instead of returning FALSE when
 * the file has been modified and "bang" is false, it will call tmpend().
 */
int tmpabort(bang)
	int	bang;
{
	/* if there is no file, return successfully */
	if (tmpfd < 0)
	{
		return TRUE;
	}

	/* see if we must return FALSE -- can't quit */
	if (!bang && tstflag(file, MODIFIED))
	{
		/* if "autowrite" is set, then act like tmpend() */
		if (*o_autowrite)
			return tmpend(bang);
		else
			return FALSE;
	}

	/* delete the tmp file */
	cutswitch();
	strcpy(prevorig, origname);
	prevline = markline(cursor);
	*origname = '\0';
	origtime = 0L;
	blkinit();
	nlines = 0;
	initflags();
	return TRUE;
}

/* This function saves the file if it has been modified, and then deletes
 * the temporary file. Returns TRUE if successful, or FALSE if the file
 * needs to be saved but can't be.  When it returns FALSE, it will not have
 * deleted the tmp file, either.
 */
int tmpend(bang)
	int	bang;
{
	/* save the file if it has been modified */
	if (tstflag(file, MODIFIED) && !tmpsave((char *)0, FALSE) && !bang)
	{
		return FALSE;
	}

	/* delete the tmp file */
	tmpabort(TRUE);

	return TRUE;
}


/* If the tmp file has been changed, then this function will force those
 * changes to be written to the disk, so that the tmp file will survive a
 * system crash or power failure.
 */
#if AMIGA || MSDOS || TOS
sync()
{
	/* MS-DOS and TOS don't flush their buffers until the file is closed,
	 * so here we close the tmp file and then immediately reopen it.
	 */
	close(tmpfd);
	tmpfd = open(tmpname, O_RDWR | O_BINARY);
	return 0;
}
#endif


/* This function stores the file's name in the second block of the temp file.
 * SLEAZE ALERT!  SLEAZE ALERT!  The "tmpblk" buffer is probably being used
 * to store the arguments to a command, so we can't use it here.  Instead,
 * we'll borrow the buffer that is used for "shift-U".
 */
int
storename(name)
	char	*name;	/* the name of the file - normally origname */
{
#ifndef CRUNCH
	int	len;
	char	*ptr;
#endif

	/* we're going to clobber the U_text buffer, so reset U_line */
	U_line = 0L;

	if (!name)
	{
		strncpy(U_text, "", BLKSIZE);
		U_text[1] = 127;
	}
#ifndef CRUNCH
# if TOS || MINT || MSDOS || AMIGA
	else if (*name != '/' && *name != '\\' && !(*name && name[1] == ':'))
# else
	else if (*name != SLASH)
# endif
	{
		/* get the directory name */
		ptr = getcwd(U_text, BLKSIZE);
		if (ptr != U_text)
		{
			strcpy(U_text, ptr);
		}

		/* append a slash to the directory name */
		len = strlen(U_text);
		U_text[len++] = SLASH;

		/* append the filename, padded with heaps o' NULs */
		strncpy(U_text + len, *name ? name : "foo", BLKSIZE - len);
	}
#endif
	else
	{
		/* copy the filename into U_text */
		strncpy(U_text, *name ? name : "foo", BLKSIZE);
	}

	if (tmpfd >= 0)
	{
		/* write the name out to second block of the temp file */
		lseek(tmpfd, (long)BLKSIZE, 0);
		if (write(tmpfd, U_text, (unsigned)BLKSIZE) < BLKSIZE)
		{
			FAIL("Error stuffing name \"%s\" into temp file", U_text);
		}
	}
	return 0;
}



/* This function handles deadly signals.  It restores sanity to the terminal
 * preserves the current temp file, and deletes any old temp files.
 */
SIGTYPE deathtrap(sig)
	int	sig;	/* the deadly signal that we caught */
{
	char	*why;

	/* restore the terminal's sanity */
	endwin();

#ifdef CRUNCH
	why = "-Elvis died";
#else
	/* give a more specific description of how Elvis died */
	switch (sig)
	{
# ifdef SIGHUP
	  case SIGHUP:	why = "-the modem lost its carrier";		break;
# endif
# ifndef DEBUG
#  ifdef SIGILL
	  case SIGILL:	why = "-Elvis hit an illegal instruction";	break;
#  endif
#  ifdef SIGBUS
	  case SIGBUS:	why = "-Elvis had a bus error";			break;
#  endif
#  ifdef SIGSEGV
#   if !TOS
	  case SIGSEGV:	why = "-Elvis had a segmentation violation";	break;
#   endif
#  endif
#  ifdef SIGSYS
	  case SIGSYS:	why = "-Elvis munged a system call";		break;
#  endif
# endif /* !DEBUG */
# ifdef SIGPIPE
	  case SIGPIPE:	why = "-the pipe reader died";			break;
# endif
# ifdef SIGTERM
	  case SIGTERM:	why = "-Elvis was terminated";			break;
# endif
# if !MINIX
#  ifdef SIGUSR1
	  case SIGUSR1:	why = "-Elvis was killed via SIGUSR1";		break;
#  endif
#  ifdef SIGUSR2
	  case SIGUSR2:	why = "-Elvis was killed via SIGUSR2";		break;
#  endif
# endif
	  default:	why = "-Elvis died";				break;
	}
#endif

	/* if we had a temp file going, then preserve it */
	if (tmpnum > 0 && tmpfd >= 0)
	{
		close(tmpfd);
		tmpfd = -1;
		sprintf(tmpblk.c, "%s \"%s\" %s", PRESERVE, why, tmpname);
		system(tmpblk.c);
	}

	/* delete any old temp files */
	cutend();

	/* exit with the proper exit status */
	exit(sig);
}
