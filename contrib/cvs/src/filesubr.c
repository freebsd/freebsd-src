/* filesubr.c --- subroutines for dealing with files
   Jim Blandy <jimb@cyclic.com>

   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* These functions were moved out of subr.c because they need different
   definitions under operating systems (like, say, Windows NT) with different
   file system semantics.  */

#include "cvs.h"

/*
 * I don't know of a convenient way to test this at configure time, or else
 * I'd certainly do it there.
 */
#if defined(NeXT)
#define LOSING_TMPNAM_FUNCTION
#endif

static int deep_remove_dir PROTO((const char *path));

/*
 * Copies "from" to "to".
 */
void
copy_file (from, to)
    const char *from;
    const char *to;
{
    struct stat sb;
    struct utimbuf t;
    int fdin, fdout;

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> copy(%s,%s)\n",
			(server_active) ? 'S' : ' ', from, to);
#else
	(void) fprintf (stderr, "-> copy(%s,%s)\n", from, to);
#endif
    if (noexec)
	return;

    if ((fdin = open (from, O_RDONLY)) < 0)
	error (1, errno, "cannot open %s for copying", from);
    if (fstat (fdin, &sb) < 0)
	error (1, errno, "cannot fstat %s", from);
    if ((fdout = creat (to, (int) sb.st_mode & 07777)) < 0)
	error (1, errno, "cannot create %s for copying", to);
    if (sb.st_size > 0)
    {
	char buf[BUFSIZ];
	int n;

	for (;;) 
	{
	    n = read (fdin, buf, sizeof(buf));
	    if (n == -1)
	    {
#ifdef EINTR
		if (errno == EINTR)
		    continue;
#endif
		error (1, errno, "cannot read file %s for copying", from);
	    }
            else if (n == 0) 
		break;
  
	    if (write(fdout, buf, n) != n) {
		error (1, errno, "cannot write file %s for copying", to);
	    }
	}

#ifdef HAVE_FSYNC
	if (fsync (fdout)) 
	    error (1, errno, "cannot fsync file %s after copying", to);
#endif
    }

    if (close (fdin) < 0) 
	error (0, errno, "cannot close %s", from);
    if (close (fdout) < 0)
	error (1, errno, "cannot close %s", to);

    /* now, set the times for the copied file to match those of the original */
    memset ((char *) &t, 0, sizeof (t));
    t.actime = sb.st_atime;
    t.modtime = sb.st_mtime;
    (void) utime (to, &t);
}

/* FIXME-krp: these functions would benefit from caching the char * &
   stat buf.  */

/*
 * Returns non-zero if the argument file is a directory, or is a symbolic
 * link which points to a directory.
 */
int
isdir (file)
    const char *file;
{
    struct stat sb;

    if (stat (file, &sb) < 0)
	return (0);
    return (S_ISDIR (sb.st_mode));
}

/*
 * Returns non-zero if the argument file is a symbolic link.
 */
int
islink (file)
    const char *file;
{
#ifdef S_ISLNK
    struct stat sb;

    if (lstat (file, &sb) < 0)
	return (0);
    return (S_ISLNK (sb.st_mode));
#else
    return (0);
#endif
}

/*
 * Returns non-zero if the argument file exists.
 */
int
isfile (file)
    const char *file;
{
    return isaccessible(file, F_OK);
}

/*
 * Returns non-zero if the argument file is readable.
 */
int
isreadable (file)
    const char *file;
{
    return isaccessible(file, R_OK);
}

/*
 * Returns non-zero if the argument file is writable.
 */
int
iswritable (file)
    const char *file;
{
    return isaccessible(file, W_OK);
}

/*
 * Returns non-zero if the argument file is accessable according to
 * mode.  If compiled with SETXID_SUPPORT also works if cvs has setxid
 * bits set.
 */
int
isaccessible (file, mode)
    const char *file;
    const int mode;
{
#ifdef SETXID_SUPPORT
    struct stat sb;
    int umask = 0;
    int gmask = 0;
    int omask = 0;
    int uid;
    
    if (stat(file, &sb) == -1)
	return 0;
    if (mode == F_OK)
	return 1;

    uid = geteuid();
    if (uid == 0)		/* superuser */
    {
	if (mode & X_OK)
	    return sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH);
	else
	    return 1;
    }
	
    if (mode & R_OK)
    {
	umask |= S_IRUSR;
	gmask |= S_IRGRP;
	omask |= S_IROTH;
    }
    if (mode & W_OK)
    {
	umask |= S_IWUSR;
	gmask |= S_IWGRP;
	omask |= S_IWOTH;
    }
    if (mode & X_OK)
    {
	umask |= S_IXUSR;
	gmask |= S_IXGRP;
	omask |= S_IXOTH;
    }

    if (sb.st_uid == uid)
	return (sb.st_mode & umask) == umask;
    else if (sb.st_gid == getegid())
	return (sb.st_mode & gmask) == gmask;
    else
	return (sb.st_mode & omask) == omask;
#else
    return access(file, mode) == 0;
#endif
}

