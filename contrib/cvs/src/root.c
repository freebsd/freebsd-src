/*
 * Copyright (c) 1992, Mark D. Baushke
 * Copyright (c) 2002, Derek R. Price
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Name of Root
 * 
 * Determine the path to the CVSROOT and set "Root" accordingly.
 */

#include "cvs.h"
#include "getline.h"

/* Printable names for things in the current_parsed_root->method enum variable.
   Watch out if the enum is changed in cvs.h! */

const char method_names[][16] = {
    "undefined", "local", "server (rsh)", "pserver",
    "kserver", "gserver", "ext", "fork"
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
    int len;

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

    if ((len = getline (&root, &root_allocated, fpin)) < 0)
    {
	int saved_errno = errno;
	/* FIXME: should be checking for end of file separately; errno
	   is not set in that case.  */
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, saved_errno, "cannot read %s", CVSADM_ROOT);
	error (0, 0, "please correct this problem");
	ret = NULL;
	goto out;
    }
    fclose (fpin);
    cp = root + (len - 1);
    if (*cp == '\n')
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
    const char *dir;
    const char *rootdir;
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

static int root_allow_count;
static char **root_allow_vector;
static int root_allow_size;

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
		(char **) xmalloc (root_allow_size * sizeof (char *));
	}
	else
	{
	    root_allow_size *= 2;
	    root_allow_vector =
		(char **) xrealloc (root_allow_vector,
				   root_allow_size * sizeof (char *));
	}

	if (root_allow_vector == NULL)
	{
	no_memory:
	    /* Strictly speaking, we're not supposed to output anything
	       now.  But we're about to exit(), give it a try.  */
	    printf ("E Fatal server error, aborting.\n\
error ENOMEM Virtual memory exhausted.\n");

	    error_exit ();
	}
    }
    p = xmalloc (strlen (arg) + 1);
    if (p == NULL)
	goto no_memory;
    strcpy (p, arg);
    root_allow_vector[root_allow_count++] = p;
}

void
root_allow_free ()
{
    if (root_allow_vector != NULL)
	free_names (&root_allow_count, root_allow_vector);
    root_allow_size = 0;
}

