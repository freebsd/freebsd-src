/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Various useful functions for the CVS support code.
 */

#include "cvs.h"
#include "getline.h"

#ifdef HAVE_NANOSLEEP
# include "xtime.h"
#else /* HAVE_NANOSLEEP */
# if !defined HAVE_USLEEP && defined HAVE_SELECT
    /* use select as a workaround */
#   include "xselect.h"
# endif /* !defined HAVE_USLEEP && defined HAVE_SELECT */
#endif /* !HAVE_NANOSLEEP */

extern char *getlogin ();

/*
 * malloc some data and die if it fails
 */
void *
xmalloc (bytes)
    size_t bytes;
{
    char *cp;

    /* Parts of CVS try to xmalloc zero bytes and then free it.  Some
       systems have a malloc which returns NULL for zero byte
       allocations but a free which can't handle NULL, so compensate. */
    if (bytes == 0)
	bytes = 1;

    cp = malloc (bytes);
    if (cp == NULL)
    {
	char buf[80];
	sprintf (buf, "out of memory; can not allocate %lu bytes",
		 (unsigned long) bytes);
	error (1, 0, buf);
    }
    return (cp);
}

/*
 * realloc data and die if it fails [I've always wanted to have "realloc" do
 * a "malloc" if the argument is NULL, but you can't depend on it.  Here, I
 * can *force* it.
 */
void *
xrealloc (ptr, bytes)
    void *ptr;
    size_t bytes;
{
    char *cp;

    if (!ptr)
	cp = malloc (bytes);
    else
	cp = realloc (ptr, bytes);

    if (cp == NULL)
    {
	char buf[80];
	sprintf (buf, "out of memory; can not reallocate %lu bytes",
		 (unsigned long) bytes);
	error (1, 0, buf);
    }
    return (cp);
}

/* Two constants which tune expand_string.  Having MIN_INCR as large
   as 1024 might waste a bit of memory, but it shouldn't be too bad
   (CVS used to allocate arrays of, say, 3000, PATH_MAX (8192, often),
   or other such sizes).  Probably anything which is going to allocate
   memory which is likely to get as big as MAX_INCR shouldn't be doing
   it in one block which must be contiguous, but since getrcskey does
   so, we might as well limit the wasted memory to MAX_INCR or so
   bytes.

   MIN_INCR and MAX_INCR should both be powers of two and we generally
   try to keep our allocations to powers of two for the most part.
   Most malloc implementations these days tend to like that.  */

#define MIN_INCR 1024
#define MAX_INCR (2*1024*1024)

/* *STRPTR is a pointer returned from malloc (or NULL), pointing to *N
   characters of space.  Reallocate it so that points to at least
   NEWSIZE bytes of space.  Gives a fatal error if out of memory;
   if it returns it was successful.  */
void
expand_string (strptr, n, newsize)
    char **strptr;
    size_t *n;
    size_t newsize;
{
    if (*n < newsize)
    {
	while (*n < newsize)
	{
	    if (*n < MIN_INCR)
		*n = MIN_INCR;
	    else if (*n >= MAX_INCR)
		*n += MAX_INCR;
	    else
	    {
		*n *= 2;
		if (*n > MAX_INCR)
		    *n = MAX_INCR;
	    }
	}
	*strptr = xrealloc (*strptr, *n);
    }
}

/* *STR is a pointer to a malloc'd string.  *LENP is its allocated
   length.  Add SRC to the end of it, reallocating if necessary.  */
void
xrealloc_and_strcat (str, lenp, src)
    char **str;
    size_t *lenp;
    const char *src;
{

    expand_string (str, lenp, strlen (*str) + strlen (src) + 1);
    strcat (*str, src);
}

/*
 * Duplicate a string, calling xmalloc to allocate some dynamic space
 */
char *
xstrdup (str)
    const char *str;
{
    char *s;

    if (str == NULL)
	return ((char *) NULL);
    s = xmalloc (strlen (str) + 1);
    (void) strcpy (s, str);
    return (s);
}

/* Remove trailing newlines from STRING, destructively. */
void
strip_trailing_newlines (str)
     char *str;
{
    int len;
    len = strlen (str) - 1;

    while (str[len] == '\n')
	str[len--] = '\0';
}

/* Return the number of levels that path ascends above where it starts.
   For example:
   "../../foo" -> 2
   "foo/../../bar" -> 1
   */
/* FIXME: Should be using ISDIRSEP, last_component, or some other
   mechanism which is more general than just looking at slashes,
   particularly for the client.c caller.  The server.c caller might
   want something different, so be careful.  */
