/* commands.c: vinum interface program, main commands */
/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Written by Greg Lehey
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: commands.c,v 1.14 2000/11/14 20:01:23 grog Exp grog $
 * $FreeBSD$
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <netdb.h>
#include <paths.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "vext.h"
#include <sys/types.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/wait.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <devstat.h>

static void dorename(struct vinum_rename_msg *msg, const char *oldname, const char *name, int maxlen);

void
vinum_create(int argc, char *argv[], char *arg0[])
{
    int error;
    FILE *dfd;						    /* file descriptor for the config file */
    char buffer[BUFSIZE];				    /* read config file in here */
    char commandline[BUFSIZE];				    /* issue command from here */
    struct _ioctl_reply *reply;
    int ioctltype;					    /* for ioctl call */
    char tempfile[PATH_MAX];				    /* name of temp file for direct editing */
    char *file;						    /* file to read */
    FILE *tf;						    /* temp file */

    if (argc == 0) {					    /* no args, */
	char *editor;					    /* editor to start */
	int status;

	editor = getenv("EDITOR");
	if (editor == NULL)
	    editor = "/usr/bin/vi";
	sprintf(tempfile, "/var/tmp/" VINUMMOD ".create.%d", getpid());	/* create a temp file */
	tf = fopen(tempfile, "w");			    /* open it */
	if (tf == NULL) {
	    fprintf(stderr, "Can't open %s: %s\n", argv[0], strerror(errno));
	    return;
	}
	printconfig(tf, "# ");				    /* and put the current config it */
	fclose(tf);
	sprintf(commandline, "%s %s", editor, tempfile);    /* create an edit command */
	status = system(commandline);			    /* do it */
	if (status != 0) {
	    fprintf(stderr, "Can't edit config: status %d\n", status);
	    return;
	}
	file = tempfile;
    } else if (argc == 1)
	file = argv[0];
    else {
	fprintf(stderr, "Expecting 1 parameter, not %d\n", argc);
	return;
    }
    reply = (struct _ioctl_reply *) &buffer;
    dfd = fopen(file, "r");
    if (dfd == NULL) {					    /* no go */
	fprintf(stderr, "Can't open %s: %s\n", file, strerror(errno));
	return;
    }
    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) {	    /* can't get config? */
	printf("Can't configure: %s (%d)\n", strerror(errno), errno);
	return;
    }
    file_line = 0;					    /* start with line 1 */
    /* Parse the configuration, and add it to the global configuration */
    for (;;) {						    /* love this style(9) */
	char *configline;

	configline = fgets(buffer, BUFSIZE, dfd);
	if (history)
	    fprintf(history, "%s", buffer);

	if (configline == NULL) {
	    if (ferror(dfd))
		perror("Can't read config file");
	    break;
	}
	file_line++;					    /* count the lines */
	if (vflag)
	    printf("%4d: %s", file_line, buffer);
	strcpy(commandline, buffer);			    /* make a copy */
	ioctl(superdev, VINUM_CREATE, buffer);
	if (reply->error != 0) {			    /* error in config */
	    if (!vflag)					    /* print this line anyway */
		printf("%4d: %s", file_line, commandline);
	    fprintf(stdout, "** %d %s: %s\n",
		file_line,
		reply->msg,
		strerror(reply->error));

	    /*
	     * XXX at the moment, we reset the config
	     * lock on error, so try to get it again.
	     * If we fail, don't cry again.
	     */
	    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) /* can't get config? */
		return;
	}
    }
    fclose(dfd);					    /* done with the config file */
    ioctltype = 0;					    /* saveconfig after update */
    error = ioctl(superdev, VINUM_SAVECONFIG, &ioctltype);  /* save the config to disk */
    if (error != 0)
	perror("Can't save Vinum config");
    if (no_devfs)
	make_devices();
    listconfig();
    checkupdates();					    /* make sure we're updating */
}

/* Read vinum config from a disk */
void
vinum_read(int argc, char *argv[], char *arg0[])
{
    int error;
    char buffer[BUFSIZE];				    /* read config file in here */
    struct _ioctl_reply *reply;
    int i;

    reply = (struct _ioctl_reply *) &buffer;
    if (argc < 1) {					    /* wrong arg count */
	fprintf(stderr, "Usage: read drive [drive ...]\n");
	return;
    }
    strcpy(buffer, "read ");
    for (i = 0; i < argc; i++) {			    /* each drive name */
	strcat(buffer, argv[i]);
	strcat(buffer, " ");
    }

    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) {	    /* can't get config? */
	fprintf(stderr, "Can't configure: %s (%d)\n", strerror(errno), errno);
	return;
    }
    ioctl(superdev, VINUM_CREATE, &buffer);
    if (reply->error != 0) {				    /* error in config */
	fprintf(stdout, "** %s: %s\n", reply->msg, strerror(reply->error));
	error = ioctl(superdev, VINUM_RELEASECONFIG, NULL); /* save the config to disk */
	if (error != 0)
	    perror("Can't save Vinum config");
    } else {
	error = ioctl(superdev, VINUM_RELEASECONFIG, NULL); /* save the config to disk */
	if (error != 0)
	    perror("Can't save Vinum config");
	if (no_devfs)
	    make_devices();
    }
    checkupdates();					    /* make sure we're updating */
}

#ifdef VINUMDEBUG
void
vinum_debug(int argc, char *argv[], char *arg0[])
{
    struct debuginfo info;

    if (argc > 0) {
	info.param = atoi(argv[0]);
	info.changeit = 1;
    } else {
	info.changeit = 0;
	sleep(2);					    /* give a chance to leave the window */
    }
    ioctl(superdev, VINUM_DEBUG, (caddr_t) & info);
}
#endif

void
vinum_modify(int argc, char *argv[], char *arg0[])
{
    fprintf(stderr, "Modify command is currently not implemented\n");
    checkupdates();					    /* make sure we're updating */
}

void
vinum_set(int argc, char *argv[], char *arg0[])
{
    fprintf(stderr, "set is not implemented yet\n");
}

void
vinum_rm(int argc, char *argv[], char *arg0[])
{
    int object;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;

    if (argc == 0)					    /* start everything */
	fprintf(stderr, "Usage: rm object [object...]\n");
    else {						    /* start specified objects */
	int index;
	enum objecttype type;

	for (index = 0; index < argc; index++) {
	    object = find_object(argv[index], &type);	    /* look for it */
	    if (type == invalid_object)
		fprintf(stderr, "Can't find object: %s\n", argv[index]);
	    else {
		message->index = object;		    /* pass object number */
		message->type = type;			    /* and type of object */
		message->force = force;			    /* do we want to force the operation? */
		message->recurse = recurse;		    /* do we want to remove subordinates? */
		ioctl(superdev, VINUM_REMOVE, message);
		if (reply.error != 0) {
		    fprintf(stderr,
			"Can't remove %s: %s (%d)\n",
			argv[index],
			reply.msg[0] ? reply.msg : strerror(reply.error),
			reply.error);
		} else if (vflag)
		    fprintf(stderr, "%s removed\n", argv[index]);
	    }
	}
	checkupdates();					    /* make sure we're updating */
    }
}

void
vinum_resetconfig(int argc, char *argv[], char *arg0[])
{
    char reply[32];
    int error;

    printf(" WARNING!  This command will completely wipe out your vinum configuration.\n"
	" All data will be lost.  If you really want to do this, enter the text\n\n"
	" NO FUTURE\n"
	" Enter text -> ");
    fgets(reply, sizeof(reply), stdin);
    if (strcmp(reply, "NO FUTURE\n"))			    /* changed his mind */
	printf("\n No change\n");
    else {
	error = ioctl(superdev, VINUM_RESETCONFIG, NULL);   /* trash config on disk */
	if (error) {
	    if (errno == EBUSY)
		fprintf(stderr, "Can't reset configuration: objects are in use\n");
	    else
		perror("Can't find vinum config");
	} else {
	    if (no_devfs)
		make_devices();				    /* recreate the /dev/vinum hierarchy */
	    printf("\b Vinum configuration obliterated\n");
	    start_daemon();				    /* then restart the daemon */
	}
    }
    checkupdates();					    /* make sure we're updating */
}

/* Initialize subdisks */
void
vinum_init(int argc, char *argv[], char *arg0[])
{
    if (argc > 0) {					    /* initialize plexes */
	int objindex;
	int objno;
	enum objecttype type;				    /* type returned */

	if (history)
	    fflush(history);				    /* don't let all the kids do it. */
	for (objindex = 0; objindex < argc; objindex++) {
	    objno = find_object(argv[objindex], &type);	    /* find the object */
	    if (objno < 0)
		printf("Can't find %s\n", argv[objindex]);
	    else {
		switch (type) {
		case volume_object:
		    initvol(objno);
		    break;

		case plex_object:
		    initplex(objno, argv[objindex]);
		    break;

		case sd_object:
		    initsd(objno, dowait);
		    break;

		default:
		    printf("Can't initialize %s: wrong object type\n", argv[objindex]);
		    break;
		}
	    }
	}
    }
    checkupdates();					    /* make sure we're updating */
}

void
initvol(int volno)
{
    printf("Initializing volumes is not implemented yet\n");
}

