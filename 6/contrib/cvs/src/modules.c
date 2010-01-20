/*
 *    Copyright (c) 1992, Brian Berliner and Jeff Polk
 *    Copyright (c) 1989-1992, Brian Berliner
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the CVS source
 *    distribution.
 *
 * Modules
 *
 *	Functions for accessing the modules file.
 *
 *	The modules file supports basically three formats of lines:
 *		key [options] directory files... [ -x directory [files] ] ...
 *		key [options] directory [ -x directory [files] ] ...
 *		key -a aliases...
 *
 *	The -a option allows an aliasing step in the parsing of the modules
 *	file.  The "aliases" listed on a line following the -a are
 *	processed one-by-one, as if they were specified as arguments on the
 *	command line.
 */

#include <assert.h>
#include "cvs.h"
#include "savecwd.h"


/* Defines related to the syntax of the modules file.  */

/* Options in modules file.  Note that it is OK to use GNU getopt features;
   we already are arranging to make sure we are using the getopt distributed
   with CVS.  */
#define	CVSMODULE_OPTS	"+ad:lo:e:s:t:"

/* Special delimiter.  */
#define CVSMODULE_SPEC	'&'

struct sortrec
{
    /* Name of the module, malloc'd.  */
    char *modname;
    /* If Status variable is set, this is either def_status or the malloc'd
       name of the status.  If Status is not set, the field is left
       uninitialized.  */
    char *status;
    /* Pointer to a malloc'd array which contains (1) the raw contents
       of the options and arguments, excluding comments, (2) a '\0',
       and (3) the storage for the "comment" field.  */
    char *rest;
    char *comment;
};

static int sort_order PROTO((const PTR l, const PTR r));
static void save_d PROTO((char *k, int ks, char *d, int ds));


/*
 * Open the modules file, and die if the CVSROOT environment variable
 * was not set.  If the modules file does not exist, that's fine, and
 * a warning message is displayed and a NULL is returned.
 */
DBM *
open_module ()
{
    char *mfile;
    DBM *retval;

    if (current_parsed_root == NULL)
    {
	error (0, 0, "must set the CVSROOT environment variable");
	error (1, 0, "or specify the '-d' global option");
    }
    mfile = xmalloc (strlen (current_parsed_root->directory)
		     + sizeof (CVSROOTADM)
		     + sizeof (CVSROOTADM_MODULES) + 3);
    (void) sprintf (mfile, "%s/%s/%s", current_parsed_root->directory,
		    CVSROOTADM, CVSROOTADM_MODULES);
    retval = dbm_open (mfile, O_RDONLY, 0666);
    free (mfile);
    return retval;
}

/*
 * Close the modules file, if the open succeeded, that is
 */
void
close_module (db)
    DBM *db;
{
    if (db != NULL)
	dbm_close (db);
}



/*
 * This is the recursive function that processes a module name.
 * It calls back the passed routine for each directory of a module
 * It runs the post checkout or post tag proc from the modules file
 */
