/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.71.2.59 1995/10/27 01:36:45 jkh Exp $
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
#include <ctype.h>
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/param.h>
#define MSDOSFS
#include <sys/mount.h>
#undef MSDOSFS
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>

static Boolean	copy_self(void);
static Boolean	root_extract(void);
static void	create_termcap(void);

#define TERMCAP_FILE	"/usr/share/misc/termcap"

static Boolean
checkLabels(Chunk **rdev, Chunk **sdev, Chunk **udev)
{
    Device **devs;
    Boolean status;
    Disk *disk;
    Chunk *c1, *c2, *rootdev, *swapdev, *usrdev;
    int i;

    status = TRUE;
    *rdev = *sdev = *udev = rootdev = swapdev = usrdev = NULL;
    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    /* First verify that we have a root device */
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	disk = (Disk *)devs[i]->private;
	msgDebug("Scanning disk %s for root filesystem\n", disk->name);
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private) {
			if (c2->flags & CHUNK_IS_ROOT) {
			    if (rootdev) {
				dialog_clear();
				msgConfirm("WARNING:  You have more than one root device set?!\n"
					   "Using the first one found.");
				continue;
			    }
			    rootdev = c2;
			    if (isDebug())
				msgDebug("Found rootdev at %s!\n", rootdev->name);
			}
			else if (!strcmp(((PartInfo *)c2->private)->mountpoint, "/usr")) {
			    if (usrdev) {
				dialog_clear();
				msgConfirm("WARNING:  You have more than one /usr filesystem.\n"
					   "Using the first one found.");
				continue;
			    }
			    usrdev = c2;
			    if (isDebug())
				msgDebug("Found usrdev at %s!\n", usrdev->name);
			}
		    }
		}
	    }
	}
    }

    /* Now check for swap devices */
    for (i = 0; devs[i]; i++) {
	disk = (Disk *)devs[i]->private;
	msgDebug("Scanning disk %s for swap partitions\n", disk->name);
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && c2->subtype == FS_SWAP) {
			swapdev = c2;
			if (isDebug())
			    msgDebug("Found swapdev at %s!\n", swapdev->name);
			break;
		    }
		}
	    }
	}
    }

    *rdev = rootdev;
    if (!rootdev) {
	dialog_clear();
	msgConfirm("No root device found - you must label a partition as /\n"
		   "in the label editor.");
	status = FALSE;
    }

    *sdev = swapdev;
    if (!swapdev) {
	dialog_clear();
	msgConfirm("No swap devices found - you must create at least one\n"
		   "swap partition.");
	status = FALSE;
    }

    *udev = usrdev;
    if (!usrdev) {
	dialog_clear();
	msgConfirm("WARNING:  No /usr filesystem found.  This is not technically\n"
		   "an error if your root filesystem is big enough (or you later\n"
		   "intend to mount your /usr filesystem over NFS), but it may otherwise\n"
		   "cause you trouble if you're not exactly sure what you are doing!");
    }
    return status;
}

static int
installInitial(void)
{
    static Boolean alreadyDone = FALSE;

    if (alreadyDone)
	return RET_SUCCESS;

    if (!variable_get(DISK_LABELLED)) {
	dialog_clear();
	msgConfirm("You need to assign disk labels before you can proceed with\nthe installation.");
	return RET_FAIL;
    }
    /* If it's labelled, assume it's also partitioned */
    if (!variable_get(DISK_PARTITIONED))
	variable_set2(DISK_PARTITIONED, "yes");

    /* If we refuse to proceed, bail. */
    dialog_clear();
    if (msgYesNo("Last Chance!  Are you SURE you want continue the installation?\n\n"
		 "If you're running this on a disk with data you wish to save\n"
		 "then WE STRONGLY ENCOURAGE YOU TO MAKE PROPER BACKUPS before\n"
		 "proceeding!\n\n"
		 "We can take no responsibility for lost disk contents!"))
	return RET_FAIL;

    if (diskLabelCommit(NULL) != RET_SUCCESS) {
	dialog_clear();
	msgConfirm("Couldn't make filesystems properly.  Aborting.");
	return RET_FAIL;
    }

    if (!copy_self()) {
	dialog_clear();
	msgConfirm("Couldn't clone the boot floppy onto the root file system.\n"
		   "Aborting.");
	return RET_FAIL;
    }

    dialog_clear();
    if (chroot("/mnt") == -1) {
	dialog_clear();
	msgConfirm("Unable to chroot to /mnt - this is bad!");
	return RET_FAIL;
    }

    chdir("/");
    variable_set2(RUNNING_ON_ROOT, "yes");

    /* stick a helpful shell over on the 4th VTY */
    systemCreateHoloshell();
    alreadyDone = TRUE;
    return RET_SUCCESS;
}

