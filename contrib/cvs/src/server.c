/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include <assert.h>
#include "cvs.h"
#include "watch.h"
#include "edit.h"
#include "fileattr.h"
#include "getline.h"
#include "buffer.h"

#ifdef SERVER_SUPPORT

#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif

#if defined (AUTH_SERVER_SUPPORT) || defined (HAVE_KERBEROS) || defined (HAVE_GSSAPI)
#include <sys/socket.h>
#endif

#ifdef HAVE_KERBEROS
#  include <netinet/in.h>
#  include <krb.h>
#  ifndef HAVE_KRB_GET_ERR_TEXT
#    define krb_get_err_text(status) krb_err_txt[status]
#  endif

/* Information we need if we are going to use Kerberos encryption.  */
static C_Block kblock;
static Key_schedule sched;

#endif

#ifdef HAVE_GSSAPI

#include <netdb.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>

/* We use Kerberos 5 routines to map the GSSAPI credential to a user
   name.  */
#include <krb5.h>

/* We need this to wrap data.  */
static gss_ctx_id_t gcontext;

static void gserver_authenticate_connection PROTO((void));

/* Whether we are already wrapping GSSAPI communication.  */
static int cvs_gssapi_wrapping;

#  ifdef ENCRYPTION
/* Whether to encrypt GSSAPI communication.  We use a global variable
   like this because we use the same buffer type (gssapi_wrap) to
   handle both authentication and encryption, and we don't want
   multiple instances of that buffer in the communication stream.  */
int cvs_gssapi_encrypt;
#  endif

#endif

/* for select */
#include <sys/types.h>
#ifdef HAVE_SYS_BSDTYPES_H
#include <sys/bsdtypes.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK O_NDELAY
#endif

/* EWOULDBLOCK is not defined by POSIX, but some BSD systems will
   return it, rather than EAGAIN, for nonblocking writes.  */
#ifdef EWOULDBLOCK
#define blocking_error(err) ((err) == EWOULDBLOCK || (err) == EAGAIN)
#else
#define blocking_error(err) ((err) == EAGAIN)
#endif

#ifdef AUTH_SERVER_SUPPORT
#ifdef HAVE_GETSPNAM
#include <shadow.h>
#endif
#endif /* AUTH_SERVER_SUPPORT */

/* For initgroups().  */
#if HAVE_INITGROUPS
#include <grp.h>
#endif /* HAVE_INITGROUPS */

#ifdef AUTH_SERVER_SUPPORT

/* The cvs username sent by the client, which might or might not be
   the same as the system username the server eventually switches to
   run as.  CVS_Username gets set iff password authentication is
   successful. */
char *CVS_Username = NULL;

/* Used to check that same repos is transmitted in pserver auth and in
   later CVS protocol.  Exported because root.c also uses. */
char *Pserver_Repos = NULL;

/* Should we check for system usernames/passwords?  Can be changed by
   CVSROOT/config.  */
int system_auth = 1;

#endif /* AUTH_SERVER_SUPPORT */


/* While processing requests, this buffer accumulates data to be sent to
   the client, and then once we are in do_cvs_command, we use it
   for all the data to be sent.  */
static struct buffer *buf_to_net;

/* This buffer is used to read input from the client.  */
static struct buffer *buf_from_net;

/*
 * This is where we stash stuff we are going to use.  Format string
 * which expects a single directory within it, starting with a slash.
 */
static char *server_temp_dir;

/* This is the original value of server_temp_dir, before any possible
   changes inserted by serve_max_dotdot.  */
static char *orig_server_temp_dir;

/* Nonzero if we should keep the temp directory around after we exit.  */
static int dont_delete_temp;

static void server_write_entries PROTO((void));

/* All server communication goes through buffer structures.  Most of
   the buffers are built on top of a file descriptor.  This structure
   is used as the closure field in a buffer.  */

struct fd_buffer
{
    /* The file descriptor.  */
    int fd;
    /* Nonzero if the file descriptor is in blocking mode.  */
    int blocking;
};

static struct buffer *fd_buffer_initialize
  PROTO ((int, int, void (*) (struct buffer *)));
static int fd_buffer_input PROTO((void *, char *, int, int, int *));
static int fd_buffer_output PROTO((void *, const char *, int, int *));
static int fd_buffer_flush PROTO((void *));
static int fd_buffer_block PROTO((void *, int));

/* Initialize a buffer built on a file descriptor.  FD is the file
   descriptor.  INPUT is nonzero if this is for input, zero if this is
   for output.  MEMORY is the function to call when a memory error
   occurs.  */

static struct buffer *
fd_buffer_initialize (fd, input, memory)
     int fd;
     int input;
     void (*memory) PROTO((struct buffer *));
{
    struct fd_buffer *n;

    n = (struct fd_buffer *) xmalloc (sizeof *n);
    n->fd = fd;
    n->blocking = 1;
    return buf_initialize (input ? fd_buffer_input : NULL,
			   input ? NULL : fd_buffer_output,
			   input ? NULL : fd_buffer_flush,
			   fd_buffer_block,
			   (int (*) PROTO((void *))) NULL,
			   memory,
			   n);
}

/* The buffer input function for a buffer built on a file descriptor.  */

static int
fd_buffer_input (closure, data, need, size, got)
     void *closure;
     char *data;
     int need;
     int size;
     int *got;
{
    struct fd_buffer *fd = (struct fd_buffer *) closure;
    int nbytes;

    if (! fd->blocking)
	nbytes = read (fd->fd, data, size);
    else
    {
	/* This case is not efficient.  Fortunately, I don't think it
           ever actually happens.  */
	nbytes = read (fd->fd, data, need == 0 ? 1 : need);
    }

    if (nbytes > 0)
    {
	*got = nbytes;
	return 0;
    }

    *got = 0;

    if (nbytes == 0)
    {
	/* End of file.  This assumes that we are using POSIX or BSD
           style nonblocking I/O.  On System V we will get a zero
           return if there is no data, even when not at EOF.  */
	return -1;
    }

    /* Some error occurred.  */

    if (blocking_error (errno))
    {
	/* Everything's fine, we just didn't get any data.  */
	return 0;
    }

    return errno;
}

/* The buffer output function for a buffer built on a file descriptor.  */

static int
fd_buffer_output (closure, data, have, wrote)
     void *closure;
     const char *data;
     int have;
     int *wrote;
{
    struct fd_buffer *fd = (struct fd_buffer *) closure;

    *wrote = 0;

    while (have > 0)
    {
	int nbytes;

	nbytes = write (fd->fd, data, have);

	if (nbytes <= 0)
	{
	    if (! fd->blocking
		&& (nbytes == 0 || blocking_error (errno)))
	    {
		/* A nonblocking write failed to write any data.  Just
                   return.  */
		return 0;
	    }

	    /* Some sort of error occurred.  */

	    if (nbytes == 0)
	        return EIO;

	    return errno;
	}

	*wrote += nbytes;
	data += nbytes;
	have -= nbytes;
    }

    return 0;
}

/* The buffer flush function for a buffer built on a file descriptor.  */

/*ARGSUSED*/
static int
fd_buffer_flush (closure)
     void *closure;
{
    /* Nothing to do.  File descriptors are always flushed.  */
    return 0;
}

/* The buffer block function for a buffer built on a file descriptor.  */

static int
fd_buffer_block (closure, block)
     void *closure;
     int block;
{
    struct fd_buffer *fd = (struct fd_buffer *) closure;
    int flags;

    flags = fcntl (fd->fd, F_GETFL, 0);
    if (flags < 0)
	return errno;

    if (block)
	flags &= ~O_NONBLOCK;
    else
	flags |= O_NONBLOCK;

    if (fcntl (fd->fd, F_SETFL, flags) < 0)
        return errno;

    fd->blocking = block;

    return 0;
}

/*
 * Make directory DIR, including all intermediate directories if necessary.
 * Returns 0 for success or errno code.
 */
static int mkdir_p PROTO((char *));

static int
mkdir_p (dir)
     char *dir;
{
    char *p;
    char *q = malloc (strlen (dir) + 1);
    int retval;

    if (q == NULL)
	return ENOMEM;

    retval = 0;

    /*
     * Skip over leading slash if present.  We won't bother to try to
     * make '/'.
     */
    p = dir + 1;
    while (1)
    {
	while (*p != '/' && *p != '\0')
	    ++p;
	if (*p == '/')
	{
	    strncpy (q, dir, p - dir);
	    q[p - dir] = '\0';
	    if (q[p - dir - 1] != '/'  &&  CVS_MKDIR (q, 0777) < 0)
	    {
		int saved_errno = errno;

		if (saved_errno != EEXIST
		    && (saved_errno != EACCES || !isdir (q)))
		{
		    retval = saved_errno;
		    goto done;
		}
	    }
	    ++p;
	}
	else
	{
	    if (CVS_MKDIR (dir, 0777) < 0)
		retval = errno;
	    goto done;
	}
    }
  done:
    free (q);
    return retval;
}

/*
 * Print the error response for error code STATUS.  The caller is
 * reponsible for making sure we get back to the command loop without
 * any further output occuring.
 * Must be called only in contexts where it is OK to send output.
 */
static void
print_error (status)
    int status;
{
    char *msg;
    buf_output0 (buf_to_net, "error  ");
    msg = strerror (status);
    if (msg)
	buf_output0 (buf_to_net, msg);
    buf_append_char (buf_to_net, '\n');

    buf_flush (buf_to_net, 0);
}

static int pending_error;
/*
 * Malloc'd text for pending error.  Each line must start with "E ".  The
 * last line should not end with a newline.
 */
static char *pending_error_text;

/* If an error is pending, print it and return 1.  If not, return 0.
   Must be called only in contexts where it is OK to send output.  */
static int
print_pending_error ()
{
    if (pending_error_text)
    {
	buf_output0 (buf_to_net, pending_error_text);
	buf_append_char (buf_to_net, '\n');
	if (pending_error)
	    print_error (pending_error);
	else
	    buf_output0 (buf_to_net, "error  \n");

	buf_flush (buf_to_net, 0);

	pending_error = 0;
	free (pending_error_text);
	pending_error_text = NULL;
	return 1;
    }
    else if (pending_error)
    {
	print_error (pending_error);
	pending_error = 0;
	return 1;
    }
    else
	return 0;
}

/* Is an error pending?  */
#define error_pending() (pending_error || pending_error_text)

static int alloc_pending PROTO ((size_t size));

/* Allocate SIZE bytes for pending_error_text and return nonzero
   if we could do it.  */
static int
alloc_pending (size)
    size_t size;
{
    if (error_pending ())
	/* Probably alloc_pending callers will have already checked for
	   this case.  But we might as well handle it if they don't, I
	   guess.  */
	return 0;
    pending_error_text = malloc (size);
    if (pending_error_text == NULL)
    {
	pending_error = ENOMEM;
	return 0;
    }
    return 1;
}

static void serve_is_modified PROTO ((char *));

static int supported_response PROTO ((char *));

static int
supported_response (name)
     char *name;
{
    struct response *rs;

    for (rs = responses; rs->name != NULL; ++rs)
	if (strcmp (rs->name, name) == 0)
	    return rs->status == rs_supported;
    error (1, 0, "internal error: testing support for unknown response?");
    /* NOTREACHED */
    return 0;
}

static void
serve_valid_responses (arg)
     char *arg;
{
    char *p = arg;
    char *q;
    struct response *rs;
    do
    {
	q = strchr (p, ' ');
	if (q != NULL)
	    *q++ = '\0';
	for (rs = responses; rs->name != NULL; ++rs)
	{
	    if (strcmp (rs->name, p) == 0)
		break;
	}
	if (rs->name == NULL)
	    /*
	     * It is a response we have never heard of (and thus never
	     * will want to use).  So don't worry about it.
	     */
	    ;
	else
	    rs->status = rs_supported;
	p = q;
    } while (q != NULL);
    for (rs = responses; rs->name != NULL; ++rs)
    {
	if (rs->status == rs_essential)
	{
	    buf_output0 (buf_to_net, "E response `");
	    buf_output0 (buf_to_net, rs->name);
	    buf_output0 (buf_to_net, "' not supported by client\nerror  \n");

	    /* FIXME: This call to buf_flush could conceivably
	       cause deadlock, as noted in server_cleanup.  */
	    buf_flush (buf_to_net, 1);

	    /* I'm doing this manually rather than via error_exit ()
	       because I'm not sure whether we want to call server_cleanup.
	       Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	    /* Hook for OS-specific behavior, for example socket subsystems on
	       NT and OS2 or dealing with windows and arguments on Mac.  */
	    SYSTEM_CLEANUP ();
#endif

	    exit (EXIT_FAILURE);
	}
	else if (rs->status == rs_optional)
	    rs->status = rs_not_supported;
    }
}

static void
serve_root (arg)
    char *arg;
{
    char *env;
    char *path;
    int save_errno;
    
    if (error_pending()) return;

    if (!isabsolute (arg))
    {
	if (alloc_pending (80 + strlen (arg)))
	    sprintf (pending_error_text,
		     "E Root %s must be an absolute pathname", arg);
	return;
    }

    /* Sending "Root" twice is illegal.  It would also be nice to
       check for the other case, in which there is no Root request
       prior to a request which requires one.

       The other way to handle a duplicate Root requests would be as a
       request to clear out all state and start over as if it was a
       new connection.  Doing this would cause interoperability
       headaches, so it should be a different request, if there is
       any reason why such a feature is needed.  */
    if (CVSroot_directory != NULL)
    {
	if (alloc_pending (80 + strlen (arg)))
	    sprintf (pending_error_text,
		     "E Protocol error: Duplicate Root request, for %s", arg);
	return;
    }

    set_local_cvsroot (arg);

    /* For pserver, this will already have happened, and the call will do
       nothing.  But for rsh, we need to do it now.  */
    parse_config (CVSroot_directory);

    path = xmalloc (strlen (CVSroot_directory)
		    + sizeof (CVSROOTADM)
		    + sizeof (CVSROOTADM_HISTORY)
		    + 10);
    (void) sprintf (path, "%s/%s", CVSroot_directory, CVSROOTADM);
    if (!isaccessible (path, R_OK | X_OK))
    {
	save_errno = errno;
	pending_error_text = malloc (80 + strlen (path));
	if (pending_error_text != NULL)
	    sprintf (pending_error_text, "E Cannot access %s", path);
	pending_error = save_errno;
    }
    (void) strcat (path, "/");
    (void) strcat (path, CVSROOTADM_HISTORY);
    if (readonlyfs == 0 && isfile (path) && !isaccessible (path, R_OK | W_OK))
    {
	save_errno = errno;
	pending_error_text = malloc (80 + strlen (path));
	if (pending_error_text != NULL)
	    sprintf (pending_error_text, "E \
Sorry, you don't have read/write access to the history file %s", path);
	pending_error = save_errno;
    }
    free (path);

#ifdef HAVE_PUTENV
    env = malloc (strlen (CVSROOT_ENV) + strlen (CVSroot_directory) + 1 + 1);
    if (env == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    (void) sprintf (env, "%s=%s", CVSROOT_ENV, CVSroot_directory);
    (void) putenv (env);
    /* do not free env, as putenv has control of it */
#endif
    parseopts(CVSroot_directory);
}

static int max_dotdot_limit = 0;

/* Is this pathname OK to recurse into when we are running as the server?
   If not, call error() with a fatal error.  */
void
server_pathname_check (path)
    char *path;
{
    /* An absolute pathname is almost surely a path on the *client* machine,
       and is unlikely to do us any good here.  It also is probably capable
       of being a security hole in the anonymous readonly case.  */
    if (isabsolute (path))
	error (1, 0, "absolute pathname `%s' illegal for server", path);
    if (pathname_levels (path) > max_dotdot_limit)
    {
	/* Similar to the isabsolute case in security implications.  */
	error (0, 0, "protocol error: `%s' contains more leading ..", path);
	error (1, 0, "than the %d which Max-dotdot specified",
	       max_dotdot_limit);
    }
}

/*
 * Add as many directories to the temp directory as the client tells us it
 * will use "..", so we never try to access something outside the temp
 * directory via "..".
 */
static void
serve_max_dotdot (arg)
    char *arg;
{
    int lim = atoi (arg);
    int i;
    char *p;

    if (lim < 0)
	return;
    p = malloc (strlen (server_temp_dir) + 2 * lim + 10);
    if (p == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    strcpy (p, server_temp_dir);
    for (i = 0; i < lim; ++i)
	strcat (p, "/d");
    if (server_temp_dir != orig_server_temp_dir)
	free (server_temp_dir);
    server_temp_dir = p;
    max_dotdot_limit = lim;
}

static char *dir_name;

static void
dirswitch (dir, repos)
    char *dir;
    char *repos;
{
    int status;
    FILE *f;
    char *b;
    size_t dir_len;

    server_write_entries ();

    if (error_pending()) return;

    if (dir_name != NULL)
	free (dir_name);

    dir_len = strlen (dir);

    /* Check for a trailing '/'.  This is not ISDIRSEP because \ in the
       protocol is an ordinary character, not a directory separator (of
       course, it is perhaps unwise to use it in directory names, but that
       is another issue).  */
    if (dir_len > 0
	&& dir[dir_len - 1] == '/')
    {
	if (alloc_pending (80 + dir_len))
	    sprintf (pending_error_text,
		     "E protocol error: invalid directory syntax in %s", dir);
	return;
    }

    dir_name = malloc (strlen (server_temp_dir) + dir_len + 40);
    if (dir_name == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    
    strcpy (dir_name, server_temp_dir);
    strcat (dir_name, "/");
    strcat (dir_name, dir);

    status = mkdir_p (dir_name);	
    if (status != 0
	&& status != EEXIST)
    {
	pending_error = status;
	if (alloc_pending (80 + strlen (dir_name)))
	    sprintf (pending_error_text, "E cannot mkdir %s", dir_name);
	return;
    }

    /* Note that this call to Subdir_Register will be a noop if the parent
       directory does not yet exist (for example, if the client sends
       "Directory foo" followed by "Directory .", then the subdirectory does
       not get registered, but if the client sends "Directory ." followed
       by "Directory foo", then the subdirectory does get registered.
       This seems pretty fishy, but maybe it is the way it needs to work.  */
    b = strrchr (dir_name, '/');
    *b = '\0';
    Subdir_Register ((List *) NULL, dir_name, b + 1);
    *b = '/';

    if ( CVS_CHDIR (dir_name) < 0)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (dir_name)))
	    sprintf (pending_error_text, "E cannot change to %s", dir_name);
	return;
    }
    /*
     * This is pretty much like calling Create_Admin, but Create_Admin doesn't
     * report errors in the right way for us.
     */
    if (CVS_MKDIR (CVSADM, 0777) < 0)
    {
	if (errno == EEXIST)
	    /* Don't create the files again.  */
	    return;
	pending_error = errno;
	return;
    }
    f = CVS_FOPEN (CVSADM_REP, "w");
    if (f == NULL)
    {
	pending_error = errno;
	return;
    }
    if (fprintf (f, "%s", repos) < 0)
    {
	pending_error = errno;
	fclose (f);
	return;
    }
    /* Non-remote CVS handles a module representing the entire tree
       (e.g., an entry like ``world -a .'') by putting /. at the end
       of the Repository file, so we do the same.  */
    if (strcmp (dir, ".") == 0
	&& CVSroot_directory != NULL
	&& strcmp (CVSroot_directory, repos) == 0)
    {
        if (fprintf (f, "/.") < 0)
	{
	    pending_error = errno;
	    fclose (f);
	    return;
	}
    }
    if (fprintf (f, "\n") < 0)
    {
	pending_error = errno;
	fclose (f);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	return;
    }
    /* We open in append mode because we don't want to clobber an
       existing Entries file.  */
    f = CVS_FOPEN (CVSADM_ENT, "a");
    if (f == NULL)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_ENT)))
	    sprintf (pending_error_text, "E cannot open %s", CVSADM_ENT);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_ENT)))
	    sprintf (pending_error_text, "E cannot close %s", CVSADM_ENT);
	return;
    }
}