void
initplex(int plexno, char *name)
{
    int sdno;
    int plexfh = NULL;					    /* file handle for plex */
    pid_t pid;
    char filename[MAXPATHLEN];				    /* create a file name here */

    /* Variables for use by children */
    int failed = 0;					    /* set if a child dies badly */

    sprintf(filename, VINUM_DIR "/plex/%s", name);
    if ((plexfh = open(filename, O_RDWR, S_IRWXU)) < 0) {   /* got a plex, open it */
	/*
	   * We don't actually write anything to the
	   * plex.  We open it to ensure that nobody
	   * else tries to open it while we initialize
	   * its subdisks.
	 */
	fprintf(stderr, "can't open plex %s: %s\n", filename, strerror(errno));
	return;
    }
    if (dowait == 0) {
	pid = fork();					    /* into the background with you */
	if (pid != 0) {					    /* I'm the parent, or we failed */
	    if (pid < 0)				    /* failure */
		printf("Couldn't fork: %s", strerror(errno));
	    close(plexfh);				    /* we don't need this any more */
	    return;
	}
    }
    /*
     * If we get here, we're either the first-level
     * child (if we're not waiting) or we're going
     * to wait.
     */
    for (sdno = 0; sdno < plex.subdisks; sdno++) {	    /* initialize each subdisk */
	get_plex_sd_info(&sd, plexno, sdno);
	initsd(sd.sdno, 0);
    }
    /* Now wait for them to complete */
    while (1) {
	int status;
	pid = wait(&status);
	if (((int) pid == -1)
	    && (errno == ECHILD))			    /* all gone */
	    break;
	if (WEXITSTATUS(status) != 0) {			    /* oh, oh */
	    printf("child %d exited with status 0x%x\n", pid, WEXITSTATUS(status));
	    failed++;
	}
    }
    if (failed == 0) {
#if 0
	message->index = plexno;			    /* pass object number */
	message->type = plex_object;			    /* and type of object */
	message->state = object_up;
	message->force = 1;				    /* insist */
	ioctl(superdev, VINUM_SETSTATE, message);
#endif
	syslog(LOG_INFO | LOG_KERN, "plex %s initialized", plex.name);
    } else
	syslog(LOG_ERR | LOG_KERN, "couldn't initialize plex %s, %d processes died",
	    plex.name,
	    failed);
    if (dowait == 0)					    /* we're the waiting child, */
	exit(0);					    /* we've done our dash */
}

/* Initialize a subdisk. */
void
initsd(int sdno, int dowait)
{
    pid_t pid;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;
    char filename[MAXPATHLEN];				    /* create a file name here */

    /* Variables for use by children */
    int sdfh;						    /* and for subdisk */
    int initsize;					    /* actual size to write */
    int64_t sdsize;					    /* size of subdisk */

    if (dowait == 0) {
	pid = fork();					    /* into the background with you */
	if (pid > 0)					    /* I'm the parent */
	    return;
	else if (pid < 0) {				    /* failure */
	    printf("couldn't fork for subdisk %d: %s", sdno, strerror(errno));
	    return;
	}
    }
    if (SSize != 0) {					    /* specified a size for init */
	if (SSize < 512)
	    SSize <<= DEV_BSHIFT;
	initsize = min(SSize, MAXPLEXINITSIZE);
    } else
	initsize = PLEXINITSIZE;
    openlog("vinum", LOG_CONS | LOG_PERROR | LOG_PID, LOG_KERN);
    get_sd_info(&sd, sdno);
    sdsize = sd.sectors * DEV_BSIZE;			    /* size of subdisk in bytes */
    sprintf(filename, VINUM_DIR "/sd/%s", sd.name);
    setproctitle("initializing %s", filename);		    /* show what we're doing */
    syslog(LOG_INFO | LOG_KERN, "initializing subdisk %s", filename);
    if ((sdfh = open(filename, O_RDWR, S_IRWXU)) < 0) {	    /* no go */
	syslog(LOG_ERR | LOG_KERN,
	    "can't open subdisk %s: %s",
	    filename,
	    strerror(errno));
	exit(1);
    }
    /* Set the subdisk in initializing state */
    message->index = sd.sdno;				    /* pass object number */
    message->type = sd_object;				    /* and type of object */
    message->state = object_initializing;
    message->verify = vflag;				    /* verify what we write? */
    message->force = 1;					    /* insist */
    ioctl(superdev, VINUM_SETSTATE, message);
    if ((SSize > 0)					    /* specified a size for init */
    &&(SSize < 512))
	SSize <<= DEV_BSHIFT;
    if (reply.error) {
	fprintf(stderr,
	    "Can't initialize %s: %s (%d)\n",
	    filename,
	    strerror(reply.error),
	    reply.error);
	exit(1);
    } else {
	do {
	    if (interval)				    /* pause between copies */
		usleep(interval * 1000);
	    message->index = sd.sdno;			    /* pass object number */
	    message->type = sd_object;			    /* and type of object */
	    message->state = object_up;
	    message->verify = vflag;			    /* verify what we write? */
	    message->blocksize = SSize;
	    ioctl(superdev, VINUM_SETSTATE, message);
	}
	while (reply.error == EAGAIN);			    /* until we're done */
	if (reply.error) {
	    fprintf(stderr,
		"Can't initialize %s: %s (%d)\n",
		filename,
		strerror(reply.error),
		reply.error);
	    get_sd_info(&sd, sdno);
	    if (sd.state != sd_up)
		/* Set the subdisk down */
		message->index = sd.sdno;		    /* pass object number */
	    message->type = sd_object;			    /* and type of object */
	    message->state = object_down;
	    message->verify = vflag;			    /* verify what we write? */
	    message->force = 1;				    /* insist */
	    ioctl(superdev, VINUM_SETSTATE, message);
	}
    }
    printf("subdisk %s initialized\n", filename);
    if (!dowait)
	exit(0);
}

void
vinum_start(int argc, char *argv[], char *arg0[])
{
    int object;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;

    if (argc == 0) {					    /* start everything */
	int devs = getnumdevs();
	struct statinfo statinfo;
	char *namelist;
	char *enamelist;				    /* end of name list */
	int i;
	char **token;					    /* list of tokens */
	int tokens;					    /* and their number */

	bzero(&statinfo, sizeof(struct statinfo));
	statinfo.dinfo = malloc(devs * sizeof(struct statinfo));
	namelist = malloc(devs * (DEVSTAT_NAME_LEN + 8));
	token = malloc((devs + 1) * sizeof(char *));
	if ((statinfo.dinfo == NULL) || (namelist == NULL) || (token == NULL)) {
	    fprintf(stderr, "Can't allocate memory for drive list\n");
	    return;
	}
	bzero(statinfo.dinfo, sizeof(struct devinfo));

	tokens = 0;					    /* no tokens yet */
	if (getdevs(&statinfo) < 0) {			    /* find out what devices we have */
	    perror("Can't get device list");
	    return;
	}
	namelist[0] = '\0';				    /* start with empty namelist */
	enamelist = namelist;				    /* point to the end of the list */

	for (i = 0; i < devs; i++) {
	    struct devstat *stat = &statinfo.dinfo->devices[i];

	    if (((stat->device_type & DEVSTAT_TYPE_MASK) == DEVSTAT_TYPE_DIRECT) /* disk device */
	    &&((stat->device_type & DEVSTAT_TYPE_PASS) == 0) /* and not passthrough */
	    &&((stat->device_name[0] != '\0'))) {	    /* and it has a name */
		sprintf(enamelist, _PATH_DEV "%s%d", stat->device_name, stat->unit_number);
		token[tokens] = enamelist;		    /* point to it */
		tokens++;				    /* one more token */
		enamelist = &enamelist[strlen(enamelist) + 1]; /* and start beyond the end */
	    }
	}
	free(statinfo.dinfo);				    /* don't need the list any more */
	vinum_read(tokens, token, &token[0]);		    /* start the system */
	free(namelist);
	free(token);
	list_defective_objects();			    /* and list anything that's down */
    } else {						    /* start specified objects */
	int index;
	enum objecttype type;

	for (index = 0; index < argc; index++) {
	    object = find_object(argv[index], &type);	    /* look for it */
	    if (type == invalid_object)
		fprintf(stderr, "Can't find object: %s\n", argv[index]);
	    else {
		int doit = 0;				    /* set to 1 if we pass our tests */
		switch (type) {
		case drive_object:
		    if (drive.state == drive_up)	    /* already up */
			fprintf(stderr, "%s is already up\n", drive.label.name);
		    else
			doit = 1;
		    break;

		case sd_object:
		    if (sd.state == sd_up)		    /* already up */
			fprintf(stderr, "%s is already up\n", sd.name);
		    else
			doit = 1;
		    break;

		case plex_object:
		    if (plex.state == plex_up)		    /* already up */
			fprintf(stderr, "%s is already up\n", plex.name);
		    else {
			int sdno;

			/*
			 * First, see if we can bring it up
			 * just by asking.  This might happen
			 * if somebody has used setupstate on
			 * the subdisks.  If we don't do this,
			 * we'll return success, but the plex
			 * won't have changed state.  Note
			 * that we don't check for errors
			 * here.
			 */
			message->index = plex.plexno;	    /* pass object number */
			message->type = plex_object;	    /* it's a plex */
			message->state = object_up;
			message->force = 0;		    /* don't force it */
			ioctl(superdev, VINUM_SETSTATE, message);
			for (sdno = 0; sdno < plex.subdisks; sdno++) {
			    get_plex_sd_info(&sd, object, sdno);
			    if ((sd.state >= sd_empty)
				&& (sd.state <= sd_reviving)) {	/* candidate for start */
				message->index = sd.sdno;   /* pass object number */
				message->type = sd_object;  /* it's a subdisk */
				message->state = object_up;
				message->force = force;	    /* don't force it, use a larger hammer */

				/*
				 * We don't do any checking here.
				 * The kernel module has a better
				 * understanding of these things,
				 * let it do it.
				 */
				if (SSize != 0) {	    /* specified a size for init */
				    if (SSize < 512)
					SSize <<= DEV_BSHIFT;
				    message->blocksize = SSize;
				} else
				    message->blocksize = DEFAULT_REVIVE_BLOCKSIZE;
				ioctl(superdev, VINUM_SETSTATE, message);
				if (reply.error != 0) {
				    if (reply.error == EAGAIN) /* we're reviving */
					continue_revive(sd.sdno);
				    else
					fprintf(stderr,
					    "Can't start %s: %s (%d)\n",
					    sd.name,
					    reply.msg[0] ? reply.msg : strerror(reply.error),
					    reply.error);
				}
				if (Verbose)
				    vinum_lsi(sd.sdno, 0);
			    }
			}
		    }
		    break;

		case volume_object:
		    if (vol.state == volume_up)		    /* already up */
			fprintf(stderr, "%s is already up\n", vol.name);
		    else
			doit = 1;
		    break;

		default:
		}

		if (doit) {
		    message->index = object;		    /* pass object number */
		    message->type = type;		    /* and type of object */
		    message->state = object_up;
		    message->force = force;		    /* don't force it, use a larger hammer */

		    /*
		     * We don't do any checking here.
		     * The kernel module has a better
		     * understanding of these things,
		     * let it do it.
		     */
		    if (SSize != 0) {			    /* specified a size for init or revive */
			if (SSize < 512)
			    SSize <<= DEV_BSHIFT;
			message->blocksize = SSize;
		    } else
			message->blocksize = 0;
		    ioctl(superdev, VINUM_SETSTATE, message);
		    if (reply.error != 0) {
			if ((reply.error == EAGAIN)	    /* we're reviving */
			&&(type == sd_object))
			    continue_revive(object);
			else
			    fprintf(stderr,
				"Can't start %s: %s (%d)\n",
				argv[index],
				reply.msg[0] ? reply.msg : strerror(reply.error),
				reply.error);
		    }
		    if (Verbose)
			vinum_li(object, type);
		}
	    }
	}
    }
    checkupdates();					    /* make sure we're updating */
}