int
installFixit(char *str)
{
    struct ufs_args args;
    pid_t child;
    int waitstatus;

    memset(&args, 0, sizeof(args));
    args.fspec = "/dev/fd0";
    Mkdir("/mnt2", NULL);

    while (1) {
	dialog_clear();
	msgConfirm("Please insert a writable fixit floppy and press return");
	if (mount(MOUNT_UFS, "/mnt2", 0, (caddr_t)&args) != -1)
	    break;
	dialog_clear();
	if (msgYesNo("Unable to mount the fixit floppy - do you want to try again?"))
	    return RET_FAIL;
    }
    dialog_clear();
    dialog_update();
    end_dialog();
    DialogActive = FALSE;
    if (!directoryExists("/tmp"))
	(void)symlink("/mnt2/tmp", "/tmp");
    if (!directoryExists("/var/tmp/vi.recover")) {
	if (Mkdir("/var/tmp/vi.recover", NULL) != RET_SUCCESS) {
	    dialog_clear();
	    msgConfirm("Warning:  Was unable to create a /var/tmp/vi.recover directory.\n"
		       "vi will kvetch and moan about it as a result but should still\n"
		       "be essentially usable.");
	}
    }
    /* Link the spwd.db file */
    if (Mkdir("/etc", NULL) != RET_SUCCESS) {
	dialog_clear();
	msgConfirm("Unable to create an /etc directory!  Things are weird on this floppy..");
    }
    else {
	if (symlink("/mnt2/etc/spwd.db", "/etc/spwd.db") == -1) {
	    dialog_clear();
	    msgConfirm("Couldn't symlink the /etc/spwd.db file!  I'm not sure I like this..");
	}
    }
    if (!file_readable(TERMCAP_FILE))
	create_termcap();
    if (!(child = fork())) {
	struct termios foo;

	signal(SIGTTOU, SIG_IGN);
	if (tcgetattr(0, &foo) != -1) {
	    foo.c_cc[VERASE] = '\010';
	    if (tcsetattr(0, TCSANOW, &foo) == -1)
		msgDebug("fixit shell: Unable to set erase character.\n");
	}
	else
	    msgDebug("fixit shell: Unable to get terminal attributes!\n");
	printf("When you're finished with this shell, please type exit.\n");
	printf("The fixit floppy itself is mounted as /mnt2\n");
	setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/stand:/mnt2/stand", 1);
	execlp("sh", "-sh", 0);
	msgDebug("fixit shell: Failed to execute shell!\n");
	return -1;
    }
    else
	(void)waitpid(child, &waitstatus, 0);

    DialogActive = TRUE;
    clear();
    dialog_clear();
    dialog_update();
    unmount("/mnt2", MNT_FORCE);
    dialog_clear();
    msgConfirm("Please remove the fixit floppy now.");
    return RET_SUCCESS;
}
  