static int
my_module (db, mname, m_type, msg, callback_proc, where, shorten,
	   local_specified, run_module_prog, build_dirs, extra_arg,
	   stack)
    DBM *db;
    char *mname;
    enum mtype m_type;
    char *msg;
    CALLBACKPROC callback_proc;
    char *where;
    int shorten;
    int local_specified;
    int run_module_prog;
    int build_dirs;
    char *extra_arg;
    List *stack;
{
    char *checkout_prog = NULL;
    char *export_prog = NULL;
    char *tag_prog = NULL;
    struct saved_cwd cwd;
    int cwd_saved = 0;
    char *line;
    int modargc;
    int xmodargc;
    char **modargv;
    char **xmodargv = NULL;
    /* Found entry from modules file, including options and such.  */
    char *value = NULL;
    char *mwhere = NULL;
    char *mfile = NULL;
    char *spec_opt = NULL;
    char *xvalue = NULL;
    int alias = 0;
    datum key, val;
    char *cp;
    int c, err = 0;
    int nonalias_opt = 0;

#ifdef SERVER_SUPPORT
    int restore_server_dir = 0;
    char *server_dir_to_restore = NULL;
    if (trace)
    {
	char *buf;

	/* We use cvs_outerr, rather than fprintf to stderr, because
	   this may be called by server code with error_use_protocol
	   set.  */
	buf = xmalloc (100
		       + strlen (mname)
		       + strlen (msg)
		       + (where ? strlen (where) : 0)
		       + (extra_arg ? strlen (extra_arg) : 0));
	sprintf (buf, "%s-> my_module (%s, %s, %s, %s)\n",
		 CLIENT_SERVER_STR,
		 mname, msg, where ? where : "",
		 extra_arg ? extra_arg : "");
	cvs_outerr (buf, 0);
	free (buf);
    }
#endif

    /* Don't process absolute directories.  Anything else could be a security
     * problem.  Before this check was put in place:
     *
     *   $ cvs -d:fork:/cvsroot co /foo
     *   cvs server: warning: cannot make directory CVS in /: Permission denied
     *   cvs [server aborted]: cannot make directory /foo: Permission denied
     *   $
     */
    if (isabsolute (mname))
	error (1, 0, "Absolute module reference invalid: `%s'", mname);

    /* Similarly for directories that attempt to step above the root of the
     * repository.
     */
    if (pathname_levels (mname) > 0)
	error (1, 0, "up-level in module reference (`..') invalid: `%s'.",
               mname);

    /* if this is a directory to ignore, add it to that list */
    if (mname[0] == '!' && mname[1] != '\0')
    {
	ign_dir_add (mname+1);
	goto do_module_return;
    }

    /* strip extra stuff from the module name */
    strip_trailing_slashes (mname);

    /*
     * Look up the module using the following scheme:
     *	1) look for mname as a module name
     *	2) look for mname as a directory
     *	3) look for mname as a file
     *  4) take mname up to the first slash and look it up as a module name
     *	   (this is for checking out only part of a module)
     */

    /* look it up as a module name */
    key.dptr = mname;
    key.dsize = strlen (key.dptr);
    if (db != NULL)
	val = dbm_fetch (db, key);
    else
	val.dptr = NULL;
    if (val.dptr != NULL)
    {
	/* copy and null terminate the value */
	value = xmalloc (val.dsize + 1);
	memcpy (value, val.dptr, val.dsize);
	value[val.dsize] = '\0';

	/* If the line ends in a comment, strip it off */
	if ((cp = strchr (value, '#')) != NULL)
	    *cp = '\0';
	else
	    cp = value + val.dsize;

	/* Always strip trailing spaces */
	while (cp > value && isspace ((unsigned char) *--cp))
	    *cp = '\0';

	mwhere = xstrdup (mname);
	goto found;
    }
    else
    {
	char *file;
	char *attic_file;
	char *acp;
	int is_found = 0;

	/* check to see if mname is a directory or file */
	file = xmalloc (strlen (current_parsed_root->directory)
			+ strlen (mname) + sizeof(RCSEXT) + 2);
	(void) sprintf (file, "%s/%s", current_parsed_root->directory, mname);
	attic_file = xmalloc (strlen (current_parsed_root->directory)
			      + strlen (mname)
			      + sizeof (CVSATTIC) + sizeof (RCSEXT) + 3);
	if ((acp = strrchr (mname, '/')) != NULL)
	{
	    *acp = '\0';
	    (void) sprintf (attic_file, "%s/%s/%s/%s%s", current_parsed_root->directory,
			    mname, CVSATTIC, acp + 1, RCSEXT);
	    *acp = '/';
	}
	else
	    (void) sprintf (attic_file, "%s/%s/%s%s",
	                    current_parsed_root->directory,
			    CVSATTIC, mname, RCSEXT);

	if (isdir (file))
	{
	    modargv = xmalloc (sizeof (*modargv));
	    modargv[0] = xstrdup (mname);
	    modargc = 1;
	    is_found = 1;
	}
	else
	{
	    (void) strcat (file, RCSEXT);
	    if (isfile (file) || isfile (attic_file))
	    {
		/* if mname was a file, we have to split it into "dir file" */
		if ((cp = strrchr (mname, '/')) != NULL && cp != mname)
		{
		    modargv = xmalloc (2 * sizeof (*modargv));
		    modargv[0] = xmalloc (strlen (mname) + 2);
		    strncpy (modargv[0], mname, cp - mname);
		    modargv[0][cp - mname] = '\0';
		    modargv[1] = xstrdup (cp + 1);
		    modargc = 2;
		}
		else
		{
		    /*
		     * the only '/' at the beginning or no '/' at all
		     * means the file we are interested in is in CVSROOT
		     * itself so the directory should be '.'
		     */
		    if (cp == mname)
		    {
			/* drop the leading / if specified */
			modargv = xmalloc (2 * sizeof (*modargv));
			modargv[0] = xstrdup (".");
			modargv[1] = xstrdup (mname + 1);
			modargc = 2;
		    }
		    else
		    {
			/* otherwise just copy it */
			modargv = xmalloc (2 * sizeof (*modargv));
			modargv[0] = xstrdup (".");
			modargv[1] = xstrdup (mname);
			modargc = 2;
		    }
		}
		is_found = 1;
	    }
	}
	free (attic_file);
	free (file);

	if (is_found)
	{
	    assert (value == NULL);

	    /* OK, we have now set up modargv with the actual
	       file/directory we want to work on.  We duplicate a
	       small amount of code here because the vast majority of
	       the code after the "found" label does not pertain to
	       the case where we found a file/directory rather than
	       finding an entry in the modules file.  */
	    if (save_cwd (&cwd))
		error_exit ();
	    cwd_saved = 1;

	    err += callback_proc (modargc, modargv, where, mwhere, mfile,
				  shorten,
				  local_specified, mname, msg);

	    free_names (&modargc, modargv);

	    /* cd back to where we started.  */
	    if (restore_cwd (&cwd, NULL))
		error_exit ();
	    free_cwd (&cwd);
	    cwd_saved = 0;

	    goto do_module_return;
	}
    }

    /* look up everything to the first / as a module */
    if (mname[0] != '/' && (cp = strchr (mname, '/')) != NULL)
    {
	/* Make the slash the new end of the string temporarily */
	*cp = '\0';
	key.dptr = mname;
	key.dsize = strlen (key.dptr);

	/* do the lookup */
	if (db != NULL)
	    val = dbm_fetch (db, key);
	else
	    val.dptr = NULL;

	/* if we found it, clean up the value and life is good */
	if (val.dptr != NULL)
	{
	    char *cp2;

	    /* copy and null terminate the value */
	    value = xmalloc (val.dsize + 1);
	    memcpy (value, val.dptr, val.dsize);
	    value[val.dsize] = '\0';

	    /* If the line ends in a comment, strip it off */
	    if ((cp2 = strchr (value, '#')) != NULL)
		*cp2 = '\0';
	    else
		cp2 = value + val.dsize;

	    /* Always strip trailing spaces */
	    while (cp2 > value  &&  isspace ((unsigned char) *--cp2))
		*cp2 = '\0';

	    /* mwhere gets just the module name */
	    mwhere = xstrdup (mname);
	    mfile = cp + 1;

	    /* put the / back in mname */
	    *cp = '/';

	    goto found;
	}

	/* put the / back in mname */
	*cp = '/';
    }

    /* if we got here, we couldn't find it using our search, so give up */
    error (0, 0, "cannot find module `%s' - ignored", mname);
    err++;
    goto do_module_return;


    /*
     * At this point, we found what we were looking for in one
     * of the many different forms.
     */
  found:

    /* remember where we start */
    if (save_cwd (&cwd))
	error_exit ();
    cwd_saved = 1;

    assert (value != NULL);

    /* search the value for the special delimiter and save for later */
    if ((cp = strchr (value, CVSMODULE_SPEC)) != NULL)
    {
	*cp = '\0';			/* null out the special char */
	spec_opt = cp + 1;		/* save the options for later */

	/* strip whitespace if necessary */
	while (cp > value  &&  isspace ((unsigned char) *--cp))
	    *cp = '\0';
    }

    /* don't do special options only part of a module was specified */
    if (mfile != NULL)
	spec_opt = NULL;

    /*
     * value now contains one of the following:
     *    1) dir
     *	  2) dir file
     *    3) the value from modules without any special args
     *		    [ args ] dir [file] [file] ...
     *	     or     -a module [ module ] ...
     */

    /* Put the value on a line with XXX prepended for getopt to eat */
    line = xmalloc (strlen (value) + 5);
    strcpy(line, "XXX ");
    strcpy(line + 4, value);

    /* turn the line into an argv[] array */
    line2argv (&xmodargc, &xmodargv, line, " \t");
    free (line);
    modargc = xmodargc;
    modargv = xmodargv;

    /* parse the args */
    optind = 0;
    while ((c = getopt (modargc, modargv, CVSMODULE_OPTS)) != -1)
    {
	switch (c)
	{
	    case 'a':
		alias = 1;
		break;
	    case 'd':
		if (mwhere)
		    free (mwhere);
		mwhere = xstrdup (optarg);
		nonalias_opt = 1;
		break;
	    case 'l':
		local_specified = 1;
		nonalias_opt = 1;
		break;
	    case 'o':
		if (checkout_prog)
		    free (checkout_prog);
		checkout_prog = xstrdup (optarg);
		nonalias_opt = 1;
		break;
	    case 'e':
		if (export_prog)
		    free (export_prog);
		export_prog = xstrdup (optarg);
		nonalias_opt = 1;
		break;
	    case 't':
		if (tag_prog)
		    free (tag_prog);
		tag_prog = xstrdup (optarg);
		nonalias_opt = 1;
		break;
	    case '?':
		error (0, 0,
		       "modules file has invalid option for key %s value %s",
		       key.dptr, value);
		err++;
		goto do_module_return;
	}
    }
    modargc -= optind;
    modargv += optind;
    if (modargc == 0  &&  spec_opt == NULL)
    {
	error (0, 0, "modules file missing directory for module %s", mname);
	++err;
	goto do_module_return;
    }

    if (alias && nonalias_opt)
    {
	/* The documentation has never said it is legal to specify
	   -a along with another option.  And I believe that in the past
	   CVS has ignored the options other than -a, more or less, in this
	   situation.  */
	error (0, 0, "\
-a cannot be specified in the modules file along with other options");
	++err;
	goto do_module_return;
    }

    /* if this was an alias, call ourselves recursively for each module */
    if (alias)
    {
	int i;

	for (i = 0; i < modargc; i++)
	{
	    /* 
	     * Recursion check: if an alias module calls itself or a module
	     * which causes the first to be called again, print an error
	     * message and stop recursing.
	     *
	     * Algorithm:
	     *
	     *   1. Check that MNAME isn't in the stack.
	     *   2. Push MNAME onto the stack.
	     *   3. Call do_module().
	     *   4. Pop MNAME from the stack.
	     */
	    if (stack && findnode (stack, mname))
		error (0, 0,
		       "module `%s' in modules file contains infinite loop",
		       mname);
	    else
	    {
		if (!stack) stack = getlist();
		push_string (stack, mname);
		err += my_module (db, modargv[i], m_type, msg, callback_proc,
                                   where, shorten, local_specified,
                                   run_module_prog, build_dirs, extra_arg,
                                   stack);
		pop_string (stack);
		if (isempty (stack)) dellist (&stack);
	    }
	}
	goto do_module_return;
    }

    if (mfile != NULL && modargc > 1)
    {
	error (0, 0, "\
module `%s' is a request for a file in a module which is not a directory",
	       mname);
	++err;
	goto do_module_return;
    }

    /* otherwise, process this module */
    if (modargc > 0)
    {
	err += callback_proc (modargc, modargv, where, mwhere, mfile, shorten,
			      local_specified, mname, msg);
    }
    else
    {
	/*
	 * we had nothing but special options, so we must
	 * make the appropriate directory and cd to it
	 */
	char *dir;

	if (!build_dirs)
	    goto do_special;

	dir = where ? where : (mwhere ? mwhere : mname);
	/* XXX - think about making null repositories at each dir here
		 instead of just at the bottom */
	make_directories (dir);
	if (CVS_CHDIR (dir) < 0)
	{
	    error (0, errno, "cannot chdir to %s", dir);
	    spec_opt = NULL;
	    err++;
	    goto do_special;
	}
	if (!isfile (CVSADM))
	{
	    char *nullrepos;

	    nullrepos = emptydir_name ();

	    Create_Admin (".", dir,
			  nullrepos, (char *) NULL, (char *) NULL, 0, 0, 1);
	    if (!noexec)
	    {
		FILE *fp;

		fp = open_file (CVSADM_ENTSTAT, "w+");
		if (fclose (fp) == EOF)
		    error (1, errno, "cannot close %s", CVSADM_ENTSTAT);
#ifdef SERVER_SUPPORT
		if (server_active)
		    server_set_entstat (dir, nullrepos);
#endif
	    }
	    free (nullrepos);
	}
    }

    /* if there were special include args, process them now */

  do_special:

    free_names (&xmodargc, xmodargv);
    xmodargv = NULL;

    /* blow off special options if -l was specified */
    if (local_specified)
	spec_opt = NULL;

#ifdef SERVER_SUPPORT
    /* We want to check out into the directory named by the module.
       So we set a global variable which tells the server to glom that
       directory name onto the front.  A cleaner approach would be some
       way of passing it down to the recursive call, through the
       callback_proc, to start_recursion, and then into the update_dir in
       the struct file_info.  That way the "Updating foo" message could
       print the actual directory we are checking out into.

       For local CVS, this is handled by the chdir call above
       (directly or via the callback_proc).  */
    if (server_active && spec_opt != NULL)
    {
	char *change_to;

	change_to = where ? where : (mwhere ? mwhere : mname);
	server_dir_to_restore = server_dir;
	restore_server_dir = 1;
	server_dir =
	    xmalloc ((server_dir_to_restore != NULL
		      ? strlen (server_dir_to_restore)
		      : 0)
		     + strlen (change_to)
		     + 5);
	server_dir[0] = '\0';
	if (server_dir_to_restore != NULL)
	{
	    strcat (server_dir, server_dir_to_restore);
	    strcat (server_dir, "/");
	}
	strcat (server_dir, change_to);
    }
#endif

    while (spec_opt != NULL)
    {
	char *next_opt;

	cp = strchr (spec_opt, CVSMODULE_SPEC);
	if (cp != NULL)
	{
	    /* save the beginning of the next arg */
	    next_opt = cp + 1;

	    /* strip whitespace off the end */
	    do
		*cp = '\0';
	    while (cp > spec_opt  &&  isspace ((unsigned char) *--cp));
	}
	else
	    next_opt = NULL;

	/* strip whitespace from front */
	while (isspace ((unsigned char) *spec_opt))
	    spec_opt++;

	if (*spec_opt == '\0')
	    error (0, 0, "Mal-formed %c option for module %s - ignored",
		   CVSMODULE_SPEC, mname);
	else
	    err += my_module (db, spec_opt, m_type, msg, callback_proc,
                               (char *) NULL, 0, local_specified,
                               run_module_prog, build_dirs, extra_arg,
	                       stack);
	spec_opt = next_opt;
    }

#ifdef SERVER_SUPPORT
    if (server_active && restore_server_dir)
    {
	free (server_dir);
	server_dir = server_dir_to_restore;
    }
#endif

    /* cd back to where we started */
    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);
    cwd_saved = 0;

    /* run checkout or tag prog if appropriate */
    if (err == 0 && run_module_prog)
    {
	if ((m_type == TAG && tag_prog != NULL) ||
	    (m_type == CHECKOUT && checkout_prog != NULL) ||
	    (m_type == EXPORT && export_prog != NULL))
	{
	    /*
	     * If a relative pathname is specified as the checkout, tag
	     * or export proc, try to tack on the current "where" value.
	     * if we can't find a matching program, just punt and use
	     * whatever is specified in the modules file.
	     */
	    char *real_prog = NULL;
	    char *prog = (m_type == TAG ? tag_prog :
			  (m_type == CHECKOUT ? checkout_prog : export_prog));
	    char *real_where = (where != NULL ? where : mwhere);
	    char *expanded_path;

	    if ((*prog != '/') && (*prog != '.'))
	    {
		real_prog = xmalloc (strlen (real_where) + strlen (prog)
				     + 10);
		(void) sprintf (real_prog, "%s/%s", real_where, prog);
		if (isfile (real_prog))
		    prog = real_prog;
	    }

	    /* XXX can we determine the line number for this entry??? */
	    expanded_path = expand_path (prog, "modules", 0);
	    if (expanded_path != NULL)
	    {
		run_setup (expanded_path);
		run_arg (real_where);

		if (extra_arg)
		    run_arg (extra_arg);

		if (!quiet)
		{
		    cvs_output (program_name, 0);
		    cvs_output (" ", 1);
		    cvs_output (cvs_cmd_name, 0);
		    cvs_output (": Executing '", 0);
		    run_print (stdout);
		    cvs_output ("'\n", 0);
		    cvs_flushout ();
		}
		err += run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
		free (expanded_path);
	    }
	    free (real_prog);
	}
    }

 do_module_return:
    /* clean up */
    if (xmodargv != NULL)
	free_names (&xmodargc, xmodargv);
    if (mwhere)
	free (mwhere);
    if (checkout_prog)
	free (checkout_prog);
    if (export_prog)
	free (export_prog);
    if (tag_prog)
	free (tag_prog);
    if (cwd_saved)
	free_cwd (&cwd);
    if (value != NULL)
	free (value);

    if (xvalue != NULL)
	free (xvalue);
    return (err);
}



