#include "cvs.h"

#ifdef SERVER_SUPPORT

/* for select */
#include <sys/types.h>
#ifdef HAVE_SYS_BSDTYPES_H
#include <sys/bsdtypes.h>
#endif
#include <sys/time.h>

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK O_NDELAY
#endif


/* Functions which the server calls.  */
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


/*
 * This is where we stash stuff we are going to use.  Format string
 * which expects a single directory within it, starting with a slash.
 */
static char *server_temp_dir;

/* Nonzero if we should keep the temp directory around after we exit.  */
static int dont_delete_temp;

static char no_mem_error;
#define NO_MEM_ERROR (&no_mem_error)

static void server_write_entries PROTO((void));

/*
 * Read a line from the stream "instream" without command line editing.
 *
 * Action is compatible with "readline", e.g. space for the result is
 * malloc'd and should be freed by the caller.
 *
 * A NULL return means end of file.  A return of NO_MEM_ERROR means
 * that we are out of memory.
 */
static char *read_line PROTO((FILE *));

static char *
read_line (stream)
    FILE *stream;
{
    int c;
    char *result;
    int input_index = 0;
    int result_size = 80;

    fflush (stdout);
    result = (char *) malloc (result_size);
    if (result == NULL)
	return NO_MEM_ERROR;
    
    while (1)
    {
	c = fgetc (stream);
	
	if (c == EOF)
	{
	    free (result);
	    return NULL;
	}
	
	if (c == '\n')
	    break;
	
	result[input_index++] = c;
	while (input_index >= result_size)
	{
	    result_size *= 2;
	    result = (char *) realloc (result, result_size);
	    if (result == NULL)
		return NO_MEM_ERROR;
	}
    }
    
    result[input_index++] = '\0';
    return result;
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
	    if (CVS_MKDIR (q, 0777) < 0)
	    {
		if (errno != EEXIST
		    && (errno != EACCES || !isdir(q)))
		{
		    retval = errno;
		    goto done;
		}
	    }
	    ++p;
	}
	else
	{
	    if (CVS_MKDIR (dir, 0777) < 0)
		retval = errno;
	    else
		retval = 0;
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
 */
static void
print_error (status)
    int status;
{
    char *msg;
    printf ("error  ");
    msg = strerror (status);
    if (msg)
	printf ("%s", msg);
    printf ("\n");
}

static int pending_error;
/*
 * Malloc'd text for pending error.  Each line must start with "E ".  The
 * last line should not end with a newline.
 */
static char *pending_error_text;

/* If an error is pending, print it and return 1.  If not, return 0.  */
static int
print_pending_error ()
{
    if (pending_error_text)
    {
	printf ("%s\n", pending_error_text);
	if (pending_error)
	    print_error (pending_error);
	else
	    printf ("error  \n");
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

int
supported_response (name)
     char *name;
{
    struct response *rs;

    for (rs = responses; rs->name != NULL; ++rs)
	if (strcmp (rs->name, name) == 0)
	    return rs->status == rs_supported;
    error (1, 0, "internal error: testing support for unknown response?");
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
	    printf ("E response `%s' not supported by client\nerror  \n",
		    rs->name);
	    exit (1);
	}
	else if (rs->status == rs_optional)
	    rs->status = rs_not_supported;
    }
}

static int use_dir_and_repos = 0;

static void
serve_root (arg)
    char *arg;
{
    char *env;
    extern char *CVSroot;
    char path[PATH_MAX];
    int save_errno;
    
    if (error_pending()) return;
    
    (void) sprintf (path, "%s/%s", arg, CVSROOTADM);
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
    if (isfile (path) && !isaccessible (path, R_OK | W_OK))
    {
	save_errno = errno;
	pending_error_text = malloc (80 + strlen (path));
	if (pending_error_text != NULL)
	    sprintf (pending_error_text, "E \
Sorry, you don't have read/write access to the history file %s", path);
	pending_error = save_errno;
    }

    CVSroot = malloc (strlen (arg) + 1);
    if (CVSroot == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    strcpy (CVSroot, arg);
#ifdef HAVE_PUTENV
    env = malloc (strlen (CVSROOT_ENV) + strlen (CVSroot) + 1 + 1);
    if (env == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    (void) sprintf (env, "%s=%s", CVSROOT_ENV, arg);
    (void) putenv (env);
    /* do not free env, as putenv has control of it */
#endif
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
    free (server_temp_dir);
    server_temp_dir = p;
}

static void
dirswitch (dir, repos)
    char *dir;
    char *repos;
{
    char *dirname;
    int status;
    FILE *f;

    server_write_entries ();

    if (error_pending()) return;

    dirname = malloc (strlen (server_temp_dir) + strlen (dir) + 40);
    if (dirname == NULL)
    {
	pending_error = ENOMEM;
	return;
    }
    
    strcpy (dirname, server_temp_dir);
    strcat (dirname, "/");
    strcat (dirname, dir);

    status = mkdir_p (dirname);	
    if (status != 0
	&& status != EEXIST)
    {
	pending_error = status;
	pending_error_text = malloc (80 + strlen(dirname));
	sprintf(pending_error_text, "E cannot mkdir %s", dirname);
	return;
    }
    if (chdir (dirname) < 0)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(dirname));
	sprintf(pending_error_text, "E cannot change to %s", dirname);
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
    f = fopen (CVSADM_REP, "w");
    if (f == NULL)
    {
	pending_error = errno;
	return;
    }
    if (fprintf (f, "%s\n", repos) < 0)
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
    f = fopen (CVSADM_ENT, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_ENT));
	sprintf(pending_error_text, "E cannot open %s", CVSADM_ENT);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_ENT));
	sprintf(pending_error_text, "E cannot close %s", CVSADM_ENT);
	return;
    }
    free (dirname);
}    

static void
serve_repository (arg)
    char *arg;
{
    dirswitch (arg + 1, arg);
}

static void
serve_directory (arg)
    char *arg;
{
    char *repos;
    use_dir_and_repos = 1;
    repos = read_line (stdin);
    if (repos == NULL)
    {
	pending_error_text = malloc (80 + strlen (arg));
	if (pending_error_text)
	{
	    if (feof (stdin))
		sprintf (pending_error_text,
			 "E end of file reading mode for %s", arg);
	    else
	    {
		sprintf (pending_error_text,
			 "E error reading mode for %s", arg);
		pending_error = errno;
	    }
	}
	else
	    pending_error = ENOMEM;
    }
    else if (repos == NO_MEM_ERROR)
    {
	pending_error = ENOMEM;
    }
    else
    {
	dirswitch (arg, repos);
	free (repos);
    }
}

static void
serve_static_directory (arg)
    char *arg;
{
    FILE *f;
    f = fopen (CVSADM_ENTSTAT, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_ENTSTAT));
	sprintf(pending_error_text, "E cannot open %s", CVSADM_ENTSTAT);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_ENTSTAT));
	sprintf(pending_error_text, "E cannot close %s", CVSADM_ENTSTAT);
	return;
    }
}

static void
serve_sticky (arg)
    char *arg;
{
    FILE *f;
    f = fopen (CVSADM_TAG, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_TAG));
	sprintf(pending_error_text, "E cannot open %s", CVSADM_TAG);
	return;
    }
    if (fprintf (f, "%s\n", arg) < 0)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_TAG));
	sprintf(pending_error_text, "E cannot write to %s", CVSADM_TAG);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_TAG));
	sprintf(pending_error_text, "E cannot close %s", CVSADM_TAG);
	return;
    }
}