int
installExpress(char *str)
{
    char *cp;

    dialog_clear();
    msgConfirm("In the next menu, you will need to set up a DOS-style (\"fdisk\") partitioning\n"
	       "scheme for your hard disk.  If you simply wish to devote all disk space\n"
	       "to FreeBSD (overwritting anything else that might be on the disk(s) selected)\n"
	       "then use the (A)ll command to select the default partitioning scheme followed\n"
	       "by a (Q)uit.  If you wish to allocate only free space to FreeBSD, move to a\n"
	       "partition marked \"unused\" and use the (C)reate command.");

    if (diskPartitionEditor("express") == RET_FAIL)
	return RET_FAIL;
    
    dialog_clear();
    msgConfirm("Next, you need to create BSD partitions inside of the fdisk partition(s)\n"
	       "just created.  If you have a reasonable amount of disk space (200MB or more)\n"
	       "and don't have any special requirements, simply use the (A)uto command to\n"
	       "allocate space automatically.  If you have more specific needs or just don't\n"
	       "care for the layout chosen by (A)uto, press F1 for more information on\n"
	       "manual layout.");

    if (diskLabelEditor("express") == RET_FAIL)
	return RET_FAIL;

    /* If this is set, we have a power installation setup here.. */
    cp = variable_get(VAR_DIST_SETS);
    if (cp) {
	/* These *ALL* have to be set at once.  Like I said - power installation setup */
	if (sscanf(cp, "%x %x %x %x %x %x",
		   &Dists, &DESDists, &SrcDists, &XF86Dists, &XF86ServerDists, &XF86FontDists) != 6) {
	    dialog_clear();
	    msgConfirm("Warning:  A %s variable was configured which did not set all\n"
		       "distributions explicitly.  Some distributions will default to\n"
		       "unselected as a result.", VAR_DIST_SETS);
	}
    }
    if (!Dists) {
	dialog_clear();
	msgConfirm("Now it is time to select an installation subset.  There are a number of\n"
		   "canned distribution sets, ranging from minimal installation sets to full\n"
		   "X11 developer oriented configurations.  You can also select a custom set\n"
		   "of distributions if none of the provided ones are suitable.");
	while (1) {
	    if (!dmenuOpenSimple(&MenuDistributions))
		return RET_FAIL;

	    if (Dists || !msgYesNo("No distributions selected.  Are you sure you wish to continue?"))
		break;
	}
    }

    if (!mediaDevice) {
	dialog_clear();
	msgConfirm("Finally, you must specify an installation medium.");
	if (!dmenuOpenSimple(&MenuMedia) || !mediaDevice)
	    return RET_FAIL;
    }

    if (installCommit("express") == RET_FAIL)
	return RET_FAIL;

    return RET_DONE;
}

/*
 * What happens when we select "Commit" in the custom installation menu.
 *
 * This is broken into multiple stages so that the user can do a full installation but come back here
 * again to load more distributions, perhaps from a different media type.  This would allow, for
 * example, the user to load the majority of the system from CDROM and then use ftp to load just the
 * DES dist.
 */
int
installCommit(char *str)
{
    int i;

    if (!mediaVerify())
	return RET_FAIL;

    i = RET_DONE;
    if (RunningAsInit) {
	if (installInitial() == RET_FAIL)
	    return RET_FAIL;
	else if (configFstab() == RET_FAIL)
	    return RET_FAIL;
	else if (!root_extract()) {
	    dialog_clear();
	    msgConfirm("Failed to load the ROOT distribution.  Please correct\n"
		       "this problem and try again.");
	    return RET_FAIL;
	}
    }

    if (distExtractAll(NULL) == RET_FAIL)
	i = RET_FAIL;

    if (installFixup(NULL) == RET_FAIL)
	i = RET_FAIL;

    if (i != RET_FAIL && str && !strcmp(str, "express")) {
	dialog_clear();
	if (!msgYesNo("Since you're running the express installation, a few post-configuration\n"
		      "questions will be asked at this point.\n\n"
		      "The FreeBSD package collection is a collection of over 300 ready-to-run\n"
		      "applications, from text editors to games to WEB servers.  If you've never\n"
		      "done so, it's definitely worth browsing through.\n\n"
		      "Would you like to do so now?"))
	    configPackages(NULL);

	dialog_clear();
	if (!msgYesNo("Would you like to configure any additional network devices or services?"))
	    configNetworking(NULL);

	/* XXX Put whatever other nice configuration questions you'd like to ask the user here XXX */
	
	/* Final menu of last resort */
	dialog_clear();
	if (!msgYesNo("Would you like to go to the general configuration menu for any last\n"
		      "additional configuration options?"))
	    dmenuOpenSimple(&MenuConfigure);
    }

    /* Write out any changes to /etc/sysconfig */
    if (RunningAsInit)
	configSysconfig();

    variable_set2(SYSTEM_STATE, i == RET_FAIL ? "installed+errors" : "installed");
    dialog_clear();
    /* Don't print this if we're express installing */
    if (strcmp(str, "express")) {
	if (Dists || i == RET_FAIL) {
	    dialog_clear();
	    msgConfirm("Installation completed with some errors.  You may wish to\n"
		       "scroll through the debugging messages on VTY1 with the\n"
		       "scroll-lock feature.");
	}
	else {
	    dialog_clear();
	    msgConfirm("Installation completed successfully.\n\n"
		       "If you have any network devices you have not yet configured,\n"
		       "see the Interfaces configuration item on the Configuration menu.");
	}
    }
    return i;
}