/* External face of do_module so that we can have an internal version which
 * accepts a stack argument to track alias recursion.
 */
int
do_module (db, mname, m_type, msg, callback_proc, where, shorten,
	   local_specified, run_module_prog, build_dirs, extra_arg)
    DBM *db;
    char *mname;
    enum mtype m_type;
    char *msg;
    CALLBACKPROC callback_proc;
    char *where;
    int shorten;
    int local_specified;
    int run_module_prog;
    int build_dirs;
    char *extra_arg;
{
    return my_module (db, mname, m_type, msg, callback_proc, where, shorten,
                       local_specified, run_module_prog, build_dirs, extra_arg,
                       NULL);
}



/* - Read all the records from the modules database into an array.
   - Sort the array depending on what format is desired.
   - Print the array in the format desired.

   Currently, there are only two "desires":

   1. Sort by module name and format the whole entry including switches,
      files and the comment field: (Including aliases)

      modulename	-s switches, one per line, even if
			it has many switches.
			Directories and files involved, formatted
			to cover multiple lines if necessary.
			# Comment, also formatted to cover multiple
			# lines if necessary.

   2. Sort by status field string and print:  (*not* including aliases)

      modulename    STATUS	Directories and files involved, formatted
				to cover multiple lines if necessary.
				# Comment, also formatted to cover multiple
				# lines if necessary.
*/

