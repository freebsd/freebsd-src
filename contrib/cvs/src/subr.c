/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Various useful functions for the CVS support code.
 */

#include "cvs.h"
#include "getline.h"

extern char *getlogin ();

/*
 * malloc some data and die if it fails
 */
char *
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
	error (1, 0, "can not allocate %lu bytes", (unsigned long) bytes);
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
	error (1, 0, "can not reallocate %lu bytes", (unsigned long) bytes);
    return (cp);
}

/* Two constants which tune expand_string.  Having MIN_INCR as large
   as 1024 might waste a bit of memory, but it shouldn't be too bad
   (CVS used to allocate arrays of, say, 3000, PATH_MAX (8192, often),
   or other such sizes).  Probably anything which is going to allocate
   memory which is likely to get as big as MAX_INCR shouldn't be doing
   it in one block which must be contiguous, but since getrcskey does
   so, we might as well limit the wasted memory to MAX_INCR or so
   bytes.  */

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
		*n += MIN_INCR;
	    else if (*n > MAX_INCR)
		*n += MAX_INCR;
	    else
		*n *= 2;
	}
	*strptr = xrealloc (*strptr, *n);
    }
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
	else if (p[0] == '.' && (p[1] == '\0' || p[1] == '/'))
	    ;
	else
	    ++level;
	p = q;
    } while (p != NULL);
    return max_level;
}


/*
 * Recover the space allocated by line2argv()
 */
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

/* Convert LINE into arguments separated by space and tab.  Set *ARGC
   to the number of arguments found, and (*ARGV)[0] to the first argument,
   (*ARGV)[1] to the second, etc.  *ARGV is malloc'd and so are each of
   (*ARGV)[0], (*ARGV)[1], ...  Use free_names() to return the memory
   allocated here back to the free pool.  */
void
line2argv (pargc, argv, line)
    int *pargc;
    char ***argv;
    char *line;
{
    char *cp;
    /* Could make a case for size_t or some other unsigned type, but
       we'll stick with int to avoid signed/unsigned warnings when
       comparing with *pargc.  */
    int argv_allocated;

    /* Small for testing.  */
    /* argv_allocated must be at least 3 because at some places
       (e.g. checkout_proc) cvs alters argv[2].  */
    argv_allocated = 4;
    *argv = (char **) xmalloc (argv_allocated * sizeof (**argv));

    *pargc = 0;
    for (cp = strtok (line, " \t"); cp; cp = strtok ((char *) NULL, " \t"))
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

/*
 * Get the caller's login from his uid. If the real uid is "root" try LOGNAME
 * USER or getlogin(). If getlogin() and getpwuid() both fail, return
 * the uid as a string.
 */
char *
getcaller ()
{
    static char uidname[20];
    struct passwd *pw;
    char *name;
    uid_t uid;

    uid = getuid ();
    if (uid == (uid_t) 0)
    {
	/* super-user; try getlogin() to distinguish */
	if (((name = getlogin ()) || (name = getenv("LOGNAME")) ||
	     (name = getenv("USER"))) && *name)
	    return (name);
    }
    if ((pw = (struct passwd *) getpwuid (uid)) == NULL)
    {
	(void) sprintf (uidname, "uid%lu", (unsigned long) uid);
	return (uidname);
    }
    return (pw->pw_name);
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
    char *rev1;
    char *rev2;
{
    int dots;
    char *gca;
    char *p[2];
    int j[2];
    char *retval;

    if (rev1 == NULL || rev2 == NULL)
    {
	error (0, 0, "sanity failure in gca");
	abort();
    }

    /* The greatest common ancestor will have no more dots, and numbers
       of digits for each component no greater than the arguments.  Therefore
       this string will be big enough.  */
    gca = xmalloc (strlen (rev1) + strlen (rev2) + 100);

    /* walk the strings, reading the common parts. */
    gca[0] = '\0';
    p[0] = rev1;
    p[1] = rev2;
    do
    {
	int i;
	char c[2];
	char *s[2];
	
	for (i = 0; i < 2; ++i)
	{
	    /* swap out the dot */
	    s[i] = strchr (p[i], '.');
	    if (s[i] != NULL) {
		c[i] = *s[i];
	    }
	    
	    /* read an int */
	    j[i] = atoi (p[i]);
	    
	    /* swap back the dot... */
	    if (s[i] != NULL) {
		*s[i] = c[i];
		p[i] = s[i] + 1;
	    }
	    else
	    {
		/* or mark us at the end */
		p[i] = NULL;
	    }
	    
	}
	
	/* use the lowest. */
	(void) sprintf (gca + strlen (gca), "%d.",
			j[0] < j[1] ? j[0] : j[1]);

    } while (j[0] == j[1]
	     && p[0] != NULL
	     && p[1] != NULL);

    /* back up over that last dot. */
    gca[strlen(gca) - 1] = '\0';

    /* numbers differ, or we ran out of strings.  we're done with the
       common parts.  */

    dots = numdots (gca);
    if (dots == 0)
    {
	/* revisions differ in trunk major number.  */

	char *q;
	char *s;

	s = (j[0] < j[1]) ? p[0] : p[1];

	if (s == NULL)
	{
	    /* we only got one number.  this is strange.  */
	    error (0, 0, "bad revisions %s or %s", rev1, rev2);
	    abort();
	}
	else
	{
	    /* we have a minor number.  use it.  */
	    q = gca + strlen (gca);
	    
	    *q++ = '.';
	    for ( ; *s != '.' && *s != '\0'; )
		*q++ = *s++;
	    
	    *q = '\0';
	}
    }
    else if ((dots & 1) == 0)
    {
	/* if we have an even number of dots, then we have a branch.
	   remove the last number in order to make it a revision.  */
	
	char *s;

	s = strrchr(gca, '.');
	*s = '\0';
    }

    retval = xstrdup (gca);
    free (gca);
    return retval;
}

/*
 *  Sanity checks and any required fix-up on message passed to RCS via '-m'.
 *  RCS 5.7 requires that a non-total-whitespace, non-null message be provided
 *  with '-m'.  Returns the original argument or a pointer to readonly
 *  static storage.
 */
char *
make_message_rcslegal (message)
     char *message;
{
    if ((message == NULL) || (*message == '\0') || isspace (*message))
    {
        char *t;

	if (message)
	    for (t = message; *t; t++)
	        if (!isspace (*t))
		    return message;

	return "*** empty log message ***\n";
    }

    return message;
}

/* Does the file FINFO contain conflict markers?  The whole concept
   of looking at the contents of the file to figure out whether there are
   unresolved conflicts is kind of bogus (people do want to manage files
   which contain those patterns not as conflict markers), but for now it
   is what we do.  */
int
file_has_markers (finfo)
    struct file_info *finfo;
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
	if (strncmp (line, RCS_MERGE_PAT, sizeof RCS_MERGE_PAT - 1) == 0)
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