/*
 * Read SIZE bytes from stdin, write them to FILE.
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
    char buf[16*1024], *bufp;
    int toread, nread, nwrote;
    while (size > 0)
    {
	toread = sizeof (buf);
	if (toread > size)
	    toread = size;

	nread = fread (buf, 1, toread, stdin);
	if (nread <= 0)
	{
	    if (feof (stdin))
	    {
		pending_error_text = malloc (80);
		if (pending_error_text)
		{
		    sprintf (pending_error_text,
			     "E premature end of file from client");
		    pending_error = 0;
		}
		else
		    pending_error = ENOMEM;
	    }
	    else if (ferror (stdin))
	    {
		pending_error_text = malloc (40);
		if (pending_error_text)
		    sprintf (pending_error_text,
			     "E error reading from client");
		pending_error = errno;
	    }
	    else
	    {
		pending_error_text = malloc (40);
		if (pending_error_text)
		    sprintf (pending_error_text,
			     "E short read from client");
		pending_error = 0;
	    }
	    return;
	}
	size -= nread;
	bufp = buf;
	while (nread)
	{
	    nwrote = write (file, bufp, nread);
	    if (nwrote < 0)
	    {
		pending_error_text = malloc (40);
		if (pending_error_text)
		    sprintf (pending_error_text, "E unable to write");
		pending_error = errno;
		return;
	    }
	    nread -= nwrote;
	    bufp += nwrote;
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
    fd = open (arg, O_WRONLY | O_CREAT | O_TRUNC, 0600);
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
	    error (1, errno, "waiting for gunzip process %d", gzip_pid);
	else if (gzip_status != 0)
	    error (1, 0, "gunzip exited %d", gzip_status);
    }
}

static void
serve_modified (arg)
     char *arg;
{
    int size;
    char *size_text;
    char *mode_text;

    int gzipped = 0;

    if (error_pending ()) return;

    mode_text = read_line (stdin);
    if (mode_text == NULL)
    {
	pending_error_text = malloc (80 + strlen (arg));
	if (pending_error_text)
	{
	    if (feof (stdin))
		sprintf (pending_error_text,
			 "E end of file reading mode for %s", arg);
	    else
	    {
		sprintf (pending_error_text,
			 "E error reading mode for %s", arg);
		pending_error = errno;
	    }
	}
	else
	    pending_error = ENOMEM;
	return;
    } 
    else if (mode_text == NO_MEM_ERROR)
    {
	pending_error = ENOMEM;
	return;
    }
    size_text = read_line (stdin);
    if (size_text == NULL)
    {
	pending_error_text = malloc (80 + strlen (arg));
	if (pending_error_text)
	{
	    if (feof (stdin))
		sprintf (pending_error_text,
			 "E end of file reading size for %s", arg);
	    else
	    {
		sprintf (pending_error_text,
			 "E error reading size for %s", arg);
		pending_error = errno;
	    }
	}
	else
	    pending_error = ENOMEM;
	return;
    } 
    else if (size_text == NO_MEM_ERROR)
    {
	pending_error = ENOMEM;
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
}

#endif /* SERVER_SUPPORT */

#if defined(SERVER_SUPPORT) || defined(CLIENT_SUPPORT)

int use_unchanged = 0;

#endif
#ifdef SERVER_SUPPORT

static void
serve_enable_unchanged (arg)
     char *arg;
{
  use_unchanged = 1;
}

static void
serve_lost (arg)
    char *arg;
{
    if (use_unchanged)
    {
	/* A missing file already indicates it is nonexistent.  */
	return;
    }
    else
    {
	struct utimbuf ut;
	int fd = open (arg, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0 || close (fd) < 0)
	{
	    pending_error = errno;
	    pending_error_text = malloc (80 + strlen(arg));
	    sprintf(pending_error_text, "E cannot open %s", arg);
	    return;
	}
	/*
	 * Set the times to the beginning of the epoch to tell time_stamp()
	 * that the file was lost.
	 */
	ut.actime = 0;
	ut.modtime = 0;
	if (utime (arg, &ut) < 0)
	{
	    pending_error = errno;
	    pending_error_text = malloc (80 + strlen(arg));
	    sprintf(pending_error_text, "E cannot utime %s", arg);
	    return;
	}
    }
}

struct an_entry {
    struct an_entry *next;
    char *entry;
};

static struct an_entry *entries;

