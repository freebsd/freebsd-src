/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.71.2.28 1995/10/13 08:19:24 jkh Exp $
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
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

static Boolean	copy_self(void);
static Boolean	root_extract(void);
static void	create_termcap(void);

static Boolean
checkLabels(Chunk **rdev, Chunk **sdev, Chunk **udev)
{
    Device **devs;
    Disk *disk;
    Chunk *c1, *c2, *rootdev, *swapdev, *usrdev;
    int i;

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
	msgConfirm("No root device found - you must label a partition as /\n"
		   "in the label editor.");
	return FALSE;
    }

    *sdev = swapdev;
    if (!swapdev) {
	msgConfirm("No swap devices found - you must create at least one\n"
		   "swap partition.");
	return FALSE;
    }

    *udev = usrdev;
    if (!usrdev)
	msgConfirm("WARNING:  No /usr filesystem found.  This is not technically\n"
		   "an error if your root filesystem is big enough (or you later\n"
		   "intend to mount your /usr filesystem over NFS), but it may otherwise\n"
		   "cause you trouble if you're not exactly sure what you are doing!");
    return TRUE;
}

static Boolean
installInitial(void)
{
    static Boolean alreadyDone = FALSE;

    if (alreadyDone)
	return TRUE;

    if (!variable_get(DISK_PARTITIONED)) {
	msgConfirm("You need to partition your disk before you can proceed with\nthe installation.");
	return FALSE;
    }
    if (!variable_get(DISK_LABELLED)) {
	msgConfirm("You need to assign disk labels before you can proceed with\nthe installation.");
	return FALSE;
    }

    /* If we refuse to proceed, bail. */
    if (msgYesNo("Last Chance!  Are you SURE you want continue the installation?\n\n"
		 "If you're running this on a disk with data you wish to save\n"
		 "then WE STRONGLY ENCOURAGE YOU TO MAKE PROPER BACKUPS before\n"
		 "proceeding!\n\n"
		 "We can take no responsibility for lost disk contents!"))
	return FALSE;

    if (diskPartitionWrite(NULL) != RET_SUCCESS) {
	msgConfirm("installInitial:  Unable to write disk partition information.");
	return FALSE;
    }

    if (installFilesystems() != RET_SUCCESS) {
	msgConfirm("Couldn't make filesystems properly.  Aborting.");
	return FALSE;
    }

    if (!copy_self()) {
	msgConfirm("Couldn't clone the boot floppy onto the root file system.\nAborting.");
	return FALSE;
    }

    dialog_clear();
    chroot("/mnt");
    chdir("/");
    variable_set2(RUNNING_ON_ROOT, "yes");
    /* stick a helpful shell over on the 4th VTY */
    if (OnVTY && !fork()) {
	int i, fd;
	struct termios foo;
	extern int login_tty(int);

	msgDebug("Starting an emergency holographic shell over on the 4th screen\n");
	for (i = 0; i < 64; i++)
	    close(i);
	fd = open("/dev/ttyv3", O_RDWR);
	ioctl(0, TIOCSCTTY, &fd);
	dup2(0, 1);
	dup2(0, 2);
	if (login_tty(fd) == -1) {
	    msgNotify("Can't set controlling terminal");
	    exit(1);
	}
	signal(SIGTTOU, SIG_IGN);
	if (tcgetattr(fd, &foo) != -1) {
	    foo.c_cc[VERASE] = '\010';
	    if (tcsetattr(fd, TCSANOW, &foo) == -1)
		printf("WARNING: Unable to set erase character.\n");
	}
	else
	    printf("WARNING: Unable to get terminal attributes!\n");
	printf("Warning: This shell is chroot()'d to /mnt\n");
	execlp("sh", "-sh", 0);
	exit(1);
    }
    alreadyDone = TRUE;
    return TRUE;
}

