/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.117 1996/07/31 09:29:28 jkh Exp $
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

static void	create_termcap(void);

#define TERMCAP_FILE	"/usr/share/misc/termcap"

static void	installConfigure(void);

Boolean
checkLabels(Boolean whinge, Chunk **rdev, Chunk **sdev, Chunk **udev, Chunk **vdev)
{
    Device **devs;
    Boolean status;
    Disk *disk;
    Chunk *c1, *c2, *rootdev, *swapdev, *usrdev, *vardev;
    int i;

    status = TRUE;
    *rdev = *sdev = *udev = *vdev = rootdev = swapdev = usrdev = vardev = NULL;

    /* We don't need to worry about root/usr/swap if we're already multiuser */
    if (!RunningAsInit)
	return status;

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
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private_data) {
			if (c2->flags & CHUNK_IS_ROOT) {
			    if (rootdev) {
				if (whinge)
				    msgConfirm("WARNING:  You have more than one root device set?!\n"
					       "Using the first one found.");
				continue;
			    }
			    else {
				rootdev = c2;
				if (isDebug())
				    msgDebug("Found rootdev at %s!\n", rootdev->name);
			    }
			}
			else if (!strcmp(((PartInfo *)c2->private_data)->mountpoint, "/usr")) {
			    if (usrdev) {
				if (whinge)
				    msgConfirm("WARNING:  You have more than one /usr filesystem.\n"
					       "Using the first one found.");
				continue;
			    }
			    else {
				usrdev = c2;
				if (isDebug())
				    msgDebug("Found usrdev at %s!\n", usrdev->name);
			    }
			}
			else if (!strcmp(((PartInfo *)c2->private_data)->mountpoint, "/var")) {
			    if (vardev) {
				if (whinge)
				    msgConfirm("WARNING:  You have more than one /var filesystem.\n"
					       "Using the first one found.");
				continue;
			    }
			    else {
				vardev = c2;
				if (isDebug())
				    msgDebug("Found vardev at %s!\n", vardev->name);
			    }
			}
		    }
		}
	    }
	}
    }

    /* Now check for swap devices */
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	disk = (Disk *)devs[i]->private;
	msgDebug("Scanning disk %s for swap partitions\n", disk->name);
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && c2->subtype == FS_SWAP && !swapdev) {
			swapdev = c2;
			if (isDebug())
			    msgDebug("Found swapdev at %s!\n", swapdev->name);
			break;
		    }
		}
	    }
	}
    }

    /* Copy our values over */
    *rdev = rootdev;
    *sdev = swapdev;
    *udev = usrdev;
    *vdev = vardev;

    if (!rootdev && whinge) {
	msgConfirm("No root device found - you must label a partition as /\n"
		   "in the label editor.");
	status = FALSE;
    }
    if (!swapdev && whinge) {
	msgConfirm("No swap devices found - you must create at least one\n"
		   "swap partition.");
	status = FALSE;
    }
    if (!usrdev && whinge) {
	msgConfirm("WARNING:  No /usr filesystem found.  This is not technically\n"
		   "an error if your root filesystem is big enough (or you later\n"
		   "intend to mount your /usr filesystem over NFS), but it may otherwise\n"
		   "cause you trouble if you're not exactly sure what you are doing!");
    }
    if (!vardev && whinge) {
	msgConfirm("WARNING:  No /var filesystem found.  This is not technically\n"
		   "an error if your root filesystem is big enough (or you later\n"
		   "intend to link /var to someplace else), but it may otherwise\n"
		   "cause your root filesystem to fill up if you receive lots of mail\n"
		   "or edit large temporary files.");
    }
    return status;
}