void
vinum_stop(int argc, char *argv[], char *arg0[])
{
    int object;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;

    if (checkupdates() && (!force))			    /* not updating? */
	return;
    message->force = force;				    /* should we force the transition? */
    if (argc == 0) {					    /* stop vinum */
	int fileid = 0;					    /* ID of Vinum kld */

	close(superdev);				    /* we can't stop if we have vinum open */
	sleep(1);					    /* wait for the daemon to let go */
	fileid = kldfind(VINUMMOD);
	if ((fileid < 0)				    /* no go */
	||(kldunload(fileid) < 0))
	    perror("Can't unload " VINUMMOD);
	else {
	    fprintf(stderr, VINUMMOD " unloaded\n");
	    exit(0);
	}

	/* If we got here, the stop failed.  Reopen the superdevice. */
	superdev = open(VINUM_SUPERDEV_NAME, O_RDWR);	    /* reopen vinum superdevice */
	if (superdev < 0) {
	    perror("Can't reopen Vinum superdevice");
	    exit(1);
	}
    } else {						    /* stop specified objects */
	int i;
	enum objecttype type;

	for (i = 0; i < argc; i++) {
	    object = find_object(argv[i], &type);	    /* look for it */
	    if (type == invalid_object)
		fprintf(stderr, "Can't find object: %s\n", argv[i]);
	    else {
		message->index = object;		    /* pass object number */
		message->type = type;			    /* and type of object */
		message->state = object_down;
		ioctl(superdev, VINUM_SETSTATE, message);
		if (reply.error != 0)
		    fprintf(stderr,
			"Can't stop %s: %s (%d)\n",
			argv[i],
			reply.msg[0] ? reply.msg : strerror(reply.error),
			reply.error);
		if (Verbose)
		    vinum_li(object, type);
	    }
	}
    }
}

void
vinum_label(int argc, char *argv[], char *arg0[])
{
    int object;
    struct _ioctl_reply reply;
    int *message = (int *) &reply;

    if (argc == 0)					    /* start everything */
	fprintf(stderr, "label: please specify one or more volume names\n");
    else {						    /* start specified objects */
	int i;
	enum objecttype type;

	for (i = 0; i < argc; i++) {
	    object = find_object(argv[i], &type);	    /* look for it */
	    if (type == invalid_object)
		fprintf(stderr, "Can't find object: %s\n", argv[i]);
	    else if (type != volume_object)		    /* it exists, but it isn't a volume */
		fprintf(stderr, "%s is not a volume\n", argv[i]);
	    else {
		message[0] = object;			    /* pass object number */
		ioctl(superdev, VINUM_LABEL, message);
		if (reply.error != 0)
		    fprintf(stderr,
			"Can't label %s: %s (%d)\n",
			argv[i],
			reply.msg[0] ? reply.msg : strerror(reply.error),
			reply.error);
		if (Verbose)
		    vinum_li(object, type);
	    }
	}
    }
    checkupdates();					    /* not updating? */
}

void
reset_volume_stats(int volno, int recurse)
{
    struct vinum_ioctl_msg msg;
    struct _ioctl_reply *reply = (struct _ioctl_reply *) &msg;

    msg.index = volno;
    msg.type = volume_object;
    /* XXX get these numbers right if we ever
     * actually return errors */
    if (ioctl(superdev, VINUM_RESETSTATS, &msg) < 0) {
	fprintf(stderr, "Can't reset stats for volume %d: %s\n", volno, reply->msg);
	longjmp(command_fail, -1);
    } else if (recurse) {
	struct _volume vol;
	int plexno;

	get_volume_info(&vol, volno);
	for (plexno = 0; plexno < vol.plexes; plexno++)
	    reset_plex_stats(vol.plex[plexno], recurse);
    }
}

void
reset_plex_stats(int plexno, int recurse)
{
    struct vinum_ioctl_msg msg;
    struct _ioctl_reply *reply = (struct _ioctl_reply *) &msg;

    msg.index = plexno;
    msg.type = plex_object;
    /* XXX get these numbers right if we ever
     * actually return errors */
    if (ioctl(superdev, VINUM_RESETSTATS, &msg) < 0) {
	fprintf(stderr, "Can't reset stats for plex %d: %s\n", plexno, reply->msg);
	longjmp(command_fail, -1);
    } else if (recurse) {
	struct _plex plex;
	struct _sd sd;
	int sdno;

	get_plex_info(&plex, plexno);
	for (sdno = 0; sdno < plex.subdisks; sdno++) {
	    get_plex_sd_info(&sd, plex.plexno, sdno);
	    reset_sd_stats(sd.sdno, recurse);
	}
    }
}

void
reset_sd_stats(int sdno, int recurse)
{
    struct vinum_ioctl_msg msg;
    struct _ioctl_reply *reply = (struct _ioctl_reply *) &msg;

    msg.index = sdno;
    msg.type = sd_object;
    /* XXX get these numbers right if we ever
     * actually return errors */
    if (ioctl(superdev, VINUM_RESETSTATS, &msg) < 0) {
	fprintf(stderr, "Can't reset stats for subdisk %d: %s\n", sdno, reply->msg);
	longjmp(command_fail, -1);
    } else if (recurse) {
	get_sd_info(&sd, sdno);				    /* get the info */
	reset_drive_stats(sd.driveno);			    /* and clear the drive */
    }
}

void
reset_drive_stats(int driveno)
{
    struct vinum_ioctl_msg msg;
    struct _ioctl_reply *reply = (struct _ioctl_reply *) &msg;

    msg.index = driveno;
    msg.type = drive_object;
    /* XXX get these numbers right if we ever
     * actually return errors */
    if (ioctl(superdev, VINUM_RESETSTATS, &msg) < 0) {
	fprintf(stderr, "Can't reset stats for drive %d: %s\n", driveno, reply->msg);
	longjmp(command_fail, -1);
    }
}

void
vinum_resetstats(int argc, char *argv[], char *argv0[])
{
    int i;
    int objno;
    enum objecttype type;

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    if (argc == 0) {
	for (objno = 0; objno < vinum_conf.volumes_allocated; objno++)
	    reset_volume_stats(objno, 1);		    /* clear everything recursively */
    } else {
	for (i = 0; i < argc; i++) {
	    objno = find_object(argv[i], &type);
	    if (objno >= 0) {				    /* not invalid */
		switch (type) {
		case drive_object:
		    reset_drive_stats(objno);
		    break;

		case sd_object:
		    reset_sd_stats(objno, recurse);
		    break;

		case plex_object:
		    reset_plex_stats(objno, recurse);
		    break;

		case volume_object:
		    reset_volume_stats(objno, recurse);
		    break;

		case invalid_object:			    /* can't get this */
		    break;
		}
	    }
	}
    }
}

/* Attach a subdisk to a plex, or a plex to a volume.
 * attach subdisk plex [offset] [rename]
 * attach plex volume  [rename]
 */