static void
serve_repository (arg)
    char *arg;
{
    pending_error_text = malloc (80);
    if (pending_error_text == NULL)
	pending_error = ENOMEM;
    else
	strcpy (pending_error_text,
		"E Repository request is obsolete; aborted");
    return;
}

static void
serve_directory (arg)
    char *arg;
{
    int status;
    char *repos;

    status = buf_read_line (buf_from_net, &repos, (int *) NULL);
    if (status == 0)
    {
	/* I think isabsolute (repos) should always be true, and that
	   any RELATIVE_REPOS stuff should only be in CVS/Repository
	   files, not the protocol (for compatibility), but I'm putting
	   in the in isabsolute check just in case.  */
	if (isabsolute (repos)
	    && strncmp (CVSroot_directory,
			repos,
			strlen (CVSroot_directory)) != 0)
	{
	    if (alloc_pending (strlen (CVSroot_directory)
			       + strlen (repos)
			       + 80))
		sprintf (pending_error_text, "\
E protocol error: directory '%s' not within root '%s'",
			 repos, CVSroot_directory);
	    return;
	}

	dirswitch (arg, repos);
	free (repos);
    }
    else if (status == -2)
    {
        pending_error = ENOMEM;
    }
    else
    {
	pending_error_text = malloc (80 + strlen (arg));
	if (pending_error_text == NULL)
	{
	    pending_error = ENOMEM;
	}
	else if (status == -1)
	{
	    sprintf (pending_error_text,
		     "E end of file reading mode for %s", arg);
	}
	else
	{
	    sprintf (pending_error_text,
		     "E error reading mode for %s", arg);
	    pending_error = status;
	}
    }
}

static void
serve_static_directory (arg)
    char *arg;
{
    FILE *f;

    if (error_pending ()) return;

    f = CVS_FOPEN (CVSADM_ENTSTAT, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_ENTSTAT)))
	    sprintf (pending_error_text, "E cannot open %s", CVSADM_ENTSTAT);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_ENTSTAT)))
	    sprintf (pending_error_text, "E cannot close %s", CVSADM_ENTSTAT);
	return;
    }
}

static void
serve_sticky (arg)
    char *arg;
{
    FILE *f;

    if (error_pending ()) return;

    f = CVS_FOPEN (CVSADM_TAG, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_TAG)))
	    sprintf (pending_error_text, "E cannot open %s", CVSADM_TAG);
	return;
    }
    if (fprintf (f, "%s\n", arg) < 0)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_TAG)))
	    sprintf (pending_error_text, "E cannot write to %s", CVSADM_TAG);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_TAG)))
	    sprintf (pending_error_text, "E cannot close %s", CVSADM_TAG);
	return;
    }
}

/*
 * Read SIZE bytes from buf_from_net, write them to FILE.
 *
 * Currently this isn't really used for receiving parts of a file --
 * the file is still sent over in one chunk.  But if/when we get
 * spiffy in-process gzip support working, perhaps the compressed
 * pieces could be sent over as they're ready, if the network is fast
 * enough.  Or something.
 */
static void
receive_partial_file (size, file)
     int size;
     int file;
{
    while (size > 0)
    {
	int status, nread;
	char *data;

	status = buf_read_data (buf_from_net, size, &data, &nread);
	if (status != 0)
	{
	    if (status == -2)
		pending_error = ENOMEM;
	    else
	    {
		pending_error_text = malloc (80);
		if (pending_error_text == NULL)
		    pending_error = ENOMEM;
		else if (status == -1)
		{
		    sprintf (pending_error_text,
			     "E premature end of file from client");
		    pending_error = 0;
		}
		else
		{
		    sprintf (pending_error_text,
			     "E error reading from client");
		    pending_error = status;
		}
	    }
	    return;
	}

	size -= nread;

	while (nread > 0)
	{
	    int nwrote;

	    nwrote = write (file, data, nread);
	    if (nwrote < 0)
	    {
		pending_error_text = malloc (40);
		if (pending_error_text != NULL)
		    sprintf (pending_error_text, "E unable to write");
		pending_error = errno;

		/* Read and discard the file data.  */
		while (size > 0)
		{
		    int status, nread;
		    char *data;

		    status = buf_read_data (buf_from_net, size, &data, &nread);
		    if (status != 0)
			return;
		    size -= nread;
		}

		return;
	    }
	    nread -= nwrote;
	    data += nwrote;
	}
    }
}

/* Receive SIZE bytes, write to filename FILE.  */
static void
receive_file (size, file, gzipped)
     int size;
     char *file;
     int gzipped;
{
    int fd;
    char *arg = file;
    pid_t gzip_pid = 0;
    int gzip_status;

    /* Write the file.  */
    fd = CVS_OPEN (arg, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
    {
	pending_error_text = malloc (40 + strlen (arg));
	if (pending_error_text)
	    sprintf (pending_error_text, "E cannot open %s", arg);
	pending_error = errno;
	return;
    }

    /*
     * FIXME: This doesn't do anything reasonable with gunzip's stderr, which
     * means that if gunzip writes to stderr, it will cause all manner of
     * protocol violations.
     */
    if (gzipped)
	fd = filter_through_gunzip (fd, 0, &gzip_pid);

    receive_partial_file (size, fd);

    if (pending_error_text)
    {
	char *p = realloc (pending_error_text,
			   strlen (pending_error_text) + strlen (arg) + 30);
	if (p)
	{
	    pending_error_text = p;
	    sprintf (p + strlen (p), ", file %s", arg);
	}
	/* else original string is supposed to be unchanged */
    }

    if (close (fd) < 0 && !error_pending ())
    {
	pending_error_text = malloc (40 + strlen (arg));
	if (pending_error_text)
	    sprintf (pending_error_text, "E cannot close %s", arg);
	pending_error = errno;
	if (gzip_pid)
	    waitpid (gzip_pid, (int *) 0, 0);
	return;
    }

    if (gzip_pid)
    {
	if (waitpid (gzip_pid, &gzip_status, 0) != gzip_pid)
	    error (1, errno, "waiting for gunzip process %ld",
		   (long) gzip_pid);
	else if (gzip_status != 0)
	    error (1, 0, "gunzip exited %d", gzip_status);
    }
}

/* Kopt for the next file sent in Modified or Is-modified.  */
static char *kopt;

static void serve_modified PROTO ((char *));

static void
serve_modified (arg)
     char *arg;
{
    int size, status;
    char *size_text;
    char *mode_text;

    int gzipped = 0;

    /*
     * This used to return immediately if error_pending () was true.
     * However, that fails, because it causes each line of the file to
     * be echoed back to the client as an unrecognized command.  The
     * client isn't reading from the socket, so eventually both
     * processes block trying to write to the other.  Now, we try to
     * read the file if we can.
     */

    status = buf_read_line (buf_from_net, &mode_text, (int *) NULL);
    if (status != 0)
    {
        if (status == -2)
	    pending_error = ENOMEM;
	else
	{
	    pending_error_text = malloc (80 + strlen (arg));
	    if (pending_error_text == NULL)
		pending_error = ENOMEM;
	    else
	    {
		if (status == -1)
		    sprintf (pending_error_text,
			     "E end of file reading mode for %s", arg);
		else
		{
		    sprintf (pending_error_text,
			     "E error reading mode for %s", arg);
		    pending_error = status;
		}
	    }
	}
	return;
    }

    status = buf_read_line (buf_from_net, &size_text, (int *) NULL);
    if (status != 0)
    {
	if (status == -2)
	    pending_error = ENOMEM;
	else
	{
	    pending_error_text = malloc (80 + strlen (arg));
	    if (pending_error_text == NULL)
		pending_error = ENOMEM;
	    else
	    {
		if (status == -1)
		    sprintf (pending_error_text,
			     "E end of file reading size for %s", arg);
		else
		{
		    sprintf (pending_error_text,
			     "E error reading size for %s", arg);
		    pending_error = errno;
		}
	    }
	}
	return;
    }
    if (size_text[0] == 'z')
    {
	gzipped = 1;
	size = atoi (size_text + 1);
    }
    else
	size = atoi (size_text);
    free (size_text);

    if (error_pending ())
    {
        /* Now that we know the size, read and discard the file data.  */
	while (size > 0)
	{
	    int status, nread;
	    char *data;

	    status = buf_read_data (buf_from_net, size, &data, &nread);
	    if (status != 0)
		return;
	    size -= nread;
	}
	return;
    }

    if (size >= 0)
    {
	receive_file (size, arg, gzipped);
	if (error_pending ()) return;
    }

    {
	int status = change_mode (arg, mode_text);
	free (mode_text);
	if (status)
	{
	    pending_error_text = malloc (40 + strlen (arg));
	    if (pending_error_text)
		sprintf (pending_error_text,
			 "E cannot change mode for %s", arg);
	    pending_error = status;
	    return;
	}
    }

    /* Make sure that the Entries indicate the right kopt.  We probably
       could do this even in the non-kopt case and, I think, save a stat()
       call in time_stamp_server.  But for conservatism I'm leaving the
       non-kopt case alone.  */
    if (kopt != NULL)
	serve_is_modified (arg);
}


static void
serve_enable_unchanged (arg)
     char *arg;
{
}

struct an_entry {
    struct an_entry *next;
    char *entry;
};

static struct an_entry *entries;

static void serve_unchanged PROTO ((char *));

static void
serve_unchanged (arg)
    char *arg;
{
    struct an_entry *p;
    char *name;
    char *cp;
    char *timefield;

    if (error_pending ())
	return;

    /* Rewrite entries file to have `=' in timestamp field.  */
    for (p = entries; p != NULL; p = p->next)
    {
	name = p->entry + 1;
	cp = strchr (name, '/');
	if (cp != NULL
	    && strlen (arg) == cp - name
	    && strncmp (arg, name, cp - name) == 0)
	{
	    timefield = strchr (cp + 1, '/') + 1;
	    if (*timefield != '=')
	    {
		cp = timefield + strlen (timefield);
		cp[1] = '\0';
		while (cp > timefield)
		{
		    *cp = cp[-1];
		    --cp;
		}
		*timefield = '=';
	    }
	    break;
	}
    }
}

static void
serve_is_modified (arg)
    char *arg;
{
    struct an_entry *p;
    char *name;
    char *cp;
    char *timefield;
    /* Have we found this file in "entries" yet.  */
    int found;

    if (error_pending ())
	return;

    /* Rewrite entries file to have `M' in timestamp field.  */
    found = 0;
    for (p = entries; p != NULL; p = p->next)
    {
	name = p->entry + 1;
	cp = strchr (name, '/');
	if (cp != NULL
	    && strlen (arg) == cp - name
	    && strncmp (arg, name, cp - name) == 0)
	{
	    timefield = strchr (cp + 1, '/') + 1;
	    if (!(timefield[0] == 'M' && timefield[1] == '/'))
	    {
		cp = timefield + strlen (timefield);
		cp[1] = '\0';
		while (cp > timefield)
		{
		    *cp = cp[-1];
		    --cp;
		}
		*timefield = 'M';
	    }
	    if (kopt != NULL)
	    {
		if (alloc_pending (strlen (name) + 80))
		    sprintf (pending_error_text,
			     "E protocol error: both Kopt and Entry for %s",
			     arg);
		free (kopt);
		kopt = NULL;
		return;
	    }
	    found = 1;
	    break;
	}
    }
    if (!found)
    {
	/* We got Is-modified but no Entry.  Add a dummy entry.
	   The "D" timestamp is what makes it a dummy.  */
	p = (struct an_entry *) malloc (sizeof (struct an_entry));
	if (p == NULL)
	{
	    pending_error = ENOMEM;
	    return;
	}
	p->entry = malloc (strlen (arg) + 80);
	if (p->entry == NULL)
	{
	    pending_error = ENOMEM;
	    free (p);
	    return;
	}
	strcpy (p->entry, "/");
	strcat (p->entry, arg);
	strcat (p->entry, "//D/");
	if (kopt != NULL)
	{
	    strcat (p->entry, kopt);
	    free (kopt);
	    kopt = NULL;
	}
	strcat (p->entry, "/");
	p->next = entries;
	entries = p;
    }
}

static void serve_entry PROTO ((char *));

static void
serve_entry (arg)
     char *arg;
{
    struct an_entry *p;
    char *cp;
    if (error_pending()) return;
    p = (struct an_entry *) malloc (sizeof (struct an_entry));
    if (p == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    /* Leave space for serve_unchanged to write '=' if it wants.  */
    cp = malloc (strlen (arg) + 2);
    if (cp == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    strcpy (cp, arg);
    p->next = entries;
    p->entry = cp;
    entries = p;
}

static void serve_kopt PROTO ((char *));

static void
serve_kopt (arg)
     char *arg;
{
    if (error_pending ())
	return;

    if (kopt != NULL)
    {
	if (alloc_pending (80 + strlen (arg)))
	    sprintf (pending_error_text,
		     "E protocol error: duplicate Kopt request: %s", arg);
	return;
    }

    /* Do some sanity checks.  In particular, that it is not too long.
       This lets the rest of the code not worry so much about buffer
       overrun attacks.  Probably should call RCS_check_kflag here,
       but that would mean changing RCS_check_kflag to handle errors
       other than via exit(), fprintf(), and such.  */
    if (strlen (arg) > 10)
    {
	if (alloc_pending (80 + strlen (arg)))
	    sprintf (pending_error_text,
		     "E protocol error: invalid Kopt request: %s", arg);
	return;
    }

    kopt = malloc (strlen (arg) + 1);
    if (kopt == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    strcpy (kopt, arg);
}

static void
server_write_entries ()
{
    FILE *f;
    struct an_entry *p;
    struct an_entry *q;

    if (entries == NULL)
	return;

    f = NULL;
    /* Note that we free all the entries regardless of errors.  */
    if (!error_pending ())
    {
	/* We open in append mode because we don't want to clobber an
           existing Entries file.  If we are checking out a module
           which explicitly lists more than one file in a particular
           directory, then we will wind up calling
           server_write_entries for each such file.  */
	f = CVS_FOPEN (CVSADM_ENT, "a");
	if (f == NULL)
	{
	    pending_error = errno;
	    if (alloc_pending (80 + strlen (CVSADM_ENT)))
		sprintf (pending_error_text, "E cannot open %s", CVSADM_ENT);
	}
    }
    for (p = entries; p != NULL;)
    {
	if (!error_pending ())
	{
	    if (fprintf (f, "%s\n", p->entry) < 0)
	    {
		pending_error = errno;
		if (alloc_pending (80 + strlen(CVSADM_ENT)))
		    sprintf (pending_error_text,
			     "E cannot write to %s", CVSADM_ENT);
	    }
	}
	free (p->entry);
	q = p->next;
	free (p);
	p = q;
    }
    entries = NULL;
    if (f != NULL && fclose (f) == EOF && !error_pending ())
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_ENT)))
	    sprintf (pending_error_text, "E cannot close %s", CVSADM_ENT);
    }
}

struct notify_note {
    /* Directory in which this notification happens.  malloc'd*/
    char *dir;

    /* malloc'd.  */
    char *filename;

    /* The following three all in one malloc'd block, pointed to by TYPE.
       Each '\0' terminated.  */
    /* "E" or "U".  */
    char *type;
    /* time+host+dir */
    char *val;
    char *watches;

    struct notify_note *next;
};

static struct notify_note *notify_list;
/* Used while building list, to point to the last node that already exists.  */
static struct notify_note *last_node;

static void serve_notify PROTO ((char *));

