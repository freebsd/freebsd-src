/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Administration ("cvs admin")
 * 
 */

#include "cvs.h"
#ifdef CVS_ADMIN_GROUP
#include <grp.h>
#endif
#include <assert.h>

static Dtype admin_dirproc PROTO ((void *callerdat, char *dir,
				   char *repos, char *update_dir,
				   List *entries));
static int admin_fileproc PROTO ((void *callerdat, struct file_info *finfo));

static const char *const admin_usage[] =
{
    "Usage: %s %s rcs-options files...\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* This structure is used to pass information through start_recursion.  */
struct admin_data
{
    /* Set default branch (-b).  It is "-b" followed by the value
       given, or NULL if not specified, or merely "-b" if -b is
       specified without a value.  */
    char *branch;

    /* Set comment leader (-c).  It is "-c" followed by the value
       given, or NULL if not specified.  The comment leader is
       relevant only for old versions of RCS, but we let people set it
       anyway.  */
    char *comment;

    /* Set strict locking (-L).  */
    int set_strict;

    /* Set nonstrict locking (-U).  */
    int set_nonstrict;

    /* Delete revisions (-o).  It is "-o" followed by the value specified.  */
    char *delete_revs;

    /* Keyword substitution mode (-k), e.g. "-kb".  */
    char *kflag;

    /* Description (-t).  See sanity.sh for various moanings about
       files and stdin and such.  "" if -t specified without an
       argument.  It is "-t" followed by the argument.  */
    char *desc;

    /* Interactive (-I).  Problematic with client/server.  */
    int interactive;

    /* Quiet (-q).  Not the same as the global -q option, which is a bit
       on the confusing side, perhaps.  */
    int quiet;

    /* This is the cheesy part.  It is a vector with the options which
       we don't deal with above (e.g. "-afoo" "-abar,baz").  In the future
       this presumably will be replaced by other variables which break
       out the data in a more convenient fashion.  AV as well as each of
       the strings it points to is malloc'd.  */
    int ac;
    char **av;
    int av_alloc;
};

/* Add an argument.  OPT is the option letter, e.g. 'a'.  ARG is the
   argument to that option, or NULL if omitted (whether NULL can actually
   happen depends on whether the option was specified as optional to
   getopt).  */
static void
arg_add (dat, opt, arg)
    struct admin_data *dat;
    int opt;
    char *arg;
{
    char *newelt = xmalloc ((arg == NULL ? 0 : strlen (arg)) + 3);
    strcpy (newelt, "-");
    newelt[1] = opt;
    if (arg == NULL)
	newelt[2] = '\0';
    else
	strcpy (newelt + 2, arg);

    if (dat->av_alloc == 0)
    {
	dat->av_alloc = 1;
	dat->av = (char **) xmalloc (dat->av_alloc * sizeof (*dat->av));
    }
    else if (dat->ac >= dat->av_alloc)
    {
	dat->av_alloc *= 2;
	dat->av = (char **) xrealloc (dat->av,
				      dat->av_alloc * sizeof (*dat->av));
    }
    dat->av[dat->ac++] = newelt;
}

int
admin (argc, argv)
    int argc;
    char **argv;
{
    int err;
#ifdef CVS_ADMIN_GROUP
    struct group *grp;
    struct group *getgrnam();
#endif
    struct admin_data admin_data;
    int c;
    int i;

    if (argc <= 1)
	usage (admin_usage);

#ifdef CVS_ADMIN_GROUP
    grp = getgrnam(CVS_ADMIN_GROUP);
     /* skip usage right check if group CVS_ADMIN_GROUP does not exist */
    if (grp != NULL)
    {
	char *me = getcaller();
	char **grnam = grp->gr_mem;
	int denied = 1;
	
	while (*grnam)
	{
	    if (strcmp(*grnam, me) == 0) 
	    {
		denied = 0;
		break;
	    }
	    grnam++;
	}

	if (denied)
	    error (1, 0, "usage is restricted to members of the group %s",
		   CVS_ADMIN_GROUP);
    }
#endif

    wrap_setup ();

    memset (&admin_data, 0, sizeof admin_data);

    /* TODO: get rid of `-' switch notation in admin_data.  For
       example, admin_data->branch should be not `-bfoo' but simply `foo'. */

    optind = 0;
    while ((c = getopt (argc, argv,
			"+ib::c:a:A:e:l::u::LUn:N:m:o:s:t::IqxV:k:")) != -1)
    {
	switch (c)
	{
	    case 'i':
		/* This has always been documented as useless in cvs.texinfo
		   and it really is--admin_fileproc silently does nothing
		   if vers->vn_user is NULL. */
		error (0, 0, "the -i option to admin is not supported");
		error (0, 0, "run add or import to create an RCS file");
		goto usage_error;

	    case 'b':
		if (admin_data.branch != NULL)
		{
		    error (0, 0, "duplicate 'b' option");
		    goto usage_error;
		}
		if (optarg == NULL)
		    admin_data.branch = xstrdup ("-b");
		else
		{
		    admin_data.branch = xmalloc (strlen (optarg) + 5);
		    strcpy (admin_data.branch, "-b");
		    strcat (admin_data.branch, optarg);
		}
		break;

	    case 'c':
		if (admin_data.comment != NULL)
		{
		    error (0, 0, "duplicate 'c' option");
		    goto usage_error;
		}
		admin_data.comment = xmalloc (strlen (optarg) + 5);
		strcpy (admin_data.comment, "-c");
		strcat (admin_data.comment, optarg);
		break;

	    case 'a':
		arg_add (&admin_data, 'a', optarg);
		break;

	    case 'A':
		/* In the client/server case, this is cheesy because
		   we just pass along the name of the RCS file, which
		   then will want to exist on the server.  This is
		   accidental; having the client specify a pathname on
		   the server is not a design feature of the protocol.  */
		arg_add (&admin_data, 'A', optarg);
		break;

	    case 'e':
		arg_add (&admin_data, 'e', optarg);
		break;

	    case 'l':
		/* Note that multiple -l options are legal.  */
		arg_add (&admin_data, 'l', optarg);
		break;

	    case 'u':
		/* Note that multiple -u options are legal.  */
		arg_add (&admin_data, 'u', optarg);
		break;

	    case 'L':
		/* Probably could also complain if -L is specified multiple
		   times, although RCS doesn't and I suppose it is reasonable
		   just to have it mean the same as a single -L.  */
		if (admin_data.set_nonstrict)
		{
		    error (0, 0, "-U and -L are incompatible");
		    goto usage_error;
		}
		admin_data.set_strict = 1;
		break;

	    case 'U':
		/* Probably could also complain if -U is specified multiple
		   times, although RCS doesn't and I suppose it is reasonable
		   just to have it mean the same as a single -U.  */
		if (admin_data.set_strict)
		{
		    error (0, 0, "-U and -L are incompatible");
		    goto usage_error;
		}
		admin_data.set_nonstrict = 1;
		break;

	    case 'n':
		/* Mostly similar to cvs tag.  Could also be parsing
		   the syntax of optarg, although for now we just pass
		   it to rcs as-is.  Note that multiple -n options are
		   legal.  */
		arg_add (&admin_data, 'n', optarg);
		break;

	    case 'N':
		/* Mostly similar to cvs tag.  Could also be parsing
		   the syntax of optarg, although for now we just pass
		   it to rcs as-is.  Note that multiple -N options are
		   legal.  */
		arg_add (&admin_data, 'N', optarg);
		break;

	    case 'm':
		/* Change log message.  Could also be parsing the syntax
		   of optarg, although for now we just pass it to rcs
		   as-is.  Note that multiple -m options are legal.  */
		arg_add (&admin_data, 'm', optarg);
		break;

	    case 'o':
		/* Delete revisions.  Probably should also be parsing the
		   syntax of optarg, so that the client can give errors
		   rather than making the server take care of that.
		   Other than that I'm not sure whether it matters much
		   whether we parse it here or in admin_fileproc.

		   Note that multiple -o options are illegal, in RCS
		   as well as here.  */

		if (admin_data.delete_revs != NULL)
		{
		    error (0, 0, "duplicate '-o' option");
		    goto usage_error;
		}
		admin_data.delete_revs = xmalloc (strlen (optarg) + 5);
		strcpy (admin_data.delete_revs, "-o");
		strcat (admin_data.delete_revs, optarg);
		break;

	    case 's':
		/* Note that multiple -s options are legal.  */
		arg_add (&admin_data, 's', optarg);
		break;

	    case 't':
		if (admin_data.desc != NULL)
		{
		    error (0, 0, "duplicate 't' option");
		    goto usage_error;
		}
		if (optarg == NULL)
		    admin_data.desc = xstrdup ("-t");
		else
		{
		    admin_data.desc = xmalloc (strlen (optarg) + 5);
		    strcpy (admin_data.desc, "-t");
		    strcat (admin_data.desc, optarg);
		}
		break;

	    case 'I':
		/* At least in RCS this can be specified several times,
		   with the same meaning as being specified once.  */
		admin_data.interactive = 1;
		break;

	    case 'q':
		admin_data.quiet = 1;
		break;

	    case 'x':
		error (0, 0, "the -x option has never done anything useful");
		error (0, 0, "RCS files in CVS always end in ,v");
		goto usage_error;

	    case 'V':
		/* No longer supported. */
		error (0, 0, "the `-V' option is obsolete");
		break;

	    case 'k':
		if (admin_data.kflag != NULL)
		{
		    error (0, 0, "duplicate '-k' option");
		    goto usage_error;
		}
		admin_data.kflag = RCS_check_kflag (optarg);
		break;
	    default:
	    case '?':
		/* getopt will have printed an error message.  */

	    usage_error:
		/* Don't use command_name; it might be "server".  */
	        error (1, 0, "specify %s -H admin for usage information",
		       program_name);
	}
    }
    argc -= optind;
    argv += optind;

    for (i = 0; i < admin_data.ac; ++i)
    {
	assert (admin_data.av[i][0] == '-');
	switch (admin_data.av[i][1])
	{
	    case 'm':
	    case 'l':
	    case 'u':
		check_numeric (&admin_data.av[i][2], argc, argv);
		break;
	    default:
		break;
	}
    }
    if (admin_data.branch != NULL)
	check_numeric (admin_data.branch + 2, argc, argv);
    if (admin_data.delete_revs != NULL)
    {
	char *p;

	check_numeric (admin_data.delete_revs + 2, argc, argv);
	p = strchr (admin_data.delete_revs + 2, ':');
	if (p != NULL && isdigit (p[1]))
	    check_numeric (p + 1, argc, argv);
	else if (p != NULL && p[1] == ':' && isdigit(p[2]))
	    check_numeric (p + 2, argc, argv);
    }

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	/* We're the client side.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	/* Note that option_with_arg does not work for us, because some
	   of the options must be sent without a space between the option
	   and its argument.  */
	if (admin_data.interactive)
	    error (1, 0, "-I option not useful with client/server");
	if (admin_data.branch != NULL)
	    send_arg (admin_data.branch);
	if (admin_data.comment != NULL)
	    send_arg (admin_data.comment);
	if (admin_data.set_strict)
	    send_arg ("-L");
	if (admin_data.set_nonstrict)
	    send_arg ("-U");
	if (admin_data.delete_revs != NULL)
	    send_arg (admin_data.delete_revs);
	if (admin_data.desc != NULL)
	    send_arg (admin_data.desc);
	if (admin_data.quiet)
	    send_arg ("-q");
	if (admin_data.kflag != NULL)
	    send_arg (admin_data.kflag);

	for (i = 0; i < admin_data.ac; ++i)
	    send_arg (admin_data.av[i]);

	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_files (argc, argv, 0, 0, SEND_NO_CONTENTS);
	send_to_server ("admin\012", 0);
        err = get_responses_and_close ();
	goto return_it;
    }
#endif /* CLIENT_SUPPORT */

    lock_tree_for_write (argc, argv, 0, 0);

    err = start_recursion (admin_fileproc, (FILESDONEPROC) NULL, admin_dirproc,
			   (DIRLEAVEPROC) NULL, (void *)&admin_data,
			   argc, argv, 0,
			   W_LOCAL, 0, 0, (char *) NULL, 1);
    Lock_Cleanup ();

 return_it:
    if (admin_data.branch != NULL)
	free (admin_data.branch);
    if (admin_data.comment != NULL)
	free (admin_data.comment);
    if (admin_data.delete_revs != NULL)
	free (admin_data.delete_revs);
    if (admin_data.kflag != NULL)
	free (admin_data.kflag);
    if (admin_data.desc != NULL)
	free (admin_data.desc);
    for (i = 0; i < admin_data.ac; ++i)
	free (admin_data.av[i]);
    if (admin_data.av != NULL)
	free (admin_data.av);

    return (err);
}