static void
serve_unchanged (arg)
    char *arg;
{
    if (error_pending ())
	return;
    if (!use_unchanged) 
    {
	/* A missing file already indicates it is unchanged.  */
	return;
    }
    else
    {
	struct an_entry *p;
	char *name;
	char *cp;
	char *timefield;

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
}

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
	f = fopen (CVSADM_ENT, "w");
	if (f == NULL)
	{
	    pending_error = errno;
	    pending_error_text = malloc (80 + strlen(CVSADM_ENT));
	    sprintf(pending_error_text, "E cannot open %s", CVSADM_ENT);
	}
    }
    for (p = entries; p != NULL;)
    {
	if (!error_pending ())
	{
	    if (fprintf (f, "%s\n", p->entry) < 0)
	    {
		pending_error = errno;
		pending_error_text = malloc (80 + strlen(CVSADM_ENT));
		sprintf(pending_error_text, "E cannot write to %s", CVSADM_ENT);
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
	pending_error_text = malloc (80 + strlen(CVSADM_ENT));
	sprintf(pending_error_text, "E cannot close %s", CVSADM_ENT);
    }
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
	pending_error_text = malloc (strlen (arg) + 80);
	sprintf (pending_error_text, "E Protocol error: bad global option %s",
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

/*
 * We must read data from a child process and send it across the
 * network.  We do not want to block on writing to the network, so we
 * store the data from the child process in memory.  A BUFFER
 * structure holds the status of one communication, and uses a linked
 * list of buffer_data structures to hold data.
 */

struct buffer
{
    /* Data.  */
    struct buffer_data *data;

    /* Last buffer on data chain.  */
    struct buffer_data *last;

    /* File descriptor to write to or read from.  */
    int fd;

    /* Nonzero if this is an output buffer (sanity check).  */
    int output;

    /* Nonzero if the file descriptor is in nonblocking mode.  */
    int nonblocking;

    /* Function to call if we can't allocate memory.  */
    void (*memory_error) PROTO((struct buffer *));
};

/* Data is stored in lists of these structures.  */

struct buffer_data
{
    /* Next buffer in linked list.  */
    struct buffer_data *next;

    /*
     * A pointer into the data area pointed to by the text field.  This
     * is where to find data that has not yet been written out.
     */
    char *bufp;

    /* The number of data bytes found at BUFP.  */
    int size;

    /*
     * Actual buffer.  This never changes after the structure is
     * allocated.  The buffer is BUFFER_DATA_SIZE bytes.
     */
    char *text;
};

/* The size we allocate for each buffer_data structure.  */
#define BUFFER_DATA_SIZE (4096)

#ifdef SERVER_FLOWCONTROL
/* The maximum we'll queue to the remote client before blocking.  */
# ifndef SERVER_HI_WATER
#  define SERVER_HI_WATER (2 * 1024 * 1024)
# endif /* SERVER_HI_WATER */
/* When the buffer drops to this, we restart the child */
# ifndef SERVER_LO_WATER
#  define SERVER_LO_WATER (1 * 1024 * 1024)
# endif /* SERVER_LO_WATER */
#endif /* SERVER_FLOWCONTROL */

/* Linked list of available buffer_data structures.  */
static struct buffer_data *free_buffer_data;

static void allocate_buffer_datas PROTO((void));
static inline struct buffer_data *get_buffer_data PROTO((void));
static int buf_empty_p PROTO((struct buffer *));
static void buf_output PROTO((struct buffer *, const char *, int));
static void buf_output0 PROTO((struct buffer *, const char *));
static inline void buf_append_char PROTO((struct buffer *, int));
static int buf_send_output PROTO((struct buffer *));
static int set_nonblock PROTO((struct buffer *));
static int set_block PROTO((struct buffer *));
static int buf_send_counted PROTO((struct buffer *));
static inline void buf_append_data PROTO((struct buffer *,
				     struct buffer_data *,
				     struct buffer_data *));
static int buf_read_file PROTO((FILE *, long, struct buffer_data **,
				  struct buffer_data **));
static int buf_input_data PROTO((struct buffer *, int *));
static void buf_copy_lines PROTO((struct buffer *, struct buffer *, int));
static int buf_copy_counted PROTO((struct buffer *, struct buffer *));

#ifdef SERVER_FLOWCONTROL
static int buf_count_mem PROTO((struct buffer *));
static int set_nonblock_fd PROTO((int));
#endif /* SERVER_FLOWCONTROL */

/* Allocate more buffer_data structures.  */

static void
allocate_buffer_datas ()
{
    struct buffer_data *alc;
    char *space;
    int i;

    /* Allocate buffer_data structures in blocks of 16.  */
#define ALLOC_COUNT (16)

    alc = ((struct buffer_data *)
	   malloc (ALLOC_COUNT * sizeof (struct buffer_data)));
    space = (char *) valloc (ALLOC_COUNT * BUFFER_DATA_SIZE);
    if (alc == NULL || space == NULL)
	return;
    for (i = 0; i < ALLOC_COUNT; i++, alc++, space += BUFFER_DATA_SIZE)
    {
	alc->next = free_buffer_data;
	free_buffer_data = alc;
	alc->text = space;
    }	  
}

/* Get a new buffer_data structure.  */

static inline struct buffer_data *
get_buffer_data ()
{
    struct buffer_data *ret;

    if (free_buffer_data == NULL)
    {
	allocate_buffer_datas ();
	if (free_buffer_data == NULL)
	    return NULL;
    }

    ret = free_buffer_data;
    free_buffer_data = ret->next;
    return ret;
}

/* See whether a buffer is empty.  */

static int
buf_empty_p (buf)
    struct buffer *buf;
{
    struct buffer_data *data;

    for (data = buf->data; data != NULL; data = data->next)
	if (data->size > 0)
	    return 0;
    return 1;
}

#ifdef SERVER_FLOWCONTROL
/*
 * Count how much data is stored in the buffer..
 * Note that each buffer is a malloc'ed chunk BUFFER_DATA_SIZE.
 */

static int
buf_count_mem (buf)
    struct buffer *buf;
{
    struct buffer_data *data;
    int mem = 0;

    for (data = buf->data; data != NULL; data = data->next)
	mem += BUFFER_DATA_SIZE;

    return mem;
}
#endif /* SERVER_FLOWCONTROL */

/* Add data DATA of length LEN to BUF.  */

static void
buf_output (buf, data, len)
    struct buffer *buf;
    const char *data;
    int len;
{
    if (! buf->output)
	abort ();

    if (buf->data != NULL
	&& (((buf->last->text + BUFFER_DATA_SIZE)
	     - (buf->last->bufp + buf->last->size))
	    >= len))
    {
	memcpy (buf->last->bufp + buf->last->size, data, len);
	buf->last->size += len;
	return;
    }

    while (1)
    {
	struct buffer_data *newdata;

	newdata = get_buffer_data ();
	if (newdata == NULL)
	{
	    (*buf->memory_error) (buf);
	    return;
	}

	if (buf->data == NULL)
	    buf->data = newdata;
	else
	    buf->last->next = newdata;
	newdata->next = NULL;
	buf->last = newdata;

	newdata->bufp = newdata->text;

	if (len <= BUFFER_DATA_SIZE)
	{
	    newdata->size = len;
	    memcpy (newdata->text, data, len);
	    return;
	}

	newdata->size = BUFFER_DATA_SIZE;
	memcpy (newdata->text, data, BUFFER_DATA_SIZE);

	data += BUFFER_DATA_SIZE;
	len -= BUFFER_DATA_SIZE;
    }

    /*NOTREACHED*/
}

/* Add a '\0' terminated string to BUF.  */

static void
buf_output0 (buf, string)
    struct buffer *buf;
    const char *string;
{
    buf_output (buf, string, strlen (string));
}

/* Add a single character to BUF.  */

static inline void
buf_append_char (buf, ch)
    struct buffer *buf;
    int ch;
{
    if (buf->data != NULL
	&& (buf->last->text + BUFFER_DATA_SIZE
	    != buf->last->bufp + buf->last->size))
    {
	*(buf->last->bufp + buf->last->size) = ch;
	++buf->last->size;
    }
    else
    {
	char b;

	b = ch;
	buf_output (buf, &b, 1);
    }
}

/*
 * Send all the output we've been saving up.  Returns 0 for success or
 * errno code.  If the buffer has been set to be nonblocking, this
 * will just write until the write would block.
 */

static int
buf_send_output (buf)
     struct buffer *buf;
{
    if (! buf->output)
	abort ();

    while (buf->data != NULL)
    {
	struct buffer_data *data;

	data = buf->data;
	while (data->size > 0)
	{
	    int nbytes;

	    nbytes = write (buf->fd, data->bufp, data->size);
	    if (nbytes <= 0)
	    {
		int status;

		if (buf->nonblocking
		    && (nbytes == 0
#ifdef EWOULDBLOCK
			|| errno == EWOULDBLOCK
#endif
			|| errno == EAGAIN))
		{
		    /*
		     * A nonblocking write failed to write any data.
		     * Just return.
		     */
		    return 0;
		}

		/*
		 * An error, or EOF.  Throw away all the data and
		 * return.
		 */
		if (nbytes == 0)
		    status = EIO;
		else
		    status = errno;

		buf->last->next = free_buffer_data;
		free_buffer_data = buf->data;
		buf->data = NULL;
		buf->last = NULL;

		return status;
	    }

	    data->size -= nbytes;
	    data->bufp += nbytes;
	}

	buf->data = data->next;
	data->next = free_buffer_data;
	free_buffer_data = data;
    }

    buf->last = NULL;

    return 0;
}

#ifdef SERVER_FLOWCONTROL
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

static int
set_nonblock (buf)
     struct buffer *buf;
{
    int flags;

    if (buf->nonblocking)
	return 0;
    flags = fcntl (buf->fd, F_GETFL, 0);
    if (flags < 0)
	return errno;
    if (fcntl (buf->fd, F_SETFL, flags | O_NONBLOCK) < 0)
	return errno;
    buf->nonblocking = 1;
    return 0;
}

/*
 * Set buffer BUF to blocking I/O.  Returns 0 for success or errno
 * code.
 */

static int
set_block (buf)
     struct buffer *buf;
{
    int flags;

    if (! buf->nonblocking)
	return 0;
    flags = fcntl (buf->fd, F_GETFL, 0);
    if (flags < 0)
	return errno;
    if (fcntl (buf->fd, F_SETFL, flags & ~O_NONBLOCK) < 0)
	return errno;
    buf->nonblocking = 0;
    return 0;
}

/*
 * Send a character count and some output.  Returns errno code or 0 for
 * success.
 *
 * Sending the count in binary is OK since this is only used on a pipe
 * within the same system.
 */

static int
buf_send_counted (buf)
     struct buffer *buf;
{
    int size;
    struct buffer_data *data;

    if (! buf->output)
	abort ();

    size = 0;
    for (data = buf->data; data != NULL; data = data->next)
	size += data->size;

    data = get_buffer_data ();
    if (data == NULL)
    {
	(*buf->memory_error) (buf);
	return ENOMEM;
    }

    data->next = buf->data;
    buf->data = data;
    if (buf->last == NULL)
	buf->last = data;

    data->bufp = data->text;
    data->size = sizeof (int);

    *((int *) data->text) = size;

    return buf_send_output (buf);
}

/* Append a list of buffer_data structures to an buffer.  */

static inline void
buf_append_data (buf, data, last)
     struct buffer *buf;
     struct buffer_data *data;
     struct buffer_data *last;
{
    if (data != NULL)
    {
	if (buf->data == NULL)
	    buf->data = data;
	else
	    buf->last->next = data;
	buf->last = last;
    }
}

/*
 * Copy the contents of file F into buffer_data structures.  We can't
 * copy directly into an buffer, because we want to handle failure and
 * succeess differently.  Returns 0 on success, or -2 if out of
 * memory, or a status code on error.  Since the caller happens to
 * know the size of the file, it is passed in as SIZE.  On success,
 * this function sets *RETP and *LASTP, which may be passed to
 * buf_append_data.
 */

static int
buf_read_file (f, size, retp, lastp)
    FILE *f;
    long size;
    struct buffer_data **retp;
    struct buffer_data **lastp;
{
    int status;

    *retp = NULL;
    *lastp = NULL;

    while (size > 0)
    {
	struct buffer_data *data;
	int get;

	data = get_buffer_data ();
	if (data == NULL)
	{
	    status = -2;
	    goto error_return;
	}

	if (*retp == NULL)
	    *retp = data;
	else
	    (*lastp)->next = data;
	data->next = NULL;
	*lastp = data;

	data->bufp = data->text;
	data->size = 0;

	if (size > BUFFER_DATA_SIZE)
	    get = BUFFER_DATA_SIZE;
	else
	    get = size;

	errno = EIO;
	if (fread (data->text, get, 1, f) != 1)
	{
	    status = errno;
	    goto error_return;
	}

	data->size += get;
	size -= get;
    }

    return 0;

  error_return:
    if (*retp != NULL)
    {
	(*lastp)->next = free_buffer_data;
	free_buffer_data = *retp;
    }
    return status;
}

static int
buf_read_file_to_eof (f, retp, lastp)
     FILE *f;
     struct buffer_data **retp;
     struct buffer_data **lastp;
{
    int status;

    *retp = NULL;
    *lastp = NULL;

    while (!feof (f))
    {
	struct buffer_data *data;
	int get, nread;

	data = get_buffer_data ();
	if (data == NULL)
	{
	    status = -2;
	    goto error_return;
	}

	if (*retp == NULL)
	    *retp = data;
	else
	    (*lastp)->next = data;
	data->next = NULL;
	*lastp = data;

	data->bufp = data->text;
	data->size = 0;

	get = BUFFER_DATA_SIZE;

	errno = EIO;
	nread = fread (data->text, 1, get, f);
	if (nread == 0 && !feof (f))
	{
	    status = errno;
	    goto error_return;
	}

	data->size = nread;
    }

    return 0;

  error_return:
    if (*retp != NULL)
    {
	(*lastp)->next = free_buffer_data;
	free_buffer_data = *retp;
    }
    return status;
}

static int
buf_chain_length (buf)
     struct buffer_data *buf;
{
    int size = 0;
    while (buf)
    {
	size += buf->size;
	buf = buf->next;
    }
    return size;
}

/*
 * Read an arbitrary amount of data from a file descriptor into an
 * input buffer.  The file descriptor will be in nonblocking mode, and
 * we just grab what we can.  Return 0 on success, or -1 on end of
 * file, or -2 if out of memory, or an error code.  If COUNTP is not
 * NULL, *COUNTP is set to the number of bytes read.
 */

static int
buf_input_data (buf, countp)
     struct buffer *buf;
     int *countp;
{
    if (buf->output)
	abort ();

    if (countp != NULL)
	*countp = 0;

    while (1)
    {
	int get;
	int nbytes;

	if (buf->data == NULL
	    || (buf->last->bufp + buf->last->size
		== buf->last->text + BUFFER_DATA_SIZE))
	{
	    struct buffer_data *data;

	    data = get_buffer_data ();
	    if (data == NULL)
	    {
		(*buf->memory_error) (buf);
		return -2;
	    }

	    if (buf->data == NULL)
		buf->data = data;
	    else
		buf->last->next = data;
	    data->next = NULL;
	    buf->last = data;

	    data->bufp = data->text;
	    data->size = 0;
	}

	get = ((buf->last->text + BUFFER_DATA_SIZE)
	       - (buf->last->bufp + buf->last->size));
	nbytes = read (buf->fd, buf->last->bufp + buf->last->size, get);
	if (nbytes <= 0)
	{
	    if (nbytes == 0)
	    {
		/*
		 * This assumes that we are using POSIX or BSD style
		 * nonblocking I/O.  On System V we will get a zero
		 * return if there is no data, even when not at EOF.
		 */
		return -1;
	    }

	    if (errno == EAGAIN
#ifdef EWOULDBLOCK
		|| errno == EWOULDBLOCK
#endif
		)
	      return 0;

	    return errno;
	}

	buf->last->size += nbytes;
	if (countp != NULL)
	    *countp += nbytes;
    }

    /*NOTREACHED*/
}

/*
 * Copy lines from an input buffer to an output buffer.  This copies
 * all complete lines (characters up to a newline) from INBUF to
 * OUTBUF.  Each line in OUTBUF is preceded by the character COMMAND
 * and a space.
 */

static void
buf_copy_lines (outbuf, inbuf, command)
     struct buffer *outbuf;
     struct buffer *inbuf;
     int command;
{
    if (! outbuf->output || inbuf->output)
	abort ();

    while (1)
    {
	struct buffer_data *data;
	struct buffer_data *nldata;
	char *nl;
	int len;

	/* See if there is a newline in INBUF.  */
	nldata = NULL;
	nl = NULL;
	for (data = inbuf->data; data != NULL; data = data->next)
	{
	    nl = memchr (data->bufp, '\n', data->size);
	    if (nl != NULL)
	    {
		nldata = data;
		break;
	    }
	}

	if (nldata == NULL)
	{
	    /* There are no more lines in INBUF.  */
	    return;
	}

	/* Put in the command.  */
	buf_append_char (outbuf, command);
	buf_append_char (outbuf, ' ');

	if (inbuf->data != nldata)
	{
	    /*
	     * Simply move over all the buffers up to the one containing
	     * the newline.
	     */
	    for (data = inbuf->data; data->next != nldata; data = data->next)
		;
	    data->next = NULL;
	    buf_append_data (outbuf, inbuf->data, data);
	    inbuf->data = nldata;
	}

	/*
	 * If the newline is at the very end of the buffer, just move
	 * the buffer onto OUTBUF.  Otherwise we must copy the data.
	 */
	len = nl + 1 - nldata->bufp;
	if (len == nldata->size)
	{
	    inbuf->data = nldata->next;
	    if (inbuf->data == NULL)
		inbuf->last = NULL;

	    nldata->next = NULL;
	    buf_append_data (outbuf, nldata, nldata);
	}
	else
	{
	    buf_output (outbuf, nldata->bufp, len);
	    nldata->bufp += len;
	    nldata->size -= len;
	}
    }
}

/*
 * Copy counted data from one buffer to another.  The count is an
 * integer, host size, host byte order (it is only used across a
 * pipe).  If there is enough data, it should be moved over.  If there
 * is not enough data, it should remain on the original buffer.  This
 * returns the number of bytes it needs to see in order to actually
 * copy something over.
 */

static int
buf_copy_counted (outbuf, inbuf)
     struct buffer *outbuf;
     struct buffer *inbuf;
{
    if (! outbuf->output || inbuf->output)
	abort ();

    while (1)
    {
	struct buffer_data *data;
	int need;
	union
	{
	    char intbuf[sizeof (int)];
	    int i;
	} u;
	char *intp;
	int count;
	struct buffer_data *start;
	int startoff;
	struct buffer_data *stop;
	int stopwant;

	/* See if we have enough bytes to figure out the count.  */
	need = sizeof (int);
	intp = u.intbuf;
	for (data = inbuf->data; data != NULL; data = data->next)
	{
	    if (data->size >= need)
	    {
		memcpy (intp, data->bufp, need);
		break;
	    }
	    memcpy (intp, data->bufp, data->size);
	    intp += data->size;
	    need -= data->size;
	}
	if (data == NULL)
	{
	    /* We don't have enough bytes to form an integer.  */
	    return need;
	}

	count = u.i;
	start = data;
	startoff = need;

	/*
	 * We have an integer in COUNT.  We have gotten all the data
	 * from INBUF in all buffers before START, and we have gotten
	 * STARTOFF bytes from START.  See if we have enough bytes
	 * remaining in INBUF.
	 */
	need = count - (start->size - startoff);
	if (need <= 0)
	{
	    stop = start;
	    stopwant = count;
	}
	else
	{
	    for (data = start->next; data != NULL; data = data->next)
	    {
		if (need <= data->size)
		    break;
		need -= data->size;
	    }
	    if (data == NULL)
	    {
		/* We don't have enough bytes.  */
		return need;
	    }
	    stop = data;
	    stopwant = need;
	}

	/*
	 * We have enough bytes.  Free any buffers in INBUF before
	 * START, and remove STARTOFF bytes from START, so that we can
	 * forget about STARTOFF.
	 */
	start->bufp += startoff;
	start->size -= startoff;

	if (start->size == 0)
	    start = start->next;

	if (stop->size == stopwant)
	{
	    stop = stop->next;
	    stopwant = 0;
	}

	while (inbuf->data != start)
	{
	    data = inbuf->data;
	    inbuf->data = data->next;
	    data->next = free_buffer_data;
	    free_buffer_data = data;
	}

	/*
	 * We want to copy over the bytes from START through STOP.  We
	 * only want STOPWANT bytes from STOP.
	 */

	if (start != stop)
	{
	    /* Attach the buffers from START through STOP to OUTBUF.  */
	    for (data = start; data->next != stop; data = data->next)
		;
	    inbuf->data = stop;
	    data->next = NULL;
	    buf_append_data (outbuf, start, data);
	}

	if (stopwant > 0)
	{
	    buf_output (outbuf, stop->bufp, stopwant);
	    stop->bufp += stopwant;
	    stop->size -= stopwant;
	}
    }

    /*NOTREACHED*/
}

static struct buffer protocol;

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
    server_cleanup (0);
    exit (1);
}

static void
input_memory_error (buf)
     struct buffer *buf;
{
    outbuf_memory_error (buf);
}

/* Execute COMMAND in a subprocess with the approriate funky things done.  */

static struct fd_set_wrapper { fd_set fds; } command_fds_to_drain;
static int max_command_fd;

#ifdef SERVER_FLOWCONTROL
static int flowcontrol_pipe[2];
#endif /* SERVER_FLOWCONTROL */

static void
do_cvs_command (command)
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

    dev_null_fd = open ("/dev/null", O_RDONLY);
    if (dev_null_fd < 0)
    {
	print_error (errno);
	goto error_exit;
    }

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

	protocol.data = protocol.last = NULL;
	protocol.fd = protocol_pipe[1];
	protocol.output = 1;
	protocol.nonblocking = 0;
	protocol.memory_error = protocol_memory_error;

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
	struct buffer outbuf;
	struct buffer stdoutbuf;
	struct buffer stderrbuf;
	struct buffer protocol_inbuf;
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
	    printf ("E internal error: FD_SETSIZE not big enough.\nerror  \n");
	    goto error_exit;
	}

	outbuf.data = outbuf.last = NULL;
	outbuf.fd = STDOUT_FILENO;
	outbuf.output = 1;
	outbuf.nonblocking = 0;
	outbuf.memory_error = outbuf_memory_error;

	stdoutbuf.data = stdoutbuf.last = NULL;
	stdoutbuf.fd = stdout_pipe[0];
	stdoutbuf.output = 0;
	stdoutbuf.nonblocking = 0;
	stdoutbuf.memory_error = input_memory_error;

	stderrbuf.data = stderrbuf.last = NULL;
	stderrbuf.fd = stderr_pipe[0];
	stderrbuf.output = 0;
	stderrbuf.nonblocking = 0;
	stderrbuf.memory_error = input_memory_error;

	protocol_inbuf.data = protocol_inbuf.last = NULL;
	protocol_inbuf.fd = protocol_pipe[0];
	protocol_inbuf.output = 0;
	protocol_inbuf.nonblocking = 0;
	protocol_inbuf.memory_error = input_memory_error;

	set_nonblock (&outbuf);
	set_nonblock (&stdoutbuf);
	set_nonblock (&stderrbuf);
	set_nonblock (&protocol_inbuf);

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
	    bufmemsize = buf_count_mem (&outbuf);
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
	    if (! buf_empty_p (&outbuf))
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
		buf_send_output (&outbuf);
	    }

	    if (stdout_pipe[0] >= 0
		&& (FD_ISSET (stdout_pipe[0], &readfds)))
	    {
	        int status;

	        status = buf_input_data (&stdoutbuf, (int *) NULL);

		buf_copy_lines (&outbuf, &stdoutbuf, 'M');

		if (status == -1)
		    stdout_pipe[0] = -1;
		else if (status > 0)
		{
		    print_error (status);
		    goto error_exit;
		}

		/* What should we do with errors?  syslog() them?  */
		buf_send_output (&outbuf);
	    }

	    if (stderr_pipe[0] >= 0
		&& (FD_ISSET (stderr_pipe[0], &readfds)))
	    {
	        int status;

	        status = buf_input_data (&stderrbuf, (int *) NULL);

		buf_copy_lines (&outbuf, &stderrbuf, 'E');

		if (status == -1)
		    stderr_pipe[0] = -1;
		else if (status > 0)
		{
		    print_error (status);
		    goto error_exit;
		}

		/* What should we do with errors?  syslog() them?  */
		buf_send_output (&outbuf);
	    }

	    if (protocol_pipe[0] >= 0
		&& (FD_ISSET (protocol_pipe[0], &readfds)))
	    {
		int status;
		int count_read;
		
		status = buf_input_data (&protocol_inbuf, &count_read);

		/*
		 * We only call buf_copy_counted if we have read
		 * enough bytes to make it worthwhile.  This saves us
		 * from continually recounting the amount of data we
		 * have.
		 */
		count_needed -= count_read;
		if (count_needed <= 0)
		  count_needed = buf_copy_counted (&outbuf, &protocol_inbuf);

		if (status == -1)
		    protocol_pipe[0] = -1;
		else if (status > 0)
		{
		    print_error (status);
		    goto error_exit;
		}

		/* What should we do with errors?  syslog() them?  */
		buf_send_output (&outbuf);
	    }
	}

	/*
	 * OK, we've gotten EOF on all the pipes.  If there is
	 * anything left on stdoutbuf or stderrbuf (this could only
	 * happen if there was no trailing newline), send it over.
	 */
	if (! buf_empty_p (&stdoutbuf))
	{
	    buf_append_char (&stdoutbuf, '\n');
	    buf_copy_lines (&outbuf, &stdoutbuf, 'M');
	}
	if (! buf_empty_p (&stderrbuf))
	{
	    buf_append_char (&stderrbuf, '\n');
	    buf_copy_lines (&outbuf, &stderrbuf, 'E');
	}
	if (! buf_empty_p (&protocol_inbuf))
	    buf_output0 (&outbuf,
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
		/*
		 * This is really evil, because signals might be numbered
		 * differently on the two systems.  We should be using
		 * signal names (either of the "Terminated" or the "SIGTERM"
		 * variety).  But cvs doesn't currently use libiberty...we
		 * could roll our own....  FIXME.
		 */
		printf ("E Terminated with fatal signal %d\n", sig);

		/* Test for a core dump.  Is this portable?  */
		if (status & 0x80)
		{
		    printf ("E Core dumped; preserving %s on server.\n\
E CVS locks may need cleaning up.\n",
			    server_temp_dir);
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
	set_block (&outbuf);
	buf_send_output (&outbuf);
    }

    if (errs)
	/* We will have printed an error message already.  */
	printf ("error  \n");
    else
	printf ("ok\n");
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
	    while (read (flowcontrol_pipe[0], buf, 1) == 1)
	    {
		if (*buf == 'S')	/* Stop */
		    paused = 1;
		else if (*buf == 'G')	/* Go */
		    paused = 0;
		else
		    return;		/* ??? */
	    }
	}
    }
}
#endif /* SERVER_FLOWCONTROL */