static int
installInitial(void)
{
    static Boolean alreadyDone = FALSE;

    if (alreadyDone)
	return DITEM_SUCCESS;

    if (!variable_get(DISK_LABELLED)) {
	msgConfirm("You need to assign disk labels before you can proceed with\n"
		   "the installation.");
	return DITEM_FAILURE;
    }
    /* If it's labelled, assume it's also partitioned */
    if (!variable_get(DISK_PARTITIONED))
	variable_set2(DISK_PARTITIONED, "yes");

    /* If we refuse to proceed, bail. */
    dialog_clear_norefresh();
    if (msgYesNo("Last Chance!  Are you SURE you want continue the installation?\n\n"
		 "If you're running this on a disk with data you wish to save\n"
		 "then WE STRONGLY ENCOURAGE YOU TO MAKE PROPER BACKUPS before\n"
		 "proceeding!\n\n"
		 "We can take no responsibility for lost disk contents!"))
	return DITEM_FAILURE | DITEM_RESTORE;

    if (DITEM_STATUS(diskLabelCommit(NULL)) != DITEM_SUCCESS) {
	msgConfirm("Couldn't make filesystems properly.  Aborting.");
	return DITEM_FAILURE;
    }
    else if (isDebug())
	msgDebug("installInitial: Scribbled successfully on the disk(s)\n");

    if (!copySelf()) {
	msgConfirm("Couldn't clone the boot floppy onto the root file system.\n"
		   "Aborting.");
	return DITEM_FAILURE;
    }

    if (chroot("/mnt") == -1) {
	msgConfirm("Unable to chroot to /mnt - this is bad!");
	return DITEM_FAILURE;
    }

    chdir("/");
    variable_set2(RUNNING_ON_ROOT, "yes");

    /* stick a helpful shell over on the 4th VTY */
    systemCreateHoloshell();

    alreadyDone = TRUE;
    return DITEM_SUCCESS;
}

int
installFixitCDROM(dialogMenuItem *self)
{
    msgConfirm("Sorry, this feature is currently unimplemented but will,\n"
	       "at some point in the future, support the use of the live\n"
	       "filesystem CD (CD 2) in fixing your system.");
    return DITEM_SUCCESS;
}

int
installFixitFloppy(dialogMenuItem *self)
{
    struct ufs_args args;
    pid_t child;
    int waitstatus;

    variable_set2(SYSTEM_STATE, "fixit");
    memset(&args, 0, sizeof(args));
    args.fspec = "/dev/fd0";
    Mkdir("/mnt2");

    while (1) {
	msgConfirm("Please insert a writable fixit floppy and press return");
	if (mount(MOUNT_UFS, "/mnt2", 0, (caddr_t)&args) != -1)
	    break;
	if (msgYesNo("Unable to mount the fixit floppy - do you want to try again?"))
	    return DITEM_FAILURE;
    }
    dialog_clear();
    end_dialog();
    DialogActive = FALSE;
    if (!directory_exists("/tmp"))
	(void)symlink("/mnt2/tmp", "/tmp");
    if (!directory_exists("/var/tmp/vi.recover")) {
	if (DITEM_STATUS(Mkdir("/var/tmp/vi.recover")) != DITEM_SUCCESS) {
	    msgConfirm("Warning:  Was unable to create a /var/tmp/vi.recover directory.\n"
		       "vi will kvetch and moan about it as a result but should still\n"
		       "be essentially usable.");
	}
    }
    /* Link the spwd.db file */
    if (DITEM_STATUS(Mkdir("/etc")) != DITEM_SUCCESS)
	msgConfirm("Unable to create an /etc directory!  Things are weird on this floppy..");
    else if (symlink("/mnt2/etc/spwd.db", "/etc/spwd.db") == -1 && errno != EEXIST)
	msgConfirm("Couldn't symlink the /etc/spwd.db file!  I'm not sure I like this..");
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
    unmount("/mnt2", MNT_FORCE);
    msgConfirm("Please remove the fixit floppy now.");
    return DITEM_SUCCESS;
}
  