static void
serve_notify (arg)
    char *arg;
{
    struct notify_note *new;
    char *data;
    int status;

    if (error_pending ()) return;

    new = (struct notify_note *) malloc (sizeof (struct notify_note));
    if (new == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    if (dir_name == NULL)
	goto error;
    new->dir = malloc (strlen (dir_name) + 1);
    if (new->dir == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    strcpy (new->dir, dir_name);
    new->filename = malloc (strlen (arg) + 1);
    if (new->filename == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    strcpy (new->filename, arg);

    status = buf_read_line (buf_from_net, &data, (int *) NULL);
    if (status != 0)
    {
	if (status == -2)
	    pending_error = ENOMEM;
	else
	{
	    pending_error_text = malloc (80 + strlen (arg));
	    if (pending_error_text == NULL)
		pending_error = ENOMEM;
	    else
	    {
		if (status == -1)
		    sprintf (pending_error_text,
			     "E end of file reading notification for %s", arg);
		else
		{
		    sprintf (pending_error_text,
			     "E error reading notification for %s", arg);
		    pending_error = status;
		}
	    }
	}
    }
    else
    {
	char *cp;

	new->type = data;
	if (data[1] != '\t')
	    goto error;
	data[1] = '\0';
	cp = data + 2;
	new->val = cp;
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    goto error;
	*cp++ = '+';
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    goto error;
	*cp++ = '+';
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    goto error;
	*cp++ = '\0';
	new->watches = cp;
	/* If there is another tab, ignore everything after it,
	   for future expansion.  */
	cp = strchr (cp, '\t');
	if (cp != NULL)
	{
	    *cp = '\0';
	}

	new->next = NULL;

	if (last_node == NULL)
	{
	    notify_list = new;
	}
	else
	    last_node->next = new;
	last_node = new;
    }
    return;
  error:
    pending_error_text = malloc (40);
    if (pending_error_text)
	strcpy (pending_error_text,
		"E Protocol error; misformed Notify request");
    pending_error = 0;
    return;
}

/* Process all the Notify requests that we have stored up.  Returns 0
   if successful, if not prints error message (via error()) and
   returns negative value.  */
static int
server_notify ()
{
    struct notify_note *p;
    char *repos;

    while (notify_list != NULL)
    {
	if ( CVS_CHDIR (notify_list->dir) < 0)
	{
	    error (0, errno, "cannot change to %s", notify_list->dir);
	    return -1;
	}
	repos = Name_Repository (NULL, NULL);

	lock_dir_for_write (repos);

	fileattr_startdir (repos);

	notify_do (*notify_list->type, notify_list->filename, getcaller(),
		   notify_list->val, notify_list->watches, repos);

	buf_output0 (buf_to_net, "Notified ");
	{
	    char *dir = notify_list->dir + strlen (server_temp_dir) + 1;
	    if (dir[0] == '\0')
	        buf_append_char (buf_to_net, '.');
	    else
	        buf_output0 (buf_to_net, dir);
	    buf_append_char (buf_to_net, '/');
	    buf_append_char (buf_to_net, '\n');
	}
	buf_output0 (buf_to_net, repos);
	buf_append_char (buf_to_net, '/');
	buf_output0 (buf_to_net, notify_list->filename);
	buf_append_char (buf_to_net, '\n');

	p = notify_list->next;
	free (notify_list->filename);
	free (notify_list->dir);
	free (notify_list->type);
	free (notify_list);
	notify_list = p;

	fileattr_write ();
	fileattr_free ();

	Lock_Cleanup ();
    }

    /* The code used to call fflush (stdout) here, but that is no
       longer necessary.  The data is now buffered in buf_to_net,
       which will be flushed by the caller, do_cvs_command.  */

    return 0;
}

static int argument_count;
static char **argument_vector;
static int argument_vector_size;

static void
serve_argument (arg)
     char *arg;
{
    char *p;
    
    if (error_pending()) return;
    
    if (argument_vector_size <= argument_count)
    {
	argument_vector_size *= 2;
	argument_vector =
	    (char **) realloc ((char *)argument_vector,
			       argument_vector_size * sizeof (char *));
	if (argument_vector == NULL)
	{
	    pending_error = ENOMEM;
	    return;
	}
    }
    p = malloc (strlen (arg) + 1);
    if (p == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    strcpy (p, arg);
    argument_vector[argument_count++] = p;
}

static void
serve_argumentx (arg)
     char *arg;
{
    char *p;
    
    if (error_pending()) return;
    
    p = argument_vector[argument_count - 1];
    p = realloc (p, strlen (p) + 1 + strlen (arg) + 1);
    if (p == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    strcat (p, "\n");
    strcat (p, arg);
    argument_vector[argument_count - 1] = p;
}

static void
serve_global_option (arg)
    char *arg;
{
    if (arg[0] != '-' || arg[1] == '\0' || arg[2] != '\0')
    {
    error_return:
	if (alloc_pending (strlen (arg) + 80))
	    sprintf (pending_error_text,
		     "E Protocol error: bad global option %s",
		     arg);
	return;
    }
    switch (arg[1])
    {
	case 'n':
	    noexec = 1;
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'r':
	    cvswrite = 0;
	    break;
	case 'Q':
	    really_quiet = 1;
	    break;
	case 'l':
	    logoff = 1;
	    break;
	case 't':
	    trace = 1;
	    break;
	default:
	    goto error_return;
    }
}

static void
serve_set (arg)
    char *arg;
{
    /* FIXME: This sends errors immediately (I think); they should be
       put into pending_error.  */
    variable_set (arg);
}

#ifdef ENCRYPTION

#ifdef HAVE_KERBEROS

static void
serve_kerberos_encrypt (arg)
     char *arg;
{
    /* All future communication with the client will be encrypted.  */

    buf_to_net = krb_encrypt_buffer_initialize (buf_to_net, 0, sched,
						kblock,
						buf_to_net->memory_error);
    buf_from_net = krb_encrypt_buffer_initialize (buf_from_net, 1, sched,
						  kblock,
						  buf_from_net->memory_error);
}

#endif /* HAVE_KERBEROS */

#ifdef HAVE_GSSAPI

static void
serve_gssapi_encrypt (arg)
     char *arg;
{
    if (cvs_gssapi_wrapping)
    {
	/* We're already using a gssapi_wrap buffer for stream
           authentication.  Flush everything we've output so far, and
           turn on encryption for future data.  On the input side, we
           should only have unwrapped as far as the Gssapi-encrypt
           command, so future unwrapping will become encrypted.  */
	buf_flush (buf_to_net, 1);
	cvs_gssapi_encrypt = 1;
	return;
    }

    /* All future communication with the client will be encrypted.  */

    cvs_gssapi_encrypt = 1;

    buf_to_net = cvs_gssapi_wrap_buffer_initialize (buf_to_net, 0,
						    gcontext,
						    buf_to_net->memory_error);
    buf_from_net = cvs_gssapi_wrap_buffer_initialize (buf_from_net, 1,
						      gcontext,
						      buf_from_net->memory_error);

    cvs_gssapi_wrapping = 1;
}

#endif /* HAVE_GSSAPI */

#endif /* ENCRYPTION */

#ifdef HAVE_GSSAPI

static void
serve_gssapi_authenticate (arg)
     char *arg;
{
    if (cvs_gssapi_wrapping)
    {
	/* We're already using a gssapi_wrap buffer for encryption.
           That includes authentication, so we don't have to do
           anything further.  */
	return;
    }

    buf_to_net = cvs_gssapi_wrap_buffer_initialize (buf_to_net, 0,
						    gcontext,
						    buf_to_net->memory_error);
    buf_from_net = cvs_gssapi_wrap_buffer_initialize (buf_from_net, 1,
						      gcontext,
						      buf_from_net->memory_error);

    cvs_gssapi_wrapping = 1;
}

#endif /* HAVE_GSSAPI */

#ifdef SERVER_FLOWCONTROL
/* The maximum we'll queue to the remote client before blocking.  */
# ifndef SERVER_HI_WATER
#  define SERVER_HI_WATER (2 * 1024 * 1024)
# endif /* SERVER_HI_WATER */
/* When the buffer drops to this, we restart the child */
# ifndef SERVER_LO_WATER
#  define SERVER_LO_WATER (1 * 1024 * 1024)
# endif /* SERVER_LO_WATER */

static int set_nonblock_fd PROTO((int));

/*
 * Set buffer BUF to non-blocking I/O.  Returns 0 for success or errno
 * code.
 */

static int
set_nonblock_fd (fd)
     int fd;
{
    int flags;

    flags = fcntl (fd, F_GETFL, 0);
    if (flags < 0)
	return errno;
    if (fcntl (fd, F_SETFL, flags | O_NONBLOCK) < 0)
	return errno;
    return 0;
}

#endif /* SERVER_FLOWCONTROL */

static void serve_questionable PROTO((char *));

static void
serve_questionable (arg)
    char *arg;
{
    static int initted;

    if (!initted)
    {
	/* Pick up ignores from CVSROOTADM_IGNORE, $HOME/.cvsignore on server,
	   and CVSIGNORE on server.  */
	ign_setup ();
	initted = 1;
    }

    if (dir_name == NULL)
    {
	buf_output0 (buf_to_net, "E Protocol error: 'Directory' missing");
	return;
    }

    if (!ign_name (arg))
    {
	char *update_dir;

	buf_output (buf_to_net, "M ? ", 4);
	update_dir = dir_name + strlen (server_temp_dir) + 1;
	if (!(update_dir[0] == '.' && update_dir[1] == '\0'))
	{
	    buf_output0 (buf_to_net, update_dir);
	    buf_output (buf_to_net, "/", 1);
	}
	buf_output0 (buf_to_net, arg);
	buf_output (buf_to_net, "\n", 1);
    }
}

static void serve_case PROTO ((char *));

static void
serve_case (arg)
    char *arg;
{
    ign_case = 1;
}

static struct buffer *protocol;

/* This is the output which we are saving up to send to the server, in the
   child process.  We will push it through, via the `protocol' buffer, when
   we have a complete line.  */
static struct buffer *saved_output;
/* Likewise, but stuff which will go to stderr.  */
static struct buffer *saved_outerr;

static void
protocol_memory_error (buf)
    struct buffer *buf;
{
    error (1, ENOMEM, "Virtual memory exhausted");
}

/*
 * Process IDs of the subprocess, or negative if that subprocess
 * does not exist.
 */
static pid_t command_pid;

static void
outbuf_memory_error (buf)
    struct buffer *buf;
{
    static const char msg[] = "E Fatal server error\n\
error ENOMEM Virtual memory exhausted.\n";
    if (command_pid > 0)
	kill (command_pid, SIGTERM);

    /*
     * We have arranged things so that printing this now either will
     * be legal, or the "E fatal error" line will get glommed onto the
     * end of an existing "E" or "M" response.
     */

    /* If this gives an error, not much we could do.  syslog() it?  */
    write (STDOUT_FILENO, msg, sizeof (msg) - 1);
    error_exit ();
}

static void
input_memory_error (buf)
     struct buffer *buf;
{
    outbuf_memory_error (buf);
}



/* If command is legal, return 1.
 * Else if command is illegal and croak_on_illegal is set, then die.
 * Else just return 0 to indicate that command is illegal.
 */
static int
check_command_legal_p (cmd_name)
    char *cmd_name;
{
    /* Right now, only pserver notices illegal commands -- namely,
     * write attempts by a read-only user.  Therefore, if CVS_Username
     * is not set, this just returns 1, because CVS_Username unset
     * means pserver is not active.
     */
#ifdef AUTH_SERVER_SUPPORT
    if (CVS_Username == NULL)
        return 1;

    if (lookup_command_attribute (cmd_name) & CVS_CMD_MODIFIES_REPOSITORY)
    {
        /* This command has the potential to modify the repository, so
         * we check if the user have permission to do that.
         *
         * (Only relevant for remote users -- local users can do
         * whatever normal Unix file permissions allow them to do.)
         *
         * The decision method:
         *
         *    If $CVSROOT/CVSADMROOT_READERS exists and user is listed
         *    in it, then read-only access for user.
         *
         *    Or if $CVSROOT/CVSADMROOT_WRITERS exists and user NOT
         *    listed in it, then also read-only access for user.
         *
         *    Else read-write access for user.
         */

         char *linebuf = NULL;
         int num_red = 0;
         size_t linebuf_len = 0;
         char *fname;
         size_t flen;
         FILE *fp;
         int found_it = 0;
         
         /* else */
         flen = strlen (CVSroot_directory)
                + strlen (CVSROOTADM)
                + strlen (CVSROOTADM_READERS)
                + 3;

         fname = xmalloc (flen);
         (void) sprintf (fname, "%s/%s/%s", CVSroot_directory,
			CVSROOTADM, CVSROOTADM_READERS);

         fp = fopen (fname, "r");

         if (fp == NULL)
	 {
	     if (!existence_error (errno))
	     {
		 /* Need to deny access, so that attackers can't fool
		    us with some sort of denial of service attack.  */
		 error (0, errno, "cannot open %s", fname);
		 free (fname);
		 return 0;
	     }
	 }
         else  /* successfully opened readers file */
         {
             while ((num_red = getline (&linebuf, &linebuf_len, fp)) >= 0)
             {
                 /* Hmmm, is it worth importing my own readline
                    library into CVS?  It takes care of chopping
                    leading and trailing whitespace, "#" comments, and
                    newlines automatically when so requested.  Would
                    save some code here...  -kff */

                 /* Chop newline by hand, for strcmp()'s sake. */
                 if (linebuf[num_red - 1] == '\n')
                     linebuf[num_red - 1] = '\0';

                 if (strcmp (linebuf, CVS_Username) == 0)
                     goto handle_illegal;
             }
	     if (num_red < 0 && !feof (fp))
		 error (0, errno, "cannot read %s", fname);

             /* If not listed specifically as a reader, then this user
                has write access by default unless writers are also
                specified in a file . */
	     if (fclose (fp) < 0)
		 error (0, errno, "cannot close %s", fname);
         }
	 free (fname);

	 /* Now check the writers file.  */

         flen = strlen (CVSroot_directory)
                + strlen (CVSROOTADM)
                + strlen (CVSROOTADM_WRITERS)
                + 3;

         fname = xmalloc (flen);
         (void) sprintf (fname, "%s/%s/%s", CVSroot_directory,
			CVSROOTADM, CVSROOTADM_WRITERS);

         fp = fopen (fname, "r");

         if (fp == NULL)
         {
	     if (linebuf)
	         free (linebuf);
	     if (existence_error (errno))
	     {
		 /* Writers file does not exist, so everyone is a writer,
		    by default.  */
		 free (fname);
		 return 1;
	     }
	     else
	     {
		 /* Need to deny access, so that attackers can't fool
		    us with some sort of denial of service attack.  */
		 error (0, errno, "cannot read %s", fname);
		 free (fname);
		 return 0;
	     }
         }

         found_it = 0;
         while ((num_red = getline (&linebuf, &linebuf_len, fp)) >= 0)
         {
             /* Chop newline by hand, for strcmp()'s sake. */
             if (linebuf[num_red - 1] == '\n')
                 linebuf[num_red - 1] = '\0';
           
             if (strcmp (linebuf, CVS_Username) == 0)
             {
                 found_it = 1;
                 break;
             }
         }
	 if (num_red < 0 && !feof (fp))
	     error (0, errno, "cannot read %s", fname);

         if (found_it)
         {
             if (fclose (fp) < 0)
		 error (0, errno, "cannot close %s", fname);
             if (linebuf)
                 free (linebuf);
	     free (fname);
             return 1;
         }
         else   /* writers file exists, but this user not listed in it */
         {
         handle_illegal:
             if (fclose (fp) < 0)
		 error (0, errno, "cannot close %s", fname);
             if (linebuf)
                 free (linebuf);
	     free (fname);
	     return 0;
         }
    }
#endif /* AUTH_SERVER_SUPPORT */

    /* If ever reach end of this function, command must be legal. */
    return 1;
}



/* Execute COMMAND in a subprocess with the approriate funky things done.  */

static struct fd_set_wrapper { fd_set fds; } command_fds_to_drain;
static int max_command_fd;

#ifdef SERVER_FLOWCONTROL
static int flowcontrol_pipe[2];
#endif /* SERVER_FLOWCONTROL */

static void
do_cvs_command (cmd_name, command)
    char *cmd_name;
    int (*command) PROTO((int argc, char **argv));
{
    /*
     * The following file descriptors are set to -1 if that file is not
     * currently open.
     */

    /* Data on these pipes is a series of '\n'-terminated lines.  */
    int stdout_pipe[2];
    int stderr_pipe[2];

    /*
     * Data on this pipe is a series of counted (see buf_send_counted)
     * packets.  Each packet must be processed atomically (i.e. not
     * interleaved with data from stdout_pipe or stderr_pipe).
     */
    int protocol_pipe[2];
    
    int dev_null_fd = -1;

    int errs;

    command_pid = -1;
    stdout_pipe[0] = -1;
    stdout_pipe[1] = -1;
    stderr_pipe[0] = -1;
    stderr_pipe[1] = -1;
    protocol_pipe[0] = -1;
    protocol_pipe[1] = -1;

    server_write_entries ();

    if (print_pending_error ())
	goto free_args_and_return;

    /* Global `command_name' is probably "server" right now -- only
       serve_export() sets it to anything else.  So we will use local
       parameter `cmd_name' to determine if this command is legal for
       this user.  */
    if (!check_command_legal_p (cmd_name))
    {
	buf_output0 (buf_to_net, "E ");
	buf_output0 (buf_to_net, program_name);
	buf_output0 (buf_to_net, " [server aborted]: \"");
	buf_output0 (buf_to_net, cmd_name);
	buf_output0 (buf_to_net, "\" requires write access to the repository\n\
error  \n");
	goto free_args_and_return;
    }

    (void) server_notify ();

    /*
     * We use a child process which actually does the operation.  This
     * is so we can intercept its standard output.  Even if all of CVS
     * were written to go to some special routine instead of writing
     * to stdout or stderr, we would still need to do the same thing
     * for the RCS commands.
     */

    if (pipe (stdout_pipe) < 0)
    {
	print_error (errno);
	goto error_exit;
    }
    if (pipe (stderr_pipe) < 0)
    {
	print_error (errno);
	goto error_exit;
    }
    if (pipe (protocol_pipe) < 0)
    {
	print_error (errno);
	goto error_exit;
    }
#ifdef SERVER_FLOWCONTROL
    if (pipe (flowcontrol_pipe) < 0)
    {
	print_error (errno);
	goto error_exit;
    }
    set_nonblock_fd (flowcontrol_pipe[0]);
    set_nonblock_fd (flowcontrol_pipe[1]);
#endif /* SERVER_FLOWCONTROL */

    dev_null_fd = CVS_OPEN (DEVNULL, O_RDONLY);
    if (dev_null_fd < 0)
    {
	print_error (errno);
	goto error_exit;
    }

    /* We shouldn't have any partial lines from cvs_output and
       cvs_outerr, but we handle them here in case there is a bug.  */
    if (! buf_empty_p (saved_output))
    {
	buf_append_char (saved_output, '\n');
	buf_copy_lines (buf_to_net, saved_output, 'M');
    }
    if (! buf_empty_p (saved_outerr))
    {
	buf_append_char (saved_outerr, '\n');
	buf_copy_lines (buf_to_net, saved_outerr, 'E');
    }

    /* Flush out any pending data.  */
    buf_flush (buf_to_net, 1);

    /* Don't use vfork; we're not going to exec().  */
    command_pid = fork ();
    if (command_pid < 0)
    {
	print_error (errno);
	goto error_exit;
    }
    if (command_pid == 0)
    {
	int exitstatus;

	/* Since we're in the child, and the parent is going to take
	   care of packaging up our error messages, we can clear this
	   flag.  */
	error_use_protocol = 0;

	protocol = fd_buffer_initialize (protocol_pipe[1], 0,
					 protocol_memory_error);

	/* At this point we should no longer be using buf_to_net and
           buf_from_net.  Instead, everything should go through
           protocol.  */
	buf_to_net = NULL;
	buf_from_net = NULL;

	/* These were originally set up to use outbuf_memory_error.
           Since we're now in the child, we should use the simpler
           protocol_memory_error function.  */
	saved_output->memory_error = protocol_memory_error;
	saved_outerr->memory_error = protocol_memory_error;

	if (dup2 (dev_null_fd, STDIN_FILENO) < 0)
	    error (1, errno, "can't set up pipes");
	if (dup2 (stdout_pipe[1], STDOUT_FILENO) < 0)
	    error (1, errno, "can't set up pipes");
	if (dup2 (stderr_pipe[1], STDERR_FILENO) < 0)
	    error (1, errno, "can't set up pipes");
	close (stdout_pipe[0]);
	close (stderr_pipe[0]);
	close (protocol_pipe[0]);
#ifdef SERVER_FLOWCONTROL
	close (flowcontrol_pipe[1]);
#endif /* SERVER_FLOWCONTROL */

	/*
	 * Set this in .bashrc if you want to give yourself time to attach
	 * to the subprocess with a debugger.
	 */
	if (getenv ("CVS_SERVER_SLEEP"))
	{
	    int secs = atoi (getenv ("CVS_SERVER_SLEEP"));
	    sleep (secs);
	}

	exitstatus = (*command) (argument_count, argument_vector);

	/*
	 * When we exit, that will close the pipes, giving an EOF to
	 * the parent.
	 */
	exit (exitstatus);
    }

    /* OK, sit around getting all the input from the child.  */
    {
	struct buffer *stdoutbuf;
	struct buffer *stderrbuf;
	struct buffer *protocol_inbuf;
	/* Number of file descriptors to check in select ().  */
	int num_to_check;
	int count_needed = 0;
#ifdef SERVER_FLOWCONTROL
	int have_flowcontrolled = 0;
#endif /* SERVER_FLOWCONTROL */

	FD_ZERO (&command_fds_to_drain.fds);
	num_to_check = stdout_pipe[0];
	FD_SET (stdout_pipe[0], &command_fds_to_drain.fds);
	if (stderr_pipe[0] > num_to_check)
	  num_to_check = stderr_pipe[0];
	FD_SET (stderr_pipe[0], &command_fds_to_drain.fds);
	if (protocol_pipe[0] > num_to_check)
	  num_to_check = protocol_pipe[0];
	FD_SET (protocol_pipe[0], &command_fds_to_drain.fds);
	if (STDOUT_FILENO > num_to_check)
	  num_to_check = STDOUT_FILENO;
	max_command_fd = num_to_check;
	/*
	 * File descriptors are numbered from 0, so num_to_check needs to
	 * be one larger than the largest descriptor.
	 */
	++num_to_check;
	if (num_to_check > FD_SETSIZE)
	{
	    buf_output0 (buf_to_net,
			 "E internal error: FD_SETSIZE not big enough.\n\
error  \n");
	    goto error_exit;
	}

	stdoutbuf = fd_buffer_initialize (stdout_pipe[0], 1,
					  input_memory_error);

	stderrbuf = fd_buffer_initialize (stderr_pipe[0], 1,
					  input_memory_error);

	protocol_inbuf = fd_buffer_initialize (protocol_pipe[0], 1,
					       input_memory_error);

	set_nonblock (buf_to_net);
	set_nonblock (stdoutbuf);
	set_nonblock (stderrbuf);
	set_nonblock (protocol_inbuf);

	if (close (stdout_pipe[1]) < 0)
	{
	    print_error (errno);
	    goto error_exit;
	}
	stdout_pipe[1] = -1;

	if (close (stderr_pipe[1]) < 0)
	{
	    print_error (errno);
	    goto error_exit;
	}
	stderr_pipe[1] = -1;

	if (close (protocol_pipe[1]) < 0)
	{
	    print_error (errno);
	    goto error_exit;
	}
	protocol_pipe[1] = -1;

#ifdef SERVER_FLOWCONTROL
	if (close (flowcontrol_pipe[0]) < 0)
	{
	    print_error (errno);
	    goto error_exit;
	}
	flowcontrol_pipe[0] = -1;
#endif /* SERVER_FLOWCONTROL */

	if (close (dev_null_fd) < 0)
	{
	    print_error (errno);
	    goto error_exit;
	}
	dev_null_fd = -1;

	while (stdout_pipe[0] >= 0
	       || stderr_pipe[0] >= 0
	       || protocol_pipe[0] >= 0)
	{
	    fd_set readfds;
	    fd_set writefds;
	    int numfds;
#ifdef SERVER_FLOWCONTROL
	    int bufmemsize;

	    /*
	     * See if we are swamping the remote client and filling our VM.
	     * Tell child to hold off if we do.
	     */
	    bufmemsize = buf_count_mem (buf_to_net);
	    if (!have_flowcontrolled && (bufmemsize > SERVER_HI_WATER))
	    {
		if (write(flowcontrol_pipe[1], "S", 1) == 1)
		    have_flowcontrolled = 1;
	    }
	    else if (have_flowcontrolled && (bufmemsize < SERVER_LO_WATER))
	    {
		if (write(flowcontrol_pipe[1], "G", 1) == 1)
		    have_flowcontrolled = 0;
	    }
#endif /* SERVER_FLOWCONTROL */

	    FD_ZERO (&readfds);
	    FD_ZERO (&writefds);
	    if (! buf_empty_p (buf_to_net))
	        FD_SET (STDOUT_FILENO, &writefds);

	    if (stdout_pipe[0] >= 0)
	    {
		FD_SET (stdout_pipe[0], &readfds);
	    }
	    if (stderr_pipe[0] >= 0)
	    {
		FD_SET (stderr_pipe[0], &readfds);
	    }
	    if (protocol_pipe[0] >= 0)
	    {
		FD_SET (protocol_pipe[0], &readfds);
	    }

	    /* This process of selecting on the three pipes means that
	       we might not get output in the same order in which it
	       was written, thus producing the well-known
	       "out-of-order" bug.  If the child process uses
	       cvs_output and cvs_outerr, it will send everything on
	       the protocol_pipe and avoid this problem, so the
	       solution is to use cvs_output and cvs_outerr in the
	       child process.  */
	    do {
		/* This used to select on exceptions too, but as far
                   as I know there was never any reason to do that and
                   SCO doesn't let you select on exceptions on pipes.  */
		numfds = select (num_to_check, &readfds, &writefds,
				 (fd_set *)0, (struct timeval *)NULL);
		if (numfds < 0
		    && errno != EINTR)
		{
		    print_error (errno);
		    goto error_exit;
		}
	    } while (numfds < 0);
	    
	    if (FD_ISSET (STDOUT_FILENO, &writefds))
	    {
		/* What should we do with errors?  syslog() them?  */
		buf_send_output (buf_to_net);
	    }

	    if (stdout_pipe[0] >= 0
		&& (FD_ISSET (stdout_pipe[0], &readfds)))
	    {
	        int status;

	        status = buf_input_data (stdoutbuf, (int *) NULL);

		buf_copy_lines (buf_to_net, stdoutbuf, 'M');

		if (status == -1)
		    stdout_pipe[0] = -1;
		else if (status > 0)
		{
		    print_error (status);
		    goto error_exit;
		}

		/* What should we do with errors?  syslog() them?  */
		buf_send_output (buf_to_net);
	    }

	    if (stderr_pipe[0] >= 0
		&& (FD_ISSET (stderr_pipe[0], &readfds)))
	    {
	        int status;

	        status = buf_input_data (stderrbuf, (int *) NULL);

		buf_copy_lines (buf_to_net, stderrbuf, 'E');

		if (status == -1)
		    stderr_pipe[0] = -1;
		else if (status > 0)
		{
		    print_error (status);
		    goto error_exit;
		}

		/* What should we do with errors?  syslog() them?  */
		buf_send_output (buf_to_net);
	    }

	    if (protocol_pipe[0] >= 0
		&& (FD_ISSET (protocol_pipe[0], &readfds)))
	    {
		int status;
		int count_read;
		int special;
		
		status = buf_input_data (protocol_inbuf, &count_read);

		if (status == -1)
		    protocol_pipe[0] = -1;
		else if (status > 0)
		{
		    print_error (status);
		    goto error_exit;
		}

		/*
		 * We only call buf_copy_counted if we have read
		 * enough bytes to make it worthwhile.  This saves us
		 * from continually recounting the amount of data we
		 * have.
		 */
		count_needed -= count_read;
		while (count_needed <= 0)
		{
		    count_needed = buf_copy_counted (buf_to_net,
						     protocol_inbuf,
						     &special);

		    /* What should we do with errors?  syslog() them?  */
		    buf_send_output (buf_to_net);

		    /* If SPECIAL got set to -1, it means that the child
		       wants us to flush the pipe.  We don't want to block
		       on the network, but we flush what we can.  If the
		       client supports the 'F' command, we send it.  */
		    if (special == -1)
		    {
			if (supported_response ("F"))
			{
			    buf_append_char (buf_to_net, 'F');
			    buf_append_char (buf_to_net, '\n');
			}

			cvs_flusherr ();
		    }
		}
	    }
	}

	/*
	 * OK, we've gotten EOF on all the pipes.  If there is
	 * anything left on stdoutbuf or stderrbuf (this could only
	 * happen if there was no trailing newline), send it over.
	 */
	if (! buf_empty_p (stdoutbuf))
	{
	    buf_append_char (stdoutbuf, '\n');
	    buf_copy_lines (buf_to_net, stdoutbuf, 'M');
	}
	if (! buf_empty_p (stderrbuf))
	{
	    buf_append_char (stderrbuf, '\n');
	    buf_copy_lines (buf_to_net, stderrbuf, 'E');
	}
	if (! buf_empty_p (protocol_inbuf))
	    buf_output0 (buf_to_net,
			 "E Protocol error: uncounted data discarded\n");

	errs = 0;

	while (command_pid > 0)
	{
	    int status;
	    pid_t waited_pid;
	    waited_pid = waitpid (command_pid, &status, 0);
	    if (waited_pid < 0)
	    {
		/*
		 * Intentionally ignoring EINTR.  Other errors
		 * "can't happen".
		 */
		continue;
	    }
	    
	    if (WIFEXITED (status))
		errs += WEXITSTATUS (status);
	    else
	    {
	        int sig = WTERMSIG (status);
	        char buf[50];
		/*
		 * This is really evil, because signals might be numbered
		 * differently on the two systems.  We should be using
		 * signal names (either of the "Terminated" or the "SIGTERM"
		 * variety).  But cvs doesn't currently use libiberty...we
		 * could roll our own....  FIXME.
		 */
		buf_output0 (buf_to_net, "E Terminated with fatal signal ");
		sprintf (buf, "%d\n", sig);
		buf_output0 (buf_to_net, buf);

		/* Test for a core dump.  Is this portable?  */
		if (status & 0x80)
		{
		    buf_output0 (buf_to_net, "E Core dumped; preserving ");
		    buf_output0 (buf_to_net, orig_server_temp_dir);
		    buf_output0 (buf_to_net, " on server.\n\
E CVS locks may need cleaning up.\n");
		    dont_delete_temp = 1;
		}
		++errs;
	    }
	    if (waited_pid == command_pid)
		command_pid = -1;
	}

	/*
	 * OK, we've waited for the child.  By now all CVS locks are free
	 * and it's OK to block on the network.
	 */
	set_block (buf_to_net);
	buf_flush (buf_to_net, 1);
    }

    if (errs)
	/* We will have printed an error message already.  */
	buf_output0 (buf_to_net, "error  \n");
    else
	buf_output0 (buf_to_net, "ok\n");
    goto free_args_and_return;

 error_exit:
    if (command_pid > 0)
	kill (command_pid, SIGTERM);

    while (command_pid > 0)
    {
	pid_t waited_pid;
	waited_pid = waitpid (command_pid, (int *) 0, 0);
	if (waited_pid < 0 && errno == EINTR)
	    continue;
	if (waited_pid == command_pid)
	    command_pid = -1;
    }

    close (dev_null_fd);
    close (protocol_pipe[0]);
    close (protocol_pipe[1]);
    close (stderr_pipe[0]);
    close (stderr_pipe[1]);
    close (stdout_pipe[0]);
    close (stdout_pipe[1]);

 free_args_and_return:
    /* Now free the arguments.  */
    {
	/* argument_vector[0] is a dummy argument, we don't mess with it.  */
	char **cp;
	for (cp = argument_vector + 1;
	     cp < argument_vector + argument_count;
	     ++cp)
	    free (*cp);

	argument_count = 1;
    }

    /* Flush out any data not yet sent.  */
    set_block (buf_to_net);
    buf_flush (buf_to_net, 1);

    return;
}

#ifdef SERVER_FLOWCONTROL
/*
 * Called by the child at convenient points in the server's execution for
 * the server child to block.. ie: when it has no locks active.
 */
void
server_pause_check()
{
    int paused = 0;
    char buf[1];

    while (read (flowcontrol_pipe[0], buf, 1) == 1)
    {
	if (*buf == 'S')	/* Stop */
	    paused = 1;
	else if (*buf == 'G')	/* Go */
	    paused = 0;
	else
	    return;		/* ??? */
    }
    while (paused) {
	int numfds, numtocheck;
	fd_set fds;

	FD_ZERO (&fds);
	FD_SET (flowcontrol_pipe[0], &fds);
	numtocheck = flowcontrol_pipe[0] + 1;
	
	do {
	    numfds = select (numtocheck, &fds, (fd_set *)0,
			     (fd_set *)0, (struct timeval *)NULL);
	    if (numfds < 0
		&& errno != EINTR)
	    {
		print_error (errno);
		return;
	    }
	} while (numfds < 0);
	    
	if (FD_ISSET (flowcontrol_pipe[0], &fds))
	{
	    int got;

	    while ((got = read (flowcontrol_pipe[0], buf, 1)) == 1)
	    {
		if (*buf == 'S')	/* Stop */
		    paused = 1;
		else if (*buf == 'G')	/* Go */
		    paused = 0;
		else
		    return;		/* ??? */
	    }

	    /* This assumes that we are using BSD or POSIX nonblocking
               I/O.  System V nonblocking I/O returns zero if there is
               nothing to read.  */
	    if (got == 0)
	        error (1, 0, "flow control EOF");
	    if (got < 0 && ! blocking_error (errno))
	    {
	        error (1, errno, "flow control read failed");
	    }
	}
    }
}
#endif /* SERVER_FLOWCONTROL */

/* This variable commented in server.h.  */
char *server_dir = NULL;

static void output_dir PROTO((char *, char *));

static void
output_dir (update_dir, repository)
    char *update_dir;
    char *repository;
{
    if (server_dir != NULL)
    {
	buf_output0 (protocol, server_dir);
	buf_output0 (protocol, "/");
    }
    if (update_dir[0] == '\0')
	buf_output0 (protocol, ".");
    else
	buf_output0 (protocol, update_dir);
    buf_output0 (protocol, "/\n");
    buf_output0 (protocol, repository);
    buf_output0 (protocol, "/");
}

/*
 * Entries line that we are squirreling away to send to the client when
 * we are ready.
 */
static char *entries_line;

/*
 * File which has been Scratch_File'd, we are squirreling away that fact
 * to inform the client when we are ready.
 */
static char *scratched_file;

/*
 * The scratched_file will need to be removed as well as having its entry
 * removed.
 */
static int kill_scratched_file;

void
server_register (name, version, timestamp, options, tag, date, conflict)
    char *name;
    char *version;
    char *timestamp;
    char *options;
    char *tag;
    char *date;
    char *conflict;
{
    int len;

    if (options == NULL)
	options = "";

    if (trace)
    {
	(void) fprintf (stderr,
			"%c-> server_register(%s, %s, %s, %s, %s, %s, %s)\n",
			(server_active) ? 'S' : ' ', /* silly */
			name, version, timestamp, options, tag ? tag : "",
			date ? date : "", conflict ? conflict : "");
    }

    if (entries_line != NULL)
    {
	/*
	 * If CVS decides to Register it more than once (which happens
	 * on "cvs update foo/foo.c" where foo and foo.c are already
	 * checked out), use the last of the entries lines Register'd.
	 */
	free (entries_line);
    }

    /*
     * I have reports of Scratch_Entry and Register both happening, in
     * two different cases.  Using the last one which happens is almost
     * surely correct; I haven't tracked down why they both happen (or
     * even verified that they are for the same file).
     */
    if (scratched_file != NULL)
    {
	free (scratched_file);
	scratched_file = NULL;
    }

    len = (strlen (name) + strlen (version) + strlen (options) + 80);
    if (tag)
	len += strlen (tag);
    if (date)
	len += strlen (date);
    
    entries_line = xmalloc (len);
    sprintf (entries_line, "/%s/%s/", name, version);
    if (conflict != NULL)
    {
	strcat (entries_line, "+=");
    }
    strcat (entries_line, "/");
    strcat (entries_line, options);
    strcat (entries_line, "/");
    if (tag != NULL)
    {
	strcat (entries_line, "T");
	strcat (entries_line, tag);
    }
    else if (date != NULL)
    {
	strcat (entries_line, "D");
	strcat (entries_line, date);
    }
}

void
server_scratch (fname)
    char *fname;
{
    /*
     * I have reports of Scratch_Entry and Register both happening, in
     * two different cases.  Using the last one which happens is almost
     * surely correct; I haven't tracked down why they both happen (or
     * even verified that they are for the same file).
     */
    if (entries_line != NULL)
    {
	free (entries_line);
	entries_line = NULL;
    }

    if (scratched_file != NULL)
    {
	buf_output0 (protocol,
		     "E CVS server internal error: duplicate Scratch_Entry\n");
	buf_send_counted (protocol);
	return;
    }
    scratched_file = xstrdup (fname);
    kill_scratched_file = 1;
}

void
server_scratch_entry_only ()
{
    kill_scratched_file = 0;
}

/* Print a new entries line, from a previous server_register.  */
static void
new_entries_line ()
{
    if (entries_line)
    {
	buf_output0 (protocol, entries_line);
	buf_output (protocol, "\n", 1);
    }
    else
	/* Return the error message as the Entries line.  */
	buf_output0 (protocol,
		     "CVS server internal error: Register missing\n");
    free (entries_line);
    entries_line = NULL;
}


static void
serve_ci (arg)
    char *arg;
{
    do_cvs_command ("commit", commit);
}

static void
checked_in_response (file, update_dir, repository)
    char *file;
    char *update_dir;
    char *repository;
{
    if (supported_response ("Mode"))
    {
	struct stat sb;
	char *mode_string;

	if ( CVS_STAT (file, &sb) < 0)
	{
	    /* Not clear to me why the file would fail to exist, but it
	       was happening somewhere in the testsuite.  */
	    if (!existence_error (errno))
		error (0, errno, "cannot stat %s", file);
	}
	else
	{
	    buf_output0 (protocol, "Mode ");
	    mode_string = mode_to_string (sb.st_mode);
	    buf_output0 (protocol, mode_string);
	    buf_output0 (protocol, "\n");
	    free (mode_string);
	}
    }

    buf_output0 (protocol, "Checked-in ");
    output_dir (update_dir, repository);
    buf_output0 (protocol, file);
    buf_output (protocol, "\n", 1);
    new_entries_line ();
}

void
server_checked_in (file, update_dir, repository)
    char *file;
    char *update_dir;
    char *repository;
{
    if (noexec)
	return;
    if (scratched_file != NULL && entries_line == NULL)
    {
	/*
	 * This happens if we are now doing a "cvs remove" after a previous
	 * "cvs add" (without a "cvs ci" in between).
	 */
	buf_output0 (protocol, "Remove-entry ");
	output_dir (update_dir, repository);
	buf_output0 (protocol, file);
	buf_output (protocol, "\n", 1);
	free (scratched_file);
	scratched_file = NULL;
    }
    else
    {
	checked_in_response (file, update_dir, repository);
    }
    buf_send_counted (protocol);
}

void
server_update_entries (file, update_dir, repository, updated)
    char *file;
    char *update_dir;
    char *repository;
    enum server_updated_arg4 updated;
{
    if (noexec)
	return;
    if (updated == SERVER_UPDATED)
	checked_in_response (file, update_dir, repository);
    else
    {
	if (!supported_response ("New-entry"))
	    return;
	buf_output0 (protocol, "New-entry ");
	output_dir (update_dir, repository);
	buf_output0 (protocol, file);
	buf_output (protocol, "\n", 1);
	new_entries_line ();
    }

    buf_send_counted (protocol);
}

static void
serve_update (arg)
    char *arg;
{
    do_cvs_command ("update", update);
}

static void
serve_diff (arg)
    char *arg;
{
    do_cvs_command ("diff", diff);
}

static void
serve_log (arg)
    char *arg;
{
    do_cvs_command ("log", cvslog);
}

static void
serve_add (arg)
    char *arg;
{
    do_cvs_command ("add", add);
}

static void
serve_remove (arg)
    char *arg;
{
    do_cvs_command ("cvsremove", cvsremove);
}

static void
serve_status (arg)
    char *arg;
{
    do_cvs_command ("status", status);
}

static void
serve_rdiff (arg)
    char *arg;
{
    do_cvs_command ("patch", patch);
}

static void
serve_tag (arg)
    char *arg;
{
    do_cvs_command ("cvstag", cvstag);
}

static void
serve_rtag (arg)
    char *arg;
{
    do_cvs_command ("rtag", rtag);
}

static void
serve_import (arg)
    char *arg;
{
    do_cvs_command ("import", import);
}

static void
serve_admin (arg)
    char *arg;
{
    do_cvs_command ("admin", admin);
}

static void
serve_history (arg)
    char *arg;
{
    do_cvs_command ("history", history);
}

static void
serve_release (arg)
    char *arg;
{
    do_cvs_command ("release", release);
}

static void serve_watch_on PROTO ((char *));

static void
serve_watch_on (arg)
    char *arg;
{
    do_cvs_command ("watch_on", watch_on);
}

static void serve_watch_off PROTO ((char *));

static void
serve_watch_off (arg)
    char *arg;
{
    do_cvs_command ("watch_off", watch_off);
}

static void serve_watch_add PROTO ((char *));

static void
serve_watch_add (arg)
    char *arg;
{
    do_cvs_command ("watch_add", watch_add);
}

static void serve_watch_remove PROTO ((char *));

static void
serve_watch_remove (arg)
    char *arg;
{
    do_cvs_command ("watch_remove", watch_remove);
}

static void serve_watchers PROTO ((char *));

static void
serve_watchers (arg)
    char *arg;
{
    do_cvs_command ("watchers", watchers);
}

static void serve_editors PROTO ((char *));

static void
serve_editors (arg)
    char *arg;
{
    do_cvs_command ("editors", editors);
}

static int noop PROTO ((int, char **));

static int
noop (argc, argv)
    int argc;
    char **argv;
{
    return 0;
}

static void serve_noop PROTO ((char *));

static void
serve_noop (arg)
    char *arg;
{
    do_cvs_command ("noop", noop);
}

static void serve_init PROTO ((char *));

static void
serve_init (arg)
    char *arg;
{
    if (!isabsolute (arg))
    {
	if (alloc_pending (80 + strlen (arg)))
	    sprintf (pending_error_text,
		     "E Root %s must be an absolute pathname", arg);
	/* Fall through to do_cvs_command which will return the
	   actual error.  */
    }
    set_local_cvsroot (arg);

    do_cvs_command ("init", init);
}

static void serve_annotate PROTO ((char *));

static void
serve_annotate (arg)
    char *arg;
{
    do_cvs_command ("annotate", annotate);
}

static void
serve_co (arg)
    char *arg;
{
    char *tempdir;
    int status;

    if (print_pending_error ())
	return;

    if (!isdir (CVSADM))
    {
	/*
	 * The client has not sent a "Repository" line.  Check out
	 * into a pristine directory.
	 */
	tempdir = malloc (strlen (server_temp_dir) + 80);
	if (tempdir == NULL)
	{
	    buf_output0 (buf_to_net, "E Out of memory\n");
	    return;
	}
	strcpy (tempdir, server_temp_dir);
	strcat (tempdir, "/checkout-dir");
	status = mkdir_p (tempdir);
	if (status != 0 && status != EEXIST)
	{
	    buf_output0 (buf_to_net, "E Cannot create ");
	    buf_output0 (buf_to_net, tempdir);
	    buf_append_char (buf_to_net, '\n');
	    print_error (errno);
	    free (tempdir);
	    return;
	}

	if ( CVS_CHDIR (tempdir) < 0)
	{
	    buf_output0 (buf_to_net, "E Cannot change to directory ");
	    buf_output0 (buf_to_net, tempdir);
	    buf_append_char (buf_to_net, '\n');
	    print_error (errno);
	    free (tempdir);
	    return;
	}
	free (tempdir);
    }

    /* Compensate for server_export()'s setting of command_name.
     *
     * [It probably doesn't matter if do_cvs_command() gets "export"
     *  or "checkout", but we ought to be accurate where possible.]
     */
    do_cvs_command ((strcmp (command_name, "export") == 0) ?
                    "export" : "checkout",
                    checkout);
}

static void
serve_export (arg)
    char *arg;
{
    /* Tell checkout() to behave like export not checkout.  */
    command_name = "export";
    serve_co (arg);
}

void
server_copy_file (file, update_dir, repository, newfile)
    char *file;
    char *update_dir;
    char *repository;
    char *newfile;
{
    if (!supported_response ("Copy-file"))
	return;
    buf_output0 (protocol, "Copy-file ");
    output_dir (update_dir, repository);
    buf_output0 (protocol, file);
    buf_output0 (protocol, "\n");
    buf_output0 (protocol, newfile);
    buf_output0 (protocol, "\n");
}

/* See server.h for description.  */

void
server_modtime (finfo, vers_ts)
    struct file_info *finfo;
    Vers_TS *vers_ts;
{
    char date[MAXDATELEN];
    int year, month, day, hour, minute, second;
    /* Note that these strings are specified in RFC822 and do not vary
       according to locale.  */
    static const char *const month_names[] =
      {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    assert (vers_ts->vn_rcs != NULL);

    if (!supported_response ("Mod-time"))
	return;

    /* The only hard part about this routine is converting the date
       formats.  In terms of functionality it all boils down to the
       call to RCS_getrevtime.  */
    if (RCS_getrevtime (finfo->rcs, vers_ts->vn_rcs, date, 0) == (time_t) -1)
	/* FIXME? should we be printing some kind of warning?  For one
	   thing I'm not 100% sure whether this happens in non-error
	   circumstances.  */
	return;

    sscanf (date, SDATEFORM, &year, &month, &day, &hour, &minute, &second);
    sprintf (date, "%d %s %d %d:%d:%d -0000", day,
	     month < 1 || month > 12 ? "???" : month_names[month - 1],
	     year, hour, minute, second);
    buf_output0 (protocol, "Mod-time ");
    buf_output0 (protocol, date);
    buf_output0 (protocol, "\n");
}

/* See server.h for description.  */

void
server_updated (finfo, vers, updated, file_info, checksum)
    struct file_info *finfo;
    Vers_TS *vers;
    enum server_updated_arg4 updated;
    struct stat *file_info;
    unsigned char *checksum;
{
    if (noexec)
    {
	/* Hmm, maybe if we did the same thing for entries_file, we
	   could get rid of the kludges in server_register and
	   server_scratch which refrain from warning if both
	   Scratch_Entry and Register get called.  Maybe.  */
	if (scratched_file)
	{
	    free (scratched_file);
	    scratched_file = NULL;
	}
	return;
    }

    if (entries_line != NULL && scratched_file == NULL)
    {
	FILE *f;
	struct stat sb;
	struct buffer_data *list, *last;
	unsigned long size;
	char size_text[80];

	if ( CVS_STAT (finfo->file, &sb) < 0)
	{
	    if (existence_error (errno))
	    {
		/*
		 * If we have a sticky tag for a branch on which the
		 * file is dead, and cvs update the directory, it gets
		 * a T_CHECKOUT but no file.  So in this case just
		 * forget the whole thing.  */
		free (entries_line);
		entries_line = NULL;
		goto done;
	    }
	    error (1, errno, "reading %s", finfo->fullname);
	}

	if (checksum != NULL)
	{
	    static int checksum_supported = -1;

	    if (checksum_supported == -1)
	    {
		checksum_supported = supported_response ("Checksum");
	    }

	    if (checksum_supported)
	    {
	        int i;
		char buf[3];

	        buf_output0 (protocol, "Checksum ");
		for (i = 0; i < 16; i++)
		{
		    sprintf (buf, "%02x", (unsigned int) checksum[i]);
		    buf_output0 (protocol, buf);
		}
		buf_append_char (protocol, '\n');
	    }
	}

	if (updated == SERVER_UPDATED)
	{
	    Node *node;
	    Entnode *entnode;

	    if (!(supported_response ("Created")
		  && supported_response ("Update-existing")))
		buf_output0 (protocol, "Updated ");
	    else
	    {
		assert (vers != NULL);
		if (vers->ts_user == NULL)
		    buf_output0 (protocol, "Created ");
		else
		    buf_output0 (protocol, "Update-existing ");
	    }

	    /* Now munge the entries to say that the file is unmodified,
	       in case we end up processing it again (e.g. modules3-6
	       in the testsuite).  */
	    node = findnode_fn (finfo->entries, finfo->file);
	    entnode = (Entnode *)node->data;
	    free (entnode->timestamp);
	    entnode->timestamp = xstrdup ("=");
	}
	else if (updated == SERVER_MERGED)
	    buf_output0 (protocol, "Merged ");
	else if (updated == SERVER_PATCHED)
	    buf_output0 (protocol, "Patched ");
	else if (updated == SERVER_RCS_DIFF)
	    buf_output0 (protocol, "Rcs-diff ");
	else
	    abort ();
	output_dir (finfo->update_dir, finfo->repository);
	buf_output0 (protocol, finfo->file);
	buf_output (protocol, "\n", 1);

	new_entries_line ();

        {
	    char *mode_string;

	    /* FIXME: When we check out files the umask of the server
	       (set in .bashrc if rsh is in use) affects what mode we
	       send, and it shouldn't.  */
	    if (file_info != NULL)
	        mode_string = mode_to_string (file_info->st_mode);
	    else
	        mode_string = mode_to_string (sb.st_mode);
	    buf_output0 (protocol, mode_string);
	    buf_output0 (protocol, "\n");
	    free (mode_string);
	}

	list = last = NULL;
	size = 0;
	if (sb.st_size > 0)
	{
	    /* Throughout this section we use binary mode to read the
	       file we are sending.  The client handles any line ending
	       translation if necessary.  */

	    if (file_gzip_level
		/*
		 * For really tiny files, the gzip process startup
		 * time will outweigh the compression savings.  This
		 * might be computable somehow; using 100 here is just
		 * a first approximation.
		 */
		&& sb.st_size > 100)
	    {
		int status, fd, gzip_status;
		pid_t gzip_pid;

		fd = CVS_OPEN (finfo->file, O_RDONLY | OPEN_BINARY, 0);
		if (fd < 0)
		    error (1, errno, "reading %s", finfo->fullname);
		fd = filter_through_gzip (fd, 1, file_gzip_level, &gzip_pid);
		f = fdopen (fd, "rb");
		status = buf_read_file_to_eof (f, &list, &last);
		size = buf_chain_length (list);
		if (status == -2)
		    (*protocol->memory_error) (protocol);
		else if (status != 0)
		    error (1, ferror (f) ? errno : 0, "reading %s",
			   finfo->fullname);
		if (fclose (f) == EOF)
		    error (1, errno, "reading %s", finfo->fullname);
		if (waitpid (gzip_pid, &gzip_status, 0) == -1)
		    error (1, errno, "waiting for gzip process %ld",
			   (long) gzip_pid);
		else if (gzip_status != 0)
		    error (1, 0, "gzip exited %d", gzip_status);
		/* Prepending length with "z" is flag for using gzip here.  */
		buf_output0 (protocol, "z");
	    }
	    else
	    {
		long status;

		size = sb.st_size;
		f = CVS_FOPEN (finfo->file, "rb");
		if (f == NULL)
		    error (1, errno, "reading %s", finfo->fullname);
		status = buf_read_file (f, sb.st_size, &list, &last);
		if (status == -2)
		    (*protocol->memory_error) (protocol);
		else if (status != 0)
		    error (1, ferror (f) ? errno : 0, "reading %s",
			   finfo->fullname);
		if (fclose (f) == EOF)
		    error (1, errno, "reading %s", finfo->fullname);
	    }
	}

	sprintf (size_text, "%lu\n", size);
	buf_output0 (protocol, size_text);

	buf_append_data (protocol, list, last);
	/* Note we only send a newline here if the file ended with one.  */

	/*
	 * Avoid using up too much disk space for temporary files.
	 * A file which does not exist indicates that the file is up-to-date,
	 * which is now the case.  If this is SERVER_MERGED, the file is
	 * not up-to-date, and we indicate that by leaving the file there.
	 * I'm thinking of cases like "cvs update foo/foo.c foo".
	 */
	if ((updated == SERVER_UPDATED
	     || updated == SERVER_PATCHED
	     || updated == SERVER_RCS_DIFF)
	    /* But if we are joining, we'll need the file when we call
	       join_file.  */
	    && !joining ())
	    CVS_UNLINK (finfo->file);
    }
    else if (scratched_file != NULL && entries_line == NULL)
    {
	if (strcmp (scratched_file, finfo->file) != 0)
	    error (1, 0,
		   "CVS server internal error: `%s' vs. `%s' scratched",
		   scratched_file,
		   finfo->file);
	free (scratched_file);
	scratched_file = NULL;

	if (kill_scratched_file)
	    buf_output0 (protocol, "Removed ");
	else
	    buf_output0 (protocol, "Remove-entry ");
	output_dir (finfo->update_dir, finfo->repository);
	buf_output0 (protocol, finfo->file);
	buf_output (protocol, "\n", 1);
    }
    else if (scratched_file == NULL && entries_line == NULL)
    {
	/*
	 * This can happen with death support if we were processing
	 * a dead file in a checkout.
	 */
    }
    else
	error (1, 0,
	       "CVS server internal error: Register *and* Scratch_Entry.\n");
    buf_send_counted (protocol);
  done:;
}

/* Return whether we should send patches in RCS format.  */

int
server_use_rcs_diff ()
{
    return supported_response ("Rcs-diff");
}

void
server_set_entstat (update_dir, repository)
    char *update_dir;
    char *repository;
{
    static int set_static_supported = -1;
    if (set_static_supported == -1)
	set_static_supported = supported_response ("Set-static-directory");
    if (!set_static_supported) return;

    buf_output0 (protocol, "Set-static-directory ");
    output_dir (update_dir, repository);
    buf_output0 (protocol, "\n");
    buf_send_counted (protocol);
}

void
server_clear_entstat (update_dir, repository)
     char *update_dir;
     char *repository;
{
    static int clear_static_supported = -1;
    if (clear_static_supported == -1)
	clear_static_supported = supported_response ("Clear-static-directory");
    if (!clear_static_supported) return;

    if (noexec)
	return;

    buf_output0 (protocol, "Clear-static-directory ");
    output_dir (update_dir, repository);
    buf_output0 (protocol, "\n");
    buf_send_counted (protocol);
}

void
server_set_sticky (update_dir, repository, tag, date, nonbranch)
    char *update_dir;
    char *repository;
    char *tag;
    char *date;
    int nonbranch;
{
    static int set_sticky_supported = -1;

    assert (update_dir != NULL);

    if (set_sticky_supported == -1)
	set_sticky_supported = supported_response ("Set-sticky");
    if (!set_sticky_supported) return;

    if (noexec)
	return;

    if (tag == NULL && date == NULL)
    {
	buf_output0 (protocol, "Clear-sticky ");
	output_dir (update_dir, repository);
	buf_output0 (protocol, "\n");
    }
    else
    {
	buf_output0 (protocol, "Set-sticky ");
	output_dir (update_dir, repository);
	buf_output0 (protocol, "\n");
	if (tag != NULL)
	{
	    if (nonbranch)
		buf_output0 (protocol, "N");
	    else
		buf_output0 (protocol, "T");
	    buf_output0 (protocol, tag);
	}
	else
	{
	    buf_output0 (protocol, "D");
	    buf_output0 (protocol, date);
	}
	buf_output0 (protocol, "\n");
    }
    buf_send_counted (protocol);
}

struct template_proc_data
{
    char *update_dir;
    char *repository;
};

/* Here as a static until we get around to fixing Parse_Info to pass along
   a void * for it.  */
static struct template_proc_data *tpd;

static int
template_proc (repository, template)
    char *repository;
    char *template;
{
    FILE *fp;
    char buf[1024];
    size_t n;
    struct stat sb;
    struct template_proc_data *data = tpd;

    if (!supported_response ("Template"))
	/* Might want to warn the user that the rcsinfo feature won't work.  */
	return 0;
    buf_output0 (protocol, "Template ");
    output_dir (data->update_dir, data->repository);
    buf_output0 (protocol, "\n");

    fp = CVS_FOPEN (template, "rb");
    if (fp == NULL)
    {
	error (0, errno, "Couldn't open rcsinfo template file %s", template);
	return 1;
    }
    if (fstat (fileno (fp), &sb) < 0)
    {
	error (0, errno, "cannot stat rcsinfo template file %s", template);
	return 1;
    }
    sprintf (buf, "%ld\n", (long) sb.st_size);
    buf_output0 (protocol, buf);
    while (!feof (fp))
    {
	n = fread (buf, 1, sizeof buf, fp);
	buf_output (protocol, buf, n);
	if (ferror (fp))
	{
	    error (0, errno, "cannot read rcsinfo template file %s", template);
	    (void) fclose (fp);
	    return 1;
	}
    }
    if (fclose (fp) < 0)
	error (0, errno, "cannot close rcsinfo template file %s", template);
    return 0;
}

void
server_template (update_dir, repository)
    char *update_dir;
    char *repository;
{
    struct template_proc_data data;
    data.update_dir = update_dir;
    data.repository = repository;
    tpd = &data;
    (void) Parse_Info (CVSROOTADM_RCSINFO, repository, template_proc, 1);
}

static void
serve_gzip_contents (arg)
     char *arg;
{
    int level;
    level = atoi (arg);
    if (level == 0)
	level = 6;
    file_gzip_level = level;
}

static void
serve_gzip_stream (arg)
     char *arg;
{
    int level;
    level = atoi (arg);
    if (level == 0)
	level = 6;

    /* All further communication with the client will be compressed.  */

    buf_to_net = compress_buffer_initialize (buf_to_net, 0, level,
					     buf_to_net->memory_error);
    buf_from_net = compress_buffer_initialize (buf_from_net, 1, level,
					       buf_from_net->memory_error);
}

static void
serve_ignore (arg)
    char *arg;
{
    /*
     * Just ignore this command.  This is used to support the
     * update-patches command, which is not a real command, but a signal
     * to the client that update will accept the -u argument.
     */
}

static int
expand_proc (pargc, argv, where, mwhere, mfile, shorten,
	     local_specified, omodule, msg)
    int *pargc;
    char **argv;
    char *where;
    char *mwhere;
    char *mfile;
    int shorten;
    int local_specified;
    char *omodule;
    char *msg;
{
    int i;
    char *dir = argv[0];

    /* If mwhere has been specified, the thing we're expanding is a
       module -- just return its name so the client will ask for the
       right thing later.  If it is an alias or a real directory,
       mwhere will not be set, so send out the appropriate
       expansion. */

    if (mwhere != NULL)
    {
	buf_output0 (buf_to_net, "Module-expansion ");
	if (server_dir != NULL)
	{
	    buf_output0 (buf_to_net, server_dir);
	    buf_output0 (buf_to_net, "/");
	}
	buf_output0 (buf_to_net, mwhere);
	if (mfile != NULL)
	{
	    buf_append_char (buf_to_net, '/');
	    buf_output0 (buf_to_net, mfile);
	}
	buf_append_char (buf_to_net, '\n');
    }
    else
    {
	/* We may not need to do this anymore -- check the definition
           of aliases before removing */
	if (*pargc == 1)
	{
	    buf_output0 (buf_to_net, "Module-expansion ");
	    if (server_dir != NULL)
	    {
		buf_output0 (buf_to_net, server_dir);
		buf_output0 (buf_to_net, "/");
	    }
	    buf_output0 (buf_to_net, dir);
	    buf_append_char (buf_to_net, '\n');
	}
	else
	{
	    for (i = 1; i < *pargc; ++i)
	    {
	        buf_output0 (buf_to_net, "Module-expansion ");
		if (server_dir != NULL)
		{
		    buf_output0 (buf_to_net, server_dir);
		    buf_output0 (buf_to_net, "/");
		}
		buf_output0 (buf_to_net, dir);
		buf_append_char (buf_to_net, '/');
		buf_output0 (buf_to_net, argv[i]);
		buf_append_char (buf_to_net, '\n');
	    }
	}
    }
    return 0;
}

static void
serve_expand_modules (arg)
    char *arg;
{
    int i;
    int err;
    DBM *db;
    err = 0;

    server_expanding = 1;
    db = open_module ();
    for (i = 1; i < argument_count; i++)
	err += do_module (db, argument_vector[i],
			  CHECKOUT, "Updating", expand_proc,
			  NULL, 0, 0, 0,
			  (char *) NULL);
    close_module (db);
    server_expanding = 0;
    {
	/* argument_vector[0] is a dummy argument, we don't mess with it.  */
	char **cp;
	for (cp = argument_vector + 1;
	     cp < argument_vector + argument_count;
	     ++cp)
	    free (*cp);

	argument_count = 1;
    }
    if (err)
	/* We will have printed an error message already.  */
	buf_output0 (buf_to_net, "error  \n");
    else
	buf_output0 (buf_to_net, "ok\n");

    /* The client is waiting for the module expansions, so we must
       send the output now.  */
    buf_flush (buf_to_net, 1);
}

void
server_prog (dir, name, which)
    char *dir;
    char *name;
    enum progs which;
{
    if (!supported_response ("Set-checkin-prog"))
    {
	buf_output0 (buf_to_net, "E \
warning: this client does not support -i or -u flags in the modules file.\n");
	return;
    }
    switch (which)
    {
	case PROG_CHECKIN:
	    buf_output0 (buf_to_net, "Set-checkin-prog ");
	    break;
	case PROG_UPDATE:
	    buf_output0 (buf_to_net, "Set-update-prog ");
	    break;
    }
    buf_output0 (buf_to_net, dir);
    buf_append_char (buf_to_net, '\n');
    buf_output0 (buf_to_net, name);
    buf_append_char (buf_to_net, '\n');
}

static void
serve_checkin_prog (arg)
    char *arg;
{
    FILE *f;
    f = CVS_FOPEN (CVSADM_CIPROG, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_CIPROG)))
	    sprintf (pending_error_text, "E cannot open %s", CVSADM_CIPROG);
	return;
    }
    if (fprintf (f, "%s\n", arg) < 0)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_CIPROG)))
	    sprintf (pending_error_text,
		     "E cannot write to %s", CVSADM_CIPROG);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_CIPROG)))
	    sprintf (pending_error_text, "E cannot close %s", CVSADM_CIPROG);
	return;
    }
}

