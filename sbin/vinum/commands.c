/* commands.c: vinum interface program, main commands */
/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
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
 */

/* $Id: commands.c,v 1.6 1999/03/23 03:40:07 grog Exp grog $ */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libutil.h>
#include <netdb.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dev/vinum/vinumhdr.h>
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
	if (verbose)
	    printf("%4d: %s", file_line, buffer);
	strcpy(commandline, buffer);			    /* make a copy */
	ioctl(superdev, VINUM_CREATE, buffer);
	if (reply->error != 0) {			    /* error in config */
	    if (!verbose)				    /* print this line anyway */
		printf("%4d: %s", file_line, commandline);
	    fprintf(stdout, "** %d %s: %s\n", file_line, reply->msg, strerror(reply->error));
	    /* XXX at the moment, we reset the config
	     * lock on error, so try to get it again.
	     * If we fail, don't cry again */
	    if (ioctl(superdev, VINUM_STARTCONFIG, &force)) /* can't get config? */
		return;
	}
    }
    fclose(dfd);					    /* done with the config file */
    ioctltype = 0;					    /* saveconfig after update */
    error = ioctl(superdev, VINUM_SAVECONFIG, &ioctltype);  /* save the config to disk */
    if (error != 0)
	perror("Can't save Vinum config");
    make_devices();
    listconfig();
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
	make_devices();
    }
}

void 
vinum_volume(int argc, char *argv[], char *arg0[])
{
    int i;
    char *line;
    struct _ioctl_reply *reply;

    line = arg0[0];
    for (i = 0; i < argc; i++)
	line[strlen(line)] = ' ';			    /* remove the blocks */
    ioctl(superdev, VINUM_CREATE, line);
    reply = (struct _ioctl_reply *) line;
    if (reply->error != 0)				    /* error in config */
	fprintf(stdout, "** %d %s: %s\n", file_line, reply->msg, strerror(reply->error));
}

void 
vinum_plex(int argc, char *argv[], char *arg0[])
{
    int i;
    char *line;
    struct _ioctl_reply *reply;

    line = arg0[0];
    for (i = 0; i < argc; i++)
	line[strlen(line)] = ' ';			    /* remove the blocks */
    ioctl(superdev, VINUM_CREATE, line);
    reply = (struct _ioctl_reply *) line;
    if (reply->error != 0)				    /* error in config */
	fprintf(stdout, "** %d %s: %s\n", file_line, reply->msg, strerror(reply->error));
}

void 
vinum_sd(int argc, char *argv[], char *arg0[])
{
    int i;
    char *line;
    struct _ioctl_reply *reply;

    line = arg0[0];
    for (i = 0; i < argc; i++)
	line[strlen(line)] = ' ';			    /* remove the blocks */
    ioctl(superdev, VINUM_CREATE, line);
    reply = (struct _ioctl_reply *) line;
    if (reply->error != 0)				    /* error in config */
	fprintf(stdout, "** %d %s: %s\n", file_line, reply->msg, strerror(reply->error));
}

void 
vinum_drive(int argc, char *argv[], char *arg0[])
{
    int i;
    char *line;
    struct _ioctl_reply *reply;

    line = arg0[0];
    for (i = 0; i < argc; i++)
	line[strlen(line)] = ' ';			    /* remove the blocks */
    ioctl(superdev, VINUM_CREATE, line);
    reply = (struct _ioctl_reply *) line;
    if (reply->error != 0)				    /* error in config */
	fprintf(stdout, "** %d %s: %s\n", file_line, reply->msg, strerror(reply->error));
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
		} else if (verbose)
		    fprintf(stderr, "%s removed\n", argv[index]);
	    }
	}
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
	    make_devices();				    /* recreate the /dev/vinum hierarchy */
	    printf("\b Vinum configuration obliterated\n");
	    start_daemon();				    /* then restart the daemon */
	}
    }
}

