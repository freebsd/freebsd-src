/* vinum.c: vinum interface program */
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

/* $Id: v.c,v 1.25 1999/03/21 01:18:23 grog Exp grog $ */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libutil.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dev/vinum/vinumhdr.h>
#include "vext.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/resource.h>

FILE *cf;						    /* config file handle */

char buffer[BUFSIZE];					    /* buffer to read in to */

int line = 0;						    /* stdin line number for error messages */
int file_line = 0;					    /* and line in input file (yes, this is tacky) */
int inerror;						    /* set to 1 to exit after end of config file */

/* flags */

#if VINUMDEBUG
int debug = 0;						    /* debug flag, usage varies */
#endif
int force = 0;						    /* set to 1 to force some dangerous ops */
int verbose = 0;					    /* set verbose operation */
int Verbose = 0;					    /* set very verbose operation */
int recurse = 0;					    /* set recursion */
int stats = 0;						    /* show statistics */
int dowait = 0;						    /* wait for completion */

/* Structures to read kernel data into */
struct _vinum_conf vinum_conf;				    /* configuration information */

struct volume vol;
struct plex plex;
struct sd sd;
struct drive drive;

jmp_buf command_fail;					    /* return on a failed command */
int superdev;						    /* vinum super device */

void start_daemon(void);

#define ofs(x) ((void *) (& ((struct confdata *) 0)->x))    /* offset of x in struct confdata */

/*   create description-file
   Create a volume as described in description-file
   modify description-file
   Modify the objects as described in description-file
   list [-r] [volume | plex | subdisk]
   List information about specified objects
   set [-f] state volume | plex | subdisk | disk
   Set the state of the object to state
   rm [-f] [-r] volume | plex | subdisk
   Remove an object
   start [volume | plex | subdisk]
   Allow the system to access the objects
   stop [-f] [volume | plex | subdisk]
   Terminate access the objects
 */

char *token[MAXARGS];					    /* pointers to individual tokens */
int tokens;						    /* number of tokens */

