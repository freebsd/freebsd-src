/* JT thinks BeOS is worth the trouble. */

/* CVS client-related stuff.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/*
 * $FreeBSD$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <assert.h>
#include "cvs.h"
#include "getline.h"
#include "edit.h"
#include "buffer.h"

#ifdef CLIENT_SUPPORT

# include "md5.h"

# if defined(AUTH_CLIENT_SUPPORT) || HAVE_KERBEROS || defined(SOCK_ERRNO) || defined(SOCK_STRERROR)
#   ifdef HAVE_WINSOCK_H
#     include <winsock.h>
#   else /* No winsock.h */
#     include <sys/socket.h>
#     include <netinet/in.h>
#     include <arpa/inet.h>
#     include <netdb.h>
#   endif /* No winsock.h */
# endif

/* If SOCK_ERRNO is defined, then send()/recv() and other socket calls
   do not set errno, but that this macro should be used to obtain an
   error code.  This probably doesn't make sense unless
   NO_SOCKET_TO_FD is also defined. */
# ifndef SOCK_ERRNO
#   define SOCK_ERRNO errno
# endif

/* If SOCK_STRERROR is defined, then the error codes returned by
   socket operations are not known to strerror, and this macro must be
   used instead to convert those error codes to strings. */
# ifndef SOCK_STRERROR
#   define SOCK_STRERROR strerror

#   if STDC_HEADERS
#     include <string.h>
#   endif

#   ifndef strerror
extern char *strerror ();
#   endif
# endif /* ! SOCK_STRERROR */

# if HAVE_KERBEROS

#   include <krb.h>

extern char *krb_realmofhost ();
#   ifndef HAVE_KRB_GET_ERR_TEXT
#     define krb_get_err_text(status) krb_err_txt[status]
#   endif /* HAVE_KRB_GET_ERR_TEXT */

/* Information we need if we are going to use Kerberos encryption.  */
static C_Block kblock;
static Key_schedule sched;

# endif /* HAVE_KERBEROS */

# ifdef HAVE_GSSAPI

#   include "xgssapi.h"

/* This is needed for GSSAPI encryption.  */
static gss_ctx_id_t gcontext;

static int connect_to_gserver PROTO((cvsroot_t *, int, struct hostent *));

# endif /* HAVE_GSSAPI */

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
static void handle_rcs_diff PROTO((char *, int));
static void handle_removed PROTO((char *, int));
static void handle_remove_entry PROTO((char *, int));
static void handle_set_static_directory PROTO((char *, int));
static void handle_clear_static_directory PROTO((char *, int));
static void handle_set_sticky PROTO((char *, int));
static void handle_clear_sticky PROTO((char *, int));
static void handle_set_checkin_prog PROTO((char *, int));
static void handle_set_update_prog PROTO((char *, int));
static void handle_module_expansion PROTO((char *, int));
static void handle_wrapper_rcs_option PROTO((char *, int));
static void handle_m PROTO((char *, int));
static void handle_e PROTO((char *, int));
static void handle_f PROTO((char *, int));
static void handle_notified PROTO((char *, int));

static size_t try_read_from_server PROTO ((char *, size_t));

static void auth_server PROTO ((cvsroot_t *, struct buffer *, struct buffer *,
				int, int, struct hostent *));

/* We need to keep track of the list of directories we've sent to the
   server.  This list, along with the current CVSROOT, will help us
   decide which command-line arguments to send.  */
List *dirs_sent_to_server = NULL;

static int is_arg_a_parent_or_listed_dir PROTO((Node *, void *));

static int
is_arg_a_parent_or_listed_dir (n, d)
    Node *n;
    void *d;
{
    char *directory = n->key;	/* name of the dir sent to server */
    char *this_argv_elem = (char *) d;	/* this argv element */

    /* Say we should send this argument if the argument matches the
       beginning of a directory name sent to the server.  This way,
       the server will know to start at the top of that directory
       hierarchy and descend. */

    if (strncmp (directory, this_argv_elem, strlen (this_argv_elem)) == 0)
	return 1;

    return 0;
}

static int arg_should_not_be_sent_to_server PROTO((char *));

/* Return nonzero if this argument should not be sent to the
   server. */

static int
arg_should_not_be_sent_to_server (arg)
    char *arg;
{
    /* Decide if we should send this directory name to the server.  We
       should always send argv[i] if:

       1) the list of directories sent to the server is empty (as it
       will be for checkout, etc.).

       2) the argument is "."

       3) the argument is a file in the cwd and the cwd is checked out
       from the current root

       4) the argument lies within one of the paths in
       dirs_sent_to_server.

       */

    if (list_isempty (dirs_sent_to_server))
	return 0;		/* always send it */

    if (strcmp (arg, ".") == 0)
	return 0;		/* always send it */

    /* We should send arg if it is one of the directories sent to the
       server or the parent of one; this tells the server to descend
       the hierarchy starting at this level. */
    if (isdir (arg))
    {
	if (walklist (dirs_sent_to_server, is_arg_a_parent_or_listed_dir, arg))
	    return 0;

	/* If arg wasn't a parent, we don't know anything about it (we
	   would have seen something related to it during the
	   send_files phase).  Don't send it.  */
	return 1;
    }

    /* Try to decide whether we should send arg to the server by
       checking the contents of the corresponding CVSADM directory. */
    {
	char *t, *this_root;

	/* Calculate "dirname arg" */
	for (t = arg + strlen (arg) - 1; t >= arg; t--)
	{
	    if (ISDIRSEP(*t))
		break;
	}

	/* Now we're either poiting to the beginning of the
	   string, or we found a path separator. */
	if (t >= arg)
	{
	    /* Found a path separator.  */
	    char c = *t;
	    *t = '\0';
	    
	    /* First, check to see if we sent this directory to the
               server, because it takes less time than actually
               opening the stuff in the CVSADM directory.  */
	    if (walklist (dirs_sent_to_server, is_arg_a_parent_or_listed_dir,
			  arg))
	    {
		*t = c;		/* make sure to un-truncate the arg */
		return 0;
	    }

	    /* Since we didn't find it in the list, check the CVSADM
               files on disk.  */
	    this_root = Name_Root (arg, (char *) NULL);
	    *t = c;
	}
	else
	{
	    /* We're at the beginning of the string.  Look at the
               CVSADM files in cwd.  */
	    this_root = (CVSroot_cmdline ? xstrdup(CVSroot_cmdline)
			 : Name_Root ((char *) NULL, (char *) NULL));
	}

	/* Now check the value for root. */
	if (CVSroot_cmdline == NULL && this_root && current_parsed_root
	    && (strcmp (this_root, current_parsed_root->original) != 0))
	{
	    /* Don't send this, since the CVSROOTs don't match. */
	    free (this_root);
	    return 1;
	}
	free (this_root);
    }
    
    /* OK, let's send it. */
    return 0;
}



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
 * If RESPECT_UMASK is set, then honor the umask.
 */
int
change_mode (filename, mode_string, respect_umask)
    char *filename;
    char *mode_string;
    int respect_umask;
{
#ifdef CHMOD_BROKEN
    char *p;
    int writeable = 0;

    /* We can only distinguish between
         1) readable
         2) writeable
         3) Picasso's "Blue Period"
       We handle the first two. */
    p = mode_string;
    while (*p != '\0')
    {
	if ((p[0] == 'u' || p[0] == 'g' || p[0] == 'o') && p[1] == '=')
	{
	    char *q = p + 2;
	    while (*q != ',' && *q != '\0')
	    {
		if (*q == 'w')
		    writeable = 1;
		++q;
	    }
	}
	/* Skip to the next field.  */
	while (*p != ',' && *p != '\0')
	    ++p;
	if (*p == ',')
	    ++p;
    }

    /* xchmod honors the umask for us.  In the !respect_umask case, we
       don't try to cope with it (probably to handle that well, the server
       needs to deal with modes in data structures, rather than via the
       modes in temporary files).  */
    xchmod (filename, writeable);
	return 0;

#else /* ! CHMOD_BROKEN */

    char *p;
    mode_t mode = 0;
    mode_t oumask;

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
		    mode |= S_IXUSR;
	    }
	    else if (p[0] == 'g')
	    {
		if (can_read)
		    mode |= S_IRGRP;
		if (can_write)
		    mode |= S_IWGRP;
		if (can_execute)
		    mode |= S_IXGRP;
	    }
	    else if (p[0] == 'o')
	    {
		if (can_read)
		    mode |= S_IROTH;
		if (can_write)
		    mode |= S_IWOTH;
		if (can_execute)
		    mode |= S_IXOTH;
	    }
	}
	/* Skip to the next field.  */
	while (*p != ',' && *p != '\0')
	    ++p;
	if (*p == ',')
	    ++p;
    }

    if (respect_umask)
    {
	oumask = umask (0);
	(void) umask (oumask);
	mode &= ~oumask;
    }

    if (chmod (filename, mode) < 0)
	return errno;
    return 0;
#endif /* ! CHMOD_BROKEN */
}

#endif /* CLIENT_SUPPORT or SERVER_SUPPORT */

#ifdef CLIENT_SUPPORT

int client_prune_dirs;

static List *ignlist = (List *) NULL;

/* Buffer to write to the server.  */
static struct buffer *to_server;

/* Buffer used to read from the server.  */
static struct buffer *from_server;


/* We want to be able to log data sent between us and the server.  We
   do it using log buffers.  Each log buffer has another buffer which
   handles the actual I/O, and a file to log information to.

   This structure is the closure field of a log buffer.  */

struct log_buffer
{
    /* The underlying buffer.  */
    struct buffer *buf;
    /* The file to log information to.  */
    FILE *log;
};

static struct buffer *log_buffer_initialize
  PROTO((struct buffer *, FILE *, int, void (*) (struct buffer *)));
static int log_buffer_input PROTO((void *, char *, int, int, int *));
static int log_buffer_output PROTO((void *, const char *, int, int *));
static int log_buffer_flush PROTO((void *));
static int log_buffer_block PROTO((void *, int));
static int log_buffer_shutdown PROTO((struct buffer *));

/* Create a log buffer.  */

static struct buffer *
log_buffer_initialize (buf, fp, input, memory)
     struct buffer *buf;
     FILE *fp;
     int input;
     void (*memory) PROTO((struct buffer *));
{
    struct log_buffer *n;

    n = (struct log_buffer *) xmalloc (sizeof *n);
    n->buf = buf;
    n->log = fp;
    return buf_initialize (input ? log_buffer_input : NULL,
			   input ? NULL : log_buffer_output,
			   input ? NULL : log_buffer_flush,
			   log_buffer_block,
			   log_buffer_shutdown,
			   memory,
			   n);
}

/* The input function for a log buffer.  */

static int
log_buffer_input (closure, data, need, size, got)
     void *closure;
     char *data;
     int need;
     int size;
     int *got;
{
    struct log_buffer *lb = (struct log_buffer *) closure;
    int status;
    size_t n_to_write;

    if (lb->buf->input == NULL)
	abort ();

    status = (*lb->buf->input) (lb->buf->closure, data, need, size, got);
    if (status != 0)
	return status;

    if (*got > 0)
    {
	n_to_write = *got;
	if (fwrite (data, 1, n_to_write, lb->log) != n_to_write)
	    error (0, errno, "writing to log file");
    }

    return 0;
}

/* The output function for a log buffer.  */

static int
log_buffer_output (closure, data, have, wrote)
     void *closure;
     const char *data;
     int have;
     int *wrote;
{
    struct log_buffer *lb = (struct log_buffer *) closure;
    int status;
    size_t n_to_write;

    if (lb->buf->output == NULL)
	abort ();

    status = (*lb->buf->output) (lb->buf->closure, data, have, wrote);
    if (status != 0)
	return status;

    if (*wrote > 0)
    {
	n_to_write = *wrote;
	if (fwrite (data, 1, n_to_write, lb->log) != n_to_write)
	    error (0, errno, "writing to log file");
    }

    return 0;
}

/* The flush function for a log buffer.  */

static int
log_buffer_flush (closure)
     void *closure;
{
    struct log_buffer *lb = (struct log_buffer *) closure;

    if (lb->buf->flush == NULL)
	abort ();

    /* We don't really have to flush the log file here, but doing it
       will let tail -f on the log file show what is sent to the
       network as it is sent.  */
    if (fflush (lb->log) != 0)
        error (0, errno, "flushing log file");

    return (*lb->buf->flush) (lb->buf->closure);
}

/* The block function for a log buffer.  */

static int
log_buffer_block (closure, block)
     void *closure;
     int block;
{
    struct log_buffer *lb = (struct log_buffer *) closure;

    if (block)
	return set_block (lb->buf);
    else
	return set_nonblock (lb->buf);
}

/* The shutdown function for a log buffer.  */

static int
log_buffer_shutdown (buf)
     struct buffer *buf;
{
    struct log_buffer *lb = (struct log_buffer *) buf->closure;
    int retval;

    retval = buf_shutdown (lb->buf);
    if (fclose (lb->log) < 0)
	error (0, errno, "closing log file");
    return retval;
}

#ifdef NO_SOCKET_TO_FD

/* Under certain circumstances, we must communicate with the server
   via a socket using send() and recv().  This is because under some
   operating systems (OS/2 and Windows 95 come to mind), a socket
   cannot be converted to a file descriptor -- it must be treated as a
   socket and nothing else.
   
   We may also need to deal with socket routine error codes differently
   in these cases.  This is handled through the SOCK_ERRNO and
   SOCK_STRERROR macros. */

/* These routines implement a buffer structure which uses send and
   recv.  The buffer is always in blocking mode so we don't implement
   the block routine.  */

/* Note that it is important that these routines always handle errors
   internally and never return a positive errno code, since it would in
   general be impossible for the caller to know in general whether any
   error code came from a socket routine (to decide whether to use
   SOCK_STRERROR or simply strerror to print an error message). */

/* We use an instance of this structure as the closure field.  */

struct socket_buffer
{
    /* The socket number.  */
    int socket;
};

static struct buffer *socket_buffer_initialize
  PROTO ((int, int, void (*) (struct buffer *)));
static int socket_buffer_input PROTO((void *, char *, int, int, int *));
static int socket_buffer_output PROTO((void *, const char *, int, int *));
static int socket_buffer_flush PROTO((void *));
static int socket_buffer_shutdown PROTO((struct buffer *));



/* Create a buffer based on a socket.  */

static struct buffer *
socket_buffer_initialize (socket, input, memory)
    int socket;
    int input;
    void (*memory) PROTO((struct buffer *));
{
    struct socket_buffer *n;

    n = (struct socket_buffer *) xmalloc (sizeof *n);
    n->socket = socket;
    return buf_initialize (input ? socket_buffer_input : NULL,
			   input ? NULL : socket_buffer_output,
			   input ? NULL : socket_buffer_flush,
			   (int (*) PROTO((void *, int))) NULL,
			   socket_buffer_shutdown,
			   memory,
			   n);
}



/* The buffer input function for a buffer built on a socket.  */

static int
socket_buffer_input (closure, data, need, size, got)
     void *closure;
     char *data;
     int need;
     int size;
     int *got;
{
    struct socket_buffer *sb = (struct socket_buffer *) closure;
    int nbytes;

    /* I believe that the recv function gives us exactly the semantics
       we want.  If there is a message, it returns immediately with
       whatever it could get.  If there is no message, it waits until
       one comes in.  In other words, it is not like read, which in
       blocking mode normally waits until all the requested data is
       available.  */

    *got = 0;

    do
    {

	/* Note that for certain (broken?) networking stacks, like
	   VMS's UCX (not sure what version, problem reported with
	   recv() in 1997), and (according to windows-NT/config.h)
	   Windows NT 3.51, we must call recv or send with a
	   moderately sized buffer (say, less than 200K or something),
	   or else there may be network errors (somewhat hard to
	   produce, e.g. WAN not LAN or some such).  buf_read_data
	   makes sure that we only recv() BUFFER_DATA_SIZE bytes at
	   a time.  */

	nbytes = recv (sb->socket, data, size, 0);
	if (nbytes < 0)
	    error (1, 0, "reading from server: %s", SOCK_STRERROR (SOCK_ERRNO));
	if (nbytes == 0)
	{
	    /* End of file (for example, the server has closed
	       the connection).  If we've already read something, we
	       just tell the caller about the data, not about the end of
	       file.  If we've read nothing, we return end of file.  */
	    if (*got == 0)
		return -1;
	    else
		return 0;
	}
	need -= nbytes;
	size -= nbytes;
	data += nbytes;
	*got += nbytes;
    }
    while (need > 0);

    return 0;
}



/* The buffer output function for a buffer built on a socket.  */

static int
socket_buffer_output (closure, data, have, wrote)
     void *closure;
     const char *data;
     int have;
     int *wrote;
{
    struct socket_buffer *sb = (struct socket_buffer *) closure;

    *wrote = have;

    /* See comment in socket_buffer_input regarding buffer size we pass
       to send and recv.  */

#ifdef SEND_NEVER_PARTIAL
    /* If send() never will produce a partial write, then just do it.  This
       is needed for systems where its return value is something other than
       the number of bytes written.  */
    if (send (sb->socket, data, have, 0) < 0)
	error (1, 0, "writing to server socket: %s", SOCK_STRERROR (SOCK_ERRNO));
#else
    while (have > 0)
    {
	int nbytes;

	nbytes = send (sb->socket, data, have, 0);
	if (nbytes < 0)
	    error (1, 0, "writing to server socket: %s", SOCK_STRERROR (SOCK_ERRNO));

	have -= nbytes;
	data += nbytes;
    }
#endif

    return 0;
}