void
vinum_attach(int argc, char *argv[], char *argv0[])
{
    int i;
    enum objecttype supertype;
    struct vinum_ioctl_msg msg;
    struct _ioctl_reply *reply = (struct _ioctl_reply *) &msg;
    const char *objname = argv[0];
    const char *supername = argv[1];
    int sdno = -1;
    int plexno = -1;
    char oldname[MAXNAME + 8];
    char newname[MAXNAME + 8];
    int rename = 0;					    /* set if we want to rename the object */

    if ((argc < 2)
	|| (argc > 4)) {
	fprintf(stderr,
	    "Usage: \tattach <subdisk> <plex> [rename] [<plexoffset>]\n"
	    "\tattach <plex> <volume> [rename]\n");
	return;
    }
    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    msg.index = find_object(objname, &msg.type);	    /* find the object to attach */
    msg.otherobject = find_object(supername, &supertype);   /* and the object to attach to */
    msg.force = force;					    /* did we specify the use of force? */
    msg.recurse = recurse;
    msg.offset = -1;					    /* and no offset */

    for (i = 2; i < argc; i++) {
	if (!strcmp(argv[i], "rename")) {
	    rename = 1;
	    msg.rename = 1;				    /* do renaming */
	} else if (!isdigit(argv[i][0])) {		    /* not an offset */
	    fprintf(stderr, "Unknown attribute: %s\n", supername);
	    return;
	} else
	    msg.offset = sizespec(argv[i]);
    }

    switch (msg.type) {
    case sd_object:
	find_object(argv[1], &supertype);
	if (supertype != plex_object) {			    /* huh? */
	    fprintf(stderr, "%s can only be attached to a plex\n", objname);
	    return;
	}
	if ((plex.organization != plex_concat)		    /* not a cat plex, */
	&&(!force)) {
	    fprintf(stderr, "Can't attach subdisks to a %s plex\n", plex_org(plex.organization));
	    return;
	}
	sdno = msg.index;				    /* note the subdisk number for later */
	break;

    case plex_object:
	find_object(argv[1], &supertype);
	if (supertype != volume_object) {		    /* huh? */
	    fprintf(stderr, "%s can only be attached to a volume\n", objname);
	    return;
	}
	break;

    case volume_object:
    case drive_object:
	fprintf(stderr, "Can only attach subdisks and plexes\n");
	return;

    default:
	fprintf(stderr, "%s is not a Vinum object\n", objname);
	return;
    }

    ioctl(superdev, VINUM_ATTACH, &msg);
    if (reply->error != 0) {
	if (reply->error == EAGAIN)			    /* reviving */
	    continue_revive(sdno);			    /* continue the revive */
	else
	    fprintf(stderr,
		"Can't attach %s to %s: %s (%d)\n",
		objname,
		supername,
		reply->msg[0] ? reply->msg : strerror(reply->error),
		reply->error);
    }
    if (rename) {
	struct sd;
	struct _plex;
	struct _volume;

	/* we've overwritten msg with the
	 * ioctl reply, start again */
	msg.index = find_object(objname, &msg.type);	    /* find the object to rename */
	switch (msg.type) {
	case sd_object:
	    get_sd_info(&sd, msg.index);
	    get_plex_info(&plex, sd.plexno);
	    for (sdno = 0; sdno < plex.subdisks; sdno++) {
		if (plex.sdnos[sdno] == msg.index)	    /* found our subdisk */
		    break;
	    }
	    sprintf(newname, "%s.s%d", plex.name, sdno);
	    sprintf(oldname, "%s", sd.name);
	    vinum_rename_2(oldname, newname);
	    break;

	case plex_object:
	    get_plex_info(&plex, msg.index);
	    get_volume_info(&vol, plex.volno);
	    for (plexno = 0; plexno < vol.plexes; plexno++) {
		if (vol.plex[plexno] == msg.index)	    /* found our subdisk */
		    break;
	    }
	    sprintf(newname, "%s.p%d", vol.name, plexno);
	    sprintf(oldname, "%s", plex.name);
	    vinum_rename_2(oldname, newname);		    /* this may recurse */
	    break;

	default:					    /* can't get here */
	}
    }
    checkupdates();					    /* make sure we're updating */
}

/* Detach a subdisk from a plex, or a plex from a volume.
 * detach subdisk plex [rename]
 * detach plex volume [rename]
 */
void
vinum_detach(int argc, char *argv[], char *argv0[])
{
    struct vinum_ioctl_msg msg;
    struct _ioctl_reply *reply = (struct _ioctl_reply *) &msg;

    if ((argc < 1)
	|| (argc > 2)) {
	fprintf(stderr,
	    "Usage: \tdetach <subdisk> [rename]\n"
	    "\tdetach <plex> [rename]\n");
	return;
    }
    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    msg.index = find_object(argv[0], &msg.type);	    /* find the object to detach */
    msg.force = force;					    /* did we specify the use of force? */
    msg.rename = 0;					    /* don't specify new name */
    msg.recurse = recurse;				    /* but recurse if we have to */

    /* XXX are we going to keep this?
     * Don't document it yet, since the
     * kernel side of things doesn't
     * implement it */
    if (argc == 2) {
	if (!strcmp(argv[1], "rename"))
	    msg.rename = 1;				    /* do renaming */
	else {
	    fprintf(stderr, "Unknown attribute: %s\n", argv[1]);
	    return;
	}
    }
    if ((msg.type != sd_object)
	&& (msg.type != plex_object)) {
	fprintf(stderr, "Can only detach subdisks and plexes\n");
	return;
    }
    ioctl(superdev, VINUM_DETACH, &msg);
    if (reply->error != 0)
	fprintf(stderr,
	    "Can't detach %s: %s (%d)\n",
	    argv[0],
	    reply->msg[0] ? reply->msg : strerror(reply->error),
	    reply->error);
    checkupdates();					    /* make sure we're updating */
}

static void
dorename(struct vinum_rename_msg *msg, const char *oldname, const char *name, int maxlen)
{
    struct _ioctl_reply *reply = (struct _ioctl_reply *) msg;

    if (strlen(name) > maxlen) {
	fprintf(stderr, "%s is too long\n", name);
	return;
    }
    strcpy(msg->newname, name);
    ioctl(superdev, VINUM_RENAME, msg);
    if (reply->error != 0)
	fprintf(stderr,
	    "Can't rename %s to %s: %s (%d)\n",
	    oldname,
	    name,
	    reply->msg[0] ? reply->msg : strerror(reply->error),
	    reply->error);
}

/* Rename an object:
 * rename <object> "newname"
 */
void
vinum_rename_2(char *oldname, char *newname)
{
    struct vinum_rename_msg msg;
    int volno;
    int plexno;

    msg.index = find_object(oldname, &msg.type);	    /* find the object to rename */
    msg.recurse = recurse;

    /* Ugh.  Determine how long the name may be */
    switch (msg.type) {
    case drive_object:
	dorename(&msg, oldname, newname, MAXDRIVENAME);
	break;

    case sd_object:
	dorename(&msg, oldname, newname, MAXSDNAME);
	break;

    case plex_object:
	plexno = msg.index;
	dorename(&msg, oldname, newname, MAXPLEXNAME);
	if (recurse) {
	    int sdno;

	    get_plex_info(&plex, plexno);		    /* find out who we are */
	    msg.type = sd_object;
	    for (sdno = 0; sdno < plex.subdisks; sdno++) {
		char sdname[MAXPLEXNAME + 8];

		get_plex_sd_info(&sd, plex.plexno, sdno);   /* get info about the subdisk */
		sprintf(sdname, "%s.s%d", newname, sdno);
		msg.index = sd.sdno;			    /* number of the subdisk */
		dorename(&msg, sd.name, sdname, MAXSDNAME);
	    }
	}
	break;

    case volume_object:
	volno = msg.index;
	dorename(&msg, oldname, newname, MAXVOLNAME);
	if (recurse) {
	    int sdno;
	    int plexno;

	    get_volume_info(&vol, volno);		    /* find out who we are */
	    for (plexno = 0; plexno < vol.plexes; plexno++) {
		char plexname[MAXVOLNAME + 8];

		msg.type = plex_object;
		sprintf(plexname, "%s.p%d", newname, plexno);
		msg.index = vol.plex[plexno];		    /* number of the plex */
		dorename(&msg, plex.name, plexname, MAXPLEXNAME);
		get_plex_info(&plex, vol.plex[plexno]);	    /* find out who we are */
		msg.type = sd_object;
		for (sdno = 0; sdno < plex.subdisks; sdno++) {
		    char sdname[MAXPLEXNAME + 8];

		    get_plex_sd_info(&sd, plex.plexno, sdno); /* get info about the subdisk */
		    sprintf(sdname, "%s.s%d", plexname, sdno);
		    msg.index = sd.sdno;		    /* number of the subdisk */
		    dorename(&msg, sd.name, sdname, MAXSDNAME);
		}
	    }
	}
	break;

    default:
	fprintf(stderr, "%s is not a Vinum object\n", oldname);
	return;
    }
}

void
vinum_rename(int argc, char *argv[], char *argv0[])
{
    if (argc != 2) {
	fprintf(stderr, "Usage: \trename <object> <new name>\n");
	return;
    }
    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    vinum_rename_2(argv[0], argv[1]);
    checkupdates();					    /* make sure we're updating */
}

/*
 * Move objects:
 *
 * mv <dest> <src> ...
 */
