/* CVS client-related stuff.  */

#include "cvs.h"

#ifdef CLIENT_SUPPORT

#include "update.h"		/* Things shared with update.c */
#include "md5.h"

#ifdef AUTH_CLIENT_SUPPORT
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
char *get_cvs_password PROTO((char *user, char *host, char *cvsrooot));
#endif /* AUTH_CLIENT_SUPPORT */

#if HAVE_KERBEROS
#define CVS_PORT 1999

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <krb.h>

extern char *krb_realmofhost ();
#ifndef HAVE_KRB_GET_ERR_TEXT
#define krb_get_err_text(status) krb_err_txt[status]
#endif /* HAVE_KRB_GET_ERR_TEXT */
#endif /* HAVE_KERBEROS */

static void add_prune_candidate PROTO((char *));

/* All the commands.  */
int add PROTO((int argc, char **argv));
int admin PROTO((int argc, char **argv));
int checkout PROTO((int argc, char **argv));
int commit PROTO((int argc, char **argv));
int diff PROTO((int argc, char **argv));
int history PROTO((int argc, char **argv));
int import PROTO((int argc, char **argv));
int cvslog PROTO((int argc, char **argv));
int patch PROTO((int argc, char **argv));
int release PROTO((int argc, char **argv));
int cvsremove PROTO((int argc, char **argv));
int rtag PROTO((int argc, char **argv));
int status PROTO((int argc, char **argv));
int tag PROTO((int argc, char **argv));
int update PROTO((int argc, char **argv));

/* All the response handling functions.  */
static void handle_ok PROTO((char *, int));
static void handle_error PROTO((char *, int));
static void handle_valid_requests PROTO((char *, int));
static void handle_checked_in PROTO((char *, int));
static void handle_new_entry PROTO((char *, int));
static void handle_checksum PROTO((char *, int));
static void handle_copy_file PROTO((char *, int));
static void handle_updated PROTO((char *, int));
static void handle_merged PROTO((char *, int));
static void handle_patched PROTO((char *, int));
static void handle_removed PROTO((char *, int));
static void handle_remove_entry PROTO((char *, int));
static void handle_set_static_directory PROTO((char *, int));
static void handle_clear_static_directory PROTO((char *, int));
static void handle_set_sticky PROTO((char *, int));
static void handle_clear_sticky PROTO((char *, int));
static void handle_set_checkin_prog PROTO((char *, int));
static void handle_set_update_prog PROTO((char *, int));
static void handle_module_expansion PROTO((char *, int));
static void handle_m PROTO((char *, int));
static void handle_e PROTO((char *, int));

#endif /* CLIENT_SUPPORT */

#if defined(CLIENT_SUPPORT) || defined(SERVER_SUPPORT)

/* Shared with server.  */

/*
 * Return a malloc'd, '\0'-terminated string
 * corresponding to the mode in SB.
 */
char *
#ifdef __STDC__
mode_to_string (mode_t mode)
#else /* ! __STDC__ */
mode_to_string (mode)
	mode_t mode;
#endif /* __STDC__ */
{
	char buf[18], u[4], g[4], o[4];
	int i;

	i = 0;
	if (mode & S_IRUSR) u[i++] = 'r';
	if (mode & S_IWUSR) u[i++] = 'w';
	if (mode & S_IXUSR) u[i++] = 'x';
	u[i] = '\0';
	
	i = 0;
	if (mode & S_IRGRP) g[i++] = 'r';
	if (mode & S_IWGRP) g[i++] = 'w';
	if (mode & S_IXGRP) g[i++] = 'x';
	g[i] = '\0';
	
	i = 0;
	if (mode & S_IROTH) o[i++] = 'r';
	if (mode & S_IWOTH) o[i++] = 'w';
	if (mode & S_IXOTH) o[i++] = 'x';
	o[i] = '\0';

	sprintf(buf, "u=%s,g=%s,o=%s", u, g, o);
	return xstrdup(buf);
}

/*
 * Change mode of FILENAME to MODE_STRING.
 * Returns 0 for success or errno code.
 */
int
change_mode (filename, mode_string)
    char *filename;
    char *mode_string;
{
    char *p;
    mode_t mode = 0;

    p = mode_string;
    while (*p != '\0')
    {
	if ((p[0] == 'u' || p[0] == 'g' || p[0] == 'o') && p[1] == '=')
	{
	    int can_read = 0, can_write = 0, can_execute = 0;
	    char *q = p + 2;
	    while (*q != ',' && *q != '\0')
	    {
		if (*q == 'r')
		    can_read = 1;
		else if (*q == 'w')
		    can_write = 1;
		else if (*q == 'x')
		    can_execute = 1;
		++q;
	    }
	    if (p[0] == 'u')
	    {
		if (can_read)
		    mode |= S_IRUSR;
		if (can_write)
		    mode |= S_IWUSR;
		if (can_execute)
                  {
#ifdef EXECUTE_PERMISSION_LOSES
                    KFF_DEBUG (printf ("*** S_IXUSR in change_mode().\n"));
#else /* ! EXECUTE_PERMISSION_LOSES */
                    mode |= S_IXUSR;
#endif /* EXECUTE_PERMISSION_LOSES */
                  }
	    }
	    else if (p[0] == 'g')
	    {
		if (can_read)
		    mode |= S_IRGRP;
		if (can_write)
		    mode |= S_IWGRP;
		if (can_execute)
                  {
#ifdef EXECUTE_PERMISSION_LOSES
                    KFF_DEBUG (printf ("*** S_IXGRP in change_mode().\n"));
#else /* ! EXECUTE_PERMISSION_LOSES */
		    mode |= S_IXGRP;
#endif /* EXECUTE_PERMISSION_LOSES */
                  }
	    }
	    else if (p[0] == 'o')
	    {
		if (can_read)
		    mode |= S_IROTH;
		if (can_write)
		    mode |= S_IWOTH;
		if (can_execute)
                  {
#ifdef EXECUTE_PERMISSION_LOSES
                    KFF_DEBUG (printf ("*** S_IXOTH in change_mode().\n"));
#else /* ! EXECUTE_PERMISSION_LOSES */
		    mode |= S_IXOTH;
#endif /* EXECUTE_PERMISSION_LOSES */
                  }
	    }
	}
	/* Skip to the next field.  */
	while (*p != ',' && *p != '\0')
	    ++p;
	if (*p == ',')
	    ++p;
    }
    if (chmod (filename, mode) < 0)
	return errno;
    return 0;
}

#endif /* CLIENT_SUPPORT or SERVER_SUPPORT */

#ifdef CLIENT_SUPPORT

/* The host part of CVSROOT.  */
static char *server_host;
/* The user part of CVSROOT */
static char *server_user;
/* The repository part of CVSROOT.  */
static char *server_cvsroot;

int client_active;

int client_prune_dirs;

/* Set server_host and server_cvsroot.  */
static void
parse_cvsroot ()
{
    char *p;

    server_host = xstrdup (CVSroot);
    server_cvsroot = strchr (server_host, ':');
    *server_cvsroot = '\0';
    ++server_cvsroot;

    if ( (p = strchr (server_host, '@')) == NULL) {
      server_user = NULL;
    } else {
      server_user = server_host;
      server_host = p;
      ++server_host;
      *p = '\0';
    }
 			
    client_active = 1;
}

/* Stream to write to the server.  */
FILE *to_server;
/* Stream to read from the server.  */
FILE *from_server;

#if ! RSH_NOT_TRANSPARENT
/* Process ID of rsh subprocess.  */
static int rsh_pid = -1;
#endif /* ! RSH_NOT_TRANSPARENT */


/*
 * Read a line from the server.
 *
 * Space for the result is malloc'd and should be freed by the caller.
 *
 * Returns number of bytes read.  If EOF_OK, then return 0 on end of file,
 * else end of file is an error.
 */
static int
read_line (resultp, eof_ok)
    char **resultp;
    int eof_ok;
{
    int c;
    char *result;
    int input_index = 0;
    int result_size = 80;

    fflush (to_server);
    result = (char *) xmalloc (result_size);

    while (1)
    {
	c = getc (from_server);

	if (c == EOF)
	{
	    free (result);
	    if (ferror (from_server))
		error (1, errno, "reading from server");
	    /* It's end of file.  */
	    if (eof_ok)
		return 0;
	    else
		error (1, 0, "premature end of file from server");
	}

	if (c == '\n')
	    break;
	
	result[input_index++] = c;
	while (input_index + 1 >= result_size)
	{
	    result_size *= 2;
	    result = (char *) xrealloc (result, result_size);
	}
    }

    if (resultp)
	*resultp = result;

    /* Terminate it just for kicks, but we *can* deal with embedded NULs.  */
    result[input_index] = '\0';

    if (resultp == NULL)
	free (result);
    return input_index;
}

#endif /* CLIENT_SUPPORT */


#if defined(CLIENT_SUPPORT) || defined(SERVER_SUPPORT)

/*
 * Zero if compression isn't supported or requested; non-zero to indicate
 * a compression level to request from gzip.
 */
int gzip_level;

int filter_through_gzip (fd, dir, level, pidp)
     int fd, dir, level;
     pid_t *pidp;
{
  static char buf[5] = "-";
  static char *gzip_argv[3] = { "gzip", buf };

  sprintf (buf+1, "%d", level);
  return filter_stream_through_program (fd, dir, &gzip_argv[0], pidp);
}

int filter_through_gunzip (fd, dir, pidp)
     int fd, dir;
     pid_t *pidp;
{
  static char *gunzip_argv[2] = { "gunzip" };
  return filter_stream_through_program (fd, dir, &gunzip_argv[0], pidp);
}

#endif /* CLIENT_SUPPORT or SERVER_SUPPORT */

#ifdef CLIENT_SUPPORT

/*
 * The Repository for the top level of this command (not necessarily
 * the CVSROOT, just the current directory at the time we do it).
 */
static char *toplevel_repos;

/* Working directory when we first started.  */
char toplevel_wd[PATH_MAX];

static void
handle_ok (args, len)
    char *args;
    int len;
{
    return;
}

static void
handle_error (args, len)
    char *args;
    int len;
{
    int something_printed;
    
    /*
     * First there is a symbolic error code followed by a space, which
     * we ignore.
     */
    char *p = strchr (args, ' ');
    if (p == NULL)
    {
	error (0, 0, "invalid data from cvs server");
	return;
    }
    ++p;
    len -= p - args;
    something_printed = 0;
    for (; len > 0; --len)
    {
	something_printed = 1;
	putc (*p++, stderr);
    }
    if (something_printed)
	putc ('\n', stderr);
}

static void
handle_valid_requests (args, len)
    char *args;
    int len;
{
    char *p = args;
    char *q;
    struct request *rq;
    do
    {
	q = strchr (p, ' ');
	if (q != NULL)
	    *q++ = '\0';
	for (rq = requests; rq->name != NULL; ++rq)
	{
	    if (strcmp (rq->name, p) == 0)
		break;
	}
	if (rq->name == NULL)
	    /*
	     * It is a request we have never heard of (and thus never
	     * will want to use).  So don't worry about it.
	     */
	    ;
	else
	{
	    if (rq->status == rq_enableme)
	    {
		/*
		 * Server wants to know if we have this, to enable the
		 * feature.
		 */
		if (fprintf(to_server, "%s\n", rq->name) < 0)
		    error (1, errno, "writing to server");
		if (!strcmp("UseUnchanged",rq->name))
		    use_unchanged = 1;
	    }
	    else
		rq->status = rq_supported;
	}
	p = q;
    } while (q != NULL);
    for (rq = requests; rq->name != NULL; ++rq)
    {
	if (rq->status == rq_essential)
	    error (1, 0, "request `%s' not supported by server", rq->name);
	else if (rq->status == rq_optional)
	    rq->status = rq_not_supported;
    }
}