int
installFixit(char *str)
{
    struct ufs_args args;
    pid_t child;
    int waitstatus;

    memset(&args, 0, sizeof(args));
    args.fspec = "/dev/fd0";
    (void)mkdir("/mnt2", 0755);

    while (1) {
	msgConfirm("Please insert a writable fixit floppy and press return");
	if (mount(MOUNT_UFS, "/mnt2", 0, (caddr_t)&args) != -1)
	    break;
	if (msgYesNo("Unable to mount the fixit floppy - do you want to try again?"))
	    return RET_FAIL;
    }
    dialog_clear();
    dialog_update();
    end_dialog();
    DialogActive = FALSE;
    /* Try to leach a big /tmp off the fixit floppy */
    if (!file_executable("/tmp"))
	(void)symlink("/mnt2/tmp", "/tmp");
    if (!file_readable("/var/tmp/vi.recover")) {
	Mkdir("/var", NULL);
	(void)symlink("/mnt2/tmp", "/var/tmp");
	Mkdir("/mnt2/tmp/vi.recover", NULL);
    }
    /* Link the spwd.db file */
    Mkdir("/etc", NULL);
    (void)symlink("/mnt2/etc/spwd.db", "/etc/spwd.db");
    create_termcap();
    if ((child = fork()) != 0)
	(void)waitpid(child, &waitstatus, 0);
    else {
	int i, fd;
	extern int login_tty(int);
	struct termios foo;

	for (i = 0; i < 64; i++)
	    close(i);
	fd = open("/dev/ttyv0", O_RDWR);
	ioctl(0, TIOCSCTTY, &fd);
	dup2(0, 1);
	dup2(0, 2);
	if (login_tty(fd) == -1) {
	    msgNotify("Can't set controlling terminal");
	    exit(1);
	}
	signal(SIGTTOU, SIG_IGN);
	if (tcgetattr(fd, &foo) != -1) {
	    foo.c_cc[VERASE] = '\010';
	    if (tcsetattr(fd, TCSANOW, &foo) == -1)
		printf("WARNING: Unable to set erase character.\n");
	}
	else
	    printf("WARNING: Unable to get terminal attributes!\n");
	printf("When you're finished with this shell, please type exit.\n");
	setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/stand:/mnt2/stand", 1);
	execlp("sh", "-sh", 0);
	return -1;
    }
    DialogActive = TRUE;
    clear();
    dialog_clear();
    dialog_update();
    unmount("/mnt2", MNT_FORCE);
    msgConfirm("Please remove the fixit floppy and press return");
    return RET_SUCCESS;
}

int
installUpgrade(char *str)
{
    /* Storyboard:
       1. Verify that user has mounted/newfs flagged all desired directories
       for upgrading.  Should have selected a / at the very least, with
       warning for no /usr.  If not, throw into partition/disklabel editors
       with appropriate popup info in-between.

       2. If BIN distribution selected, backup /etc to some location -
       prompt user for this location.

       3. Extract distributions.  Warn if BIN distribution not among those
          selected.

       4. If BIN extracted, do fixups - read in old sysconfig and try to
       intelligently merge the old values into the new sysconfig (only replace
       something if set in old and still defaulted or disabled in new).

       Some fixups might be:  copy these files back from old:  passwd files, group file, fstab, exports, hosts,
       make.conf, host.conf, ???

       Spawn a shell and invite user to look around before exiting.
       */
    return RET_SUCCESS;
}
  