/* The buffer flush function for a buffer built on a socket.  */

/*ARGSUSED*/
static int
socket_buffer_flush (closure)
     void *closure;
{
    /* Nothing to do.  Sockets are always flushed.  */
    return 0;
}



static int
socket_buffer_shutdown (buf)
    struct buffer *buf;
{
    struct socket_buffer *n = (struct socket_buffer *) buf->closure;
    char tmp;

    /* no need to flush children of an endpoint buffer here */

    if (buf->input)
    {
	int err = 0;
	if (! buf_empty_p (buf)
	    || (err = recv (n->socket, &tmp, 1, 0)) > 0)
	    error (0, 0, "dying gasps from %s unexpected", current_parsed_root->hostname);
	else if (err == -1)
	    error (0, 0, "reading from %s: %s", current_parsed_root->hostname, SOCK_STRERROR (SOCK_ERRNO));

	/* shutdown() socket */
# ifdef SHUTDOWN_SERVER
	if (current_parsed_root->method != server_method)
# endif
	if (shutdown (n->socket, 0) < 0)
	{
	    error (1, 0, "shutting down server socket: %s", SOCK_STRERROR (SOCK_ERRNO));
	}

	buf->input = NULL;
    }
    else if (buf->output)
    {
	/* shutdown() socket */
# ifdef SHUTDOWN_SERVER
	/* FIXME:  Should have a SHUTDOWN_SERVER_INPUT &
	 * SHUTDOWN_SERVER_OUTPUT
	 */
	if (current_parsed_root->method == server_method)
	    SHUTDOWN_SERVER (n->socket);
	else
# endif
	if (shutdown (n->socket, 1) < 0)
	{
	    error (1, 0, "shutting down server socket: %s", SOCK_STRERROR (SOCK_ERRNO));
	}

	buf->output = NULL;
    }

    return 0;
}

#endif /* NO_SOCKET_TO_FD */

/*
 * Read a line from the server.  Result does not include the terminating \n.
 *
 * Space for the result is malloc'd and should be freed by the caller.
 *
 * Returns number of bytes read.
 */
static int
read_line (resultp)
    char **resultp;
{
    int status;
    char *result;
    int len;

    status = buf_flush (to_server, 1);
    if (status != 0)
	error (1, status, "writing to server");

    status = buf_read_line (from_server, &result, &len);
    if (status != 0)
    {
	if (status == -1)
	    error (1, 0, "end of file from server (consult above messages if any)");
	else if (status == -2)
	    error (1, 0, "out of memory");
	else
	    error (1, status, "reading from server");
    }

    if (resultp != NULL)
	*resultp = result;
    else
	free (result);

    return len;
}

#endif /* CLIENT_SUPPORT */


#if defined(CLIENT_SUPPORT) || defined(SERVER_SUPPORT)

/*
 * Zero if compression isn't supported or requested; non-zero to indicate
 * a compression level to request from gzip.
 */
int gzip_level;

/*
 * Level of compression to use when running gzip on a single file.
 */
int file_gzip_level;

#endif /* CLIENT_SUPPORT or SERVER_SUPPORT */

#ifdef CLIENT_SUPPORT

/*
 * The Repository for the top level of this command (not necessarily
 * the CVSROOT, just the current directory at the time we do it).
 */
static char *toplevel_repos = NULL;

/* Working directory when we first started.  Note: we could speed things
   up on some systems by using savecwd.h here instead of just always
   storing a name.  */
char *toplevel_wd;

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

    /* Next we print the text of the message from the server.  We
       probably should be prefixing it with "server error" or some
       such, because if it is something like "Out of memory", the
       current behavior doesn't say which machine is out of
       memory.  */

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
	    if (rq->flags & RQ_ENABLEME)
	    {
		/*
		 * Server wants to know if we have this, to enable the
		 * feature.
		 */
		send_to_server (rq->name, 0);
                send_to_server ("\012", 0);
	    }
	    else
		rq->flags |= RQ_SUPPORTED;
	}
	p = q;
    } while (q != NULL);
    for (rq = requests; rq->name != NULL; ++rq)
    {
	if ((rq->flags & RQ_SUPPORTED)
	    || (rq->flags & RQ_ENABLEME))
	    continue;
	if (rq->flags & RQ_ESSENTIAL)
	    error (1, 0, "request `%s' not supported by server", rq->name);
    }
}

/* This variable holds the result of Entries_Open, so that we can
   close Entries_Close on it when we move on to a new directory, or
   when we finish.  */
static List *last_entries;

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

static char *last_dir_name;

static void
call_in_directory (pathname, func, data)
    char *pathname;
    void (*func) PROTO((char *data, List *ent_list, char *short_pathname,
			  char *filename));
    char *data;
{
    char *dir_name;
    char *filename;
    /* This is what we get when we hook up the directory (working directory
       name) from PATHNAME with the filename from REPOSNAME.  For example:
       pathname: ccvs/src/
       reposname: /u/src/master/ccvs/foo/ChangeLog
       short_pathname: ccvs/src/ChangeLog
       */
    char *short_pathname;
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
    read_line (&reposname);
    assert (reposname != NULL);

    reposdirname_absolute = 0;
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
    reposdirname = xstrdup (short_repos);
    p = strrchr (reposdirname, '/');
    if (p == NULL)
    {
	reposdirname = xrealloc (reposdirname, 2);
	reposdirname[0] = '.'; reposdirname[1] = '\0';
    }
    else
	*p = '\0';

    dir_name = xstrdup (pathname);
    p = strrchr (dir_name, '/');
    if (p == NULL)
    {
	dir_name = xrealloc (dir_name, 2);
	dir_name[0] = '.'; dir_name[1] = '\0';
    }
    else
	*p = '\0';
    if (client_prune_dirs)
	add_prune_candidate (dir_name);

    filename = strrchr (short_repos, '/');
    if (filename == NULL)
	filename = short_repos;
    else
	++filename;

    short_pathname = xmalloc (strlen (pathname) + strlen (filename) + 5);
    strcpy (short_pathname, pathname);
    strcat (short_pathname, filename);

    if (last_dir_name == NULL
	|| strcmp (last_dir_name, dir_name) != 0)
    {
	int newdir;

	if (strcmp (command_name, "export") != 0)
	    if (last_entries)
		Entries_Close (last_entries);

	if (last_dir_name)
	    free (last_dir_name);
	last_dir_name = dir_name;

	if (toplevel_wd == NULL)
	{
	    toplevel_wd = xgetwd ();
	    if (toplevel_wd == NULL)
		error (1, errno, "could not get working directory");
	}

	if (CVS_CHDIR (toplevel_wd) < 0)
	    error (1, errno, "could not chdir to %s", toplevel_wd);
	newdir = 0;

	/* Create the CVS directory at the top level if needed.  The
	   isdir seems like an unneeded system call, but it *does*
	   need to be called both if the CVS_CHDIR below succeeds
	   (e.g.  "cvs co .") or if it fails (e.g. basicb-1a in
	   testsuite).  We only need to do this for the "." case,
	   since the server takes care of forcing this directory to be
	   created in all other cases.  If we don't create CVSADM
	   here, the call to Entries_Open below will fail.  FIXME:
	   perhaps this means that we should change our algorithm
	   below that calls Create_Admin instead of having this code
	   here? */
	if (/* I think the reposdirname_absolute case has to do with
	       things like "cvs update /foo/bar".  In any event, the
	       code below which tries to put toplevel_repos into
	       CVS/Repository is almost surely unsuited to
	       the reposdirname_absolute case.  */
	    !reposdirname_absolute
	    && (strcmp (dir_name, ".") == 0)
	    && ! isdir (CVSADM))
	{
	    char *repo;
	    char *r;

	    newdir = 1;

	    repo = xmalloc (strlen (toplevel_repos)
			    + 10);
	    strcpy (repo, toplevel_repos);
	    r = repo + strlen (repo);
	    if (r[-1] != '.' || r[-2] != '/')
	        strcpy (r, "/.");

	    Create_Admin (".", ".", repo, (char *) NULL,
			  (char *) NULL, 0, 1, 1);

	    free (repo);
	}

	if ( CVS_CHDIR (dir_name) < 0)
	{
	    char *dir;
	    char *dirp;
	    
	    if (! existence_error (errno))
		error (1, errno, "could not chdir to %s", dir_name);
	    
	    /* Directory does not exist, we need to create it.  */
	    newdir = 1;

	    /* Provided we are willing to assume that directories get
	       created one at a time, we could simplify this a lot.
	       Do note that one aspect still would need to walk the
	       dir_name path: the checking for "fncmp (dir, CVSADM)".  */

	    dir = xmalloc (strlen (dir_name) + 1);
	    dirp = dir_name;
	    rdirp = reposdirname;

	    /* This algorithm makes nested directories one at a time
               and create CVS administration files in them.  For
               example, we're checking out foo/bar/baz from the
               repository:

	       1) create foo, point CVS/Repository to <root>/foo
	       2)     .. foo/bar                   .. <root>/foo/bar
	       3)     .. foo/bar/baz               .. <root>/foo/bar/baz
	       
	       As you can see, we're just stepping along DIR_NAME (with
	       DIRP) and REPOSDIRNAME (with RDIRP) respectively.

	       We need to be careful when we are checking out a
	       module, however, since DIR_NAME and REPOSDIRNAME are not
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
		    strncpy (dir, dir_name, dirp - dir_name);
		    dir[dirp - dir_name] = '\0';
		    /* Skip the slash.  */
		    ++dirp;
		    if (rdirp == NULL)
			/* This just means that the repository string has
			   fewer components than the dir_name string.  But
			   that is OK (e.g. see modules3-8 in testsuite).  */
			;
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
		    strcpy (dir, dir_name);
		}

		if (fncmp (dir, CVSADM) == 0)
		{
		    error (0, 0, "cannot create a directory named %s", dir);
		    error (0, 0, "because CVS uses \"%s\" for its own uses",
			   CVSADM);
		    error (1, 0, "rename the directory and try again");
		}

		if (mkdir_if_needed (dir))
		{
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
		    char *r, *b;

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
			/* See comment near start of function; the only
			   way that the server can put the right thing
			   in each CVS/Repository file is to create the
			   directories one at a time.  I think that the
			   CVS server has been doing this all along.  */
			error (0, 0, "\
warning: server is not creating directories one at a time");
			strncpy (r, reposdirname, rdirp - reposdirname);
			r[rdirp - reposdirname] = '\0';
		    }
		    else
			strcpy (r, reposdirname);

		    Create_Admin (dir, dir, repo,
				  (char *)NULL, (char *)NULL, 0, 0, 1);
		    free (repo);

		    b = strrchr (dir, '/');
		    if (b == NULL)
			Subdir_Register ((List *) NULL, (char *) NULL, dir);
		    else
		    {
			*b = '\0';
			Subdir_Register ((List *) NULL, dir, b + 1);
			*b = '/';
		    }
		}

		if (rdirp != NULL)
		{
		    /* Skip the slash.  */
		    ++rdirp;
		}

	    } while (dirp != NULL);
	    free (dir);
	    /* Now it better work.  */
	    if ( CVS_CHDIR (dir_name) < 0)
		error (1, errno, "could not chdir to %s", dir_name);
	}
	else if (strcmp (command_name, "export") == 0)
	    /* Don't create CVSADM directories if this is export.  */
	    ;
	else if (!isdir (CVSADM))
	{
	    /*
	     * Put repository in CVS/Repository.  For historical
	     * (pre-CVS/Root) reasons, this is an absolute pathname,
	     * but what really matters is the part of it which is
	     * relative to cvsroot.
	     */
	    char *repo;

	    if (reposdirname_absolute)
		repo = reposdirname;
	    else
	    {
		repo = xmalloc (strlen (reposdirname)
				+ strlen (toplevel_repos)
				+ 10);
		strcpy (repo, toplevel_repos);
		strcat (repo, "/");
		strcat (repo, reposdirname);
	    }

	    Create_Admin (".", ".", repo, (char *)NULL, (char *)NULL, 0, 1, 1);
	    if (repo != reposdirname)
		free (repo);
	}

	if (strcmp (command_name, "export") != 0)
	{
	    last_entries = Entries_Open (0, dir_name);

	    /* If this is a newly created directory, we will record
	       all subdirectory information, so call Subdirs_Known in
	       case there are no subdirectories.  If this is not a
	       newly created directory, it may be an old working
	       directory from before we recorded subdirectory
	       information in the Entries file.  We force a search for
	       all subdirectories now, to make sure our subdirectory
	       information is up to date.  If the Entries file does
	       record subdirectory information, then this call only
	       does list manipulation.  */
	    if (newdir)
		Subdirs_Known (last_entries);
	    else
	    {
		List *dirlist;

		dirlist = Find_Directories ((char *) NULL, W_LOCAL,
					    last_entries);
		dellist (&dirlist);
	    }
	}
    }
    else
	free (dir_name);
    free (reposdirname);
    (*func) (data, last_entries, short_pathname, filename);
    free (short_pathname);
    free (reposname);
}