int 
main(int argc, char *argv[], char *envp[])
{
#if __FreeBSD__ >= 3
    if (modfind(WRONGMOD) >= 0) {			    /* wrong module loaded, */
	fprintf(stderr, "Wrong module loaded: %s.  Starting %s.\n", VINUMMOD, WRONGMOD);
	argv[0] = "/sbin/" WRONGMOD;
	execve(argv[0], argv, envp);
	exit(1);
    }
    if (modfind(VINUMMOD) < 0) {
	/* need to load the vinum module */
	if (kldload(VINUMMOD) < 0 || modfind(VINUMMOD) < 0) {
	    perror(VINUMMOD ": Kernel module not available");
	    return 1;
	}
    }
#endif

    superdev = open(VINUM_SUPERDEV_NAME, O_RDWR);	    /* open vinum superdevice */
    if (superdev < 0) {					    /* no go */
	if (errno == ENODEV) {				    /* not configured, */
	    superdev = open(VINUM_WRONGSUPERDEV_NAME, O_RDWR); /* do we have a debug mismatch? */
	    if (superdev >= 0) {			    /* yup! */
#if VINUMDEBUG
		fprintf(stderr,
		    "This program is compiled with debug support, but the kernel module does\n"
		    "not have debug support.  This program must be matched with the kernel\n"
		    "module.  Please alter /usr/src/sbin/" VINUMMOD "/Makefile and remove\n"
		    "the option -DVINUMDEBUG from the CFLAGS definition, or alternatively\n"
		    "edit /usr/src/sys/modules/" VINUMMOD "/Makefile and add the option\n"
		    "-DVINUMDEBUG to the CFLAGS definition.  Then rebuild the component\n"
		    "of your choice with 'make clean all install'.  If you rebuild the kernel\n"
		    "module, you must stop " VINUMMOD " and restart it\n");
#else
		fprintf(stderr,
		    "This program is compiled without debug support, but the kernel module\n"
		    "includes debug support.  This program must be matched with the kernel\n"
		    "module.  Please alter /usr/src/sbin/" VINUMMOD "/Makefile and add\n"
		    "the option -DVINUMDEBUG to the CFLAGS definition, or alternatively\n"
		    "edit /usr/src/sys/modules/" VINUMMOD "/Makefile and remove the option\n"
		    "-DVINUMDEBUG from the CFLAGS definition.  Then rebuild the component\n"
		    "of your choice with 'make clean all install'.  If you rebuild the kernel\n"
		    "module, you must stop " VINUMMOD " and restart it\n");
#endif
		return 1;
	    }
	} else if (errno == ENOENT)			    /* we don't have our node, */
	    make_devices();				    /* create them first */
	if (superdev < 0) {
	    perror("Can't open " VINUM_SUPERDEV_NAME);
	    return 1;
	}
    }
    /* Check if the dæmon is running.  If not, start it in the
     * background */
    start_daemon();

    if (argc > 1) {					    /* we have a command on the line */
	if (setjmp(command_fail) != 0)			    /* long jumped out */
	    return -1;
	parseline(argc - 1, &argv[1]);			    /* do it */
    } else {
	for (;;) {					    /* ugh */
	    char *c;
	    int childstatus;				    /* from wait4 */

	    setjmp(command_fail);			    /* come back here on catastrophic failure */

	    while (wait4(-1, &childstatus, WNOHANG, NULL) > 0);	/* wait for all dead children */
	    c = readline(VINUMMOD " -> ");		    /* get an input */
	    if (c == NULL) {				    /* EOF or error */
		if (ferror(stdin)) {
		    fprintf(stderr, "Can't read input: %s (%d)\n", strerror(errno), errno);
		    return 1;
		} else {				    /* EOF */
		    printf("\n");
		    return 0;
		}
	    } else if (*c) {				    /* got something there */
		add_history(c);				    /* save it in the history */
		strcpy(buffer, c);			    /* put it where we can munge it */
		free(c);
		line++;					    /* count the lines */
		tokens = tokenize(buffer, token);
		/* got something potentially worth parsing */
		if (tokens)
		    parseline(tokens, token);		    /* and do what he says */
	    }
	}
    }
    return 0;						    /* normal completion */
}

/* stop the hard way */
void 
vinum_quit(int argc, char *argv[], char *argv0[])
{
    exit(0);
}

#define FUNKEY(x) { kw_##x, &vinum_##x }		    /* create pair "kw_foo", vinum_foo */

struct funkey {
    enum keyword kw;
    void (*fun) (int argc, char *argv[], char *arg0[]);
} funkeys[] = {

    FUNKEY(create),
	FUNKEY(read),
#ifdef VINUMDEBUG
	FUNKEY(debug),
#endif
	FUNKEY(volume),
	FUNKEY(plex),
	FUNKEY(sd),
	FUNKEY(drive),
	FUNKEY(modify),
	FUNKEY(list),
	FUNKEY(ld),
	FUNKEY(ls),
	FUNKEY(lp),
	FUNKEY(lv),
	FUNKEY(info),
	FUNKEY(set),
	FUNKEY(init),
	FUNKEY(label),
	FUNKEY(resetconfig),
	FUNKEY(rm),
	FUNKEY(attach),
	FUNKEY(detach),
	FUNKEY(rename),
	FUNKEY(replace),
	FUNKEY(printconfig),
	FUNKEY(saveconfig),
	FUNKEY(start),
	FUNKEY(stop),
	FUNKEY(makedev),
	FUNKEY(help),
	FUNKEY(quit),
	FUNKEY(setdaemon),
	FUNKEY(resetstats)
};