static void output_dir PROTO((char *, char *));

static void
output_dir (update_dir, repository)
    char *update_dir;
    char *repository;
{
    if (use_dir_and_repos)
    {
	if (update_dir[0] == '\0')
	    buf_output0 (&protocol, ".");
	else
	    buf_output0 (&protocol, update_dir);
	buf_output0 (&protocol, "/\n");
    }
    buf_output0 (&protocol, repository);
    buf_output0 (&protocol, "/");
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

    if (trace)
    {
	(void) fprintf (stderr,
			"%c-> server_register(%s, %s, %s, %s, %s, %s, %s)\n",
			(server_active) ? 'S' : ' ', /* silly */
			name, version, timestamp, options, tag ? tag : "",
			date ? date : "", conflict ? conflict : "");
    }

    if (options == NULL)
	options = "";

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
	buf_output0 (&protocol,
		     "E CVS server internal error: duplicate Scratch_Entry\n");
	buf_send_counted (&protocol);
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
	buf_output0 (&protocol, entries_line);
	buf_output (&protocol, "\n", 1);
    }
    else
	/* Return the error message as the Entries line.  */
	buf_output0 (&protocol,
		     "CVS server internal error: Register missing\n");
    free (entries_line);
    entries_line = NULL;
}

static void
serve_ci (arg)
    char *arg;
{
    do_cvs_command (commit);
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
	buf_output0 (&protocol, "Remove-entry ");
	output_dir (update_dir, repository);
	buf_output0 (&protocol, file);
	buf_output (&protocol, "\n", 1);
	free (scratched_file);
	scratched_file = NULL;
    }
    else
    {
	buf_output0 (&protocol, "Checked-in ");
	output_dir (update_dir, repository);
	buf_output0 (&protocol, file);
	buf_output (&protocol, "\n", 1);
	new_entries_line ();
    }
    buf_send_counted (&protocol);
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
	buf_output0 (&protocol, "Checked-in ");
    else
    {
	if (!supported_response ("New-entry"))
	    return;
	buf_output0 (&protocol, "New-entry ");
    }

    output_dir (update_dir, repository);
    buf_output0 (&protocol, file);
    buf_output (&protocol, "\n", 1);
    new_entries_line ();
    buf_send_counted (&protocol);
}