void
vinum_mv(int argc, char *argv[], char *argv0[])
{
    int i;						    /* loop index */
    int srcobj;
    int destobj;
    enum objecttype srct;
    enum objecttype destt;
    int sdno;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *msg = (struct vinum_ioctl_msg *) &reply;

    if (argc < 2) {
	fprintf(stderr, "Usage: \tmove <dest> <src> ...\n");
	return;
    }
    /* Get current config */
    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Cannot get vinum config\n");
	return;
    }
    /* Get our destination */
    destobj = find_object(argv[0], &destt);
    if (destobj == -1) {
	fprintf(stderr, "Can't find %s\n", argv[0]);
	return;
    }
    /* Verify that the target is a drive */
    if (destt != drive_object) {
	fprintf(stderr, "%s is not a drive\n", argv[0]);
	return;
    }
    for (i = 1; i < argc; i++) {			    /* for all the sources */
	srcobj = find_object(argv[i], &srct);
	if (srcobj == -1) {
	    fprintf(stderr, "Can't find %s\n", argv[i]);
	    continue;
	}
	msg->index = destobj;
	switch (srct) {					    /* Handle the source object */
	case drive_object:				    /* Move all subdisks on the drive to dst. */
	    get_drive_info(&drive, srcobj);		    /* get info on drive */
	    for (sdno = 0; sdno < vinum_conf.subdisks_allocated; ++sdno) {
		get_sd_info(&sd, sdno);
		if (sd.driveno == srcobj) {
		    msg->index = destobj;
		    msg->otherobject = sd.sdno;
		    if (ioctl(superdev, VINUM_MOVE, msg) < 0)
			fprintf(stderr,
			    "Can't move %s (part of %s) to %s: %s (%d)\n",
			    sd.name,
			    drive.label.name,
			    argv[0],
			    strerror(reply.error),
			    reply.error);
		}
	    }
	    break;

	case sd_object:
	    msg->otherobject = srcobj;
	    if (ioctl(superdev, VINUM_MOVE, msg) < 0)
		fprintf(stderr,
		    "Can't move %s to %s: %s (%d)\n",
		    sd.name,
		    argv[0],
		    strerror(reply.error),
		    reply.error);
	    break;

	case plex_object:
	    get_plex_info(&plex, srcobj);
	    for (sdno = 0; sdno < plex.subdisks; ++sdno) {
		get_plex_sd_info(&sd, plex.plexno, sdno);
		msg->index = destobj;
		msg->otherobject = sd.sdno;
		if (ioctl(superdev, VINUM_MOVE, msg) < 0)
		    fprintf(stderr,
			"Can't move %s (part of %s) to %s: %s (%d)\n",
			sd.name,
			plex.name,
			argv[0],
			strerror(reply.error),
			reply.error);
	    }
	    break;

	case volume_object:
	case invalid_object:
	default:
	    fprintf(stderr, "Can't move %s (inappropriate object).\n", argv[i]);
	    break;
	}
	if (reply.error)
	    fprintf(stderr,
		"Can't move %s to %s: %s (%d)\n",
		argv[i],
		argv[0],
		strerror(reply.error),
		reply.error);
    }
    checkupdates();					    /* make sure we're updating */
}

/*
 * Replace objects.  Not implemented, may never be.
 */
void
vinum_replace(int argc, char *argv[], char *argv0[])
{
    fprintf(stderr, "'replace' not implemented yet.  Use 'move' instead\n");
}

/* Primitive help function */
void
vinum_help(int argc, char *argv[], char *argv0[])
{
    char commands[] =
    {
	"COMMANDS\n"
	"attach plex volume [rename]\n"
	"attach subdisk plex [offset] [rename]\n"
	"        Attach a plex to a volume, or a subdisk to a plex.\n"
	"checkparity plex [-f] [-v]\n"
	"        Check the parity blocks of a RAID-4 or RAID-5 plex.\n"
	"concat [-f] [-n name] [-v] drives\n"
	"        Create a concatenated volume from the specified drives.\n"
	"create [-f] description-file\n"
	"        Create a volume as described in description-file.\n"
	"debug   Cause the volume manager to enter the kernel debugger.\n"
	"debug flags\n"
	"        Set debugging flags.\n"
	"detach [-f] [plex | subdisk]\n"
	"        Detach a plex or subdisk from the volume or plex to which it is\n"
	"        attached.\n"
	"dumpconfig [drive ...]\n"
	"        List the configuration information stored on the specified\n"
	"        drives, or all drives in the system if no drive names are speci-\n"
	"        fied.\n"
	"info [-v] [-V]\n"
	"        List information about volume manager state.\n"
	"init [-S size] [-w] plex | subdisk\n"
	"        Initialize the contents of a subdisk or all the subdisks of a\n"
	"        plex to all zeros.\n"
	"label volume\n"
	"        Create a volume label.\n"
	"l | list [-r] [-s] [-v] [-V] [volume | plex | subdisk]\n"
	"        List information about specified objects.\n"
	"ld [-r] [-s] [-v] [-V] [volume]\n"
	"        List information about drives.\n"
	"ls [-r] [-s] [-v] [-V] [subdisk]\n"
	"        List information about subdisks.\n"
	"lp [-r] [-s] [-v] [-V] [plex]\n"
	"        List information about plexes.\n"
	"lv [-r] [-s] [-v] [-V] [volume]\n"
	"        List information about volumes.\n"
	"makedev\n"
	"        Remake the device nodes in /dev/vinum.\n"
	"mirror [-f] [-n name] [-s] [-v] drives\n"
	"        Create a mirrored volume from the specified drives.\n"
	"move | mv -f drive object ...\n"
	"        Move the object(s) to the specified drive.\n"
	"printconfig [file]\n"
	"        Write a copy of the current configuration to file.\n"
	"quit    Exit the vinum program when running in interactive mode.  Nor-\n"
	"        mally this would be done by entering the EOF character.\n"
	"read disk ...\n"
	"        Read the vinum configuration from the specified disks.\n"
	"rename [-r] [drive | subdisk | plex | volume] newname\n"
	"        Change the name of the specified object.\n"
	"rebuildparity plex [-f] [-v] [-V]\n"
	"        Rebuild the parity blocks of a RAID-4 or RAID-5 plex.\n"
	"resetconfig\n"
	"        Reset the complete vinum configuration.\n"
	"resetstats [-r] [volume | plex | subdisk]\n"
	"        Reset statistisc counters for the specified objects, or for all\n"
	"        objects if none are specified.\n"
	"rm [-f] [-r] volume | plex | subdisk\n"
	"        Remove an object.\n"
	"saveconfig\n"
	"        Save vinum configuration to disk after configuration failures.\n"
	"setdaemon [value]\n"
	"        Set daemon configuration.\n"
	"setstate state [volume | plex | subdisk | drive]\n"
	"        Set state without influencing other objects, for diagnostic pur-\n"
	"        poses only.\n"
	"start   Read configuration from all vinum drives.\n"
	"start [-i interval] [-S size] [-w] volume | plex | subdisk\n"
	"        Allow the system to access the objects.\n"
	"stop [-f] [volume | plex | subdisk]\n"
	"        Terminate access to the objects, or stop vinum if no parameters\n"
	"        are specified.\n"
	"stripe [-f] [-n name] [-v] drives\n"
	"        Create a striped volume from the specified drives.\n"
    };
    puts(commands);
}

/* Set daemon options.
 * XXX quick and dirty: use a bitmap, which requires
 * knowing which bit does what.  FIXME */
void
vinum_setdaemon(int argc, char *argv[], char *argv0[])
{
    int options;

    switch (argc) {
    case 0:
	if (ioctl(superdev, VINUM_GETDAEMON, &options) < 0)
	    fprintf(stderr, "Can't get daemon options: %s (%d)\n", strerror(errno), errno);
	else
	    printf("Options mask: %d\n", options);
	break;

    case 1:
	options = atoi(argv[0]);
	if (ioctl(superdev, VINUM_SETDAEMON, &options) < 0)
	    fprintf(stderr, "Can't set daemon options: %s (%d)\n", strerror(errno), errno);
	break;

    default:
	fprintf(stderr, "Usage: \tsetdaemon [<bitmask>]\n");
    }
    checkupdates();					    /* make sure we're updating */
}

/* Save config info */
void
vinum_saveconfig(int argc, char *argv[], char *argv0[])
{
    int ioctltype;

    if (argc != 0) {
	printf("Usage: saveconfig\n");
	return;
    }
    ioctltype = 1;					    /* user saveconfig */
    if (ioctl(superdev, VINUM_SAVECONFIG, &ioctltype) < 0)
	fprintf(stderr, "Can't save configuration: %s (%d)\n", strerror(errno), errno);
    checkupdates();					    /* make sure we're updating */
}

/*
 * Create a volume name for the quick and dirty
 * commands.  It will be of the form "vinum#",
 * where # is a small positive number.
 */
void
genvolname()
{
    int v;						    /* volume number */
    static char volumename[MAXVOLNAME];			    /* name to create */
    enum objecttype type;

    objectname = volumename;				    /* point to it */
    for (v = 0;; v++) {
	sprintf(objectname, "vinum%d", v);		    /* create the name */
	if (find_object(objectname, &type) == -1)	    /* does it exist? */
	    return;					    /* no, it's ours */
    }
}

/*
 * Create a drive for the quick and dirty
 * commands.  The name will be of the form
 * vinumdrive#, where # is a small positive
 * number.  Return the name of the drive.
 */
struct _drive *
create_drive(char *devicename)
{
    int d;						    /* volume number */
    static char drivename[MAXDRIVENAME];		    /* name to create */
    enum objecttype type;
    struct _ioctl_reply *reply;

    /*
     * We're never likely to get anything
     * like 10000 drives.  The only reason for
     * this limit is to stop the thing
     * looping if we have a bug somewhere.
     */
    for (d = 0; d < 100000; d++) {			    /* look for a free drive number */
	sprintf(drivename, "vinumdrive%d", d);		    /* create the name */
	if (find_object(drivename, &type) == -1) {	    /* does it exist? */
	    char command[MAXDRIVENAME * 2];

	    sprintf(command, "drive %s device %s", drivename, devicename); /* create a create command */
	    if (vflag)
		printf("drive %s device %s\n", drivename, devicename); /* create a create command */
	    ioctl(superdev, VINUM_CREATE, command);
	    reply = (struct _ioctl_reply *) &command;
	    if (reply->error != 0) {			    /* error in config */
		if (reply->msg[0])
		    fprintf(stderr,
			"Can't create drive %s, device %s: %s\n",
			drivename,
			devicename,
			reply->msg);
		else
		    fprintf(stderr,
			"Can't create drive %s, device %s: %s (%d)\n",
			drivename,
			devicename,
			strerror(reply->error),
			reply->error);
		longjmp(command_fail, -1);		    /* give up */
	    }
	    find_object(drivename, &type);
	    return &drive;				    /* return the name of the drive */
	}
    }
    fprintf(stderr, "Can't generate a drive name\n");
    /* NOTREACHED */
    return NULL;
}