static int use_directory = -1;

static char *get_short_pathname PROTO((const char *));

static char *
get_short_pathname (name)
    const char *name;
{
    const char *retval;
    if (use_directory)
	return (char *) name;
    if (strncmp (name, toplevel_repos, strlen (toplevel_repos)) != 0)
	error (1, 0, "server bug: name `%s' doesn't specify file in `%s'",
	       name, toplevel_repos);
    retval = name + strlen (toplevel_repos) + 1;
    if (retval[-1] != '/')
	error (1, 0, "server bug: name `%s' doesn't specify file in `%s'",
	       name, toplevel_repos);
    return (char *) retval;
}

/*
 * Do all the processing for PATHNAME, where pathname consists of the
 * repository and the filename.  The parameters we pass to FUNC are:
 * DATA is just the DATA parameter which was passed to
 * call_in_directory; ENT_LIST is a pointer to an entries list (which
 * we manage the storage for); SHORT_PATHNAME is the pathname of the
 * file relative to the (overall) directory in which the command is
 * taking place; and FILENAME is the filename portion only of
 * SHORT_PATHNAME.  When we call FUNC, the curent directory points to
 * the directory portion of SHORT_PATHNAME.  */

static char *last_dirname;

static void
call_in_directory (pathname, func, data)
    char *pathname;
    void (*func) PROTO((char *data, List *ent_list, char *short_pathname,
			  char *filename));
    char *data;
{
    static List *last_entries;

    char *dirname;
    char *filename;
    /* Just the part of pathname relative to toplevel_repos.  */
    char *short_pathname = get_short_pathname (pathname);
    char *p;

    /*
     * Do the whole descent in parallel for the repositories, so we
     * know what to put in CVS/Repository files.  I'm not sure the
     * full hair is necessary since the server does a similar
     * computation; I suspect that we only end up creating one
     * directory at a time anyway.
     *
     * Also note that we must *only* worry about this stuff when we
     * are creating directories; `cvs co foo/bar; cd foo/bar; cvs co
     * CVSROOT; cvs update' is legitimate, but in this case
     * foo/bar/CVSROOT/CVS/Repository is not a subdirectory of
     * foo/bar/CVS/Repository.
     */
    char *reposname;
    char *short_repos;
    char *reposdirname;
    char *rdirp;
    int reposdirname_absolute;

    reposname = NULL;
    if (use_directory)
	read_line (&reposname, 0);

    reposdirname_absolute = 0;
    if (reposname != NULL)
    {
	if (strncmp (reposname, toplevel_repos, strlen (toplevel_repos)) != 0)
	{
	    reposdirname_absolute = 1;
	    short_repos = reposname;
	}
	else
	{
	    short_repos = reposname + strlen (toplevel_repos) + 1;
	    if (short_repos[-1] != '/')
	    {
		reposdirname_absolute = 1;
		short_repos = reposname;
	    }
	}
    }
    else
    {
	short_repos = short_pathname;
    }
    reposdirname = xstrdup (short_repos);
    p = strrchr (reposdirname, '/');
    if (p == NULL)
    {
	reposdirname = xrealloc (reposdirname, 2);
	reposdirname[0] = '.'; reposdirname[1] = '\0';
    }
    else
	*p = '\0';

    dirname = xstrdup (short_pathname);
    p = strrchr (dirname, '/');
    if (p == NULL)
    {
	dirname = xrealloc (dirname, 2);
	dirname[0] = '.'; dirname[1] = '\0';
    }
    else
	*p = '\0';
    if (client_prune_dirs)
	add_prune_candidate (dirname);

    filename = strrchr (short_repos, '/');
    if (filename == NULL)
	filename = short_repos;
    else
	++filename;

    if (reposname != NULL)
    {
	/* This is the use_directory case.  */

	short_pathname = xmalloc (strlen (pathname) + strlen (filename) + 5);
	strcpy (short_pathname, pathname);
	strcat (short_pathname, filename);
    }

    if (last_dirname == NULL
	|| strcmp (last_dirname, dirname) != 0)
    {
	if (last_dirname)
	    free (last_dirname);
	last_dirname = dirname;

	if (toplevel_wd[0] == '\0')
	    if (getwd (toplevel_wd) == NULL)
		error (1, 0,
		       "could not get working directory: %s", toplevel_wd);

	if (chdir (toplevel_wd) < 0)
	    error (1, errno, "could not chdir to %s", toplevel_wd);
	if (chdir (dirname) < 0)
	{
	    char *dir;
	    char *dirp;
	    
	    if (! existence_error (errno))
		error (1, errno, "could not chdir to %s", dirname);
	    
	    /* Directory does not exist, we need to create it.  */
	    dir = xmalloc (strlen (dirname) + 1);
	    dirp = dirname;
	    rdirp = reposdirname;

	    /* This algorithm makes nested directories one at a time
               and create CVS administration files in them.  For
               example, we're checking out foo/bar/baz from the
               repository:

	       1) create foo, point CVS/Repository to <root>/foo
	       2)     .. foo/bar                   .. <root>/foo/bar
	       3)     .. foo/bar/baz               .. <root>/foo/bar/baz
	       
	       As you can see, we're just stepping along DIRNAME (with
	       DIRP) and REPOSDIRNAME (with RDIRP) respectively.

	       We need to be careful when we are checking out a
	       module, however, since DIRNAME and REPOSDIRNAME are not
	       going to be the same.  Since modules will not have any
	       slashes in their names, we should watch the output of
	       STRCHR to decide whether or not we should use STRCHR on
	       the RDIRP.  That is, if we're down to a module name,
	       don't keep picking apart the repository directory name.  */

	    do
	    {
		dirp = strchr (dirp, '/');
		if (dirp)
		  {
		    strncpy (dir, dirname, dirp - dirname);
		    dir[dirp - dirname] = '\0';
		    /* Skip the slash.  */
		    ++dirp;
		    if (rdirp == NULL)
		      error (0, 0,
			     "internal error: repository string too short.");
		    else
		      rdirp = strchr (rdirp, '/');
		  }
		else
		  {
		    /* If there are no more slashes in the dir name,
                       we're down to the most nested directory -OR- to
                       the name of a module.  In the first case, we
                       should be down to a DIRP that has no slashes,
                       so it won't help/hurt to do another STRCHR call
                       on DIRP.  It will definitely hurt, however, if
                       we're down to a module name, since a module
                       name can point to a nested directory (that is,
                       DIRP will still have slashes in it.  Therefore,
                       we should set it to NULL so the routine below
                       copies the contents of REMOTEDIRNAME onto the
                       root repository directory (does this if rdirp
                       is set to NULL, because we used to do an extra
                       STRCHR call here). */

		    rdirp = NULL;
		    strcpy (dir, dirname);
		  }

		if (CVS_MKDIR (dir, 0777) < 0)
		{
                  /* Now, let me get this straight.  In IBM C/C++
                   * under OS/2, the error string for EEXIST is:
                   *
                   *     "The file already exists",
                   *
                   * and the error string for EACCESS is:
                   *
                   *     "The file or directory specified is read-only".
                   *
                   * Nonetheless, mkdir() will set EACCESS if the
                   * directory *exists*, according both to the
                   * documentation and its actual behavior.
                   *
                   * I'm sure that this made sense, to someone,
                   * somewhere, sometime.  Just not me, here, now.
                   */
#ifdef EACCESS
                  if ((errno != EACCESS) && (errno != EEXIST))
                    error (1, errno, "cannot make directory %s", dir);
#else /* ! defined(EACCESS) */
                  if ((errno != EEXIST))
                    error (1, errno, "cannot make directory %s", dir);
#endif /* defined(EACCESS) */
                  
                  /* It already existed, fine.  Just keep going.  */
		}
		else if (strcmp (command_name, "export") == 0)
		    /* Don't create CVSADM directories if this is export.  */
		    ;
		else
		{
		    /*
		     * Put repository in CVS/Repository.  For historical
		     * (pre-CVS/Root) reasons, this is an absolute pathname,
		     * but what really matters is the part of it which is
		     * relative to cvsroot.
		     */
		    char *repo;
		    char *r;

		    repo = xmalloc (strlen (reposdirname)
				    + strlen (toplevel_repos)
				    + 80);
		    if (reposdirname_absolute)
			r = repo;
		    else
		    {
			strcpy (repo, toplevel_repos);
			strcat (repo, "/");
			r = repo + strlen (repo);
		    }

		    if (rdirp)
		    {
			strncpy (r, reposdirname, rdirp - reposdirname);
			r[rdirp - reposdirname] = '\0';
		    }
		    else
			strcpy (r, reposdirname);

		    Create_Admin (dir, dir, repo,
				  (char *)NULL, (char *)NULL);
		    free (repo);
		}

		if (rdirp != NULL)
		{
		    /* Skip the slash.  */
		    ++rdirp;
		}

	    } while (dirp != NULL);
	    free (dir);
	    /* Now it better work.  */
	    if (chdir (dirname) < 0)
		error (1, errno, "could not chdir to %s", dirname);
	}

	if (strcmp (command_name, "export") != 0)
	{
	    if (last_entries)
		Entries_Close (last_entries);
	    last_entries = Entries_Open (0);
	}
    }
    else
	free (dirname);
    free (reposdirname);
    (*func) (data, last_entries, short_pathname, filename);
    if (reposname != NULL)
    {
	free (short_pathname);
	free (reposname);
    }
}

static void
copy_a_file (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    char *newname;

    read_line (&newname, 0);
    copy_file (filename, newname);
    free (newname);
}

static void
handle_copy_file (args, len)
     char *args;
     int len;
{
    call_in_directory (args, copy_a_file, (char *)NULL);
}

/*
 * The Checksum response gives the checksum for the file transferred
 * over by the next Updated, Merged or Patch response.  We just store
 * it here, and then check it in update_entries.
 */

static int stored_checksum_valid;
static unsigned char stored_checksum[16];

static void
handle_checksum (args, len)
     char *args;
     int len;
{
    char *s;
    char buf[3];
    int i;

    if (stored_checksum_valid)
        error (1, 0, "Checksum received before last one was used");

    s = args;
    buf[2] = '\0';
    for (i = 0; i < 16; i++)
    {
        char *bufend;

	buf[0] = *s++;
	buf[1] = *s++;
	stored_checksum[i] = (char) strtol (buf, &bufend, 16);
	if (bufend != buf + 2)
	    break;
    }

    if (i < 16 || *s != '\0')
        error (1, 0, "Invalid Checksum response: `%s'", args);

    stored_checksum_valid = 1;
}

/*
 * If we receive a patch, but the patch program fails to apply it, we
 * want to request the original file.  We keep a list of files whose
 * patches have failed.
 */

char **failed_patches;
int failed_patches_count;

struct update_entries_data
{
    enum {
      /*
       * We are just getting an Entries line; the local file is
       * correct.
       */
      UPDATE_ENTRIES_CHECKIN,
      /* We are getting the file contents as well.  */
      UPDATE_ENTRIES_UPDATE,
      /*
       * We are getting a patch against the existing local file, not
       * an entire new file.
       */
      UPDATE_ENTRIES_PATCH
    } contents;

    /*
     * String to put in the timestamp field or NULL to use the timestamp
     * of the file.
     */
    char *timestamp;
};