int
installExpress(dialogMenuItem *self)
{
    int i;

    variable_set2(SYSTEM_STATE, "express");
    if (DITEM_STATUS((i = diskPartitionEditor(self))) == DITEM_FAILURE)
	return i;
    
    if (DITEM_STATUS((i = diskLabelEditor(self))) == DITEM_FAILURE)
	return i;

    if (!Dists) {
	dialog_clear_norefresh();
	if (!dmenuOpenSimple(&MenuDistributions, FALSE) && !Dists)
	    return DITEM_FAILURE | DITEM_RECREATE;
    }

    if (!mediaDevice) {
	dialog_clear_norefresh();
	if (!dmenuOpenSimple(&MenuMedia, FALSE) || !mediaDevice)
	    return DITEM_FAILURE | DITEM_RECREATE;
    }

    if (DITEM_STATUS((i = installCommit(self))) == DITEM_SUCCESS) {
	i |= DITEM_LEAVE_MENU;
	/* Give user the option of one last configuration spree */
	installConfigure();

	/* Now write out any changes .. */
	configResolv();
	configSysconfig("/etc/sysconfig");
    }
    return i | DITEM_RECREATE;
}

/* Novice mode installation */
int
installNovice(dialogMenuItem *self)
{
    int i;
    extern int cdromMounted;

    variable_set2(SYSTEM_STATE, "novice");
    dialog_clear_norefresh();
    msgConfirm("In the next menu, you will need to set up a DOS-style (\"fdisk\") partitioning\n"
	       "scheme for your hard disk.  If you simply wish to devote all disk space\n"
	       "to FreeBSD (overwritting anything else that might be on the disk(s) selected)\n"
	       "then use the (A)ll command to select the default partitioning scheme followed\n"
	       "by a (Q)uit.  If you wish to allocate only free space to FreeBSD, move to a\n"
	       "partition marked \"unused\" and use the (C)reate command.");

    if (DITEM_STATUS(diskPartitionEditor(self)) == DITEM_FAILURE)
	return DITEM_FAILURE;
    
    dialog_clear_norefresh();
    msgConfirm("Next, you need to create BSD partitions inside of the fdisk partition(s)\n"
	       "just created.  If you have a reasonable amount of disk space (200MB or more)\n"
	       "and don't have any special requirements, simply use the (A)uto command to\n"
	       "allocate space automatically.  If you have more specific needs or just don't\n"
	       "care for the layout chosen by (A)uto, press F1 for more information on\n"
	       "manual layout.");

    if (DITEM_STATUS(diskLabelEditor(self)) == DITEM_FAILURE)
	return DITEM_FAILURE;

    dialog_clear_norefresh();
    msgConfirm("Now it is time to select an installation subset.  There are a number of\n"
	       "canned distribution sets, ranging from minimal installation sets to full\n"
	       "X11 developer oriented configurations.  You can also select a custom set\n"
	       "of distributions if none of the provided ones are suitable.");
    while (1) {
	if (!dmenuOpenSimple(&MenuDistributions, FALSE) && !Dists)
	    return DITEM_FAILURE | DITEM_RECREATE;
	
	if (Dists || !msgYesNo("No distributions selected.  Are you sure you wish to continue?"))
	    break;
    }

    if (!mediaDevice) {
	msgConfirm("Finally, you must specify an installation medium.");
	if (!dmenuOpenSimple(&MenuMedia, FALSE) || !mediaDevice)
	    return DITEM_FAILURE | DITEM_RECREATE;
    }

    if (DITEM_STATUS((i = installCommit(self))) == DITEM_FAILURE) {
	msgConfirm("Installation completed with some errors.  You may wish to\n"
		   "scroll through the debugging messages on VTY1 with the\n"
		   "scroll-lock feature.  You can also chose \"No\" at the next\n"
		   "prompt and go back into the installation menus to try and retry\n"
		   "whichever operations have failed.");
	return i | DITEM_RECREATE;

    }
    else
	msgConfirm("Congradulations!  You now have FreeBSD installed on your system.\n\n"
		   "We will now move on to the final configuration questions.\n"
		   "For any option you do not wish to configure, simply select\n"
		   "No.\n\n"
		   "If you wish to re-enter this utility after the system is up, you\n"
		   "may do so by typing: /stand/sysinstall.");

    if (mediaDevice->type != DEVICE_TYPE_FTP && mediaDevice->type != DEVICE_TYPE_NFS) {
	if (!msgYesNo("Does this system have a network interface card?")) {
	    Device *save = mediaDevice;

	    /* This will also set the media device, which we don't want */
	    tcpDeviceSelect();
	    /* so we restore our saved value below */
	    mediaDevice = save;
	    dialog_clear_norefresh();
	}
    }

    if (!msgYesNo("Would you like to configure Samba for connecting NETBUI clients to this\n"
		  "machine?  Windows 95, Windows NT and Windows for Workgroups\n"
		  "machines can use NETBUI transport for disk and printer sharing."))
	configSamba(self);

    if (!msgYesNo("Will this machine be an IP gateway (e.g. will it forward packets\n"
		  "between interfaces)?"))
	variable_set2("gateway", "YES");

    if (!msgYesNo("Do you want to allow anonymous FTP connections to this machine?"))
	configAnonFTP(self);

    if (!msgYesNo("Do you want to configure this machine as an NFS server?"))
	configNFSServer(self);

    if (!msgYesNo("Do you want to configure this machine as an NFS client?"))
	variable_set2("nfs_client", "YES");

    if (!msgYesNo("Do you want to configure this machine as a WEB server?"))
	configApache(self);

    if (!msgYesNo("Would you like to customize your system console settings?")) {
	WINDOW *w = savescr();

	dmenuOpenSimple(&MenuSyscons, FALSE);
	restorescr(w);
    }

    if (!msgYesNo("Would you like to set this machine's time zone now?")) {
	WINDOW *w = savescr();

	dialog_clear();
	systemExecute("rm -f /etc/wall_cmos_clock /etc/localtime; tzsetup");
	restorescr(w);
    }

    if (!msgYesNo("Does this system have a mouse attached to it?")) {
	WINDOW *w = savescr();

	dmenuOpenSimple(&MenuMouse, FALSE);
	restorescr(w);
    }

    if (directory_exists("/usr/X11R6")) {
	if (!msgYesNo("Would you like to configure your X server at this time?"))
	    configXFree86(self);
    }

    if (cdromMounted) {
	if (!msgYesNo("Would you like to link to the ports tree on your CDROM?\n\n"
		      "This will require that you have your FreeBSD CD in the CDROM\n"
		      "drive to use the ports collection, but at a substantial savings\n"
		      "in disk space (NOTE:  This may take as long as 15 or 20 minutes\n"
		      "depending on the speed of your CDROM drive)."))
	    configPorts(self);
    }

    if (!msgYesNo("The FreeBSD package collection is a collection of over 450 ready-to-run\n"
		  "applications, from text editors to games to WEB servers.  Would you like\n"
		  "to browse the collection now?"))
	configPackages(self);

    /* XXX Put whatever other nice configuration questions you'd like to ask the user here XXX */

    /* Give user the option of one last configuration spree */
    installConfigure();

    /* Now write out any changes .. */
    configResolv();
    configSysconfig("/etc/sysconfig");

    return DITEM_LEAVE_MENU | DITEM_RECREATE;
}