static void
copy_a_file (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    char *newname;
#ifdef USE_VMS_FILENAMES
    char *p;
#endif

    read_line (&newname);

#ifdef USE_VMS_FILENAMES
    /* Mogrify the filename so VMS is happy with it. */
    for(p = newname; *p; p++)
       if(*p == '.' || *p == '#') *p = '_';
#endif
    /* cvsclient.texi has said for a long time that newname must be in the
       same directory.  Wouldn't want a malicious or buggy server overwriting
       ~/.profile, /etc/passwd, or anything like that.  */
    if (last_component (newname) != newname)
	error (1, 0, "protocol error: Copy-file tried to specify directory");

    if (unlink_file (newname) && !existence_error (errno))
	error (0, errno, "unable to remove %s", newname);
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


static void read_counted_file PROTO ((char *, char *));

/* Read from the server the count for the length of a file, then read
   the contents of that file and write them to FILENAME.  FULLNAME is
   the name of the file for use in error messages.  FIXME-someday:
   extend this to deal with compressed files and make update_entries
   use it.  On error, gives a fatal error.  */
static void
read_counted_file (filename, fullname)
    char *filename;
    char *fullname;
{
    char *size_string;
    size_t size;
    char *buf;

    /* Pointers in buf to the place to put data which will be read,
       and the data which needs to be written, respectively.  */
    char *pread;
    char *pwrite;
    /* Number of bytes left to read and number of bytes in buf waiting to
       be written, respectively.  */
    size_t nread;
    size_t nwrite;

    FILE *fp;

    read_line (&size_string);
    if (size_string[0] == 'z')
	error (1, 0, "\
protocol error: compressed files not supported for that operation");
    /* FIXME: should be doing more error checking, probably.  Like using
       strtoul and making sure we used up the whole line.  */
    size = atoi (size_string);
    free (size_string);

    /* A more sophisticated implementation would use only a limited amount
       of buffer space (8K perhaps), and read that much at a time.  We allocate
       a buffer for the whole file only to make it easy to keep track what
       needs to be read and written.  */
    buf = xmalloc (size);

    /* FIXME-someday: caller should pass in a flag saying whether it
       is binary or not.  I haven't carefully looked into whether
       CVS/Template files should use local text file conventions or
       not.  */
    fp = CVS_FOPEN (filename, "wb");
    if (fp == NULL)
	error (1, errno, "cannot write %s", fullname);
    nread = size;
    nwrite = 0;
    pread = buf;
    pwrite = buf;
    while (nread > 0 || nwrite > 0)
    {
	size_t n;

	if (nread > 0)
	{
	    n = try_read_from_server (pread, nread);
	    nread -= n;
	    pread += n;
	    nwrite += n;
	}

	if (nwrite > 0)
	{
	    n = fwrite (pwrite, 1, nwrite, fp);
	    if (ferror (fp))
		error (1, errno, "cannot write %s", fullname);
	    nwrite -= n;
	    pwrite += n;
	}
    }
    free (buf);
    if (fclose (fp) < 0)
	error (1, errno, "cannot close %s", fullname);
}

/* OK, we want to swallow the "U foo.c" response and then output it only
   if we can update the file.  In the future we probably want some more
   systematic approach to parsing tagged text, but for now we keep it
   ad hoc.  "Why," I hear you cry, "do we not just look at the
   Update-existing and Created responses?"  That is an excellent question,
   and the answer is roughly conservatism/laziness--I haven't read through
   update.c enough to figure out the exact correspondence or lack thereof
   between those responses and a "U foo.c" line (note that Merged, from
   join_file, can be either "C foo" or "U foo" depending on the context).  */
/* Nonzero if we have seen +updated and not -updated.  */
static int updated_seen;
/* Filename from an "fname" tagged response within +updated/-updated.  */
static char *updated_fname;

/* This struct is used to hold data when reading the +importmergecmd
   and -importmergecmd tags.  We put the variables in a struct only
   for namespace issues.  FIXME: As noted above, we need to develop a
   more systematic approach.  */
static struct
{
    /* Nonzero if we have seen +importmergecmd and not -importmergecmd.  */
    int seen;
    /* Number of conflicts, from a "conflicts" tagged response.  */
    int conflicts;
    /* First merge tag, from a "mergetag1" tagged response.  */
    char *mergetag1;
    /* Second merge tag, from a "mergetag2" tagged response.  */
    char *mergetag2;
    /* Repository, from a "repository" tagged response.  */
    char *repository;
} importmergecmd;

/* Nonzero if we should arrange to return with a failure exit status.  */
static int failure_exit;


/*
 * The time stamp of the last file we registered.
 */
static time_t last_register_time;

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

/* Mode that we got in a "Mode" response (malloc'd), or NULL if none.  */
static char *stored_mode;

static void handle_mode PROTO ((char *, int));

static void
handle_mode (args, len)
    char *args;
    int len;
{
    if (stored_mode != NULL)
	error (1, 0, "protocol error: duplicate Mode");
    stored_mode = xstrdup (args);
}

/* Nonzero if time was specified in Mod-time.  */
static int stored_modtime_valid;
/* Time specified in Mod-time.  */
static time_t stored_modtime;

static void handle_mod_time PROTO ((char *, int));

static void
handle_mod_time (args, len)
    char *args;
    int len;
{
    if (stored_modtime_valid)
	error (0, 0, "protocol error: duplicate Mod-time");
    stored_modtime = get_date (args, NULL);
    if (stored_modtime == (time_t) -1)
	error (0, 0, "protocol error: cannot parse date %s", args);
    else
	stored_modtime_valid = 1;
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
      UPDATE_ENTRIES_PATCH,
      /*
       * We are getting an RCS change text (diff -n output) against
       * the existing local file, not an entire new file.
       */
      UPDATE_ENTRIES_RCS_DIFF
    } contents;

    enum {
	/* We are replacing an existing file.  */
	UPDATE_ENTRIES_EXISTING,
	/* We are creating a new file.  */
	UPDATE_ENTRIES_NEW,
	/* We don't know whether it is existing or new.  */
	UPDATE_ENTRIES_EXISTING_OR_NEW
    } existp;

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
    char *options = NULL;
    char *tag = NULL;
    char *date = NULL;
    char *tag_or_date;
    char *scratch_entries = NULL;
    int bin;

#ifdef UTIME_EXPECTS_WRITABLE
    int change_it_back = 0;
#endif

    read_line (&entries_line);

    /*
     * Parse the entries line.
     */
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
    if (*tag_or_date == 'T')
        tag = tag_or_date + 1;
    else if (*tag_or_date == 'D')
        date = tag_or_date + 1;

    /* Done parsing the entries line. */

    if (data->contents == UPDATE_ENTRIES_UPDATE
	|| data->contents == UPDATE_ENTRIES_PATCH
	|| data->contents == UPDATE_ENTRIES_RCS_DIFF)
    {
	char *size_string;
	char *mode_string;
	int size;
	char *buf;
	char *temp_filename;
	int use_gzip;
	int patch_failed;

	read_line (&mode_string);
	
	read_line (&size_string);
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

	/* Note that checking this separately from writing the file is
	   a race condition: if the existence or lack thereof of the
	   file changes between now and the actual calls which
	   operate on it, we lose.  However (a) there are so many
	   cases, I'm reluctant to try to fix them all, (b) in some
	   cases the system might not even have a system call which
	   does the right thing, and (c) it isn't clear this needs to
	   work.  */
	if (data->existp == UPDATE_ENTRIES_EXISTING
	    && !isfile (filename))
	    /* Emit a warning and update the file anyway.  */
	    error (0, 0, "warning: %s unexpectedly disappeared",
		   short_pathname);

	if (data->existp == UPDATE_ENTRIES_NEW
	    && isfile (filename))
	{
	    /* Emit a warning and refuse to update the file; we don't want
	       to clobber a user's file.  */
	    size_t nread;
	    size_t toread;

	    /* size should be unsigned, but until we get around to fixing
	       that, work around it.  */
	    size_t usize;

	    char buf[8192];

	    /* This error might be confusing; it isn't really clear to
	       the user what to do about it.  Keep in mind that it has
	       several causes: (1) something/someone creates the file
	       during the time that CVS is running, (2) the repository
	       has two files whose names clash for the client because
	       of case-insensitivity or similar causes, (3) a special
	       case of this is that a file gets renamed for example
	       from a.c to A.C.  A "cvs update" on a case-insensitive
	       client will get this error.  Repeating the update takes
	       care of the problem, but is it clear to the user what
	       is going on and what to do about it?, (4) the client
	       has a file which the server doesn't know about (e.g. "?
	       foo" file), and that name clashes with a file the
	       server does know about, (5) classify.c will print the same
	       message for other reasons.

	       I hope the above paragraph makes it clear that making this
	       clearer is not a one-line fix.  */
	    error (0, 0, "move away %s; it is in the way", short_pathname);
	    if (updated_fname != NULL)
	    {
		cvs_output ("C ", 0);
		cvs_output (updated_fname, 0);
		cvs_output ("\n", 1);
	    }
	    failure_exit = 1;

	discard_file_and_return:
	    /* Now read and discard the file contents.  */
	    usize = size;
	    nread = 0;
	    while (nread < usize)
	    {
		toread = usize - nread;
		if (toread > sizeof buf)
		    toread = sizeof buf;

		nread += try_read_from_server (buf, toread);
		if (nread == usize)
		    break;
	    }

	    free (mode_string);
	    free (scratch_entries);
	    free (entries_line);

	    /* The Mode, Mod-time, and Checksum responses should not carry
	       over to a subsequent Created (or whatever) response, even
	       in the error case.  */
	    if (stored_mode != NULL)
	    {
		free (stored_mode);
		stored_mode = NULL;
	    }
	    stored_modtime_valid = 0;
	    stored_checksum_valid = 0;

	    if (updated_fname != NULL)
	    {
		free (updated_fname);
		updated_fname = NULL;
	    }
	    return;
	}

	temp_filename = xmalloc (strlen (filename) + 80);
#ifdef USE_VMS_FILENAMES
        /* A VMS rename of "blah.dat" to "foo" to implies a
           destination of "foo.dat" which is unfortinate for CVS */
       sprintf (temp_filename, "%s_new_", filename);
#else
#ifdef _POSIX_NO_TRUNC
	sprintf (temp_filename, ".new.%.9s", filename);
#else /* _POSIX_NO_TRUNC */
	sprintf (temp_filename, ".new.%s", filename);
#endif /* _POSIX_NO_TRUNC */
#endif /* USE_VMS_FILENAMES */

	buf = xmalloc (size);

        /* Some systems, like OS/2 and Windows NT, end lines with CRLF
           instead of just LF.  Format translation is done in the C
           library I/O funtions.  Here we tell them whether or not to
           convert -- if this file is marked "binary" with the RCS -kb
           flag, then we don't want to convert, else we do (because
           CVS assumes text files by default). */

	if (options)
	    bin = !(strcmp (options, "-kb"));
	else
	    bin = 0;

	if (data->contents == UPDATE_ENTRIES_RCS_DIFF)
	{
	    /* This is an RCS change text.  We just hold the change
	       text in memory.  */

	    if (use_gzip)
		error (1, 0,
		       "server error: gzip invalid with RCS change text");

	    read_from_server (buf, size);
	}
	else
	{
	    int fd;

	    fd = CVS_OPEN (temp_filename,
			   (O_WRONLY | O_CREAT | O_TRUNC
			    | (bin ? OPEN_BINARY : 0)),
			   0777);

	    if (fd < 0)
	    {
		/* I can see a case for making this a fatal error; for
		   a condition like disk full or network unreachable
		   (for a file server), carrying on and giving an
		   error on each file seems unnecessary.  But if it is
		   a permission problem, or some such, then it is
		   entirely possible that future files will not have
		   the same problem.  */
		error (0, errno, "cannot write %s", short_pathname);
		goto discard_file_and_return;
	    }

	    if (size > 0)
	    {
		read_from_server (buf, size);

		if (use_gzip)
		{
		    if (gunzip_and_write (fd, short_pathname, 
					  (unsigned char *) buf, size))
			error (1, 0, "aborting due to compression error");
		}
		else if (write (fd, buf, size) != size)
		    error (1, errno, "writing %s", short_pathname);
	    }

	    if (close (fd) < 0)
		error (1, errno, "writing %s", short_pathname);
	}

	/* This is after we have read the file from the net (a change
	   from previous versions, where the server would send us
	   "M U foo.c" before Update-existing or whatever), but before
	   we finish writing the file (arguably a bug).  The timing
	   affects a user who wants status info about how far we have
	   gotten, and also affects whether "U foo.c" appears in addition
	   to various error messages.  */
	if (updated_fname != NULL)
	{
	    cvs_output ("U ", 0);
	    cvs_output (updated_fname, 0);
	    cvs_output ("\n", 1);
	    free (updated_fname);
	    updated_fname = 0;
	}

	patch_failed = 0;

	if (data->contents == UPDATE_ENTRIES_UPDATE)
	{
	    rename_file (temp_filename, filename);
	}
	else if (data->contents == UPDATE_ENTRIES_PATCH)
	{
	    /* You might think we could just leave Patched out of
	       Valid-responses and not get this response.  However, if
	       memory serves, the CVS 1.9 server bases this on -u
	       (update-patches), and there is no way for us to send -u
	       or not based on whether the server supports "Rcs-diff".  

	       Fall back to transmitting entire files.  */
	    patch_failed = 1;
	}
	else
	{
	    char *filebuf;
	    size_t filebufsize;
	    size_t nread;
	    char *patchedbuf;
	    size_t patchedlen;

	    /* Handle UPDATE_ENTRIES_RCS_DIFF.  */

	    if (!isfile (filename))
	        error (1, 0, "patch original file %s does not exist",
		       short_pathname);
	    filebuf = NULL;
	    filebufsize = 0;
	    nread = 0;

	    get_file (filename, short_pathname, bin ? FOPEN_BINARY_READ : "r",
		      &filebuf, &filebufsize, &nread);
	    /* At this point the contents of the existing file are in
               FILEBUF, and the length of the contents is in NREAD.
               The contents of the patch from the network are in BUF,
               and the length of the patch is in SIZE.  */

	    if (! rcs_change_text (short_pathname, filebuf, nread, buf, size,
				   &patchedbuf, &patchedlen))
		patch_failed = 1;
	    else
	    {
		if (stored_checksum_valid)
		{
		    struct cvs_MD5Context context;
		    unsigned char checksum[16];

		    /* We have a checksum.  Check it before writing
		       the file out, so that we don't have to read it
		       back in again.  */
		    cvs_MD5Init (&context);
		    cvs_MD5Update (&context,
				   (unsigned char *) patchedbuf, patchedlen);
		    cvs_MD5Final (checksum, &context);
		    if (memcmp (checksum, stored_checksum, 16) != 0)
		    {
			error (0, 0,
			       "checksum failure after patch to %s; will refetch",
			       short_pathname);

			patch_failed = 1;
		    }

		    stored_checksum_valid = 0;
		}

		if (! patch_failed)
		{
		    FILE *e;

		    e = open_file (temp_filename,
				   bin ? FOPEN_BINARY_WRITE : "w");
		    if (fwrite (patchedbuf, 1, patchedlen, e) != patchedlen)
			error (1, errno, "cannot write %s", temp_filename);
		    if (fclose (e) == EOF)
			error (1, errno, "cannot close %s", temp_filename);
		    rename_file (temp_filename, filename);
		}

		free (patchedbuf);
	    }

	    free (filebuf);
	}

	free (temp_filename);

	if (stored_checksum_valid && ! patch_failed)
	{
	    FILE *e;
	    struct cvs_MD5Context context;
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
	    e = CVS_FOPEN (filename, "r");
	    if (e == NULL)
	        error (1, errno, "could not open %s", short_pathname);

	    cvs_MD5Init (&context);
	    while ((len = fread (buf, 1, sizeof buf, e)) != 0)
		cvs_MD5Update (&context, buf, len);
	    if (ferror (e))
		error (1, errno, "could not read %s", short_pathname);
	    cvs_MD5Final (checksum, &context);

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

		patch_failed = 1;
	    }
	}

	if (patch_failed)
	{
	    /* Save this file to retrieve later.  */
	    failed_patches = (char **) xrealloc ((char *) failed_patches,
						 ((failed_patches_count + 1)
						  * sizeof (char *)));
	    failed_patches[failed_patches_count] = xstrdup (short_pathname);
	    ++failed_patches_count;

	    stored_checksum_valid = 0;

	    free (mode_string);
	    free (buf);
	    free (scratch_entries);
	    free (entries_line);

	    return;
	}

        {
	    int status = change_mode (filename, mode_string, 1);
	    if (status != 0)
		error (0, status, "cannot change mode of %s", short_pathname);
	}

	free (mode_string);
	free (buf);
    }

    if (stored_mode != NULL)
    {
	change_mode (filename, stored_mode, 1);
	free (stored_mode);
	stored_mode = NULL;
    }
   
    if (stored_modtime_valid)
    {
	struct utimbuf t;

	memset (&t, 0, sizeof (t));
	/* There is probably little point in trying to preserved the
	   actime (or is there? What about Checked-in?).  */
	t.modtime = t.actime = stored_modtime;

#ifdef UTIME_EXPECTS_WRITABLE
	if (!iswritable (filename))
	{
	    xchmod (filename, 1);
	    change_it_back = 1;
	}
#endif  /* UTIME_EXPECTS_WRITABLE  */

	if (utime (filename, &t) < 0)
	    error (0, errno, "cannot set time on %s", filename);

#ifdef UTIME_EXPECTS_WRITABLE
	if (change_it_back == 1)
	{
	    xchmod (filename, 0);
	    change_it_back = 0;
	}
#endif  /*  UTIME_EXPECTS_WRITABLE  */

	stored_modtime_valid = 0;
    }

    /*
     * Process the entries line.  Do this after we've written the file,
     * since we need the timestamp.
     */
    if (strcmp (command_name, "export") != 0)
    {
	char *local_timestamp;
	char *file_timestamp;

	(void) time (&last_register_time);

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
	{
	    local_timestamp = file_timestamp;

	    /* Checking for command_name of "commit" doesn't seem like
	       the cleanest way to handle this, but it seem to roughly
	       parallel what the :local: code which calls
	       mark_up_to_date ends up amounting to.  Some day, should
	       think more about what the Checked-in response means
	       vis-a-vis both Entries and Base and clarify
	       cvsclient.texi accordingly.  */

	    if (!strcmp (command_name, "commit"))
		mark_up_to_date (filename);
	}

	Register (ent_list, filename, vn, local_timestamp,
		  options, tag, date, ts[0] == '+' ? file_timestamp : NULL);

	if (file_timestamp)
	    free (file_timestamp);

    }
    free (scratch_entries);
    free (entries_line);
}

static void
handle_checked_in (args, len)
    char *args;
    int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_CHECKIN;
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
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
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
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
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_created PROTO((char *, int));

static void
handle_created (args, len)
    char *args;
    int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_UPDATE;
    dat.existp = UPDATE_ENTRIES_NEW;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_update_existing PROTO((char *, int));