int
pathname_levels (path)
    char *path;
{
    char *p;
    char *q;
    int level;
    int max_level;

    max_level = 0;
    p = path;
    level = 0;
    do
    {
	q = strchr (p, '/');
	if (q != NULL)
	    ++q;
	if (p[0] == '.' && p[1] == '.' && (p[2] == '\0' || p[2] == '/'))
	{
	    --level;
	    if (-level > max_level)
		max_level = -level;
	}
	else if (p[0] == '\0' || p[0] == '/' ||
		 (p[0] == '.' && (p[1] == '\0' || p[1] == '/')))
	    ;
	else
	    ++level;
	p = q;
    } while (p != NULL);
    return max_level;
}


/* Free a vector, where (*ARGV)[0], (*ARGV)[1], ... (*ARGV)[*PARGC - 1]
   are malloc'd and so is *ARGV itself.  Such a vector is allocated by
   line2argv or expand_wild, for example.  */
void
free_names (pargc, argv)
    int *pargc;
    char **argv;
{
    register int i;

    for (i = 0; i < *pargc; i++)
    {					/* only do through *pargc */
	free (argv[i]);
    }
    free (argv);
    *pargc = 0;				/* and set it to zero when done */
}

/* Convert LINE into arguments separated by SEPCHARS.  Set *ARGC
   to the number of arguments found, and (*ARGV)[0] to the first argument,
   (*ARGV)[1] to the second, etc.  *ARGV is malloc'd and so are each of
   (*ARGV)[0], (*ARGV)[1], ...  Use free_names() to return the memory
   allocated here back to the free pool.  */
void
line2argv (pargc, argv, line, sepchars)
    int *pargc;
    char ***argv;
    char *line;
    char *sepchars;
{
    char *cp;
    /* Could make a case for size_t or some other unsigned type, but
       we'll stick with int to avoid signed/unsigned warnings when
       comparing with *pargc.  */
    int argv_allocated;

    /* Small for testing.  */
    argv_allocated = 1;
    *argv = (char **) xmalloc (argv_allocated * sizeof (**argv));

    *pargc = 0;
    for (cp = strtok (line, sepchars); cp; cp = strtok ((char *) NULL, sepchars))
    {
	if (*pargc == argv_allocated)
	{
	    argv_allocated *= 2;
	    *argv = xrealloc (*argv, argv_allocated * sizeof (**argv));
	}
	(*argv)[*pargc] = xstrdup (cp);
	(*pargc)++;
    }
}

/*
 * Returns the number of dots ('.') found in an RCS revision number
 */
int
numdots (s)
    const char *s;
{
    int dots = 0;

    for (; *s; s++)
    {
	if (*s == '.')
	    dots++;
    }
    return (dots);
}

/* Compare revision numbers REV1 and REV2 by consecutive fields.
   Return negative, zero, or positive in the manner of strcmp.  The
   two revision numbers must have the same number of fields, or else
   compare_revnums will return an inaccurate result. */
int
compare_revnums (rev1, rev2)
    const char *rev1;
    const char *rev2;
{
    const char *s, *sp;
    const char *t, *tp;
    char *snext, *tnext;
    int result = 0;

    sp = s = rev1;
    tp = t = rev2;
    while (result == 0)
    {
	result = strtoul (sp, &snext, 10) - strtoul (tp, &tnext, 10);
	if (*snext == '\0' || *tnext == '\0')
	    break;
	sp = snext + 1;
	tp = tnext + 1;
    }

    return result;
}

char *
increment_revnum (rev)
    const char *rev;
{
    char *newrev, *p;
    int lastfield;
    size_t len = strlen (rev);

    newrev = (char *) xmalloc (len + 2);
    memcpy (newrev, rev, len + 1);
    p = strrchr (newrev, '.');
    if (p == NULL)
    {
	free (newrev);
	return NULL;
    }
    lastfield = atoi (++p);
    sprintf (p, "%d", lastfield + 1);

    return newrev;
}

/* Return the username by which the caller should be identified in
   CVS, in contexts such as the author field of RCS files, various
   logs, etc.  */