static void
serve_update (arg)
    char *arg;
{
    do_cvs_command (update);
}

static void
serve_diff (arg)
    char *arg;
{
    do_cvs_command (diff);
}

static void
serve_log (arg)
    char *arg;
{
    do_cvs_command (cvslog);
}

static void
serve_add (arg)
    char *arg;
{
    do_cvs_command (add);
}

static void
serve_remove (arg)
    char *arg;
{
    do_cvs_command (cvsremove);
}

static void
serve_status (arg)
    char *arg;
{
    do_cvs_command (status);
}

static void
serve_rdiff (arg)
    char *arg;
{
    do_cvs_command (patch);
}

static void
serve_tag (arg)
    char *arg;
{
    do_cvs_command (tag);
}

static void
serve_rtag (arg)
    char *arg;
{
    do_cvs_command (rtag);
}

static void
serve_import (arg)
    char *arg;
{
    do_cvs_command (import);
}

static void
serve_admin (arg)
    char *arg;
{
    do_cvs_command (admin);
}

static void
serve_history (arg)
    char *arg;
{
    do_cvs_command (history);
}

static void
serve_release (arg)
    char *arg;
{
    do_cvs_command (release);
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
	    printf ("E Out of memory\n");
	    return;
	}
	strcpy (tempdir, server_temp_dir);
	strcat (tempdir, "/checkout-dir");
	status = mkdir_p (tempdir);
	if (status != 0 && status != EEXIST)
	{
	    printf ("E Cannot create %s\n", tempdir);
	    print_error (errno);
	    free (tempdir);
	    return;
	}

	if (chdir (tempdir) < 0)
	{
	    printf ("E Cannot change to directory %s\n", tempdir);
	    print_error (errno);
	    free (tempdir);
	    return;
	}
	free (tempdir);
    }
    do_cvs_command (checkout);
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
    buf_output0 (&protocol, "Copy-file ");
    output_dir (update_dir, repository);
    buf_output0 (&protocol, file);
    buf_output0 (&protocol, "\n");
    buf_output0 (&protocol, newfile);
    buf_output0 (&protocol, "\n");
}

