/*
 * Copyright (c) 1992, Mark D. Baushke
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Name of Root
 * 
 * Determine the path to the CVSROOT and set "Root" accordingly.
 * If this looks like of modified clone of Name_Repository() in
 * repos.c, it is... 
 */

#include "cvs.h"
#include "getline.h"

/* Printable names for things in the CVSroot_method enum variable.
   Watch out if the enum is changed in cvs.h! */

char *method_names[] = {
    "local", "server (rsh)", "pserver", "kserver", "gserver", "ext"
};

#ifndef DEBUG

char *
Name_Root (dir, update_dir)
    char *dir;
    char *update_dir;
{
    FILE *fpin;
    char *ret, *xupdate_dir;
    char *root = NULL;
    size_t root_allocated = 0;
    char *tmp;
    char *cvsadm;
    char *cp;

    if (update_dir && *update_dir)
	xupdate_dir = update_dir;
    else
	xupdate_dir = ".";

    if (dir != NULL)
    {
	cvsadm = xmalloc (strlen (dir) + sizeof (CVSADM) + 10);
	(void) sprintf (cvsadm, "%s/%s", dir, CVSADM);
	tmp = xmalloc (strlen (dir) + sizeof (CVSADM_ROOT) + 10);
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_ROOT);
    }
    else
    {
	cvsadm = xstrdup (CVSADM);
	tmp = xstrdup (CVSADM_ROOT);
    }

    /*
     * Do not bother looking for a readable file if there is no cvsadm
     * directory present.
     *
     * It is possible that not all repositories will have a CVS/Root
     * file. This is ok, but the user will need to specify -d
     * /path/name or have the environment variable CVSROOT set in
     * order to continue.  */
    if ((!isdir (cvsadm)) || (!isreadable (tmp)))
    {
	ret = NULL;
	goto out;
    }

    /*
     * The assumption here is that the CVS Root is always contained in the
     * first line of the "Root" file.
     */
    fpin = open_file (tmp, "r");

    if (getline (&root, &root_allocated, fpin) < 0)
    {
	/* FIXME: should be checking for end of file separately; errno
	   is not set in that case.  */
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, errno, "cannot read %s", CVSADM_ROOT);
	error (0, 0, "please correct this problem");
	ret = NULL;
	goto out;
    }
    (void) fclose (fpin);
    if ((cp = strrchr (root, '\n')) != NULL)
	*cp = '\0';			/* strip the newline */

    /*
     * root now contains a candidate for CVSroot. It must be an
     * absolute pathname or specify a remote server.
     */

    if (
#ifdef CLIENT_SUPPORT
	(strchr (root, ':') == NULL) &&
#endif
    	! isabsolute (root))
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, 0,
	       "ignoring %s because it does not contain an absolute pathname.",
	       CVSADM_ROOT);
	ret = NULL;
	goto out;
    }

#ifdef CLIENT_SUPPORT
    if ((strchr (root, ':') == NULL) && !isdir (root))
#else /* ! CLIENT_SUPPORT */
    if (!isdir (root))
#endif /* CLIENT_SUPPORT */
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, 0,
	       "ignoring %s because it specifies a non-existent repository %s",
	       CVSADM_ROOT, root);
	ret = NULL;
	goto out;
    }

    /* allocate space to return and fill it in */
    strip_trailing_slashes (root);
    ret = xstrdup (root);
 out:
    free (cvsadm);
    free (tmp);
    if (root != NULL)
	free (root);
    return (ret);
}

/*
 * Write the CVS/Root file so that the environment variable CVSROOT
 * and/or the -d option to cvs will be validated or not necessary for
 * future work.
 */