/* Take args arguments at argv and attempt to perform the operation specified */
void 
parseline(int args, char *argv[])
{
    int i;
    int j;
    enum keyword command;				    /* command to execute */

    if ((args == 0)					    /* empty line */
    ||(*argv[0] == '#'))				    /* or a comment, */
	return;
    if (args == MAXARGS) {				    /* too many arguments, */
	fprintf(stderr, "Too many arguments to %s, this can't be right\n", argv[0]);
	return;
    }
    command = get_keyword(argv[0], &keyword_set);
    force = 0;						    /* initialize flags */
    verbose = 0;					    /* initialize flags */
    Verbose = 0;					    /* initialize flags */
    recurse = 0;					    /* initialize flags */
    stats = 0;						    /* initialize flags */
    /* First handle generic options */
    for (i = 1; (i < args) && (argv[i][0] == '-'); i++) {   /* while we have flags */
	for (j = 1; j < strlen(argv[i]); j++)
	    switch (argv[i][j]) {
#if VINUMDEBUG
	    case 'd':					    /* -d: debug */
		debug = 1;
		break;
#endif

	    case 'f':					    /* -f: force */
		force = 1;
		break;

	    case 'v':					    /* -v: verbose */
		verbose++;
		break;

	    case 'V':					    /* -V: Very verbose */
		verbose++;
		Verbose++;
		break;

	    case 'r':					    /* -r: recurse */
		recurse = 1;
		break;

	    case 's':					    /* -s: show statistics */
		stats = 1;
		break;

	    case 'w':					    /* -w: wait for completion */
		dowait = 1;
		break;

	    default:
		fprintf(stderr, "Invalid flag: %s\n", argv[i]);
	    }
    }

    /* Pass what we have left to the command to handle it */
    for (j = 0; j < (sizeof(funkeys) / sizeof(struct funkey)); j++) {
	if (funkeys[j].kw == command) {			    /* found the command */
	    funkeys[j].fun(args - i, &argv[i], &argv[0]);
	    return;
	}
    }
    fprintf(stderr, "Unknown command: %s\n", argv[0]);
}

void 
get_drive_info(struct drive *drive, int index)
{
    *(int *) drive = index;				    /* put in drive to hand to driver */
    if (ioctl(superdev, VINUM_DRIVECONFIG, drive) < 0) {
	fprintf(stderr,
	    "Can't get config for drive %d: %s\n",
	    index,
	    strerror(errno));
	longjmp(command_fail, -1);
    }
}

void 
get_sd_info(struct sd *sd, int index)
{
    *(int *) sd = index;				    /* put in sd to hand to driver */
    if (ioctl(superdev, VINUM_SDCONFIG, sd) < 0) {
	fprintf(stderr,
	    "Can't get config for subdisk %d: %s\n",
	    index,
	    strerror(errno));
	longjmp(command_fail, -1);
    }
}

/* Get the contents of the sd entry for subdisk <sdno>
 * of the specified plex. */
void 
get_plex_sd_info(struct sd *sd, int plexno, int sdno)
{
    ((int *) sd)[0] = plexno;
    ((int *) sd)[1] = sdno;				    /* pass parameters */
    if (ioctl(superdev, VINUM_PLEXSDCONFIG, sd) < 0) {
	fprintf(stderr,
	    "Can't get config for subdisk %d (part of plex %d): %s\n",
	    sdno,
	    plexno,
	    strerror(errno));
	longjmp(command_fail, -1);
    }
}

void 
get_plex_info(struct plex *plex, int index)
{
    *(int *) plex = index;				    /* put in plex to hand to driver */
    if (ioctl(superdev, VINUM_PLEXCONFIG, plex) < 0) {
	fprintf(stderr,
	    "Can't get config for plex %d: %s\n",
	    index,
	    strerror(errno));
	longjmp(command_fail, -1);
    }
}

void 
get_volume_info(struct volume *volume, int index)
{
    *(int *) volume = index;				    /* put in volume to hand to driver */
    if (ioctl(superdev, VINUM_VOLCONFIG, volume) < 0) {
	fprintf(stderr,
	    "Can't get config for volume %d: %s\n",
	    index,
	    strerror(errno));
	longjmp(command_fail, -1);
    }
}