char *
getcaller ()
{
#ifndef SYSTEM_GETCALLER
    static char *cache;
    struct passwd *pw;
    uid_t uid;
#endif

    /* If there is a CVS username, return it.  */
#ifdef AUTH_SERVER_SUPPORT
    if (CVS_Username != NULL)
	return CVS_Username;
#endif

#ifdef SYSTEM_GETCALLER
    return SYSTEM_GETCALLER ();
#else
    /* Get the caller's login from his uid.  If the real uid is "root"
       try LOGNAME USER or getlogin(). If getlogin() and getpwuid()
       both fail, return the uid as a string.  */

    if (cache != NULL)
	return cache;

    uid = getuid ();
    if (uid == (uid_t) 0)
    {
	char *name;

	/* super-user; try getlogin() to distinguish */
	if (((name = getlogin ()) || (name = getenv("LOGNAME")) ||
	     (name = getenv("USER"))) && *name)
	{
	    cache = xstrdup (name);
	    return cache;
	}
    }
    if ((pw = (struct passwd *) getpwuid (uid)) == NULL)
    {
	char uidname[20];

	(void) sprintf (uidname, "uid%lu", (unsigned long) uid);
	cache = xstrdup (uidname);
	return cache;
    }
    cache = xstrdup (pw->pw_name);
    return cache;
#endif
}

#ifdef lint
#ifndef __GNUC__
/* ARGSUSED */
time_t
get_date (date, now)
    char *date;
    struct timeb *now;
{
    time_t foo = 0;

    return (foo);
}
#endif
#endif

/* Given two revisions, find their greatest common ancestor.  If the
   two input revisions exist, then rcs guarantees that the gca will
   exist.  */

char *
gca (rev1, rev2)
    const char *rev1;
    const char *rev2;
{
    int dots;
    char *gca, *g;
    const char *p1, *p2;
    int r1, r2;
    char *retval;

    if (rev1 == NULL || rev2 == NULL)
    {
	error (0, 0, "sanity failure in gca");
	abort();
    }

    /* The greatest common ancestor will have no more dots, and numbers
       of digits for each component no greater than the arguments.  Therefore
       this string will be big enough.  */
    g = gca = xmalloc (strlen (rev1) + strlen (rev2) + 100);

    /* walk the strings, reading the common parts. */
    p1 = rev1;
    p2 = rev2;
    do
    {
	r1 = strtol (p1, (char **) &p1, 10);
	r2 = strtol (p2, (char **) &p2, 10);
	
	/* use the lowest. */
	(void) sprintf (g, "%d.", r1 < r2 ? r1 : r2);
	g += strlen (g);
	if (*p1 == '.') ++p1;
	else break;
	if (*p2 == '.') ++p2;
	else break;
    } while (r1 == r2);

    /* erase that last dot. */
    *--g = '\0';

    /* numbers differ, or we ran out of strings.  we're done with the
       common parts.  */

    dots = numdots (gca);
    if (dots == 0)
    {
	/* revisions differ in trunk major number.  */

	if (r2 < r1) p1 = p2;
	if (*p1 == '\0')
	{
	    /* we only got one number.  this is strange.  */
	    error (0, 0, "bad revisions %s or %s", rev1, rev2);
	    abort();
	}
	else
	{
	    /* we have a minor number.  use it.  */
	    *g++ = '.';
	    while (*p1 != '.' && *p1 != '\0')
		*g++ = *p1++;
	    *g = '\0';
	}
    }
    else if ((dots & 1) == 0)
    {
	/* if we have an even number of dots, then we have a branch.
	   remove the last number in order to make it a revision.  */
	
	g = strrchr (gca, '.');
	*g = '\0';
    }

    retval = xstrdup (gca);
    free (gca);
    return retval;
}

/* Give fatal error if REV is numeric and ARGC,ARGV imply we are
   planning to operate on more than one file.  The current directory
   should be the working directory.  Note that callers assume that we
   will only be checking the first character of REV; it need not have
   '\0' at the end of the tag name and other niceties.  Right now this
   is only called from admin.c, but if people like the concept it probably
   should also be called from diff -r, update -r, get -r, and log -r.  */

void
check_numeric (rev, argc, argv)
    const char *rev;
    int argc;
    char **argv;
{
    if (rev == NULL || !isdigit ((unsigned char) *rev))
	return;

    /* Note that the check for whether we are processing more than one
       file is (basically) syntactic; that is, we don't behave differently
       depending on whether a directory happens to contain only a single
       file or whether it contains more than one.  I strongly suspect this
       is the least confusing behavior.  */
    if (argc != 1
	|| (!wrap_name_has (argv[0], WRAP_TOCVS) && isdir (argv[0])))
    {
	error (0, 0, "while processing more than one file:");
	error (1, 0, "attempt to specify a numeric revision");
    }
}

/*
 *  Sanity checks and any required fix-up on message passed to RCS via '-m'.
 *  RCS 5.7 requires that a non-total-whitespace, non-null message be provided
 *  with '-m'.  Returns a newly allocated, non-empty buffer with whitespace
 *  stripped from end of lines and end of buffer.
 *
 *  TODO: We no longer use RCS to manage repository files, so maybe this
 *  nonsense about non-empty log fields can be dropped.
 */