/* The version of commit we call from the Install Custom menu */
int
installCustomCommit(dialogMenuItem *self)
{
    int i;

    i = installCommit(self);
    if (DITEM_STATUS(i) == DITEM_SUCCESS) {
	/* Give user the option of one last configuration spree */
	installConfigure();

	/* Now write out any changes .. */
	configResolv();
	configSysconfig("/etc/sysconfig");
	return i;
    }
    else
	msgConfirm("The commit operation completed with errors.  Not\n"
		   "updating /etc files.");
    return i;
}

/*
 * What happens when we finally decide to going ahead with the installation.
 *
 * This is broken into multiple stages so that the user can do a full
 * installation but come back here again to load more distributions,
 * perhaps from a different media type.  This would allow, for
 * example, the user to load the majority of the system from CDROM and
 * then use ftp to load just the DES dist.
 */
int
installCommit(dialogMenuItem *self)
{
    int i;
    char *str;

    if (!mediaVerify())
	return DITEM_FAILURE;

    str = variable_get(SYSTEM_STATE);
    if (isDebug())
	msgDebug("installCommit: System state is `%s'\n", str);

    if (RunningAsInit) {
	/* Do things we wouldn't do to a multi-user system */
	if (DITEM_STATUS((i = installInitial())) == DITEM_FAILURE)
	    return i;
	if (DITEM_STATUS((i = configFstab())) == DITEM_FAILURE)
	    return i;
    }

    i = distExtractAll(self);
    if (DITEM_STATUS(i) == DITEM_FAILURE)
    	(void)installFixup(self);
    else
    	i = installFixup(self);

    /* Don't print this if we're express or novice installing - they have their own error reporting */
    if (strcmp(str, "express") && strcmp(str, "novice")) {
	if (Dists || DITEM_STATUS(i) == DITEM_FAILURE)
	    msgConfirm("Installation completed with some errors.  You may wish to\n"
		       "scroll through the debugging messages on VTY1 with the\n"
		       "scroll-lock feature.");
	else
	    msgConfirm("Installation completed successfully.\n\n"
		       "If you have any network devices you have not yet configured,\n"
		       "see the Interfaces configuration item on the Configuration menu.");
    }
    variable_set2(SYSTEM_STATE, DITEM_STATUS(i) == DITEM_FAILURE ? "error-install" : "full-install");
    return i | DITEM_RECREATE;
}

