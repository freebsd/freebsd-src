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

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)subr.c 1.64 94/10/07 $";
USE(rcsid);
#endif

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
char *
xrealloc (ptr, bytes)
    char *ptr;
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

/*
 * Recover the space allocated by Find_Names() and line2argv()
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
    *pargc = 0;				/* and set it to zero when done */
}

/*
 * Convert a line into argc/argv components and return the result in the
 * arguments as passed.  Use free_names() to return the memory allocated here
 * back to the free pool.
 */
void
line2argv (pargc, argv, line)
    int *pargc;
    char **argv;
    char *line;
{
    char *cp;

    *pargc = 0;
    for (cp = strtok (line, " \t"); cp; cp = strtok ((char *) NULL, " \t"))
    {
	argv[*pargc] = xstrdup (cp);
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
    char gca[PATH_MAX];
    char *p[2];
    int j[2];

    if (rev1 == NULL || rev2 == NULL)
    {
	error (0, 0, "sanity failure in gca");
	abort();
    }

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

    return (xstrdup (gca));
}

/*
 *  Sanity checks and any required fix-up on message passed to RCS via '-m'.
 *  RCS 5.7 requires that a non-total-whitespace, non-null message be provided
 *  with '-m'.
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