/* Update the Entries line for this file.  */
static void
update_entries (data_arg, ent_list, short_pathname, filename)
    char *data_arg;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    char *entries_line;
    struct update_entries_data *data = (struct update_entries_data *)data_arg;

    char *cp;
    char *user;
    char *vn;
    /* Timestamp field.  Always empty according to the protocol.  */
    char *ts;
    char *options;
    char *tag;
    char *date;
    char *tag_or_date;
    char *scratch_entries;
    int bin = 0;

    read_line (&entries_line, 0);

    /*
     * Parse the entries line.
     */
    if (strcmp (command_name, "export") != 0)
    {
	scratch_entries = xstrdup (entries_line);

	if (scratch_entries[0] != '/')
	    error (1, 0, "bad entries line `%s' from server", entries_line);
	user = scratch_entries + 1;
	if ((cp = strchr (user, '/')) == NULL)
	    error (1, 0, "bad entries line `%s' from server", entries_line);
	*cp++ = '\0';
	vn = cp;
	if ((cp = strchr (vn, '/')) == NULL)
	    error (1, 0, "bad entries line `%s' from server", entries_line);
	*cp++ = '\0';
	
	ts = cp;
	if ((cp = strchr (ts, '/')) == NULL)
	    error (1, 0, "bad entries line `%s' from server", entries_line);
	*cp++ = '\0';
	options = cp;
	if ((cp = strchr (options, '/')) == NULL)
	    error (1, 0, "bad entries line `%s' from server", entries_line);
	*cp++ = '\0';
	tag_or_date = cp;

	/* If a slash ends the tag_or_date, ignore everything after it.  */
	cp = strchr (tag_or_date, '/');
	if (cp != NULL)
	    *cp = '\0';
	tag = (char *) NULL;
	date = (char *) NULL;
	if (*tag_or_date == 'T')
	    tag = tag_or_date + 1;
	else if (*tag_or_date == 'D')
	    date = tag_or_date + 1;
    }

    if (data->contents == UPDATE_ENTRIES_UPDATE
	|| data->contents == UPDATE_ENTRIES_PATCH)
    {
	char *size_string;
	char *mode_string;
	int size;
	int size_read;
	int size_left;
	int fd;
	char *buf;
	char *buf2;
	char *temp_filename;
	int use_gzip, gzip_status;
	pid_t gzip_pid = 0;

	read_line (&mode_string, 0);
	
	read_line (&size_string, 0);
	if (size_string[0] == 'z')
	{
	    use_gzip = 1;
	    size = atoi (size_string+1);
	}
	else
	{
	    use_gzip = 0;
	    size = atoi (size_string);
	}
	free (size_string);

	temp_filename = xmalloc (strlen (filename) + 80);
#ifdef _POSIX_NO_TRUNC
	sprintf (temp_filename, ".new.%.9s", filename);
#else /* _POSIX_NO_TRUNC */
	sprintf (temp_filename, ".new.%s", filename);
#endif /* _POSIX_NO_TRUNC */
	buf = xmalloc (size);

        /* Some systems, like OS/2 and Windows NT, end lines with CRLF
           instead of just LF.  Format translation is done in the C
           library I/O funtions.  Here we tell them whether or not to
           convert -- if this file is marked "binary" with the RCS -kb
           flag, then we don't want to convert, else we do (because
           CVS assumes text files by default). */

        bin = !(strcmp (options, "-kb"));
        fd = open (temp_filename,
                   O_WRONLY | O_CREAT | O_TRUNC | (bin ? OPEN_BINARY : 0),
                   0777);

	if (fd < 0)
	    error (1, errno, "writing %s", short_pathname);

	if (use_gzip)
	    fd = filter_through_gunzip (fd, 0, &gzip_pid);

	if (size > 0)
	{
	    buf2 = buf;
	    size_left = size;
	    while ((size_read = fread (buf2, 1, size_left, from_server)) != size_left)
	    {
		if (feof (from_server))
		    /* FIXME: Should delete temp_filename.  */
		    error (1, 0, "unexpected end of file from server");
		else if (ferror (from_server))
		    /* FIXME: Should delete temp_filename.  */
		    error (1, errno, "reading from server");
		else
		  {
		    /* short reads are ok if we keep trying */
		    buf2 += size_read;
		    size_left -= size_read;
		  }
	    }
	    if (write (fd, buf, size) != size)
		error (1, errno, "writing %s", short_pathname);
	}
	if (close (fd) < 0)
	    error (1, errno, "writing %s", short_pathname);
	if (gzip_pid > 0)
	{
	    if (waitpid (gzip_pid, &gzip_status, 0) == -1)
		error (1, errno, "waiting for gzip process %d", gzip_pid);
	    else if (gzip_status != 0)
		error (1, 0, "gzip process exited %d", gzip_status);
	}

	gzip_pid = -1;

	/* Since gunzip writes files without converting LF to CRLF
	   (a reasonable behavior), we now have a patch file in LF
	   format.  Leave the file as is if we're just going to feed
	   it to patch; patch can handle it.  However, if it's the
	   final source file, convert it.  */

	if (data->contents == UPDATE_ENTRIES_UPDATE)
	{
#ifdef LINES_CRLF_TERMINATED

            /* `bin' is non-zero iff `options' contains "-kb", meaning
                treat this file as binary. */

	    if (use_gzip && (! bin))
	    {
	        convert_file (temp_filename, O_RDONLY | OPEN_BINARY,
	    		      filename, O_WRONLY | O_CREAT | O_TRUNC);
	        if (unlink (temp_filename) < 0)
	            error (0, errno, "warning: couldn't delete %s",
                           temp_filename);
	    }
	    else
		rename_file (temp_filename, filename);
	        
#else /* ! LINES_CRLF_TERMINATED */
	    rename_file (temp_filename, filename);
#endif /* LINES_CRLF_TERMINATED */
	}
	else
	{
	    int retcode;
	    char backup[PATH_MAX];
	    struct stat s;

	    (void) sprintf (backup, "%s~", filename);
	    (void) unlink_file (backup);
	    if (!isfile (filename))
	        error (1, 0, "patch original file %s does not exist",
		       short_pathname);
	    if (stat (temp_filename, &s) < 0)
	        error (1, 1, "can't stat patch file %s", temp_filename);
	    if (s.st_size == 0)
	        retcode = 0;
	    else
	    {
	        run_setup ("%s -f -s -b ~ %s %s", PATCH_PROGRAM,
			   filename, temp_filename);
		retcode = run_exec (DEVNULL, RUN_TTY, RUN_TTY, RUN_NORMAL);
	    }
	    /* FIXME: should we really be silently ignoring errors?  */
	    (void) unlink_file (temp_filename);
	    if (retcode == 0)
	    {
		/* FIXME: should we really be silently ignoring errors?  */
		(void) unlink_file (backup);
	    }
	    else
	    {
	        int old_errno = errno;
		char *path_tmp;

	        if (isfile (backup))
		    rename_file (backup, filename);
       
		/* Get rid of the patch reject file.  */
		path_tmp = xmalloc (strlen (filename + 10));
		strcpy (path_tmp, filename);
		strcat (path_tmp, ".rej");
		/* FIXME: should we really be silently ignoring errors?  */
		(void) unlink_file (path_tmp);
		free (path_tmp);

		/* Save this file to retrieve later.  */
		failed_patches =
		    (char **) xrealloc ((char *) failed_patches,
					((failed_patches_count + 1)
					 * sizeof (char *)));
		failed_patches[failed_patches_count] =
		    xstrdup (short_pathname);
		++failed_patches_count;

		error (retcode == -1 ? 1 : 0, retcode == -1 ? old_errno : 0,
		       "could not patch %s%s", filename,
		       retcode == -1 ? "" : "; will refetch");

		stored_checksum_valid = 0;

		return;
	    }
	}
	free (temp_filename);

	if (stored_checksum_valid)
	{
	    FILE *e;
	    struct MD5Context context;
	    unsigned char buf[8192];
	    unsigned len;
	    unsigned char checksum[16];

	    /*
	     * Compute the MD5 checksum.  This will normally only be
	     * used when receiving a patch, so we always compute it
	     * here on the final file, rather than on the received
	     * data.
	     *
	     * Note that if the file is a text file, we should read it
	     * here using text mode, so its lines will be terminated the same
	     * way they were transmitted.
	     */
	    e = fopen (filename, "r");
	    if (e == NULL)
	        error (1, errno, "could not open %s", short_pathname);

	    MD5Init (&context);
	    while ((len = fread (buf, 1, sizeof buf, e)) != 0)
		MD5Update (&context, buf, len);
	    if (ferror (e))
		error (1, errno, "could not read %s", short_pathname);
	    MD5Final (checksum, &context);

	    fclose (e);

	    stored_checksum_valid = 0;

	    if (memcmp (checksum, stored_checksum, 16) != 0)
	    {
	        if (data->contents != UPDATE_ENTRIES_PATCH)
		    error (1, 0, "checksum failure on %s",
			   short_pathname);

		error (0, 0,
		       "checksum failure after patch to %s; will refetch",
		       short_pathname);

		/* Save this file to retrieve later.  */
		failed_patches =
		    (char **) xrealloc ((char *) failed_patches,
					((failed_patches_count + 1)
					 * sizeof (char *)));
		failed_patches[failed_patches_count] =
		    xstrdup (short_pathname);
		++failed_patches_count;

		return;
	    }
	}

        {
	    /* FIXME: we should be respecting the umask.  */
	    int status = change_mode (filename, mode_string);
	    if (status != 0)
		error (0, status, "cannot change mode of %s", short_pathname);
	}

	free (mode_string);
	free (buf);
    }

    /*
     * Process the entries line.  Do this after we've written the file,
     * since we need the timestamp.
     */
    if (strcmp (command_name, "export") != 0)
    {
	char *local_timestamp;
	char *file_timestamp;

	local_timestamp = data->timestamp;
	if (local_timestamp == NULL || ts[0] == '+')
	    file_timestamp = time_stamp (filename);
	else
	    file_timestamp = NULL;

	/*
	 * These special version numbers signify that it is not up to
	 * date.  Create a dummy timestamp which will never compare
	 * equal to the timestamp of the file.
	 */
	if (vn[0] == '\0' || vn[0] == '0' || vn[0] == '-')
	    local_timestamp = "dummy timestamp";
	else if (local_timestamp == NULL)
	    local_timestamp = file_timestamp;

	Register (ent_list, filename, vn, local_timestamp,
		  options, tag, date, ts[0] == '+' ? file_timestamp : NULL);

	if (file_timestamp)
	    free (file_timestamp);

	free (scratch_entries);
    }
    free (entries_line);
}

static void
handle_checked_in (args, len)
    char *args;
    int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_CHECKIN;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void
handle_new_entry (args, len)
    char *args;
    int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_CHECKIN;
    dat.timestamp = "dummy timestamp from new-entry";
    call_in_directory (args, update_entries, (char *)&dat);
}

static void
handle_updated (args, len)
    char *args;
    int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_UPDATE;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void
handle_merged (args, len)
    char *args;
    int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_UPDATE;
    dat.timestamp = "Result of merge";
    call_in_directory (args, update_entries, (char *)&dat);
}

static void
handle_patched (args, len)
     char *args;
     int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_PATCH;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void
remove_entry (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    Scratch_Entry (ent_list, filename);
}

static void
handle_remove_entry (args, len)
    char *args;
    int len;
{
    call_in_directory (args, remove_entry, (char *)NULL);
}

static void
remove_entry_and_file (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    Scratch_Entry (ent_list, filename);
    if (unlink_file (filename) < 0)
	error (0, errno, "unable to remove %s", short_pathname);
}

static void
handle_removed (args, len)
    char *args;
    int len;
{
    call_in_directory (args, remove_entry_and_file, (char *)NULL);
}

