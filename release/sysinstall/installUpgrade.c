/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: installUpgrade.c,v 1.33.2.17 1997/10/29 07:47:27 jkh Exp $
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
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>

static int installUpgradeNonInteractive(dialogMenuItem *self);

typedef struct _hitList {
    enum { JUST_COPY, CALL_HANDLER } action ;
    char *name;
    Boolean optional;
    void (*handler)(struct _hitList *self);
} HitList;

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
   { JUST_COPY,		"fstab",		FALSE, NULL },
   { JUST_COPY,		"ftpusers",		TRUE, NULL },
   { JUST_COPY,		"gettytab",		TRUE, NULL },
   { JUST_COPY,		"gnats",		TRUE, NULL },
   { JUST_COPY,		"group",		FALSE, NULL },
   { JUST_COPY,		"host.conf",		TRUE, NULL },
   { JUST_COPY,		"hosts",		TRUE, NULL },
   { JUST_COPY,		"hosts.equiv",		TRUE, NULL },
   { JUST_COPY,		"hosts.lpd",		TRUE, NULL },
   { JUST_COPY,		"inetd.conf",		TRUE, NULL },
   { JUST_COPY,		"kerberosIV",		TRUE, NULL },
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
   { JUST_COPY,		"networks",		TRUE, NULL },
   { JUST_COPY,		"passwd",		FALSE, NULL },
   { JUST_COPY,		"phones",		TRUE, NULL },
   { JUST_COPY,		"ppp",			TRUE, NULL },
   { JUST_COPY,		"printcap",		TRUE, NULL },
   { JUST_COPY,		"profile",		TRUE, NULL },
   { JUST_COPY,		"protocols",		TRUE, NULL },
   { JUST_COPY,		"pwd.db",		TRUE, NULL },
   { JUST_COPY,		"rc.local",		TRUE, NULL },
   { JUST_COPY,		"rc.conf",		FALSE, NULL },
   { JUST_COPY,		"remote",		TRUE, NULL },
   { JUST_COPY,		"resolv.conf",		TRUE, NULL },
   { JUST_COPY,		"rmt",			TRUE, NULL },
   { JUST_COPY,		"security",		TRUE, NULL },
   { JUST_COPY,		"sendmail.cf",		TRUE, NULL },
   { JUST_COPY,		"services",		TRUE, NULL },
   { JUST_COPY,		"shells",		TRUE, NULL },
   { JUST_COPY,		"skeykeys",		TRUE, NULL },
   { JUST_COPY,		"spwd.db",		TRUE, NULL },
   { JUST_COPY,		"supfile",		TRUE, NULL },
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
    system("rm -rf /etc/upgrade");
    Mkdir("/etc/upgrade");
    while (h->name) {
	if (!file_readable(h->name)) {
	    if (!h->optional)
		msgConfirm("Unable to find an old /etc/%s file!  That is decidedly non-standard and\n"
			   "your upgraded system may function a little strangely as a result.", h->name);
	}
	else {
	    if (h->action == JUST_COPY) {
		/* Move the just-loaded copy aside */
		vsystem("mv /etc/%s /etc/upgrade/%s", h->name, h->name);

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
installUpgrade(dialogMenuItem *self)
{
    char saved_etc[FILENAME_MAX];
    Boolean extractingBin = TRUE;

    if (variable_get(VAR_NONINTERACTIVE))
	return installUpgradeNonInteractive(self);

    variable_set2(SYSTEM_STATE, "upgrade");
    systemDisplayHelp("upgrade");

    dialog_clear_norefresh();
    if (msgYesNo("Given all that scary stuff you just read, are you sure you want to\n"
		 "risk it all and proceed with this upgrade?") != 0)
	return DITEM_FAILURE | DITEM_RESTORE;

    if (!Dists) {
	msgConfirm("First, you must select some distribution components.  The upgrade procedure\n"
		   "will only upgrade the distributions you select in the next set of menus.");
	if (!dmenuOpenSimple(&MenuDistributions, FALSE) || !Dists)
	    return DITEM_FAILURE | DITEM_RESTORE;
	dialog_clear_norefresh();
    }
    else if (!(Dists & DIST_BIN)) {	    /* No bin selected?  Not much of an upgrade.. */
	if (msgYesNo("You didn't select the bin distribution as one of the distributons to load.\n"
		     "This one is pretty vital to a successful upgrade.  Are you SURE you don't\n"
		     "want to select the bin distribution?  Chose No to bring up the Distributions\n"
		     "menu again.") != 0) {
	    if (!dmenuOpenSimple(&MenuDistributions, FALSE))
		return DITEM_FAILURE | DITEM_RESTORE;
	    dialog_clear_norefresh();
	}
    }

    /* Still?!  OK!  They must know what they're doing.. */
    if (!(Dists & DIST_BIN))
	extractingBin = FALSE;

    if (RunningAsInit) {
	Device **devs;
	int i, cnt;
	char *cp;

	cp = variable_get(VAR_DISK);
	devs = deviceFind(cp, DEVICE_TYPE_DISK);
	cnt = deviceCount(devs);
	if (!cnt) {
	    msgConfirm("No disks found!  Please verify that your disk controller is being\n"
		       "properly probed at boot time.  See the Hardware Guide on the\n"
		       "Documentation menu for clues on diagnosing this type of problem.");
	    return DITEM_FAILURE;
	}
	else {
	    /* Enable all the drives before we start */
	    for (i = 0; i < cnt; i++)
		devs[i]->enabled = TRUE;
	}

	msgConfirm("OK.  First, we're going to go to the disk label editor.  In this editor\n"
		   "you will be expected to Mount any partitions you're interested in\n"
		   "upgrading.  DO NOT set the Newfs flag to Y on anything in the label editor\n"
		   "unless you're absolutely sure you know what you're doing!  In this\n"
		   "instance, you'll be using the label editor as little more than a fancy\n"
		   "screen-oriented partition mounting tool.\n\n"
		   "Once you're done in the label editor, press Q to return here for the next\n"
		   "step.");

	if (DITEM_STATUS(diskLabelEditor(self)) == DITEM_FAILURE) {
	    msgConfirm("The disk label editor returned an error status.  Upgrade operation\n"
		       "aborted.");
	    return DITEM_FAILURE | DITEM_RESTORE;
	}

	/* Don't write out MBR info */
	variable_set2(DISK_PARTITIONED, "written");
	if (DITEM_STATUS(diskLabelCommit(self)) == DITEM_FAILURE) {
	    msgConfirm("Not all file systems were properly mounted.  Upgrade operation\n"
		       "aborted.");
	    variable_unset(DISK_PARTITIONED);
	    return DITEM_FAILURE | DITEM_RESTORE;
	}

	msgNotify("Updating /stand on root filesystem");
	(void)vsystem("find -x /stand | cpio %s -pdum /mnt", cpioVerbosity());

	if (DITEM_STATUS(chroot("/mnt")) == DITEM_FAILURE) {
	    msgConfirm("Unable to chroot to /mnt - something is wrong with the\n"
		       "root partition or the way it's mounted if this doesn't work.");
	    variable_unset(DISK_PARTITIONED);
	    return DITEM_FAILURE | DITEM_RESTORE;
	}
	chdir("/");
	systemCreateHoloshell();
    }

    saved_etc[0] = '\0';
    if (extractingBin) {
	while (!*saved_etc) {
	    char *cp = msgGetInput("/usr/tmp/etc", "Under which directory do you wish to save your current /etc?");

	    if (!cp || !*cp || Mkdir(cp)) {
		if (msgYesNo("Directory was not specified, was invalid or user selected Cancel.\n\n"
			     "Doing an upgrade without first backing up your /etc directory is a very\n"
			     "bad idea!  Do you want to go back and specify the save directory again?") != 0)
		    break;
	    }
	    else {
		SAFE_STRCPY(saved_etc, cp);
	    }
	}

	if (saved_etc[0]) {
	    msgNotify("Preserving /etc directory..");
	    if (vsystem("tar -cBpf - -C /etc . | tar --unlink -xBpf - -C %s", saved_etc))
		if (msgYesNo("Unable to backup your /etc into %s.\n"
			     "Do you want to continue anyway?", saved_etc) != 0)
		    return DITEM_FAILURE | DITEM_RESTORE;
	}

	msgNotify("chflags'ing old binaries - please wait.");
	(void)vsystem("chflags -R noschg /bin /sbin /usr/sbin /usr/bin /kernel*");

	if (file_readable("/kernel")) {
	    msgNotify("Moving old kernel to /kernel.prev");
	    if (system("mv /kernel /kernel.prev")) {
		if (!msgYesNo("Hmmm!  I couldn't move the old kernel over!  Do you want to\n"
			      "treat this as a big problem and abort the upgrade?  Due to the\n"
			      "way that this upgrade process works, you will have to reboot\n"
			      "and start over from the beginning.  Select Yes to reboot now"))
		    systemShutdown(1);
	    }
	    else 
		msgConfirm("NOTICE: Your old kernel is in /kernel.prev should this upgrade\n"
			   "fail for any reason and you need to boot your old kernel");
	}
    }

media:
    /* We do this very late, but we unfortunately need to back up /etc first */
    if (!mediaVerify())
	return DITEM_FAILURE | DITEM_RESTORE;

    if (!mediaDevice->init(mediaDevice)) {
	if (!msgYesNo("Couldn't initialize the media.  Would you like\n"
		   "to adjust your media selection and try again?")) {
	    mediaDevice = NULL;
	    goto media;
	}
	else
	    return DITEM_FAILURE | DITEM_REDRAW;
    }
    
    msgNotify("Beginning extraction of distributions..");
    if (DITEM_STATUS(distExtractAll(self)) == DITEM_FAILURE) {
	msgConfirm("Hmmmm.  We couldn't even extract the bin distribution.  This upgrade\n"
		   "should be considered a failure and started from the beginning, sorry!\n"
		   "The system will reboot now.");
	dialog_clear();
	systemShutdown(1);
    }
    else if (Dists) {
	if (!extractingBin || !(Dists & DIST_BIN)) {
	    msgNotify("The extraction process seems to have had some problems, but we got most\n"
		       "of the essentials.  We'll treat this as a warning since it may have been\n"
		       "only non-essential distributions which failed to load.");
	}
	else {
	    msgConfirm("Hmmmm.  We couldn't even extract the bin distribution.  This upgrade\n"
		       "should be considered a failure and started from the beginning, sorry!\n"
		       "The system will reboot now.");
	    dialog_clear();
	    systemShutdown(1);
	}
    }

    if (extractingBin) {
	msgNotify("OK, now it's time to go pound on your root a little bit to create all the\n"
		  "/dev entries and such that a new system expects to see.  I'll also perform a\n"
		  "few \"fixup\" operations to repair the effects of splatting a bin distribution\n"
		  "on top of an existing system..");
	if (DITEM_STATUS(installFixup(self)) == DITEM_FAILURE) {
	    msgConfirm("Hmmmmm.  The fixups don't seem to have been very happy.\n"
		       "You may wish to examine the system a little more closely when\n"
		       "it comes time to merge your /etc customizations back.");
	}
    }
    
    msgNotify("First stage of upgrade completed successfully!\n\n"
	       "Next comes stage 2, where we attempt to resurrect your /etc\n"
	       "directory!");

    if (saved_etc && chdir(saved_etc)) {
	msgConfirm("Unable to go to your saved /etc directory in %s?!  Argh!\n"
		   "Something went seriously wrong!  It's quite possible that\n"
		   "your former /etc is toast.  I hope you didn't have any\n"
		   "important customizations you wanted to keep in there.. :(", saved_etc);
    }
    else {
	/* Now try to resurrect the /etc files */
	traverseHitlist(etc_files);
    }

    msgConfirm("OK!  At this stage, we've resurrected all the /etc files\n"
	       "and moved each new copy over to /etc/upgrade/<file> in case you want\n"
	       "to see what the new versions look like.  If you want to wander over\n"
	       "to the Emergency Holographic Shell [ALT-F4] at this point to do\n"
	       "that, now would be a good time.  When you're ready to reboot into\n"
	       "the new system, just exit the installation.");
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
installUpgradeNonInteractive(dialogMenuItem *self)
{
    char *saved_etc;
    Boolean extractingBin = TRUE;

    variable_set2(SYSTEM_STATE, "upgrade");

    /* Make sure at least BIN is selected */
    Dists |= DIST_BIN;

    if (RunningAsInit) {
	Device **devs;
	int i, cnt;
	char *cp;

	cp = variable_get(VAR_DISK);
	devs = deviceFind(cp, DEVICE_TYPE_DISK);
	cnt = deviceCount(devs);
	if (!cnt) {
	    msgConfirm("No disks found!  Please verify that your disk controller is being\n"
		       "properly probed at boot time.  See the Hardware Guide on the\n"
		       "Documentation menu for clues on diagnosing this type of problem.");
	    return DITEM_FAILURE;
	}
	else {
	    /* Enable all the drives befor we start */
	    for (i = 0; i < cnt; i++)
		devs[i]->enabled = TRUE;
	}

	msgConfirm("OK.  First, we're going to go to the disk label editor.  In this editor\n"
		   "you will be expected to Mount any partitions you're interested in\n"
		   "upgrading.  DO NOT set the Newfs flag to Y on anything in the label editor\n"
		   "unless you're absolutely sure you know what you're doing!  In this\n"
		   "instance, you'll be using the label editor as little more than a fancy\n"
		   "screen-oriented partition mounting tool.\n\n"
		   "Once you're done in the label editor, press Q to return here for the next\n"
		   "step.");

	if (DITEM_STATUS(diskLabelEditor(self)) == DITEM_FAILURE) {
	    msgConfirm("The disk label editor returned an error status.  Upgrade operation\n"
		       "aborted.");
	    return DITEM_FAILURE | DITEM_RESTORE;
	}

	/* Don't write out MBR info */
	variable_set2(DISK_PARTITIONED, "written");
	if (DITEM_STATUS(diskLabelCommit(self)) == DITEM_FAILURE) {
	    msgConfirm("Not all file systems were properly mounted.  Upgrade operation\n"
		       "aborted.");
	    variable_unset(DISK_PARTITIONED);
	    return DITEM_FAILURE | DITEM_RESTORE;
	}

	if (extractingBin) {
	    msgNotify("chflags'ing old binaries - please wait.");
	    (void)vsystem("chflags -R noschg /mnt/");
	}
	msgNotify("Updating /stand on root filesystem");
	(void)vsystem("find -x /stand | cpio %s -pdum /mnt", cpioVerbosity());

	if (DITEM_STATUS(chroot("/mnt")) == DITEM_FAILURE) {
	    msgConfirm("Unable to chroot to /mnt - something is wrong with the\n"
		       "root partition or the way it's mounted if this doesn't work.");
	    variable_unset(DISK_PARTITIONED);
	    return DITEM_FAILURE | DITEM_RESTORE;
	}
	chdir("/");
	systemCreateHoloshell();
    }

    if (!mediaVerify() || !mediaDevice->init(mediaDevice)) {
	msgNotify("Upgrade: Couldn't initialize media.");
	return DITEM_FAILURE | DITEM_RESTORE;
    }

    saved_etc = "/usr/tmp/etc";
    Mkdir(saved_etc);
    msgNotify("Preserving /etc directory..");
    if (vsystem("tar -cpBf - -C /etc . | tar -xpBf - -C %s", saved_etc)) {
	msgNotify("Unable to backup your /etc into %s.", saved_etc);
	return DITEM_FAILURE | DITEM_RESTORE;
    }

    if (file_readable("/kernel")) {
	msgNotify("Moving old kernel to /kernel.prev");
	if (!system("chflags noschg /kernel && mv /kernel /kernel.prev")) {
	    /* Give us a working kernel in case we crash and reboot */
	    system("cp /kernel.prev /kernel");
	}
    }

    msgNotify("Beginning extraction of distributions..");
    if (DITEM_STATUS(distExtractAll(self)) == DITEM_FAILURE) {
	msgConfirm("Hmmmm.  We couldn't even extract the bin distribution.  This upgrade\n"
		   "should be considered a failure and started from the beginning, sorry!\n"
		   "The system will reboot now.");
	dialog_clear();
	systemShutdown(1);
    }
    else if (Dists) {
	if (!(Dists & DIST_BIN)) {
	    msgNotify("The extraction process seems to have had some problems, but we got most\n"
		       "of the essentials.  We'll treat this as a warning since it may have been\n"
		       "only non-essential distributions which failed to upgrade.");
	}
	else {
	    msgConfirm("Hmmmm.  We couldn't even extract the bin distribution.  This upgrade\n"
		       "should be considered a failure and started from the beginning, sorry!\n"
		       "The system will reboot now.");
	    dialog_clear();
	    systemShutdown(1);
	}
    }

    msgNotify("OK, now it's time to go pound on your root a little bit to create all the\n"
	      "/dev entries and such that a new system expects to see.  I'll also perform a\n"
	      "few \"fixup\" operations to repair the effects of splatting a bin distribution\n"
	      "on top of an existing system..");
    if (DITEM_STATUS(installFixup(self)) == DITEM_FAILURE) {
	msgNotify("Hmmmmm.  The fixups don't seem to have been very happy.\n"
		  "You may wish to examine the system a little more closely when\n"
		  "it comes time to merge your /etc customizations back.");
    }
    
    msgNotify("First stage of upgrade completed successfully.");
    if (vsystem("tar -cpBf - -C %s . | tar --unlink -xpBf - -C /etc", saved_etc)) {
	msgNotify("Unable to resurrect your old /etc!");
	return DITEM_FAILURE | DITEM_RESTORE;
    }
    return DITEM_SUCCESS | DITEM_REDRAW;
}