int
installFixup(char *str)
{
    Device **devs;
    int i;

    if (!file_readable("/kernel")) {
	if (file_readable("/kernel.GENERIC")) {
	    if (vsystem("ln -f /kernel.GENERIC /kernel")) {
		dialog_clear();
		msgConfirm("Unable to link /kernel into place!");
		return RET_FAIL;
	    }
	}
	else {
	    dialog_clear();
	    msgConfirm("Can't find a kernel image to link to on the root file system!\n"
		       "You're going to have a hard time getting this system to\n"
		       "boot from the hard disk, I'm afraid!");
	    return RET_FAIL;
	}
    }
    /* Resurrect /dev after bin distribution screws it up */
    if (RunningAsInit) {
	msgNotify("Remaking all devices.. Please wait!");
	if (vsystem("cd /dev; sh MAKEDEV all")) {
	    dialog_clear();
	    msgConfirm("MAKEDEV returned non-zero status");
	    return RET_FAIL;
	}

	msgNotify("Resurrecting /dev entries for slices..");
	devs = deviceFind(NULL, DEVICE_TYPE_DISK);
	if (!devs)
	    msgFatal("Couldn't get a disk device list!");

	/* Resurrect the slices that the former clobbered */
	for (i = 0; devs[i]; i++) {
	    Disk *disk = (Disk *)devs[i]->private;
	    Chunk *c1;

	    if (!devs[i]->enabled)
		continue;
	    if (!disk->chunks)
		msgFatal("No chunk list found for %s!", disk->name);
	    for (c1 = disk->chunks->part; c1; c1 = c1->next) {
		if (c1->type == freebsd) {
		    msgNotify("Making slice entries for %s", c1->name);
		    if (vsystem("cd /dev; sh MAKEDEV %sh", c1->name)) {
			dialog_clear();
			msgConfirm("Unable to make slice entries for %s!", c1->name);
			return RET_FAIL;
		    }
		}
	    }
	}
	/* XXX Do all the last ugly work-arounds here which we'll try and excise someday right?? XXX */

	msgNotify("Fixing permissions..");
	/* BOGON #1:  XFree86 extracting /usr/X11R6 with root-only perms */
	if (directoryExists("/usr/X11R6"))
	    chmod("/usr/X11R6", 0755);

	/* BOGON #2: We leave /etc in a bad state */
	chmod("/etc", 0755);

	/* Now run all the mtree stuff to fix things up */
        vsystem("mtree -deU -f /etc/mtree/BSD.root.dist -p /");
        vsystem("mtree -deU -f /etc/mtree/BSD.var.dist -p /var");
        vsystem("mtree -deU -f /etc/mtree/BSD.usr.dist -p /usr");
    }
    return RET_SUCCESS;
}