static void
installConfigure(void)
{
    /* Final menu of last resort */
    if (!msgYesNo("Visit the general configuration menu for a chance to set\n"
		  "any last options?")) {
	WINDOW *w = savescr();

	dmenuOpenSimple(&MenuConfigure, FALSE);
	restorescr(w);
    }
}

int
installFixup(dialogMenuItem *self)
{
    Device **devs;
    int i;

    if (!file_readable("/kernel")) {
	if (file_readable("/kernel.GENERIC")) {
	    if (vsystem("cp -p /kernel.GENERIC /kernel")) {
		msgConfirm("Unable to link /kernel into place!");
		return DITEM_FAILURE;
	    }
	}
	else {
	    msgConfirm("Can't find a kernel image to link to on the root file system!\n"
		       "You're going to have a hard time getting this system to\n"
		       "boot from the hard disk, I'm afraid!");
	    return DITEM_FAILURE;
	}
    }
    /* Resurrect /dev after bin distribution screws it up */
    if (RunningAsInit) {
	msgNotify("Remaking all devices.. Please wait!");
	if (vsystem("cd /dev; sh MAKEDEV all")) {
	    msgConfirm("MAKEDEV returned non-zero status");
	    return DITEM_FAILURE;
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
			msgConfirm("Unable to make slice entries for %s!", c1->name);
			return DITEM_FAILURE;
		    }
		}
	    }
	}
	/* XXX Do all the last ugly work-arounds here which we'll try and excise someday right?? XXX */

	msgNotify("Fixing permissions..");
	/* BOGON #1:  XFree86 extracting /usr/X11R6 with root-only perms */
	if (directory_exists("/usr/X11R6")) {
	    vsystem("chmod -R a+r /usr/X11R6");
	    vsystem("find /usr/X11R6 -type d | xargs chmod a+x");
	}
	/* BOGON #2: We leave /etc in a bad state */
	chmod("/etc", 0755);

	/* BOGON #3: No /var/db/mountdtab complains */
	Mkdir("/var/db");
	creat("/var/db/mountdtab", 0644);

	/* Now run all the mtree stuff to fix things up */
        vsystem("mtree -deU -f /etc/mtree/BSD.root.dist -p /");
        vsystem("mtree -deU -f /etc/mtree/BSD.var.dist -p /var");
        vsystem("mtree -deU -f /etc/mtree/BSD.usr.dist -p /usr");
    }
    return DITEM_SUCCESS;
}