static void
handle_update_existing (args, len)
    char *args;
    int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_UPDATE;
    dat.existp = UPDATE_ENTRIES_EXISTING;
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
    /* Think this could be UPDATE_ENTRIES_EXISTING, but just in case...  */
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
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
    /* Think this could be UPDATE_ENTRIES_EXISTING, but just in case...  */
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void
handle_rcs_diff (args, len)
     char *args;
     int len;
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_RCS_DIFF;
    /* Think this could be UPDATE_ENTRIES_EXISTING, but just in case...  */
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
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
    /* Note that we don't ignore existence_error's here.  The server
       should be sending Remove-entry rather than Removed in cases
       where the file does not exist.  And if the user removes the
       file halfway through a cvs command, we should be printing an
       error.  */
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
    if (strcmp (toplevel_repos, current_parsed_root->directory) != 0)
	return 0;

    return strchr (pathname, '/') == NULL;
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
	read_line (NULL);
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
	read_line (NULL);
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

    read_line (&tagspec);

    /* FIXME-update-dir: error messages should include the directory.  */
    f = CVS_FOPEN (CVSADM_TAG, "w+");
    if (f == NULL)
    {
	/* Making this non-fatal is a bit of a kludge (see dirs2
	   in testsuite).  A better solution would be to avoid having
	   the server tell us about a directory we shouldn't be doing
	   anything with anyway (e.g. by handling directory
	   addition/removal better).  */
	error (0, errno, "cannot open %s", CVSADM_TAG);
	free (tagspec);
	return;
    }
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
	read_line (NULL);
        /* Swallow the tag line.  */
	read_line (NULL);
	return;
    }
    if (is_cvsroot_level (pathname))
    {
        /*
	 * Top level (directory containing CVSROOT).  This seems to normally
	 * lack a CVS directory, so don't try to create files in it.
	 */

	/* Swallow the repository.  */
	read_line (NULL);
        /* Swallow the tag line.  */
	read_line (NULL);
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
	read_line (NULL);
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


static void template PROTO ((char *, List *, char *, char *));

static void
template (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    /* FIXME: should be computing second argument from CVSADM_TEMPLATE
       and short_pathname.  */
    read_counted_file (CVSADM_TEMPLATE, "<CVS/Template file>");
}

static void handle_template PROTO ((char *, int));

static void
handle_template (pathname, len)
    char *pathname;
    int len;
{
    call_in_directory (pathname, template, NULL);
}


struct save_prog {
    char *name;
    char *dir;
    struct save_prog *next;
};

static struct save_prog *checkin_progs;
static struct save_prog *update_progs;

/*
 * Unlike some responses this doesn't include the repository.  So we can't
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

    read_line (&prog);
    if (strcmp (command_name, "export") == 0)
	return;

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

    read_line (&prog);
    if (strcmp (command_name, "export") == 0)
	return;

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

    char *fname;
    FILE *f;

    if (toplevel_wd != NULL)
    {
	if (CVS_CHDIR (toplevel_wd) < 0)
	    error (1, errno, "could not chdir to %s", toplevel_wd);
    }
    for (p = checkin_progs; p != NULL; )
    {
	fname = xmalloc (strlen (p->dir) + sizeof CVSADM_CIPROG + 10);
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
	free (fname);
    }
    checkin_progs = NULL;
    for (p = update_progs; p != NULL; )
    {
	fname = xmalloc (strlen (p->dir) + sizeof CVSADM_UPROG + 10);
	sprintf (fname, "%s/%s", p->dir, CVSADM_UPROG);
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
	free (fname);
    }
    update_progs = NULL;
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

    if ((dir[0] == '.' && dir[1] == '\0')
	|| (prune_candidates != NULL
	    && strcmp (dir, prune_candidates->dir) == 0))
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

    if (toplevel_wd != NULL)
    {
	if (CVS_CHDIR (toplevel_wd) < 0)
	    error (1, errno, "could not chdir to %s", toplevel_wd);
    }
    for (p = prune_candidates; p != NULL; )
    {
	if (isemptydir (p->dir, 1))
	{
	    char *b;

	    if (unlink_file_dir (p->dir) < 0)
		error (0, errno, "cannot remove %s", p->dir);
	    b = strrchr (p->dir, '/');
	    if (b == NULL)
		Subdir_Deregister ((List *) NULL, (char *) NULL, p->dir);
	    else
	    {
		*b = '\0';
		Subdir_Deregister ((List *) NULL, p->dir, b + 1);
	    }
	}
	free (p->dir);
	q = p->next;
	free (p);
	p = q;
    }
    prune_candidates = NULL;
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

    /* FIXME: this is probably not the best place to check; I wish I
     * knew where in here's callers to really trap this bug.  To
     * reproduce the bug, just do this:
     * 
     *       mkdir junk
     *       cd junk
     *       cvs -d some_repos update foo
     *
     * Poof, CVS seg faults and dies!  It's because it's trying to
     * send a NULL string to the server but dies in send_to_server.
     * That string was supposed to be the repository, but it doesn't
     * get set because there's no CVSADM dir, and somehow it's not
     * getting set from the -d argument either... ?
     */
    if (repos == NULL)
    {
        /* Lame error.  I want a real fix but can't stay up to track
           this down right now. */
        error (1, 0, "no repository");
    }

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

    /* Add a directory name to the list of those sent to the
       server. */
    if (update_dir && (*update_dir != '\0')
	&& (strcmp (update_dir, ".") != 0)
	&& (findnode (dirs_sent_to_server, update_dir) == NULL))
    {
	Node *n;
	n = getnode ();
	n->type = NT_UNKNOWN;
	n->key = xstrdup (update_dir);
	n->data = NULL;

	if (addnode (dirs_sent_to_server, n))
	    error (1, 0, "cannot add directory %s to list", n->key);
    }

    /* 80 is large enough for any of CVSADM_*.  */
    adm_name = xmalloc (strlen (dir) + 80);

    send_to_server ("Directory ", 0);
    {
	/* Send the directory name.  I know that this
	   sort of duplicates code elsewhere, but each
	   case seems slightly different...  */
	char buf[1];
	char *p = update_dir;
	while (*p != '\0')
	{
	    assert (*p != '\012');
	    if (ISDIRSEP (*p))
	    {
		buf[0] = '/';
		send_to_server (buf, 1);
	    }
	    else
	    {
		buf[0] = *p;
		send_to_server (buf, 1);
	    }
	    ++p;
	}
    }
    send_to_server ("\012", 1);
    send_to_server (repos, 0);
    send_to_server ("\012", 1);

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
	    send_to_server ("Static-directory\012", 0);
	}
    }
    if (supported_request ("Sticky"))
    {
	FILE *f;
	if (dir[0] == '\0')
	    strcpy (adm_name, CVSADM_TAG);
	else
	    sprintf (adm_name, "%s/%s", dir, CVSADM_TAG);

	f = CVS_FOPEN (adm_name, "r");
	if (f == NULL)
	{
	    if (! existence_error (errno))
		error (1, errno, "reading %s", adm_name);
	}
	else
	{
	    char line[80];
	    char *nl = NULL;
	    send_to_server ("Sticky ", 0);
	    while (fgets (line, sizeof (line), f) != NULL)
	    {
		send_to_server (line, 0);
		nl = strchr (line, '\n');
		if (nl != NULL)
		    break;
	    }
	    if (nl == NULL)
                send_to_server ("\012", 1);
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

	f = CVS_FOPEN (adm_name, "r");
	if (f == NULL)
	{
	    if (! existence_error (errno))
		error (1, errno, "reading %s", adm_name);
	}
	else
	{
	    char line[80];
	    char *nl = NULL;

	    send_to_server ("Checkin-prog ", 0);

	    while (fgets (line, sizeof (line), f) != NULL)
	    {
		send_to_server (line, 0);

		nl = strchr (line, '\n');
		if (nl != NULL)
		    break;
	    }
	    if (nl == NULL)
		send_to_server ("\012", 1);
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

	f = CVS_FOPEN (adm_name, "r");
	if (f == NULL)
	{
	    if (! existence_error (errno))
		error (1, errno, "reading %s", adm_name);
	}
	else
	{
	    char line[80];
	    char *nl = NULL;

	    send_to_server ("Update-prog ", 0);

	    while (fgets (line, sizeof (line), f) != NULL)
	    {
		send_to_server (line, 0);

		nl = strchr (line, '\n');
		if (nl != NULL)
		    break;
	    }
	    if (nl == NULL)
		send_to_server ("\012", 1);
	    if (fclose (f) == EOF)
		error (0, errno, "closing %s", adm_name);
	}
    }
    free (adm_name);
    if (last_repos != NULL)
	free (last_repos);
    if (last_update_dir != NULL)
	free (last_update_dir);
    last_repos = xstrdup (repos);
    last_update_dir = xstrdup (update_dir);
}

/* Send a Repository line and set toplevel_repos.  */

void
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
		 *
		 * 21 Aug 1998: Well, Mr. Above-Comment-Writer, it
		 * does need to work after all.  When we are using the
		 * client in a multi-cvsroot environment, it will be
		 * fairly common that we have the above case (e.g.,
		 * cwd checked out from one repository but
		 * subdirectory checked out from another).  We can't
		 * assume that by walking up a directory in our wd we
		 * necessarily walk up a directory in the repository.
		 */
		/*
		 * This gets toplevel_repos wrong for "cvs update ../foo"
		 * but I'm not sure toplevel_repos matters in that case.
		 */

		int repository_len, update_dir_len;

		strip_trailing_slashes (update_dir);

		repository_len = strlen (repository);
		update_dir_len = strlen (update_dir);

		/* Try to remove the path components in UPDATE_DIR
                   from REPOSITORY.  If the path elements don't exist
                   in REPOSITORY, or the removal of those path
                   elements mean that we "step above"
                   current_parsed_root->directory, set toplevel_repos to
                   current_parsed_root->directory. */
		if ((repository_len > update_dir_len)
		    && (strcmp (repository + repository_len - update_dir_len,
				update_dir) == 0)
		    /* TOPLEVEL_REPOS shouldn't be above current_parsed_root->directory */
		    && ((repository_len - update_dir_len)
			> strlen (current_parsed_root->directory)))
		{
		    /* The repository name contains UPDATE_DIR.  Set
                       toplevel_repos to the repository name without
                       UPDATE_DIR. */

		    toplevel_repos = xmalloc (repository_len - update_dir_len);
		    /* Note that we don't copy the trailing '/'.  */
		    strncpy (toplevel_repos, repository,
			     repository_len - update_dir_len - 1);
		    toplevel_repos[repository_len - update_dir_len - 1] = '\0';
		}
		else
		{
		    toplevel_repos = xstrdup (current_parsed_root->directory);
		}
	    }
	}
    }

    send_repository (dir, repository, update_dir);
}

/* The "expanded" modules.  */
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

/* Original, not "expanded" modules.  */
static int module_argc;
static char **module_argv;

void
client_expand_modules (argc, argv, local)
    int argc;
    char **argv;
    int local;
{
    int errs;
    int i;

    module_argc = argc;
    module_argv = (char **) xmalloc ((argc + 1) * sizeof (module_argv[0]));
    for (i = 0; i < argc; ++i)
	module_argv[i] = xstrdup (argv[i]);
    module_argv[argc] = NULL;

    for (i = 0; i < argc; ++i)
	send_arg (argv[i]);
    send_a_repository ("", current_parsed_root->directory, "");

    send_to_server ("expand-modules\012", 0);

    errs = get_server_responses ();
    if (last_repos != NULL)
        free (last_repos);
    last_repos = NULL;
    if (last_update_dir != NULL)
        free (last_update_dir);
    last_update_dir = NULL;
    if (errs)
	error (errs, 0, "cannot expand modules");
}

void
client_send_expansions (local, where, build_dirs)
    int local;
    char *where;
    int build_dirs;
{
    int i;
    char *argv[1];

    /* Send the original module names.  The "expanded" module name might
       not be suitable as an argument to a co request (e.g. it might be
       the result of a -d argument in the modules file).  It might be
       cleaner if we genuinely expanded module names, all the way to a
       local directory and repository, but that isn't the way it works
       now.  */
    send_file_names (module_argc, module_argv, 0);

    for (i = 0; i < modules_count; ++i)
    {
	argv[0] = where ? where : modules_vector[i];
	if (isfile (argv[0]))
	    send_files (1, argv, local, 0, build_dirs ? SEND_BUILD_DIRS : 0);
    }
    send_a_repository ("", current_parsed_root->directory, "");
}

void
client_nonexpanded_setup ()
{
    send_a_repository ("", current_parsed_root->directory, "");
}

/* Receive a cvswrappers line from the server; it must be a line
   containing an RCS option (e.g., "*.exe   -k 'b'").

   Note that this doesn't try to handle -t/-f options (which are a
   whole separate issue which noone has thought much about, as far
   as I know).

   We need to know the keyword expansion mode so we know whether to
   read the file in text or binary mode.  */

static void
handle_wrapper_rcs_option (args, len)
    char *args;
    int len;
{
    char *p;

    /* Enforce the notes in cvsclient.texi about how the response is not
       as free-form as it looks.  */
    p = strchr (args, ' ');
    if (p == NULL)
	goto handle_error;
    if (*++p != '-'
	|| *++p != 'k'
	|| *++p != ' '
	|| *++p != '\'')
	goto handle_error;
    if (strchr (p, '\'') == NULL)
	goto handle_error;

    /* Add server-side cvswrappers line to our wrapper list. */
    wrap_add (args, 0);
    return;
 handle_error:
    error (0, errno, "protocol error: ignoring invalid wrappers %s", args);
}


static void
handle_m (args, len)
    char *args;
    int len;
{
    /* In the case where stdout and stderr point to the same place,
       fflushing stderr will make output happen in the correct order.
       Often stderr will be line-buffered and this won't be needed,
       but not always (is that true?  I think the comment is probably
       based on being confused between default buffering between
       stdout and stderr.  But I'm not sure).  */
    fflush (stderr);
    fwrite (args, len, sizeof (*args), stdout);
    putc ('\n', stdout);
}

static void handle_mbinary PROTO ((char *, int));

static void
handle_mbinary (args, len)
    char *args;
    int len;
{
    char *size_string;
    size_t size;
    size_t totalread;
    size_t nread;
    size_t toread;
    char buf[8192];

    /* See comment at handle_m about (non)flush of stderr.  */

    /* Get the size.  */
    read_line (&size_string);
    size = atoi (size_string);
    free (size_string);

    /* OK, now get all the data.  The algorithm here is that we read
       as much as the network wants to give us in
       try_read_from_server, and then we output it all, and then
       repeat, until we get all the data.  */
    totalread = 0;
    while (totalread < size)
    {
	toread = size - totalread;
	if (toread > sizeof buf)
	    toread = sizeof buf;

	nread = try_read_from_server (buf, toread);
	cvs_output_binary (buf, nread);
	totalread += nread;
    }
}

static void
handle_e (args, len)
    char *args;
    int len;
{
    /* In the case where stdout and stderr point to the same place,
       fflushing stdout will make output happen in the correct order.  */
    fflush (stdout);
    fwrite (args, len, sizeof (*args), stderr);
    putc ('\n', stderr);
}

/*ARGSUSED*/
static void
handle_f (args, len)
    char *args;
    int len;
{
    fflush (stderr);
}

static void handle_mt PROTO ((char *, int));

static void
handle_mt (args, len)
    char *args;
    int len;
{
    char *p;
    char *tag = args;
    char *text;

    /* See comment at handle_m for more details.  */
    fflush (stderr);

    p = strchr (args, ' ');
    if (p == NULL)
	text = NULL;
    else
    {
	*p++ = '\0';
	text = p;
    }

    switch (tag[0])
    {
	case '+':
	    if (strcmp (tag, "+updated") == 0)
		updated_seen = 1;
	    else if (strcmp (tag, "+importmergecmd") == 0)
		importmergecmd.seen = 1;
	    break;
	case '-':
	    if (strcmp (tag, "-updated") == 0)
		updated_seen = 0;
	    else if (strcmp (tag, "-importmergecmd") == 0)
	    {
		char buf[80];

		/* Now that we have gathered the information, we can
                   output the suggested merge command.  */

		if (importmergecmd.conflicts == 0
		    || importmergecmd.mergetag1 == NULL
		    || importmergecmd.mergetag2 == NULL
		    || importmergecmd.repository == NULL)
		{
		    error (0, 0,
			   "invalid server: incomplete importmergecmd tags");
		    break;
		}

		sprintf (buf, "\n%d conflicts created by this import.\n",
			 importmergecmd.conflicts);
		cvs_output (buf, 0);
		cvs_output ("Use the following command to help the merge:\n\n",
			    0);
		cvs_output ("\t", 1);
		cvs_output (program_name, 0);
		if (CVSroot_cmdline != NULL)
		{
		    cvs_output (" -d ", 0);
		    cvs_output (CVSroot_cmdline, 0);
		}
		cvs_output (" checkout -j", 0);
		cvs_output (importmergecmd.mergetag1, 0);
		cvs_output (" -j", 0);
		cvs_output (importmergecmd.mergetag2, 0);
		cvs_output (" ", 1);
		cvs_output (importmergecmd.repository, 0);
		cvs_output ("\n\n", 0);

		/* Clear the static variables so that everything is
                   ready for any subsequent importmergecmd tag.  */
		importmergecmd.conflicts = 0;
		free (importmergecmd.mergetag1);
		importmergecmd.mergetag1 = NULL;
		free (importmergecmd.mergetag2);
		importmergecmd.mergetag2 = NULL;
		free (importmergecmd.repository);
		importmergecmd.repository = NULL;

		importmergecmd.seen = 0;
	    }
	    break;
	default:
	    if (updated_seen)
	    {
		if (strcmp (tag, "fname") == 0)
		{
		    if (updated_fname != NULL)
		    {
			/* Output the previous message now.  This can happen
			   if there was no Update-existing or other such
			   response, due to the -n global option.  */
			cvs_output ("U ", 0);
			cvs_output (updated_fname, 0);
			cvs_output ("\n", 1);
			free (updated_fname);
		    }
		    updated_fname = xstrdup (text);
		}
		/* Swallow all other tags.  Either they are extraneous
		   or they reflect future extensions that we can
		   safely ignore.  */
	    }
	    else if (importmergecmd.seen)
	    {
		if (strcmp (tag, "conflicts") == 0)
		    importmergecmd.conflicts = atoi (text);
		else if (strcmp (tag, "mergetag1") == 0)
		    importmergecmd.mergetag1 = xstrdup (text);
		else if (strcmp (tag, "mergetag2") == 0)
		    importmergecmd.mergetag2 = xstrdup (text);
		else if (strcmp (tag, "repository") == 0)
		    importmergecmd.repository = xstrdup (text);
		/* Swallow all other tags.  Either they are text for
                   which we are going to print our own version when we
                   see -importmergecmd, or they are future extensions
                   we can safely ignore.  */
	    }
	    else if (strcmp (tag, "newline") == 0)
		printf ("\n");
	    else if (text != NULL)
		printf ("%s", text);
    }
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
    RSP_LINE("Created", handle_created, response_type_normal, rs_optional),
    RSP_LINE("Update-existing", handle_update_existing, response_type_normal,
       rs_optional),
    RSP_LINE("Merged", handle_merged, response_type_normal, rs_essential),
    RSP_LINE("Patched", handle_patched, response_type_normal, rs_optional),
    RSP_LINE("Rcs-diff", handle_rcs_diff, response_type_normal, rs_optional),
    RSP_LINE("Mode", handle_mode, response_type_normal, rs_optional),
    RSP_LINE("Mod-time", handle_mod_time, response_type_normal, rs_optional),
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
    RSP_LINE("Template", handle_template, response_type_normal,
       rs_optional),
    RSP_LINE("Set-checkin-prog", handle_set_checkin_prog, response_type_normal,
       rs_optional),
    RSP_LINE("Set-update-prog", handle_set_update_prog, response_type_normal,
       rs_optional),
    RSP_LINE("Notified", handle_notified, response_type_normal, rs_optional),
    RSP_LINE("Module-expansion", handle_module_expansion, response_type_normal,
       rs_optional),
    RSP_LINE("Wrapper-rcsOption", handle_wrapper_rcs_option,
       response_type_normal,
       rs_optional),
    RSP_LINE("M", handle_m, response_type_normal, rs_essential),
    RSP_LINE("Mbinary", handle_mbinary, response_type_normal, rs_optional),
    RSP_LINE("E", handle_e, response_type_normal, rs_essential),
    RSP_LINE("F", handle_f, response_type_normal, rs_optional),
    RSP_LINE("MT", handle_mt, response_type_normal, rs_optional),
    /* Possibly should be response_type_error.  */
    RSP_LINE(NULL, NULL, response_type_normal, rs_essential)

#undef RSP_LINE
};