/* Is this the top level (directory containing CVSROOT)?  */
static int
is_cvsroot_level (pathname)
    char *pathname;
{
    char *short_pathname;

    if (strcmp (toplevel_repos, server_cvsroot) != 0)
	return 0;

    if (!use_directory)
    {
	if (strncmp (pathname, server_cvsroot, strlen (server_cvsroot)) != 0)
	    error (1, 0,
		   "server bug: pathname `%s' doesn't specify file in `%s'",
		   pathname, server_cvsroot);
	short_pathname = pathname + strlen (server_cvsroot) + 1;
	if (short_pathname[-1] != '/')
	    error (1, 0,
		   "server bug: pathname `%s' doesn't specify file in `%s'",
		   pathname, server_cvsroot);
	return strchr (short_pathname, '/') == NULL;
    }
    else
    {
	return strchr (pathname, '/') == NULL;
    }
}

static void
set_static (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    FILE *fp;
    fp = open_file (CVSADM_ENTSTAT, "w+");
    if (fclose (fp) == EOF)
        error (1, errno, "cannot close %s", CVSADM_ENTSTAT);
}

static void
handle_set_static_directory (args, len)
    char *args;
    int len;
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL, 0);
	return;
    }
    call_in_directory (args, set_static, (char *)NULL);
}

static void
clear_static (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    if (unlink_file (CVSADM_ENTSTAT) < 0 && ! existence_error (errno))
        error (1, errno, "cannot remove file %s", CVSADM_ENTSTAT);
}

static void
handle_clear_static_directory (pathname, len)
    char *pathname;
    int len;
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL, 0);
	return;
    }

    if (is_cvsroot_level (pathname))
    {
        /*
	 * Top level (directory containing CVSROOT).  This seems to normally
	 * lack a CVS directory, so don't try to create files in it.
	 */
	return;
    }
    call_in_directory (pathname, clear_static, (char *)NULL);
}

static void
set_sticky (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    char *tagspec;
    FILE *f;

    read_line (&tagspec, 0);
    f = open_file (CVSADM_TAG, "w+");
    if (fprintf (f, "%s\n", tagspec) < 0)
	error (1, errno, "writing %s", CVSADM_TAG);
    if (fclose (f) == EOF)
	error (1, errno, "closing %s", CVSADM_TAG);
    free (tagspec);
}

static void
handle_set_sticky (pathname, len)
    char *pathname;
    int len;
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL, 0);
        /* Swallow the tag line.  */
	(void) read_line (NULL, 0);
	return;
    }
    if (is_cvsroot_level (pathname))
    {
        /*
	 * Top level (directory containing CVSROOT).  This seems to normally
	 * lack a CVS directory, so don't try to create files in it.
	 */

	/* Swallow the repository.  */
	read_line (NULL, 0);
        /* Swallow the tag line.  */
	(void) read_line (NULL, 0);
	return;
    }

    call_in_directory (pathname, set_sticky, (char *)NULL);
}

static void
clear_sticky (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    if (unlink_file (CVSADM_TAG) < 0 && ! existence_error (errno))
	error (1, errno, "cannot remove %s", CVSADM_TAG);
}

static void
handle_clear_sticky (pathname, len)
    char *pathname;
    int len;
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL, 0);
	return;
    }

    if (is_cvsroot_level (pathname))
    {
        /*
	 * Top level (directory containing CVSROOT).  This seems to normally
	 * lack a CVS directory, so don't try to create files in it.
	 */
	return;
    }

    call_in_directory (pathname, clear_sticky, (char *)NULL);
}

struct save_prog {
    char *name;
    char *dir;
    struct save_prog *next;
};

static struct save_prog *checkin_progs;
static struct save_prog *update_progs;

/*
 * Unlike some requests this doesn't include the repository.  So we can't
 * just call call_in_directory and have the right thing happen; we save up
 * the requests and do them at the end.
 */
static void
handle_set_checkin_prog (args, len)
    char *args;
    int len;
{
    char *prog;
    struct save_prog *p;
    read_line (&prog, 0);
    p = (struct save_prog *) xmalloc (sizeof (struct save_prog));
    p->next = checkin_progs;
    p->dir = xstrdup (args);
    p->name = prog;
    checkin_progs = p;
}
    
static void
handle_set_update_prog (args, len)
    char *args;
    int len;
{
    char *prog;
    struct save_prog *p;
    read_line (&prog, 0);
    p = (struct save_prog *) xmalloc (sizeof (struct save_prog));
    p->next = update_progs;
    p->dir = xstrdup (args);
    p->name = prog;
    update_progs = p;
}

static void do_deferred_progs PROTO((void));

static void
do_deferred_progs ()
{
    struct save_prog *p;
    struct save_prog *q;

    char fname[PATH_MAX];
    FILE *f;
    if (toplevel_wd[0] != '\0')
      {
	if (chdir (toplevel_wd) < 0)
	  error (1, errno, "could not chdir to %s", toplevel_wd);
      }
    for (p = checkin_progs; p != NULL; )
    {
	sprintf (fname, "%s/%s", p->dir, CVSADM_CIPROG);
	f = open_file (fname, "w");
	if (fprintf (f, "%s\n", p->name) < 0)
	    error (1, errno, "writing %s", fname);
	if (fclose (f) == EOF)
	    error (1, errno, "closing %s", fname);
	free (p->name);
	free (p->dir);
	q = p->next;
	free (p);
	p = q;
    }
    checkin_progs = NULL;
    for (p = update_progs; p != NULL; p = p->next)
    {
	sprintf (fname, "%s/%s", p->dir, CVSADM_UPROG);
	f = open_file (fname, "w");
	if (fprintf (f, "%s\n", p->name) < 0)
	    error (1, errno, "writing %s", fname);
	if (fclose (f) == EOF)
	    error (1, errno, "closing %s", fname);
	free (p->name);
	free (p->dir);
	free (p);
    }
    update_progs = NULL;
}

static int client_isemptydir PROTO((char *));

/*
 * Returns 1 if the argument directory exists and is completely empty,
 * other than the existence of the CVS directory entry.  Zero otherwise.
 */
static int
client_isemptydir (dir)
    char *dir;
{
    DIR *dirp;
    struct dirent *dp;

    if ((dirp = opendir (dir)) == NULL)
    {
	if (! existence_error (errno))
	    error (0, errno, "cannot open directory %s for empty check", dir);
	return (0);
    }
    errno = 0;
    while ((dp = readdir (dirp)) != NULL)
    {
	if (strcmp (dp->d_name, ".") != 0 && strcmp (dp->d_name, "..") != 0 &&
	    strcmp (dp->d_name, CVSADM) != 0)
	{
	    (void) closedir (dirp);
	    return (0);
	}
    }
    if (errno != 0)
    {
	error (0, errno, "cannot read directory %s", dir);
	(void) closedir (dirp);
	return (0);
    }
    (void) closedir (dirp);
    return (1);
}

struct save_dir {
    char *dir;
    struct save_dir *next;
};

struct save_dir *prune_candidates;

static void
add_prune_candidate (dir)
    char *dir;
{
    struct save_dir *p;

    if (dir[0] == '.' && dir[1] == '\0')
	return;
    p = (struct save_dir *) xmalloc (sizeof (struct save_dir));
    p->dir = xstrdup (dir);
    p->next = prune_candidates;
    prune_candidates = p;
}

static void process_prune_candidates PROTO((void));

static void
process_prune_candidates ()
{
    struct save_dir *p;
    struct save_dir *q;

    if (toplevel_wd[0] != '\0')
      {
	if (chdir (toplevel_wd) < 0)
	  error (1, errno, "could not chdir to %s", toplevel_wd);
      }
    for (p = prune_candidates; p != NULL; )
    {
	if (client_isemptydir (p->dir))
	{
          unlink_file_dir (p->dir);
	}
	free (p->dir);
	q = p->next;
	free (p);
	p = q;
    }
}

/* Send a Repository line.  */

static char *last_repos;
static char *last_update_dir;

static void send_repository PROTO((char *, char *, char *));

static void
send_repository (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    char *adm_name;

    if (update_dir == NULL || update_dir[0] == '\0')
	update_dir = ".";

    if (last_repos != NULL
	&& strcmp (repos, last_repos) == 0
	&& last_update_dir != NULL
	&& strcmp (update_dir, last_update_dir) == 0)
	/* We've already sent it.  */
	return;

    if (client_prune_dirs)
	add_prune_candidate (update_dir);

    /* 80 is large enough for any of CVSADM_*.  */
    adm_name = xmalloc (strlen (dir) + 80);

    if (use_directory == -1)
	use_directory = supported_request ("Directory");

    if (use_directory)
    {
	if (fprintf (to_server, "Directory ") < 0)
	    error (1, errno, "writing to server");
	if (fprintf (to_server, "%s", update_dir) < 0)
	    error (1, errno, "writing to server");

	if (fprintf (to_server, "\n%s\n", repos)
	    < 0)
	    error (1, errno, "writing to server");
    }
    else
    {
	if (fprintf (to_server, "Repository %s\n", repos) < 0)
	    error (1, errno, "writing to server");
    }
    if (supported_request ("Static-directory"))
    {
	adm_name[0] = '\0';
	if (dir[0] != '\0')
	{
	    strcat (adm_name, dir);
	    strcat (adm_name, "/");
	}
	strcat (adm_name, CVSADM_ENTSTAT);
	if (isreadable (adm_name))
	{
	    if (fprintf (to_server, "Static-directory\n") < 0)
		error (1, errno, "writing to server");
	}
    }
    if (supported_request ("Sticky"))
    {
	FILE *f;
	if (dir[0] == '\0')
	    strcpy (adm_name, CVSADM_TAG);
	else
	    sprintf (adm_name, "%s/%s", dir, CVSADM_TAG);

	f = fopen (adm_name, "r");
	if (f == NULL)
	{
	    if (! existence_error (errno))
		error (1, errno, "reading %s", adm_name);
	}
	else
	{
	    char line[80];
	    char *nl;
	    if (fprintf (to_server, "Sticky ") < 0)
		error (1, errno, "writing to server");
	    while (fgets (line, sizeof (line), f) != NULL)
	    {
		if (fprintf (to_server, "%s", line) < 0)
		    error (1, errno, "writing to server");
		nl = strchr (line, '\n');
		if (nl != NULL)
		    break;
	    }
	    if (nl == NULL)
		if (fprintf (to_server, "\n") < 0)
		    error (1, errno, "writing to server");
	    if (fclose (f) == EOF)
		error (0, errno, "closing %s", adm_name);
	}
    }
    if (supported_request ("Checkin-prog"))
    {
	FILE *f;
	if (dir[0] == '\0')
	    strcpy (adm_name, CVSADM_CIPROG);
	else
	    sprintf (adm_name, "%s/%s", dir, CVSADM_CIPROG);

	f = fopen (adm_name, "r");
	if (f == NULL)
	{
	    if (! existence_error (errno))
		error (1, errno, "reading %s", adm_name);
	}
	else
	{
	    char line[80];
	    char *nl;
	    if (fprintf (to_server, "Checkin-prog ") < 0)
		error (1, errno, "writing to server");
	    while (fgets (line, sizeof (line), f) != NULL)
	    {
		if (fprintf (to_server, "%s", line) < 0)
		    error (1, errno, "writing to server");
		nl = strchr (line, '\n');
		if (nl != NULL)
		    break;
	    }
	    if (nl == NULL)
		if (fprintf (to_server, "\n") < 0)
		    error (1, errno, "writing to server");
	    if (fclose (f) == EOF)
		error (0, errno, "closing %s", adm_name);
	}
    }
    if (supported_request ("Update-prog"))
    {
	FILE *f;
	if (dir[0] == '\0')
	    strcpy (adm_name, CVSADM_UPROG);
	else
	    sprintf (adm_name, "%s/%s", dir, CVSADM_UPROG);

	f = fopen (adm_name, "r");
	if (f == NULL)
	{
	    if (! existence_error (errno))
		error (1, errno, "reading %s", adm_name);
	}
	else
	{
	    char line[80];
	    char *nl;
	    if (fprintf (to_server, "Update-prog ") < 0)
		error (1, errno, "writing to server");
	    while (fgets (line, sizeof (line), f) != NULL)
	    {
		if (fprintf (to_server, "%s", line) < 0)
		    error (1, errno, "writing to server");
		nl = strchr (line, '\n');
		if (nl != NULL)
		    break;
	    }
	    if (nl == NULL)
		if (fprintf (to_server, "\n") < 0)
		    error (1, errno, "writing to server");
	    if (fclose (f) == EOF)
		error (0, errno, "closing %s", adm_name);
	}
    }
    if (last_repos != NULL)
	free (last_repos);
    if (last_update_dir != NULL)
	free (last_update_dir);
    last_repos = xstrdup (repos);
    last_update_dir = xstrdup (update_dir);
}