void
server_updated (file, update_dir, repository, updated, file_info, checksum)
    char *file;
    char *update_dir;
    char *repository;
    enum server_updated_arg4 updated;
    struct stat *file_info;
    unsigned char *checksum;
{
    char *short_pathname;

    if (noexec)
	return;

    short_pathname = xmalloc (strlen (update_dir) + strlen (file) + 10);
    if (update_dir[0] == '\0')
	strcpy (short_pathname, file);
    else
	sprintf (short_pathname, "%s/%s", update_dir, file);

    if (entries_line != NULL && scratched_file == NULL)
    {
	FILE *f;
	struct stat sb;
	struct buffer_data *list, *last;
	unsigned long size;
	char size_text[80];

	if (stat (file, &sb) < 0)
	{
	    if (existence_error (errno))
	    {
		/*
		 * If we have a sticky tag for a branch on which the
		 * file is dead, and cvs update the directory, it gets
		 * a T_CHECKOUT but no file.  So in this case just
		 * forget the whole thing.
		 */
		free (entries_line);
		entries_line = NULL;
		goto done;
	    }
	    error (1, errno, "reading %s", short_pathname);
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

	        buf_output0 (&protocol, "Checksum ");
		for (i = 0; i < 16; i++)
		{
		    sprintf (buf, "%02x", (unsigned int) checksum[i]);
		    buf_output0 (&protocol, buf);
		}
		buf_append_char (&protocol, '\n');
	    }
	}

	if (updated == SERVER_UPDATED)
	    buf_output0 (&protocol, "Updated ");
	else if (updated == SERVER_MERGED)
	    buf_output0 (&protocol, "Merged ");
	else if (updated == SERVER_PATCHED)
	    buf_output0 (&protocol, "Patched ");
	else
	    abort ();
	output_dir (update_dir, repository);
	buf_output0 (&protocol, file);
	buf_output (&protocol, "\n", 1);

	new_entries_line ();

        {
	    char *mode_string;

	    /* FIXME: When we check out files the umask of the server
	       (set in .bashrc if rsh is in use, or set in main.c in
	       the kerberos case, I think) affects what mode we send,
	       and it shouldn't.  */
	    if (file_info != NULL)
	        mode_string = mode_to_string (file_info->st_mode);
	    else
	        mode_string = mode_to_string (sb.st_mode);
	    buf_output0 (&protocol, mode_string);
	    buf_output0 (&protocol, "\n");
	    free (mode_string);
	}

	list = last = NULL;
	size = 0;
	if (sb.st_size > 0)
	{
	    if (gzip_level
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

		fd = open (file, O_RDONLY, 0);
		if (fd < 0)
		    error (1, errno, "reading %s", short_pathname);
		fd = filter_through_gzip (fd, 1, gzip_level, &gzip_pid);
		f = fdopen (fd, "r");
		status = buf_read_file_to_eof (f, &list, &last);
		size = buf_chain_length (list);
		if (status == -2)
		    (*protocol.memory_error) (&protocol);
		else if (status != 0)
		    error (1, ferror (f) ? errno : 0, "reading %s",
			   short_pathname);
		if (fclose (f) == EOF)
		    error (1, errno, "reading %s", short_pathname);
		if (waitpid (gzip_pid, &gzip_status, 0) == -1)
		    error (1, errno, "waiting for gzip process %d", gzip_pid);
		else if (gzip_status != 0)
		    error (1, 0, "gzip exited %d", gzip_status);
		/* Prepending length with "z" is flag for using gzip here.  */
		buf_output0 (&protocol, "z");
	    }
	    else
	    {
		long status;

		size = sb.st_size;
		f = fopen (file, "r");
		if (f == NULL)
		    error (1, errno, "reading %s", short_pathname);
		status = buf_read_file (f, sb.st_size, &list, &last);
		if (status == -2)
		    (*protocol.memory_error) (&protocol);
		else if (status != 0)
		    error (1, ferror (f) ? errno : 0, "reading %s",
			   short_pathname);
		if (fclose (f) == EOF)
		    error (1, errno, "reading %s", short_pathname);
	    }
	}

	sprintf (size_text, "%lu\n", size);
	buf_output0 (&protocol, size_text);

	buf_append_data (&protocol, list, last);
	/* Note we only send a newline here if the file ended with one.  */

	/*
	 * Avoid using up too much disk space for temporary files.
	 * A file which does not exist indicates that the file is up-to-date,
	 * which is now the case.  If this is SERVER_MERGED, the file is
	 * not up-to-date, and we indicate that by leaving the file there.
	 * I'm thinking of cases like "cvs update foo/foo.c foo".
	 */
	if ((updated == SERVER_UPDATED || updated == SERVER_PATCHED)
	    /* But if we are joining, we'll need the file when we call
	       join_file.  */
	    && !joining ())
	    unlink (file);
    }
    else if (scratched_file != NULL && entries_line == NULL)
    {
	if (strcmp (scratched_file, file) != 0)
	    error (1, 0,
		   "CVS server internal error: `%s' vs. `%s' scratched",
		   scratched_file,
		   file);
	free (scratched_file);
	scratched_file = NULL;

	if (kill_scratched_file)
	    buf_output0 (&protocol, "Removed ");
	else
	    buf_output0 (&protocol, "Remove-entry ");
	output_dir (update_dir, repository);
	buf_output0 (&protocol, file);
	buf_output (&protocol, "\n", 1);
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
    buf_send_counted (&protocol);
  done:
    free (short_pathname);
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

    buf_output0 (&protocol, "Set-static-directory ");
    output_dir (update_dir, repository);
    buf_output0 (&protocol, "\n");
    buf_send_counted (&protocol);
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

    buf_output0 (&protocol, "Clear-static-directory ");
    output_dir (update_dir, repository);
    buf_output0 (&protocol, "\n");
    buf_send_counted (&protocol);
}