#endif /* CLIENT_SUPPORT or SERVER_SUPPORT */
#ifdef CLIENT_SUPPORT

/* 
 * If LEN is 0, then send_to_server() computes string's length itself.
 *
 * Therefore, pass the real length when transmitting data that might
 * contain 0's.
 */
void
send_to_server (str, len)
     char *str;
     size_t len;
{
    static int nbytes;

    if (len == 0)
	len = strlen (str);

    buf_output (to_server, str, len);
      
    /* There is no reason not to send data to the server, so do it
       whenever we've accumulated enough information in the buffer to
       make it worth sending.  */
    nbytes += len;
    if (nbytes >= 2 * BUFFER_DATA_SIZE)
    {
	int status;

        status = buf_send_output (to_server);
	if (status != 0)
	    error (1, status, "error writing to server");
	nbytes = 0;
    }
}

/* Read up to LEN bytes from the server.  Returns actual number of
   bytes read, which will always be at least one; blocks if there is
   no data available at all.  Gives a fatal error on EOF or error.  */
static size_t
try_read_from_server (buf, len)
    char *buf;
    size_t len;
{
    int status, nread;
    char *data;

    status = buf_read_data (from_server, len, &data, &nread);
    if (status != 0)
    {
	if (status == -1)
	    error (1, 0,
		   "end of file from server (consult above messages if any)");
	else if (status == -2)
	    error (1, 0, "out of memory");
	else
	    error (1, status, "reading from server");
    }

    memcpy (buf, data, nread);

    return nread;
}

/*
 * Read LEN bytes from the server or die trying.
 */
void
read_from_server (buf, len)
    char *buf;
    size_t len;
{
    size_t red = 0;
    while (red < len)
    {
	red += try_read_from_server (buf + red, len - red);
	if (red == len)
	    break;
    }
}

/*
 * Get some server responses and process them.  Returns nonzero for
 * error, 0 for success.  */
int
get_server_responses ()
{
    struct response *rs;
    do
    {
	char *cmd;
	int len;
	
	len = read_line (&cmd);
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
	    /* We might want to handle control characters and the like
	       in some other way other than just sending them to stdout.
	       One common reason for this error is if people use :ext:
	       with a version of rsh which is doing CRLF translation or
	       something, and so the client gets "ok^M" instead of "ok".
	       Right now that will tend to print part of this error
	       message over the other part of it.  It seems like we could
	       do better (either in general, by quoting or omitting all
	       control characters, and/or specifically, by detecting the CRLF
	       case and printing a specific error message).  */
	    error (0, 0,
		   "warning: unrecognized response `%s' from cvs server",
		   cmd);
	free (cmd);
    } while (rs->type == response_type_normal);

    if (updated_fname != NULL)
    {
	/* Output the previous message now.  This can happen
	   if there was no Update-existing or other such
	   response, due to the -n global option.  */
	cvs_output ("U ", 0);
	cvs_output (updated_fname, 0);
	cvs_output ("\n", 1);
	free (updated_fname);
	updated_fname = NULL;
    }

    if (rs->type == response_type_error)
	return 1;
    if (failure_exit)
	return 1;
    return 0;
}

/* Get the responses and then close the connection.  */

/*
 * Flag var; we'll set it in start_server() and not one of its
 * callees, such as start_rsh_server().  This means that there might
 * be a small window between the starting of the server and the
 * setting of this var, but all the code in that window shouldn't care
 * because it's busy checking return values to see if the server got
 * started successfully anyway.
 */
int server_started = 0;

int
get_responses_and_close ()
{
    int errs = get_server_responses ();
    int status;

    if (last_entries != NULL)
    {
	Entries_Close (last_entries);
	last_entries = NULL;
    }

    do_deferred_progs ();

    if (client_prune_dirs)
	process_prune_candidates ();

    /* First we shut down TO_SERVER.  That tells the server that its input is
     * finished.  It then shuts down the buffer it is sending to us, at which
     * point our shut down of FROM_SERVER will complete.
     */

    status = buf_shutdown (to_server);
    if (status != 0)
	error (0, status, "shutting down buffer to server");
    buf_free (to_server);
    to_server = NULL;

    status = buf_shutdown (from_server);
    if (status != 0)
	error (0, status, "shutting down buffer from server");
    buf_free (from_server);
    from_server = NULL;
    server_started = 0;

    /* see if we need to sleep before returning to avoid time-stamp races */
    if (last_register_time)
    {
	sleep_past (last_register_time);
    }

    return errs;
}
	
#ifndef NO_EXT_METHOD
static void start_rsh_server PROTO((cvsroot_t *, struct buffer **, struct buffer **));
#endif

int
supported_request (name)
    char *name;
{
    struct request *rq;

    for (rq = requests; rq->name; rq++)
	if (!strcmp (rq->name, name))
	    return (rq->flags & RQ_SUPPORTED) != 0;
    error (1, 0, "internal error: testing support for unknown option?");
    /* NOTREACHED */
    return 0;
}



#if defined (AUTH_CLIENT_SUPPORT) || defined (HAVE_KERBEROS)
static struct hostent *init_sockaddr PROTO ((struct sockaddr_in *, char *,
					     unsigned int));

static struct hostent *
init_sockaddr (name, hostname, port)
    struct sockaddr_in *name;
    char *hostname;
    unsigned int port;
{
    struct hostent *hostinfo;
    unsigned short shortport = port;

    memset (name, 0, sizeof (*name));
    name->sin_family = AF_INET;
    name->sin_port = htons (shortport);
    hostinfo = gethostbyname (hostname);
    if (hostinfo == NULL)
    {
	fprintf (stderr, "Unknown host %s.\n", hostname);
	error_exit ();
    }
    name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
    return hostinfo;
}

#endif /* defined (AUTH_CLIENT_SUPPORT) || defined (HAVE_KERBEROS) */



#ifdef AUTH_CLIENT_SUPPORT

/* Generic function to do port number lookup tasks.
 *
 * In order of precedence, will return:
 * 	getenv (envname), if defined
 * 	getservbyname (portname), if defined
 * 	defaultport
 */
static int
get_port_number (envname, portname, defaultport)
    const char *envname;
    const char *portname;
    int defaultport;
{
    struct servent *s;
    char *port_s;

    if (envname && (port_s = getenv (envname)))
    {
	int port = atoi (port_s);
	if (port <= 0)
	{
	    error (0, 0, "%s must be a positive integer!  If you", envname);
	    error (0, 0, "are trying to force a connection via rsh, please");
	    error (0, 0, "put \":server:\" at the beginning of your CVSROOT");
	    error (1, 0, "variable.");
	}
	return port;
    }
    else if (portname && (s = getservbyname (portname, "tcp")))
	return ntohs (s->s_port);
    else
	return defaultport;
}



/* get the port number for a client to connect to based on the port
 * and method of a cvsroot_t.
 *
 * we do this here instead of in parse_cvsroot so that we can keep network
 * code confined to a localized area and also to delay the lookup until the
 * last possible moment so it remains possible to run cvs client commands that
 * skip opening connections to the server (i.e. skip network operations
 * entirely)
 *
 * and yes, I know none of the commands do that now, but here's to planning
 * for the future, eh?  cheers.
 *
 * FIXME - We could cache the port lookup safely right now as we never change
 * it for a single root on the fly, but we'd have to un'const some other
 * functions - REMOVE_FIXME? This may be unecessary.  We're talking about,
 * what, usually one, sometimes two lookups of the port per invocation.  I
 * think twice is by far the rarer of the two cases - only the login function
 * will need to do it to save the canonical CVSROOT. -DRP
 */
int
get_cvs_port_number (root)
    const cvsroot_t *root;
{

    if (root->port) return root->port;

    switch (root->method)
    {
	case gserver_method:
	case pserver_method:
	    return get_port_number ("CVS_CLIENT_PORT", "cvspserver", CVS_AUTH_PORT);
#ifdef HAVE_KERBEROS
	case kserver_method:
	    return get_port_number ("CVS_CLIENT_PORT", "cvs", CVS_PORT);
#endif
	default:
	    error(1, EINVAL, "internal error: get_cvs_port_number called for invalid connection method (%s)",
		    method_names[root->method]);
	    break;
    }
    /* NOTREACHED */
    return -1;
}



void
make_bufs_from_fds (tofd, fromfd, child_pid, to_server, from_server, is_sock)
    int tofd;
    int fromfd;
    int child_pid;
    struct buffer **to_server;
    struct buffer **from_server;
    int is_sock;
{
    FILE *to_server_fp;
    FILE *from_server_fp;

#ifdef NO_SOCKET_TO_FD
    if (is_sock)
    {
	assert (tofd == fromfd);
	*to_server = socket_buffer_initialize (tofd, 0,
					      (BUFMEMERRPROC) NULL);
	*from_server = socket_buffer_initialize (tofd, 1,
						(BUFMEMERRPROC) NULL);
    }
    else
#endif /* NO_SOCKET_TO_FD */
    {
	/* todo: some OS's don't need these calls... */
	close_on_exec (tofd);
	close_on_exec (fromfd);

	/* SCO 3 and AIX have a nasty bug in the I/O libraries which precludes
	   fdopening the same file descriptor twice, so dup it if it is the
	   same.  */
	if (tofd == fromfd)
	{
	    fromfd = dup (tofd);
	    if (fromfd < 0)
		error (1, errno, "cannot dup net connection");
	}

	/* These will use binary mode on systems which have it.  */
	/*
	 * Also, we know that from_server is shut down second, so we pass
	 * child_pid in there.  In theory, it should be stored in both
	 * buffers with a ref count...
	 */
	to_server_fp = fdopen (tofd, FOPEN_BINARY_WRITE);
	if (to_server_fp == NULL)
	    error (1, errno, "cannot fdopen %d for write", tofd);
	*to_server = stdio_buffer_initialize (to_server_fp, 0, 0,
					     (BUFMEMERRPROC) NULL);

	from_server_fp = fdopen (fromfd, FOPEN_BINARY_READ);
	if (from_server_fp == NULL)
	    error (1, errno, "cannot fdopen %d for read", fromfd);
	*from_server = stdio_buffer_initialize (from_server_fp, child_pid, 1,
					       (BUFMEMERRPROC) NULL);
    }
}



/* Connect to a forked server process. */

void
connect_to_forked_server (to_server, from_server)
    struct buffer **to_server;
    struct buffer **from_server;
{
    int tofd, fromfd;
    int child_pid;

    /* This is pretty simple.  All we need to do is choose the correct
       cvs binary and call piped_child. */

    char *command[3];

    command[0] = getenv ("CVS_SERVER");
    if (! command[0])
	command[0] = program_path;
    
    command[1] = "server";
    command[2] = NULL;

    if (trace)
    {
	fprintf (stderr, " -> Forking server: %s %s\n", command[0], command[1]);
    }

    child_pid = piped_child (command, &tofd, &fromfd);
    if (child_pid < 0)
	error (1, 0, "could not fork server process");

    make_bufs_from_fds (tofd, fromfd, child_pid, to_server, from_server, 0);
}

/* Connect to the authenticating server.

   If VERIFY_ONLY is non-zero, then just verify that the password is
   correct and then shutdown the connection.

   If VERIFY_ONLY is 0, then really connect to the server.

   If DO_GSSAPI is non-zero, then we use GSSAPI authentication rather
   than the pserver password authentication.

   If we fail to connect or if access is denied, then die with fatal
   error.  */
void
connect_to_pserver (root, to_server_p, from_server_p, verify_only, do_gssapi)
    cvsroot_t *root;
    struct buffer **to_server_p;
    struct buffer **from_server_p;
    int verify_only;
    int do_gssapi;
{
    int sock;
    int port_number;
    struct sockaddr_in client_sai;
    struct hostent *hostinfo;
    struct buffer *to_server, *from_server;

    sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
	error (1, 0, "cannot create socket: %s", SOCK_STRERROR (SOCK_ERRNO));
    }
    port_number = get_cvs_port_number (root);
    hostinfo = init_sockaddr (&client_sai, root->hostname, port_number);
    if (trace)
    {
	fprintf (stderr, " -> Connecting to %s(%s):%d\n",
		 root->hostname,
		 inet_ntoa (client_sai.sin_addr), port_number);
    }
    if (connect (sock, (struct sockaddr *) &client_sai, sizeof (client_sai))
	< 0)
	error (1, 0, "connect to %s(%s):%d failed: %s",
	       root->hostname,
	       inet_ntoa (client_sai.sin_addr),
	       port_number, SOCK_STRERROR (SOCK_ERRNO));

    make_bufs_from_fds (sock, sock, 0, &to_server, &from_server, 1);

    auth_server (root, to_server, from_server, verify_only, do_gssapi, hostinfo);

    if (verify_only)
    {
	int status;

	status = buf_shutdown (to_server);
	if (status != 0)
	    error (0, status, "shutting down buffer to server");
	buf_free (to_server);
	to_server = NULL;

	status = buf_shutdown (from_server);
	if (status != 0)
	    error (0, status, "shutting down buffer from server");
	buf_free (from_server);
	from_server = NULL;

	/* Don't need to set server_started = 0 since we don't set it to 1
	 * until returning from this call.
	 */
    }
    else
    {
	*to_server_p = to_server;
	*from_server_p = from_server;
    }

    return;
}



static void
auth_server (root, lto_server, lfrom_server, verify_only, do_gssapi, hostinfo)
    cvsroot_t *root;
    struct buffer *lto_server;
    struct buffer *lfrom_server;
    int verify_only;
    int do_gssapi;
    struct hostent *hostinfo;
{
    char *username;			/* the username we use to connect */
    char no_passwd = 0;			/* gets set if no password found */

    /* FIXME!!!!!!!!!!!!!!!!!!
     *
     * THIS IS REALLY UGLY!
     *
     * I'm setting the globals here so we can make calls to send_to_server &
     * read_line.  This happens again _after_ we return if we're not in
     * verify_only mode.  We should be relying on the values we passed in, but
     * sent_to_server and read_line don't require an outside buf yet.
     */
    to_server = lto_server;
    from_server = lfrom_server;

    /* Run the authorization mini-protocol before anything else. */
    if (do_gssapi)
    {
#ifdef HAVE_GSSAPI
	FILE *fp = stdio_buffer_get_file(lto_server);
	int fd = fp ? fileno(fp) : -1;
	struct stat s;

	if ((fd < 0) || (fstat (fd, &s) < 0) || !S_ISSOCK(s.st_mode))
	{
	    error (1, 0, "gserver currently only enabled for socket connections");
	}

	if (! connect_to_gserver (root, fd, hostinfo))
	{
	    error (1, 0,
		    "authorization failed: server %s rejected access to %s",
		    root->hostname, root->directory);
	}
#else
	error (1, 0, "This client does not support GSSAPI authentication");
#endif
    }
    else
    {
	char *begin      = NULL;
	char *password   = NULL;
	char *end        = NULL;
	
	if (verify_only)
	{
	    begin = "BEGIN VERIFICATION REQUEST";
	    end   = "END VERIFICATION REQUEST";
	}
	else
	{
	    begin = "BEGIN AUTH REQUEST";
	    end   = "END AUTH REQUEST";
	}

	/* Get the password, probably from ~/.cvspass. */
	password = get_cvs_password ();
	username = root->username ? root->username : getcaller();

	/* Send the empty string by default.  This is so anonymous CVS
	   access doesn't require client to have done "cvs login". */
	if (password == NULL) 
	{
	    no_passwd = 1;
	    password = scramble ("");
	}

	/* Announce that we're starting the authorization protocol. */
	send_to_server(begin, 0);
	send_to_server("\012", 1);

	/* Send the data the server needs. */
	send_to_server(root->directory, 0);
	send_to_server("\012", 1);
	send_to_server(username, 0);
	send_to_server("\012", 1);
	send_to_server(password, 0);
	send_to_server("\012", 1);

	/* Announce that we're ending the authorization protocol. */
	send_to_server(end, 0);
	send_to_server("\012", 1);

        /* Paranoia. */
        memset (password, 0, strlen (password));
    }

