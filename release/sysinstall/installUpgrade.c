/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: installUpgrade.c,v 1.2 1995/10/19 18:37:46 jkh Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>

typedef struct _hitList {
    enum { JUST_COPY, CALL_HANDLER } action ;
    char *name;
    Boolean optional;
    void (*handler)(struct _hitList *self);
} HitList;

/* cop-out function for files we can't handle */
static void
doByHand(HitList *h)
{
    msgConfirm("/etc/%s is one of those files that this upgrade procedure just isn't\n"
	       "smart enough to deal with right now.  You'll need to merge the old and\n"
	       "new versions by hand when the option to do so is later presented.", h->name);
}

/* These are the only meaningful files I know about */
static HitList etc_files [] = {
   { JUST_COPY,		"Xaccel.ini",		TRUE, NULL },
   { JUST_COPY,		"adduser.conf",		TRUE, NULL },
   { JUST_COPY,		"aliases",		TRUE, NULL },
   { JUST_COPY,		"aliases.db",		TRUE, NULL },
   { JUST_COPY,		"amd.map",		TRUE, NULL },
   { JUST_COPY,		"crontab",		TRUE, NULL },
   { JUST_COPY,		"csh.cshrc",		TRUE, NULL },
   { JUST_COPY,		"csh.login",		TRUE, NULL },
   { JUST_COPY,		"csh.logout",		TRUE, NULL },
   { JUST_COPY,		"daily",		TRUE, NULL },
   { JUST_COPY,		"disktab",		TRUE, NULL },
   { JUST_COPY,		"dm.conf",		TRUE, NULL },
   { JUST_COPY,		"exports",		TRUE, NULL },
   { JUST_COPY,		"fbtab",		TRUE, NULL },
   { CALL_HANDLER,	"fstab",		FALSE, doByHand },
   { JUST_COPY,		"ftpusers",		TRUE, NULL },
   { JUST_COPY,		"gnats",		TRUE, NULL },
   { JUST_COPY,		"group",		FALSE, NULL },
   { JUST_COPY,		"host.conf",		TRUE, NULL },
   { JUST_COPY,		"hosts",		TRUE, NULL },
   { JUST_COPY,		"hosts.equiv",		TRUE, NULL },
   { JUST_COPY,		"hosts.lpd",		TRUE, NULL },
   { CALL_HANDLER,	"inetd.conf",		FALSE, doByHand },
   { CALL_HANDLER,	"kerberosIV",		TRUE, doByHand },
   { JUST_COPY,		"localtime",		TRUE, NULL },
   { JUST_COPY,		"login.access",		TRUE, NULL },
   { JUST_COPY,		"mail.rc",		TRUE, NULL },
   { JUST_COPY,		"make.conf",		TRUE, NULL },
   { JUST_COPY,		"manpath.config",	TRUE, NULL },
   { JUST_COPY,		"master.passwd",	TRUE, NULL },
   { JUST_COPY,		"mib.txt",		TRUE, NULL },
   { JUST_COPY,		"modems",		TRUE, NULL },
   { JUST_COPY,		"monthly",		TRUE, NULL },
   { JUST_COPY,		"motd",			TRUE, NULL },
   { JUST_COPY,		"namedb",		TRUE, NULL },
   { CALL_HANDLER,	"netstart",		FALSE, doByHand },
   { JUST_COPY,		"networks",		TRUE, NULL },
   { JUST_COPY,		"passwd",		FALSE, NULL },
   { JUST_COPY,		"phones",		TRUE, NULL },
   { JUST_COPY,		"ppp",			TRUE, NULL },
   { JUST_COPY,		"printcap",		TRUE, NULL },
   { JUST_COPY,		"profile",		TRUE, NULL },
   { JUST_COPY,		"protocols",		TRUE, NULL },
   { JUST_COPY,		"pwd.db",		TRUE, NULL },
   { CALL_HANDLER,	"rc",			FALSE, doByHand },
   { CALL_HANDLER,	"rc.i386",		TRUE, doByHand },
   { JUST_COPY,		"rc.local",		TRUE, NULL },
   { CALL_HANDLER,	"rc.serial",		TRUE, doByHand },
   { JUST_COPY,		"remote",		TRUE, NULL },
   { JUST_COPY,		"resolv.conf",		TRUE, NULL },
   { JUST_COPY,		"rmt",			TRUE, NULL },
   { JUST_COPY,		"security",		TRUE, NULL },
   { JUST_COPY,		"sendmail.cf",		TRUE, NULL },
   { CALL_HANDLER,	"services",		TRUE, doByHand },
   { JUST_COPY,		"shells",		TRUE, NULL },
   { JUST_COPY,		"skeykeys",		TRUE, NULL },
   { JUST_COPY,		"spwd.db",		TRUE, NULL },
   { JUST_COPY,		"supfile",		TRUE, NULL },
   { CALL_HANDLER,	"sysconfig",		FALSE, doByHand },
   { JUST_COPY,		"syslog.conf",		TRUE, NULL },
   { JUST_COPY,		"termcap",		TRUE, NULL },
   { JUST_COPY,		"ttys",			TRUE, NULL },
   { JUST_COPY,		"uucp",			TRUE, NULL },
   { JUST_COPY,		"weekly",		TRUE, NULL },
   { 0 },
};

