/* Implementation for "cvs edit", "cvs watch on", and related commands

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"
#include "getline.h"
#include "watch.h"
#include "edit.h"
#include "fileattr.h"

static int watch_onoff PROTO ((int, char **));

static int setting_default;
static int turning_on;

static int setting_tedit;
static int setting_tunedit;
static int setting_tcommit;

static int onoff_fileproc PROTO ((void *callerdat, struct file_info *finfo));

static int
onoff_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    fileattr_set (finfo->file, "_watched", turning_on ? "" : NULL);
    return 0;
}

static int onoff_filesdoneproc PROTO ((void *, int, char *, char *, List *));

static int
onoff_filesdoneproc (callerdat, err, repository, update_dir, entries)
    void *callerdat;
    int err;
    char *repository;
    char *update_dir;
    List *entries;
{
    if (setting_default)
	fileattr_set (NULL, "_watched", turning_on ? "" : NULL);
    return err;
}

static int
watch_onoff (argc, argv)
    int argc;
    char **argv;
{
    int c;
    int local = 0;
    int err;

    optind = 0;
    while ((c = getopt (argc, argv, "+lR")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case '?':
	    default:
		usage (watch_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote)
    {
	start_server ();

	ign_setup ();

	if (local)
	    send_arg ("-l");
	send_arg ("--");
	send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_to_server (turning_on ? "watch-on\012" : "watch-off\012", 0);
	return get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    setting_default = (argc <= 0);

    lock_tree_for_write (argc, argv, local, W_LOCAL, 0);

    err = start_recursion (onoff_fileproc, onoff_filesdoneproc,
			   (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv, local, W_LOCAL, 0, LOCK_NONE,
			   (char *)NULL, 0);

    Lock_Cleanup ();
    return err;
}

int
watch_on (argc, argv)
    int argc;
    char **argv;
{
    turning_on = 1;
    return watch_onoff (argc, argv);
}

int
watch_off (argc, argv)
    int argc;
    char **argv;
{
    turning_on = 0;
    return watch_onoff (argc, argv);
}

static int dummy_fileproc PROTO ((void *callerdat, struct file_info *finfo));

static int
dummy_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    /* This is a pretty hideous hack, but the gist of it is that recurse.c
       won't call notify_check unless there is a fileproc, so we can't just
       pass NULL for fileproc.  */
    return 0;
}

static int ncheck_fileproc PROTO ((void *callerdat, struct file_info *finfo));

/* Check for and process notifications.  Local only.  I think that doing
   this as a fileproc is the only way to catch all the
   cases (e.g. foo/bar.c), even though that means checking over and over
   for the same CVSADM_NOTIFY file which we removed the first time we
   processed the directory.  */