/*
 * Create a volume with a single concatenated plex from
 * as much space as we can get on the specified drives.
 * If the drives aren't Vinum drives, make them so.
 */
void
vinum_concat(int argc, char *argv[], char *argv0[])
{
    int o;						    /* object number */
    char buffer[BUFSIZE];
    struct _drive *drive;				    /* drive we're currently looking at */
    struct _ioctl_reply *reply;
    int ioctltype;
    int error;
    enum objecttype type;

    reply = (struct _ioctl_reply *) &buffer;
    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) {	    /* can't get config? */
	printf("Can't configure: %s (%d)\n", strerror(errno), errno);
	return;
    }
    if (!objectname)					    /* we need a name for our object */
	genvolname();
    sprintf(buffer, "volume %s", objectname);
    if (vflag)
	printf("volume %s\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);		    /* create the volume */
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create volume %s: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create volume %s: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    sprintf(buffer, "plex name %s.p0 org concat", objectname);
    if (vflag)
	printf("  plex name %s.p0 org concat\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create plex %s.p0: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create plex %s.p0: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    for (o = 0; o < argc; o++) {
	if ((drive = find_drive_by_devname(argv[o])) == NULL) /* doesn't exist */
	    drive = create_drive(argv[o]);		    /* create it */
	sprintf(buffer, "sd name %s.p0.s%d drive %s size 0", objectname, o, drive->label.name);
	if (vflag)
	    printf("    sd name %s.p0.s%d drive %s size 0\n", objectname, o, drive->label.name);
	ioctl(superdev, VINUM_CREATE, buffer);
	if (reply->error != 0) {			    /* error in config */
	    if (reply->msg[0])
		fprintf(stderr,
		    "Can't create subdisk %s.p0.s%d: %s\n",
		    objectname,
		    o,
		    reply->msg);
	    else
		fprintf(stderr,
		    "Can't create subdisk %s.p0.s%d: %s (%d)\n",
		    objectname,
		    o,
		    strerror(reply->error),
		    reply->error);
	    longjmp(command_fail, -1);			    /* give up */
	}
    }

    /* done, save the config */
    ioctltype = 0;					    /* saveconfig after update */
    error = ioctl(superdev, VINUM_SAVECONFIG, &ioctltype);  /* save the config to disk */
    if (error != 0)
	perror("Can't save Vinum config");
    find_object(objectname, &type);			    /* find the index of the volume */
    make_vol_dev(vol.volno, 1);				    /* and create the devices */
    if (vflag) {
	vflag--;					    /* XXX don't give too much detail */
	find_object(objectname, &type);			    /* point to the volume */
	vinum_lvi(vol.volno, 1);			    /* and print info about it */
    }
}


/*
 * Create a volume with a single striped plex from
 * as much space as we can get on the specified drives.
 * If the drives aren't Vinum drives, make them so.
 */
void
vinum_stripe(int argc, char *argv[], char *argv0[])
{
    int o;						    /* object number */
    char buffer[BUFSIZE];
    struct _drive *drive;				    /* drive we're currently looking at */
    struct _ioctl_reply *reply;
    int ioctltype;
    int error;
    enum objecttype type;
    off_t maxsize;
    int fe;						    /* freelist entry index */
    struct drive_freelist freelist;
    struct ferq {					    /* request to pass to ioctl */
	int driveno;
	int fe;
    } *ferq = (struct ferq *) &freelist;
    u_int64_t bigchunk;					    /* biggest chunk in freelist */

    maxsize = QUAD_MAX;
    reply = (struct _ioctl_reply *) &buffer;

    /*
     * First, check our drives.
     */
    if (argc < 2) {
	fprintf(stderr, "You need at least two drives to create a striped plex\n");
	return;
    }
    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) {	    /* can't get config? */
	printf("Can't configure: %s (%d)\n", strerror(errno), errno);
	return;
    }
    if (!objectname)					    /* we need a name for our object */
	genvolname();
    for (o = 0; o < argc; o++) {
	if ((drive = find_drive_by_devname(argv[o])) == NULL) /* doesn't exist */
	    drive = create_drive(argv[o]);		    /* create it */
	/* Now find the largest chunk available on the drive */
	bigchunk = 0;					    /* ain't found nothin' yet */
	for (fe = 0; fe < drive->freelist_entries; fe++) {
	    ferq->driveno = drive->driveno;
	    ferq->fe = fe;
	    if (ioctl(superdev, VINUM_GETFREELIST, &freelist) < 0) {
		fprintf(stderr,
		    "Can't get free list element %d: %s\n",
		    fe,
		    strerror(errno));
		longjmp(command_fail, -1);
	    }
	    bigchunk = bigchunk > freelist.sectors ? bigchunk : freelist.sectors; /* max it */
	}
	maxsize = min(maxsize, bigchunk);		    /* this is as much as we can do */
    }

    /* Now create the volume */
    sprintf(buffer, "volume %s", objectname);
    if (vflag)
	printf("volume %s\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);		    /* create the volume */
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create volume %s: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create volume %s: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    sprintf(buffer, "plex name %s.p0 org striped 279k", objectname);
    if (vflag)
	printf("  plex name %s.p0 org striped 279k\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create plex %s.p0: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create plex %s.p0: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    for (o = 0; o < argc; o++) {
	drive = find_drive_by_devname(argv[o]);		    /* we know it exists... */
	sprintf(buffer,
	    "sd name %s.p0.s%d drive %s size %lldb",
	    objectname,
	    o,
	    drive->label.name,
	    (long long) maxsize);
	if (vflag)
	    printf("    sd name %s.p0.s%d drive %s size %lldb\n",
		objectname,
		o,
		drive->label.name,
		(long long) maxsize);
	ioctl(superdev, VINUM_CREATE, buffer);
	if (reply->error != 0) {			    /* error in config */
	    if (reply->msg[0])
		fprintf(stderr,
		    "Can't create subdisk %s.p0.s%d: %s\n",
		    objectname,
		    o,
		    reply->msg);
	    else
		fprintf(stderr,
		    "Can't create subdisk %s.p0.s%d: %s (%d)\n",
		    objectname,
		    o,
		    strerror(reply->error),
		    reply->error);
	    longjmp(command_fail, -1);			    /* give up */
	}
    }

    /* done, save the config */
    ioctltype = 0;					    /* saveconfig after update */
    error = ioctl(superdev, VINUM_SAVECONFIG, &ioctltype);  /* save the config to disk */
    if (error != 0)
	perror("Can't save Vinum config");
    find_object(objectname, &type);			    /* find the index of the volume */
    make_vol_dev(vol.volno, 1);				    /* and create the devices */
    if (vflag) {
	vflag--;					    /* XXX don't give too much detail */
	find_object(objectname, &type);			    /* point to the volume */
	vinum_lvi(vol.volno, 1);			    /* and print info about it */
    }
}

/*
 * Create a volume with a single RAID-4 plex from
 * as much space as we can get on the specified drives.
 * If the drives aren't Vinum drives, make them so.
 */