static struct sortrec *s_head;

static int s_max = 0;			/* Number of elements allocated */
static int s_count = 0;			/* Number of elements used */

static int Status;		        /* Nonzero if the user is
					   interested in status
					   information as well as
					   module name */
static char def_status[] = "NONE";

/* Sort routine for qsort:
   - If we want the "Status" field to be sorted, check it first.
   - Then compare the "module name" fields.  Since they are unique, we don't
     have to look further.
*/
static int
sort_order (l, r)
    const PTR l;
    const PTR r;
{
    int i;
    const struct sortrec *left = (const struct sortrec *) l;
    const struct sortrec *right = (const struct sortrec *) r;

    if (Status)
    {
	/* If Sort by status field, compare them. */
	if ((i = strcmp (left->status, right->status)) != 0)
	    return (i);
    }
    return (strcmp (left->modname, right->modname));
}

static void
save_d (k, ks, d, ds)
    char *k;
    int ks;
    char *d;
    int ds;
{
    char *cp, *cp2;
    struct sortrec *s_rec;

    if (Status && *d == '-' && *(d + 1) == 'a')
	return;				/* We want "cvs co -s" and it is an alias! */

    if (s_count == s_max)
    {
	s_max += 64;
	s_head = (struct sortrec *) xrealloc ((char *) s_head, s_max * sizeof (*s_head));
    }
    s_rec = &s_head[s_count];
    s_rec->modname = cp = xmalloc (ks + 1);
    (void) strncpy (cp, k, ks);
    *(cp + ks) = '\0';

    s_rec->rest = cp2 = xmalloc (ds + 1);
    cp = d;
    *(cp + ds) = '\0';	/* Assumes an extra byte at end of static dbm buffer */

    while (isspace ((unsigned char) *cp))
	cp++;
    /* Turn <spaces> into one ' ' -- makes the rest of this routine simpler */
    while (*cp)
    {
	if (isspace ((unsigned char) *cp))
	{
	    *cp2++ = ' ';
	    while (isspace ((unsigned char) *cp))
		cp++;
	}
	else
	    *cp2++ = *cp++;
    }
    *cp2 = '\0';

    /* Look for the "-s statusvalue" text */
    if (Status)
    {
	s_rec->status = def_status;

	for (cp = s_rec->rest; (cp2 = strchr (cp, '-')) != NULL; cp = ++cp2)
	{
	    if (*(cp2 + 1) == 's' && *(cp2 + 2) == ' ')
	    {
		char *status_start;

		cp2 += 3;
		status_start = cp2;
		while (*cp2 != ' ' && *cp2 != '\0')
		    cp2++;
		s_rec->status = xmalloc (cp2 - status_start + 1);
		strncpy (s_rec->status, status_start, cp2 - status_start);
		s_rec->status[cp2 - status_start] = '\0';
		cp = cp2;
		break;
	    }
	}
    }
    else
	cp = s_rec->rest;

    /* Find comment field, clean up on all three sides & compress blanks */
    if ((cp2 = cp = strchr (cp, '#')) != NULL)
    {
	if (*--cp2 == ' ')
	    *cp2 = '\0';
	if (*++cp == ' ')
	    cp++;
	s_rec->comment = cp;
    }
    else
	s_rec->comment = "";

    s_count++;
}