/* Initialize a plex */
void 
vinum_init(int argc, char *argv[], char *arg0[])
{
    if (argc > 0) {					    /* initialize plexes */
	int plexindex;
	int sdno;
	int plexno;
	int plexfh = NULL;				    /* file handle for plex */
	pid_t pid;
	enum objecttype type;				    /* type returned */
	struct _ioctl_reply reply;
	struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;
	char filename[MAXPATHLEN];			    /* create a file name here */

	/* Variables for use by children */
	int failed = 0;					    /* set if a child dies badly */
	int sdfh;					    /* and for subdisk */
	char zeros[PLEXINITSIZE];
	int count;					    /* write count */
	long long offset;				    /* offset in subdisk */
	long long sdsize;				    /* size of subdisk */

	if (history)
	    fflush(history);				    /* don't let all the kids do it. */
	for (plexindex = 0; plexindex < argc; plexindex++) {
	    plexno = find_object(argv[plexindex], &type);   /* find the object */
	    if (plexno < 0)
		printf("Can't find %s\n", argv[plexindex]);
	    else if (type != plex_object) {
		/* XXX Consider doing this for all plexes in
		 * a volume, etc. */
		printf("%s is not a plex\n", argv[plexindex]);
		break;
	    } else if (plex.state == plex_unallocated)	    /* not a real plex, */
		printf("%s is not allocated, can't initialize\n", plex.name);
	    else {
		sprintf(filename, VINUM_DIR "/plex/%s", argv[plexindex]);
		if ((plexfh = open(filename, O_RDWR, S_IRWXU)) < 0) { /* got a plex, open it */
							    /* We don't actually write anything to the plex,
		       * since the system will try to format it.  We open
		       * it to ensure that nobody else tries to open it
		       * while we initialize its subdisks */
		    fprintf(stderr, "can't open plex %s: %s\n", filename, strerror(errno));
		    return;
		}
	    }
	    if (dowait == 0) {				    /* don't wait for completion */
		pid = fork();
		if (pid == 0) {				    /* non-waiting parent */
		    close(plexfh);			    /* we don't need this any more */
		    sleep(1);				    /* give them a chance to print */
		    return;				    /* and go on about our business */
		}
	    }
	    /*
	     * If we get here, we're either the first-level child
	     * (if we're not waiting) or we're going to wait.
	     */
	    bzero(zeros, sizeof(zeros));
	    openlog("vinum", LOG_CONS | LOG_PERROR | LOG_PID, LOG_KERN);
	    for (sdno = 0; sdno < plex.subdisks; sdno++) {  /* initialize each subdisk */
		/* We already have the plex data in global
		 * plex from the call to find_object */
		pid = fork();				    /* into the background with you */
		if (pid == 0) {				    /* I'm the child */
		    get_plex_sd_info(&sd, plexno, sdno);
		    sdsize = sd.sectors * DEV_BSIZE;	    /* size of subdisk in bytes */
		    sprintf(filename, VINUM_DIR "/rsd/%s", sd.name);
		    setproctitle("initializing %s", filename); /* show what we're doing */
		    syslog(LOG_INFO | LOG_KERN, "initializing subdisk %s", filename);
		    if ((sdfh = open(filename, O_RDWR, S_IRWXU)) < 0) {	/* no go */
			syslog(LOG_ERR | LOG_KERN,
			    "can't open subdisk %s: %s",
			    filename,
			    strerror(errno));
			exit(1);
		    }
		    for (offset = 0; offset < sdsize; offset += count) {
			count = write(sdfh, zeros, PLEXINITSIZE); /* write a block */
			if (count < 0) {
			    syslog(LOG_ERR | LOG_KERN,
				"can't write subdisk %s: %s",
				filename,
				strerror(errno));
			    exit(1);
			}
							    /* XXX Grrrr why doesn't this thing recognize EOF? */
			else if (count == 0)
			    break;
		    }
		    syslog(LOG_INFO | LOG_KERN, "subdisk %s initialized", filename);
							    /* Bring the subdisk up */
		    message->index = sd.sdno;		    /* pass object number */
		    message->type = sd_object;		    /* and type of object */
		    message->state = object_up;
		    message->force = 1;			    /* insist */
		    ioctl(superdev, VINUM_SETSTATE, message);
		    exit(0);
		} else if (pid < 0)			    /* failure */
		    printf("couldn't fork for subdisk %d: %s", sdno, strerror(errno));
	    }
	    /* Now wait for them to complete */
	    while (1) {
		int status;
		pid = wait(&status);
		if (((int) pid == -1)
		    && (errno == ECHILD))		    /* all gone */
		    break;
		if (WEXITSTATUS(status) != 0) {		    /* oh, oh */
		    printf("child %d exited with status 0x%x\n", pid, WEXITSTATUS(status));
		    failed++;
		}
	    }
	    if (failed == 0) {
		syslog(LOG_INFO | LOG_KERN, "plex %s initialized", plex.name);
	    } else
		syslog(LOG_ERR | LOG_KERN, "couldn't initialize plex %s, %d processes died",
		    plex.name,
		    failed);
	    if (dowait == 0)				    /* we're the waiting child, */
		exit(0);				    /* we've done our dash */
	}
    }
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

	statinfo.dinfo = malloc(devs * sizeof(struct statinfo));
	namelist = malloc(devs * (DEVSTAT_NAME_LEN + 8));
	token = malloc((devs + 1) * sizeof(char *));
	if ((statinfo.dinfo == NULL) || (namelist == NULL) || (token == NULL)) {
	    fprintf(stderr, "Can't allocate memory for drive list\n");
	    return;
	}
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
&&((stat->device_type & DEVSTAT_TYPE_PASS) == 0)	    /* and not passthrough */
	    &&((stat->device_name[0] != '\0'))) {	    /* and it has a name */
		sprintf(enamelist, "/dev/%s%d", stat->device_name, stat->unit_number);
		token[tokens] = enamelist;		    /* point to it */
		tokens++;				    /* one more token */
		enamelist = &enamelist[strlen(enamelist) + 1]; /* and start beyond the end */
	    }
	}
	free(statinfo.dinfo);				    /* don't need the list any more */
	vinum_read(tokens, token, &token[0]);		    /* start the system */
	free(namelist);
	free(token);
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

			for (sdno = 0; sdno < plex.subdisks; sdno++) {
			    get_plex_sd_info(&sd, object, sdno);
			    if ((sd.state >= sd_empty)
				&& (sd.state <= sd_stale)) { /* candidate for init */
				message->index = sd.sdno;   /* pass object number */
				message->type = sd_object;  /* it's a subdisk */
				message->state = object_up;
				message->force = 0;	    /* don't force it, use a larger hammer */
				ioctl(superdev, VINUM_SETSTATE, message);
				if (reply.error != 0) {
				    if (reply.error == EAGAIN) /* we're reviving */
					continue_revive(sd.sdno);
				    else
					fprintf(stderr,
					    "Can't start %s: %s (%d)\n",
					    argv[index],
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
		    message->force = 0;			    /* don't force it, use a larger hammer */
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
}

void 
vinum_stop(int argc, char *argv[], char *arg0[])
{
    int object;
    struct _ioctl_reply reply;
    struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;

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
	struct volume vol;
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
	struct plex plex;
	struct sd sd;
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
	if (supertype != plex_object) {			    /* huh? */
	    fprintf(stderr, "%s can only be attached to a plex\n", objname);
	    return;
	}
	get_plex_info(&plex, supertype);
	if (plex.organization != plex_concat) {		    /* not a cat plex, */
	    fprintf(stderr, "Can't attach subdisks to a %s plex\n", plex_org(plex.organization));
	    return;
	}
	sdno = msg.index;				    /* note the subdisk number for later */
	break;

    case plex_object:
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
	struct plex;
	struct volume;

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
	    vinum_rename_2(sd.name, newname);
	    break;

	case plex_object:
	    get_plex_info(&plex, msg.index);
	    get_volume_info(&vol, plex.volno);
	    for (plexno = 0; plexno < vol.plexes; plexno++) {
		if (vol.plex[plexno] == msg.index)	    /* found our subdisk */
		    break;
	    }
	    sprintf(newname, "%s.p%d", vol.name, plexno);
	    vinum_rename_2(plex.name, newname);		    /* this may recurse */
	    break;

	default:					    /* can't get here */
	}
    }
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
}

#ifdef COMPLETE
/* Replace an object.  Syntax and semantics TBD */
void 
vinum_replace(int argc, char *argv[], char *argv0[])
{
    int maxlen;
    struct vinum_rename_msg msg;
    struct _ioctl_reply *reply = (struct _ioctl_reply *) &msg;

    if (argc != 2) {
	fprintf(stderr, "Usage: \trename <object> <new name>\n");
	return;
    }
    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    fprintf(stderr, "Not implemented yet\n");
}
#endif
/* Replace an object.  Syntax and semantics TBD */
void 
vinum_replace(int argc, char *argv[], char *argv0[])
{
    fprintf(stderr, "replace function not implemented yet\n");
}

/* Primitive help function */
void 
vinum_help(int argc, char *argv[], char *argv0[])
{
    char commands[] =
    {
	"COMMANDS\n"
	"create [-f description-file]\n"
	"          Create a volume as described in description-file\n"
	"attach plex volume [rename]\n"
	"attach subdisk plex [offset] [rename]\n"
	"          Attach a plex to a volume, or a subdisk to a plex.\n"
	"debug\n"
	"          Cause the volume manager to enter the kernel debugger.\n"
	"debug flags\n"
	"          Set debugging flags.\n"
	"detach [plex | subdisk]\n"
	"          Detach a plex or subdisk from the volume or plex to which it is\n"
	"          attached.\n"
	"info [-v]\n"
	"          List information about volume manager state.\n"
	"init [-v] [-w] plex\n"
	"          Initialize a plex by writing zeroes to all its subdisks.\n"
	"label volume\n"
	"          Create a volume label\n"
	"list [-r] [-s] [-v] [-V] [volume | plex | subdisk]\n"
	"          List information about specified objects\n"
	"l [-r] [-s] [-v] [-V] [volume | plex | subdisk]\n"
	"          List information about specified objects (alternative to\n"
	"          list command)\n"
	"ld [-r] [-s] [-v] [-V] [volume]\n"
	"          List information about drives\n"
	"ls [-r] [-s] [-v] [-V] [subdisk]\n"
	"          List information about subdisks\n"
	"lp [-r] [-s] [-v] [-V] [plex]\n"
	"          List information about plexes\n"
	"lv [-r] [-s] [-v] [-V] [volume]\n"
	"          List information about volumes\n"
	"printconfig [file]\n"
	"          Write a copy of the current configuration to file.\n"
	"makedev\n"
	"          Remake the device nodes in /dev/vinum.\n"
	"quit\n"
	"          Exit the vinum program when running in interactive mode.  Nor-\n"
	"          mally this would be done by entering the EOF character.\n"
	"read disk [disk...]\n"
	"          Read the vinum configuration from the specified disks.\n"
	"rename [-r] [drive | subdisk | plex | volume] newname\n"
	"          Change the name of the specified object.\n"
	"resetconfig\n"
	"          Reset the complete vinum configuration.\n"
	"resetstats [-r] [volume | plex | subdisk]\n"
	"          Reset statistisc counters for the specified objects, or for all\n"
	"          objects if none are specified.\n"
	"rm [-f] [-r] volume | plex | subdisk\n"
	"          Remove an object\n"
	"saveconfig\n"
	"          Save vinum configuration to disk.\n"
	"setdaemon [value]\n"
	"          Set daemon configuration.\n"
	"start\n"
	"          Read configuration from all vinum drives.\n"
	"start [volume | plex | subdisk]\n"
	"          Allow the system to access the objects\n"
	"stop [-f] [volume | plex | subdisk]\n"
	"          Terminate access to the objects, or stop vinum if no parameters\n"
	"          are specified.\n"
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
}