void
vinum_raid4(int argc, char *argv[], char *argv0[])
{
    int o;						    /* object number */
    char buffer[BUFSIZE];
    struct _drive *drive;				    /* drive we're currently looking at */
    struct _ioctl_reply *reply;
    int ioctltype;
    int error;
    enum objecttype type;
    off_t maxsize;
    int fe;						    /* freelist entry index */
    struct drive_freelist freelist;
    struct ferq {					    /* request to pass to ioctl */
	int driveno;
	int fe;
    } *ferq = (struct ferq *) &freelist;
    u_int64_t bigchunk;					    /* biggest chunk in freelist */

    maxsize = QUAD_MAX;
    reply = (struct _ioctl_reply *) &buffer;

    /*
     * First, check our drives.
     */
    if (argc < 3) {
	fprintf(stderr, "You need at least three drives to create a RAID-4 plex\n");
	return;
    }
    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) {	    /* can't get config? */
	printf("Can't configure: %s (%d)\n", strerror(errno), errno);
	return;
    }
    if (!objectname)					    /* we need a name for our object */
	genvolname();
    for (o = 0; o < argc; o++) {
	if ((drive = find_drive_by_devname(argv[o])) == NULL) /* doesn't exist */
	    drive = create_drive(argv[o]);		    /* create it */
	/* Now find the largest chunk available on the drive */
	bigchunk = 0;					    /* ain't found nothin' yet */
	for (fe = 0; fe < drive->freelist_entries; fe++) {
	    ferq->driveno = drive->driveno;
	    ferq->fe = fe;
	    if (ioctl(superdev, VINUM_GETFREELIST, &freelist) < 0) {
		fprintf(stderr,
		    "Can't get free list element %d: %s\n",
		    fe,
		    strerror(errno));
		longjmp(command_fail, -1);
	    }
	    bigchunk = bigchunk > freelist.sectors ? bigchunk : freelist.sectors; /* max it */
	}
	maxsize = min(maxsize, bigchunk);		    /* this is as much as we can do */
    }

    /* Now create the volume */
    sprintf(buffer, "volume %s", objectname);
    if (vflag)
	printf("volume %s\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);		    /* create the volume */
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create volume %s: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create volume %s: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    sprintf(buffer, "plex name %s.p0 org raid4 279k", objectname);
    if (vflag)
	printf("  plex name %s.p0 org raid4 279k\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create plex %s.p0: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create plex %s.p0: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    for (o = 0; o < argc; o++) {
	drive = find_drive_by_devname(argv[o]);		    /* we know it exists... */
	sprintf(buffer,
	    "sd name %s.p0.s%d drive %s size %lldb",
	    objectname,
	    o,
	    drive->label.name,
	    (long long) maxsize);
	if (vflag)
	    printf("    sd name %s.p0.s%d drive %s size %lldb\n",
		objectname,
		o,
		drive->label.name,
		(long long) maxsize);
	ioctl(superdev, VINUM_CREATE, buffer);
	if (reply->error != 0) {			    /* error in config */
	    if (reply->msg[0])
		fprintf(stderr,
		    "Can't create subdisk %s.p0.s%d: %s\n",
		    objectname,
		    o,
		    reply->msg);
	    else
		fprintf(stderr,
		    "Can't create subdisk %s.p0.s%d: %s (%d)\n",
		    objectname,
		    o,
		    strerror(reply->error),
		    reply->error);
	    longjmp(command_fail, -1);			    /* give up */
	}
    }

    /* done, save the config */
    ioctltype = 0;					    /* saveconfig after update */
    error = ioctl(superdev, VINUM_SAVECONFIG, &ioctltype);  /* save the config to disk */
    if (error != 0)
	perror("Can't save Vinum config");
    find_object(objectname, &type);			    /* find the index of the volume */
    make_vol_dev(vol.volno, 1);				    /* and create the devices */
    if (vflag) {
	vflag--;					    /* XXX don't give too much detail */
	find_object(objectname, &type);			    /* point to the volume */
	vinum_lvi(vol.volno, 1);			    /* and print info about it */
    }
}

/*
 * Create a volume with a single RAID-4 plex from
 * as much space as we can get on the specified drives.
 * If the drives aren't Vinum drives, make them so.
 */
void
vinum_raid5(int argc, char *argv[], char *argv0[])
{
    int o;						    /* object number */
    char buffer[BUFSIZE];
    struct _drive *drive;				    /* drive we're currently looking at */
    struct _ioctl_reply *reply;
    int ioctltype;
    int error;
    enum objecttype type;
    off_t maxsize;
    int fe;						    /* freelist entry index */
    struct drive_freelist freelist;
    struct ferq {					    /* request to pass to ioctl */
	int driveno;
	int fe;
    } *ferq = (struct ferq *) &freelist;
    u_int64_t bigchunk;					    /* biggest chunk in freelist */

    maxsize = QUAD_MAX;
    reply = (struct _ioctl_reply *) &buffer;

    /*
     * First, check our drives.
     */
    if (argc < 3) {
	fprintf(stderr, "You need at least three drives to create a RAID-5 plex\n");
	return;
    }
    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) {	    /* can't get config? */
	printf("Can't configure: %s (%d)\n", strerror(errno), errno);
	return;
    }
    if (!objectname)					    /* we need a name for our object */
	genvolname();
    for (o = 0; o < argc; o++) {
	if ((drive = find_drive_by_devname(argv[o])) == NULL) /* doesn't exist */
	    drive = create_drive(argv[o]);		    /* create it */
	/* Now find the largest chunk available on the drive */
	bigchunk = 0;					    /* ain't found nothin' yet */
	for (fe = 0; fe < drive->freelist_entries; fe++) {
	    ferq->driveno = drive->driveno;
	    ferq->fe = fe;
	    if (ioctl(superdev, VINUM_GETFREELIST, &freelist) < 0) {
		fprintf(stderr,
		    "Can't get free list element %d: %s\n",
		    fe,
		    strerror(errno));
		longjmp(command_fail, -1);
	    }
	    bigchunk = bigchunk > freelist.sectors ? bigchunk : freelist.sectors; /* max it */
	}
	maxsize = min(maxsize, bigchunk);		    /* this is as much as we can do */
    }

    /* Now create the volume */
    sprintf(buffer, "volume %s", objectname);
    if (vflag)
	printf("volume %s\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);		    /* create the volume */
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create volume %s: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create volume %s: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    sprintf(buffer, "plex name %s.p0 org raid5 279k", objectname);
    if (vflag)
	printf("  plex name %s.p0 org raid5 279k\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create plex %s.p0: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create plex %s.p0: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    for (o = 0; o < argc; o++) {
	drive = find_drive_by_devname(argv[o]);		    /* we know it exists... */
	sprintf(buffer,
	    "sd name %s.p0.s%d drive %s size %lldb",
	    objectname,
	    o,
	    drive->label.name,
	    (long long) maxsize);
	if (vflag)
	    printf("    sd name %s.p0.s%d drive %s size %lldb\n",
		objectname,
		o,
		drive->label.name,
		(long long) maxsize);
	ioctl(superdev, VINUM_CREATE, buffer);
	if (reply->error != 0) {			    /* error in config */
	    if (reply->msg[0])
		fprintf(stderr,
		    "Can't create subdisk %s.p0.s%d: %s\n",
		    objectname,
		    o,
		    reply->msg);
	    else
		fprintf(stderr,
		    "Can't create subdisk %s.p0.s%d: %s (%d)\n",
		    objectname,
		    o,
		    strerror(reply->error),
		    reply->error);
	    longjmp(command_fail, -1);			    /* give up */
	}
    }

    /* done, save the config */
    ioctltype = 0;					    /* saveconfig after update */
    error = ioctl(superdev, VINUM_SAVECONFIG, &ioctltype);  /* save the config to disk */
    if (error != 0)
	perror("Can't save Vinum config");
    find_object(objectname, &type);			    /* find the index of the volume */
    make_vol_dev(vol.volno, 1);				    /* and create the devices */
    if (vflag) {
	vflag--;					    /* XXX don't give too much detail */
	find_object(objectname, &type);			    /* point to the volume */
	vinum_lvi(vol.volno, 1);			    /* and print info about it */
    }
}

/*
 * Create a volume with a two plexes from as much space
 * as we can get on the specified drives.  If the
 * drives aren't Vinum drives, make them so.
 *
 * The number of drives must be even, and at least 4
 * for a striped plex.  Specify striped plexes with the
 * -s flag; otherwise they will be concatenated.  It's
 * possible that the two plexes may differ in length.
 */