static void
serve_update_prog (arg)
    char *arg;
{
    FILE *f;

    /* Before we do anything we need to make sure we are not in readonly
       mode.  */
    if (!check_command_legal_p ("commit"))
    {
	/* I might be willing to make this a warning, except we lack the
	   machinery to do so.  */
	if (alloc_pending (80))
	    sprintf (pending_error_text, "\
E Flag -u in modules not allowed in readonly mode");
	return;
    }

    f = CVS_FOPEN (CVSADM_UPROG, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_UPROG)))
	    sprintf (pending_error_text, "E cannot open %s", CVSADM_UPROG);
	return;
    }
    if (fprintf (f, "%s\n", arg) < 0)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_UPROG)))
	    sprintf (pending_error_text, "E cannot write to %s", CVSADM_UPROG);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	if (alloc_pending (80 + strlen (CVSADM_UPROG)))
	    sprintf (pending_error_text, "E cannot close %s", CVSADM_UPROG);
	return;
    }
}

static void serve_valid_requests PROTO((char *arg));

#endif /* SERVER_SUPPORT */
#if defined(SERVER_SUPPORT) || defined(CLIENT_SUPPORT)

/*
 * Parts of this table are shared with the client code,
 * but the client doesn't need to know about the handler
 * functions.
 */