/* Go newfs and/or mount all the filesystems we've been asked to */
int
installFilesystems(char *str)
{
    int i;
    Disk *disk;
    Chunk *c1, *c2, *rootdev, *swapdev, *usrdev;
    Device **devs;
    PartInfo *root;
    char dname[40];
    extern int MakeDevChunk(Chunk *c, char *n);
    Boolean upgrade = FALSE;

    if (!(str && !strcmp(str, "script")) && !checkLabels(&rootdev, &swapdev, &usrdev))
	return RET_FAIL;

    root = (PartInfo *)rootdev->private;
    command_clear();
    upgrade = str && !strcmp(str, "upgrade");

    /* First, create and mount the root device */
    sprintf(dname, "/dev/%s", rootdev->name);
    if (!MakeDevChunk(rootdev, "/dev") || !file_readable(dname)) {
	dialog_clear();
	msgConfirm("Unable to make device node for %s in /dev!\n"
		   "The writing of filesystems will be aborted.", rootdev->name);
	return RET_FAIL;
    }

    if (strcmp(root->mountpoint, "/")) {
	dialog_clear();
	msgConfirm("Warning: %s is marked as a root partition but is mounted on %s", rootdev->name, root->mountpoint);
    }

    if (root->newfs) {
	int i;

	msgNotify("Making a new root filesystem on %s", rootdev->name);
	i = vsystem("%s /dev/r%s", root->newfs_cmd, rootdev->name);
	if (i) {
	    dialog_clear();
	    msgConfirm("Unable to make new root filesystem on /dev/r%s!\n"
		       "Command returned status %d", rootdev->name, i);
	    return RET_FAIL;
	}
    }
    else {
	if (!upgrade) {
	    dialog_clear();
	    msgConfirm("Warning:  Root device is selected read-only.  It will be assumed\n"
		       "that you have the appropriate device entries already in /dev.\n");
	}
	msgNotify("Checking integrity of existing %s filesystem.", rootdev->name);
	i = vsystem("fsck -y /dev/r%s", rootdev->name);
	if (i) {
	    dialog_clear();
	    msgConfirm("Warning: fsck returned status of %d for /dev/r%s.\n"
		       "This partition may be unsafe to use.", i, rootdev->name);
	}
    }
    if (Mount("/mnt", dname)) {
	dialog_clear();
	msgConfirm("Unable to mount the root file system on %s!  Giving up.", dname);
	return RET_FAIL;
    }

    /* Now buzz through the rest of the partitions and mount them too */
    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;

	disk = (Disk *)devs[i]->private;
	if (!disk->chunks) {
	    dialog_clear();
	    msgConfirm("No chunk list found for %s!", disk->name);
	    return RET_FAIL;
	}
	if (root->newfs || upgrade) {
	    Mkdir("/mnt/dev", NULL);
	    MakeDevDisk(disk, "/mnt/dev");
	}

	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private) {
			PartInfo *tmp = (PartInfo *)c2->private;

			/* Already did root */
			if (c2 == rootdev)
			    continue;

			if (tmp->newfs)
			    command_shell_add(tmp->mountpoint, "%s /mnt/dev/r%s", tmp->newfs_cmd, c2->name);
			else
			    command_shell_add(tmp->mountpoint, "fsck -y /mnt/dev/r%s", c2->name);
			command_func_add(tmp->mountpoint, Mount, c2->name);
		    }
		    else if (c2->type == part && c2->subtype == FS_SWAP) {
			char fname[80];
			int i;

			sprintf(fname, "/mnt/dev/%s", c2->name);
			i = swapon(fname);
			if (!i)
			    msgNotify("Added %s as a swap device", fname);
			else {
			    dialog_clear();
			    msgConfirm("Unable to add %s as a swap device: %s", fname, strerror(errno));
			}
		    }
		}
	    }
	    else if (c1->type == fat && c1->private && (root->newfs || upgrade)) {
		char name[FILENAME_MAX];

		sprintf(name, "/mnt%s", ((PartInfo *)c1->private)->mountpoint);
		Mkdir(name, NULL);
	    }
	}
    }

    /* Copy the boot floppy's dev files */
    if ((root->newfs || upgrade) && vsystem("find -x /dev | cpio -pdmv /mnt")) {
	dialog_clear();
	msgConfirm("Couldn't clone the /dev files!");
	return RET_FAIL;
    }
    
    command_sort();
    command_execute();
    return RET_SUCCESS;
}