/*
 * Open a file and die if it fails
 */
FILE *
open_file (name, mode)
    const char *name;
    const char *mode;
{
    FILE *fp;

    if ((fp = fopen (name, mode)) == NULL)
	error (1, errno, "cannot open %s", name);
    return (fp);
}

/*
 * Make a directory and die if it fails
 */
void
make_directory (name)
    const char *name;
{
    struct stat sb;

    if (stat (name, &sb) == 0 && (!S_ISDIR (sb.st_mode)))
	    error (0, 0, "%s already exists but is not a directory", name);
    if (!noexec && mkdir (name, 0777) < 0)
	error (1, errno, "cannot make directory %s", name);
}

/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
void
make_directories (name)
    const char *name;
{
    char *cp;

    if (noexec)
	return;

    if (mkdir (name, 0777) == 0 || errno == EEXIST)
	return;
    if (! existence_error (errno))
    {
	error (0, errno, "cannot make path to %s", name);
	return;
    }
    if ((cp = strrchr (name, '/')) == NULL)
	return;
    *cp = '\0';
    make_directories (name);
    *cp++ = '/';
    if (*cp == '\0')
	return;
    (void) mkdir (name, 0777);
}

/*
 * Change the mode of a file, either adding write permissions, or removing
 * all write permissions.  Either change honors the current umask setting.
 */
void
xchmod (fname, writable)
    char *fname;
    int writable;
{
    struct stat sb;
    mode_t mode, oumask;

    if (stat (fname, &sb) < 0)
    {
	if (!noexec)
	    error (0, errno, "cannot stat %s", fname);
	return;
    }
    oumask = umask (0);
    (void) umask (oumask);
    if (writable)
    {
	mode = sb.st_mode | (~oumask
			     & (((sb.st_mode & S_IRUSR) ? S_IWUSR : 0)
				| ((sb.st_mode & S_IRGRP) ? S_IWGRP : 0)
				| ((sb.st_mode & S_IROTH) ? S_IWOTH : 0)));
    }
    else
    {
	mode = sb.st_mode & ~(S_IWRITE | S_IWGRP | S_IWOTH) & ~oumask;
    }

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> chmod(%s,%o)\n",
			(server_active) ? 'S' : ' ', fname,
			(unsigned int) mode);
#else
	(void) fprintf (stderr, "-> chmod(%s,%o)\n", fname,
			(unsigned int) mode);
#endif
    if (noexec)
	return;

    if (chmod (fname, mode) < 0)
	error (0, errno, "cannot change mode of file %s", fname);
}

/*
 * Rename a file and die if it fails
 */
void
rename_file (from, to)
    const char *from;
    const char *to;
{
    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> rename(%s,%s)\n",
			(server_active) ? 'S' : ' ', from, to);
#else
	(void) fprintf (stderr, "-> rename(%s,%s)\n", from, to);
#endif
    if (noexec)
	return;

    if (rename (from, to) < 0)
	error (1, errno, "cannot rename file %s to %s", from, to);
}

/*
 * link a file, if possible.  Warning: the Windows NT version of this
 * function just copies the file, so only use this function in ways
 * that can deal with either a link or a copy.
 */
int
link_file (from, to)
    const char *from;
    const char *to;
{
    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> link(%s,%s)\n",
			(server_active) ? 'S' : ' ', from, to);
#else
	(void) fprintf (stderr, "-> link(%s,%s)\n", from, to);
#endif
    if (noexec)
	return (0);

    return (link (from, to));
}

/*
 * unlink a file, if possible.
 */
int
unlink_file (f)
    const char *f;
{
    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> unlink(%s)\n",
			(server_active) ? 'S' : ' ', f);
#else
	(void) fprintf (stderr, "-> unlink(%s)\n", f);
#endif
    if (noexec)
	return (0);

    return (unlink (f));
}

/*
 * Unlink a file or dir, if possible.  If it is a directory do a deep
 * removal of all of the files in the directory.  Return -1 on error
 * (in which case errno is set).
 */
int
unlink_file_dir (f)
    const char *f;
{
    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> unlink_file_dir(%s)\n",
			(server_active) ? 'S' : ' ', f);
#else
	(void) fprintf (stderr, "-> unlink_file_dir(%s)\n", f);
#endif
    if (noexec)
	return (0);

    /* For at least some unices, if root tries to unlink() a directory,
       instead of doing something rational like returning EISDIR,
       the system will gleefully go ahead and corrupt the filesystem.
       So we first call isdir() to see if it is OK to call unlink().  This
       doesn't quite work--if someone creates a directory between the
       call to isdir() and the call to unlink(), we'll still corrupt
       the filesystem.  Where is the Unix Haters Handbook when you need
       it?  */
    if (isdir(f)) 
	return deep_remove_dir(f);
    else
    {
	if (unlink (f) != 0)
	    return -1;
    }
    /* We were able to remove the file from the disk */
    return 0;
}

/* Remove a directory and everything it contains.  Returns 0 for
 * success, -1 for failure (in which case errno is set).
 */