struct request requests[] =
{
#ifdef SERVER_SUPPORT
#define REQ_LINE(n, f, s) {n, f, s}
#else
#define REQ_LINE(n, f, s) {n, s}
#endif

  REQ_LINE("Root", serve_root, rq_essential),
  REQ_LINE("Valid-responses", serve_valid_responses, rq_essential),
  REQ_LINE("valid-requests", serve_valid_requests, rq_essential),
  REQ_LINE("Repository", serve_repository, rq_optional),
  REQ_LINE("Directory", serve_directory, rq_essential),
  REQ_LINE("Max-dotdot", serve_max_dotdot, rq_optional),
  REQ_LINE("Static-directory", serve_static_directory, rq_optional),
  REQ_LINE("Sticky", serve_sticky, rq_optional),
  REQ_LINE("Checkin-prog", serve_checkin_prog, rq_optional),
  REQ_LINE("Update-prog", serve_update_prog, rq_optional),
  REQ_LINE("Entry", serve_entry, rq_essential),
  REQ_LINE("Kopt", serve_kopt, rq_optional),
  REQ_LINE("Modified", serve_modified, rq_essential),
  REQ_LINE("Is-modified", serve_is_modified, rq_optional),

  /* The client must send this request to interoperate with CVS 1.5
     through 1.9 servers.  The server must support it (although it can
     be and is a noop) to interoperate with CVS 1.5 to 1.9 clients.  */
  REQ_LINE("UseUnchanged", serve_enable_unchanged, rq_enableme),