int
installVarDefaults(char *unused)
{
    /* Set default startup options */
    variable_set2(VAR_ROUTEDFLAGS,		"-q");
    variable_set2(VAR_RELNAME,			RELEASE_NAME);
    variable_set2(VAR_CPIO_VERBOSITY,		"high");
    variable_set2(VAR_TAPE_BLOCKSIZE,		DEFAULT_TAPE_BLOCKSIZE);
    variable_set2(VAR_FTP_USER,			"ftp");
    variable_set2(VAR_BROWSER_PACKAGE,		"lynx-2.4.2");
    variable_set2(VAR_BROWSER_BINARY,		"/usr/local/bin/lynx");
    variable_set2(VAR_CONFIG_FILE,		"freebsd.cfg");
    variable_set2(VAR_FTP_STATE,		"passive");
    variable_set2(VAR_FTP_ONERROR,		"abort");
    if (getpid() != 1)
	variable_set2(SYSTEM_STATE,		"update");
    else
	variable_set2(SYSTEM_STATE,		"init");
    return RET_SUCCESS;
}

/* Copy the boot floppy contents into /stand */
static Boolean
copy_self(void)
{
    int i;

    msgWeHaveOutput("Copying the boot floppy to /stand on root filesystem");
    i = vsystem("find -x /stand | cpio -pdmv /mnt");
    if (i) {
	dialog_clear();
	msgConfirm("Copy returned error status of %d!", i);
	return FALSE;
    }

    /* Copy the /etc files into their rightful place */
    if (vsystem("cd /mnt/stand; find etc | cpio -pdmv /mnt")) {
	dialog_clear();
	msgConfirm("Couldn't copy up the /etc files!");
	return TRUE;
    }
    return TRUE;
}

static Boolean loop_on_root_floppy(void);

static Boolean
root_extract(void)
{
    int fd;
    static Boolean alreadyExtracted = FALSE;

    if (alreadyExtracted)
	return TRUE;

    if (mediaDevice) {
	if (isDebug())
	    msgDebug("Attempting to extract root image from %s\n", mediaDevice->name);
	switch(mediaDevice->type) {

	case DEVICE_TYPE_FLOPPY:
	    alreadyExtracted = loop_on_root_floppy();
	    break;

	default:
	    if (!mediaDevice->init(mediaDevice))
		break;
	    fd = mediaDevice->get(mediaDevice, "floppies/root.flp", FALSE);
	    if (fd < 0) {
		dialog_clear();
		msgConfirm("Couldn't get root image from %s!\n"
			   "Will try to get it from floppy.", mediaDevice->name);
		mediaDevice->shutdown(mediaDevice);
	        alreadyExtracted = loop_on_root_floppy();
	    }
	    else {
		msgNotify("Loading root image from:\n%s", mediaDevice->name);
		alreadyExtracted = mediaExtractDist("/", fd);
		mediaDevice->close(mediaDevice, fd);
	    }
	    break;
	}
    }
    else
	alreadyExtracted = loop_on_root_floppy();
    return alreadyExtracted;
}

static Boolean
loop_on_root_floppy(void)
{
    int fd;
    int status = FALSE;

    while (1) {
	fd = getRootFloppy();
	if (fd != -1) {
	    msgNotify("Extracting root floppy..");
	    status = mediaExtractDist("/", fd);
	    close(fd);
	    break;
	}
    }
    return status;
}

static void
create_termcap(void)
{
    FILE *fp;

    const char *caps[] = {
	termcap_vt100, termcap_cons25, termcap_cons25_m, termcap_cons25r,
	termcap_cons25r_m, termcap_cons25l1, termcap_cons25l1_m, NULL,
    };
    const char **cp;

    if (!file_readable(TERMCAP_FILE)) {
	Mkdir("/usr/share/misc", NULL);
	fp = fopen(TERMCAP_FILE, "w");
	if (!fp) {
	    dialog_clear();
	    msgConfirm("Unable to initialize termcap file. Some screen-oriented\nutilities may not work.");
	    return;
	}
	cp = caps;
	while (*cp)
	    fprintf(fp, "%s\n", *(cp++));
	fclose(fp);
    }
}