void
Create_Root (dir, rootdir)
    char *dir;
    char *rootdir;
{
    FILE *fout;
    char *tmp;

    if (noexec)
	return;

    /* record the current cvs root */

    if (rootdir != NULL)
    {
        if (dir != NULL)
	{
	    tmp = xmalloc (strlen (dir) + sizeof (CVSADM_ROOT) + 10);
	    (void) sprintf (tmp, "%s/%s", dir, CVSADM_ROOT);
	}
        else
	    tmp = xstrdup (CVSADM_ROOT);

        fout = open_file (tmp, "w+");
        if (fprintf (fout, "%s\n", rootdir) < 0)
	    error (1, errno, "write to %s failed", tmp);
        if (fclose (fout) == EOF)
	    error (1, errno, "cannot close %s", tmp);
	free (tmp);
    }
}

#endif /* ! DEBUG */


/* The root_allow_* stuff maintains a list of legal CVSROOT
   directories.  Then we can check against them when a remote user
   hands us a CVSROOT directory.  */

static unsigned int root_allow_count;
static char **root_allow_vector;
static unsigned int root_allow_size;

void
root_allow_add (arg)
    char *arg;
{
    char *p;

    if (root_allow_size <= root_allow_count)
    {
	if (root_allow_size == 0)
	{
	    root_allow_size = 1;
	    root_allow_vector =
		(char **) malloc (root_allow_size * sizeof (char *));
	}
	else
	{
	    root_allow_size *= 2;
	    root_allow_vector =
		(char **) realloc (root_allow_vector,
				   root_allow_size * sizeof (char *));
	}

	if (root_allow_vector == NULL)
	{
	no_memory:
	    /* Strictly speaking, we're not supposed to output anything
	       now.  But we're about to exit(), give it a try.  */
	    printf ("E Fatal server error, aborting.\n\
error ENOMEM Virtual memory exhausted.\n");

	    /* I'm doing this manually rather than via error_exit ()
	       because I'm not sure whether we want to call server_cleanup.
	       Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	    /* Hook for OS-specific behavior, for example socket
	       subsystems on NT and OS2 or dealing with windows
	       and arguments on Mac.  */
	    SYSTEM_CLEANUP ();
#endif

	    exit (EXIT_FAILURE);
	}
    }
    p = malloc (strlen (arg) + 1);
    if (p == NULL)
	goto no_memory;
    strcpy (p, arg);
    root_allow_vector[root_allow_count++] = p;
}

void
root_allow_free ()
{
    if (root_allow_vector != NULL)
	free (root_allow_vector);
    root_allow_count = 0;
    root_allow_size = 0;
}

int
root_allow_ok (arg)
    char *arg;
{
    unsigned int i;

    if (root_allow_count == 0)
    {
	/* Probably someone upgraded from CVS before 1.9.10 to 1.9.10
	   or later without reading the documentation about
	   --allow-root.  Printing an error here doesn't disclose any
	   particularly useful information to an attacker because a
	   CVS server configured in this way won't let *anyone* in.  */

	/* Note that we are called from a context where we can spit
	   back "error" rather than waiting for the next request which
	   expects responses.  */
	printf ("\
error 0 Server configuration missing --allow-root in inetd.conf\n");
	error_exit ();
    }

    for (i = 0; i < root_allow_count; ++i)
	if (strcmp (root_allow_vector[i], arg) == 0)
	    return 1;
    return 0;
}


/* Parse a CVSROOT variable into its constituent parts -- method,
 * username, hostname, directory.  The prototypical CVSROOT variable
 * looks like:
 *
 * :method:user@host:path
 *
 * Some methods may omit fields; local, for example, doesn't need user
 * and host.
 *
 * Returns zero on success, non-zero on failure. */

char *CVSroot_original = NULL;	/* the CVSroot that was passed in */
int client_active;		/* nonzero if we are doing remote access */
CVSmethod CVSroot_method;	/* one of the enum values defined in cvs.h */
char *CVSroot_username;		/* the username or NULL if method == local */
char *CVSroot_hostname;		/* the hostname or NULL if method == local */
char *CVSroot_directory;	/* the directory name */