    {
	char *read_buf;

	/* Loop, getting responses from the server.  */
	while (1)
	{
	    read_line (&read_buf);

	    if (strcmp (read_buf, "I HATE YOU") == 0)
	    {
		/* Authorization not granted.
		 *
		 * This is a little confusing since we can reach this while loop in GSSAPI
		 * mode, but if GSSAPI authentication failed, we already jumped to the
		 * rejected label (there is no case where the connect_to_gserver function
		 * can return 1 and we will not receive "I LOVE YOU" from the server, barring
		 * broken connections and garbled messages, of course).
		 *
		 * i.e. This is a pserver specific error message and should be since
		 * GSSAPI doesn't use username.
		 */
		error (0, 0,
			"authorization failed: server %s rejected access to %s for user %s",
			root->hostname, root->directory, username);

		/* Output a special error message if authentication was attempted
		with no password -- the user should be made aware that they may
		have missed a step. */
		if (no_passwd)
		{
		    error (0, 0,
			    "used empty password; try \"cvs login\" with a real password");
		}
		error_exit();
	    }
	    else if (strncmp (read_buf, "E ", 2) == 0)
	    {
		fprintf (stderr, "%s\n", read_buf + 2);

		/* Continue with the authentication protocol.  */
	    }
	    else if (strncmp (read_buf, "error ", 6) == 0)
	    {
		char *p;

		/* First skip the code.  */
		p = read_buf + 6;
		while (*p != ' ' && *p != '\0')
		    ++p;

		/* Skip the space that follows the code.  */
		if (*p == ' ')
		    ++p;

		/* Now output the text.  */
		fprintf (stderr, "%s\n", p);
		error_exit();
	    }
	    else if (strcmp (read_buf, "I LOVE YOU") == 0)
	    {
		free (read_buf);
		break;
	    }
	    else
	    {
		error (1, 0, 
		       "unrecognized auth response from %s: %s", 
		       root->hostname, read_buf);
	    }
	    free (read_buf);
	}
    }
}
#endif /* AUTH_CLIENT_SUPPORT */



#ifdef HAVE_KERBEROS

/* This function has not been changed to deal with NO_SOCKET_TO_FD
   (i.e., systems on which sockets cannot be converted to file
   descriptors).  The first person to try building a kerberos client
   on such a system (OS/2, Windows 95, and maybe others) will have to
   take care of this.  */
void
start_tcp_server (root, to_server, from_server)
    cvsroot_t *root;
    struct buffer **to_server;
    struct buffer **from_server;
{
    int s;
    const char *portenv;
    int port;
    struct hostent *hp;
    struct sockaddr_in sin;
    char *hname;

    s = socket (AF_INET, SOCK_STREAM, 0);
    if (s < 0)
	error (1, 0, "cannot create socket: %s", SOCK_STRERROR (SOCK_ERRNO));

    port = get_cvs_port_number (root);

    hp = init_sockaddr (&sin, root->hostname, port);

    hname = xmalloc (strlen (hp->h_name) + 1);
    strcpy (hname, hp->h_name);
  
    if (trace)
    {
	fprintf (stderr, " -> Connecting to %s(%s):%d\n",
		 root->hostname,
		 inet_ntoa (sin.sin_addr), port);
    }

    if (connect (s, (struct sockaddr *) &sin, sizeof sin) < 0)
	error (1, 0, "connect to %s(%s):%d failed: %s",
	       root->hostname,
	       inet_ntoa (sin.sin_addr),
	       port, SOCK_STRERROR (SOCK_ERRNO));

    {
	const char *realm;
	struct sockaddr_in laddr;
	int laddrlen;
	KTEXT_ST ticket;
	MSG_DAT msg_data;
	CREDENTIALS cred;
	int status;

	realm = krb_realmofhost (hname);

	laddrlen = sizeof (laddr);
	if (getsockname (s, (struct sockaddr *) &laddr, &laddrlen) < 0)
	    error (1, 0, "getsockname failed: %s", SOCK_STRERROR (SOCK_ERRNO));

	/* We don't care about the checksum, and pass it as zero.  */
	status = krb_sendauth (KOPT_DO_MUTUAL, s, &ticket, "rcmd",
			       hname, realm, (unsigned long) 0, &msg_data,
			       &cred, sched, &laddr, &sin, "KCVSV1.0");
	if (status != KSUCCESS)
	    error (1, 0, "kerberos authentication failed: %s",
		   krb_get_err_text (status));
	memcpy (kblock, cred.session, sizeof (C_Block));
    }

    close_on_exec (s);

    free (hname);

    /* Give caller the values it wants. */
    make_bufs_from_fds (s, s, 0, to_server, from_server, 1);
}

#endif /* HAVE_KERBEROS */

#ifdef HAVE_GSSAPI

/* Receive a given number of bytes.  */

static void
recv_bytes (sock, buf, need)
     int sock;
     char *buf;
     int need;
{
    while (need > 0)
    {
	int got;

	got = recv (sock, buf, need, 0);
	if (got <= 0)
	    error (1, 0, "recv() from server %s: %s", current_parsed_root->hostname,
		   got == 0 ? "EOF" : SOCK_STRERROR (SOCK_ERRNO));

	buf += got;
	need -= got;
    }
}

/* Connect to the server using GSSAPI authentication.  */

/* FIXME
 *
 * This really needs to be rewritten to use a buffer and not a socket.
 * This would enable gserver to work with the SSL code I'm about to commit
 * since the SSL connection is going to look like a FIFO and not a socket.
 *
 * I think, basically, it will need to use buf_output and buf_read directly
 * since I don't think there is a read_bytes function - only read_line.
 *
 * recv_bytes could then be removed too.
 *
 * Besides, I added some cruft to reenable the socket which shouldn't be
 * there.  This would also enable its removal.
 */
#define BUFSIZE 1024
static int
connect_to_gserver (root, sock, hostinfo)
    cvsroot_t *root;
    int sock;
    struct hostent *hostinfo;
{
    char *str;
    char buf[BUFSIZE];
    gss_buffer_desc *tok_in_ptr, tok_in, tok_out;
    OM_uint32 stat_min, stat_maj;
    gss_name_t server_name;

    str = "BEGIN GSSAPI REQUEST\012";

    if (send (sock, str, strlen (str), 0) < 0)
	error (1, 0, "cannot send: %s", SOCK_STRERROR (SOCK_ERRNO));

    if (strlen (hostinfo->h_name) > BUFSIZE - 5)
	error (1, 0, "Internal error: hostname exceeds length of buffer");
    sprintf (buf, "cvs@%s", hostinfo->h_name);
    tok_in.length = strlen (buf);
    tok_in.value = buf;
    gss_import_name (&stat_min, &tok_in, GSS_C_NT_HOSTBASED_SERVICE,
		     &server_name);

    tok_in_ptr = GSS_C_NO_BUFFER;
    gcontext = GSS_C_NO_CONTEXT;

    do
    {
	stat_maj = gss_init_sec_context (&stat_min, GSS_C_NO_CREDENTIAL,
					 &gcontext, server_name,
					 GSS_C_NULL_OID,
					 (GSS_C_MUTUAL_FLAG
					  | GSS_C_REPLAY_FLAG),
					 0, NULL, tok_in_ptr, NULL, &tok_out,
					 NULL, NULL);
	if (stat_maj != GSS_S_COMPLETE && stat_maj != GSS_S_CONTINUE_NEEDED)
	{
	    OM_uint32 message_context;
	    OM_uint32 new_stat_min;

	    message_context = 0;
	    gss_display_status (&new_stat_min, stat_maj, GSS_C_GSS_CODE,
                                GSS_C_NULL_OID, &message_context, &tok_out);
	    error (0, 0, "GSSAPI authentication failed: %s",
		   (char *) tok_out.value);

	    message_context = 0;
	    gss_display_status (&new_stat_min, stat_min, GSS_C_MECH_CODE,
				GSS_C_NULL_OID, &message_context, &tok_out);
	    error (1, 0, "GSSAPI authentication failed: %s",
		   (char *) tok_out.value);
	}

	if (tok_out.length == 0)
	{
	    tok_in.length = 0;
	}
	else
	{
	    char cbuf[2];
	    int need;

	    cbuf[0] = (tok_out.length >> 8) & 0xff;
	    cbuf[1] = tok_out.length & 0xff;
	    if (send (sock, cbuf, 2, 0) < 0)
		error (1, 0, "cannot send: %s", SOCK_STRERROR (SOCK_ERRNO));
	    if (send (sock, tok_out.value, tok_out.length, 0) < 0)
		error (1, 0, "cannot send: %s", SOCK_STRERROR (SOCK_ERRNO));

	    recv_bytes (sock, cbuf, 2);
	    need = ((cbuf[0] & 0xff) << 8) | (cbuf[1] & 0xff);

	    if (need > sizeof buf)
	    {
		int got;

		/* This usually means that the server sent us an error
		   message.  Read it byte by byte and print it out.
		   FIXME: This is a terrible error handling strategy.
		   However, even if we fix the server, we will still
		   want to do this to work with older servers.  */
		buf[0] = cbuf[0];
		buf[1] = cbuf[1];
		got = recv (sock, buf + 2, sizeof buf - 2, 0);
		if (got < 0)
		    error (1, 0, "recv() from server %s: %s",
			   root->hostname, SOCK_STRERROR (SOCK_ERRNO));
		buf[got + 2] = '\0';
		if (buf[got + 1] == '\n')
		    buf[got + 1] = '\0';
		error (1, 0, "error from server %s: %s", root->hostname,
		       buf);
	    }

	    recv_bytes (sock, buf, need);
	    tok_in.length = need;
	}

	tok_in.value = buf;
	tok_in_ptr = &tok_in;
    }
    while (stat_maj == GSS_S_CONTINUE_NEEDED);

    return 1;
}

#endif /* HAVE_GSSAPI */

static int send_variable_proc PROTO ((Node *, void *));

static int
send_variable_proc (node, closure)
    Node *node;
    void *closure;
{
    send_to_server ("Set ", 0);
    send_to_server (node->key, 0);
    send_to_server ("=", 1);
    send_to_server (node->data, 0);
    send_to_server ("\012", 1);
    return 0;
}

/* Contact the server.  */
void
start_server ()
{
    int rootless;
    char *log = getenv ("CVS_CLIENT_LOG");

    /* Clear our static variables for this invocation. */
    if (toplevel_repos != NULL)
	free (toplevel_repos);
    toplevel_repos = NULL;


    /* Note that generally speaking we do *not* fall back to a different
       way of connecting if the first one does not work.  This is slow
       (*really* slow on a 14.4kbps link); the clean way to have a CVS
       which supports several ways of connecting is with access methods.  */

    switch (current_parsed_root->method)
    {

#ifdef AUTH_CLIENT_SUPPORT
	case pserver_method:
	    /* Toss the return value.  It will die with an error message if
	     * anything goes wrong anyway.
	     */
	    connect_to_pserver (current_parsed_root, &to_server, &from_server, 0, 0);
	    break;
#endif

#if HAVE_KERBEROS
	case kserver_method:
	    start_tcp_server (current_parsed_root, &to_server, &from_server);
	    break;
#endif

#ifdef HAVE_GSSAPI
	case gserver_method:
	    /* GSSAPI authentication is handled by the pserver.  */
	    connect_to_pserver (current_parsed_root, &to_server, &from_server, 0, 1);
	    break;
#endif

	case ext_method:
#if defined (NO_EXT_METHOD)
	    error (0, 0, ":ext: method not supported by this port of CVS");
	    error (1, 0, "try :server: instead");
#else
	    start_rsh_server (current_parsed_root, &to_server, &from_server);
#endif
	    break;

	case server_method:
#if defined(START_SERVER)
	    {
	    int tofd, fromfd;
	    START_SERVER (&tofd, &fromfd, getcaller (),
			  current_parsed_root->username, current_parsed_root->hostname,
			  current_parsed_root->directory);
# ifdef START_SERVER_RETURNS_SOCKET
	    make_bufs_from_fds (tofd, fromfd, 0, &to_server, &from_server, 1);
# else
	    make_bufs_from_fds (tofd, fromfd, 0, &to_server, &from_server, 0);
# endif /* START_SERVER_RETURNS_SOCKET */
	    }
#else
	    /* FIXME: It should be possible to implement this portably,
	       like pserver, which would get rid of the duplicated code
	       in {vms,windows-NT,...}/startserver.c.  */
	    error (1, 0,
"the :server: access method is not supported by this port of CVS");
#endif
	    break;

        case fork_method:
	    connect_to_forked_server (&to_server, &from_server);
	    break;

	default:
	    error (1, 0, "\
(start_server internal error): unknown access method");
	    break;
    }

    /* "Hi, I'm Darlene and I'll be your server tonight..." */
    server_started = 1;

    /* Set up logfiles, if any.
     *
     * We do this _after_ authentication on purpose.  Wouldn't really like to
     * worry about logging passwords...
     */
    if (log)
    {
	int len = strlen (log);
	char *buf = xmalloc (len + 5);
	char *p;
	FILE *fp;

	strcpy (buf, log);
	p = buf + len;

	/* Open logfiles in binary mode so that they reflect
	   exactly what was transmitted and received (that is
	   more important than that they be maximally
	   convenient to view).  */
	/* Note that if we create several connections in a single CVS client
	   (currently used by update.c), then the last set of logfiles will
	   overwrite the others.  There is currently no way around this.  */
	strcpy (p, ".in");
	fp = open_file (buf, "wb");
        if (fp == NULL)
	    error (0, errno, "opening to-server logfile %s", buf);
	else
	    to_server = log_buffer_initialize (to_server, fp, 0,
					       (BUFMEMERRPROC) NULL);

	strcpy (p, ".out");
	fp = open_file (buf, "wb");
        if (fp == NULL)
	    error (0, errno, "opening from-server logfile %s", buf);
	else
	    from_server = log_buffer_initialize (from_server, fp, 1,
						 (BUFMEMERRPROC) NULL);

	free (buf);
    }

    /* Clear static variables.  */
    if (toplevel_repos != NULL)
	free (toplevel_repos);
    toplevel_repos = NULL;
    if (last_dir_name != NULL)
	free (last_dir_name);
    last_dir_name = NULL;
    if (last_repos != NULL)
	free (last_repos);
    last_repos = NULL;
    if (last_update_dir != NULL)
	free (last_update_dir);
    last_update_dir = NULL;
    stored_checksum_valid = 0;
    if (stored_mode != NULL)
    {
	free (stored_mode);
	stored_mode = NULL;
    }

    rootless = (strcmp (command_name, "init") == 0);
    if (!rootless)
    {
	send_to_server ("Root ", 0);
	send_to_server (current_parsed_root->directory, 0);
	send_to_server ("\012", 1);
    }

    {
	struct response *rs;

	send_to_server ("Valid-responses", 0);

	for (rs = responses; rs->name != NULL; ++rs)
	{
	    send_to_server (" ", 0);
	    send_to_server (rs->name, 0);
	}
	send_to_server ("\012", 1);
    }
    send_to_server ("valid-requests\012", 0);

    if (get_server_responses ())
	error_exit ();

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
		send_to_server ("Global_option -n\012", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -n option.");
	}
	if (quiet)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -q\012", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -q option.");
	}
	if (really_quiet)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -Q\012", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -Q option.");
	}
	if (!cvswrite)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -r\012", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -r option.");
	}
	if (trace)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -t\012", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -t option.");
	}
	if (logoff)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -l\012", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -l option.");
	}
    }

    /* Find out about server-side cvswrappers.  An extra network
       turnaround for cvs import seems to be unavoidable, unless we
       want to add some kind of client-side place to configure which
       filenames imply binary.  For cvs add, we could avoid the
       problem by keeping a copy of the wrappers in CVSADM (the main
       reason to bother would be so we could make add work without
       contacting the server, I suspect).  */

    if ((strcmp (command_name, "import") == 0)
        || (strcmp (command_name, "add") == 0))
    {
	if (supported_request ("wrapper-sendme-rcsOptions"))
	{
	    int err;
	    send_to_server ("wrapper-sendme-rcsOptions\012", 0);
	    err = get_server_responses ();
	    if (err != 0)
		error (err, 0, "error reading from server");
	}
    }

    if (cvsencrypt && !rootless)
    {
#ifdef ENCRYPTION
	/* Turn on encryption before turning on compression.  We do
           not want to try to compress the encrypted stream.  Instead,
           we want to encrypt the compressed stream.  If we can't turn
           on encryption, bomb out; don't let the user think the data
           is being encrypted when it is not.  */
#ifdef HAVE_KERBEROS
	if (current_parsed_root->method == kserver_method)
	{
	    if (! supported_request ("Kerberos-encrypt"))
		error (1, 0, "This server does not support encryption");
	    send_to_server ("Kerberos-encrypt\012", 0);
	    to_server = krb_encrypt_buffer_initialize (to_server, 0, sched,
						       kblock,
						       (BUFMEMERRPROC) NULL);
	    from_server = krb_encrypt_buffer_initialize (from_server, 1,
							 sched, kblock,
							 (BUFMEMERRPROC) NULL);
	}
	else
#endif /* HAVE_KERBEROS */
#ifdef HAVE_GSSAPI
	if (current_parsed_root->method == gserver_method)
	{
	    if (! supported_request ("Gssapi-encrypt"))
		error (1, 0, "This server does not support encryption");
	    send_to_server ("Gssapi-encrypt\012", 0);
	    to_server = cvs_gssapi_wrap_buffer_initialize (to_server, 0,
							   gcontext,
							   ((BUFMEMERRPROC)
							    NULL));
	    from_server = cvs_gssapi_wrap_buffer_initialize (from_server, 1,
							     gcontext,
							     ((BUFMEMERRPROC)
							      NULL));
	    cvs_gssapi_encrypt = 1;
	}
	else
#endif /* HAVE_GSSAPI */
	    error (1, 0, "Encryption is only supported when using GSSAPI or Kerberos");
#else /* ! ENCRYPTION */
	error (1, 0, "This client does not support encryption");
#endif /* ! ENCRYPTION */
    }

    if (gzip_level && !rootless)
    {
	if (supported_request ("Gzip-stream"))
	{
	    char gzip_level_buf[5];
	    send_to_server ("Gzip-stream ", 0);
	    sprintf (gzip_level_buf, "%d", gzip_level);
	    send_to_server (gzip_level_buf, 0);
	    send_to_server ("\012", 1);

	    /* All further communication with the server will be
               compressed.  */

	    to_server = compress_buffer_initialize (to_server, 0, gzip_level,
						    (BUFMEMERRPROC) NULL);
	    from_server = compress_buffer_initialize (from_server, 1,
						      gzip_level,
						      (BUFMEMERRPROC) NULL);
	}
#ifndef NO_CLIENT_GZIP_PROCESS
	else if (supported_request ("gzip-file-contents"))
	{
            char gzip_level_buf[5];
	    send_to_server ("gzip-file-contents ", 0);
            sprintf (gzip_level_buf, "%d", gzip_level);
	    send_to_server (gzip_level_buf, 0);

	    send_to_server ("\012", 1);

	    file_gzip_level = gzip_level;
	}
#endif
	else
	{
	    fprintf (stderr, "server doesn't support gzip-file-contents\n");
	    /* Setting gzip_level to 0 prevents us from giving the
               error twice if update has to contact the server again
               to fetch unpatchable files.  */
	    gzip_level = 0;
	}
    }

    if (cvsauthenticate && ! cvsencrypt && !rootless)
    {
	/* Turn on authentication after turning on compression, so
	   that we can compress the authentication information.  We
	   assume that encrypted data is always authenticated--the
	   ability to decrypt the data stream is itself a form of
	   authentication.  */
#ifdef HAVE_GSSAPI
	if (current_parsed_root->method == gserver_method)
	{
	    if (! supported_request ("Gssapi-authenticate"))
		error (1, 0,
		       "This server does not support stream authentication");
	    send_to_server ("Gssapi-authenticate\012", 0);
	    to_server = cvs_gssapi_wrap_buffer_initialize (to_server, 0,
							   gcontext,
							   ((BUFMEMERRPROC)
							    NULL));
	    from_server = cvs_gssapi_wrap_buffer_initialize (from_server, 1,
							     gcontext,
							     ((BUFMEMERRPROC)
							      NULL));
	}
	else
	    error (1, 0, "Stream authentication is only supported when using GSSAPI");
#else /* ! HAVE_GSSAPI */
	error (1, 0, "This client does not support stream authentication");
#endif /* ! HAVE_GSSAPI */
    }