void
traverseHitlist(HitList *h)
{
    while (h->name) {
	if (!file_readable(h->name)) {
	    if (!h->optional)
		msgConfirm("Unable to find an old /etc/%s file!  That is decidedly non-standard and\n"
			   "your upgraded system may function a little strangely as a result.");
	}
	else {
	    if (h->action == JUST_COPY) {
		/* Nuke the just-loaded copy thoroughly */
		vsystem("rm -rf /etc/%s", h->name);

		/* Copy the old one into its place */
		msgNotify("Resurrecting %s..", h->name);
		/* Do this with tar so that symlinks and such are preserved */
		if (vsystem("tar cf - %s | tar xpf - -C /etc", h->name))
		    msgConfirm("Unable to resurrect your old /etc/%s!  Hmmmm.", h->name);
	    }
	    else /* call handler */
		h->handler(h);
	}
	++h;
    }
}
		
int
installUpgrade(char *str)
{
    char *saved_etc = NULL;
    Boolean extractingBin = TRUE;
    int waitstatus;
    pid_t child;

    if (!RunningAsInit) {
	msgConfirm("You can only perform this procedure when booted off the installation\n"
		   "floppy.");
	return RET_FAIL;
    }

    if (!Dists) {
	msgConfirm("You haven't specified any distributions yet.  The upgrade procedure\n"
		   "will only upgrade those portions of the system for which a distribution\n"
		   "has been selected.  In the next screen, we'll go to the Distributions\n"
		   "menu to select those portions of 2.1 you wish to install on top of your\n"
		   "2.0.5 system.");
	if (!dmenuOpenSimple(&MenuDistributions))
	    return RET_FAIL;
    }

    /* No bin selected?  Not much of an upgrade.. */
    if (!(Dists & DIST_BIN)) {
	if (msgYesNo("You didn't select the bin distribution as one of the distributons to load.\n"
		     "This one is pretty vital to a successful 2.1 upgrade.  Are you SURE you don't\n"
		     "want to select the bin distribution?  Chose _No_ to bring up the Distributions\n"
		     "menu.")) {
	    (void)dmenuOpenSimple(&MenuDistributions);
	}
    }

    /* Still?!  OK!  They must know what they're doing.. */
    if (!(Dists & DIST_BIN))
	extractingBin = FALSE;

    systemDisplayHelp("upgrade");

    if (msgYesNo("Given all that scary stuff you just read, are you sure you want to\n"
		 "risk it all and proceed with this upgrade?"))
	return RET_FAIL;

    /* Note that we're now upgrading */
    variable_set2(SYSTEM_INSTALLED, "upgrade");

    msgConfirm("OK.  First, we're going to go to the disk label editor.  In this editor\n"
	       "you will be expected to *Mount* any partitions you're interested in\n"
	       "upgrading.  Don't set the Newfs flag to Y on anything in the label editor\n"
	       "unless you're absolutely sure you know what you're doing!  In this\n"
	       "instance, you'll be using the label editor as little more than a fancy\n"
	       "screen-oriented filesystem mounting utility, so think of it that way.\n\n"
	       "Once you're done in the label editor, press Q to return here for the next\n"
	       "step.\n");

    if (diskLabelEditor(NULL) == RET_FAIL) {
	msgConfirm("The disk label editor failed to work properly!  Upgrade operation\n"
		   "aborted.");
	return RET_FAIL;
    }

    /* Don't write out MBR info */
    variable_set2(DISK_PARTITIONED, "written");
    if (diskLabelCommit(NULL) == RET_FAIL) {
	msgConfirm("Not all file systems were properly mounted.  Upgrade operation\n"
		   "aborted.");
	variable_unset(DISK_PARTITIONED);
	return RET_FAIL;
    }

    if (chroot("/mnt") == RET_FAIL) {
	msgConfirm("Unable to chroot to /mnt - something is wrong with the\n"
		   "root partition or the way it's mounted if this doesn't work.");
	variable_unset(DISK_PARTITIONED);
	return RET_FAIL;
    }
    chdir("/");
    if (extractingBin) {
	while (!saved_etc) {
	    saved_etc = msgGetInput("/usr/tmp/etc", "Under which directory do you wish to save your current /etc?");
	    if (!saved_etc || !*saved_etc || Mkdir(saved_etc, NULL)) {
		if (msgYesNo("Directory was not specified, was invalid or user selected Cancel.\n\n"
			     "Doing an upgrade without first backing up your /etc directory is a very\n"
			     "bad idea!  Do you want to go back and specify the save directory again?"))
		    break;
	    }
	}

	if (saved_etc) {
	    msgNotify("Preserving /etc directory..");
	    /* cp returns a bogus status, so we can't check the status meaningfully.  Bleah. */
	    (void)vsystem("cp -pr /etc/* %s", saved_etc)) {
	}

	if (file_readable("/kernel")) {
	    msgNotify("Moving old kernel to /kernel.205");
	    if (system("chflags noschg /mnt/kernel && mv /mnt/kernel /mnt/kernel.205"))
		if (!msgYesNo("Hmmm!  I couldn't move the old kernel over!  Do you want to\n"
			      "treat this as a big problem and abort the upgrade?"))
		    return RET_FAIL;
	}
    }
    msgNotify("Beginning extraction of distributions..");
    if (distExtractAll(NULL) == RET_FAIL) {
	if (extractingBin && (Dists & DIST_BIN)) {
	    msgConfirm("Hmmmm.  We couldn't even extract the bin distribution.  This upgrade\n"
		       "should be considered a failure and started from the beginning, sorry!\n");
	    return RET_FAIL;
	}
	msgConfirm("The extraction process seems to have had some problems, but we got most\n"
		   "of the essentials.  We'll treat this as a warning since it may have been\n"
		   "only non-essential distributions which failed to load.");
    }

    if (extractingBin) {
	msgNotify("OK, now it's time to go pound on your root a little bit to create all the\n"
		  "/dev entries and such that a 2.1 system expects to see.  I'll also perform a\n"
		  "few \"fixup\" operations to repair the effects of splatting a bin distribution\n"
		  "on top of an existing system..");
	if (installFixup() == RET_FAIL)
	    msgConfirm("Hmmmmm.  The fixups don't seem to have been very happy.\n"
		       "You may wish to examine the system a little more closely when\n"
		       "it comes time to merge your /etc customizations back.");
    }
    
    if (extractingBin)
	configSysconfig();

    if (installFinal() == RET_FAIL)
	msgConfirm("Some of the final configuration stuff evidently failed, but\n"
		   "the first stage of the upgrade should otherwise be considered\n"
		   "a success!\n\n"
		   "Next comes stage 2, where we attempt to resurrect your /etc\n"
		   "directory!");
    else
	msgConfirm("First stage of upgrade completed successfully!\n\n"
		   "Next comes stage 2, where we attempt to resurrect your /etc\n"
		   "directory!");

    if (chdir(saved_etc)) {
	msgConfirm("Unable to go to your saved /etc directory in %s?!  Argh!\n"
		   "Something went seriously wrong!  It's quite possible that\n"
		   "your former /etc is toast.  I hope you didn't have any\n"
		   "important customizations you wanted to keep in there.. :(\n");
	return RET_FAIL;
    }

    /* Now try to resurrect the /etc files */
    traverseHitlist(etc_files);

    msgConfirm("OK!  At this stage, we've resurrected all the /etc files we could\n"
	       "(and you may have been warned about some that you'll have to merge\n"
	       "yourself, by hand) and we're going to drop you into a shell to do\n"
	       "the rest yourself (sorry about this!).  Once the system looks good\n"
	       "to you, exit the shell and reboot the system.");

    chdir("/");
    dialog_clear();
    dialog_update();
    end_dialog();
    DialogActive = FALSE;

    if (!(child = fork())) {
	struct termios foo;

	signal(SIGTTOU, SIG_IGN);
	if (tcgetattr(0, &foo) != -1) {
	    foo.c_cc[VERASE] = '\010';
	    if (tcsetattr(0, TCSANOW, &foo) == -1)
		msgDebug("Unable to set the erase character.\n");
	}
	else
	    msgDebug("Unable to get the terminal attributes!\n");
	printf("Well, good luck!  When you're done, type exit to return.\n");
	execlp("sh", "-sh", 0);
	msgDebug("Was unable to execute sh for post-upgrade shell!\n");
	exit(1);
    }
    else
	(void)waitpid(child, &waitstatus, 0);
    DialogActive = TRUE;
    clear();
    dialog_clear();
    dialog_update();
    return RET_SUCCESS;
}