int
root_allow_ok (arg)
    char *arg;
{
    int i;

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



/* This global variable holds the global -d option.  It is NULL if -d
   was not used, which means that we must get the CVSroot information
   from the CVSROOT environment variable or from a CVS/Root file.  */
char *CVSroot_cmdline;



/* FIXME - Deglobalize this. */
cvsroot_t *current_parsed_root = NULL;



/* allocate and initialize a cvsroot_t
 *
 * We must initialize the strings to NULL so we know later what we should
 * free
 *
 * Some of the other zeroes remain meaningful as, "never set, use default",
 * or the like
 */
static cvsroot_t *
new_cvsroot_t ()
{
    cvsroot_t *newroot;

    /* gotta store it somewhere */
    newroot = xmalloc(sizeof(cvsroot_t));

    newroot->original = NULL;
    newroot->method = null_method;
#ifdef CLIENT_SUPPORT
    newroot->username = NULL;
    newroot->password = NULL;
    newroot->hostname = NULL;
    newroot->port = 0;
    newroot->directory = NULL;
    newroot->proxy_hostname = NULL;
    newroot->proxy_port = 0;
    newroot->isremote = 0;
#endif /* CLIENT_SUPPORT */

    return newroot;
}



/* Dispose of a cvsroot_t and its component parts */
void
free_cvsroot_t (root)
    cvsroot_t *root;
{
    if (root->original != NULL)
	free (root->original);
    if (root->directory != NULL)
	free (root->directory);
#ifdef CLIENT_SUPPORT
    if (root->username != NULL)
	free (root->username);
    if (root->password != NULL)
    {
	/* I like to be paranoid */
	memset (root->password, 0, strlen (root->password));
	free (root->password);
    }
    if (root->hostname != NULL)
	free (root->hostname);
    if (root->proxy_hostname != NULL)
	free (root->proxy_hostname);
#endif /* CLIENT_SUPPORT */
    free (root);
}



/*
 * Parse a CVSROOT string to allocate and return a new cvsroot_t structure.
 * Valid specifications are:
 *
 *	:(gserver|kserver|pserver):[[user][:password]@]host[:[port]]/path
 *	[:(ext|server):][[user]@]host[:]/path
 *	[:local:[e:]]/path
 *	:fork:/path
 *
 * INPUTS
 *	root_in		C String containing the CVSROOT to be parsed.
 *
 * RETURNS
 *	A pointer to a newly allocated cvsroot_t structure upon success and
 *	NULL upon failure.  The caller is responsible for disposing of
 *	new structures with a call to free_cvsroot_t().
 *
 * NOTES
 * 	This would have been a lot easier to write in Perl.
 *
 * SEE ALSO
 * 	free_cvsroot_t()
 */
cvsroot_t *
parse_cvsroot (root_in)
    const char *root_in;
{
    cvsroot_t *newroot;			/* the new root to be returned */
    char *cvsroot_save;			/* what we allocated so we can dispose
					 * it when finished */
    char *firstslash;			/* save where the path spec starts
					 * while we parse
					 * [[user][:password]@]host[:[port]]
					 */
    char *cvsroot_copy, *p, *q;		/* temporary pointers for parsing */
#ifdef CLIENT_SUPPORT
    int check_hostname, no_port, no_password;
#endif /* CLIENT_SUPPORT */

    /* allocate some space */
    newroot = new_cvsroot_t();

    /* save the original string */
    newroot->original = xstrdup (root_in);

    /* and another copy we can munge while parsing */
    cvsroot_save = cvsroot_copy = xstrdup (root_in);

    if (*cvsroot_copy == ':')
    {
	char *method = ++cvsroot_copy;

	/* Access method specified, as in
	 * "cvs -d :(gserver|kserver|pserver):[[user][:password]@]host[:[port]]/path",
	 * "cvs -d [:(ext|server):][[user]@]host[:]/path",
	 * "cvs -d :local:e:\path",
	 * "cvs -d :fork:/path".
	 * We need to get past that part of CVSroot before parsing the
	 * rest of it.
	 */

	if (! (p = strchr (method, ':')))
	{
	    error (0, 0, "No closing `:' on method in CVSROOT.");
	    goto error_exit;
	}
	*p = '\0';
	cvsroot_copy = ++p;

#ifdef CLIENT_SUPPORT
	/* Look for method options, for instance, proxy, proxyport.
	 * We don't handle these, but we like to try and warn the user that
	 * they are being ignored.
	 */
	if (p = strchr (method, ';'))	
	{
	    *p++ = '\0';
	    if (!really_quiet)
	    {
		error (0, 0,
"WARNING: Ignoring method options found in CVSROOT: `%s'.",
		       p);
		error (0, 0,
"Use CVS version 1.12.7 or later to handle method options.");
	    }
	}
#endif /* CLIENT_SUPPORT */

	/* Now we have an access method -- see if it's valid. */

	if (strcmp (method, "local") == 0)
	    newroot->method = local_method;
	else if (strcmp (method, "pserver") == 0)
	    newroot->method = pserver_method;
	else if (strcmp (method, "kserver") == 0)
	    newroot->method = kserver_method;
	else if (strcmp (method, "gserver") == 0)
	    newroot->method = gserver_method;
	else if (strcmp (method, "server") == 0)
	    newroot->method = server_method;
	else if (strcmp (method, "ext") == 0)
	    newroot->method = ext_method;
	else if (strcmp (method, "fork") == 0)
	    newroot->method = fork_method;
	else
	{
	    error (0, 0, "Unknown method (`%s') in CVSROOT.", method);
	    goto error_exit;
	}
    }
    else
    {
	/* If the method isn't specified, assume EXT_METHOD if the string looks
	   like a relative path and LOCAL_METHOD otherwise.  */

	newroot->method = ((*cvsroot_copy != '/' && strchr (cvsroot_copy, '/'))
			  ? ext_method
			  : local_method);
    }

#ifdef CLIENT_SUPPORT
    newroot->isremote = (newroot->method != local_method);
#endif /* CLIENT_SUPPORT */


    if ((newroot->method != local_method)
	&& (newroot->method != fork_method))
    {
	/* split the string into [[user][:password]@]host[:[port]] & /path
	 *
	 * this will allow some characters such as '@' & ':' to remain unquoted
	 * in the path portion of the spec
	 */
	if ((p = strchr (cvsroot_copy, '/')) == NULL)
	{
	    error (0, 0, "CVSROOT requires a path spec:");
	    error (0, 0,
":(gserver|kserver|pserver):[[user][:password]@]host[:[port]]/path");
	    error (0, 0, "[:(ext|server):][[user]@]host[:]/path");
	    goto error_exit;
	}
	firstslash = p;		/* == NULL if '/' not in string */
	*p = '\0';

        /* Don't parse username, password, hostname, or port without client
         * support.
         */
#ifdef CLIENT_SUPPORT
	/* Check to see if there is a username[:password] in the string. */
	if ((p = strchr (cvsroot_copy, '@')) != NULL)
	{
	    *p = '\0';
	    /* check for a password */
	    if ((q = strchr (cvsroot_copy, ':')) != NULL)
	    {
		*q = '\0';
		newroot->password = xstrdup (++q);
		/* Don't check for *newroot->password == '\0' since
		 * a user could conceivably wish to specify a blank password
		 *
		 * (newroot->password == NULL means to use the
		 * password from .cvspass)
		 */
	    }

	    /* copy the username */
	    if (*cvsroot_copy != '\0')
		/* a blank username is impossible, so leave it NULL in that
		 * case so we know to use the default username
		 */
		newroot->username = xstrdup (cvsroot_copy);

	    cvsroot_copy = ++p;
	}

	/* now deal with host[:[port]] */

	/* the port */
	if ((p = strchr (cvsroot_copy, ':')) != NULL)
	{
	    *p++ = '\0';
	    if (strlen(p))
	    {
		q = p;
		if (*q == '-') q++;
		while (*q)
		{
		    if (!isdigit(*q++))
		    {
			error (0, 0,
"CVSROOT may only specify a positive, non-zero, integer port (not `%s').",
				p);
			error (0, 0,
                               "Perhaps you entered a relative pathname?");
			goto error_exit;
		    }
		}
		if ((newroot->port = atoi (p)) <= 0)
		{
		    error (0, 0,
"CVSROOT may only specify a positive, non-zero, integer port (not `%s').",
			    p);
		    error (0, 0, "Perhaps you entered a relative pathname?");
		    goto error_exit;
		}
	    }
	}

	/* copy host */
	if (*cvsroot_copy != '\0')
	    /* blank hostnames are invalid, but for now leave the field NULL
	     * and catch the error during the sanity checks later
	     */
	    newroot->hostname = xstrdup (cvsroot_copy);

	/* restore the '/' */
	cvsroot_copy = firstslash;
	*cvsroot_copy = '/';
#endif /* CLIENT_SUPPORT */
    }

    /*
     * Parse the path for all methods.
     */
    /* Here & local_cvsroot() should be the only places this needs to be
     * called on a CVSROOT now.  cvsroot->original is saved for error messages
     * and, otherwise, we want no trailing slashes.
     */
    Sanitize_Repository_Name( cvsroot_copy );
    newroot->directory = xstrdup(cvsroot_copy);

    /*
     * Do various sanity checks.
     */

#if ! defined (CLIENT_SUPPORT) && ! defined (DEBUG)
    if (newroot->method != local_method)
    {
	error (0, 0, "CVSROOT is set for a remote access method but your");
	error (0, 0, "CVS executable doesn't support it.");
	goto error_exit;
    }
#endif

#if ! defined (SERVER_SUPPORT) && ! defined (DEBUG)
    if (newroot->method == fork_method)
    {
	error (0, 0, "CVSROOT is set to use the :fork: access method but your");
	error (0, 0, "CVS executable doesn't support it.");
	goto error_exit;
     }
#endif

#ifdef CLIENT_SUPPORT
    if (newroot->username && ! newroot->hostname)
    {
	error (0, 0, "Missing hostname in CVSROOT.");
	goto error_exit;
    }

    check_hostname = 0;
    no_password = 1;
    no_port = 0;
#endif /* CLIENT_SUPPORT */
    switch (newroot->method)
    {
    case local_method:
#ifdef CLIENT_SUPPORT
	if (newroot->username || newroot->hostname)
	{
	    error (0, 0, "Can't specify hostname and username in CVSROOT");
	    error (0, 0, "when using local access method.");
	    goto error_exit;
	}
	no_port = 1;
	/* no_password already set */
#endif /* CLIENT_SUPPORT */
	/* cvs.texinfo has always told people that CVSROOT must be an
	   absolute pathname.  Furthermore, attempts to use a relative
	   pathname produced various errors (I couldn't get it to work),
	   so there would seem to be little risk in making this a fatal
	   error.  */
	if (!isabsolute (newroot->directory))
	{
	    error (0, 0, "CVSROOT must be an absolute pathname (not `%s')",
		   newroot->directory);
	    error (0, 0, "when using local access method.");
	    goto error_exit;
	}
	break;
#ifdef CLIENT_SUPPORT
    case fork_method:
	/* We want :fork: to behave the same as other remote access
           methods.  Therefore, don't check to see that the repository
           name is absolute -- let the server do it.  */
	if (newroot->username || newroot->hostname)
	{
	    error (0, 0, "Can't specify hostname and username in CVSROOT");
	    error (0, 0, "when using fork access method.");
	    goto error_exit;
	}
	newroot->hostname = xstrdup("server");  /* for error messages */
	if (!isabsolute (newroot->directory))
	{
	    error (0, 0, "CVSROOT must be an absolute pathname (not `%s')",
		   newroot->directory);
	    error (0, 0, "when using fork access method.");
	    goto error_exit;
	}
	no_port = 1;
	/* no_password already set */
	break;
    case kserver_method:
# ifndef HAVE_KERBEROS
       	error (0, 0, "CVSROOT is set for a kerberos access method but your");
	error (0, 0, "CVS executable doesn't support it.");
	goto error_exit;
# else
	check_hostname = 1;
	/* no_password already set */
	break;
# endif
    case gserver_method:
# ifndef HAVE_GSSAPI
	error (0, 0, "CVSROOT is set for a GSSAPI access method but your");
	error (0, 0, "CVS executable doesn't support it.");
	goto error_exit;
# else
	check_hostname = 1;
	/* no_password already set */
	break;
# endif
    case server_method:
    case ext_method:
	no_port = 1;
	/* no_password already set */
	check_hostname = 1;
	break;
    case pserver_method:
	no_password = 0;
	check_hostname = 1;
	break;
#endif /* CLIENT_SUPPORT */
    default:
	error (1, 0, "Invalid method found in parse_cvsroot");
    }

#ifdef CLIENT_SUPPORT
    if (no_password && newroot->password)
    {
	error (0, 0, "CVSROOT password specification is only valid for");
	error (0, 0, "pserver connection method.");
	goto error_exit;
    }

    if (check_hostname && !newroot->hostname)
    {
	error (0, 0, "Didn't specify hostname in CVSROOT.");
	goto error_exit;
    }

    if (no_port && newroot->port)
	{
	    error (0, 0, "CVSROOT port specification is only valid for gserver, kserver,");
	    error (0, 0, "and pserver connection methods.");
	    goto error_exit;
	}
#endif /* CLIENT_SUPPORT */

    if (*newroot->directory == '\0')
    {
	error (0, 0, "Missing directory in CVSROOT.");
	goto error_exit;
    }
    
    /* Hooray!  We finally parsed it! */
    free (cvsroot_save);
    return newroot;

error_exit:
    free (cvsroot_save);
    free_cvsroot_t (newroot);
    return NULL;
}



#ifdef AUTH_CLIENT_SUPPORT
/* Use root->username, root->hostname, root->port, and root->directory
 * to create a normalized CVSROOT fit for the .cvspass file
 *
 * username defaults to the result of getcaller()
 * port defaults to the result of get_cvs_port_number()
 *
 * FIXME - we could cache the canonicalized version of a root inside the
 * cvsroot_t, but we'd have to un'const the input here and stop expecting the
 * caller to be responsible for our return value
 */
char *
normalize_cvsroot (root)
    const cvsroot_t *root;
{
    char *cvsroot_canonical;
    char *p, *hostname, *username;
    char port_s[64];

    /* get the appropriate port string */
    sprintf (port_s, "%d", get_cvs_port_number (root));

    /* use a lower case hostname since we know hostnames are case insensitive */
    /* Some logic says we should be tacking our domain name on too if it isn't
     * there already, but for now this works.  Reverse->Forward lookups are
     * almost certainly too much since that would make CVS immune to some of
     * the DNS trickery that makes life easier for sysadmins when they want to
     * move a repository or the like
     */
    p = hostname = xstrdup(root->hostname);
    while (*p)
    {
	*p = tolower(*p);
	p++;
    }

    /* get the username string */
    username = root->username ? root->username : getcaller();
    cvsroot_canonical = xmalloc ( strlen(username)
				+ strlen(hostname) + strlen(port_s)
				+ strlen(root->directory) + 12);
    sprintf (cvsroot_canonical, ":pserver:%s@%s:%s%s",
	    username, hostname, port_s, root->directory);

    free (hostname);
    return cvsroot_canonical;
}
#endif /* AUTH_CLIENT_SUPPORT */



/* allocate and return a cvsroot_t structure set up as if we're using the local
 * repository DIR.  */
cvsroot_t *
local_cvsroot (dir)
    const char *dir;
{
    cvsroot_t *newroot = new_cvsroot_t();

    newroot->original = xstrdup(dir);
    newroot->method = local_method;
    newroot->directory = xstrdup(dir);
    /* Here and parse_cvsroot() should be the only places this needs to be
     * called on a CVSROOT now.  cvsroot->original is saved for error messages
     * and, otherwise, we want no trailing slashes.
     */
    Sanitize_Repository_Name( newroot->directory );
    return newroot;
}



#ifdef DEBUG
/* This is for testing the parsing function.  Use

     gcc -I. -I.. -I../lib -DDEBUG root.c -o root

   to compile.  */

#include <stdio.h>

char *program_name = "testing";
char *cvs_cmd_name = "parse_cvsroot";		/* XXX is this used??? */

/* Toy versions of various functions when debugging under unix.  Yes,
   these make various bad assumptions, but they're pretty easy to
   debug when something goes wrong.  */

void
error_exit PROTO ((void))
{
    exit (1);
}

int
isabsolute (dir)
    const char *dir;
{
    return (dir && (*dir == '/'));
}

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
  
    if ((current_parsed_root = parse_cvsroot (argv[1])) == NULL)
    {
	fprintf (stderr, "%s: Parsing failed.\n", program_name);
	exit (1);
    }
    printf ("CVSroot: %s\n", argv[1]);
    printf ("current_parsed_root->method: %s\n", method_names[current_parsed_root->method]);
    printf ("current_parsed_root->username: %s\n",
	    current_parsed_root->username ? current_parsed_root->username : "NULL");
    printf ("current_parsed_root->hostname: %s\n",
	    current_parsed_root->hostname ? current_parsed_root->hostname : "NULL");
    printf ("current_parsed_root->directory: %s\n", current_parsed_root->directory);

   exit (0);
   /* NOTREACHED */
}
#endif