void
server_set_sticky (update_dir, repository, tag, date)
    char *update_dir;
    char *repository;
    char *tag;
    char *date;
{
    static int set_sticky_supported = -1;
    if (set_sticky_supported == -1)
	set_sticky_supported = supported_response ("Set-sticky");
    if (!set_sticky_supported) return;

    if (noexec)
	return;

    if (tag == NULL && date == NULL)
    {
	buf_output0 (&protocol, "Clear-sticky ");
	output_dir (update_dir, repository);
	buf_output0 (&protocol, "\n");
    }
    else
    {
	buf_output0 (&protocol, "Set-sticky ");
	output_dir (update_dir, repository);
	buf_output0 (&protocol, "\n");
	if (tag != NULL)
	{
	    buf_output0 (&protocol, "T");
	    buf_output0 (&protocol, tag);
	}
	else
	{
	    buf_output0 (&protocol, "D");
	    buf_output0 (&protocol, date);
	}
	buf_output0 (&protocol, "\n");
    }
    buf_send_counted (&protocol);
}

static void
serve_gzip_contents (arg)
     char *arg;
{
    int level;
    level = atoi (arg);
    if (level == 0)
	level = 6;
    gzip_level = level;
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
      printf ("Module-expansion %s\n", mwhere);
    else
      {
	/* We may not need to do this anymore -- check the definition
           of aliases before removing */
	if (*pargc == 1)
	  printf ("Module-expansion %s\n", dir);
	else
	  for (i = 1; i < *pargc; ++i)
	    printf ("Module-expansion %s/%s\n", dir, argv[i]);
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

    /*
     * FIXME: error handling is bogus; do_module can write to stdout and/or
     * stderr and we're not using do_cvs_command.
     */

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
	printf ("error  \n");
    else
	printf ("ok\n");
}

void
server_prog (dir, name, which)
    char *dir;
    char *name;
    enum progs which;
{
    if (!supported_response ("Set-checkin-prog"))
    {
	printf ("E \
warning: this client does not support -i or -u flags in the modules file.\n");
	return;
    }
    switch (which)
    {
	case PROG_CHECKIN:
	    printf ("Set-checkin-prog ");
	    break;
	case PROG_UPDATE:
	    printf ("Set-update-prog ");
	    break;
    }
    printf ("%s\n%s\n", dir, name);
}

static void
serve_checkin_prog (arg)
    char *arg;
{
    FILE *f;
    f = fopen (CVSADM_CIPROG, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_CIPROG));
	sprintf(pending_error_text, "E cannot open %s", CVSADM_CIPROG);
	return;
    }
    if (fprintf (f, "%s\n", arg) < 0)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_CIPROG));
	sprintf(pending_error_text, "E cannot write to %s", CVSADM_CIPROG);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_CIPROG));
	sprintf(pending_error_text, "E cannot close %s", CVSADM_CIPROG);
	return;
    }
}

static void
serve_update_prog (arg)
    char *arg;
{
    FILE *f;
    f = fopen (CVSADM_UPROG, "w+");
    if (f == NULL)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_UPROG));
	sprintf(pending_error_text, "E cannot open %s", CVSADM_UPROG);
	return;
    }
    if (fprintf (f, "%s\n", arg) < 0)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_UPROG));
	sprintf(pending_error_text, "E cannot write to %s", CVSADM_UPROG);
	return;
    }
    if (fclose (f) == EOF)
    {
	pending_error = errno;
	pending_error_text = malloc (80 + strlen(CVSADM_UPROG));
	sprintf(pending_error_text, "E cannot close %s", CVSADM_UPROG);
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
  REQ_LINE("Repository", serve_repository, rq_essential),
  REQ_LINE("Directory", serve_directory, rq_optional),
  REQ_LINE("Max-dotdot", serve_max_dotdot, rq_optional),
  REQ_LINE("Static-directory", serve_static_directory, rq_optional),
  REQ_LINE("Sticky", serve_sticky, rq_optional),
  REQ_LINE("Checkin-prog", serve_checkin_prog, rq_optional),
  REQ_LINE("Update-prog", serve_update_prog, rq_optional),
  REQ_LINE("Entry", serve_entry, rq_essential),
  REQ_LINE("Modified", serve_modified, rq_essential),
  REQ_LINE("Lost", serve_lost, rq_optional),
  REQ_LINE("UseUnchanged", serve_enable_unchanged, rq_enableme),
  REQ_LINE("Unchanged", serve_unchanged, rq_optional),
  REQ_LINE("Argument", serve_argument, rq_essential),
  REQ_LINE("Argumentx", serve_argumentx, rq_essential),
  REQ_LINE("Global_option", serve_global_option, rq_optional),
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
    printf ("Valid-requests");
    for (rq = requests; rq->name != NULL; rq++)
	if (rq->func != NULL)
	    printf (" %s", rq->name);
    printf ("\nok\n");
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
    int len;
    char *cmd;
    char *temp_dir;

    if (dont_delete_temp)
	return;

    /* What a bogus kludge.  This disgusting code makes all kinds of
       assumptions about SunOS, and is only for a bug in that system.
       So only enable it on Suns.  */
#ifdef sun
    if (command_pid > 0) {
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
	switch (errno) {
	case ECHILD:
	  command_pid_is_dead++;
	  break;
	case EINTR:
	  goto do_waitpid;
	}
      else
	/* waitpid should always return one of the above values */
	abort ();
      while (!command_pid_is_dead) {
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
			&timeout)) {
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

    /* This might be set by the user in ~/.bashrc, ~/.cshrc, etc.  */
    temp_dir = getenv ("TMPDIR");
    if (temp_dir == NULL || temp_dir[0] == '\0')
        temp_dir = "/tmp";
    chdir(temp_dir);

    len = strlen (server_temp_dir) + 80;
    cmd = malloc (len);
    if (cmd == NULL)
    {
	printf ("E Cannot delete %s on server; out of memory\n",
		server_temp_dir);
	return;
    }
    sprintf (cmd, "rm -rf %s", server_temp_dir);
    system (cmd);
    free (cmd);
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

    /* Since we're in the server parent process, error should use the
       protocol to report error messages.  */
    error_use_protocol = 1;

    /*
     * Put Rcsbin at the start of PATH, so that rcs programs can find
     * themselves.
     */
#ifdef HAVE_PUTENV
    if (Rcsbin != NULL && *Rcsbin)
    {
        char *p;
	char *env;

	p = getenv ("PATH");
	if (p != NULL)
	{
	    env = malloc (strlen (Rcsbin) + strlen (p) + sizeof "PATH=:");
	    if (env != NULL)
	        sprintf (env, "PATH=%s:%s", Rcsbin, p);
	}
	else
	{
	    env = malloc (strlen (Rcsbin) + sizeof "PATH=");
	    if (env != NULL)
	        sprintf (env, "PATH=%s", Rcsbin);
	}
	if (env == NULL)
	{
	    printf ("E Fatal server error, aborting.\n\
error ENOMEM Virtual memory exhausted.\n");
	    exit (1);
	}
	putenv (env);
    }
#endif

    /* OK, now figure out where we stash our temporary files.  */
    {
	char *p;

	/* This might be set by the user in ~/.bashrc, ~/.cshrc, etc.  */
	char *temp_dir = getenv ("TMPDIR");
	if (temp_dir == NULL || temp_dir[0] == '\0')
	    temp_dir = "/tmp";

	server_temp_dir = malloc (strlen (temp_dir) + 80);
	if (server_temp_dir == NULL)
	{
	    /*
	     * Strictly speaking, we're not supposed to output anything
	     * now.  But we're about to exit(), give it a try.
	     */
	    printf ("E Fatal server error, aborting.\n\
error ENOMEM Virtual memory exhausted.\n");
	    exit (1);
	}
	strcpy (server_temp_dir, temp_dir);

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
	sprintf (p, "%d", getpid ());
    }

    (void) SIG_register (SIGHUP, server_cleanup);
    (void) SIG_register (SIGINT, server_cleanup);
    (void) SIG_register (SIGQUIT, server_cleanup);
    (void) SIG_register (SIGPIPE, server_cleanup);
    (void) SIG_register (SIGTERM, server_cleanup);
    
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
	exit (1);
    }

    argument_count = 1;
    argument_vector[0] = "Dummy argument 0";

    server_active = 1;
    while (1)
    {
	char *cmd, *orig_cmd;
	struct request *rq;
	
	orig_cmd = cmd = read_line (stdin);
	if (cmd == NULL)
	    break;
	if (cmd == NO_MEM_ERROR)
	{
	    printf ("E Fatal server error, aborting.\n\
error ENOMEM Virtual memory exhausted.\n");
	    break;
	}
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
		printf ("error  unrecognized request `%s'\n", cmd);
	}
	free (orig_cmd);
    }
    server_cleanup (0);
    return 0;
}


