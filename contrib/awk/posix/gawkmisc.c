/* gawkmisc.c --- miscellanious gawk routines that are OS specific.
 
   Copyright (C) 1986, 1988, 1989, 1991 - 96 the Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

char quote = '\'';
char *defpath = DEFPATH;
char envsep = ':';

/* gawk_name --- pull out the "gawk" part from how the OS called us */

char *
gawk_name(filespec)
const char *filespec;
{
	char *p;
    
	/* "path/name" -> "name" */
	p = strrchr(filespec, '/');
	return (p == NULL ? (char *) filespec : p + 1);
}

/* os_arg_fixup --- fixup the command line */

void
os_arg_fixup(argcp, argvp)
int *argcp;
char ***argvp;
{
	/* no-op */
	return;
}

/* os_devopen --- open special per-OS devices */

int
os_devopen(name, flag)
const char *name;
int flag;
{
	/* no-op */
	return INVALID_HANDLE;
}

/* optimal_bufsize --- determine optimal buffer size */

int
optimal_bufsize(fd, stb)
int fd;
struct stat *stb;
{
	/* force all members to zero in case OS doesn't use all of them. */
	memset(stb, '\0', sizeof(struct stat));

	/*
	 * System V.n, n < 4, doesn't have the file system block size in the
	 * stat structure. So we have to make some sort of reasonable
	 * guess. We use stdio's BUFSIZ, since that is what it was
	 * meant for in the first place.
	 */
#ifdef HAVE_ST_BLKSIZE
#define DEFBLKSIZE	(stb->st_blksize ? stb->st_blksize : BUFSIZ)
#else
#define	DEFBLKSIZE	BUFSIZ
#endif

	if (isatty(fd))
		return BUFSIZ;
	if (fstat(fd, stb) == -1)
		fatal("can't stat fd %d (%s)", fd, strerror(errno));
	if (lseek(fd, (off_t)0, 0) == -1)	/* not a regular file */
		return DEFBLKSIZE;
	if (stb->st_size > 0 && stb->st_size < DEFBLKSIZE) /* small file */
		return stb->st_size;
	return DEFBLKSIZE;
}

/* ispath --- return true if path has directory components */

int
ispath(file)
const char *file;
{
	return (strchr(file, '/') != NULL);
}

/* isdirpunct --- return true if char is a directory separator */

int
isdirpunct(c)
int c;
{
	return (c == '/');
}