#ifdef AUTH_SERVER_SUPPORT
/* Die if CVSroot_directory and Pserver_Repos don't match. */
static void
check_root_consistent ()
{
    /* FIXME: Should be using a deferred error, as the rest of
       serve_root does.  As it is now the call to error could conceivably
       cause deadlock, as noted in server_cleanup.  Best solution would
       presumably be to write some code so that error() automatically
       defers the error in those cases where that is needed.  */
    /* FIXME?  Possible that the wording should be more clear (e.g.
          Root says "%s" but pserver protocol says "%s"
       or something which would aid people who are writing implementations
       of the client side of the CVS protocol.  I don't see any security
       problem with revealing that information.  */
    if ((Pserver_Repos != NULL) && (CVSroot_directory != NULL))
	if (strcmp (Pserver_Repos, CVSroot_directory) != 0)
	    error (1, 0, "repository mismatch: \"%s\" vs \"%s\"",
		   Pserver_Repos, CVSroot_directory);
}

#endif /* AUTH_SERVER_SUPPORT */


int
parse_cvsroot (CVSroot)
    char *CVSroot;
{
    static int cvsroot_parsed = 0;
    char *cvsroot_copy, *p;
    int check_hostname;

    /* Don't go through the trouble twice. */
    if (cvsroot_parsed)
    {
	error (0, 0, "WARNING (parse_cvsroot): someone called me twice!\n");
	return 0;
    }

    CVSroot_original = xstrdup (CVSroot);
    cvsroot_copy = xstrdup (CVSroot);

    if ((*cvsroot_copy == ':'))
    {
	char *method = ++cvsroot_copy;

	/* Access method specified, as in
	 * "cvs -d :pserver:user@host:/path",
	 * "cvs -d :local:e:\path", or
	 * "cvs -d :kserver:user@host:/path".
	 * We need to get past that part of CVSroot before parsing the
	 * rest of it.
	 */

	if (! (p = strchr (method, ':')))
	{
	    error (0, 0, "bad CVSroot: %s", CVSroot);
	    return 1;
	}
	*p = '\0';
	cvsroot_copy = ++p;

	/* Now we have an access method -- see if it's valid. */

	if (strcmp (method, "local") == 0)
	    CVSroot_method = local_method;
	else if (strcmp (method, "pserver") == 0)
	    CVSroot_method = pserver_method;
	else if (strcmp (method, "kserver") == 0)
	    CVSroot_method = kserver_method;
	else if (strcmp (method, "gserver") == 0)
	    CVSroot_method = gserver_method;
	else if (strcmp (method, "server") == 0)
	    CVSroot_method = server_method;
	else if (strcmp (method, "ext") == 0)
	    CVSroot_method = ext_method;
	else
	{
	    error (0, 0, "unknown method in CVSroot: %s", CVSroot);
	    return 1;
	}
    }
    else
    {
	/* If the method isn't specified, assume
	   SERVER_METHOD/EXT_METHOD if the string contains a colon or
	   LOCAL_METHOD otherwise.  */

	CVSroot_method = ((strchr (cvsroot_copy, ':'))
#ifdef RSH_NOT_TRANSPARENT
			  ? server_method
#else
			  ? ext_method
#endif
			  : local_method);
    }

    client_active = (CVSroot_method != local_method);

    /* Check for username/hostname if we're not LOCAL_METHOD. */

    CVSroot_username = NULL;
    CVSroot_hostname = NULL;

    if (CVSroot_method != local_method)
    {
	/* Check to see if there is a username in the string. */

	if ((p = strchr (cvsroot_copy, '@')))
	{
	    CVSroot_username = cvsroot_copy;
	    *p = '\0';
	    cvsroot_copy = ++p;
	    if (*CVSroot_username == '\0')
		CVSroot_username = NULL;
	}

	if ((p = strchr (cvsroot_copy, ':')))
	{
	    CVSroot_hostname = cvsroot_copy;
	    *p = '\0';
	    cvsroot_copy = ++p;
      
	    if (*CVSroot_hostname == '\0')
		CVSroot_hostname = NULL;
	}
    }

    CVSroot_directory = cvsroot_copy;
#ifdef AUTH_SERVER_SUPPORT
    check_root_consistent ();
#endif /* AUTH_SERVER_SUPPORT */

#if ! defined (CLIENT_SUPPORT) && ! defined (DEBUG)
    if (CVSroot_method != local_method)
    {
	error (0, 0, "Your CVSROOT is set for a remote access method");
	error (0, 0, "but your CVS executable doesn't support it");
	error (0, 0, "(%s)", CVSroot);
	return 1;
    }
#endif
  
    /* Do various sanity checks. */

    if (CVSroot_username && ! CVSroot_hostname)
    {
	error (0, 0, "missing hostname in CVSROOT: %s", CVSroot);
	return 1;
    }

    check_hostname = 0;
    switch (CVSroot_method)
    {
    case local_method:
	if (CVSroot_username || CVSroot_hostname)
	{
	    error (0, 0, "can't specify hostname and username in CVSROOT");
	    error (0, 0, "when using local access method");
	    error (0, 0, "(%s)", CVSroot);
	    return 1;
	}
	/* cvs.texinfo has always told people that CVSROOT must be an
	   absolute pathname.  Furthermore, attempts to use a relative
	   pathname produced various errors (I couldn't get it to work),
	   so there would seem to be little risk in making this a fatal
	   error.  */
	if (!isabsolute (CVSroot_directory))
	    error (1, 0, "CVSROOT %s must be an absolute pathname",
		   CVSroot_directory);
	break;
    case kserver_method:
#ifndef HAVE_KERBEROS
	error (0, 0, "Your CVSROOT is set for a kerberos access method");
	error (0, 0, "but your CVS executable doesn't support it");
	error (0, 0, "(%s)", CVSroot);
	return 1;
#endif
	check_hostname = 1;
	break;
    case gserver_method:
#ifndef HAVE_GSSAPI
	error (0, 0, "Your CVSROOT is set for a GSSAPI access method");
	error (0, 0, "but your CVS executable doesn't support it");
	error (0, 0, "(%s)", CVSroot);
	return 1;
#endif
	check_hostname = 1;
	break;
    case server_method:
    case ext_method:
    case pserver_method:
	check_hostname = 1;
	break;
    }

    if (check_hostname)
    {
	if (! CVSroot_hostname)
	{
	    error (0, 0, "didn't specify hostname in CVSROOT: %s", CVSroot);
	    return 1;
	}
    }

    if (*CVSroot_directory == '\0')
    {
	error (0, 0, "missing directory in CVSROOT: %s", CVSroot);
	return 1;
    }
    
    /* Hooray!  We finally parsed it! */
    return 0;
}