int
installExpress(char *str)
{
    msgConfirm("In the next menu, you will need to set up a DOS-style\n"
	       "(\"fdisk\") partitioning scheme for your hard disk.  If you\n"
	       "don't want to do anything special, just type `A' to use the\n"
	       "whole disk and then `Q' to quit.  If you wish to share\n"
	       "a disk with multiple operating systems, do NOT use the\n"
	       "`A' command.");
    if (diskPartitionEditor("express") == RET_FAIL)
	return RET_FAIL;
    
    msgConfirm("Next, you need to lay out BSD partitions inside of the\n"
	       "fdisk partition just created.  If you don't want to\n"
	       "do anything special, just type `A' to use the default\n"
	       "partitioning scheme and then `Q' to quit.");
    if (diskLabelEditor("express") == RET_FAIL)
	return RET_FAIL;
    
    msgConfirm("Now it is time to select an installation subset.  There\n"
	       "are many different configurations, ranging from minimal\n"
	       "installation sets to full X developer oriented configs.\n"
	       "You can also select a custom software set if none of the\n"
	       "default configurations are suitable.");
    
    while (1) {
	if (!dmenuOpenSimple(&MenuInstallType))
	    return RET_FAIL;

	if (Dists || !msgYesNo("No distributions selected.  Are you sure you wish to continue?"))
	    break;
    }

    msgConfirm("Finally, you must specify an installation medium.");
    if (!dmenuOpenSimple(&MenuMedia))
	return RET_FAIL;
    
    if (installCommit("express") == RET_FAIL)
	return RET_FAIL;

    if (msgYesNo("Since you're running the express installation, a few\n"
		 "post-configuration questions will be asked at this point.\n\n"
		 "Our packages collection contains many useful utilities, from\n"
		 "text editors to WEB servers, and is definitely worth browsing\n"
		 "through even if you don't install any of it for now.\n\n"
		 "Would you like to browse the selection of packaged\n"
		 "software at this time?"))
	configPackages(NULL);

    if (msgYesNo("Would you like to configure any additional network services?"))
	dmenuOpenSimple(&MenuNetworking);

    /* Put whatever other nice configuration questions you'd like to ask the user here */

    /* Final menu of last resort */
    if (msgYesNo("Would you like to go to the general configuration menu for\n"
		 "any last additional configuration options?"))
	dmenuOpenSimple(&MenuConfigure);
    return 0;
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

    i = RET_SUCCESS;
    if (RunningAsInit) {
	if (installInitial() == RET_FAIL)
	    i = RET_FAIL;
	else if (configFstab() == RET_FAIL)
	    i = RET_FAIL;
	else if (!root_extract()) {
	    msgConfirm("Failed to load the ROOT distribution.  Please correct\n"
		       "this problem and try again.");
	    i = RET_FAIL;
	}
    }

    if (i != RET_FAIL && distExtractAll(NULL) == RET_FAIL)
	i = RET_FAIL;

    if (i != RET_FAIL && installFixup() == RET_FAIL)
	i = RET_FAIL;

    dialog_clear();
    /* We get a NULL value for str if run from installExpress(), in which case we don't want to print the following */
    if (str) {
	if (Dists || i == RET_FAIL)
	    msgConfirm("Installation completed with some errors.  You may wish to\n"
		       "scroll through the debugging messages on ALT-F2 with the\n"
		       "scroll-lock feature.  Press [ENTER] to return to the\n"
		       "installation menu.");
	else
	    msgConfirm("Installation completed successfully, now  press [ENTER] to return\n"
		       "to the main menu. If you have any network devices you have not yet\n"
		       "configured, see the Interface configuration item on the\n"
		       "Configuration menu.");
    }
    return i;
}

int
installFixup(void)
{
    Device **devs;
    int i;

    if (!file_readable("/kernel") && file_readable("/kernel.GENERIC")) {
	if (vsystem("ln -f /kernel.GENERIC /kernel")) {
	    msgConfirm("Unable to link /kernel into place!");
	    return RET_FAIL;
	}
    }
    else {
	msgConfirm("Can't find a kernel image to link to on the root file system!\n"
		   "You're going to have a hard time getting this system to\n"
		   "boot from the hard disk, I'm afraid!");
	return RET_FAIL;
    }
    /* Resurrect /dev after bin distribution screws it up */
    if (RunningAsInit) {
	msgNotify("Remaking all devices.. Please wait!");
	if (vsystem("cd /dev; sh MAKEDEV all")) {
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
			msgConfirm("Unable to make slice entries for %s!", c1->name);
			return RET_FAIL;
		    }
		}
	    }
	}
    }

    /* XXX Do all the last ugly work-arounds here which we'll try and excise someday right?? XXX */
    /* BOGON #1:  XFree86 extracting /usr/X11R6 with root-only perms */
    if (file_readable("/usr/X11R6"))
	chmod("/usr/X11R6", 0755);

    /* BOGON #2: We leave /etc in a bad state */
    chmod("/etc", 0755);
    return RET_SUCCESS;
}

