/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $FreeBSD$
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
#include <sys/signal.h>
#include <sys/fcntl.h>

const char *StartName;		/* Initial contents of argv[0] */

static void
screech(int sig)
{
    msgDebug("\007Signal %d caught!  That's bad!\n", sig);
    longjmp(BailOut, sig);
}

int
main(int argc, char **argv)
{
    int choice, scroll, curr, max, status;
    
    /* Record name to be able to restart */
    StartName = argv[0];

    /* Catch fatal signals and complain about them if running as init */
    if (getpid() == 1) {
	signal(SIGBUS, screech);
	signal(SIGSEGV, screech);
    }
    signal(SIGPIPE, SIG_IGN);

    /* We don't work too well when running as non-root anymore */
    if (geteuid() != 0) {
	fprintf(stderr, "Error: This utility should only be run as root.\n");
	return 1;
    }

#ifdef PC98
    {
	/* XXX */
	char *p = getenv("TERM");
	if (p && strcmp(p, "cons25") == 0)
	    putenv("TERM=cons25w");
    }
#endif

    /* Set up whatever things need setting up */
    systemInitialize(argc, argv);

    /* Set default flag and variable values */
    installVarDefaults(NULL);
    /* only when multi-user is it reasonable to do this here */
    if (!RunningAsInit)
	installEnvironment();

    if (argc > 1 && !strcmp(argv[1], "-fake")) {
	variable_set2(VAR_DEBUG, "YES", 0);
	Fake = TRUE;
	msgConfirm("I'll be just faking it from here on out, OK?");
    }
    if (argc > 1 && !strcmp(argv[1], "-restart"))
	Restarting = TRUE;

    /* Try to preserve our scroll-back buffer */
    if (OnVTY) {
	for (curr = 0; curr < 25; curr++)
	    putchar('\n');
    }
    /* Move stderr aside */
    if (DebugFD)
	dup2(DebugFD, 2);

    /* Initialize driver modules, if we haven't already done so (ie,
       the user hit Ctrl-C -> Restart. */
    if (!pvariable_get("modulesInitialize")) {
	moduleInitialize();
	pvariable_set("modulesInitialize=1");
    }

    /* Initialize PC-card, if we haven't already done so. */
#ifdef PCCARD_ARCH
    if (!pvariable_get("pccardInitialize")) {
	pccardInitialize();
	pvariable_set("pccardInitialize=1");
    }
#endif

    /* Initialize USB, if we haven't already done so. */
    if (!pvariable_get("usbInitialize")) {
	usbInitialize();
	pvariable_set("usbInitialize=1");
    }

    /* Probe for all relevant devices on the system */
    deviceGetAll();

    /* First, see if we have any arguments to process (and argv[0] counts if it's not "sysinstall") */
    if (!RunningAsInit) {
	int i, start_arg;

	if (!strstr(argv[0], "sysinstall"))
	    start_arg = 0;
	else if (Fake || Restarting)
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
    else
	dispatch_load_file_int(TRUE);

    status = setjmp(BailOut);
    if (status) {
	msgConfirm("A signal %d was caught - I'm saving what I can and shutting\n"
		   "down.  If you can reproduce the problem, please turn Debug on\n"
		   "in the Options menu for the extra information it provides\n"
		   "in debugging problems like this.", status);
	systemShutdown(status);
    }

    /* Begin user dialog at outer menu */
    dialog_clear();
    while (1) {
	choice = scroll = curr = max = 0;
	dmenuOpen(&MenuInitial, &choice, &scroll, &curr, &max, TRUE);
	if (getpid() != 1
#if defined(__alpha__) || defined(__sparc64__)
	    || !msgNoYes("Are you sure you wish to exit?  The system will halt.")
#else
	    || !msgNoYes("Are you sure you wish to exit?  The system will reboot\n"
		         "(be sure to remove any floppies/CDs/DVDs from the drives).")
#endif
	    )
	    break;
    }

    /* Say goodnight, Gracie */
    systemShutdown(0);

    return 0; /* We should never get here */
}