/* Create the device nodes for vinum objects */
void 
make_devices(void)
{
    int volno;
    int plexno;
    int sdno;
    int driveno;

    char filename[PATH_MAX];				    /* for forming file names */

    if (access("/dev", W_OK) < 0) {			    /* can't access /dev to write? */
	if (errno == EROFS)				    /* because it's read-only, */
	    fprintf(stderr, VINUMMOD ": /dev is mounted read-only, not rebuilding " VINUM_DIR "\n");
	else
	    perror(VINUMMOD ": Can't write to /dev");
	return;
    }
    if (superdev >= 0)					    /* super device open */
	close(superdev);

    system("rm -rf " VINUM_DIR " " VINUM_RDIR);		    /* remove the old directories */
    system("mkdir -p " VINUM_DIR "/drive "		    /* and make them again */
	VINUM_DIR "/plex "
	VINUM_DIR "/sd "
	VINUM_DIR "/rsd "
	VINUM_DIR "/vol "
	VINUM_DIR "/rvol "
	VINUM_RDIR);

    if (mknod(VINUM_SUPERDEV_NAME,
	    S_IRWXU | S_IFBLK,				    /* block device, user only */
	    VINUM_SUPERDEV) < 0)
	fprintf(stderr, "Can't create %s: %s\n", VINUM_SUPERDEV_NAME, strerror(errno));

    if (mknod(VINUM_WRONGSUPERDEV_NAME,
	    S_IRWXU | S_IFBLK,				    /* block device, user only */
	    VINUM_WRONGSUPERDEV) < 0)
	fprintf(stderr, "Can't create %s: %s\n", VINUM_WRONGSUPERDEV_NAME, strerror(errno));

    superdev = open(VINUM_SUPERDEV_NAME, O_RDWR);	    /* open the super device */

    if (mknod(VINUM_DAEMON_DEV_NAME,			    /* daemon super device */
	    S_IRWXU | S_IFBLK,				    /* block device, user only */
	    VINUM_DAEMON_DEV) < 0)
	fprintf(stderr, "Can't create %s: %s\n", VINUM_DAEMON_DEV_NAME, strerror(errno));

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    /* First, create directories for the volumes */
    for (volno = 0; volno < vinum_conf.volumes_used; volno++) {
	dev_t voldev;
	dev_t rvoldev;

	get_volume_info(&vol, volno);
	if (vol.state != volume_unallocated) {		    /* we could have holes in our lists */
	    voldev = VINUMBDEV(volno, 0, 0, VINUM_VOLUME_TYPE);	/* create a block device number */
	    rvoldev = VINUMCDEV(volno, 0, 0, VINUM_VOLUME_TYPE); /* and a character device */

	    /* Create /dev/vinum/<myvol> */
	    sprintf(filename, VINUM_DIR "/%s", vol.name);
	    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFBLK, voldev) < 0)
		fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

	    /* Create /dev/rvinum/<myvol> */
	    sprintf(filename, VINUM_RDIR "/%s", vol.name);
	    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFCHR, rvoldev) < 0)
		fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

	    /* Create /dev/vinum/r<myvol> XXX until we fix fsck and friends */
	    sprintf(filename, VINUM_DIR "/r%s", vol.name);
	    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFCHR, rvoldev) < 0)
		fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

	    /* Create /dev/vinum/vol/<myvol> */
	    sprintf(filename, VINUM_DIR "/vol/%s", vol.name);
	    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFBLK, voldev) < 0)
		fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

	    /* Create /dev/vinum/rvol/<myvol> */
	    sprintf(filename, VINUM_DIR "/rvol/%s", vol.name);
	    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFCHR, rvoldev) < 0)
		fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

	    /* Create /dev/vinum/vol/<myvol>.plex/ */
	    sprintf(filename, VINUM_DIR "/vol/%s.plex", vol.name);
	    if (mkdir(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

	    /* Now create device entries for the plexes in
	     * /dev/vinum/<vol>.plex/ and /dev/vinum/plex */
	    for (plexno = 0; plexno < vol.plexes; plexno++) {
		dev_t plexdev;

		get_plex_info(&plex, vol.plex[plexno]);
		if (plex.state != plex_unallocated) {
		    plexdev = VINUMBDEV(volno, plexno, 0, VINUM_PLEX_TYPE);

							    /* Create device /dev/vinum/vol/<vol>.plex/<plex> */
		    sprintf(filename, VINUM_DIR "/vol/%s.plex/%s", vol.name, plex.name);
		    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFBLK, plexdev) < 0)
			fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

							    /* And /dev/vinum/plex/<plex> */
		    sprintf(filename, VINUM_DIR "/plex/%s", plex.name);
		    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFBLK, plexdev) < 0)
			fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

							    /* Create directory /dev/vinum/vol/<vol>.plex/<plex>.sd */
		    sprintf(filename, VINUM_DIR "/vol/%s.plex/%s.sd", vol.name, plex.name);
		    if (mkdir(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
			fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

							    /* Create the contents of /dev/vinum/<vol>.plex/<plex>.sd */
		    for (sdno = 0; sdno < plex.subdisks; sdno++) {
			dev_t sddev;

			get_plex_sd_info(&sd, vol.plex[plexno], sdno);
			if (sd.state != sd_unallocated) {
			    sddev = VINUMBDEV(volno, plexno, sdno, VINUM_SD_TYPE);

							    /* Create /dev/vinum/vol/<vol>.plex/<plex>.sd/<sd> */
			    sprintf(filename, VINUM_DIR "/vol/%s.plex/%s.sd/%s", vol.name, plex.name, sd.name);
			    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFBLK, sddev) < 0)
				fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

							    /* /dev/vinum/sd/<sd> */
			    sprintf(filename, VINUM_DIR "/sd/%s", sd.name);
			    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFBLK, sddev) < 0)
				fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));

							    /* And /dev/vinum/rsd/<sd> */
			    sprintf(filename, VINUM_DIR "/rsd/%s", sd.name);
			    sddev = VINUMCDEV(volno, plexno, sdno, VINUM_SD_TYPE);
			    if (mknod(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IFCHR, sddev) < 0)
				fprintf(stderr, "Can't create %s: %s\n", filename, strerror(errno));
			}
		    }
		}
	    }
	}
    }

    /* Drives.  Do this later (both logical and physical names) XXX */
    for (driveno = 0; driveno < vinum_conf.drives_used; driveno++) {
	get_drive_info(&drive, driveno);
	if (drive.state != drive_unallocated) {
	    sprintf(filename, "ln -s %s " VINUM_DIR "/drive/%s", drive.devicename, drive.label.name);
	    system(filename);
	}
    }
}