void
vinum_mirror(int argc, char *argv[], char *argv0[])
{
    int o;						    /* object number */
    int p;						    /* plex number */
    char buffer[BUFSIZE];
    struct _drive *drive;				    /* drive we're currently looking at */
    struct _ioctl_reply *reply;
    int ioctltype;
    int error;
    enum objecttype type;
    off_t maxsize[2];					    /* maximum subdisk size for striped plexes */
    int fe;						    /* freelist entry index */
    struct drive_freelist freelist;
    struct ferq {					    /* request to pass to ioctl */
	int driveno;
	int fe;
    } *ferq = (struct ferq *) &freelist;
    u_int64_t bigchunk;					    /* biggest chunk in freelist */

    if (sflag)						    /* striped, */
	maxsize[0] = maxsize[1] = QUAD_MAX;		    /* we need to calculate sd size */
    else
	maxsize[0] = maxsize[1] = 0;			    /* let the kernel routines do it */

    reply = (struct _ioctl_reply *) &buffer;

    /*
     * First, check our drives.
     */
    if (argc & 1) {
	fprintf(stderr, "You need an even number of drives to create a mirrored volume\n");
	return;
    }
    if (sflag && (argc < 4)) {
	fprintf(stderr, "You need at least 4 drives to create a mirrored, striped volume\n");
	return;
    }
    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) {	    /* can't get config? */
	printf("Can't configure: %s (%d)\n", strerror(errno), errno);
	return;
    }
    if (!objectname)					    /* we need a name for our object */
	genvolname();
    for (o = 0; o < argc; o++) {
	if ((drive = find_drive_by_devname(argv[o])) == NULL) /* doesn't exist */
	    drive = create_drive(argv[o]);		    /* create it */
	if (sflag) {					    /* striping, */
	    /* Find the largest chunk available on the drive */
	    bigchunk = 0;				    /* ain't found nothin' yet */
	    for (fe = 0; fe < drive->freelist_entries; fe++) {
		ferq->driveno = drive->driveno;
		ferq->fe = fe;
		if (ioctl(superdev, VINUM_GETFREELIST, &freelist) < 0) {
		    fprintf(stderr,
			"Can't get free list element %d: %s\n",
			fe,
			strerror(errno));
		    longjmp(command_fail, -1);
		}
		bigchunk = bigchunk > freelist.sectors ? bigchunk : freelist.sectors; /* max it */
	    }
	    maxsize[o & 1] = min(maxsize[o & 1], bigchunk); /* get the maximum size of a subdisk  */
	}
    }

    /* Now create the volume */
    sprintf(buffer, "volume %s setupstate", objectname);
    if (vflag)
	printf("volume %s setupstate\n", objectname);
    ioctl(superdev, VINUM_CREATE, buffer);		    /* create the volume */
    if (reply->error != 0) {				    /* error in config */
	if (reply->msg[0])
	    fprintf(stderr,
		"Can't create volume %s: %s\n",
		objectname,
		reply->msg);
	else
	    fprintf(stderr,
		"Can't create volume %s: %s (%d)\n",
		objectname,
		strerror(reply->error),
		reply->error);
	longjmp(command_fail, -1);			    /* give up */
    }
    for (p = 0; p < 2; p++) {				    /* create each plex */
	if (sflag) {
	    sprintf(buffer, "plex name %s.p%d org striped 279k", objectname, p);
	    if (vflag)
		printf("  plex name %s.p%d org striped 279k\n", objectname, p);
	} else {					    /* concat */
	    sprintf(buffer, "plex name %s.p%d org concat", objectname, p);
	    if (vflag)
		printf("  plex name %s.p%d org concat\n", objectname, p);
	}
	ioctl(superdev, VINUM_CREATE, buffer);
	if (reply->error != 0) {			    /* error in config */
	    if (reply->msg[0])
		fprintf(stderr,
		    "Can't create plex %s.p%d: %s\n",
		    objectname,
		    p,
		    reply->msg);
	    else
		fprintf(stderr,
		    "Can't create plex %s.p%d: %s (%d)\n",
		    objectname,
		    p,
		    strerror(reply->error),
		    reply->error);
	    longjmp(command_fail, -1);			    /* give up */
	}
	/* Now look at the subdisks */
	for (o = p; o < argc; o += 2) {			    /* every second one */
	    drive = find_drive_by_devname(argv[o]);	    /* we know it exists... */
	    sprintf(buffer,
		"sd name %s.p%d.s%d drive %s size %lldb",
		objectname,
		p,
		o >> 1,
		drive->label.name,
		(long long) maxsize[p]);
	    if (vflag)
		printf("    sd name %s.p%d.s%d drive %s size %lldb\n",
		    objectname,
		    p,
		    o >> 1,
		    drive->label.name,
		    (long long) maxsize[p]);
	    ioctl(superdev, VINUM_CREATE, buffer);
	    if (reply->error != 0) {			    /* error in config */
		if (reply->msg[0])
		    fprintf(stderr,
			"Can't create subdisk %s.p%d.s%d: %s\n",
			objectname,
			p,
			o >> 1,
			reply->msg);
		else
		    fprintf(stderr,
			"Can't create subdisk %s.p%d.s%d: %s (%d)\n",
			objectname,
			p,
			o >> 1,
			strerror(reply->error),
			reply->error);
		longjmp(command_fail, -1);		    /* give up */
	    }
	}
    }

    /* done, save the config */
    ioctltype = 0;					    /* saveconfig after update */
    error = ioctl(superdev, VINUM_SAVECONFIG, &ioctltype);  /* save the config to disk */
    if (error != 0)
	perror("Can't save Vinum config");
    find_object(objectname, &type);			    /* find the index of the volume */
    make_vol_dev(vol.volno, 1);				    /* and create the devices */
    if (vflag) {
	vflag--;					    /* XXX don't give too much detail */
	sflag = 0;					    /* no stats, please */
	find_object(objectname, &type);			    /* point to the volume */
	vinum_lvi(vol.volno, 1);			    /* and print info about it */
    }
}

void
vinum_readpol(int argc, char *argv[], char *argv0[])
{
    int object;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;
    enum objecttype type;
    struct _plex plex;
    struct _volume vol;
    int plexno;

    if (argc == 0) {					    /* start everything */
	fprintf(stderr, "Usage: readpol <volume> <plex>|round\n");
	return;
    }
    object = find_object(argv[1], &type);		    /* look for it */
    if (type != volume_object) {
	fprintf(stderr, "%s is not a volume\n", argv[1]);
	return;
    }
    get_volume_info(&vol, object);
    if (strcmp(argv[2], "round")) {			    /* not 'round' */
	object = find_object(argv[2], &type);		    /* look for it */
	if (type != plex_object) {
	    fprintf(stderr, "%s is not a plex\n", argv[2]);
	    return;
	}
	get_plex_info(&plex, object);
	plexno = plex.plexno;
    } else						    /* round */
	plexno = -1;

    /* Set the value */
    message->index = vol.volno;
    message->otherobject = plexno;
    if (ioctl(superdev, VINUM_READPOL, message) < 0)
	fprintf(stderr, "Can't set read policy: %s (%d)\n", strerror(errno), errno);
    if (vflag)
	vinum_lpi(plexno, recurse);
}

/*
 * Brute force set state function.  Don't look at
 * any dependencies, just do it.
 */
void
vinum_setstate(int argc, char *argv[], char *argv0[])
{
    int object;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;
    int index;
    enum objecttype type;
    int state;

    for (index = 1; index < argc; index++) {
	object = find_object(argv[index], &type);	    /* look for it */
	if (type == invalid_object)
	    fprintf(stderr, "Can't find object: %s\n", argv[index]);
	else {
	    int doit = 0;				    /* set to 1 if we pass our tests */
	    switch (type) {
	    case drive_object:
		state = DriveState(argv[0]);		    /* get the state */
		if (drive.state == state)		    /* already in that state */
		    fprintf(stderr, "%s is already %s\n", drive.label.name, argv[0]);
		else
		    doit = 1;
		break;

	    case sd_object:
		state = SdState(argv[0]);		    /* get the state */
		if (sd.state == state)			    /* already in that state */
		    fprintf(stderr, "%s is already %s\n", sd.name, argv[0]);
		else
		    doit = 1;
		break;

	    case plex_object:
		state = PlexState(argv[0]);		    /* get the state */
		if (plex.state == state)		    /* already in that state */
		    fprintf(stderr, "%s is already %s\n", plex.name, argv[0]);
		else
		    doit = 1;
		break;

	    case volume_object:
		state = VolState(argv[0]);		    /* get the state */
		if (vol.state == state)			    /* already in that state */
		    fprintf(stderr, "%s is already %s\n", vol.name, argv[0]);
		else
		    doit = 1;
		break;

	    default:
		state = 0;				    /* to keep the compiler happy */
	    }

	    if (state == -1)
		fprintf(stderr, "Invalid state for object: %s\n", argv[0]);
	    else if (doit) {
		message->index = object;		    /* pass object number */
		message->type = type;			    /* and type of object */
		message->state = state;
		message->force = force;			    /* don't force it, use a larger hammer */
		ioctl(superdev, VINUM_SETSTATE_FORCE, message);
		if (reply.error != 0)
		    fprintf(stderr,
			"Can't start %s: %s (%d)\n",
			argv[index],
			reply.msg[0] ? reply.msg : strerror(reply.error),
			reply.error);
		if (Verbose)
		    vinum_li(object, type);
	    }
	}
    }
}

void
vinum_checkparity(int argc, char *argv[], char *argv0[])
{
    Verbose = vflag;					    /* accept -v for verbose */
    if (argc == 0)					    /* no parameters? */
	fprintf(stderr, "Usage: checkparity object [object...]\n");
    else
	parityops(argc, argv, checkparity);
}

void
vinum_rebuildparity(int argc, char *argv[], char *argv0[])
{
    if (argc == 0)					    /* no parameters? */
	fprintf(stderr, "Usage: rebuildparity object [object...]\n");
    else
	parityops(argc, argv, vflag ? rebuildandcheckparity : rebuildparity);
}

/*
 * Common code for rebuildparity and checkparity.
 * We bend the meanings of some flags here:
 *
 * -v: Report incorrect parity on rebuild.
 * -V: Show running count of position being checked.
 * -f: Start from beginning of the plex.
 */
void
parityops(int argc, char *argv[], enum parityop op)
{
    int object;
    struct _plex plex;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;
    int index;
    enum objecttype type;
    char *msg;
    off_t block;

    if (op == checkparity)
	msg = "Checking";
    else
	msg = "Rebuilding";
    for (index = 0; index < argc; index++) {
	object = find_object(argv[index], &type);	    /* look for it */
	if (type != plex_object)
	    fprintf(stderr, "%s is not a plex\n", argv[index]);
	else {
	    get_plex_info(&plex, object);
	    if (!isparity((&plex)))
		fprintf(stderr, "%s is not a RAID-4 or RAID-5 plex\n", argv[index]);
	    else {
		do {
		    message->index = object;		    /* pass object number */
		    message->type = type;		    /* and type of object */
		    message->op = op;			    /* what to do */
		    if (force)
			message->offset = 0;		    /* start at the beginning */
		    else
			message->offset = plex.checkblock;  /* continue where we left off */
		    force = 0;				    /* don't reset after the first time */
		    ioctl(superdev, VINUM_PARITYOP, message);
		    get_plex_info(&plex, object);
		    if (Verbose) {
			block = (plex.checkblock << DEV_BSHIFT) * (plex.subdisks - 1);
			if (block != 0)
			    printf("\r%s at %s (%d%%)    ",
				msg,
				roughlength(block, 1),
				((int) (block * 100 / plex.length) >> DEV_BSHIFT));
			if ((reply.error == EAGAIN)
			    && (reply.msg[0]))		    /* got a comment back */
			    fputs(reply.msg, stderr);	    /* show it */
			fflush(stdout);
		    }
		}
		while (reply.error == EAGAIN);
		if (reply.error != 0) {
		    if (reply.msg[0])
			fputs(reply.msg, stderr);
		    else
			fprintf(stderr,
			    "%s failed: %s\n",
			    msg,
			    strerror(reply.error));
		} else if (Verbose) {
		    if (op == checkparity)
			fprintf(stderr, "%s has correct parity\n", argv[index]);
		    else
			fprintf(stderr, "Rebuilt parity on %s\n", argv[index]);
		}
	    }
	}
    }
}

/* Local Variables: */
/* fill-column: 50 */
/* End: */