#ifdef FILENAMES_CASE_INSENSITIVE
    if (supported_request ("Case") && !rootless)
	send_to_server ("Case\012", 0);
#endif

    /* If "Set" is not supported, just silently fail to send the variables.
       Users with an old server should get a useful error message when it
       fails to recognize the ${=foo} syntax.  This way if someone uses
       several servers, some of which are new and some old, they can still
       set user variables in their .cvsrc without trouble.  */
    if (supported_request ("Set"))
	walklist (variable_list, send_variable_proc, NULL);
}

#ifndef NO_EXT_METHOD

/* Contact the server by starting it with rsh.  */

/* Right now, we have two different definitions for this function,
   depending on whether we start the rsh server using popenRW or not.
   This isn't ideal, and the best thing would probably be to change
   the OS/2 port to be more like the regular Unix client (i.e., by
   implementing piped_child)... but I'm doing something else at the
   moment, and wish to make only one change at a time.  -Karl */

# ifdef START_RSH_WITH_POPEN_RW

/* This is actually a crock -- it's OS/2-specific, for no one else
   uses it.  If I get time, I want to make piped_child and all the
   other stuff in os2/run.c work right.  In the meantime, this gets us
   up and running, and that's most important. */

static void
start_rsh_server (root, to_server, from_server)
    cvsroot_t *root;
    struct buffer **to_server;
    struct buffer **from_server;
{
    int pipes[2];
    int child_pid;

    /* If you're working through firewalls, you can set the
       CVS_RSH environment variable to a script which uses rsh to
       invoke another rsh on a proxy machine.  */
    char *cvs_rsh = getenv ("CVS_RSH");
    char *cvs_server = getenv ("CVS_SERVER");
    int i = 0;
    /* This needs to fit "rsh", "-b", "-l", "USER", "host",
       "cmd (w/ args)", and NULL.  We leave some room to grow. */
    char *rsh_argv[10];

    if (!cvs_rsh)
	/* People sometimes suggest or assume that this should default
	   to "remsh" on systems like HPUX in which that is the
	   system-supplied name for the rsh program.  However, that
	   causes various problems (keep in mind that systems such as
	   HPUX might have non-system-supplied versions of "rsh", like
	   a Kerberized one, which one might want to use).  If we
	   based the name on what is found in the PATH of the person
	   who runs configure, that would make it harder to
	   consistently produce the same result in the face of
	   different people producing binary distributions.  If we
	   based it on "remsh" always being the default for HPUX
	   (e.g. based on uname), that might be slightly better but
	   would require us to keep track of what the defaults are for
	   each system type, and probably would cope poorly if the
	   existence of remsh or rsh varies from OS version to OS
	   version.  Therefore, it seems best to have the default
	   remain "rsh", and tell HPUX users to specify remsh, for
	   example in CVS_RSH or other such mechanisms to be devised,
	   if that is what they want (the manual already tells them
	   that).  */
	cvs_rsh = "ssh";
    if (!cvs_server)
	cvs_server = "cvs";

    /* The command line starts out with rsh. */
    rsh_argv[i++] = cvs_rsh;

#   ifdef RSH_NEEDS_BINARY_FLAG
    /* "-b" for binary, under OS/2. */
    rsh_argv[i++] = "-b";
#   endif /* RSH_NEEDS_BINARY_FLAG */

    /* Then we strcat more things on the end one by one. */
    if (root->username != NULL)
    {
	rsh_argv[i++] = "-l";
	rsh_argv[i++] = root->username;
    }

    rsh_argv[i++] = root->hostname;
    rsh_argv[i++] = cvs_server;
    rsh_argv[i++] = "server";

    /* Mark the end of the arg list. */
    rsh_argv[i]   = (char *) NULL;

    if (trace)
    {
	fprintf (stderr, " -> Starting server: ");
	for (i = 0; rsh_argv[i]; i++)
	    fprintf (stderr, "%s ", rsh_argv[i]);
	putc ('\n', stderr);
    }

    /* Do the deed. */
    child_pid = popenRW (rsh_argv, pipes);
    if (child_pid < 0)
	error (1, errno, "cannot start server via rsh");

    /* Give caller the file descriptors in a form it can deal with. */
    make_bufs_from_fds (pipes[0], pipes[1], child_pid, to_server, from_server, 0);
}

# else /* ! START_RSH_WITH_POPEN_RW */

static void
start_rsh_server (root, to_server, from_server)
    cvsroot_t *root;
    struct buffer **to_server;
    struct buffer **from_server;
{
    /* If you're working through firewalls, you can set the
       CVS_RSH environment variable to a script which uses rsh to
       invoke another rsh on a proxy machine.  */
    char *cvs_rsh = getenv ("CVS_RSH");
    char *cvs_server = getenv ("CVS_SERVER");
    char *command;
    int tofd, fromfd;
    int child_pid;

    if (!cvs_rsh)
	cvs_rsh = "ssh";
    if (!cvs_server)
	cvs_server = "cvs";

    /* Pass the command to rsh as a single string.  This shouldn't
       affect most rsh servers at all, and will pacify some buggy
       versions of rsh that grab switches out of the middle of the
       command (they're calling the GNU getopt routines incorrectly).  */
    command = xmalloc (strlen (cvs_server) + 8);

    /* If you are running a very old (Nov 3, 1994, before 1.5)
     * version of the server, you need to make sure that your .bashrc
     * on the server machine does not set CVSROOT to something
     * containing a colon (or better yet, upgrade the server).  */
    sprintf (command, "%s server", cvs_server);

    {
        char *argv[10];
	char **p = argv;

	*p++ = cvs_rsh;
	*p++ = root->hostname;

	/* If the login names differ between client and server
	 * pass it on to rsh.
	 */
	if (root->username != NULL)
	{
	    *p++ = "-l";
	    *p++ = root->username;
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
	child_pid = piped_child (argv, &tofd, &fromfd);

	if (child_pid < 0)
	    error (1, errno, "cannot start server via rsh");
    }
    free (command);

    make_bufs_from_fds (tofd, fromfd, child_pid, to_server, from_server, 0);
}

# endif /* START_RSH_WITH_POPEN_RW */

#endif /* NO_EXT_METHOD */



/* Send an argument STRING.  */
void
send_arg (string)
    char *string;
{
    char buf[1];
    char *p = string;

    send_to_server ("Argument ", 0);

    while (*p)
    {
	if (*p == '\n')
	{
	    send_to_server ("\012Argumentx ", 0);
	}
	else
        {
	    buf[0] = *p;
	    send_to_server (buf, 1);
        }
	++p;
    }
    send_to_server ("\012", 1);
}

static void send_modified PROTO ((char *, char *, Vers_TS *));

/* VERS->OPTIONS specifies whether the file is binary or not.  NOTE: BEFORE
   using any other fields of the struct vers, we would need to fix
   client_process_import_file to set them up.  */

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
    size_t bufsize;
    int bin;

    if (trace)
	(void) fprintf (stderr, " -> Sending file `%s' to server\n", file);

    /* Don't think we can assume fstat exists.  */
    if ( CVS_STAT (file, &sb) < 0)
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

    if (vers && vers->options)
      bin = !(strcmp (vers->options, "-kb"));
    else
      bin = 0;

#ifdef BROKEN_READWRITE_CONVERSION
    if (!bin)
    {
	/* If only stdio, not open/write/etc., do text/binary
	   conversion, use convert_file which can compensate
	   (FIXME: we could just use stdio instead which would
	   avoid the whole problem).  */
	char tfile[1024]; strcpy(tfile, file); strcat(tfile, ".CVSBFCTMP");
	convert_file (file, O_RDONLY,
		      tfile, O_WRONLY | O_CREAT | O_TRUNC | OPEN_BINARY);
	fd = CVS_OPEN (tfile, O_RDONLY | OPEN_BINARY);
	if (fd < 0)
	    error (1, errno, "reading %s", short_pathname);
    }
    else
	fd = CVS_OPEN (file, O_RDONLY | OPEN_BINARY);
#else
    fd = CVS_OPEN (file, O_RDONLY | (bin ? OPEN_BINARY : 0));
#endif

    if (fd < 0)
	error (1, errno, "reading %s", short_pathname);

    if (file_gzip_level && sb.st_size > 100)
    {
	size_t newsize = 0;

	if (read_and_gzip (fd, short_pathname, (unsigned char **)&buf,
			   &bufsize, &newsize,
			   file_gzip_level))
	    error (1, 0, "aborting due to compression error");

	if (close (fd) < 0)
	    error (0, errno, "warning: can't close %s", short_pathname);

        {
          char tmp[80];

	  send_to_server ("Modified ", 0);
	  send_to_server (file, 0);
	  send_to_server ("\012", 1);
	  send_to_server (mode_string, 0);
	  send_to_server ("\012z", 2);
	  sprintf (tmp, "%lu\n", (unsigned long) newsize);
	  send_to_server (tmp, 0);

          send_to_server (buf, newsize);
        }
    }
    else
    {
    	int newsize;

        {
	    char *bufp = buf;
	    int len;

	    /* FIXME: This is gross.  It assumes that we might read
	       less than st_size bytes (true on NT), but not more.
	       Instead of this we should just be reading a block of
	       data (e.g. 8192 bytes), writing it to the network, and
	       so on until EOF.  */
	    while ((len = read (fd, bufp, (buf + sb.st_size) - bufp)) > 0)
	        bufp += len;

	    if (len < 0)
	        error (1, errno, "reading %s", short_pathname);

	    newsize = bufp - buf;
	}
	if (close (fd) < 0)
	    error (0, errno, "warning: can't close %s", short_pathname);

        {
          char tmp[80];

	  send_to_server ("Modified ", 0);
	  send_to_server (file, 0);
	  send_to_server ("\012", 1);
	  send_to_server (mode_string, 0);
	  send_to_server ("\012", 1);
          sprintf (tmp, "%lu\012", (unsigned long) newsize);
          send_to_server (tmp, 0);
        }
#ifdef BROKEN_READWRITE_CONVERSION
	if (!bin)
	{
	    char tfile[1024]; strcpy(tfile, file); strcat(tfile, ".CVSBFCTMP");
	    if (CVS_UNLINK (tfile) < 0)
		error (0, errno, "warning: can't remove temp file %s", tfile);
	}
#endif

	/*
	 * Note that this only ends with a newline if the file ended with
	 * one.
	 */
	if (newsize > 0)
	    send_to_server (buf, newsize);
    }
    free (buf);
    free (mode_string);
}

/* The address of an instance of this structure is passed to
   send_fileproc, send_filesdoneproc, and send_direntproc, as the
   callerdat parameter.  */

struct send_data
{
    /* Each of the following flags are zero for clear or nonzero for set.  */
    int build_dirs;
    int force;
    int no_contents;
    int backup_modified;
};

static int send_fileproc PROTO ((void *callerdat, struct file_info *finfo));

/* Deal with one file.  */
static int
send_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    struct send_data *args = (struct send_data *) callerdat;
    Vers_TS *vers;
    struct file_info xfinfo;
    /* File name to actually use.  Might differ in case from
       finfo->file.  */
    char *filename;

    send_a_repository ("", finfo->repository, finfo->update_dir);

    xfinfo = *finfo;
    xfinfo.repository = NULL;
    xfinfo.rcs = NULL;
    vers = Version_TS (&xfinfo, NULL, NULL, NULL, 0, 0);

    if (vers->entdata != NULL)
	filename = vers->entdata->user;
    else
	filename = finfo->file;

    if (vers->vn_user != NULL)
    {
	/* The Entries request.  */
	send_to_server ("Entry /", 0);
	send_to_server (filename, 0);
	send_to_server ("/", 0);
	send_to_server (vers->vn_user, 0);
	send_to_server ("/", 0);
	if (vers->ts_conflict != NULL)
	{
	    if (vers->ts_user != NULL &&
		strcmp (vers->ts_conflict, vers->ts_user) == 0)
		send_to_server ("+=", 0);
	    else
		send_to_server ("+modified", 0);
	}
	send_to_server ("/", 0);
	send_to_server (vers->entdata != NULL
			? vers->entdata->options
			: vers->options,
			0);
	send_to_server ("/", 0);
	if (vers->entdata != NULL && vers->entdata->tag)
	{
	    send_to_server ("T", 0);
	    send_to_server (vers->entdata->tag, 0);
	}
	else if (vers->entdata != NULL && vers->entdata->date)
          {
	    send_to_server ("D", 0);
	    send_to_server (vers->entdata->date, 0);
          }
	send_to_server ("\012", 1);
    }
    else
    {
	/* It seems a little silly to re-read this on each file, but
	   send_dirent_proc doesn't get called if filenames are specified
	   explicitly on the command line.  */
	wrap_add_file (CVSDOTWRAPPER, 1);

	if (wrap_name_has (filename, WRAP_RCSOPTION))
	{
	    /* No "Entry", but the wrappers did give us a kopt so we better
	       send it with "Kopt".  As far as I know this only happens
	       for "cvs add".  Question: is there any reason why checking
	       for options from wrappers isn't done in Version_TS?

	       Note: it might have been better to just remember all the
	       kopts on the client side, rather than send them to the server,
	       and have it send us back the same kopts.  But that seemed like
	       a bigger change than I had in mind making now.  */

	    if (supported_request ("Kopt"))
	    {
		char *opt;

		send_to_server ("Kopt ", 0);
		opt = wrap_rcsoption (filename, 1);
		send_to_server (opt, 0);
		send_to_server ("\012", 1);
		free (opt);
	    }
	    else
		error (0, 0,
		       "\
warning: ignoring -k options due to server limitations");
	}
    }

    if (vers->ts_user == NULL)
    {
	/*
	 * Do we want to print "file was lost" like normal CVS?
	 * Would it always be appropriate?
	 */
	/* File no longer exists.  Don't do anything, missing files
	   just happen.  */
    }
    else if (vers->ts_rcs == NULL
	     || args->force
	     || strcmp (vers->ts_user, vers->ts_rcs) != 0
	     || (vers->vn_user && *vers->vn_user == '0'))
    {
	if (args->no_contents
	    && supported_request ("Is-modified"))
	{
	    send_to_server ("Is-modified ", 0);
	    send_to_server (filename, 0);
	    send_to_server ("\012", 1);
	}
	else
	    send_modified (filename, finfo->fullname, vers);

        if (args->backup_modified)
        {
            char *bakname;
            bakname = backup_file (filename, vers->vn_user);
            /* This behavior is sufficiently unexpected to
               justify overinformativeness, I think. */
            if (! really_quiet)
                printf ("(Locally modified %s moved to %s)\n",
                        filename, bakname);
            free (bakname);
        }
    }
    else
    {
	send_to_server ("Unchanged ", 0);
	send_to_server (filename, 0);
	send_to_server ("\012", 1);
    }

    /* if this directory has an ignore list, add this file to it */
    if (ignlist)
    {
	Node *p;

	p = getnode ();
	p->type = FILES;
	p->key = xstrdup (finfo->file);
	(void) addnode (ignlist, p);
    }

    freevers_ts (&vers);
    return 0;
}