/*
 * Called to run "rcs" on a particular file.
 */
/* ARGSUSED */
static int
admin_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    struct admin_data *admin_data = (struct admin_data *) callerdat;
    Vers_TS *vers;
    char *version;
    int i;
    int status = 0;
    RCSNode *rcs, *rcs2;

    vers = Version_TS (finfo, NULL, NULL, NULL, 0, 0);

    version = vers->vn_user;
    if (version == NULL)
	goto exitfunc;
    else if (strcmp (version, "0") == 0)
    {
	error (0, 0, "cannot admin newly added file `%s'", finfo->file);
	goto exitfunc;
    }

    rcs = vers->srcfile;
    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, (FILE **) NULL, (struct rcsbuffer *) NULL);

    status = 0;

    if (!admin_data->quiet)
    {
	cvs_output ("RCS file: ", 0);
	cvs_output (rcs->path, 0);
	cvs_output ("\n", 1);
    }

    if (admin_data->branch != NULL)
    {
	char *branch = &admin_data->branch[2];
	if (*branch != '\0' && ! isdigit (*branch))
	{
	    branch = RCS_whatbranch (rcs, admin_data->branch + 2);
	    if (branch == NULL)
	    {
		error (0, 0, "%s: Symbolic name %s is undefined.",
				rcs->path, admin_data->branch + 2);
		status = 1;
	    }
	}
	if (status == 0)
	    RCS_setbranch (rcs, branch);
	if (branch != NULL && branch != &admin_data->branch[2])
	    free (branch);
    }
    if (admin_data->comment != NULL)
    {
	if (rcs->comment != NULL)
	    free (rcs->comment);
	rcs->comment = xstrdup (admin_data->comment + 2);
    }
    if (admin_data->set_strict)
	rcs->strict_locks = 1;
    if (admin_data->set_nonstrict)
	rcs->strict_locks = 0;
    if (admin_data->delete_revs != NULL)
    {
	char *s, *t, *rev1, *rev2;
	/* Set for :, clear for ::.  */
	int inclusive;
	char *t2;

	s = admin_data->delete_revs + 2;
	inclusive = 1;
	t = strchr (s, ':');
	if (t != NULL)
	{
	    if (t[1] == ':')
	    {
		inclusive = 0;
		t2 = t + 2;
	    }
	    else
		t2 = t + 1;
	}

	/* Note that we don't support '-' for ranges.  RCS considers it
	   obsolete and it is problematic with tags containing '-'.  "cvs log"
	   has made the same decision.  */

	if (t == NULL)
	{
	    /* -orev */
	    rev1 = xstrdup (s);
	    rev2 = xstrdup (s);
	}
	else if (t == s)
	{
	    /* -o:rev2 */
	    rev1 = NULL;
	    rev2 = xstrdup (t2);
	}
	else
	{
	    *t = '\0';
	    rev1 = xstrdup (s);
	    *t = ':';	/* probably unnecessary */
	    if (*t2 == '\0')
		/* -orev1: */
		rev2 = NULL;
	    else
		/* -orev1:rev2 */
		rev2 = xstrdup (t2);
	}

	if (rev1 == NULL && rev2 == NULL)
	{
	    /* RCS segfaults if `-o:' is given */
	    error (0, 0, "no valid revisions specified in `%s' option",
		   admin_data->delete_revs);
	    status = 1;
	}
	else
	{
	    status |= RCS_delete_revs (rcs, rev1, rev2, inclusive);
	    if (rev1)
		free (rev1);
	    if (rev2)
		free (rev2);
	}
    }
    if (admin_data->desc != NULL)
    {
	free (rcs->desc);
	rcs->desc = NULL;
	if (admin_data->desc[2] == '-')
	    rcs->desc = xstrdup (admin_data->desc + 3);
	else
	{
	    char *descfile = admin_data->desc + 2;
	    size_t bufsize = 0;
	    size_t len;

	    /* If -t specified with no argument, read from stdin. */
	    if (*descfile == '\0')
		descfile = NULL;
	    get_file (descfile, descfile, "r", &rcs->desc, &bufsize, &len);
	}
    }
    if (admin_data->kflag != NULL)
    {
	char *kflag = admin_data->kflag + 2;
	if (!rcs->expand || strcmp (rcs->expand, kflag) != 0)
	{
	    if (rcs->expand)
		free (rcs->expand);
	    rcs->expand = xstrdup (kflag);
	}
    }

    /* Handle miscellaneous options.  TODO: decide whether any or all
       of these should have their own fields in the admin_data
       structure. */
    for (i = 0; i < admin_data->ac; ++i)
    {
	char *arg;
	char *p, *rev, *revnum, *tag, *msg;
	char **users;
	int argc, u;
	Node *n;
	RCSVers *delta;
	
	arg = admin_data->av[i];
	switch (arg[1])
	{
	    case 'a': /* fall through */
	    case 'e':
	        line2argv (&argc, &users, arg + 2, " ,\t\n");
		if (arg[1] == 'a')
		    for (u = 0; u < argc; ++u)
			RCS_addaccess (rcs, users[u]);
		else
		    for (u = 0; u < argc; ++u)
			RCS_delaccess (rcs, users[u]);
		free_names (&argc, users);
		break;
	    case 'A':

		/* See admin-19a-admin and friends in sanity.sh for
		   relative pathnames.  It makes sense to think in
		   terms of a syntax which give pathnames relative to
		   the repository or repository corresponding to the
		   current directory or some such (and perhaps don't
		   include ,v), but trying to worry about such things
		   is a little pointless unless you first worry about
		   whether "cvs admin -A" as a whole makes any sense
		   (currently probably not, as access lists don't
		   affect the behavior of CVS).  */

		rcs2 = RCS_parsercsfile (arg + 2);
		if (rcs2 == NULL)
		    error (1, 0, "cannot continue");

		p = xstrdup (RCS_getaccess (rcs2));
	        line2argv (&argc, &users, p, " \t\n");
		free (p);
		freercsnode (&rcs2);

		for (u = 0; u < argc; ++u)
		    RCS_addaccess (rcs, users[u]);
		free_names (&argc, users);
		break;
	    case 'n': /* fall through */
	    case 'N':
		if (arg[2] == '\0')
		{
		    cvs_outerr ("missing symbolic name after ", 0);
		    cvs_outerr (arg, 0);
		    cvs_outerr ("\n", 1);
		    break;
		}
		p = strchr (arg, ':');
		if (p == NULL)
		{
		    if (RCS_deltag (rcs, arg + 2) != 0)
		    {
			error (0, 0, "%s: Symbolic name %s is undefined.",
			       rcs->path, 
			       arg + 2);
			status = 1;
			continue;
		    }
		    break;
		}
		*p = '\0';
		tag = xstrdup (arg + 2);
		*p++ = ':';

		/* Option `n' signals an error if this tag is already bound. */
		if (arg[1] == 'n')
		{
		    n = findnode (RCS_symbols (rcs), tag);
		    if (n != NULL)
		    {
			error (0, 0,
			       "%s: symbolic name %s already bound to %s",
			       rcs->path,
			       tag, n->data);
			status = 1;
			continue;
		    }
		}

		/* Expand rev if necessary. */
		rev = RCS_gettag (rcs, p, 0, NULL);
		RCS_settag (rcs, tag, rev);
		if (rev != NULL)
		    free (rev);
		free (tag);
		break;
	    case 's':
	        p = strchr (arg, ':');
		if (p == NULL)
		{
		    tag = xstrdup (arg + 2);
		    rev = RCS_head (rcs);
		}
		else
		{
		    *p = '\0';
		    tag = xstrdup (arg + 2);
		    *p++ = ':';
		    rev = xstrdup (p);
		}
		revnum = RCS_gettag (rcs, rev, 0, NULL);
		free (rev);
		if (revnum != NULL)
		    n = findnode (rcs->versions, revnum);
		if (revnum == NULL || n == NULL)
		{
		    error (0, 0,
			   "%s: can't set state of nonexisting revision %s",
			   rcs->path,
			   rev);
		    if (revnum != NULL)
			free (revnum);
		    status = 1;
		    continue;
		}
		delta = (RCSVers *) n->data;
		free (delta->state);
		delta->state = tag;
		break;

	    case 'm':
	        p = strchr (arg, ':');
		if (p == NULL)
		{
		    error (0, 0, "%s: -m option lacks revision number",
			   rcs->path);
		    status = 1;
		    continue;
		}
		*p = '\0';
		rev = RCS_gettag (rcs, arg + 2, 0, NULL);
		if (rev == NULL)
		{
		    error (0, 0, "%s: no such revision %s", rcs->path, rev);
		    status = 1;
		    continue;
		}
		*p++ = ':';
		msg = p;

		n = findnode (rcs->versions, rev);
		delta = (RCSVers *) n->data;
		if (delta->text == NULL)
		{
		    delta->text = (Deltatext *) xmalloc (sizeof (Deltatext));
		    memset ((void *) delta->text, 0, sizeof (Deltatext));
		}
		delta->text->version = xstrdup (delta->version);
		delta->text->log = make_message_rcslegal (msg);
		break;

	    case 'l':
	        status |= RCS_lock (rcs, arg[2] ? arg + 2 : NULL, 0);
		break;
	    case 'u':
		status |= RCS_unlock (rcs, arg[2] ? arg + 2 : NULL, 0);
		break;
	    default: assert(0);	/* can't happen */
	}
    }

    /* TODO: reconcile the weird discrepancies between
       admin_data->quiet and quiet. */
    if (status == 0)
    {
	RCS_rewrite (rcs, NULL, NULL);
	if (!admin_data->quiet)
	    cvs_output ("done\n", 5);
    }
    else
    {
	/* Note that this message should only occur after another
	   message has given a more specific error.  The point of this
	   additional message is to make it clear that the previous problems
	   caused CVS to forget about the idea of modifying the RCS file.  */
	error (0, 0, "cannot modify RCS file for `%s'", finfo->file);

	/* Upon failure, we want to abandon any changes made to the
	   RCS data structure.  Forcing a reparse does the trick,
	   but leaks memory and is kludgey.  Should we export
	   free_rcsnode_contents for this purpose? */
	RCS_reparsercsfile (rcs, (FILE **) NULL, (struct rcsbuffer *) NULL);
    }

  exitfunc:
    freevers_ts (&vers);
    return status;
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
admin_dirproc (callerdat, dir, repos, update_dir, entries)
    void *callerdat;
    char *dir;
    char *repos;
    char *update_dir;
    List *entries;
{
    if (!quiet)
	error (0, 0, "Administrating %s", update_dir);
    return (R_PROCESS);
}