  REQ_LINE("Unchanged", serve_unchanged, rq_essential),
  REQ_LINE("Notify", serve_notify, rq_optional),
  REQ_LINE("Questionable", serve_questionable, rq_optional),
  REQ_LINE("Case", serve_case, rq_optional),
  REQ_LINE("Argument", serve_argument, rq_essential),
  REQ_LINE("Argumentx", serve_argumentx, rq_essential),
  REQ_LINE("Global_option", serve_global_option, rq_optional),
  REQ_LINE("Gzip-stream", serve_gzip_stream, rq_optional),
  REQ_LINE("Set", serve_set, rq_optional),
#ifdef ENCRYPTION
#  ifdef HAVE_KERBEROS
  REQ_LINE("Kerberos-encrypt", serve_kerberos_encrypt, rq_optional),
#  endif
#  ifdef HAVE_GSSAPI
  REQ_LINE("Gssapi-encrypt", serve_gssapi_encrypt, rq_optional),
#  endif
#endif
#ifdef HAVE_GSSAPI
  REQ_LINE("Gssapi-authenticate", serve_gssapi_authenticate, rq_optional),
#endif
  REQ_LINE("expand-modules", serve_expand_modules, rq_optional),
  REQ_LINE("ci", serve_ci, rq_essential),
  REQ_LINE("co", serve_co, rq_essential),
  REQ_LINE("update", serve_update, rq_essential),
  REQ_LINE("diff", serve_diff, rq_optional),
  REQ_LINE("log", serve_log, rq_optional),
  REQ_LINE("add", serve_add, rq_optional),
  REQ_LINE("remove", serve_remove, rq_optional),
  REQ_LINE("update-patches", serve_ignore, rq_optional),
  REQ_LINE("gzip-file-contents", serve_gzip_contents, rq_optional),
  REQ_LINE("status", serve_status, rq_optional),
  REQ_LINE("rdiff", serve_rdiff, rq_optional),
  REQ_LINE("tag", serve_tag, rq_optional),
  REQ_LINE("rtag", serve_rtag, rq_optional),
  REQ_LINE("import", serve_import, rq_optional),
  REQ_LINE("admin", serve_admin, rq_optional),
  REQ_LINE("export", serve_export, rq_optional),
  REQ_LINE("history", serve_history, rq_optional),
  REQ_LINE("release", serve_release, rq_optional),
  REQ_LINE("watch-on", serve_watch_on, rq_optional),
  REQ_LINE("watch-off", serve_watch_off, rq_optional),
  REQ_LINE("watch-add", serve_watch_add, rq_optional),
  REQ_LINE("watch-remove", serve_watch_remove, rq_optional),
  REQ_LINE("watchers", serve_watchers, rq_optional),
  REQ_LINE("editors", serve_editors, rq_optional),
  REQ_LINE("init", serve_init, rq_optional),
  REQ_LINE("annotate", serve_annotate, rq_optional),
  REQ_LINE("noop", serve_noop, rq_optional),
  REQ_LINE(NULL, NULL, rq_optional)

#undef REQ_LINE
};

#endif /* SERVER_SUPPORT or CLIENT_SUPPORT */
#ifdef SERVER_SUPPORT

static void
serve_valid_requests (arg)
     char *arg;
{
    struct request *rq;
    if (print_pending_error ())
	return;
    buf_output0 (buf_to_net, "Valid-requests");
    for (rq = requests; rq->name != NULL; rq++)
    {
	if (rq->func != NULL)
	{
	    buf_append_char (buf_to_net, ' ');
	    buf_output0 (buf_to_net, rq->name);
	}
    }
    buf_output0 (buf_to_net, "\nok\n");

    /* The client is waiting for the list of valid requests, so we
       must send the output now.  */
    buf_flush (buf_to_net, 1);
}

#ifdef sun
/*
 * Delete temporary files.  SIG is the signal making this happen, or
 * 0 if not called as a result of a signal.
 */
static int command_pid_is_dead;
static void wait_sig (sig)
     int sig;
{
    int status;
    pid_t r = wait (&status);
    if (r == command_pid)
	command_pid_is_dead++;
}
#endif

void
server_cleanup (sig)
    int sig;
{
    /* Do "rm -rf" on the temp directory.  */
    int status;
    int save_noexec;

    if (buf_to_net != NULL)
    {
	/* FIXME: If this is not the final call from server, this
	   could deadlock, because the client might be blocked writing
	   to us.  This should not be a problem in practice, because
	   we do not generate much output when the client is not
	   waiting for it.  */
	set_block (buf_to_net);
	buf_flush (buf_to_net, 1);

	/* The calls to buf_shutdown are currently only meaningful
	   when we are using compression.  First we shut down
	   BUF_FROM_NET.  That will pick up the checksum generated
	   when the client shuts down its buffer.  Then, after we have
	   generated any final output, we shut down BUF_TO_NET.  */

	status = buf_shutdown (buf_from_net);
	if (status != 0)
	{
	    error (0, status, "shutting down buffer from client");
	    buf_flush (buf_to_net, 1);
	}
    }

    if (dont_delete_temp)
    {
	if (buf_to_net != NULL)
	    (void) buf_shutdown (buf_to_net);
	return;
    }

    /* What a bogus kludge.  This disgusting code makes all kinds of
       assumptions about SunOS, and is only for a bug in that system.
       So only enable it on Suns.  */
#ifdef sun
    if (command_pid > 0)
    {
	/* To avoid crashes on SunOS due to bugs in SunOS tmpfs
	   triggered by the use of rename() in RCS, wait for the
	   subprocess to die.  Unfortunately, this means draining output
	   while waiting for it to unblock the signal we sent it.  Yuck!  */
	int status;
	pid_t r;

	signal (SIGCHLD, wait_sig);
	if (sig)
	    /* Perhaps SIGTERM would be more correct.  But the child
	       process will delay the SIGINT delivery until its own
	       children have exited.  */
	    kill (command_pid, SIGINT);
	/* The caller may also have sent a signal to command_pid, so
	   always try waiting.  First, though, check and see if it's still
	   there....  */
    do_waitpid:
	r = waitpid (command_pid, &status, WNOHANG);
	if (r == 0)
	    ;
	else if (r == command_pid)
	    command_pid_is_dead++;
	else if (r == -1)
	    switch (errno)
	    {
		case ECHILD:
		    command_pid_is_dead++;
		    break;
		case EINTR:
		    goto do_waitpid;
	    }
	else
	    /* waitpid should always return one of the above values */
	    abort ();
	while (!command_pid_is_dead)
	{
	    struct timeval timeout;
	    struct fd_set_wrapper readfds;
	    char buf[100];
	    int i;

	    /* Use a non-zero timeout to avoid eating up CPU cycles.  */
	    timeout.tv_sec = 2;
	    timeout.tv_usec = 0;
	    readfds = command_fds_to_drain;
	    switch (select (max_command_fd + 1, &readfds.fds,
			    (fd_set *)0, (fd_set *)0,
			    &timeout))
	    {
		case -1:
		    if (errno != EINTR)
			abort ();
		case 0:
		    /* timeout */
		    break;
		case 1:
		    for (i = 0; i <= max_command_fd; i++)
		    {
			if (!FD_ISSET (i, &readfds.fds))
			    continue;
			/* this fd is non-blocking */
			while (read (i, buf, sizeof (buf)) >= 1)
			    ;
		    }
		    break;
		default:
		    abort ();
	    }
	}
    }
#endif

    CVS_CHDIR (Tmpdir);
    /* Temporarily clear noexec, so that we clean up our temp directory
       regardless of it (this could more cleanly be handled by moving
       the noexec check to all the unlink_file_dir callers from
       unlink_file_dir itself).  */
    save_noexec = noexec;
    noexec = 0;
    /* FIXME?  Would be nice to not ignore errors.  But what should we do?
       We could try to do this before we shut down the network connection,
       and try to notify the client (but the client might not be waiting
       for responses).  We could try something like syslog() or our own
       log file.  */
    unlink_file_dir (orig_server_temp_dir);
    noexec = save_noexec;

    if (buf_to_net != NULL)
	(void) buf_shutdown (buf_to_net);
}

int server_active = 0;
int server_expanding = 0;