static int
deep_remove_dir (path)
    const char *path;
{
    DIR		  *dirp;
    struct dirent *dp;
    char	   buf[PATH_MAX];

    if (rmdir (path) != 0 && (errno == ENOTEMPTY || errno == EEXIST)) 
    {
	if ((dirp = opendir (path)) == NULL)
	    /* If unable to open the directory return
	     * an error
	     */
	    return -1;

	while ((dp = readdir (dirp)) != NULL)
	{
	    if (strcmp (dp->d_name, ".") == 0 ||
			strcmp (dp->d_name, "..") == 0)
		continue;

	    sprintf (buf, "%s/%s", path, dp->d_name);

	    /* See comment in unlink_file_dir explanation of why we use
	       isdir instead of just calling unlink and checking the
	       status.  */
	    if (isdir(buf)) 
	    {
		if (deep_remove_dir(buf))
		{
		    closedir(dirp);
		    return -1;
		}
	    }
	    else
	    {
		if (unlink (buf) != 0)
		{
		    closedir(dirp);
		    return -1;
		}
	    }
	}
	closedir (dirp);
	return rmdir (path);
	}

    /* Was able to remove the directory return 0 */
    return 0;
}

/* Read NCHARS bytes from descriptor FD into BUF.
   Return the number of characters successfully read.
   The number returned is always NCHARS unless end-of-file or error.  */
static size_t
block_read (fd, buf, nchars)
    int fd;
    char *buf;
    size_t nchars;
{
    char *bp = buf;
    size_t nread;

    do 
    {
	nread = read (fd, bp, nchars);
	if (nread == (size_t)-1)
	{
#ifdef EINTR
	    if (errno == EINTR)
		continue;
#endif
	    return (size_t)-1;
	}

	if (nread == 0)
	    break; 

	bp += nread;
	nchars -= nread;
    } while (nchars != 0);

    return bp - buf;
} 

    
/*
 * Compare "file1" to "file2". Return non-zero if they don't compare exactly.
 */
int
xcmp (file1, file2)
    const char *file1;
    const char *file2;
{
    char *buf1, *buf2;
    struct stat sb1, sb2;
    int fd1, fd2;
    int ret;

    if ((fd1 = open (file1, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", file1);
    if ((fd2 = open (file2, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", file2);
    if (fstat (fd1, &sb1) < 0)
	error (1, errno, "cannot fstat %s", file1);
    if (fstat (fd2, &sb2) < 0)
	error (1, errno, "cannot fstat %s", file2);

    /* A generic file compare routine might compare st_dev & st_ino here 
       to see if the two files being compared are actually the same file.
       But that won't happen in CVS, so we won't bother. */

    if (sb1.st_size != sb2.st_size)
	ret = 1;
    else if (sb1.st_size == 0)
	ret = 0;
    else
    {
	/* FIXME: compute the optimal buffer size by computing the least
	   common multiple of the files st_blocks field */
	size_t buf_size = 8 * 1024;
	size_t read1;
	size_t read2;

	buf1 = xmalloc (buf_size);
	buf2 = xmalloc (buf_size);

	do 
	{
	    read1 = block_read (fd1, buf1, buf_size);
	    if (read1 == (size_t)-1)
		error (1, errno, "cannot read file %s for comparing", file1);

	    read2 = block_read (fd2, buf2, buf_size);
	    if (read2 == (size_t)-1)
		error (1, errno, "cannot read file %s for comparing", file2);

	    /* assert (read1 == read2); */

	    ret = memcmp(buf1, buf2, read1);
	} while (ret == 0 && read1 == buf_size);

	free (buf1);
	free (buf2);
    }
	
    (void) close (fd1);
    (void) close (fd2);
    return (ret);
}

#ifdef LOSING_TMPNAM_FUNCTION
char *tmpnam(char *s)
{
    static char value[L_tmpnam+1];

    if (s){
       strcpy(s,"/tmp/cvsXXXXXX");
       mktemp(s);
       return s;
    }else{
       strcpy(value,"/tmp/cvsXXXXXX");
       mktemp(s);
       return value;
    }
}
#endif

/* Return non-zero iff FILENAME is absolute.
   Trivial under Unix, but more complicated under other systems.  */
int
isabsolute (filename)
    const char *filename;
{
    return filename[0] == '/';
}


/* Return a pointer into PATH's last component.  */
char *
last_component (path)
    char *path;
{
    char *last = strrchr (path, '/');

    if (last)
        return last + 1;
    else
        return path;
}

/* Return the home directory.  Returns a pointer to storage
   managed by this function or its callees (currently getenv).  */
char *
get_homedir ()
{
    return getenv ("HOME");
}

/* See cvs.h for description.  On unix this does nothing, because the
   shell expands the wildcards.  */
void
expand_wild (argc, argv, pargc, pargv)
    int argc;
    char **argv;
    int *pargc;
    char ***pargv;
{
    int i;
    *pargc = argc;
    *pargv = (char **) xmalloc (argc * sizeof (char *));
    for (i = 0; i < argc; ++i)
	(*pargv)[i] = xstrdup (argv[i]);
}