/* Set up the global CVSroot* variables as if we're using the local
   repository DIR. */

void
set_local_cvsroot (dir)
    char *dir;
{
    CVSroot_original = xstrdup (dir);
    CVSroot_method = local_method;
    CVSroot_directory = CVSroot_original;
#ifdef AUTH_SERVER_SUPPORT
    check_root_consistent ();
#endif /* AUTH_SERVER_SUPPORT */
    CVSroot_username = NULL;
    CVSroot_hostname = NULL;
    client_active = 0;
}


#ifdef DEBUG
/* This is for testing the parsing function. */

#include <stdio.h>

char *CVSroot;
char *program_name = "testing";
char *command_name = "parse_cvsroot";		/* XXX is this used??? */

void
main (argc, argv)
    int argc;
    char *argv[];
{
    program_name = argv[0];

    if (argc != 2)
    {
	fprintf (stderr, "Usage: %s <CVSROOT>\n", program_name);
	exit (2);
    }
  
    if (parse_cvsroot (argv[1]))
    {
	fprintf (stderr, "%s: Parsing failed.", program_name);
	exit (1);
    }
    printf ("CVSroot: %s\n", argv[1]);
    printf ("CVSroot_method: %s\n", method_names[CVSroot_method]);
    printf ("CVSroot_username: %s\n",
	    CVSroot_username ? CVSroot_username : "NULL");
    printf ("CVSroot_hostname: %s\n",
	    CVSroot_hostname ? CVSroot_hostname : "NULL");
    printf ("CVSroot_directory: %s\n", CVSroot_directory);

   exit (0);
   /* NOTREACHED */
}
#endif