char *
make_message_rcslegal (message)
     char *message;
{
    char *dst, *dp, *mp;

    if (message == NULL) message = "";

    /* Strip whitespace from end of lines and end of string. */
    dp = dst = (char *) xmalloc (strlen (message) + 1);
    for (mp = message; *mp != '\0'; ++mp)
    {
	if (*mp == '\n')
	{
	    /* At end-of-line; backtrack to last non-space. */
	    while (dp > dst && (dp[-1] == ' ' || dp[-1] == '\t'))
		--dp;
	}
	*dp++ = *mp;
    }

    /* Backtrack to last non-space at end of string, and truncate. */
    while (dp > dst && isspace ((unsigned char) dp[-1]))
	--dp;
    *dp = '\0';

    /* After all that, if there was no non-space in the string,
       substitute a non-empty message. */
    if (*dst == '\0')
    {
	free (dst);
	dst = xstrdup ("*** empty log message ***");
    }

    return dst;
}

/* Does the file FINFO contain conflict markers?  The whole concept
   of looking at the contents of the file to figure out whether there are
   unresolved conflicts is kind of bogus (people do want to manage files
   which contain those patterns not as conflict markers), but for now it
   is what we do.  */
int
file_has_markers (finfo)
    const struct file_info *finfo;
{
    FILE *fp;
    char *line = NULL;
    size_t line_allocated = 0;
    int result;

    result = 0;
    fp = CVS_FOPEN (finfo->file, "r");
    if (fp == NULL)
	error (1, errno, "cannot open %s", finfo->fullname);
    while (getline (&line, &line_allocated, fp) > 0)
    {
	if (strncmp (line, RCS_MERGE_PAT_1, sizeof RCS_MERGE_PAT_1 - 1) == 0 ||
	    strncmp (line, RCS_MERGE_PAT_2, sizeof RCS_MERGE_PAT_2 - 1) == 0 ||
	    strncmp (line, RCS_MERGE_PAT_3, sizeof RCS_MERGE_PAT_3 - 1) == 0)
	{
	    result = 1;
	    goto out;
	}
    }
    if (ferror (fp))
	error (0, errno, "cannot read %s", finfo->fullname);
out:
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", finfo->fullname);
    if (line != NULL)
	free (line);
    return result;
}

/* Read the entire contents of the file NAME into *BUF.
   If NAME is NULL, read from stdin.  *BUF
   is a pointer returned from malloc (or NULL), pointing to *BUFSIZE
   bytes of space.  The actual size is returned in *LEN.  On error,
   give a fatal error.  The name of the file to use in error messages
   (typically will include a directory if we have changed directory)
   is FULLNAME.  MODE is "r" for text or "rb" for binary.  */

void
get_file (name, fullname, mode, buf, bufsize, len)
    const char *name;
    const char *fullname;
    const char *mode;
    char **buf;
    size_t *bufsize;
    size_t *len;
{
    struct stat s;
    size_t nread;
    char *tobuf;
    FILE *e;
    size_t filesize;

    if (name == NULL)
    {
	e = stdin;
	filesize = 100;	/* force allocation of minimum buffer */
    }
    else
    {
	/* Although it would be cleaner in some ways to just read
	   until end of file, reallocating the buffer, this function
	   does get called on files in the working directory which can
	   be of arbitrary size, so I think we better do all that
	   extra allocation.  */

	if (CVS_STAT (name, &s) < 0)
	    error (1, errno, "can't stat %s", fullname);

	/* Convert from signed to unsigned.  */
	filesize = s.st_size;

	e = open_file (name, mode);
    }

    if (*buf == NULL || *bufsize <= filesize)
    {
	*bufsize = filesize + 1;
	*buf = xrealloc (*buf, *bufsize);
    }

    tobuf = *buf;
    nread = 0;
    while (1)
    {
	size_t got;

	got = fread (tobuf, 1, *bufsize - (tobuf - *buf), e);
	if (ferror (e))
	    error (1, errno, "can't read %s", fullname);
	nread += got;
	tobuf += got;

	if (feof (e))
	    break;

	/* Allocate more space if needed.  */
	if (tobuf == *buf + *bufsize)
	{
	    int c;
	    long off;

	    c = getc (e);
	    if (c == EOF)
		break;
	    off = tobuf - *buf;
	    expand_string (buf, bufsize, *bufsize + 100);
	    tobuf = *buf + off;
	    *tobuf++ = c;
	    ++nread;
	}
    }

    if (e != stdin && fclose (e) < 0)
	error (0, errno, "cannot close %s", fullname);

    *len = nread;

    /* Force *BUF to be large enough to hold a null terminator. */
    if (nread == *bufsize)
	expand_string (buf, bufsize, *bufsize + 1);
    (*buf)[nread] = '\0';
}