/* Send a Repository line and set toplevel_repos.  */
static void send_a_repository PROTO((char *, char *, char *));

static void
send_a_repository (dir, repository, update_dir)
    char *dir;
    char *repository;
    char *update_dir;
{
    if (toplevel_repos == NULL && repository != NULL)
    {
	if (update_dir[0] == '\0'
	    || (update_dir[0] == '.' && update_dir[1] == '\0'))
	    toplevel_repos = xstrdup (repository);
	else
	{
	    /*
	     * Get the repository from a CVS/Repository file if update_dir
	     * is absolute.  This is not correct in general, because
	     * the CVS/Repository file might not be the top-level one.
	     * This is for cases like "cvs update /foo/bar" (I'm not
	     * sure it matters what toplevel_repos we get, but it does
	     * matter that we don't hit the "internal error" code below).
	     */
	    if (update_dir[0] == '/')
		toplevel_repos = Name_Repository (update_dir, update_dir);
	    else
	    {
		/*
		 * Guess the repository of that directory by looking at a
		 * subdirectory and removing as many pathname components
		 * as are in update_dir.  I think that will always (or at
		 * least almost always) be 1.
		 *
		 * So this deals with directories which have been
		 * renamed, though it doesn't necessarily deal with
		 * directories which have been put inside other
		 * directories (and cvs invoked on the containing
		 * directory).  I'm not sure the latter case needs to
		 * work.
		 */
		/*
		 * This gets toplevel_repos wrong for "cvs update ../foo"
		 * but I'm not sure toplevel_repos matters in that case.
		 */
		int slashes_in_update_dir;
		int slashes_skipped;
		char *p;

		slashes_in_update_dir = 0;
		for (p = update_dir; *p != '\0'; ++p)
		    if (*p == '/')
			++slashes_in_update_dir;

		slashes_skipped = 0;
		p = repository + strlen (repository);
		while (1)
		{
		    if (p == repository)
			error (1, 0,
			       "internal error: not enough slashes in %s",
			       repository);
		    if (*p == '/')
			++slashes_skipped;
		    if (slashes_skipped < slashes_in_update_dir + 1)
			--p;
		    else
			break;
		}
		toplevel_repos = xmalloc (p - repository + 1);
		/* Note that we don't copy the trailing '/'.  */
		strncpy (toplevel_repos, repository, p - repository);
		toplevel_repos[p - repository] = '\0';
	    }
	}
    }

    send_repository (dir, repository, update_dir);
}

static int modules_count;
static int modules_allocated;
static char **modules_vector;

static void
handle_module_expansion (args, len)
    char *args;
    int len;
{
    if (modules_vector == NULL)
    {
	modules_allocated = 1; /* Small for testing */
	modules_vector = (char **) xmalloc
	  (modules_allocated * sizeof (modules_vector[0]));
    }
    else if (modules_count >= modules_allocated)
    {
	modules_allocated *= 2;
	modules_vector = (char **) xrealloc
	  ((char *) modules_vector,
	   modules_allocated * sizeof (modules_vector[0]));
    }
    modules_vector[modules_count] = xmalloc (strlen (args) + 1);
    strcpy (modules_vector[modules_count], args);
    ++modules_count;
}

void
client_expand_modules (argc, argv, local)
    int argc;
    char **argv;
    int local;
{
    int errs;
    int i;

    for (i = 0; i < argc; ++i)
	send_arg (argv[i]);
    send_a_repository ("", server_cvsroot, "");
    if (fprintf (to_server, "expand-modules\n") < 0)
	error (1, errno, "writing to server");
    errs = get_server_responses ();
    if (last_repos != NULL)
        free (last_repos);
    last_repos = NULL;
    if (last_update_dir != NULL)
        free (last_update_dir);
    last_update_dir = NULL;
    if (errs)
	error (errs, 0, "");
}

void
client_send_expansions (local)
     int local;
{
    int i;
    char *argv[1];
    for (i = 0; i < modules_count; ++i)
    {
	argv[0] = modules_vector[i];
	if (isfile (argv[0]))
	    send_files (1, argv, local, 0);
	else
	    send_file_names (1, argv);
    }
    send_a_repository ("", server_cvsroot, "");
}

void
client_nonexpanded_setup ()
{
    send_a_repository ("", server_cvsroot, "");
}

static void
handle_m (args, len)
    char *args;
    int len;
{
  fwrite (args, len, sizeof (*args), stdout);
  putc ('\n', stdout);
}

static void
handle_e (args, len)
    char *args;
    int len;
{
  fwrite (args, len, sizeof (*args), stderr);
  putc ('\n', stderr);
}

#endif /* CLIENT_SUPPORT */
#if defined(CLIENT_SUPPORT) || defined(SERVER_SUPPORT)

/* This table must be writeable if the server code is included.  */
struct response responses[] =
{
#ifdef CLIENT_SUPPORT
#define RSP_LINE(n, f, t, s) {n, f, t, s}
#else /* ! CLIENT_SUPPORT */
#define RSP_LINE(n, f, t, s) {n, s}
#endif /* CLIENT_SUPPORT */

    RSP_LINE("ok", handle_ok, response_type_ok, rs_essential),
    RSP_LINE("error", handle_error, response_type_error, rs_essential),
    RSP_LINE("Valid-requests", handle_valid_requests, response_type_normal,
       rs_essential),
    RSP_LINE("Checked-in", handle_checked_in, response_type_normal,
       rs_essential),
    RSP_LINE("New-entry", handle_new_entry, response_type_normal, rs_optional),
    RSP_LINE("Checksum", handle_checksum, response_type_normal, rs_optional),
    RSP_LINE("Copy-file", handle_copy_file, response_type_normal, rs_optional),
    RSP_LINE("Updated", handle_updated, response_type_normal, rs_essential),
    RSP_LINE("Merged", handle_merged, response_type_normal, rs_essential),
    RSP_LINE("Patched", handle_patched, response_type_normal, rs_optional),
    RSP_LINE("Removed", handle_removed, response_type_normal, rs_essential),
    RSP_LINE("Remove-entry", handle_remove_entry, response_type_normal,
       rs_optional),
    RSP_LINE("Set-static-directory", handle_set_static_directory,
       response_type_normal,
       rs_optional),
    RSP_LINE("Clear-static-directory", handle_clear_static_directory,
       response_type_normal,
       rs_optional),
    RSP_LINE("Set-sticky", handle_set_sticky, response_type_normal,
       rs_optional),
    RSP_LINE("Clear-sticky", handle_clear_sticky, response_type_normal,
       rs_optional),
    RSP_LINE("Set-checkin-prog", handle_set_checkin_prog, response_type_normal,
       rs_optional),
    RSP_LINE("Set-update-prog", handle_set_update_prog, response_type_normal,
       rs_optional),
    RSP_LINE("Module-expansion", handle_module_expansion, response_type_normal,
       rs_optional),
    RSP_LINE("M", handle_m, response_type_normal, rs_essential),
    RSP_LINE("E", handle_e, response_type_normal, rs_essential),
    /* Possibly should be response_type_error.  */
    RSP_LINE(NULL, NULL, response_type_normal, rs_essential)

#undef RSP_LINE
};

#endif /* CLIENT_SUPPORT or SERVER_SUPPORT */
#ifdef CLIENT_SUPPORT

/*
 * Get some server responses and process them.  Returns nonzero for
 * error, 0 for success.
 */
int
get_server_responses ()
{
    struct response *rs;
    do
    {
	char *cmd;
	int len;
	
	len = read_line (&cmd, 0);
	for (rs = responses; rs->name != NULL; ++rs)
	    if (strncmp (cmd, rs->name, strlen (rs->name)) == 0)
	    {
		int cmdlen = strlen (rs->name);
		if (cmd[cmdlen] == '\0')
		    ;
		else if (cmd[cmdlen] == ' ')
		    ++cmdlen;
		else
		    /*
		     * The first len characters match, but it's a different
		     * response.  e.g. the response is "oklahoma" but we
		     * matched "ok".
		     */
		    continue;
		(*rs->func) (cmd + cmdlen, len - cmdlen);
		break;
	    }
	if (rs->name == NULL)
	    /* It's OK to print just to the first '\0'.  */
	    error (0, 0,
		   "warning: unrecognized response `%s' from cvs server",
		   cmd);
	free (cmd);
    } while (rs->type == response_type_normal);
    return rs->type == response_type_error ? 1 : 0;
}

/* Get the responses and then close the connection.  */
int server_fd = -1;

int
get_responses_and_close ()
{
    int errs = get_server_responses ();

    do_deferred_progs ();

    if (client_prune_dirs)
	process_prune_candidates ();

#if defined(HAVE_KERBEROS) || defined(AUTH_CLIENT_SUPPORT)
    if (server_fd != -1)
    {
	if (shutdown (server_fd, 1) < 0)
	    error (1, errno, "shutting down connection to %s", server_host);
	/*
	 * In this case, both sides of the net connection will use the
         * same fd.
	 */
	if (fileno (from_server) != fileno (to_server))
	{
	    if (fclose (to_server) != 0)
		error (1, errno, "closing down connection to %s", server_host);
	}
    }
    else
#endif /* HAVE_KERBEROS || AUTH_CLIENT_SUPPORT */

#ifdef SHUTDOWN_SERVER
    SHUTDOWN_SERVER (fileno (to_server));
#else /* ! SHUTDOWN_SERVER */
    {

#ifdef START_RSH_WITH_POPEN_RW
	if (pclose (to_server) == EOF)
#else /* ! START_RSH_WITH_POPEN_RW */
	if (fclose (to_server) == EOF)
#endif /* START_RSH_WITH_POPEN_RW */
        {
          error (1, errno, "closing connection to %s", server_host);
        }
    }

    if (getc (from_server) != EOF)
	error (0, 0, "dying gasps from %s unexpected", server_host);
    else if (ferror (from_server))
	error (0, errno, "reading from %s", server_host);

    fclose (from_server);
#endif /* SHUTDOWN_SERVER */

#if ! RSH_NOT_TRANSPARENT
    if (rsh_pid != -1
	&& waitpid (rsh_pid, (int *) 0, 0) == -1)
      if (errno != ECHILD)
        error (1, errno, "waiting for process %d", rsh_pid);
#endif /* ! RSH_NOT_TRANSPARENT */

    return errs;
}
	
#ifndef RSH_NOT_TRANSPARENT
static void start_rsh_server PROTO((int *, int *));
#endif /* RSH_NOT_TRANSPARENT */

int
supported_request (name)
     char *name;
{
  struct request *rq;

  for (rq = requests; rq->name; rq++)
    if (!strcmp (rq->name, name))
      return rq->status == rq_supported;
  error (1, 0, "internal error: testing support for unknown option?");
}


#ifdef AUTH_CLIENT_SUPPORT
void
init_sockaddr (name, hostname, port)
     struct sockaddr_in *name;
     const char *hostname;
     unsigned short int port;
{
  struct hostent *hostinfo;
  
  name->sin_family = AF_INET;
  name->sin_port = htons (port);
  hostinfo = gethostbyname (hostname);
  if (hostinfo == NULL)
    {
      fprintf (stderr, "Unknown host %s.\n", hostname);
      exit (EXIT_FAILURE);
    }
  name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
}

