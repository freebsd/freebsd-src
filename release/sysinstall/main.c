/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id: main.c,v 1.29 1996/12/11 09:35:02 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <stdio.h>
#include <sys/signal.h>
#include <sys/fcntl.h>

static void
screech(int sig)
{
    fprintf(stderr, "\007Fatal signal %d caught!  I'm dead..\n", sig);
    if (RunningAsInit)
	pause();
    else
	exit(1);
}

int
main(int argc, char **argv)
{
    int choice, scroll, curr, max;

    /* Catch fatal signals and complain about them if running as init */
    if (getpid() == 1) {
	signal(SIGBUS, screech);
	signal(SIGSEGV, screech);
    }

    /* We don't work too well when running as non-root anymore */
    if (geteuid() != 0) {
	fprintf(stderr, "Error: This utility should only be run as root.\n");
	return 1;
    }

    /* Set up whatever things need setting up */
    systemInitialize(argc, argv);

    /* Set default flag and variable values */
    installVarDefaults(NULL);

    if (argc > 1 && !strcmp(argv[1], "-fake")) {
	variable_set2(VAR_DEBUG, "YES");
	Fake = TRUE;
	msgConfirm("I'll be just faking it from here on out, OK?");
    }

    /* Try to preserve our scroll-back buffer */
    if (OnVTY)
	for (curr = 0; curr < 25; curr++)
	    putchar('\n');

    /* Probe for all relevant devices on the system */
    deviceGetAll();

    /* First, see if we have any arguments to process (and argv[0] counts if it's not "sysinstall") */
    if (!RunningAsInit) {
	int i, start_arg;

	/* Try to set ourselves up as a CDROM if we can do that first */
	if (DITEM_STATUS(mediaSetCDROM(NULL)) == DITEM_SUCCESS) {
	    /* If we can't initialize it, it's probably not a FreeBSD CDROM so punt on it */
	    if (!mediaDevice->init(mediaDevice))
		mediaDevice = NULL;
	}

	if (!strstr(argv[0], "sysinstall"))
	    start_arg = 0;
	else if (Fake)
	    start_arg = 2;
	else
	    start_arg = 1;
	for (i = start_arg; i < argc; i++) {
	    if (DITEM_STATUS(dispatchCommand(argv[i])) != DITEM_SUCCESS)
		systemShutdown(1);
	}
	if (argc > start_arg)
	    systemShutdown(0);
    }

    {
	FILE *fp;
	Attribs *attrs;

	attrs = safe_malloc(sizeof(Attribs) * MAX_ATTRIBS);
	fp = fopen("install.cfg", "r");
	if (fp) {
	    msgNotify("Loading pre-configuration file");
	    if (DITEM_STATUS(attr_parse(attrs, fp)) == DITEM_SUCCESS) {
		int i;

		for (i = 0; attrs[i].name; i++)
		    variable_set2(attrs[i].name, attrs[i].value);
	    }
	    fclose(fp);
	    attr_free(attrs);
	}

#if defined(LOAD_CONFIG_FILE)
	else {
	    /* If we have a compiled-in startup config file name on
	       the floppy, look for it and try to load it on startup */
	    extern char *distWanted;

	    /* Tell mediaSetFloppy() to try floppy now */
	    distWanted = (char *)1;

	    /* Try to open the floppy drive if we can do that first */
	    if (DITEM_STATUS(mediaSetFloppy(NULL)) != DITEM_FAILURE &&
		mediaDevice->init(mediaDevice)) {
		int fd;

		fp = mediaDevice->get(mediaDevice, LOAD_CONFIG_FILE, TRUE);
		if (fp) {
		    msgNotify("Loading %s pre-configuration file",
			      LOAD_CONFIG_FILE);
		    if (DITEM_STATUS(attr_parse(attrs, fp)) == DITEM_SUCCESS) {
			int i;

			for (i = 0; attrs[i].name; i++)
			    variable_set2(attrs[i].name, attrs[i].value);
		    }
		    fclose(fp);
		    attr_free(attrs);
		}
		mediaDevice->shutdown(mediaDevice);
	    }
	}
#endif
    }
 
    /* Begin user dialog at outer menu */
    dialog_clear();
    while (1) {
	choice = scroll = curr = max = 0;
	dmenuOpen(&MenuInitial, &choice, &scroll, &curr, &max, TRUE);
	if (getpid() != 1 || !msgYesNo("Are you sure you wish to exit?  The system will reboot\n"
				       "(be sure to remove any floppies from the drives)."))
	    break;
    }

    /* Say goodnight, Gracie */
    systemShutdown(0);

    return 0; /* We should never get here */
}