/* Follow a chain of symbolic links to its destination.  FILENAME
   should be a handle to a malloc'd block of memory which contains the
   beginning of the chain.  This routine will replace the contents of
   FILENAME with the destination (a real file).  */

void
resolve_symlink (filename)
     char **filename;
{
    if ((! filename) || (! *filename))
	return;

    while (islink (*filename))
    {
	char *newname;
#ifdef HAVE_READLINK
	/* The clean thing to do is probably to have each filesubr.c
	   implement this (with an error if not supported by the
	   platform, in which case islink would presumably return 0).
	   But that would require editing each filesubr.c and so the
	   expedient hack seems to be looking at HAVE_READLINK.  */
	newname = xreadlink (*filename);
	
	if (isabsolute (newname))
	{
	    free (*filename);
	    *filename = newname;
	}
	else
	{
	    char *oldname = last_component (*filename);
	    int dirlen = oldname - *filename;
	    char *fullnewname = xmalloc (dirlen + strlen (newname) + 1);
	    strncpy (fullnewname, *filename, dirlen);
	    strcpy (fullnewname + dirlen, newname);
	    free (newname);
	    free (*filename);
	    *filename = fullnewname;
	}
#else
	error (1, 0, "internal error: islink doesn't like readlink");
#endif
    }
}

/*
 * Rename a file to an appropriate backup name based on BAKPREFIX.
 * If suffix non-null, then ".<suffix>" is appended to the new name.
 *
 * Returns the new name, which caller may free() if desired.
 */
char *
backup_file (filename, suffix)
     const char *filename;
     const char *suffix;
{
    char *backup_name;

    if (suffix == NULL)
    {
        backup_name = xmalloc (sizeof (BAKPREFIX) + strlen (filename) + 1);
        sprintf (backup_name, "%s%s", BAKPREFIX, filename);
    }
    else
    {
        backup_name = xmalloc (sizeof (BAKPREFIX)
                               + strlen (filename)
                               + strlen (suffix)
                               + 2);  /* one for dot, one for trailing '\0' */
        sprintf (backup_name, "%s%s.%s", BAKPREFIX, filename, suffix);
    }

    if (isfile (filename))
        copy_file (filename, backup_name);

    return backup_name;
}

/*
 * Copy a string into a buffer escaping any shell metacharacters.  The
 * buffer should be at least twice as long as the string.
 *
 * Returns a pointer to the terminating NUL byte in buffer.
 */

char *
shell_escape(buf, str)
    char *buf;
    const char *str;
{
    static const char meta[] = "$`\\\"";
    const char *p;

    for (;;)
    {
	p = strpbrk(str, meta);
	if (!p) p = str + strlen(str);
	if (p > str)
	{
	    memcpy(buf, str, p - str);
	    buf += p - str;
	}
	if (!*p) break;
	*buf++ = '\\';
	*buf++ = *p++;
	str = p;
    }
    *buf = '\0';
    return buf;
}

/*
 * We can only travel forwards in time, not backwards.  :)
 */
void
sleep_past (desttime)
    time_t desttime;
{
    time_t t;
    long s;
    long us;

    while (time (&t) <= desttime)
    {
#ifdef HAVE_GETTIMEOFDAY
	struct timeval tv;
	gettimeofday (&tv, NULL);
	if (tv.tv_sec > desttime)
	    break;
	s = desttime - tv.tv_sec;
	if (tv.tv_usec > 0)
	    us = 1000000 - tv.tv_usec;
	else
	{
	    s++;
	    us = 0;
	}
#else
	/* default to 20 ms increments */
	s = desttime - t;
	us = 20000;
#endif

#if defined(HAVE_NANOSLEEP)
	{
	    struct timespec ts;
	    ts.tv_sec = s;
	    ts.tv_nsec = us * 1000;
	    (void)nanosleep (&ts, NULL);
	}
#elif defined(HAVE_USLEEP)
	if (s > 0)
	    (void)sleep (s);
	else
	    (void)usleep (us);
#elif defined(HAVE_SELECT)
	{
	    /* use select instead of sleep since it is a fairly portable way of
	     * sleeping for ms.
	     */
	    struct timeval tv;
	    tv.tv_sec = s;
	    tv.tv_usec = us;
	    (void)select (0, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL, &tv);
	}
#else
	if (us > 0) s++;
	(void)sleep(s);
#endif
    }
}