/* Go newfs and/or mount all the filesystems we've been asked to */
int
installFilesystems(void)
{
    int i;
    Disk *disk;
    Chunk *c1, *c2, *rootdev, *swapdev, *usrdev;
    Device **devs;
    PartInfo *root;
    char dname[40];
    extern int MakeDevChunk(Chunk *c, char *n);

    if (!checkLabels(&rootdev, &swapdev, &usrdev))
	return RET_FAIL;
    root = (PartInfo *)rootdev->private;
    command_clear();

    /* First, create and mount the root device */
    sprintf(dname, "/dev/%s", rootdev->name);
    if (!MakeDevChunk(rootdev, "/dev") || !file_readable(dname)) {
	msgConfirm("Unable to make device node for %s in /dev!\n"
		   "The installation will be aborted.", rootdev->name);
	return RET_FAIL;
    }

    if (strcmp(root->mountpoint, "/"))
	msgConfirm("Warning: %s is marked as a root partition but is mounted on %s", rootdev->name, root->mountpoint);

    if (root->newfs) {
	int i;

	msgNotify("Making a new root filesystem on %s", rootdev->name);
	i = vsystem("%s /dev/r%s", root->newfs_cmd, rootdev->name);
	if (i) {
	    msgConfirm("Unable to make new root filesystem on /dev/r%s!\n"
		       "Command returned status %d", rootdev->name, i);
	    return RET_FAIL;
	}
    }
    else {
	msgConfirm("Warning:  You have selected a Read-Only root device and\n"
		   "and may be unable to find the appropriate device entries\n"
		   "on it if it is from an older pre-slice version of FreeBSD.");
	msgNotify("Checking integrity of existing %s filesystem.", rootdev->name);
	i = vsystem("fsck -y /dev/r%s", rootdev->name);
	if (i)
	    msgConfirm("Warning: fsck returned status of %d for /dev/r%s.\n"
		       "This partition may be unsafe to use.", i, rootdev->name);
    }
    if (Mount("/mnt", dname)) {
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
	    msgConfirm("No chunk list found for %s!", disk->name);
	    return RET_FAIL;
	}
	if (root->newfs) {
	    Mkdir("/mnt/dev", NULL);
	    MakeDevDisk(disk, "/mnt/dev");
	}

	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private) {
			PartInfo *tmp = (PartInfo *)c2->private;

			if (!strcmp(tmp->mountpoint, "/"))
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
			else
			    msgConfirm("Unable to add %s as a swap device: %s", fname, strerror(errno));
		    }
		}
	    }
	    else if (c1->type == fat && c1->private && root->newfs) {
		char name[FILENAME_MAX];

		sprintf(name, "/mnt%s", ((PartInfo *)c1->private)->mountpoint);
		Mkdir(name, NULL);
	    }
	}
    }

    /* Copy the boot floppy's dev files */
    if (root->newfs && vsystem("find -x /dev | cpio -pdmv /mnt")) {
	msgConfirm("Couldn't clone the /dev files!");
	return RET_FAIL;
    }
    
    command_sort();
    command_execute();
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
	msgConfirm("Copy returned error status of %d!", i);
	return FALSE;
    }

    /* Copy the /etc files into their rightful place */
    if (vsystem("cd /mnt/stand; find etc | cpio -pdmv /mnt")) {
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
	    msgDebug("Attempting to extract root image from %s device\n", mediaDevice->description);
	switch(mediaDevice->type) {

	case DEVICE_TYPE_FLOPPY:
	    alreadyExtracted = loop_on_root_floppy();
	    break;

	default:
	    if (!mediaDevice->init(mediaDevice))
		break;
	    fd = mediaDevice->get(mediaDevice, "floppies/root.flp", NULL);
	    if (fd < 0) {
		msgConfirm("Couldn't get root image from %s!\nWill try to get it from floppy.", mediaDevice->name);
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

    if (!file_readable("/usr/share/misc/termcap")) {
	Mkdir("/usr/share/misc", NULL);
	fp = fopen("/usr/share/misc/termcap", "w");
	if (!fp) {
	    msgConfirm("Unable to initialize termcap file. Some screen-oriented\n"
		       "utilities may not work.");
	    return;
	}
	cp = caps;
	while (*cp)
	    fprintf(fp, "%s\n", *(cp++));
	fclose(fp);
    }
}

/* Specify which release to load from FTP or CD */
int
installSelectRelease(char *str)
{
    char *cp;
    int i;

    dialog_clear();
    if ((cp = msgGetInput(variable_get(RELNAME), "Please specify the release you wish to load")) != NULL) {
	variable_set2(RELNAME, cp);
	i = RET_SUCCESS;
    }
    else
	i = RET_FAIL;
    dialog_clear();
    return i;
}