void
connect_to_pserver (tofdp, fromfdp, log)
     int *tofdp, *fromfdp;
     char *log;
{
  int sock;
  int tofd, fromfd;
  struct hostent *host;
  struct sockaddr_in client_sai;

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock == -1)
    {
      fprintf (stderr, "socket() failed\n");
      exit (1);
    }
  init_sockaddr (&client_sai, server_host, CVS_AUTH_PORT);
  connect (sock, (struct sockaddr *) &client_sai, sizeof (client_sai));

  /* Run the authorization mini-protocol before anything else. */
  {
    int i;
    char ch, read_buf[PATH_MAX];
    char *begin      = "BEGIN AUTH REQUEST\n";
    char *repository = server_cvsroot;
    char *username   = server_user;
    char *password   = NULL;
    char *end        = "END AUTH REQUEST\n";

    /* Get the password, probably from ~/.cvspass. */
    password = get_cvs_password (server_user, server_host, server_cvsroot);

    /* Announce that we're starting the authorization protocol. */
    write (sock, begin, strlen (begin));

    /* Send the data the server needs. */
    write (sock, repository, strlen (repository));
    write (sock, "\n", 1);
    write (sock, username, strlen (username));
    write (sock, "\n", 1);
    write (sock, password, strlen (password));
    write (sock, "\n", 1);

    /* Announce that we're ending the authorization protocol. */
    write (sock, end, strlen (end));

    /* Paranoia. */
    memset (password, 0, strlen (password));

    /* Get ACK or NACK from the server. 
     * 
     * We could avoid this careful read-char loop by having the ACK
     * and NACK cookies be of the same length, so we'd simply read
     * that length and see what we got.  But then there'd be Yet
     * Another Protocol Requirement floating around, and someday
     * someone would make a change that breaks it and spend a hellish
     * day tracking it down.  Therefore, we use "\n" to mark off the
     * end of both ACK and NACK, and we read until "\n".
     */
    ch = 0;
    memset (read_buf, 0, PATH_MAX);
    for (i = 0; (i < (PATH_MAX - 1)) && (ch != '\n'); i++)
      {
        read (sock, &ch, 1);
        read_buf[i] = ch;
      }

    if (strcmp (read_buf, "I HATE YOU\n") == 0)
      {
        /* Authorization not granted. */
	if (shutdown (sock, 2) < 0)
	    error (1, errno, "shutdown() failed (server %s)", server_host);
        error (1, 0, 
               "authorization failed: server %s rejected access", 
               server_host);
      }
    else if (strcmp (read_buf, "I LOVE YOU\n") != 0)
      {
        /* Unrecognized response from server. */
	if (shutdown (sock, 2) < 0)
	    error (1, errno, "shutdown() failed (server %s)", server_host);
        error (1, 0, 
               "unrecognized auth response from %s: %s", 
               server_host, read_buf);
      }
    /* Else authorization granted, so we can go on... */
  }

  /* This was stolen straight from start_kerberos_server(). */
  {
    server_fd = sock;
    close_on_exec (server_fd);
    /*
     * If we do any filtering, TOFD and FROMFD will be
     * closed.  So make sure they're copies of SERVER_FD,
     * and not the same fd number.
     */
    if (log)
      {
        tofd = dup (sock);
        fromfd = dup (sock);
      }
    else
      tofd = fromfd = sock;
  }

  /* Hand them back to the caller. */
  *tofdp   = tofd;
  *fromfdp = fromfd;
}
#endif /* AUTH_CLIENT_SUPPORT */


#if HAVE_KERBEROS
void
start_kerberos_server (tofdp, fromfdp, log)
     int *tofdp, *fromfdp;
     char *log;
{
  int tofd, fromfd;

  struct hostent *hp;
  char *hname;
  const char *realm;
  const char *portenv;
  int port;
  struct sockaddr_in sin;
  int s;
  KTEXT_ST ticket;
  int status;
  
  /*
   * We look up the host to give a better error message if it
   * does not exist.  However, we then pass server_host to
   * krb_sendauth, rather than the canonical name, because
   * krb_sendauth is going to do its own canonicalization anyhow
   * and that lets us not worry about the static storage used by
   * gethostbyname.
   */
  hp = gethostbyname (server_host);
  if (hp == NULL)
    error (1, 0, "%s: unknown host", server_host);
  hname = xmalloc (strlen (hp->h_name) + 1);
  strcpy (hname, hp->h_name);
  
  realm = krb_realmofhost (hname);
  
  portenv = getenv ("CVS_CLIENT_PORT");
  if (portenv != NULL)
    {
      port = atoi (portenv);
      if (port <= 0)
        goto try_rsh_no_message;
      port = htons (port);
    }
  else
    {
      struct servent *sp;
      
      sp = getservbyname ("cvs", "tcp");
      if (sp == NULL)
        port = htons (CVS_PORT);
      else
        port = sp->s_port;
    }
  
  s = socket (AF_INET, SOCK_STREAM, 0);
  if (s < 0)
    error (1, errno, "socket");
  
  memset (&sin, 0, sizeof sin);
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = 0;
  
  if (bind (s, (struct sockaddr *) &sin, sizeof sin) < 0)
    error (1, errno, "bind");
  
  memcpy (&sin.sin_addr, hp->h_addr, hp->h_length);
  sin.sin_port = port;
  
  tofd = -1;
  if (connect (s, (struct sockaddr *) &sin, sizeof sin) < 0)
    {
      error (0, errno, "connect");
      close (s);
    }
  else
    {
      struct sockaddr_in laddr;
      int laddrlen;
      MSG_DAT msg_data;
      CREDENTIALS cred;
      Key_schedule sched;
      
      laddrlen = sizeof (laddr);
      if (getsockname (s, (struct sockaddr *) &laddr, &laddrlen) < 0)
        error (1, errno, "getsockname");
      
      /* We don't care about the checksum, and pass it as zero.  */
      status = krb_sendauth (KOPT_DO_MUTUAL, s, &ticket, "rcmd",
                             hname, realm, (unsigned long) 0, &msg_data,
                             &cred, sched, &laddr, &sin, "KCVSV1.0");
      if (status != KSUCCESS)
        {
          error (0, 0, "kerberos: %s", krb_get_err_text(status));
          close (s);
        }
      else
        {
          server_fd = s;
          close_on_exec (server_fd);
          /*
           * If we do any filtering, TOFD and FROMFD will be
           * closed.  So make sure they're copies of SERVER_FD,
           * and not the same fd number.
           */
          if (log)
            {
              tofd = dup (s);
              fromfd = dup (s);
            }
          else
            tofd = fromfd = s;
        }
    }
  
  if (tofd == -1)
    {
      error (0, 0, "trying to start server using rsh");
    try_rsh_no_message:
      server_fd = -1;
#if ! RSH_NOT_TRANSPARENT
      start_rsh_server (&tofd, &fromfd);
#else /* RSH_NOT_TRANSPARENT */
#if defined (START_SERVER)
      START_SERVER (&tofd, &fromfd, getcaller (),
                    server_user, server_host, server_cvsroot);
#endif /* defined (START_SERVER) */
#endif /* ! RSH_NOT_TRANSPARENT */
    }
  free (hname);

  /* Give caller the values it wants. */
  *tofdp   = tofd;
  *fromfdp = fromfd;
}

#endif /* HAVE_KERBEROS */

/* Contact the server.  */
void
start_server ()
{
    int tofd, fromfd;
    char *log = getenv ("CVS_CLIENT_LOG");

#if HAVE_KERBEROS
    start_kerberos_server (&tofd, &fromfd, log);

#else /* ! HAVE_KERBEROS */

#ifdef AUTH_CLIENT_SUPPORT
    if (use_authenticating_server)
      {
        connect_to_pserver (&tofd, &fromfd, log);
      }
    else
#endif /* AUTH_CLIENT_SUPPORT */
      {
#if ! RSH_NOT_TRANSPARENT
        start_rsh_server (&tofd, &fromfd);
#else /* RSH_NOT_TRANSPARENT */

#if defined(START_SERVER)
        START_SERVER (&tofd, &fromfd, getcaller (),
                      server_user, server_host, server_cvsroot);
#endif /* defined(START_SERVER) */
#endif /* ! RSH_NOT_TRANSPARENT */
      }
#endif /* HAVE_KERBEROS */

    /* todo: some OS's don't need these calls... */
    close_on_exec (tofd);
    close_on_exec (fromfd);

    if (log)
    {
	int len = strlen (log);
	char *buf = xmalloc (5 + len);
	char *p;
	static char *teeprog[3] = { "tee" };

	teeprog[1] = buf;
	strcpy (buf, log);
	p = buf + len;

	strcpy (p, ".in");
	tofd = filter_stream_through_program (tofd, 0, teeprog, 0);

	strcpy (p, ".out");
	fromfd = filter_stream_through_program (fromfd, 1, teeprog, 0);

	free (buf);
    }

    /* These will use binary mode on systems which have it.  */
    to_server = fdopen (tofd, FOPEN_BINARY_WRITE);
    if (to_server == NULL)
	error (1, errno, "cannot fdopen %d for write", tofd);
    from_server = fdopen (fromfd, FOPEN_BINARY_READ);
    if (from_server == NULL)
	error (1, errno, "cannot fdopen %d for read", fromfd);

    /* Clear static variables.  */
    if (toplevel_repos != NULL)
        free (toplevel_repos);
    toplevel_repos = NULL;
    if (last_dirname != NULL)
        free (last_dirname);
    last_dirname = NULL;
    if (last_repos != NULL)
        free (last_repos);
    last_repos = NULL;
    if (last_update_dir != NULL)
        free (last_update_dir);
    last_update_dir = NULL;
    stored_checksum_valid = 0;

    if (fprintf (to_server, "Root %s\n", server_cvsroot) < 0)
	error (1, errno, "writing to server");
    {
	struct response *rs;
	if (fprintf (to_server, "Valid-responses") < 0)
	    error (1, errno, "writing to server");
	for (rs = responses; rs->name != NULL; ++rs)
	{
	    if (fprintf (to_server, " %s", rs->name) < 0)
		error (1, errno, "writing to server");
	}
	if (fprintf (to_server, "\n") < 0)
	    error (1, errno, "writing to server");
    }
    if (fprintf (to_server, "valid-requests\n") < 0)
	error (1, errno, "writing to server");
    if (get_server_responses ())
	exit (1);

    /*
     * Now handle global options.
     *
     * -H, -f, -d, -e should be handled OK locally.
     *
     * -b we ignore (treating it as a server installation issue).
     * FIXME: should be an error message.
     *
     * -v we print local version info; FIXME: Add a protocol request to get
     * the version from the server so we can print that too.
     *
     * -l -t -r -w -q -n and -Q need to go to the server.
     */

    {
	int have_global = supported_request ("Global_option");

	if (noexec)
	{
	    if (have_global)
	    {
		if (fprintf (to_server, "Global_option -n\n") < 0)
		    error (1, errno, "writing to server");
	    }
	    else
		error (1, 0,
		       "This server does not support the global -n option.");
	}
	if (quiet)
	{
	    if (have_global)
	    {
		if (fprintf (to_server, "Global_option -q\n") < 0)
		    error (1, errno, "writing to server");
	    }
	    else
		error (1, 0,
		       "This server does not support the global -q option.");
	}
	if (really_quiet)
	{
	    if (have_global)
	    {
		if (fprintf (to_server, "Global_option -Q\n") < 0)
		    error (1, errno, "writing to server");
	    }
	    else
		error (1, 0,
		       "This server does not support the global -Q option.");
	}
	if (!cvswrite)
	{
	    if (have_global)
	    {
		if (fprintf (to_server, "Global_option -r\n") < 0)
		    error (1, errno, "writing to server");
	    }
	    else
		error (1, 0,
		       "This server does not support the global -r option.");
	}
	if (trace)
	{
	    if (have_global)
	    {
		if (fprintf (to_server, "Global_option -t\n") < 0)
		    error (1, errno, "writing to server");
	    }
	    else
		error (1, 0,
		       "This server does not support the global -t option.");
	}
	if (logoff)
	{
	    if (have_global)
	    {
		if (fprintf (to_server, "Global_option -l\n") < 0)
		    error (1, errno, "writing to server");
	    }
	    else
		error (1, 0,
		       "This server does not support the global -l option.");
	}
    }
    if (gzip_level)
      {
	if (supported_request ("gzip-file-contents"))
	  {
	    if (fprintf (to_server, "gzip-file-contents %d\n", gzip_level) < 0)
	      error (1, 0, "writing to server");
	  }
	else
	  {
	    fprintf (stderr, "server doesn't support gzip-file-contents\n");
	    gzip_level = 0;
	  }
      }
}