static void send_ignproc PROTO ((char *, char *));

static void
send_ignproc (file, dir)
    char *file;
    char *dir;
{
    if (ign_inhibit_server || !supported_request ("Questionable"))
    {
	if (dir[0] != '\0')
	    (void) printf ("? %s/%s\n", dir, file);
	else
	    (void) printf ("? %s\n", file);
    }
    else
    {
	send_to_server ("Questionable ", 0);
	send_to_server (file, 0);
	send_to_server ("\012", 1);
    }
}

static int send_filesdoneproc PROTO ((void *, int, char *, char *, List *));

static int
send_filesdoneproc (callerdat, err, repository, update_dir, entries)
    void *callerdat;
    int err;
    char *repository;
    char *update_dir;
    List *entries;
{
    /* if this directory has an ignore list, process it then free it */
    if (ignlist)
    {
	ignore_files (ignlist, entries, update_dir, send_ignproc);
	dellist (&ignlist);
    }

    return (err);
}

static Dtype send_dirent_proc PROTO ((void *, char *, char *, char *, List *));

/*
 * send_dirent_proc () is called back by the recursion processor before a
 * sub-directory is processed for update.
 * A return code of 0 indicates the directory should be
 * processed by the recursion code.  A return of non-zero indicates the
 * recursion code should skip this directory.
 *
 */
static Dtype
send_dirent_proc (callerdat, dir, repository, update_dir, entries)
    void *callerdat;
    char *dir;
    char *repository;
    char *update_dir;
    List *entries;
{
    struct send_data *args = (struct send_data *) callerdat;
    int dir_exists;
    char *cvsadm_name;

    if (ignore_directory (update_dir))
    {
	/* print the warm fuzzy message */
	if (!quiet)
	    error (0, 0, "Ignoring %s", update_dir);
        return (R_SKIP_ALL);
    }

    /*
     * If the directory does not exist yet (e.g. "cvs update -d foo"),
     * no need to send any files from it.  If the directory does not
     * have a CVS directory, then we pretend that it does not exist.
     * Otherwise, we will fail when trying to open the Entries file.
     * This case will happen when checking out a module defined as
     * ``-a .''.
     */
    cvsadm_name = xmalloc (strlen (dir) + sizeof (CVSADM) + 10);
    sprintf (cvsadm_name, "%s/%s", dir, CVSADM);
    dir_exists = isdir (cvsadm_name);
    free (cvsadm_name);

    /*
     * If there is an empty directory (e.g. we are doing `cvs add' on a
     * newly-created directory), the server still needs to know about it.
     */

    if (dir_exists)
    {
	/*
	 * Get the repository from a CVS/Repository file whenever possible.
	 * The repository variable is wrong if the names in the local
	 * directory don't match the names in the repository.
	 */
	char *repos = Name_Repository (dir, update_dir);
	send_a_repository (dir, repos, update_dir);
	free (repos);

	/* initialize the ignore list for this directory */
	ignlist = getlist ();
    }
    else
    {
	/* It doesn't make sense to send a non-existent directory,
	   because there is no way to get the correct value for
	   the repository (I suppose maybe via the expand-modules
	   request).  In the case where the "obvious" choice for
	   repository is correct, the server can figure out whether
	   to recreate the directory; in the case where it is wrong
	   (that is, does not match what modules give us), we might as
	   well just fail to recreate it.

	   Checking for noexec is a kludge for "cvs -n add dir".  */
	/* Don't send a non-existent directory unless we are building
           new directories (build_dirs is true).  Otherwise, CVS may
           see a D line in an Entries file, and recreate a directory
           which the user removed by hand.  */
	if (args->build_dirs && noexec)
	    send_a_repository (dir, repository, update_dir);
    }

    return (dir_exists ? R_PROCESS : R_SKIP_ALL);
}

static int send_dirleave_proc PROTO ((void *, char *, int, char *, List *));

/*
 * send_dirleave_proc () is called back by the recursion code upon leaving
 * a directory.  All it does is delete the ignore list if it hasn't already
 * been done (by send_filesdone_proc).
 */
/* ARGSUSED */
static int
send_dirleave_proc (callerdat, dir, err, update_dir, entries)
    void *callerdat;
    char *dir;
    int err;
    char *update_dir;
    List *entries;
{

    /* Delete the ignore list if it hasn't already been done.  */
    if (ignlist)
	dellist (&ignlist);
    return err;
}

/*
 * Send each option in a string to the server, one by one.
 * This assumes that the options are separated by spaces, for example
 * STRING might be "--foo -C5 -y".
 */

void
send_option_string (string)
    char *string;
{
    char *copy;
    char *p;

    copy = xstrdup (string);
    p = copy;
    while (1)
    {
        char *s;
	char l;

	for (s = p; *s != ' ' && *s != '\0'; s++)
	    ;
	l = *s;
	*s = '\0';
	if (s != p)
	    send_arg (p);
	if (l == '\0')
	    break;
	p = s + 1;
    }
    free (copy);
}


/* Send the names of all the argument files to the server.  */

void
send_file_names (argc, argv, flags)
    int argc;
    char **argv;
    unsigned int flags;
{
    int i;
    int level;
    int max_level;
    
    /* The fact that we do this here as well as start_recursion is a bit 
       of a performance hit.  Perhaps worth cleaning up someday.  */
    if (flags & SEND_EXPAND_WILD)
	expand_wild (argc, argv, &argc, &argv);

    /* Send Max-dotdot if needed.  */
    max_level = 0;
    for (i = 0; i < argc; ++i)
    {
	level = pathname_levels (argv[i]);
	if (level > max_level)
	    max_level = level;
    }
    if (max_level > 0)
    {
	if (supported_request ("Max-dotdot"))
	{
            char buf[10];
            sprintf (buf, "%d", max_level);

	    send_to_server ("Max-dotdot ", 0);
	    send_to_server (buf, 0);
	    send_to_server ("\012", 1);
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
    {
	char buf[1];
	char *p = argv[i];
	char *line = NULL;

	if (arg_should_not_be_sent_to_server (argv[i]))
	    continue;

#ifdef FILENAMES_CASE_INSENSITIVE
	/* We want to send the file name as it appears
	   in CVS/Entries.  We put this inside an ifdef
	   to avoid doing all these system calls in
	   cases where fncmp is just strcmp anyway.  */
	/* For now just do this for files in the local
	   directory.  Would be nice to handle the
	   non-local case too, though.  */
	/* The isdir check could more gracefully be replaced
	   with a way of having Entries_Open report back the
	   error to us and letting us ignore existence_error.
	   Or some such.  */
	if (p == last_component (p) && isdir (CVSADM))
	{
	    List *entries;
	    Node *node;

	    /* If we were doing non-local directory,
	       we would save_cwd, CVS_CHDIR
	       like in update.c:isemptydir.  */
	    /* Note that if we are adding a directory,
	       the following will read the entry
	       that we just wrote there, that is, we
	       will get the case specified on the
	       command line, not the case of the
	       directory in the filesystem.  This
	       is correct behavior.  */
	    entries = Entries_Open (0, NULL);
	    node = findnode_fn (entries, p);
	    if (node != NULL)
	    {
		line = xstrdup (node->key);
		p = line;
		delnode (node);
	    }
	    Entries_Close (entries);
	}
#endif /* FILENAMES_CASE_INSENSITIVE */

	send_to_server ("Argument ", 0);

	while (*p)
	{
	    if (*p == '\n')
	    {
		send_to_server ("\012Argumentx ", 0);
	    }
	    else if (ISDIRSEP (*p))
	    {
		buf[0] = '/';
		send_to_server (buf, 1);
	    }
	    else
	    {
		buf[0] = *p;
		send_to_server (buf, 1);
	    }
	    ++p;
	}
	send_to_server ("\012", 1);
	if (line != NULL)
	    free (line);
    }

    if (flags & SEND_EXPAND_WILD)
    {
	int i;
	for (i = 0; i < argc; ++i)
	    free (argv[i]);
	free (argv);
    }
}


/* Send Repository, Modified and Entry.  argc and argv contain only
  the files to operate on (or empty for everything), not options.
  local is nonzero if we should not recurse (-l option).  flags &
  SEND_BUILD_DIRS is nonzero if nonexistent directories should be
  sent.  flags & SEND_FORCE is nonzero if we should send unmodified
  files to the server as though they were modified.  flags &
  SEND_NO_CONTENTS means that this command only needs to know
  _whether_ a file is modified, not the contents.  Also sends Argument
  lines for argc and argv, so should be called after options are sent.  */
void
send_files (argc, argv, local, aflag, flags)
    int argc;
    char **argv;
    int local;
    int aflag;
    unsigned int flags;
{
    struct send_data args;
    int err;

    /*
     * aflag controls whether the tag/date is copied into the vers_ts.
     * But we don't actually use it, so I don't think it matters what we pass
     * for aflag here.
     */
    args.build_dirs = flags & SEND_BUILD_DIRS;
    args.force = flags & SEND_FORCE;
    args.no_contents = flags & SEND_NO_CONTENTS;
    args.backup_modified = flags & BACKUP_MODIFIED_FILES;
    err = start_recursion
	(send_fileproc, send_filesdoneproc,
	 send_dirent_proc, send_dirleave_proc, (void *) &args,
	 argc, argv, local, W_LOCAL, aflag, LOCK_NONE, (char *)NULL, 0);
    if (err)
	error_exit ();
    if (toplevel_repos == NULL)
	/*
	 * This happens if we are not processing any files,
	 * or for checkouts in directories without any existing stuff
	 * checked out.  The following assignment is correct for the
	 * latter case; I don't think toplevel_repos matters for the
	 * former.
	 */
	toplevel_repos = xstrdup (current_parsed_root->directory);
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
client_process_import_file (message, vfile, vtag, targc, targv, repository,
                            all_files_binary, modtime)
    char *message;
    char *vfile;
    char *vtag;
    int targc;
    char *targv[];
    char *repository;
    int all_files_binary;

    /* Nonzero for "import -d".  */
    int modtime;
{
    char *update_dir;
    char *fullname;
    Vers_TS vers;

    assert (toplevel_repos != NULL);

    if (strncmp (repository, toplevel_repos, strlen (toplevel_repos)) != 0)
	error (1, 0,
	       "internal error: pathname `%s' doesn't specify file in `%s'",
	       repository, toplevel_repos);

    if (strcmp (repository, toplevel_repos) == 0)
    {
	update_dir = "";
	fullname = xstrdup (vfile);
    }
    else
    {
	update_dir = repository + strlen (toplevel_repos) + 1;

	fullname = xmalloc (strlen (vfile) + strlen (update_dir) + 10);
	strcpy (fullname, update_dir);
	strcat (fullname, "/");
	strcat (fullname, vfile);
    }

    send_a_repository ("", repository, update_dir);
    if (all_files_binary)
    {
	vers.options = xmalloc (4); /* strlen("-kb") + 1 */
	strcpy (vers.options, "-kb");
    }
    else
    {
	vers.options = wrap_rcsoption (vfile, 1);
    }
    if (vers.options != NULL)
    {
	if (supported_request ("Kopt"))
	{
	    send_to_server ("Kopt ", 0);
	    send_to_server (vers.options, 0);
	    send_to_server ("\012", 1);
	}
	else
	    error (0, 0,
		   "warning: ignoring -k options due to server limitations");
    }
    if (modtime)
    {
	if (supported_request ("Checkin-time"))
	{
	    struct stat sb;
	    char *rcsdate;
	    char netdate[MAXDATELEN];

	    if (CVS_STAT (vfile, &sb) < 0)
		error (1, errno, "cannot stat %s", fullname);
	    rcsdate = date_from_time_t (sb.st_mtime);
	    date_to_internet (netdate, rcsdate);
	    free (rcsdate);

	    send_to_server ("Checkin-time ", 0);
	    send_to_server (netdate, 0);
	    send_to_server ("\012", 1);
	}
	else
	    error (0, 0,
		   "warning: ignoring -d option due to server limitations");
    }
    send_modified (vfile, fullname, &vers);
    if (vers.options != NULL)
	free (vers.options);
    free (fullname);
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
	toplevel_repos = xstrdup (current_parsed_root->directory);
    send_repository ("", toplevel_repos, ".");
}

static void
notified_a_file (data, ent_list, short_pathname, filename)
    char *data;
    List *ent_list;
    char *short_pathname;
    char *filename;
{
    FILE *fp;
    FILE *newf;
    size_t line_len = 8192;
    char *line = xmalloc (line_len);
    char *cp;
    int nread;
    int nwritten;
    char *p;

    fp = open_file (CVSADM_NOTIFY, "r");
    if (getline (&line, &line_len, fp) < 0)
    {
	if (feof (fp))
	    error (0, 0, "cannot read %s: end of file", CVSADM_NOTIFY);
	else
	    error (0, errno, "cannot read %s", CVSADM_NOTIFY);
	goto error_exit;
    }
    cp = strchr (line, '\t');
    if (cp == NULL)
    {
	error (0, 0, "malformed %s file", CVSADM_NOTIFY);
	goto error_exit;
    }
    *cp = '\0';
    if (strcmp (filename, line + 1) != 0)
    {
	error (0, 0, "protocol error: notified %s, expected %s", filename,
	       line + 1);
    }

    if (getline (&line, &line_len, fp) < 0)
    {
	if (feof (fp))
	{
	    free (line);
	    if (fclose (fp) < 0)
		error (0, errno, "cannot close %s", CVSADM_NOTIFY);
	    if ( CVS_UNLINK (CVSADM_NOTIFY) < 0)
		error (0, errno, "cannot remove %s", CVSADM_NOTIFY);
	    return;
	}
	else
	{
	    error (0, errno, "cannot read %s", CVSADM_NOTIFY);
	    goto error_exit;
	}
    }
    newf = open_file (CVSADM_NOTIFYTMP, "w");
    if (fputs (line, newf) < 0)
    {
	error (0, errno, "cannot write %s", CVSADM_NOTIFYTMP);
	goto error2;
    }
    while ((nread = fread (line, 1, line_len, fp)) > 0)
    {
	p = line;
	while ((nwritten = fwrite (p, 1, nread, newf)) > 0)
	{
	    nread -= nwritten;
	    p += nwritten;
	}
	if (ferror (newf))
	{
	    error (0, errno, "cannot write %s", CVSADM_NOTIFYTMP);
	    goto error2;
	}
    }
    if (ferror (fp))
    {
	error (0, errno, "cannot read %s", CVSADM_NOTIFY);
	goto error2;
    }
    if (fclose (newf) < 0)
    {
	error (0, errno, "cannot close %s", CVSADM_NOTIFYTMP);
	goto error_exit;
    }
    free (line);
    if (fclose (fp) < 0)
    {
	error (0, errno, "cannot close %s", CVSADM_NOTIFY);
	return;
    }

    {
        /* In this case, we want rename_file() to ignore noexec. */
        int saved_noexec = noexec;
        noexec = 0;
        rename_file (CVSADM_NOTIFYTMP, CVSADM_NOTIFY);
        noexec = saved_noexec;
    }

    return;
  error2:
    (void) fclose (newf);
  error_exit:
    free (line);
    (void) fclose (fp);
}

static void
handle_notified (args, len)
    char *args;
    int len;
{
    call_in_directory (args, notified_a_file, NULL);
}

void
client_notify (repository, update_dir, filename, notif_type, val)
    char *repository;
    char *update_dir;
    char *filename;
    int notif_type;
    char *val;
{
    char buf[2];

    send_a_repository ("", repository, update_dir);
    send_to_server ("Notify ", 0);
    send_to_server (filename, 0);
    send_to_server ("\012", 1);
    buf[0] = notif_type;
    buf[1] = '\0';
    send_to_server (buf, 1);
    send_to_server ("\t", 1);
    send_to_server (val, 0);
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

    send_to_server ("Argument ", 0);
    send_to_server (option, 0);
    send_to_server ("\012", 1);

    send_arg (arg);
}

/* Send a date to the server.  The input DATE is in RCS format.
   The time will be GMT.

   We then convert that to the format required in the protocol
   (including the "-D" option) and send it.  According to
   cvsclient.texi, RFC 822/1123 format is preferred.  */

void
client_senddate (date)
    const char *date;
{
    char buf[MAXDATELEN];

    date_to_internet (buf, (char *)date);
    option_with_arg ("-D", buf);
}

void
send_init_command ()
{
    /* This is here because we need the current_parsed_root->directory variable.  */
    send_to_server ("init ", 0);
    send_to_server (current_parsed_root->directory, 0);
    send_to_server ("\012", 0);
}

#endif /* CLIENT_SUPPORT */