/* command line interface for the 'makedev' command */
void 
vinum_makedev(int argc, char *argv[], char *arg0[])
{
    make_devices();
}

/*
 * Find the object "name".  Return object type at type,
 * and the index as the return value.
 * If not found, return -1 and invalid_object.
 */
int 
find_object(const char *name, enum objecttype *type)
{
    int object;

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	*type = invalid_object;
	return -1;
    }
    /* Search the drive table */
    for (object = 0; object < vinum_conf.drives_used; object++) {
	get_drive_info(&drive, object);
	if (strcmp(name, drive.label.name) == 0) {
	    *type = drive_object;
	    return object;
	}
    }

    /* Search the subdisk table */
    for (object = 0; object < vinum_conf.subdisks_used; object++) {
	get_sd_info(&sd, object);
	if (strcmp(name, sd.name) == 0) {
	    *type = sd_object;
	    return object;
	}
    }

    /* Search the plex table */
    for (object = 0; object < vinum_conf.plexes_used; object++) {
	get_plex_info(&plex, object);
	if (strcmp(name, plex.name) == 0) {
	    *type = plex_object;
	    return object;
	}
    }

    /* Search the volume table */
    for (object = 0; object < vinum_conf.volumes_used; object++) {
	get_volume_info(&vol, object);
	if (strcmp(name, vol.name) == 0) {
	    *type = volume_object;
	    return object;
	}
    }

    /* Didn't find the name: invalid */
    *type = invalid_object;
    return -1;
}