/* Go newfs and/or mount all the filesystems we've been asked to */
int
installFilesystems(dialogMenuItem *self)
{
    int i;
    Disk *disk;
    Chunk *c1, *c2, *rootdev, *swapdev, *usrdev, *vardev;
    Device **devs;
    PartInfo *root;
    char dname[80], *str;
    extern int MakeDevChunk(Chunk *c, char *n);
    Boolean upgrade = FALSE;

    str = variable_get(SYSTEM_STATE);

    if (!checkLabels(TRUE, &rootdev, &swapdev, &usrdev, &vardev))
	return DITEM_FAILURE;

    if (rootdev)
	root = (PartInfo *)rootdev->private_data;
    else
	root = NULL;

    command_clear();
    upgrade = str && !strcmp(str, "upgrade");

    if (swapdev) {
	/* As the very first thing, try to get ourselves some swap space */
	sprintf(dname, "/dev/%s", swapdev->name);
	if (!Fake && (!MakeDevChunk(swapdev, "/dev") || !file_readable(dname))) {
	    msgConfirm("Unable to make device node for %s in /dev!\n"
		       "The creation of filesystems will be aborted.", dname);
	    return DITEM_FAILURE;
	}

	if (!Fake) {
	    if (!swapon(dname))
		msgNotify("Added %s as initial swap device", dname);
	    else
		msgConfirm("WARNING!  Unable to swap to %s: %s\n"
			   "This may cause the installation to fail at some point\n"
			   "if you don't have a lot of memory.", dname, strerror(errno));
	}
    }

    if (rootdev) {
	/* Next, create and/or mount the root device */
	sprintf(dname, "/dev/r%sa", rootdev->disk->name);
	if (!Fake && (!MakeDevChunk(rootdev, "/dev") || !file_readable(dname))) {
	    msgConfirm("Unable to make device node for %s in /dev!\n"
		       "The creation of filesystems will be aborted.", dname);
	    return DITEM_FAILURE;
	}
	if (strcmp(root->mountpoint, "/"))
	    msgConfirm("Warning: %s is marked as a root partition but is mounted on %s", rootdev->name, root->mountpoint);

	if (root->newfs) {
	    int i;

	    msgNotify("Making a new root filesystem on %s", dname);
	    i = vsystem("%s %s", root->newfs_cmd, dname);
	    if (i) {
		msgConfirm("Unable to make new root filesystem on %s!\n"
			   "Command returned status %d", dname, i);
		return DITEM_FAILURE;
	    }
	}
	else {
	    if (!upgrade) {
		msgConfirm("Warning:  Using existing root partition.  It will be assumed\n"
			   "that you have the appropriate device entries already in /dev.");
	    }
	    msgNotify("Checking integrity of existing %s filesystem.", dname);
	    i = vsystem("fsck -y %s", dname);
	    if (i)
		msgConfirm("Warning: fsck returned status of %d for %s.\n"
			   "This partition may be unsafe to use.", i, dname);
	}

	/* Switch to block device */
	sprintf(dname, "/dev/%sa", rootdev->disk->name);
	if (Mount("/mnt", dname)) {
	    msgConfirm("Unable to mount the root file system on %s!  Giving up.", dname);
	    return DITEM_FAILURE;
	}
    }

    /* Now buzz through the rest of the partitions and mount them too */
    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;

	disk = (Disk *)devs[i]->private;
	if (!disk->chunks) {
	    msgConfirm("No chunk list found for %s!", disk->name);
	    return DITEM_FAILURE;
	}
	if (root && (root->newfs || upgrade)) {
	    Mkdir("/mnt/dev");
	    if (!Fake)
		MakeDevDisk(disk, "/mnt/dev");
	}

	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private_data) {
			PartInfo *tmp = (PartInfo *)c2->private_data;

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

			if (c2 == swapdev)
			    continue;
			sprintf(fname, "/mnt/dev/%s", c2->name);
			i = (Fake || swapon(fname));
			if (!i)
			    msgNotify("Added %s as an additional swap device", fname);
			else
			    msgConfirm("Unable to add %s as a swap device: %s", fname, strerror(errno));
		    }
		}
	    }
	    else if (c1->type == fat && c1->private_data && (root->newfs || upgrade)) {
		char name[FILENAME_MAX];

		sprintf(name, "/mnt%s", ((PartInfo *)c1->private_data)->mountpoint);
		Mkdir(name);
	    }
	}
    }

    msgNotify("Copying initial device files..");
    /* Copy the boot floppy's dev files */
    if ((root->newfs || upgrade) && vsystem("find -x /dev | cpio %s -pdum /mnt", cpioVerbosity())) {
	msgConfirm("Couldn't clone the /dev files!");
	return DITEM_FAILURE;
    }
    
    command_sort();
    command_execute();
    return DITEM_SUCCESS;
}