#ifdef AUTH_SERVER_SUPPORT

/* This was test code, which we may need again. */
#if 0
  /* If we were invoked this way, then stdin comes from the
     client and stdout/stderr writes to it. */
  int c;
  while ((c = getc (stdin)) != EOF && c != '*')
    {
      printf ("%c", toupper (c));
      fflush (stdout);
    }
  exit (0);
#endif /* 1/0 */


/* 
 * 0 means no entry found for this user.
 * 1 means entry found and password matches.
 * 2 means entry found, but password does not match.
 */
int
check_repository_password (username, password, repository)
     char *username, *password, *repository;
{
  int retval = 0;
  FILE *fp;
  char *filename;
  char linebuf[MAXLINELEN];
  int found_it = 0, len;

  filename = xmalloc (strlen (repository)
                      + 1
                      + strlen ("CVSROOT")
                      + 1
                      + strlen ("passwd")
                      + 1);

  strcpy (filename, repository);
  strcat (filename, "/CVSROOT");
  strcat (filename, "/passwd");
  
  fp = fopen (filename, "r");
  if (fp == NULL)
    {
      /* This is ok -- the cvs passwd file might not exist. */
      fclose (fp);
      return 0;
    }

  /* Look for a relevant line -- one with this user's name. */
  len = strlen (username);
  while (fgets (linebuf, MAXPATHLEN - 1, fp))
    {
      if ((strncmp (linebuf, username, len) == 0)
          && (linebuf[len] == ':'))
        {
          found_it = 1;
          break;
        }
    }
  fclose (fp);

  /* If found_it != 0, then linebuf contains the information we need. */
  if (found_it)
    {
      char *found_password;

      strtok (linebuf, ":");
      found_password = strtok (NULL, ": \n");

      if (strcmp (found_password, crypt (password, found_password)) == 0)
        retval = 1;
      else
        retval = 2;
    }
  else
    retval = 0;

  free (filename);

  return retval;
}


/* Return 1 if password matches, else 0. */
int
check_password (username, password, repository)
     char *username, *password, *repository;
{
  int rc;

  /* First we see if this user has a password in the CVS-specific
     password file.  If so, that's enough to authenticate with.  If
     not, we'll check /etc/passwd. */

  rc = check_repository_password (username, password, repository);

  if (rc == 1)
    return 1;
  else if (rc == 2)
    return 0;
  else if (rc == 0)
    {
      /* No cvs password found, so try /etc/passwd. */

      struct passwd *pw;
      char *found_passwd;

      pw = getpwnam (username);
      if (pw == NULL)
        {
          printf ("E Fatal error, aborting.\n"
                  "error 0 %s: no such user\n", username);
          exit (1);
        }
      found_passwd = pw->pw_passwd;
      
      if (found_passwd && *found_passwd)
        return (! strcmp (found_passwd, crypt (password, found_passwd)));
      else if (password && *password)
        return 1;
      else
        return 0;
    }
  else
    {
      /* Something strange happened.  We don't know what it was, but
         we certainly won't grant authorization. */
      return 0;
    }
}


/* Read username and password from client (i.e., stdin).
   If correct, then switch to run as that user and send an ACK to the
   client via stdout, else send NACK and die. */
void
authenticate_connection ()
{
  int len;
  char tmp[PATH_MAX];
  char repository[PATH_MAX];
  char username[PATH_MAX];
  char password[PATH_MAX];
  char server_user[PATH_MAX];
  struct passwd *pw;
  
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
   * Note that the actual client/server protocol has not started up
   * yet, because we haven't authenticated!  Therefore, there are
   * certain things we can't take for granted.  For example, don't use
   * error() because `error_use_protocol' has not yet been set by
   * server().  
   *
   * We need to know where the repository is too, to look up the
   * password in the special CVS passwd file before we try
   * /etc/passwd.  However, the repository is normally transmitted in
   * the regular client/server protocol, which has not yet started,
   * blah blah blah.  This is why the client transmits the repository
   * as part of the "authentication protocol".  Thus, the repository
   * will be redundantly retransmitted later, but that's no big deal.
   */

  /* Make sure the protocol starts off on the right foot... */
  fgets (tmp, PATH_MAX, stdin);
  if (strcmp (tmp, "BEGIN AUTH REQUEST\n"))
    {
      printf ("error: bad auth protocol start: %s", tmp);
      fflush (stdout);
      exit (1);
    }
    
  /* Get the three important pieces of information in order. */
  fgets (repository, PATH_MAX, stdin);
  fgets (username, PATH_MAX, stdin);
  fgets (password, PATH_MAX, stdin);

  /* Make them pure. */ 
  strip_trailing_newlines (repository);
  strip_trailing_newlines (username);
  strip_trailing_newlines (password);

  /* ... and make sure the protocol ends on the right foot. */
  fgets (tmp, PATH_MAX, stdin);
  if (strcmp (tmp, "END AUTH REQUEST\n"))
    {
      printf ("error: bad auth protocol end: %s", tmp);
      fflush (stdout);
      exit (1);
    }

  if (check_password (username, password, repository))
    {
      printf ("I LOVE YOU\n");
      fflush (stdout);
    }
  else
    {
      printf ("I HATE YOU\n");
      fflush (stdout);
      exit (1);
    }
  
  /* Do everything that kerberos did. */
  pw = getpwnam (username);
  if (pw == NULL)
    {
      printf ("E Fatal error, aborting.\n"
              "error 0 %s: no such user\n", username);
      exit (1);
    }
  
  initgroups (pw->pw_name, pw->pw_gid);
  setgid (pw->pw_gid);
  setuid (pw->pw_uid);
  /* Inhibit access by randoms.  Don't want people randomly
     changing our temporary tree before we check things in.  */
  umask (077);
  
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

#endif AUTH_SERVER_SUPPORT


#endif /* SERVER_SUPPORT */