#ifndef RSH_NOT_TRANSPARENT
/* Contact the server by starting it with rsh.  */

/* Right now, we have two different definitions for this function,
   depending on whether we start the rsh server using popenRW or not.
   This isn't ideal, and the best thing would probably be to change
   the OS/2 port to be more like the regular Unix client (i.e., by
   implementing piped_child)... but I'm doing something else at the
   moment, and wish to make only one change at a time.  -Karl */

#ifdef START_RSH_WITH_POPEN_RW

static void
start_rsh_server (tofdp, fromfdp)
     int *tofdp, *fromfdp;
{
  int pipes[2];
  
  /* If you're working through firewalls, you can set the
     CVS_RSH environment variable to a script which uses rsh to
     invoke another rsh on a proxy machine.  */
  char *cvs_rsh = getenv ("CVS_RSH");
  char *cvs_server = getenv ("CVS_SERVER");
  char command[PATH_MAX];
  int i = 0;
  /* This needs to fit "rsh", "-b", "-l", "USER", "host",
	 "cmd (w/ args)", and NULL.  We leave some room to grow. */
  char *rsh_argv[10];
  
  if (!cvs_rsh)
    cvs_rsh = "rsh";
  if (!cvs_server)
    cvs_server = "cvs";
  
  /* If you are running a very old (Nov 3, 1994, before 1.5)
   * version of the server, you need to make sure that your .bashrc
   * on the server machine does not set CVSROOT to something
   * containing a colon (or better yet, upgrade the server).  */
  
  /* The command line starts out with rsh. */
  rsh_argv[i++] = cvs_rsh;
  
  /* "-b" for binary, under OS/2. */
  rsh_argv[i++] = "-b";

  /* Then we strcat more things on the end one by one. */
  if (server_user != NULL)
    {
      rsh_argv[i++] = "-l";
      rsh_argv[i++] = server_user;
    }
  
  rsh_argv[i++] = server_host;
  rsh_argv[i++] = cvs_server;
  rsh_argv[i++] = "server";

  /* Mark the end of the arg list. */
  rsh_argv[i]   = (char *) NULL;

  if (trace)
    {
      fprintf (stderr, " -> Starting server: ");
      fprintf (stderr, "%s", command);
      putc ('\n', stderr);
    }
  
  /* Do the deed. */
  rsh_pid = popenRW (rsh_argv, pipes);
  if (rsh_pid < 0)
    error (1, errno, "cannot start server via rsh");

  /* Give caller the file descriptors. */
  *tofdp   = pipes[0];
  *fromfdp = pipes[1];
}

#else /* ! START_RSH_WITH_POPEN_RW */

static void
start_rsh_server (tofdp, fromfdp)
     int *tofdp;
     int *fromfdp;
{
    /* If you're working through firewalls, you can set the
       CVS_RSH environment variable to a script which uses rsh to
       invoke another rsh on a proxy machine.  */
    char *cvs_rsh = getenv ("CVS_RSH");
    char *cvs_server = getenv ("CVS_SERVER");
    char *command;

    if (!cvs_rsh)
	cvs_rsh = "rsh";
    if (!cvs_server)
	cvs_server = "cvs";

    /* Pass the command to rsh as a single string.  This shouldn't
       affect most rsh servers at all, and will pacify some buggy
       versions of rsh that grab switches out of the middle of the
       command (they're calling the GNU getopt routines incorrectly).  */
    command = xmalloc (strlen (cvs_server)
		       + strlen (server_cvsroot)
		       + 50);

    /* If you are running a very old (Nov 3, 1994, before 1.5)
     * version of the server, you need to make sure that your .bashrc
     * on the server machine does not set CVSROOT to something
     * containing a colon (or better yet, upgrade the server).  */
    sprintf (command, "%s server", cvs_server);

    {
        char *argv[10];
	char **p = argv;

	*p++ = cvs_rsh;
	*p++ = server_host;

	/* If the login names differ between client and server
	 * pass it on to rsh.
	 */
	if (server_user != NULL)
	{
	    *p++ = "-l";
	    *p++ = server_user;
	}

	*p++ = command;
	*p++ = NULL;

	if (trace)
        {
	    int i;

            fprintf (stderr, " -> Starting server: ");
	    for (i = 0; argv[i]; i++)
	        fprintf (stderr, "%s ", argv[i]);
	    putc ('\n', stderr);
	}
	rsh_pid = piped_child (argv, tofdp, fromfdp);

	if (rsh_pid < 0)
	    error (1, errno, "cannot start server via rsh");
    }
}

#endif /* START_RSH_WITH_POPEN_RW */
#endif /* ! RSH_NOT_TRANSPARENT */



/* Send an argument STRING.  */
void
send_arg (string)
    char *string;
{
    char *p = string;
    if (fprintf (to_server, "Argument ") < 0)
	error (1, errno, "writing to server");
    while (*p)
    {
	if (*p == '\n')
	{
	    if (fprintf (to_server, "\nArgumentx ") < 0)
		error (1, errno, "writing to server");
	}
	else if (putc (*p, to_server) == EOF)
	    error (1, errno, "writing to server");
	++p;
    }
    if (putc ('\n', to_server) == EOF)
	error (1, errno, "writing to server");
}

static void send_modified PROTO ((char *, char *, Vers_TS *));

static void
send_modified (file, short_pathname, vers)
    char *file;
    char *short_pathname;
    Vers_TS *vers;
{
    /* File was modified, send it.  */
    struct stat sb;
    int fd;
    char *buf;
    char *mode_string;
    int bufsize;
    int bin = 0;

    /* Don't think we can assume fstat exists.  */
    if (stat (file, &sb) < 0)
	error (1, errno, "reading %s", short_pathname);

    mode_string = mode_to_string (sb.st_mode);

    /* Beware: on systems using CRLF line termination conventions,
       the read and write functions will convert CRLF to LF, so the
       number of characters read is not the same as sb.st_size.  Text
       files should always be transmitted using the LF convention, so
       we don't want to disable this conversion.  */
    bufsize = sb.st_size;
    buf = xmalloc (bufsize);

    /* Is the file marked as containing binary data by the "-kb" flag?
       If so, make sure to open it in binary mode: */

    bin = !(strcmp (vers->options, "-kb"));
    fd = open (file, O_RDONLY | (bin ? OPEN_BINARY : 0));

    if (fd < 0)
	error (1, errno, "reading %s", short_pathname);

    if (gzip_level && sb.st_size > 100)
    {
	int nread, newsize = 0, gzip_status;
	pid_t gzip_pid;
	char *bufp = buf;
	int readsize = 8192;
#ifdef LINES_CRLF_TERMINATED
	char tempfile[L_tmpnam];
	int converting;
#endif /* LINES_CRLF_TERMINATED */

#ifdef LINES_CRLF_TERMINATED
	/* Assume everything in a "cvs import" is text.  */
	if (vers == NULL)
	    converting = 1;
	else
            /* Otherwise, we convert things unless they're binary. */
	    converting = (! bin);

	if (converting)
	{
	    /* gzip reads and writes files without munging CRLF
	       sequences, as it should, but files should be
	       transmitted in LF form.  Convert CRLF to LF before
	       gzipping, on systems where this is necessary.

	       If Windows NT supported fork, we could do this by
	       pushing another filter on in front of gzip.  But it
	       doesn't.  I'd have to write a trivial little program to
	       do the conversion and have CVS spawn it off.  But
	       little executables like that always get lost.

	       Alternatively, this cruft could go away if we switched
	       to a gzip library instead of a subprocess; then we
	       could tell gzip to open the file with CRLF translation
	       enabled.  */
	    if (close (fd) < 0)
		error (0, errno, "warning: can't close %s", short_pathname);

	    tmpnam (tempfile);
	    convert_file (file, O_RDONLY,
			  tempfile,
			  O_WRONLY | O_CREAT | O_TRUNC | OPEN_BINARY);

	    /* This OPEN_BINARY doesn't make any difference, I think, because
	       gzip will deal with the inherited handle as it pleases.  But I
	       do remember something obscure in the manuals about propagating
	       the translation mode to created processes via environment
	       variables, ick.  */
	    fd = open (tempfile, O_RDONLY | OPEN_BINARY);
	    if (fd < 0)
		error (1, errno, "reading %s", short_pathname);
	}
#endif /* LINES_CRLF_TERMINATED */

	fd = filter_through_gzip (fd, 1, gzip_level, &gzip_pid);
	while (1)
	{
	    if ((bufp - buf) + readsize >= bufsize)
	    {
		/*
		 * We need to expand the buffer if gzip ends up expanding
		 * the file.
		 */
		newsize = bufp - buf;
		while (newsize + readsize >= bufsize)
		  bufsize *= 2;
		buf = xrealloc (buf, bufsize);
		bufp = buf + newsize;
	    }
	    nread = read (fd, bufp, readsize);
	    if (nread < 0)
		error (1, errno, "reading from gzip pipe");
	    else if (nread == 0)
		/* eof */
		break;
	    bufp += nread;
	}
	newsize = bufp - buf;
	if (close (fd) < 0)
	    error (0, errno, "warning: can't close %s", short_pathname);

	if (waitpid (gzip_pid, &gzip_status, 0) != gzip_pid)
	    error (1, errno, "waiting for gzip proc %d", gzip_pid);
	else if (gzip_status != 0)
	    error (1, errno, "gzip exited %d", gzip_status);

#if LINES_CRLF_TERMINATED
	if (converting)
	{
	    if (unlink (tempfile) < 0)
		error (0, errno,
		       "warning: can't remove temp file %s", tempfile);
	}
#endif /* LINES_CRLF_TERMINATED */

	fprintf (to_server, "Modified %s\n%s\nz%lu\n", file, mode_string,
		 (unsigned long) newsize);
	fwrite (buf, newsize, 1, to_server);
	if (feof (to_server) || ferror (to_server))
	    error (1, errno, "writing to server");
    }
    else
    {
    	int newsize;

        {
	    char *bufp = buf;
	    int len;

	    while ((len = read (fd, bufp, (buf + sb.st_size) - bufp)) > 0)
	        bufp += len;

	    if (len < 0)
	        error (1, errno, "reading %s", short_pathname);

	    newsize = bufp - buf;
	}
	if (close (fd) < 0)
	    error (0, errno, "warning: can't close %s", short_pathname);

	if (fprintf (to_server, "Modified %s\n%s\n%lu\n", file,
		     mode_string, (unsigned long) newsize) < 0)
	    error (1, errno, "writing to server");

	/*
	 * Note that this only ends with a newline if the file ended with
	 * one.
	 */
	if (newsize > 0)
	    if (fwrite (buf, newsize, 1, to_server) != 1)
		error (1, errno, "writing to server");
    }
    free (buf);
    free (mode_string);
}