/* Initialize various user-settable values to their defaults */
int
installVarDefaults(dialogMenuItem *self)
{
    char *cp;

    /* Set default startup options */
    variable_set2(VAR_ROUTEDFLAGS,		"-q");
    variable_set2(VAR_RELNAME,			RELEASE_NAME);
    variable_set2(VAR_CPIO_VERBOSITY,		"high");
    variable_set2(VAR_TAPE_BLOCKSIZE,		DEFAULT_TAPE_BLOCKSIZE);
    variable_set2(VAR_INSTALL_ROOT,		"/");
    cp = getenv("EDITOR");
    if (!cp)
	cp = "/usr/bin/ee";
    variable_set2(VAR_EDITOR,			cp);
    variable_set2(VAR_FTP_USER,			"ftp");
    variable_set2(VAR_BROWSER_PACKAGE,		"lynx-2.5FM");
    variable_set2(VAR_BROWSER_BINARY,		"/usr/local/bin/lynx");
    variable_set2(VAR_FTP_STATE,		"passive");
    variable_set2(VAR_PKG_TMPDIR,		"/usr/tmp");
    if (getpid() != 1)
	variable_set2(SYSTEM_STATE,		"update");
    else
	variable_set2(SYSTEM_STATE,		"init");
    return DITEM_SUCCESS;
}

/* Copy the boot floppy contents into /stand */
Boolean
copySelf(void)
{
    int i;

    msgWeHaveOutput("Copying the boot floppy to /stand on root filesystem");
    i = vsystem("find -x /stand | cpio %s -pdum /mnt", cpioVerbosity());
    if (i) {
	msgConfirm("Copy returned error status of %d!", i);
	return FALSE;
    }

    /* Copy the /etc files into their rightful place */
    if (vsystem("cd /mnt/stand; find etc | cpio %s -pdum /mnt", cpioVerbosity())) {
	msgConfirm("Couldn't copy up the /etc files!");
	return TRUE;
    }
    return TRUE;
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
	Mkdir("/usr/share/misc");
	fp = fopen(TERMCAP_FILE, "w");
	if (!fp) {
	    msgConfirm("Unable to initialize termcap file. Some screen-oriented\nutilities may not work.");
	    return;
	}
	cp = caps;
	while (*cp)
	    fprintf(fp, "%s\n", *(cp++));
	fclose(fp);
    }
}