int
server (argc, argv)
     int argc;
     char **argv;
{
    if (argc == -1)
    {
	static const char *const msg[] =
	{
	    "Usage: %s %s\n",
	    "  Normally invoked by a cvs client on a remote machine.\n",
	    NULL
	};
	usage (msg);
    }
    /* Ignore argc and argv.  They might be from .cvsrc.  */

    buf_to_net = fd_buffer_initialize (STDOUT_FILENO, 0,
				       outbuf_memory_error);
    buf_from_net = stdio_buffer_initialize (stdin, 1, outbuf_memory_error);

    saved_output = buf_nonio_initialize (outbuf_memory_error);
    saved_outerr = buf_nonio_initialize (outbuf_memory_error);

    /* Since we're in the server parent process, error should use the
       protocol to report error messages.  */
    error_use_protocol = 1;

    /* OK, now figure out where we stash our temporary files.  */
    {
	char *p;

	/* The code which wants to chdir into server_temp_dir is not set
	   up to deal with it being a relative path.  So give an error
	   for that case.  */
	if (!isabsolute (Tmpdir))
	{
	    pending_error_text = malloc (80 + strlen (Tmpdir));
	    if (pending_error_text == NULL)
	    {
		pending_error = ENOMEM;
	    }
	    else
	    {
		sprintf (pending_error_text,
			 "E Value of %s for TMPDIR is not absolute", Tmpdir);
	    }
	    /* FIXME: we would like this error to be persistent, that
	       is, not cleared by print_pending_error.  The current client
	       will exit as soon as it gets an error, but the protocol spec
	       does not require a client to do so.  */
	}
	else
	{
	    int status;

	    server_temp_dir = malloc (strlen (Tmpdir) + 80);
	    if (server_temp_dir == NULL)
	    {
		/*
		 * Strictly speaking, we're not supposed to output anything
		 * now.  But we're about to exit(), give it a try.
		 */
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
	    strcpy (server_temp_dir, Tmpdir);

	    /* Remove a trailing slash from TMPDIR if present.  */
	    p = server_temp_dir + strlen (server_temp_dir) - 1;
	    if (*p == '/')
		*p = '\0';

	    /*
	     * I wanted to use cvs-serv/PID, but then you have to worry about
	     * the permissions on the cvs-serv directory being right.  So
	     * use cvs-servPID.
	     */
	    strcat (server_temp_dir, "/cvs-serv");

	    p = server_temp_dir + strlen (server_temp_dir);
	    sprintf (p, "%ld", (long) getpid ());

	    orig_server_temp_dir = server_temp_dir;

	    /* Create the temporary directory, and set the mode to
               700, to discourage random people from tampering with
               it.  */
	    status = mkdir_p (server_temp_dir);
	    if (status != 0 && status != EEXIST)
	    {
		if (alloc_pending (80))
		    strcpy (pending_error_text,
			    "E can't create temporary directory");
		pending_error = status;
	    }
#ifndef CHMOD_BROKEN
	    else
	    {
		if (chmod (server_temp_dir, S_IRWXU) < 0)
		{
		    int save_errno = errno;
		    if (alloc_pending (80))
			strcpy (pending_error_text, "\
E cannot change permissions on temporary directory");
		    pending_error = save_errno;
		}
	    }
#endif
	}
    }

#ifdef SIGHUP
    (void) SIG_register (SIGHUP, server_cleanup);
#endif
#ifdef SIGINT
    (void) SIG_register (SIGINT, server_cleanup);
#endif
#ifdef SIGQUIT
    (void) SIG_register (SIGQUIT, server_cleanup);
#endif
#ifdef SIGPIPE
    (void) SIG_register (SIGPIPE, server_cleanup);
#endif
#ifdef SIGTERM
    (void) SIG_register (SIGTERM, server_cleanup);
#endif

    /* Now initialize our argument vector (for arguments from the client).  */

    /* Small for testing.  */
    argument_vector_size = 1;
    argument_vector =
	(char **) malloc (argument_vector_size * sizeof (char *));
    if (argument_vector == NULL)
    {
	/*
	 * Strictly speaking, we're not supposed to output anything
	 * now.  But we're about to exit(), give it a try.
	 */
	printf ("E Fatal server error, aborting.\n\
error ENOMEM Virtual memory exhausted.\n");

	/* I'm doing this manually rather than via error_exit ()
	   because I'm not sure whether we want to call server_cleanup.
	   Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif

	exit (EXIT_FAILURE);
    }

    argument_count = 1;
    /* This gets printed if the client supports an option which the
       server doesn't, causing the server to print a usage message.
       FIXME: probably should be using program_name here.
       FIXME: just a nit, I suppose, but the usage message the server
       prints isn't literally true--it suggests "cvs server" followed
       by options which are for a particular command.  Might be nice to
       say something like "client apparently supports an option not supported
       by this server" or something like that instead of usage message.  */
    argument_vector[0] = "cvs server";

    while (1)
    {
	char *cmd, *orig_cmd;
	struct request *rq;
	int status;
	
	status = buf_read_line (buf_from_net, &cmd, (int *) NULL);
	if (status == -2)
	{
	    buf_output0 (buf_to_net, "E Fatal server error, aborting.\n\
error ENOMEM Virtual memory exhausted.\n");
	    break;
	}
	if (status != 0)
	    break;

	orig_cmd = cmd;
	for (rq = requests; rq->name != NULL; ++rq)
	    if (strncmp (cmd, rq->name, strlen (rq->name)) == 0)
	    {
		int len = strlen (rq->name);
		if (cmd[len] == '\0')
		    cmd += len;
		else if (cmd[len] == ' ')
		    cmd += len + 1;
		else
		    /*
		     * The first len characters match, but it's a different
		     * command.  e.g. the command is "cooperate" but we matched
		     * "co".
		     */
		    continue;
		(*rq->func) (cmd);
		break;
	    }
	if (rq->name == NULL)
	{
	    if (!print_pending_error ())
	    {
	        buf_output0 (buf_to_net, "error  unrecognized request `");
		buf_output0 (buf_to_net, cmd);
		buf_append_char (buf_to_net, '\'');
		buf_append_char (buf_to_net, '\n');
	    }
	}
	free (orig_cmd);
    }
    server_cleanup (0);
    return 0;
}


#if defined (HAVE_KERBEROS) || defined (AUTH_SERVER_SUPPORT) || defined (HAVE_GSSAPI)
static void switch_to_user PROTO((const char *));

static void
switch_to_user (username)
    const char *username;
{
    struct passwd *pw;

    pw = getpwnam (username);
    if (pw == NULL)
    {
	printf ("E Fatal error, aborting.\n\
error 0 %s: no such user\n", username);
	/* I'm doing this manually rather than via error_exit ()
	   because I'm not sure whether we want to call server_cleanup.
	   Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif

	exit (EXIT_FAILURE);
    }

    /* FIXME?  We don't check for errors from initgroups, setuid, &c.
       I think this mainly would come up if someone is trying to run
       the server as a non-root user.  I think we should be checking for
       errors and aborting (as with the error above from getpwnam) if
       there is an error (presumably EPERM).  That means that pserver
       should continue to work right if all of the "system usernames"
       in CVSROOT/passwd match the user which the server is being run
       as (in inetd.conf), but fail otherwise.  */

#if HAVE_INITGROUPS
    initgroups (pw->pw_name, pw->pw_gid);
#endif /* HAVE_INITGROUPS */

#ifdef SETXID_SUPPORT
    /* honor the setgid bit iff set*/
    if (getgid() != getegid())
    {
	setgid (getegid ());
    }
    else
#else
    {
	setgid (pw->pw_gid);
    }
#endif
    
    setuid (pw->pw_uid);
    /* We don't want our umask to change file modes.  The modes should
       be set by the modes used in the repository, and by the umask of
       the client.  */
    umask (0);

#if HAVE_PUTENV
    /* Set LOGNAME and USER in the environment, in case they are
       already set to something else.  */
    {
	char *env;

	env = xmalloc (sizeof "LOGNAME=" + strlen (username));
	(void) sprintf (env, "LOGNAME=%s", username);
	(void) putenv (env);

	env = xmalloc (sizeof "USER=" + strlen (username));
	(void) sprintf (env, "USER=%s", username);
	(void) putenv (env);
    }
#endif /* HAVE_PUTENV */
}
#endif

#ifdef AUTH_SERVER_SUPPORT

extern char *crypt PROTO((const char *, const char *));


/* 
 * 0 means no entry found for this user.
 * 1 means entry found and password matches.
 * 2 means entry found, but password does not match.
 *
 * If success, host_user_ptr will be set to point at the system
 * username (i.e., the "real" identity, which may or may not be the
 * CVS username) of this user; caller may free this.  Global
 * CVS_Username will point at an allocated copy of cvs username (i.e.,
 * the username argument below).
 */
static int
check_repository_password (username, password, repository, host_user_ptr)
     char *username, *password, *repository, **host_user_ptr;
{
    int retval = 0;
    FILE *fp;
    char *filename;
    char *linebuf = NULL;
    size_t linebuf_len;
    int found_it = 0;
    int namelen;

    /* We don't use CVSroot_directory because it hasn't been set yet
     * -- our `repository' argument came from the authentication
     * protocol, not the regular CVS protocol.
     */

    filename = xmalloc (strlen (repository)
			+ 1
			+ strlen (CVSROOTADM)
			+ 1
			+ strlen (CVSROOTADM_PASSWD)
			+ 1);

    (void) sprintf (filename, "%s/%s/%s", repository,
                    CVSROOTADM, CVSROOTADM_PASSWD);

    fp = CVS_FOPEN (filename, "r");
    if (fp == NULL)
    {
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", filename);
	return 0;
    }

    /* Look for a relevant line -- one with this user's name. */
    namelen = strlen (username);
    while (getline (&linebuf, &linebuf_len, fp) >= 0)
    {
	if ((strncmp (linebuf, username, namelen) == 0)
	    && (linebuf[namelen] == ':'))
        {
	    found_it = 1;
	    break;
        }
    }
    if (ferror (fp))
	error (0, errno, "cannot read %s", filename);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", filename);

    /* If found_it != 0, then linebuf contains the information we need. */
    if (found_it)
    {
	char *found_password, *host_user_tmp;

	strtok (linebuf, ":");
	found_password = strtok (NULL, ": \n");
	host_user_tmp = strtok (NULL, ": \n");
	if (host_user_tmp == NULL)
            host_user_tmp = username;

	if (strcmp (found_password, crypt (password, found_password)) == 0)
        {
            /* Give host_user_ptr permanent storage. */
            *host_user_ptr = xstrdup (host_user_tmp);
	    retval = 1;
        }
	else
        {
            *host_user_ptr = NULL;
	    retval         = 2;
        }
    }
    else
    {
	*host_user_ptr = NULL;
	retval = 0;
    }

    free (filename);
    if (linebuf)
        free (linebuf);

    return retval;
}


/* Return a hosting username if password matches, else NULL. */
static char *
check_password (username, password, repository)
    char *username, *password, *repository;
{
    int rc;
    char *host_user = NULL;

    /* First we see if this user has a password in the CVS-specific
       password file.  If so, that's enough to authenticate with.  If
       not, we'll check /etc/passwd. */

    rc = check_repository_password (username, password, repository,
				    &host_user);

    if (rc == 2)
	return NULL;

    /* else */

    if (rc == 1)
    {
        /* host_user already set by reference, so just return. */
        goto handle_return;
    }
    else if (rc == 0 && system_auth)
    {
	/* No cvs password found, so try /etc/passwd. */

	const char *found_passwd = NULL;
#ifdef HAVE_GETSPNAM
	struct spwd *pw;

	pw = getspnam (username);
	if (pw != NULL)
	{
	    found_passwd = pw->sp_pwdp;
	}
#else
	struct passwd *pw;

	pw = getpwnam (username);
	if (pw != NULL)
	{
	    found_passwd = pw->pw_passwd;
	}
#endif
	
	if (pw == NULL)
	{
	    printf ("E Fatal error, aborting.\n\
error 0 %s: no such user\n", username);

	    /* I'm doing this manually rather than via error_exit ()
	       because I'm not sure whether we want to call server_cleanup.
	       Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	    /* Hook for OS-specific behavior, for example socket subsystems on
	       NT and OS2 or dealing with windows and arguments on Mac.  */
	    SYSTEM_CLEANUP ();
#endif

	    exit (EXIT_FAILURE);
	}
	
	if (found_passwd && *found_passwd)
        {
	    host_user = ((! strcmp (found_passwd,
                                    crypt (password, found_passwd)))
                         ? username : NULL);
            goto handle_return;
        }
	else if (password && *password)
        {
	    host_user = username;
            goto handle_return;
        }
	else
        {
	    host_user = NULL;
            goto handle_return;
        }
    }
    else if (rc == 0)
    {
	/* Note that the message _does_ distinguish between the case in
	   which we check for a system password and the case in which
	   we do not.  It is a real pain to track down why it isn't
	   letting you in if it won't say why, and I am not convinced
	   that the potential information disclosure to an attacker
	   outweighs this.  */
	printf ("error 0 no such user %s in CVSROOT/passwd\n", username);

	/* I'm doing this manually rather than via error_exit ()
	   because I'm not sure whether we want to call server_cleanup.
	   Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif
	exit (EXIT_FAILURE);
    }
    else
    {
	/* Something strange happened.  We don't know what it was, but
	   we certainly won't grant authorization. */
	host_user = NULL;
        goto handle_return;
    }

handle_return:
    if (host_user)
    {
        /* Set CVS_Username here, in allocated space. 
           It might or might not be the same as host_user. */
        CVS_Username = xmalloc (strlen (username) + 1);
        strcpy (CVS_Username, username);
    }

    return host_user;
}

#endif /* AUTH_SERVER_SUPPORT */

#if defined (AUTH_SERVER_SUPPORT) || defined (HAVE_GSSAPI)

/* Read username and password from client (i.e., stdin).
   If correct, then switch to run as that user and send an ACK to the
   client via stdout, else send NACK and die. */
void
pserver_authenticate_connection ()
{
    char *tmp = NULL;
    size_t tmp_allocated = 0;
#ifdef AUTH_SERVER_SUPPORT
    char *repository = NULL;
    size_t repository_allocated = 0;
    char *username = NULL;
    size_t username_allocated = 0;
    char *password = NULL;
    size_t password_allocated = 0;

    char *host_user;
    char *descrambled_password;
#endif /* AUTH_SERVER_SUPPORT */
    int verify_and_exit = 0;

    /* The Authentication Protocol.  Client sends:
     *
     *   BEGIN AUTH REQUEST\n
     *   <REPOSITORY>\n
     *   <USERNAME>\n
     *   <PASSWORD>\n
     *   END AUTH REQUEST\n
     *
     * Server uses above information to authenticate, then sends
     *
     *   I LOVE YOU\n
     *
     * if it grants access, else
     *
     *   I HATE YOU\n
     *
     * if it denies access (and it exits if denying).
     *
     * When the client is "cvs login", the user does not desire actual
     * repository access, but would like to confirm the password with
     * the server.  In this case, the start and stop strings are
     *
     *   BEGIN VERIFICATION REQUEST\n
     *
     *            and
     *
     *   END VERIFICATION REQUEST\n
     *
     * On a verification request, the server's responses are the same
     * (with the obvious semantics), but it exits immediately after
     * sending the response in both cases.
     *
     * Why is the repository sent?  Well, note that the actual
     * client/server protocol can't start up until authentication is
     * successful.  But in order to perform authentication, the server
     * needs to look up the password in the special CVS passwd file,
     * before trying /etc/passwd.  So the client transmits the
     * repository as part of the "authentication protocol".  The
     * repository will be redundantly retransmitted later, but that's no
     * big deal.
     */

#ifdef SO_KEEPALIVE
    /* Set SO_KEEPALIVE on the socket, so that we don't hang forever
       if the client dies while we are waiting for input.  */
    {
	int on = 1;

	(void) setsockopt (STDIN_FILENO, SOL_SOCKET, SO_KEEPALIVE,
			   (char *) &on, sizeof on);
    }
#endif

    /* Make sure the protocol starts off on the right foot... */
    if (getline (&tmp, &tmp_allocated, stdin) < 0)
	/* FIXME: what?  We could try writing error/eof, but chances
	   are the network connection is dead bidirectionally.  log it
	   somewhere?  */
	;

    if (strcmp (tmp, "BEGIN VERIFICATION REQUEST\n") == 0)
	verify_and_exit = 1;
    else if (strcmp (tmp, "BEGIN AUTH REQUEST\n") == 0)
	;
    else if (strcmp (tmp, "BEGIN GSSAPI REQUEST\n") == 0)
    {
#ifdef HAVE_GSSAPI
	free (tmp);
	gserver_authenticate_connection ();
	return;
#else
	error (1, 0, "GSSAPI authentication not supported by this server");
#endif
    }
    else
	error (1, 0, "bad auth protocol start: %s", tmp);

#ifndef AUTH_SERVER_SUPPORT

    error (1, 0, "Password authentication not supported by this server");

#else /* AUTH_SERVER_SUPPORT */

    /* Get the three important pieces of information in order. */
    /* See above comment about error handling.  */
    getline (&repository, &repository_allocated, stdin);
    getline (&username, &username_allocated, stdin);
    getline (&password, &password_allocated, stdin);

    /* Make them pure. */ 
    strip_trailing_newlines (repository);
    strip_trailing_newlines (username);
    strip_trailing_newlines (password);

    /* ... and make sure the protocol ends on the right foot. */
    /* See above comment about error handling.  */
    getline (&tmp, &tmp_allocated, stdin);
    if (strcmp (tmp,
		verify_and_exit ?
		"END VERIFICATION REQUEST\n" : "END AUTH REQUEST\n")
	!= 0)
    {
	error (1, 0, "bad auth protocol end: %s", tmp);
    }
    if (!root_allow_ok (repository))
	/* Just give a generic I HATE YOU.  This is because CVS 1.9.10
	   and older clients do not support "error".  Once more recent
	   clients are more widespread, probably want to fix this (it is
	   a real pain to track down why it isn't letting you in if it
	   won't say why, and I am not convinced that the potential
	   information disclosure to an attacker outweighs this).  */
	goto i_hate_you;

    /* OK, now parse the config file, so we can use it to control how
       to check passwords.  If there was an error parsing the config
       file, parse_config already printed an error.  We keep going.
       Why?  Because if we didn't, then there would be no way to check
       in a new CVSROOT/config file to fix the broken one!  */
    parse_config (repository);

    /* We need the real cleartext before we hash it. */
    descrambled_password = descramble (password);
    host_user = check_password (username, descrambled_password, repository);
    memset (descrambled_password, 0, strlen (descrambled_password));
    free (descrambled_password);
    if (host_user)
    {
	printf ("I LOVE YOU\n");
	fflush (stdout);
    }
    else
    {
    i_hate_you:
	printf ("I HATE YOU\n");
	fflush (stdout);
	/* I'm doing this manually rather than via error_exit ()
	   because I'm not sure whether we want to call server_cleanup.
	   Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif

	exit (EXIT_FAILURE);
    }

    /* Don't go any farther if we're just responding to "cvs login". */
    if (verify_and_exit)
    {
#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif

	exit (0);
    }

    /* Set Pserver_Repos so that we can check later that the same
       repository is sent in later client/server protocol. */
    Pserver_Repos = xmalloc (strlen (repository) + 1);
    strcpy (Pserver_Repos, repository);

    /* Switch to run as this user. */
    switch_to_user (host_user);
    free (tmp);
    free (repository);
    free (username);
    free (password);

#endif /* AUTH_SERVER_SUPPORT */
}

#endif /* AUTH_SERVER_SUPPORT || HAVE_GSSAPI */


#ifdef HAVE_KERBEROS
void
kserver_authenticate_connection ()
{
    int status;
    char instance[INST_SZ];
    struct sockaddr_in peer;
    struct sockaddr_in laddr;
    int len;
    KTEXT_ST ticket;
    AUTH_DAT auth;
    char version[KRB_SENDAUTH_VLEN];
    char user[ANAME_SZ];

    strcpy (instance, "*");
    len = sizeof peer;
    if (getpeername (STDIN_FILENO, (struct sockaddr *) &peer, &len) < 0
	|| getsockname (STDIN_FILENO, (struct sockaddr *) &laddr,
			&len) < 0)
    {
	printf ("E Fatal error, aborting.\n\
error %s getpeername or getsockname failed\n", strerror (errno));
#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif
	exit (EXIT_FAILURE);
    }

#ifdef SO_KEEPALIVE
    /* Set SO_KEEPALIVE on the socket, so that we don't hang forever
       if the client dies while we are waiting for input.  */
    {
	int on = 1;

	(void) setsockopt (STDIN_FILENO, SOL_SOCKET, SO_KEEPALIVE,
			   (char *) &on, sizeof on);
    }
#endif

    status = krb_recvauth (KOPT_DO_MUTUAL, STDIN_FILENO, &ticket, "rcmd",
			   instance, &peer, &laddr, &auth, "", sched,
			   version);
    if (status != KSUCCESS)
    {
	printf ("E Fatal error, aborting.\n\
error 0 kerberos: %s\n", krb_get_err_text(status));
#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif
	exit (EXIT_FAILURE);
    }

    memcpy (kblock, auth.session, sizeof (C_Block));

    /* Get the local name.  */
    status = krb_kntoln (&auth, user);
    if (status != KSUCCESS)
    {
	printf ("E Fatal error, aborting.\n\
error 0 kerberos: can't get local name: %s\n", krb_get_err_text(status));
#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif
	exit (EXIT_FAILURE);
    }

    /* Switch to run as this user. */
    switch_to_user (user);
}
#endif /* HAVE_KERBEROS */

#ifdef HAVE_GSSAPI

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN (256)
#endif

/* Authenticate a GSSAPI connection.  This is called from
   pserver_authenticate_connection, and it handles success and failure
   the same way.  */

static void
gserver_authenticate_connection ()
{
    char hostname[MAXHOSTNAMELEN];
    struct hostent *hp;
    gss_buffer_desc tok_in, tok_out;
    char buf[1024];
    OM_uint32 stat_min, ret;
    gss_name_t server_name, client_name;
    gss_cred_id_t server_creds;
    int nbytes;
    gss_OID mechid;

    gethostname (hostname, sizeof hostname);
    hp = gethostbyname (hostname);
    if (hp == NULL)
	error (1, 0, "can't get canonical hostname");

    sprintf (buf, "cvs@%s", hp->h_name);
    tok_in.value = buf;
    tok_in.length = strlen (buf);

    if (gss_import_name (&stat_min, &tok_in, gss_nt_service_name,
			 &server_name) != GSS_S_COMPLETE)
	error (1, 0, "could not import GSSAPI service name %s", buf);

    /* Acquire the server credential to verify the client's
       authentication.  */
    if (gss_acquire_cred (&stat_min, server_name, 0, GSS_C_NULL_OID_SET,
			  GSS_C_ACCEPT, &server_creds,
			  NULL, NULL) != GSS_S_COMPLETE)
	error (1, 0, "could not acquire GSSAPI server credentials");

    gss_release_name (&stat_min, &server_name);

    /* The client will send us a two byte length followed by that many
       bytes.  */
    if (fread (buf, 1, 2, stdin) != 2)
	error (1, errno, "read of length failed");

    nbytes = ((buf[0] & 0xff) << 8) | (buf[1] & 0xff);
    assert (nbytes <= sizeof buf);

    if (fread (buf, 1, nbytes, stdin) != nbytes)
	error (1, errno, "read of data failed");

    gcontext = GSS_C_NO_CONTEXT;
    tok_in.length = nbytes;
    tok_in.value = buf;

    if (gss_accept_sec_context (&stat_min,
                                &gcontext,	/* context_handle */
                                server_creds,	/* verifier_cred_handle */
                                &tok_in,	/* input_token */
                                NULL,		/* channel bindings */
                                &client_name,	/* src_name */
                                &mechid,	/* mech_type */
                                &tok_out,	/* output_token */
                                &ret,
                                NULL,	 	/* ignore time_rec */
                                NULL)		/* ignore del_cred_handle */
	!= GSS_S_COMPLETE)
    {
	error (1, 0, "could not verify credentials");
    }

    /* FIXME: Use Kerberos v5 specific code to authenticate to a user.
       We could instead use an authentication to access mapping.  */
    {
	krb5_context kc;
	krb5_principal p;
	gss_buffer_desc desc;

	krb5_init_context (&kc);
	if (gss_display_name (&stat_min, client_name, &desc,
			      &mechid) != GSS_S_COMPLETE
	    || krb5_parse_name (kc, ((gss_buffer_t) &desc)->value, &p) != 0
	    || krb5_aname_to_localname (kc, p, sizeof buf, buf) != 0
	    || krb5_kuserok (kc, p, buf) != TRUE)
	{
	    error (1, 0, "access denied");
	}
	krb5_free_principal (kc, p);
	krb5_free_context (kc);
    }

    if (tok_out.length != 0)
    {
	char cbuf[2];

	cbuf[0] = (tok_out.length >> 8) & 0xff;
	cbuf[1] = tok_out.length & 0xff;
	if (fwrite (cbuf, 1, 2, stdout) != 2
	    || (fwrite (tok_out.value, 1, tok_out.length, stdout)
		!= tok_out.length))
	    error (1, errno, "fwrite failed");
    }

    switch_to_user (buf);

    printf ("I LOVE YOU\n");
    fflush (stdout);
}

#endif /* HAVE_GSSAPI */

#endif /* SERVER_SUPPORT */

#if defined (CLIENT_SUPPORT) || defined (SERVER_SUPPORT)

/* This global variable is non-zero if the user requests encryption on
   the command line.  */
int cvsencrypt;

/* This global variable is non-zero if the users requests stream
   authentication on the command line.  */
int cvsauthenticate;

#ifdef HAVE_GSSAPI

/* An buffer interface using GSSAPI.  This is built on top of a
   packetizing buffer.  */

/* This structure is the closure field of the GSSAPI translation
   routines.  */

struct cvs_gssapi_wrap_data
{
    /* The GSSAPI context.  */
    gss_ctx_id_t gcontext;
};

static int cvs_gssapi_wrap_input PROTO((void *, const char *, char *, int));
static int cvs_gssapi_wrap_output PROTO((void *, const char *, char *, int,
					 int *));

/* Create a GSSAPI wrapping buffer.  We use a packetizing buffer with
   GSSAPI wrapping routines.  */

struct buffer *
cvs_gssapi_wrap_buffer_initialize (buf, input, gcontext, memory)
     struct buffer *buf;
     int input;
     gss_ctx_id_t gcontext;
     void (*memory) PROTO((struct buffer *));
{
    struct cvs_gssapi_wrap_data *gd;

    gd = (struct cvs_gssapi_wrap_data *) xmalloc (sizeof *gd);
    gd->gcontext = gcontext;

    return (packetizing_buffer_initialize
	    (buf,
	     input ? cvs_gssapi_wrap_input : NULL,
	     input ? NULL : cvs_gssapi_wrap_output,
	     gd,
	     memory));
}

/* Unwrap data using GSSAPI.  */

static int
cvs_gssapi_wrap_input (fnclosure, input, output, size)
     void *fnclosure;
     const char *input;
     char *output;
     int size;
{
    struct cvs_gssapi_wrap_data *gd =
	(struct cvs_gssapi_wrap_data *) fnclosure;
    gss_buffer_desc inbuf, outbuf;
    OM_uint32 stat_min;
    int conf;

    inbuf.value = (void *) input;
    inbuf.length = size;

    if (gss_unwrap (&stat_min, gd->gcontext, &inbuf, &outbuf, &conf, NULL)
	!= GSS_S_COMPLETE)
    {
	error (1, 0, "gss_unwrap failed");
    }

    if (outbuf.length > size)
	abort ();

    memcpy (output, outbuf.value, outbuf.length);

    /* The real packet size is stored in the data, so we don't need to
       remember outbuf.length.  */

    gss_release_buffer (&stat_min, &outbuf);

    return 0;
}

/* Wrap data using GSSAPI.  */

static int
cvs_gssapi_wrap_output (fnclosure, input, output, size, translated)
     void *fnclosure;
     const char *input;
     char *output;
     int size;
     int *translated;
{
    struct cvs_gssapi_wrap_data *gd =
	(struct cvs_gssapi_wrap_data *) fnclosure;
    gss_buffer_desc inbuf, outbuf;
    OM_uint32 stat_min;
    int conf_req, conf;

    inbuf.value = (void *) input;
    inbuf.length = size;

#ifdef ENCRYPTION
    conf_req = cvs_gssapi_encrypt;
#else
    conf_req = 0;
#endif

    if (gss_wrap (&stat_min, gd->gcontext, conf_req, GSS_C_QOP_DEFAULT,
		  &inbuf, &conf, &outbuf) != GSS_S_COMPLETE)
	error (1, 0, "gss_wrap failed");

    /* The packetizing buffer only permits us to add 100 bytes.
       FIXME: I don't know what, if anything, is guaranteed by GSSAPI.
       This may need to be increased for a different GSSAPI
       implementation, or we may need a different algorithm.  */
    if (outbuf.length > size + 100)
	abort ();

    memcpy (output, outbuf.value, outbuf.length);

    *translated = outbuf.length;

    gss_release_buffer (&stat_min, &outbuf);

    return 0;
}

#endif /* HAVE_GSSAPI */

#ifdef ENCRYPTION

#ifdef HAVE_KERBEROS

/* An encryption interface using Kerberos.  This is built on top of a
   packetizing buffer.  */

/* This structure is the closure field of the Kerberos translation
   routines.  */

struct krb_encrypt_data
{
    /* The Kerberos key schedule.  */
    Key_schedule sched;
    /* The Kerberos DES block.  */
    C_Block block;
};

static int krb_encrypt_input PROTO((void *, const char *, char *, int));
static int krb_encrypt_output PROTO((void *, const char *, char *, int,
				     int *));

/* Create a Kerberos encryption buffer.  We use a packetizing buffer
   with Kerberos encryption translation routines.  */

struct buffer *
krb_encrypt_buffer_initialize (buf, input, sched, block, memory)
     struct buffer *buf;
     int input;
     Key_schedule sched;
     C_Block block;
     void (*memory) PROTO((struct buffer *));
{
    struct krb_encrypt_data *kd;

    kd = (struct krb_encrypt_data *) xmalloc (sizeof *kd);
    memcpy (kd->sched, sched, sizeof (Key_schedule));
    memcpy (kd->block, block, sizeof (C_Block));

    return packetizing_buffer_initialize (buf,
					  input ? krb_encrypt_input : NULL,
					  input ? NULL : krb_encrypt_output,
					  kd,
					  memory);
}

/* Decrypt Kerberos data.  */

static int
krb_encrypt_input (fnclosure, input, output, size)
     void *fnclosure;
     const char *input;
     char *output;
     int size;
{
    struct krb_encrypt_data *kd = (struct krb_encrypt_data *) fnclosure;
    int tcount;

    des_cbc_encrypt ((C_Block *) input, (C_Block *) output,
		     size, kd->sched, &kd->block, 0);

    /* SIZE is the size of the buffer, which is set by the encryption
       routine.  The packetizing buffer will arrange for the first two
       bytes in the decrypted buffer to be the real (unaligned)
       length.  As a safety check, make sure that the length in the
       buffer corresponds to SIZE.  Note that the length in the buffer
       is just the length of the data.  We must add 2 to account for
       the buffer count itself.  */
    tcount = ((output[0] & 0xff) << 8) + (output[1] & 0xff);
    if (((tcount + 2 + 7) & ~7) != size)
      error (1, 0, "Decryption failure");

    return 0;
}

/* Encrypt Kerberos data.  */

static int
krb_encrypt_output (fnclosure, input, output, size, translated)
     void *fnclosure;
     const char *input;
     char *output;
     int size;
     int *translated;
{
    struct krb_encrypt_data *kd = (struct krb_encrypt_data *) fnclosure;
    int aligned;

    /* For security against a known plaintext attack, we should
       initialize any padding bytes to random values.  Instead, we
       just pick up whatever is on the stack, which is at least better
       than using zero.  */

    /* Align SIZE to an 8 byte boundary.  Note that SIZE includes the
       two byte buffer count at the start of INPUT which was added by
       the packetizing buffer.  */
    aligned = (size + 7) & ~7;

    /* We use des_cbc_encrypt rather than krb_mk_priv because the
       latter sticks a timestamp in the block, and krb_rd_priv expects
       that timestamp to be within five minutes of the current time.
       Given the way the CVS server buffers up data, that can easily
       fail over a long network connection.  We trust krb_recvauth to
       guard against a replay attack.  */

    des_cbc_encrypt ((C_Block *) input, (C_Block *) output, aligned,
		     kd->sched, &kd->block, 1);

    *translated = aligned;

    return 0;
}

#endif /* HAVE_KERBEROS */
#endif /* ENCRYPTION */
#endif /* defined (CLIENT_SUPPORT) || defined (SERVER_SUPPORT) */

/* Output LEN bytes at STR.  If LEN is zero, then output up to (not including)
   the first '\0' byte.  */

void
cvs_output (str, len)
    const char *str;
    size_t len;
{
    if (len == 0)
	len = strlen (str);
#ifdef SERVER_SUPPORT
    if (error_use_protocol)
    {
	buf_output (saved_output, str, len);
	buf_copy_lines (buf_to_net, saved_output, 'M');
    }
    else if (server_active)
    {
	buf_output (saved_output, str, len);
	buf_copy_lines (protocol, saved_output, 'M');
	buf_send_counted (protocol);
    }
    else
#endif
    {
	size_t written;
	size_t to_write = len;
	const char *p = str;

	/* For symmetry with cvs_outerr we would call fflush (stderr)
	   here.  I guess the assumption is that stderr will be
	   unbuffered, so we don't need to.  That sounds like a sound
	   assumption from the manpage I looked at, but if there was
	   something fishy about it, my guess is that calling fflush
	   would not produce a significant performance problem.  */

	while (to_write > 0)
	{
	    written = fwrite (p, 1, to_write, stdout);
	    if (written == 0)
		break;
	    p += written;
	    to_write -= written;
	}
    }
}

/* Output LEN bytes at STR in binary mode.  If LEN is zero, then
   output zero bytes.  */

void
cvs_output_binary (str, len)
    char *str;
    size_t len;
{
#ifdef SERVER_SUPPORT
    if (error_use_protocol || server_active)
    {
	struct buffer *buf;
	char size_text[40];

	if (error_use_protocol)
	    buf = buf_to_net;
	else if (server_active)
	    buf = protocol;

	if (!supported_response ("Mbinary"))
	{
	    error (0, 0, "\
this client does not support writing binary files to stdout");
	    return;
	}

	buf_output0 (buf, "Mbinary\012");
	sprintf (size_text, "%lu\012", (unsigned long) len);
	buf_output0 (buf, size_text);

	/* Not sure what would be involved in using buf_append_data here
	   without stepping on the toes of our caller (which is responsible
	   for the memory allocation of STR).  */
	buf_output (buf, str, len);

	if (!error_use_protocol)
	    buf_send_counted (protocol);
    }
    else
#endif
    {
	size_t written;
	size_t to_write = len;
	const char *p = str;

	/* For symmetry with cvs_outerr we would call fflush (stderr)
	   here.  I guess the assumption is that stderr will be
	   unbuffered, so we don't need to.  That sounds like a sound
	   assumption from the manpage I looked at, but if there was
	   something fishy about it, my guess is that calling fflush
	   would not produce a significant performance problem.  */
#ifdef USE_SETMODE_STDOUT
	int oldmode;

	/* It is possible that this should be the same ifdef as
	   USE_SETMODE_BINARY but at least for the moment we keep them
	   separate.  Mostly this is just laziness and/or a question
	   of what has been tested where.  Also there might be an
	   issue of setmode vs. _setmode.  */
	/* The Windows doc says to call setmode only right after startup.
	   I assume that what they are talking about can also be helped
	   by flushing the stream before changing the mode.  */
	fflush (stdout);
	oldmode = _setmode (_fileno (stdout), _O_BINARY);
	if (oldmode < 0)
	    error (0, errno, "failed to setmode on stdout");
#endif

	while (to_write > 0)
	{
	    written = fwrite (p, 1, to_write, stdout);
	    if (written == 0)
		break;
	    p += written;
	    to_write -= written;
	}
#ifdef USE_SETMODE_STDOUT
	fflush (stdout);
	if (_setmode (_fileno (stdout), oldmode) != _O_BINARY)
	    error (0, errno, "failed to setmode on stdout");
#endif
    }
}

/* Like CVS_OUTPUT but output is for stderr not stdout.  */

void
cvs_outerr (str, len)
    const char *str;
    size_t len;
{
    if (len == 0)
	len = strlen (str);
#ifdef SERVER_SUPPORT
    if (error_use_protocol)
    {
	buf_output (saved_outerr, str, len);
	buf_copy_lines (buf_to_net, saved_outerr, 'E');
    }
    else if (server_active)
    {
	buf_output (saved_outerr, str, len);
	buf_copy_lines (protocol, saved_outerr, 'E');
	buf_send_counted (protocol);
    }
    else
#endif
    {
	size_t written;
	size_t to_write = len;
	const char *p = str;

	/* Make sure that output appears in order if stdout and stderr
	   point to the same place.  For the server case this is taken
	   care of by the fact that saved_outerr always holds less
	   than a line.  */
	fflush (stdout);

	while (to_write > 0)
	{
	    written = fwrite (p, 1, to_write, stderr);
	    if (written == 0)
		break;
	    p += written;
	    to_write -= written;
	}
    }
}

/* Flush stderr.  stderr is normally flushed automatically, of course,
   but this function is used to flush information from the server back
   to the client.  */

void
cvs_flusherr ()
{
#ifdef SERVER_SUPPORT
    if (error_use_protocol)
    {
	/* Flush what we can to the network, but don't block.  */
	buf_flush (buf_to_net, 0);
    }
    else if (server_active)
    {
	/* Send a special count to tell the parent to flush.  */
	buf_send_special_count (protocol, -1);
    }
    else
#endif
	fflush (stderr);
}

/* Make it possible for the user to see what has been written to
   stdout (it is up to the implementation to decide exactly how far it
   should go to ensure this).  */

void
cvs_flushout ()
{
#ifdef SERVER_SUPPORT
    if (error_use_protocol)
    {
	/* Flush what we can to the network, but don't block.  */
	buf_flush (buf_to_net, 0);
    }
    else if (server_active)
    {
	/* Just do nothing.  This is because the code which
	   cvs_flushout replaces, setting stdout to line buffering in
	   main.c, didn't get called in the server child process.  But
	   in the future it is quite plausible that we'll want to make
	   this case work analogously to cvs_flusherr.  */
    }
    else
#endif
	fflush (stdout);
}

/* Output TEXT, tagging it according to TAG.  There are lots more
   details about what TAG means in cvsclient.texi but for the simple
   case (e.g. non-client/server), TAG is just "newline" to output a
   newline (in which case TEXT must be NULL), and any other tag to
   output normal text.

   Note that there is no way to output either \0 or \n as part of TEXT.  */

void
cvs_output_tagged (tag, text)
    char *tag;
    char *text;
{
    if (text != NULL && strchr (text, '\n') != NULL)
	/* Uh oh.  The protocol has no way to cope with this.  For now
	   we dump core, although that really isn't such a nice
	   response given that this probably can be caused by newlines
	   in filenames and other causes other than bugs in CVS.  Note
	   that we don't want to turn this into "MT newline" because
	   this case is a newline within a tagged item, not a newline
	   as extraneous sugar for the user.  */
	assert (0);

    /* Start and end tags don't take any text, per cvsclient.texi.  */
    if (tag[0] == '+' || tag[0] == '-')
	assert (text == NULL);

#ifdef SERVER_SUPPORT
    if (server_active && supported_response ("MT"))
    {
	struct buffer *buf;

	if (error_use_protocol)
	    buf = buf_to_net;
	else
	    buf = protocol;

	buf_output0 (buf, "MT ");
	buf_output0 (buf, tag);
	if (text != NULL)
	{
	    buf_output (buf, " ", 1);
	    buf_output0 (buf, text);
	}
	buf_output (buf, "\n", 1);

	if (!error_use_protocol)
	    buf_send_counted (protocol);
    }
    else
#endif
    {
	if (strcmp (tag, "newline") == 0)
	    cvs_output ("\n", 1);
	else if (text != NULL)
	    cvs_output (text, 0);
    }
}