static int
ncheck_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    int notif_type;
    char *filename;
    char *val;
    char *cp;
    char *watches;

    FILE *fp;
    char *line = NULL;
    size_t line_len = 0;

    /* We send notifications even if noexec.  I'm not sure which behavior
       is most sensible.  */

    fp = CVS_FOPEN (CVSADM_NOTIFY, "r");
    if (fp == NULL)
    {
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", CVSADM_NOTIFY);
	return 0;
    }

    while (getline (&line, &line_len, fp) > 0)
    {
	notif_type = line[0];
	if (notif_type == '\0')
	    continue;
	filename = line + 1;
	cp = strchr (filename, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '\0';
	val = cp;
	cp = strchr (val, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '+';
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '+';
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '\0';
	watches = cp;
	cp = strchr (cp, '\n');
	if (cp == NULL)
	    continue;
	*cp = '\0';

	notify_do (notif_type, filename, getcaller (), val, watches,
		   finfo->repository);
    }
    free (line);

    if (ferror (fp))
	error (0, errno, "cannot read %s", CVSADM_NOTIFY);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", CVSADM_NOTIFY);

    if ( CVS_UNLINK (CVSADM_NOTIFY) < 0)
	error (0, errno, "cannot remove %s", CVSADM_NOTIFY);

    return 0;
}

static int send_notifications PROTO ((int, char **, int));

/* Look through the CVSADM_NOTIFY file and process each item there
   accordingly.  */
static int
send_notifications (argc, argv, local)
    int argc;
    char **argv;
    int local;
{
    int err = 0;

#ifdef CLIENT_SUPPORT
    /* OK, we've done everything which needs to happen on the client side.
       Now we can try to contact the server; if we fail, then the
       notifications stay in CVSADM_NOTIFY to be sent next time.  */
    if (current_parsed_root->isremote)
    {
	if (strcmp (command_name, "release") != 0)
	{
	    start_server ();
	    ign_setup ();
	}

	err += start_recursion (dummy_fileproc, (FILESDONEPROC) NULL,
				(DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
				argc, argv, local, W_LOCAL, 0, 0, (char *)NULL,
				0);

	send_to_server ("noop\012", 0);
	if (strcmp (command_name, "release") == 0)
	    err += get_server_responses ();
	else
	    err += get_responses_and_close ();
    }
    else
#endif
    {
	/* Local.  */

	lock_tree_for_write (argc, argv, local, W_LOCAL, 0);
	err += start_recursion (ncheck_fileproc, (FILESDONEPROC) NULL,
				(DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
				argc, argv, local, W_LOCAL, 0, 0, (char *)NULL,
				0);
	Lock_Cleanup ();
    }
    return err;
}

static int edit_fileproc PROTO ((void *callerdat, struct file_info *finfo));

static int
edit_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    FILE *fp;
    time_t now;
    char *ascnow;
    char *basefilename;

    if (noexec)
	return 0;

    /* This is a somewhat screwy way to check for this, because it
       doesn't help errors other than the nonexistence of the file
       (e.g. permissions problems).  It might be better to rearrange
       the code so that CVSADM_NOTIFY gets written only after the
       various actions succeed (but what if only some of them
       succeed).  */
    if (!isfile (finfo->file))
    {
	error (0, 0, "no such file %s; ignored", finfo->fullname);
	return 0;
    }

    fp = open_file (CVSADM_NOTIFY, "a");

    (void) time (&now);
    ascnow = asctime (gmtime (&now));
    ascnow[24] = '\0';
    /* Fix non-standard format.  */
    if (ascnow[8] == '0') ascnow[8] = ' ';
    fprintf (fp, "E%s\t%s GMT\t%s\t%s\t", finfo->file,
	     ascnow, hostname, CurDir);
    if (setting_tedit)
	fprintf (fp, "E");
    if (setting_tunedit)
	fprintf (fp, "U");
    if (setting_tcommit)
	fprintf (fp, "C");
    fprintf (fp, "\n");

    if (fclose (fp) < 0)
    {
	if (finfo->update_dir[0] == '\0')
	    error (0, errno, "cannot close %s", CVSADM_NOTIFY);
	else
	    error (0, errno, "cannot close %s/%s", finfo->update_dir,
		   CVSADM_NOTIFY);
    }

    xchmod (finfo->file, 1);

    /* Now stash the file away in CVSADM so that unedit can revert even if
       it can't communicate with the server.  We stash away a writable
       copy so that if the user removes the working file, then restores it
       with "cvs update" (which clears _editors but does not update
       CVSADM_BASE), then a future "cvs edit" can still win.  */
    /* Could save a system call by only calling mkdir_if_needed if
       trying to create the output file fails.  But copy_file isn't
       set up to facilitate that.  */
    mkdir_if_needed (CVSADM_BASE);
    basefilename = xmalloc (10 + sizeof CVSADM_BASE + strlen (finfo->file));
    strcpy (basefilename, CVSADM_BASE);
    strcat (basefilename, "/");
    strcat (basefilename, finfo->file);
    copy_file (finfo->file, basefilename);
    free (basefilename);

    {
	Node *node;

	node = findnode_fn (finfo->entries, finfo->file);
	if (node != NULL)
	    base_register (finfo, ((Entnode *) node->data)->version);
    }

    return 0;
}

static const char *const edit_usage[] =
{
    "Usage: %s %s [-lR] [files...]\n",
    "-l: Local directory only, not recursive\n",
    "-R: Process directories recursively\n",
    "-a: Specify what actions for temporary watch, one of\n",
    "    edit,unedit,commit,all,none\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
edit (argc, argv)
    int argc;
    char **argv;
{
    int local = 0;
    int c;
    int err;
    int a_omitted;

    if (argc == -1)
	usage (edit_usage);

    a_omitted = 1;
    setting_tedit = 0;
    setting_tunedit = 0;
    setting_tcommit = 0;
    optind = 0;
    while ((c = getopt (argc, argv, "+lRa:")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'a':
		a_omitted = 0;
		if (strcmp (optarg, "edit") == 0)
		    setting_tedit = 1;
		else if (strcmp (optarg, "unedit") == 0)
		    setting_tunedit = 1;
		else if (strcmp (optarg, "commit") == 0)
		    setting_tcommit = 1;
		else if (strcmp (optarg, "all") == 0)
		{
		    setting_tedit = 1;
		    setting_tunedit = 1;
		    setting_tcommit = 1;
		}
		else if (strcmp (optarg, "none") == 0)
		{
		    setting_tedit = 0;
		    setting_tunedit = 0;
		    setting_tcommit = 0;
		}
		else
		    usage (edit_usage);
		break;
	    case '?':
	    default:
		usage (edit_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (a_omitted)
    {
	setting_tedit = 1;
	setting_tunedit = 1;
	setting_tcommit = 1;
    }

    if (strpbrk (hostname, "+,>;=\t\n") != NULL)
	error (1, 0,
	       "host name (%s) contains an invalid character (+,>;=\\t\\n)",
	       hostname);
    if (strpbrk (CurDir, "+,>;=\t\n") != NULL)
	error (1, 0,
"current directory (%s) contains an invalid character (+,>;=\\t\\n)",
	       CurDir);

    /* No need to readlock since we aren't doing anything to the
       repository.  */
    err = start_recursion (edit_fileproc, (FILESDONEPROC) NULL,
			   (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv, local, W_LOCAL, 0, 0, (char *)NULL,
			   0);

    err += send_notifications (argc, argv, local);

    return err;
}

static int unedit_fileproc PROTO ((void *callerdat, struct file_info *finfo));

static int
unedit_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    FILE *fp;
    time_t now;
    char *ascnow;
    char *basefilename;

    if (noexec)
	return 0;

    basefilename = xmalloc (10 + sizeof CVSADM_BASE + strlen (finfo->file));
    strcpy (basefilename, CVSADM_BASE);
    strcat (basefilename, "/");
    strcat (basefilename, finfo->file);
    if (!isfile (basefilename))
    {
	/* This file apparently was never cvs edit'd (e.g. we are uneditting
	   a directory where only some of the files were cvs edit'd.  */
	free (basefilename);
	return 0;
    }

    if (xcmp (finfo->file, basefilename) != 0)
    {
	printf ("%s has been modified; revert changes? ", finfo->fullname);
	if (!yesno ())
	{
	    /* "no".  */
	    free (basefilename);
	    return 0;
	}
    }
    rename_file (basefilename, finfo->file);
    free (basefilename);

    fp = open_file (CVSADM_NOTIFY, "a");

    (void) time (&now);
    ascnow = asctime (gmtime (&now));
    ascnow[24] = '\0';
    /* Fix non-standard format.  */
    if (ascnow[8] == '0') ascnow[8] = ' ';
    fprintf (fp, "U%s\t%s GMT\t%s\t%s\t\n", finfo->file,
	     ascnow, hostname, CurDir);

    if (fclose (fp) < 0)
    {
	if (finfo->update_dir[0] == '\0')
	    error (0, errno, "cannot close %s", CVSADM_NOTIFY);
	else
	    error (0, errno, "cannot close %s/%s", finfo->update_dir,
		   CVSADM_NOTIFY);
    }

    /* Now update the revision number in CVS/Entries from CVS/Baserev.
       The basic idea here is that we are reverting to the revision
       that the user edited.  If we wanted "cvs update" to update
       CVS/Base as we go along (so that an unedit could revert to the
       current repository revision), we would need:

       update (or all send_files?) (client) needs to send revision in
       new Entry-base request.  update (server/local) needs to check
       revision against repository and send new Update-base response
       (like Update-existing in that the file already exists.  While
       we are at it, might try to clean up the syntax by having the
       mode only in a "Mode" response, not in the Update-base itself).  */
    {
	char *baserev;
	Node *node;
	Entnode *entdata;

	baserev = base_get (finfo);
	node = findnode_fn (finfo->entries, finfo->file);
	/* The case where node is NULL probably should be an error or
	   something, but I don't want to think about it too hard right
	   now.  */
	if (node != NULL)
	{
	    entdata = (Entnode *) node->data;
	    if (baserev == NULL)
	    {
		/* This can only happen if the CVS/Baserev file got
		   corrupted.  We suspect it might be possible if the
		   user interrupts CVS, although I haven't verified
		   that.  */
		error (0, 0, "%s not mentioned in %s", finfo->fullname,
		       CVSADM_BASEREV);

		/* Since we don't know what revision the file derives from,
		   keeping it around would be asking for trouble.  */
		if (unlink_file (finfo->file) < 0)
		    error (0, errno, "cannot remove %s", finfo->fullname);

		/* This is cheesy, in a sense; why shouldn't we do the
		   update for the user?  However, doing that would require
		   contacting the server, so maybe this is OK.  */
		error (0, 0, "run update to complete the unedit");
		return 0;
	    }
	    Register (finfo->entries, finfo->file, baserev, entdata->timestamp,
		      entdata->options, entdata->tag, entdata->date,
		      entdata->conflict);
	}
	free (baserev);
	base_deregister (finfo);
    }

    xchmod (finfo->file, 0);
    return 0;
}

static const char *const unedit_usage[] =
{
    "Usage: %s %s [-lR] [files...]\n",
    "-l: Local directory only, not recursive\n",
    "-R: Process directories recursively\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
unedit (argc, argv)
    int argc;
    char **argv;
{
    int local = 0;
    int c;
    int err;

    if (argc == -1)
	usage (unedit_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+lR")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case '?':
	    default:
		usage (unedit_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* No need to readlock since we aren't doing anything to the
       repository.  */
    err = start_recursion (unedit_fileproc, (FILESDONEPROC) NULL,
			   (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv, local, W_LOCAL, 0, 0, (char *)NULL,
			   0);

    err += send_notifications (argc, argv, local);

    return err;
}

void
mark_up_to_date (file)
    char *file;
{
    char *base;

    /* The file is up to date, so we better get rid of an out of
       date file in CVSADM_BASE.  */
    base = xmalloc (strlen (file) + 80);
    strcpy (base, CVSADM_BASE);
    strcat (base, "/");
    strcat (base, file);
    if (unlink_file (base) < 0 && ! existence_error (errno))
	error (0, errno, "cannot remove %s", file);
    free (base);
}


void
editor_set (filename, editor, val)
    char *filename;
    char *editor;
    char *val;
{
    char *edlist;
    char *newlist;

    edlist = fileattr_get0 (filename, "_editors");
    newlist = fileattr_modify (edlist, editor, val, '>', ',');
    /* If the attributes is unchanged, don't rewrite the attribute file.  */
    if (!((edlist == NULL && newlist == NULL)
	  || (edlist != NULL
	      && newlist != NULL
	      && strcmp (edlist, newlist) == 0)))
	fileattr_set (filename, "_editors", newlist);
    if (edlist != NULL)
	free (edlist);
    if (newlist != NULL)
	free (newlist);
}

struct notify_proc_args {
    /* What kind of notification, "edit", "tedit", etc.  */
    char *type;
    /* User who is running the command which causes notification.  */
    char *who;
    /* User to be notified.  */
    char *notifyee;
    /* File.  */
    char *file;
};

/* Pass as a static until we get around to fixing Parse_Info to pass along
   a void * where we can stash it.  */
static struct notify_proc_args *notify_args;

static int notify_proc PROTO ((char *repository, char *filter));

static int
notify_proc (repository, filter)
    char *repository;
    char *filter;
{
    FILE *pipefp;
    char *prog;
    char *expanded_prog;
    char *p;
    char *q;
    char *srepos;
    struct notify_proc_args *args = notify_args;

    srepos = Short_Repository (repository);
    prog = xmalloc (strlen (filter) + strlen (args->notifyee) + 1);
    /* Copy FILTER to PROG, replacing the first occurrence of %s with
       the notifyee.  We only allocated enough memory for one %s, and I doubt
       there is a need for more.  */
    for (p = filter, q = prog; *p != '\0'; ++p)
    {
	if (p[0] == '%')
	{
	    if (p[1] == 's')
	    {
		strcpy (q, args->notifyee);
		q += strlen (q);
		strcpy (q, p + 2);
		q += strlen (q);
		break;
	    }
	    else
		continue;
	}
	*q++ = *p;
    }
    *q = '\0';

    /* FIXME: why are we calling expand_proc?  Didn't we already
       expand it in Parse_Info, before passing it to notify_proc?  */
    expanded_prog = expand_path (prog, "notify", 0);
    if (!expanded_prog)
    {
	free (prog);
	return 1;
    }

    pipefp = run_popen (expanded_prog, "w");
    if (pipefp == NULL)
    {
	error (0, errno, "cannot write entry to notify filter: %s", prog);
	free (prog);
	free (expanded_prog);
	return 1;
    }

    fprintf (pipefp, "%s %s\n---\n", srepos, args->file);
    fprintf (pipefp, "Triggered %s watch on %s\n", args->type, repository);
    fprintf (pipefp, "By %s\n", args->who);

    /* Lots more potentially useful information we could add here; see
       logfile_write for inspiration.  */

    free (prog);
    free (expanded_prog);
    return (pclose (pipefp));
}

/* FIXME: this function should have a way to report whether there was
   an error so that server.c can know whether to report Notified back
   to the client.  */
void
notify_do (type, filename, who, val, watches, repository)
    int type;
    char *filename;
    char *who;
    char *val;
    char *watches;
    char *repository;
{
    static struct addremove_args blank;
    struct addremove_args args;
    char *watchers;
    char *p;
    char *endp;
    char *nextp;

    /* Initialize fields to 0, NULL, or 0.0.  */
    args = blank;
    switch (type)
    {
	case 'E':
	    if (strpbrk (val, ",>;=\n") != NULL)
	    {
		error (0, 0, "invalid character in editor value");
		return;
	    }
	    editor_set (filename, who, val);
	    break;
	case 'U':
	case 'C':
	    editor_set (filename, who, NULL);
	    break;
	default:
	    return;
    }

    watchers = fileattr_get0 (filename, "_watchers");
    p = watchers;
    while (p != NULL)
    {
	char *q;
	char *endq;
	char *nextq;
	char *notif;

	endp = strchr (p, '>');
	if (endp == NULL)
	    break;
	nextp = strchr (p, ',');

	if ((size_t)(endp - p) == strlen (who) && strncmp (who, p, endp - p) == 0)
	{
	    /* Don't notify user of their own changes.  Would perhaps
	       be better to check whether it is the same working
	       directory, not the same user, but that is hairy.  */
	    p = nextp == NULL ? nextp : nextp + 1;
	    continue;
	}

	/* Now we point q at a string which looks like
	   "edit+unedit+commit,"... and walk down it.  */
	q = endp + 1;
	notif = NULL;
	while (q != NULL)
	{
	    endq = strchr (q, '+');
	    if (endq == NULL || (nextp != NULL && endq > nextp))
	    {
		if (nextp == NULL)
		    endq = q + strlen (q);
		else
		    endq = nextp;
		nextq = NULL;
	    }
	    else
		nextq = endq + 1;

	    /* If there is a temporary and a regular watch, send a single
	       notification, for the regular watch.  */
	    if (type == 'E' && endq - q == 4 && strncmp ("edit", q, 4) == 0)
	    {
		notif = "edit";
	    }
	    else if (type == 'U'
		     && endq - q == 6 && strncmp ("unedit", q, 6) == 0)
	    {
		notif = "unedit";
	    }
	    else if (type == 'C'
		     && endq - q == 6 && strncmp ("commit", q, 6) == 0)
	    {
		notif = "commit";
	    }
	    else if (type == 'E'
		     && endq - q == 5 && strncmp ("tedit", q, 5) == 0)
	    {
		if (notif == NULL)
		    notif = "temporary edit";
	    }
	    else if (type == 'U'
		     && endq - q == 7 && strncmp ("tunedit", q, 7) == 0)
	    {
		if (notif == NULL)
		    notif = "temporary unedit";
	    }
	    else if (type == 'C'
		     && endq - q == 7 && strncmp ("tcommit", q, 7) == 0)
	    {
		if (notif == NULL)
		    notif = "temporary commit";
	    }
	    q = nextq;
	}
	if (nextp != NULL)
	    ++nextp;

	if (notif != NULL)
	{
	    struct notify_proc_args args;
	    size_t len = endp - p;
	    FILE *fp;
	    char *usersname;
	    char *line = NULL;
	    size_t line_len = 0;

	    args.notifyee = NULL;
	    usersname = xmalloc (strlen (current_parsed_root->directory)
				 + sizeof CVSROOTADM
				 + sizeof CVSROOTADM_USERS
				 + 20);
	    strcpy (usersname, current_parsed_root->directory);
	    strcat (usersname, "/");
	    strcat (usersname, CVSROOTADM);
	    strcat (usersname, "/");
	    strcat (usersname, CVSROOTADM_USERS);
	    fp = CVS_FOPEN (usersname, "r");
	    if (fp == NULL && !existence_error (errno))
		error (0, errno, "cannot read %s", usersname);
	    if (fp != NULL)
	    {
		while (getline (&line, &line_len, fp) >= 0)
		{
		    if (strncmp (line, p, len) == 0
			&& line[len] == ':')
		    {
			char *cp;
			args.notifyee = xstrdup (line + len + 1);

                        /* There may or may not be more
                           colon-separated fields added to this in the
                           future; in any case, we ignore them right
                           now, and if there are none we make sure to
                           chop off the final newline, if any. */
			cp = strpbrk (args.notifyee, ":\n");

			if (cp != NULL)
			    *cp = '\0';
			break;
		    }
		}
		if (ferror (fp))
		    error (0, errno, "cannot read %s", usersname);
		if (fclose (fp) < 0)
		    error (0, errno, "cannot close %s", usersname);
	    }
	    free (usersname);
	    if (line != NULL)
		free (line);

	    if (args.notifyee == NULL)
	    {
		args.notifyee = xmalloc (endp - p + 1);
		strncpy (args.notifyee, p, endp - p);
		args.notifyee[endp - p] = '\0';
	    }

	    notify_args = &args;
	    args.type = notif;
	    args.who = who;
	    args.file = filename;

	    (void) Parse_Info (CVSROOTADM_NOTIFY, repository, notify_proc, 1);
	    free (args.notifyee);
	}

	p = nextp;
    }
    if (watchers != NULL)
	free (watchers);

    switch (type)
    {
	case 'E':
	    if (*watches == 'E')
	    {
		args.add_tedit = 1;
		++watches;
	    }
	    if (*watches == 'U')
	    {
		args.add_tunedit = 1;
		++watches;
	    }
	    if (*watches == 'C')
	    {
		args.add_tcommit = 1;
	    }
	    watch_modify_watchers (filename, &args);
	    break;
	case 'U':
	case 'C':
	    args.remove_temp = 1;
	    watch_modify_watchers (filename, &args);
	    break;
    }
}

#ifdef CLIENT_SUPPORT
/* Check and send notifications.  This is only for the client.  */
void
notify_check (repository, update_dir)
    char *repository;
    char *update_dir;
{
    FILE *fp;
    char *line = NULL;
    size_t line_len = 0;

    if (! server_started)
	/* We are in the midst of a command which is not to talk to
	   the server (e.g. the first phase of a cvs edit).  Just chill
	   out, we'll catch the notifications on the flip side.  */
	return;

    /* We send notifications even if noexec.  I'm not sure which behavior
       is most sensible.  */

    fp = CVS_FOPEN (CVSADM_NOTIFY, "r");
    if (fp == NULL)
    {
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", CVSADM_NOTIFY);
	return;
    }
    while (getline (&line, &line_len, fp) > 0)
    {
	int notif_type;
	char *filename;
	char *val;
	char *cp;

	notif_type = line[0];
	if (notif_type == '\0')
	    continue;
	filename = line + 1;
	cp = strchr (filename, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '\0';
	val = cp;

	client_notify (repository, update_dir, filename, notif_type, val);
    }
    if (line)
	free (line);
    if (ferror (fp))
	error (0, errno, "cannot read %s", CVSADM_NOTIFY);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", CVSADM_NOTIFY);

    /* Leave the CVSADM_NOTIFY file there, until the server tells us it
       has dealt with it.  */
}
#endif /* CLIENT_SUPPORT */


static const char *const editors_usage[] =
{
    "Usage: %s %s [-lR] [files...]\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static int editors_fileproc PROTO ((void *callerdat, struct file_info *finfo));

static int
editors_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    char *them;
    char *p;

    them = fileattr_get0 (finfo->file, "_editors");
    if (them == NULL)
	return 0;

    cvs_output (finfo->fullname, 0);

    p = them;
    while (1)
    {
	cvs_output ("\t", 1);
	while (*p != '>' && *p != '\0')
	    cvs_output (p++, 1);
	if (*p == '\0')
	{
	    /* Only happens if attribute is misformed.  */
	    cvs_output ("\n", 1);
	    break;
	}
	++p;
	cvs_output ("\t", 1);
	while (1)
	{
	    while (*p != '+' && *p != ',' && *p != '\0')
		cvs_output (p++, 1);
	    if (*p == '\0')
	    {
		cvs_output ("\n", 1);
		goto out;
	    }
	    if (*p == ',')
	    {
		++p;
		break;
	    }
	    ++p;
	    cvs_output ("\t", 1);
	}
	cvs_output ("\n", 1);
    }
  out:;
    free (them);
    return 0;
}

int
editors (argc, argv)
    int argc;
    char **argv;
{
    int local = 0;
    int c;

    if (argc == -1)
	usage (editors_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+lR")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case '?':
	    default:
		usage (editors_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote)
    {
	start_server ();
	ign_setup ();

	if (local)
	    send_arg ("-l");
	send_arg ("--");
	send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_to_server ("editors\012", 0);
	return get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    return start_recursion (editors_fileproc, (FILESDONEPROC) NULL,
			    (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			    argc, argv, local, W_LOCAL, 0, 1, (char *)NULL,
			    0);
}