/* Continue reviving a subdisk in the background */
void 
continue_revive(int sdno)
{
    struct sd sd;
    pid_t pid;
    get_sd_info(&sd, sdno);

#if VINUMDEBUG
    if (debug)
	pid = 0;					    /* wander through into the "child" process */
    else
	pid = fork();					    /* do this in the background */
#else
    pid = fork();					    /* do this in the background */
#endif
    if (pid == 0) {					    /* we're the child */
	struct _ioctl_reply reply;
	struct vinum_ioctl_msg *message = (struct vinum_ioctl_msg *) &reply;

	openlog(VINUMMOD, LOG_CONS | LOG_PERROR | LOG_PID, LOG_KERN);
	syslog(LOG_INFO | LOG_KERN, "reviving %s", sd.name);

	for (reply.error = EAGAIN; reply.error == EAGAIN;) {
	    message->index = sdno;			    /* pass sd number */
	    message->type = sd_object;			    /* and type of object */
	    message->state = object_up;
	    ioctl(superdev, VINUM_SETSTATE, message);
	}
	if (reply.error) {
	    syslog(LOG_ERR | LOG_KERN,
		"can't revive %s: %s",
		sd.name,
		reply.msg[0] ? reply.msg : strerror(reply.error));
	    exit(1);
	} else {
	    get_sd_info(&sd, sdno);			    /* update the info */
	    syslog(LOG_INFO | LOG_KERN, "%s is %s", sd.name, sd_state(sd.state));
	    exit(0);
	}
    } else if (pid < 0)					    /* couldn't fork? */
	fprintf(stderr, "Can't continue reviving %s: %s\n", sd.name, strerror(errno));
    else
	printf("Reviving %s in the background\n", sd.name);
}

/*
 * Check if the daemon is running,
 * start it if it isn't.  The check itself
 * could take a while, so we do it as a separate
 * process, which will become the daemon if one isn't
 * running already
 */
void 
start_daemon(void)
{
    int pid;
    int status;
    int error;

    pid = (int) fork();

    if (pid == 0) {					    /* We're the child, do the work */
	/*
	 * We have a problem when stopping the subsystem:
	 * The only way to know that we're idle is when
	 * all open superdevs close.  But we want the
	 * daemon to clean up for us, and since we can't
	 * count the opens, we need to have the main device
	 * closed when we stop.  We solve this conundrum
	 * by getting the daemon to open a separate device.
	 */
	close(superdev);				    /* this is the wrong device */
	superdev = open(VINUM_DAEMON_DEV_NAME, O_RDWR);	    /* open deamon superdevice */
	if (superdev < 0) {
	    perror("Can't open " VINUM_DAEMON_DEV_NAME);
	    exit(1);
	}
	error = daemon(0, 0);				    /* this will fork again, but who's counting? */
	if (error != 0) {
	    fprintf(stderr, "Can't start daemon: %s (%d)\n", strerror(errno), errno);
	    exit(1);
	}
	setproctitle(VINUMMOD " daemon");		    /* show what we're doing */
	status = ioctl(superdev, VINUM_FINDDAEMON, NULL);
	if (status != 0) {				    /* no daemon, */
	    ioctl(superdev, VINUM_DAEMON, &verbose);	    /* we should hang here */
	    syslog(LOG_ERR | LOG_KERN, "%s", strerror(errno));
	    exit(1);
	}
	exit(0);					    /* when told to die */
    } else if (pid < 0)					    /* couldn't fork */
	printf("Can't fork to check daemon\n");
}