/* Deal with one file.  */
static int
send_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    Vers_TS *vers;
    int update_dir_len = strlen (update_dir);
    char *short_pathname = xmalloc (update_dir_len + strlen (file) + 40);
    strcpy (short_pathname, update_dir);
    if (update_dir[0] != '\0')
	strcat (short_pathname, "/");
    strcat (short_pathname, file);

    send_a_repository ("", repository, update_dir);

    vers = Version_TS ((char *)NULL, (char *)NULL, (char *)NULL,
		       (char *)NULL,
		       file, 0, 0, entries, (List *)NULL);

    if (vers->vn_user != NULL)
    {
	/* The Entries request.  */
	/* Not sure about whether this deals with -k and stuff right.  */
	if (fprintf (to_server, "Entry /%s/%s/%s%s/%s/", file, vers->vn_user,
		     vers->ts_conflict == NULL ? "" : "+",
		     (vers->ts_conflict == NULL ? ""
		      : (vers->ts_user != NULL &&
			 strcmp (vers->ts_conflict, vers->ts_user) == 0
			 ? "="
			 : "modified")),
		     vers->options) < 0)
	    error (1, errno, "writing to server");
	if (vers->entdata != NULL && vers->entdata->tag)
	{
	    if (fprintf (to_server, "T%s", vers->entdata->tag) < 0)
		error (1, errno, "writing to server");
	}
	else if (vers->entdata != NULL && vers->entdata->date)
	    if (fprintf (to_server, "D%s", vers->entdata->date) < 0)
		error (1, errno, "writing to server");
	if (fprintf (to_server, "\n") < 0)
	    error (1, errno, "writing to server");
    }

    if (vers->ts_user == NULL)
    {
	/*
	 * Do we want to print "file was lost" like normal CVS?
	 * Would it always be appropriate?
	 */
	/* File no longer exists.  */
	if (!use_unchanged)
	{
	    /* if the server is old, use the old request... */
	    if (fprintf (to_server, "Lost %s\n", file) < 0)
		error (1, errno, "writing to server");
	    /*
	     * Otherwise, don't do anything for missing files,
	     * they just happen.
	     */
	}
    }
    else if (vers->ts_rcs == NULL
	     || strcmp (vers->ts_user, vers->ts_rcs) != 0)
    {
	send_modified (file, short_pathname, vers);
    }
    else
    {
	/* Only use this request if the server supports it... */
	if (use_unchanged)
	    if (fprintf (to_server, "Unchanged %s\n", file) < 0)
		error (1, errno, "writing to server");
    }

    /* if this directory has an ignore list, add this file to it */
    if (ignlist)
    {
	Node *p;

	p = getnode ();
	p->type = FILES;
	p->key = xstrdup (file);
	(void) addnode (ignlist, p);
    }

    freevers_ts (&vers);
    free (short_pathname);
    return 0;
}

/*
 * send_dirent_proc () is called back by the recursion processor before a
 * sub-directory is processed for update.
 * A return code of 0 indicates the directory should be
 * processed by the recursion code.  A return of non-zero indicates the
 * recursion code should skip this directory.
 *
 */
static Dtype
send_dirent_proc (dir, repository, update_dir)
    char *dir;
    char *repository;
    char *update_dir;
{
    int dir_exists;
    char *cvsadm_repos_name;

    /*
     * If the directory does not exist yet (e.g. "cvs update -d
     * foo"), no need to send any files from it.
     */
    dir_exists = isdir (dir);

    if (ignore_directory (update_dir))
    {
	/* print the warm fuzzy message */
	if (!quiet)
	    error (0, 0, "Ignoring %s", update_dir);
        return (R_SKIP_ALL);
    }

    /* initialize the ignore list for this directory */
    ignlist = getlist ();

    /*
     * If there is an empty directory (e.g. we are doing `cvs add' on a
     * newly-created directory), the server still needs to know about it.
     */

    cvsadm_repos_name = xmalloc (strlen (dir) + sizeof (CVSADM_REP) + 80);
    sprintf (cvsadm_repos_name, "%s/%s", dir, CVSADM_REP);
    if (dir_exists && isreadable (cvsadm_repos_name))
    {
	/*
	 * Get the repository from a CVS/Repository file whenever possible.
	 * The repository variable is wrong if the names in the local
	 * directory don't match the names in the repository.
	 */
	char *repos = Name_Repository (dir, update_dir);
	send_a_repository (dir, repos, update_dir);
	free (repos);
    }
    else
	send_a_repository (dir, repository, update_dir);
    free (cvsadm_repos_name);

    return (dir_exists ? R_PROCESS : R_SKIP_ALL);
}

/*
 * Send each option in a string to the server, one by one.
 * This assumes that the options are single characters.  For
 * more complex parsing, do it yourself.
 */

void
send_option_string (string)
    char *string;
{
    char *p;
    char it[3];

    for (p = string; p[0]; p++) {
	if (p[0] == ' ')
	    continue;
	if (p[0] == '-')
	    continue;
	it[0] = '-';
	it[1] = p[0];
	it[2] = '\0';
	send_arg (it);
    }
}


/* Send the names of all the argument files to the server.  */

void
send_file_names (argc, argv)
    int argc;
    char **argv;
{
    int i;
    char *p;
    char *q;
    int level;
    int max_level;

    /* Send Max-dotdot if needed.  */
    max_level = 0;
    for (i = 0; i < argc; ++i)
    {
	p = argv[i];
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
    }
    if (max_level > 0)
    {
	if (supported_request ("Max-dotdot"))
	{
	    if (fprintf (to_server, "Max-dotdot %d\n", max_level) < 0)
		error (1, errno, "writing to server");
	}
	else
	    /*
	     * "leading .." is not strictly correct, as this also includes
	     * cases like "foo/../..".  But trying to explain that in the
	     * error message would probably just confuse users.
	     */
	    error (1, 0,
		   "leading .. not supported by old (pre-Max-dotdot) servers");
    }

    for (i = 0; i < argc; ++i)
	send_arg (argv[i]);
}


/*
 * Send Repository, Modified and Entry.  argc and argv contain only
 * the files to operate on (or empty for everything), not options.
 * local is nonzero if we should not recurse (-l option).  Also sends
 * Argument lines for argc and argv, so should be called after options
 * are sent.
 */
void
send_files (argc, argv, local, aflag)
    int argc;
    char **argv;
    int local;
    int aflag;
{
    int err;

    send_file_names (argc, argv);

    /*
     * aflag controls whether the tag/date is copied into the vers_ts.
     * But we don't actually use it, so I don't think it matters what we pass
     * for aflag here.
     */
    err = start_recursion
	(send_fileproc, update_filesdone_proc,
	 send_dirent_proc, (DIRLEAVEPROC)NULL,
	 argc, argv, local, W_LOCAL, aflag, 0, (char *)NULL, 0, 0);
    if (err)
	exit (1);
    if (toplevel_repos == NULL)
	/*
	 * This happens if we are not processing any files,
	 * or for checkouts in directories without any existing stuff
	 * checked out.  The following assignment is correct for the
	 * latter case; I don't think toplevel_repos matters for the
	 * former.
	 */
	toplevel_repos = xstrdup (server_cvsroot);
    send_repository ("", toplevel_repos, ".");
}

void
client_import_setup (repository)
    char *repository;
{
    if (toplevel_repos == NULL)		/* should always be true */
        send_a_repository ("", repository, "");
}

/*
 * Process the argument import file.
 */
int
client_process_import_file (message, vfile, vtag, targc, targv, repository)
    char *message;
    char *vfile;
    char *vtag;
    int targc;
    char *targv[];
    char *repository;
{
    char *short_pathname;
    int first_time;

    /* FIXME: I think this is always false now that we call
       client_import_setup at the start.  */

    first_time = toplevel_repos == NULL;

    if (first_time)
	send_a_repository ("", repository, "");

    if (strncmp (repository, toplevel_repos, strlen (toplevel_repos)) != 0)
	error (1, 0,
	       "internal error: pathname `%s' doesn't specify file in `%s'",
	       repository, toplevel_repos);
    short_pathname = repository + strlen (toplevel_repos) + 1;

    if (!first_time)
    {
	send_a_repository ("", repository, short_pathname);
    }
    send_modified (vfile, short_pathname, NULL);
    return 0;
}

void
client_import_done ()
{
    if (toplevel_repos == NULL)
	/*
	 * This happens if we are not processing any files,
	 * or for checkouts in directories without any existing stuff
	 * checked out.  The following assignment is correct for the
	 * latter case; I don't think toplevel_repos matters for the
	 * former.
	 */
        /* FIXME: "can't happen" now that we call client_import_setup
	   at the beginning.  */
	toplevel_repos = xstrdup (server_cvsroot);
    send_repository ("", toplevel_repos, ".");
}

/*
 * Send an option with an argument, dealing correctly with newlines in
 * the argument.  If ARG is NULL, forget the whole thing.
 */
void
option_with_arg (option, arg)
    char *option;
    char *arg;
{
    if (arg == NULL)
	return;
    if (fprintf (to_server, "Argument %s\n", option) < 0)
	error (1, errno, "writing to server");
    send_arg (arg);
}

/*
 * Send a date to the server.  This will passed a string which is the
 * result of Make_Date, and looks like YY.MM.DD.HH.MM.SS, where all
 * the letters are single digits.  The time will be GMT.  getdate on
 * the server can't parse that, so we turn it back into something
 * which it can parse.
 */

void
client_senddate (date)
    const char *date;
{
    int year, month, day, hour, minute, second;
    char buf[100];

    if (sscanf (date, DATEFORM, &year, &month, &day, &hour, &minute, &second)
	!= 6)
    {
        error (1, 0, "diff_client_senddate: sscanf failed on date");
    }

#ifndef HAVE_RCS5
    /* We need to fix the timezone in this case; see Make_Date.  */
    abort ();
#endif /* HAVE_RCS5 */

    sprintf (buf, "%d/%d/%d %d:%d:%d GMT", month, day, year,
	     hour, minute, second);
    option_with_arg ("-D", buf);
}

int
client_commit (argc, argv)
    int argc;
    char **argv;
{
    parse_cvsroot ();

    return commit (argc, argv);
}

int
client_update (argc, argv)
    int argc;
    char **argv;
{
    parse_cvsroot ();

    return update (argc, argv);
}

int
client_checkout (argc, argv)
    int argc;
    char **argv;
{
    parse_cvsroot ();
    
    return checkout (argc, argv);
}

int
client_diff (argc, argv)
    int argc;
    char **argv;
{
    parse_cvsroot ();

    return diff (argc, argv);	/* Call real code */
}

int
client_status (argc, argv)
    int argc;
    char **argv;
{
    parse_cvsroot ();
    return status (argc, argv);
}

int
client_log (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return cvslog (argc, argv);	/* Call real code */
}

int
client_add (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return add (argc, argv);	/* Call real code */
}

int
client_remove (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return cvsremove (argc, argv);	/* Call real code */
}

int
client_rdiff (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return patch (argc, argv);	/* Call real code */
}

int
client_tag (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return tag (argc, argv);	/* Call real code */
}

int
client_rtag (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return rtag (argc, argv);	/* Call real code */
}

int
client_import (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return import (argc, argv);	/* Call real code */
}

int
client_admin (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return admin (argc, argv);	/* Call real code */
}

int
client_export (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return checkout (argc, argv);	/* Call real code */
}

int
client_history (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return history (argc, argv);	/* Call real code */
}

int
client_release (argc, argv)
    int argc;
    char **argv;
{
    
    parse_cvsroot ();
    
    return release (argc, argv);	/* Call real code */
}

#endif /* CLIENT_SUPPORT */