/* Print out the module database as we know it.  If STATUS is
   non-zero, print out status information for each module. */

void
cat_module (status)
    int status;
{
    DBM *db;
    datum key, val;
    int i, c, wid, argc, cols = 80, indent, fill;
    int moduleargc;
    struct sortrec *s_h;
    char *cp, *cp2, **argv;
    char **moduleargv;

    Status = status;

    /* Read the whole modules file into allocated records */
    if (!(db = open_module ()))
	error (1, 0, "failed to open the modules file");

    for (key = dbm_firstkey (db); key.dptr != NULL; key = dbm_nextkey (db))
    {
	val = dbm_fetch (db, key);
	if (val.dptr != NULL)
	    save_d (key.dptr, key.dsize, val.dptr, val.dsize);
    }

    close_module (db);

    /* Sort the list as requested */
    qsort ((PTR) s_head, s_count, sizeof (struct sortrec), sort_order);

    /*
     * Run through the sorted array and format the entries
     * indent = space for modulename + space for status field
     */
    indent = 12 + (status * 12);
    fill = cols - (indent + 2);
    for (s_h = s_head, i = 0; i < s_count; i++, s_h++)
    {
	char *line;

	/* Print module name (and status, if wanted) */
	line = xmalloc (strlen (s_h->modname) + 15);
	sprintf (line, "%-12s", s_h->modname);
	cvs_output (line, 0);
	free (line);
	if (status)
	{
	    line = xmalloc (strlen (s_h->status) + 15);
	    sprintf (line, " %-11s", s_h->status);
	    cvs_output (line, 0);
	    free (line);
	}

	line = xmalloc (strlen (s_h->modname) + strlen (s_h->rest) + 15);
	/* Parse module file entry as command line and print options */
	(void) sprintf (line, "%s %s", s_h->modname, s_h->rest);
	line2argv (&moduleargc, &moduleargv, line, " \t");
	free (line);
	argc = moduleargc;
	argv = moduleargv;

	optind = 0;
	wid = 0;
	while ((c = getopt (argc, argv, CVSMODULE_OPTS)) != -1)
	{
	    if (!status)
	    {
		if (c == 'a' || c == 'l')
		{
		    char buf[5];

		    sprintf (buf, " -%c", c);
		    cvs_output (buf, 0);
		    wid += 3;		/* Could just set it to 3 */
		}
		else
		{
		    char buf[10];

		    if (strlen (optarg) + 4 + wid > (unsigned) fill)
		    {
			int j;

			cvs_output ("\n", 1);
			for (j = 0; j < indent; ++j)
			    cvs_output (" ", 1);
			wid = 0;
		    }
		    sprintf (buf, " -%c ", c);
		    cvs_output (buf, 0);
		    cvs_output (optarg, 0);
		    wid += strlen (optarg) + 4;
		}
	    }
	}
	argc -= optind;
	argv += optind;

	/* Format and Print all the files and directories */
	for (; argc--; argv++)
	{
	    if (strlen (*argv) + wid > (unsigned) fill)
	    {
		int j;

		cvs_output ("\n", 1);
		for (j = 0; j < indent; ++j)
		    cvs_output (" ", 1);
		wid = 0;
	    }
	    cvs_output (" ", 1);
	    cvs_output (*argv, 0);
	    wid += strlen (*argv) + 1;
	}
	cvs_output ("\n", 1);

	/* Format the comment field -- save_d (), compressed spaces */
	for (cp2 = cp = s_h->comment; *cp; cp2 = cp)
	{
	    int j;

	    for (j = 0; j < indent; ++j)
		cvs_output (" ", 1);
	    cvs_output (" # ", 0);
	    if (strlen (cp2) < (unsigned) (fill - 2))
	    {
		cvs_output (cp2, 0);
		cvs_output ("\n", 1);
		break;
	    }
	    cp += fill - 2;
	    while (*cp != ' ' && cp > cp2)
		cp--;
	    if (cp == cp2)
	    {
		cvs_output (cp2, 0);
		cvs_output ("\n", 1);
		break;
	    }

	    *cp++ = '\0';
	    cvs_output (cp2, 0);
	    cvs_output ("\n", 1);
	}

	free_names(&moduleargc, moduleargv);
	/* FIXME-leak: here is where we would free s_h->modname, s_h->rest,
	   and if applicable, s_h->status.  Not exactly a memory leak,
	   in the sense that we are about to exit(), but may be worth
	   noting if we ever do a multithreaded server or something of
	   the sort.  */
    }
    /* FIXME-leak: as above, here is where we would free s_head.  */
}
