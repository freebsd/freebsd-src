/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
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
#include <ctype.h>
#include <sys/consio.h>
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/param.h>
#define MSDOSFS
#include <sys/mount.h>
#include <ufs/ufs/ufsmount.h>
#include <fs/msdosfs/msdosfsmount.h>
#undef MSDOSFS
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <libdisk.h>
#include <limits.h>
#include <unistd.h>
#include <termios.h>

/* Hack for rsaref package add, which displays interactive license.
 * Used by package.c
 */
int _interactiveHack;
int FixItMode = 0;
int NCpus;

static void	create_termcap(void);
static void	fixit_common(void);
int		fixit_livefs_common(dialogMenuItem *self);

#define TERMCAP_FILE	"/usr/share/misc/termcap"

static void	installConfigure(void);

Boolean
checkLabels(Boolean whinge)
{
    Device **devs;
    Boolean status;
    Disk *disk;
    PartInfo *pi;
    Chunk *c1, *c2;
    int i;

    /* Don't allow whinging if noWarn is set */
    if (variable_get(VAR_NO_WARN))
	whinge = FALSE;

    status = TRUE;
    HomeChunk = RootChunk = SwapChunk = NULL;
    TmpChunk = UsrChunk = VarChunk = NULL;
#ifdef __ia64__
    EfiChunk = NULL;
#endif

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
#ifdef __ia64__
	    c2 = c1;
#elif defined(__powerpc__)
	    if (c1->type == apple) {
		for (c2 = c1->part; c2; c2 = c2->next) {
#else
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
#endif

		    pi = (PartInfo *)c2->private_data;
		    if (c2->type == part && c2->subtype != FS_SWAP && pi != NULL) {
			if (!strcmp(pi->mountpoint, "/")) {
			    if (RootChunk) {
				if (whinge)
				    msgConfirm("WARNING:  You have more than one root device set?!\n"
					       "Using the first one found.");
				continue;
			    }
			    else {
				RootChunk = c2;
				if (isDebug())
				    msgDebug("Found rootdev at %s!\n", RootChunk->name);
			    }
			}
			else if (!strcmp(pi->mountpoint, "/usr")) {
			    if (UsrChunk) {
				if (whinge)
				    msgConfirm("WARNING:  You have more than one /usr filesystem.\n"
					       "Using the first one found.");
				continue;
			    }
			    else {
				UsrChunk = c2;
				if (isDebug())
				    msgDebug("Found usrdev at %s!\n", UsrChunk->name);
			    }
			}
			else if (!strcmp(pi->mountpoint, "/var")) {
			    if (VarChunk) {
				if (whinge)
				    msgConfirm("WARNING:  You have more than one /var filesystem.\n"
					       "Using the first one found.");
				continue;
			    }
			    else {
				VarChunk = c2;
				if (isDebug())
				    msgDebug("Found vardev at %s!\n", VarChunk->name);
			    }
			} else if (!strcmp(pi->mountpoint, "/tmp")) {
			    if (TmpChunk) {
				if (whinge)
				    msgConfirm("WARNING:  You have more than one /tmp filesystem.\n"
					       "Using the first one found.");
				continue;
			    }
			    else {
				TmpChunk = c2;
				if (isDebug())
				    msgDebug("Found tmpdev at %s!\n", TmpChunk->name);
			    }
			} else if (!strcmp(pi->mountpoint, "/home")) {
			    if (HomeChunk) {
				if (whinge)
				    msgConfirm("WARNING:  You have more than one /home filesystem.\n"
					       "Using the first one found.");
				continue;
			    }
			    else {
				HomeChunk = c2;
				if (isDebug())
				    msgDebug("Found homedev at %s!\n", HomeChunk->name);
			    }
			}
		    }
#ifndef __ia64__
		}
	    }
#endif
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

#ifdef __ia64__
	    c2 = c1;
#elif defined(__powerpc__)
	    if (c1->type == apple) {
		for (c2 = c1->part; c2; c2 = c2->next) {
#else
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
#endif
		    if (c2->type == part && c2->subtype == FS_SWAP && !SwapChunk) {
			SwapChunk = c2;
			if (isDebug())
			    msgDebug("Found swapdev at %s!\n", SwapChunk->name);
			break;
		    }
#ifndef __ia64__
		}
	    }
#endif
	}
    }

#ifdef __ia64__
    for (i = 0; devs[i] != NULL; i++) {
	if (!devs[i]->enabled)
	    continue;
	disk = (Disk *)devs[i]->private;
	for (c1 = disk->chunks->part; c1 != NULL; c1 = c1->next) {
		pi = (PartInfo *)c1->private_data;
	    if (c1->type == efi && pi != NULL && pi->mountpoint[0] == '/')
		EfiChunk = c1;
	}
    }
#endif

    if (!RootChunk && whinge) {
	msgConfirm("No root device found - you must label a partition as /\n"
		   "in the label editor.");
	status = FALSE;
    }
    if (!SwapChunk && whinge) {
	if (msgYesNo("No swap devices found - you should create at least one\n"
		     "swap partition.  Without swap, the install will fail\n"
		     "if you do not have enough RAM.  Continue anyway?"))
	    status = FALSE;
    }
#ifdef __ia64__
    if (EfiChunk == NULL && whinge) {
	if (msgYesNo("No (mounted) EFI system partition found. Is this what you want?"))
	    status = FALSE;
    }
#endif
    return status;
}

static int
installInitial(void)
{
    static Boolean alreadyDone = FALSE;
    int status = DITEM_SUCCESS;

    if (alreadyDone)
	return DITEM_SUCCESS;

    if (!variable_get(DISK_LABELLED)) {
	msgConfirm("You need to assign disk labels before you can proceed with\n"
		   "the installation.");
	return DITEM_FAILURE;
    }
    /* If it's labelled, assume it's also partitioned */
    if (!variable_get(DISK_PARTITIONED))
	variable_set2(DISK_PARTITIONED, "yes", 0);

    /* If we refuse to proceed, bail. */
    dialog_clear_norefresh();
    if (!variable_get(VAR_NO_WARN)) {
	if (msgYesNo(
	    "Last Chance!  Are you SURE you want continue the installation?\n\n"
	    "If you're running this on a disk with data you wish to save\n"
	    "then WE STRONGLY ENCOURAGE YOU TO MAKE PROPER BACKUPS before\n"
	    "proceeding!\n\n"
	    "We can take no responsibility for lost disk contents!") != 0)
	return DITEM_FAILURE;
    }

    if (DITEM_STATUS(diskLabelCommit(NULL)) != DITEM_SUCCESS) {
	msgConfirm("Couldn't make filesystems properly.  Aborting.");
	return DITEM_FAILURE;
    }

    if (!copySelf()) {
	msgConfirm("installInitial: Couldn't clone the boot floppy onto the\n"
		   "root file system.  Aborting!");
	return DITEM_FAILURE;
    }

    if (!Restarting && chroot("/mnt") == -1) {
	msgConfirm("installInitial: Unable to chroot to %s - this is bad!",
		   "/mnt");
	return DITEM_FAILURE;
    }

    chdir("/");
    variable_set2(RUNNING_ON_ROOT, "yes", 0);

    /* Configure various files in /etc */
    if (DITEM_STATUS(configResolv(NULL)) == DITEM_FAILURE)
	status = DITEM_FAILURE;
    if (DITEM_STATUS(configFstab(NULL)) == DITEM_FAILURE)
	status = DITEM_FAILURE;

    /* stick a helpful shell over on the 4th VTY */
    if (!variable_get(VAR_NO_HOLOSHELL))
	systemCreateHoloshell();

    alreadyDone = TRUE;
    return status;
}

int
installFixitHoloShell(dialogMenuItem *self)
{
    FixItMode = 1;
    systemCreateHoloshell();
    FixItMode = 0;
    return DITEM_SUCCESS;
}

/*
 * Load the live filesystem from USB media.
 */
int
installFixitUSB(dialogMenuItem *self)
{
	if (!RunningAsInit)
		return (DITEM_SUCCESS);

	variable_set2(SYSTEM_STATE, "fixit", 0);

	if (DITEM_STATUS(mediaSetUSB(NULL)) != DITEM_SUCCESS ||
	    !DEVICE_INIT(mediaDevice)) {
		msgConfirm("No USB devices found!");
		return (DITEM_FAILURE);
	} else if (!file_readable("/dist/rescue/ldconfig")) {
		msgConfirm("Unable to find a FreeBSD live filesystem.");
		return (DITEM_FAILURE);
	}

	if (DITEM_STATUS(fixit_livefs_common(self)) == DITEM_FAILURE)
		return (DITEM_FAILURE);

	mediaClose();
	return (DITEM_SUCCESS);
}

int
installFixitCDROM(dialogMenuItem *self)
{
    int need_eject;

    if (!RunningAsInit)
	return DITEM_SUCCESS;

    variable_set2(SYSTEM_STATE, "fixit", 0);
    need_eject = 0;
    CDROMInitQuiet = 1;
    while (1) {
	if (need_eject)
	    msgConfirm(
	"Please insert a FreeBSD live filesystem CD/DVD and press return");
	if (DITEM_STATUS(mediaSetCDROM(NULL)) != DITEM_SUCCESS
	    || !DEVICE_INIT(mediaDevice)) {
	    /* If we can't initialize it, it's probably not a FreeBSD CDROM so punt on it */
	    mediaClose();
	    if (need_eject && msgYesNo("Unable to mount the disc. Do you want to try again?") != 0)
		return DITEM_FAILURE;
	} else if (!file_readable("/dist/rescue/ldconfig")) {
		mediaClose();
		if (need_eject &&
		    msgYesNo("Unable to find a FreeBSD live filesystem. Do you want to try again?") != 0)
		    return DITEM_FAILURE;
	} else
	    break;
	CDROMInitQuiet = 0;
	need_eject = 1;
    }
    CDROMInitQuiet = 0;

    if (DITEM_STATUS(fixit_livefs_common(self)) == DITEM_FAILURE)
	return (DITEM_FAILURE);

    mediaClose();
    if (need_eject)
	msgConfirm("Please remove the FreeBSD fixit CDROM/DVD now.");
    return DITEM_SUCCESS;
}

int
installFixitFloppy(dialogMenuItem *self)
{
    struct ufs_args args;
    extern char *distWanted;

    if (!RunningAsInit)
	return DITEM_SUCCESS;

    /* Try to open the floppy drive */
    if (DITEM_STATUS(mediaSetFloppy(NULL)) == DITEM_FAILURE || !mediaDevice) {
	msgConfirm("Unable to set media device to floppy.");
	mediaClose();
	return DITEM_FAILURE;
    }

    memset(&args, 0, sizeof(args));
    args.fspec = mediaDevice->devname;
    mediaDevice->private = "/mnt2";
    distWanted = NULL;
    Mkdir("/mnt2");

    variable_set2(SYSTEM_STATE, "fixit", 0);

    while (1) {
	if (!DEVICE_INIT(mediaDevice)) {
	    if (msgYesNo("The attempt to mount the fixit floppy failed, bad floppy\n"
			 "or unclean filesystem.  Do you want to try again?"))
		return DITEM_FAILURE;
	}
	else
	    break;
    }
    if (!directory_exists("/tmp"))
	(void)symlink("/mnt2/tmp", "/tmp");
    fixit_common();
    mediaClose();
    msgConfirm("Please remove the fixit floppy now.");
    return DITEM_SUCCESS;
}

/*
 * The common code for both fixit variants.
 */
static void
fixit_common(void)
{
    pid_t child;
    int waitstatus;

    if (!directory_exists("/var/tmp/vi.recover")) {
	if (DITEM_STATUS(Mkdir("/var/tmp/vi.recover")) != DITEM_SUCCESS) {
	    msgConfirm("Warning:  Was unable to create a /var/tmp/vi.recover directory.\n"
		       "vi will kvetch and moan about it as a result but should still\n"
		       "be essentially usable.");
	}
    }
    if (!directory_exists("/bin"))
	(void)Mkdir("/bin");
    (void)symlink("/stand/sh", "/bin/sh");
    /* Link the /etc/ files */
    if (DITEM_STATUS(Mkdir("/etc")) != DITEM_SUCCESS)
	msgConfirm("Unable to create an /etc directory!  Things are weird on this floppy..");
    else if ((symlink("/mnt2/etc/spwd.db", "/etc/spwd.db") == -1 && errno != EEXIST) ||
	     (symlink("/mnt2/etc/protocols", "/etc/protocols") == -1 && errno != EEXIST) ||
	     (symlink("/mnt2/etc/group", "/etc/group") == -1 && errno != EEXIST) ||
	     (symlink("/mnt2/etc/services", "/etc/services") == -1 && errno != EEXIST))
	msgConfirm("Couldn't symlink the /etc/ files!  I'm not sure I like this..");
    if (!file_readable(TERMCAP_FILE))
	create_termcap();
    if (strcmp(variable_get(VAR_FIXIT_TTY), "serial") == 0) 
	systemSuspendDialog();	/* must be before the fork() */
    if (!(child = fork())) {
	int i, fd;
	struct termios foo;
	extern int login_tty(int);

	ioctl(0, TIOCNOTTY, NULL);
	for (i = getdtablesize(); i >= 0; --i)
	    close(i);

	if (strcmp(variable_get(VAR_FIXIT_TTY), "serial") == 0) 
	    fd = open("/dev/console", O_RDWR);
	else
	    fd = open("/dev/ttyv3", O_RDWR);
	ioctl(0, TIOCSCTTY, &fd);
	dup2(0, 1);
	dup2(0, 2);
	DebugFD = 2;
	if (login_tty(fd) == -1)
	    msgDebug("fixit: I can't set the controlling terminal.\n");

	signal(SIGTTOU, SIG_IGN);
	if (tcgetattr(0, &foo) != -1) {
	    foo.c_cc[VERASE] = '\010';
	    if (tcsetattr(0, TCSANOW, &foo) == -1)
		msgDebug("fixit shell: Unable to set erase character.\n");
	}
	else
	    msgDebug("fixit shell: Unable to get terminal attributes!\n");
	setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/stand:"
	       "/mnt2/stand:/mnt2/bin:/mnt2/sbin:/mnt2/usr/bin:/mnt2/usr/sbin", 1);
	if (strcmp(variable_get(VAR_FIXIT_TTY), "serial") == 0) {
	    printf("Waiting for fixit shell to exit.\n"
		"When you are done, type ``exit'' to exit\n"
		"the fixit shell and be returned here.\n\n");
	    fflush(stdout);
	} else {
	    ioctl(fd, VT_ACTIVATE, 0);
	}

	/* use the .profile from the fixit medium */
	setenv("HOME", "/mnt2", 1);
	chdir("/mnt2");
	execlp("sh", "-sh", (char *)0);
	msgDebug("fixit shell: Failed to execute shell!\n");
	_exit(1);;
    }
    else {
	if (strcmp(variable_get(VAR_FIXIT_TTY), "standard") == 0) {
	    dialog_clear_norefresh();
	    msgNotify("Waiting for fixit shell to exit.  Go to VTY4 now by\n"
		"typing ALT-F4.  When you are done, type ``exit'' to exit\n"
		"the fixit shell and be returned here.\n");
	}
	(void)waitpid(child, &waitstatus, 0);
	if (strcmp(variable_get(VAR_FIXIT_TTY), "serial") == 0)
	    systemResumeDialog();
	else if (OnVTY) {
	    ioctl(0, VT_ACTIVATE, 0);
	    msgInfo(NULL);
	}
    }
    dialog_clear();
}

/*
 * Some path/lib setup is required for the livefs fixit image. Since there's
 * more than one media type for livefs now, this has been broken off into it's
 * own function.
 */
int
fixit_livefs_common(dialogMenuItem *self)
{
	struct stat sb;

	/*
	 * USB and CDROM media get mounted to /dist, but fixit code looks in
	 * /mnt2.
	 */
	unlink("/mnt2");
	rmdir("/mnt2");

	if (symlink("/dist", "/mnt2")) {
		msgConfirm("Unable to symlink /mnt2 to the disc mount point.");
		return (DITEM_FAILURE);
	}

	/*
	 * If /tmp points to /mnt2/tmp from a previous fixit floppy session,
	 * recreate it.
	 */
	if (lstat("/tmp", &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFLNK)
		unlink("/tmp");
	Mkdir("/tmp");

	/* Generate a new ld.so.hints */
	if (!file_readable("/var/run/ld.so.hints")) {
		Mkdir("/var/run");
		if (vsystem("/mnt2/rescue/ldconfig -s /mnt2/lib "
		    "/mnt2/usr/lib")) {
			msgConfirm("Warning: ldconfig could not create the "
			    "ld.so hints file.\nDynamic executables from the "
			    "disc likely won't work.");
		}
	}

	/* Create required libexec symlinks. */
	Mkdir("/libexec");
	if (!file_readable("/libexec/ld.so") &&
	    file_readable("/mnt2/libexec/ld.so")) {
		if (symlink("/mnt2/libexec/ld.so", "/libexec/ld.so"))
			msgDebug("Couldn't link to ld.so\n");
	}

	if (!file_readable("/libexec/ld-elf.so.1")) {
		if (symlink("/mnt2/libexec/ld-elf.so.1",
		    "/libexec/ld-elf.so.1")) {
			msgConfirm("Warning: could not create the symlink for "
			    "ld-elf.so.1\nDynamic executables from the disc "
			    "likely won't work.");
		}
	}

	/* $PATH doesn't include /mnt2 by default. Create convenient symlink. */
	if (!file_readable("/usr/bin/vi"))
		symlink("/mnt2/usr/bin/vi", "/usr/bin/vi");

	/* Shared code used by all fixit types. */
	fixit_common();

	return (DITEM_SUCCESS);
}

int
installExpress(dialogMenuItem *self)
{
    int i;

    dialog_clear_norefresh();
    variable_set2(SYSTEM_STATE, "express", 0);
#ifdef WITH_SLICES
    if (DITEM_STATUS((i = diskPartitionEditor(self))) == DITEM_FAILURE)
	return i;
#endif
    
    if (DITEM_STATUS((i = diskLabelEditor(self))) == DITEM_FAILURE)
	return i;

    if (DITEM_STATUS((i = installCommit(self))) == DITEM_SUCCESS) {
	i |= DITEM_LEAVE_MENU;

	/* Give user the option of one last configuration spree */
	installConfigure();
    }
    return i;
}

/* Standard mode installation */
int
installStandard(dialogMenuItem *self)
{
    int i;
#ifdef WITH_SLICES
    int tries = 0;
    Device **devs;
#endif

    variable_set2(SYSTEM_STATE, "standard", 0);
    dialog_clear_norefresh();
#ifdef WITH_SLICES
    msgConfirm("In the next menu, you will need to set up a DOS-style (\"fdisk\") partitioning\n"
	       "scheme for your hard disk.  If you simply wish to devote all disk space\n"
	       "to FreeBSD (overwriting anything else that might be on the disk(s) selected)\n"
	       "then use the (A)ll command to select the default partitioning scheme followed\n"
	       "by a (Q)uit.  If you wish to allocate only free space to FreeBSD, move to a\n"
	       "partition marked \"unused\" and use the (C)reate command.");

nodisks:
    if (DITEM_STATUS(diskPartitionEditor(self)) == DITEM_FAILURE)
	return DITEM_FAILURE;

    if (diskGetSelectCount(&devs) <= 0 && tries < 3) {
	msgConfirm("You need to select some disks to operate on!  Be sure to use SPACE\n"
		   "instead of RETURN in the disk selection menu when selecting a disk.");
	++tries;
	goto nodisks;
    }

    msgConfirm("Now you need to create BSD partitions inside of the fdisk partition(s)\n"
	       "just created.  If you have a reasonable amount of disk space (1GB or more)\n"
	       "and don't have any special requirements, simply use the (A)uto command to\n"
	       "allocate space automatically.  If you have more specific needs or just don't\n"
	       "care for the layout chosen by (A)uto, press F1 for more information on\n"
	       "manual layout.");
#else
    msgConfirm("First you need to create BSD partitions on the disk which you are\n"
	       "installing to.  If you have a reasonable amount of disk space (1GB or more)\n"
	       "and don't have any special requirements, simply use the (A)uto command to\n"
	       "allocate space automatically.  If you have more specific needs or just don't\n"
	       "care for the layout chosen by (A)uto, press F1 for more information on\n"
	       "manual layout.");
#endif

    if (DITEM_STATUS(diskLabelEditor(self)) == DITEM_FAILURE)
	return DITEM_FAILURE;

    if (DITEM_STATUS((i = installCommit(self))) == DITEM_FAILURE) {
	dialog_clear();
	msgConfirm("Installation completed with some errors.  You may wish to\n"
		   "scroll through the debugging messages on VTY1 with the\n"
		   "scroll-lock feature.  You can also choose \"No\" at the next\n"
		   "prompt and go back into the installation menus to retry\n"
		   "whichever operations have failed.");
	return i;

    }
    else {
	dialog_clear();
	msgConfirm("Congratulations!  You now have FreeBSD installed on your system.\n\n"
		   "We will now move on to the final configuration questions.\n"
		   "For any option you do not wish to configure, simply select\n"
		   "No.\n\n"
		   "If you wish to re-enter this utility after the system is up, you\n"
		   "may do so by typing: /usr/sbin/sysinstall.");
    }
    if (mediaDevice->type != DEVICE_TYPE_FTP && mediaDevice->type != DEVICE_TYPE_NFS) {
	if (!msgYesNo("Would you like to configure any Ethernet or SLIP/PPP network devices?")) {
	    Device *tmp = tcpDeviceSelect();

	    if (tmp && !((DevInfo *)tmp->private)->use_dhcp && !msgYesNo("Would you like to bring the %s interface up right now?", tmp->name))
		if (!DEVICE_INIT(tmp))
		    msgConfirm("Initialization of %s device failed.", tmp->name);
	}
	dialog_clear_norefresh();
    }

    if (!msgNoYes("Do you want this machine to function as a network gateway?"))
	variable_set2("gateway_enable", "YES", 1);

    dialog_clear_norefresh();
    if (!msgNoYes("Do you want to configure inetd and the network services that it provides?"))
        configInetd(self);

    dialog_clear_norefresh();
    if (!msgNoYes("Would you like to enable SSH login?"))
	variable_set2("sshd_enable", "YES", 1);

    dialog_clear_norefresh();
    if (!msgNoYes("Do you want to have anonymous FTP access to this machine?"))
	configAnonFTP(self);

    dialog_clear_norefresh();
    if (!msgNoYes("Do you want to configure this machine as an NFS server?"))
	configNFSServer(self);

    dialog_clear_norefresh();
    if (!msgNoYes("Do you want to configure this machine as an NFS client?"))
	variable_set2("nfs_client_enable", "YES", 1);

#ifdef WITH_SYSCONS
    dialog_clear_norefresh();
    if (!msgNoYes("Would you like to customize your system console settings?"))
	dmenuOpenSimple(&MenuSyscons, FALSE);
#endif

    dialog_clear_norefresh();
    if (!msgYesNo("Would you like to set this machine's time zone now?"))
	systemExecute("tzsetup");

#ifdef WITH_MICE
    dialog_clear_norefresh();
    if (!msgNoYes("Does this system have a PS/2, serial, or bus mouse?"))
	dmenuOpenSimple(&MenuMouse, FALSE);
#endif

#ifdef __i386__
    if (checkLoaderACPI() != 0) {
    	dialog_clear_norefresh();
    	if (!msgNoYes("ACPI was disabled during boot.\n"
		      "Would you like to disable it permanently?"))
		(void)configLoaderACPI(1 /*disable*/);
    }
#endif

    /* Now would be a good time to checkpoint the configuration data */
    configRC_conf();
    sync();

    dialog_clear_norefresh();
    if (!msgYesNo("The FreeBSD package collection is a collection of thousands of ready-to-run\n"
		  "applications, from text editors to games to WEB servers and more.  Would you\n"
		  "like to browse the collection now?")) {
	(void)configPackages(self);
    }

    if (!msgYesNo("Would you like to add any initial user accounts to the system?\n"
		  "Adding at least one account for yourself at this stage is suggested\n"
		  "since working as the \"root\" user is dangerous (it is easy to do\n"
		  "things which adversely affect the entire system)."))
	(void)configUsers(self);

    msgConfirm("Now you must set the system manager's password.\n"
	       "This is the password you'll use to log in as \"root\".");
    if (!systemExecute("passwd root"))
	variable_set2("root_password", "YES", 0);

    /* XXX Put whatever other nice configuration questions you'd like to ask the user here XXX */

    /* Give user the option of one last configuration spree */
    dialog_clear_norefresh();
    installConfigure();
    return DITEM_LEAVE_MENU;
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
 * then use ftp to load a different dist.
 */
int
installCommit(dialogMenuItem *self)
{
    int i;
    char *str;

    dialog_clear_norefresh();
    if (!Dists)
	distConfig(NULL);

    if (!Dists) {
	(void)dmenuOpenSimple(&MenuDistributions, FALSE);
	/* select reasonable defaults if necessary */
	if (!Dists)
	    Dists = _DIST_USER;
	if (!KernelDists)
	    KernelDists = selectKernel();
    }

    if (!mediaVerify())
	return DITEM_FAILURE;

    str = variable_get(SYSTEM_STATE);
    if (isDebug())
	msgDebug("installCommit: System state is `%s'\n", str);

    /* Installation stuff we wouldn't do to a running system */
    if (RunningAsInit && DITEM_STATUS((i = installInitial())) == DITEM_FAILURE)
	return i;

try_media:
    if (!DEVICE_INIT(mediaDevice)) {
	if (!msgYesNo("Unable to initialize selected media. Would you like to\n"
		      "adjust your media configuration and try again?")) {
	    mediaDevice = NULL;
	    if (!mediaVerify())
		return DITEM_FAILURE;
	    else
		goto try_media;
	}
	else
	    return DITEM_FAILURE;
    }

    /* Now go get it all */
    i = distExtractAll(self);

    /* When running as init, *now* it's safe to grab the rc.foo vars */
    installEnvironment();

    variable_set2(SYSTEM_STATE, DITEM_STATUS(i) == DITEM_FAILURE ? "error-install" : "full-install", 0);

    return i;
}

static void
installConfigure(void)
{
    /* Final menu of last resort */
    if (!msgNoYes("Visit the general configuration menu for a chance to set\n"
		  "any last options?"))
	dmenuOpenSimple(&MenuConfigure, FALSE);
    configRC_conf();
    sync();
}

int
installFixupBase(dialogMenuItem *self)
{
#if defined(__i386__) || defined(__amd64__)
    FILE *fp;
#endif
#ifdef __ia64__
    const char *efi_mntpt;
#endif

    /* All of this is done only as init, just to be safe */
    if (RunningAsInit) {
#if defined(__i386__) || defined(__amd64__)
	if ((fp = fopen("/boot/loader.conf", "a")) != NULL) {
	    if (!OnVTY) {
		fprintf(fp, "# -- sysinstall generated deltas -- #\n");
		fprintf(fp, "console=\"comconsole\"\n");
	    }
	    fclose(fp);
	}
#endif
	
	/* BOGON #2: We leave /etc in a bad state */
	chmod("/etc", 0755);
	
	/* BOGON #3: No /var/db/mountdtab complains */
	Mkdir("/var/db");
	creat("/var/db/mountdtab", 0644);
	
	/* BOGON #4: /compat created by default in root fs */
	Mkdir("/usr/compat");
	vsystem("ln -s usr/compat /compat");

	/* BOGON #5: aliases database not built for bin */
	vsystem("newaliases");

	/* BOGON #6: Remove /stand (finally) */
	vsystem("rm -rf /stand");

	/* Now run all the mtree stuff to fix things up */
        vsystem("mtree -deU -f /etc/mtree/BSD.root.dist -p /");
        vsystem("mtree -deU -f /etc/mtree/BSD.var.dist -p /var");
        vsystem("mtree -deU -f /etc/mtree/BSD.usr.dist -p /usr");

#ifdef __ia64__
	/* Move /boot to the the EFI partition and make /boot a link to it. */
	efi_mntpt = (EfiChunk != NULL) ? ((PartInfo *)EfiChunk->private_data)->mountpoint : NULL;
	if (efi_mntpt != NULL) {
		vsystem("if [ ! -L /boot ]; then mv /boot %s; fi", efi_mntpt);
		vsystem("if [ ! -e /boot ]; then ln -sf %s/boot /boot; fi",
		    efi_mntpt + 1);	/* Skip leading '/' */
		/* Make sure the kernel knows which partition is the root file system. */
		vsystem("echo 'vfs.root.mountfrom=\"ufs:/dev/%s\"' >> /boot/loader.conf", RootChunk->name);
	}
#endif

	/* Do all the last ugly work-arounds here */
    }
    return DITEM_SUCCESS | DITEM_RESTORE;
}

int
installFixupKernel(dialogMenuItem *self, int dists)
{

    /* All of this is done only as init, just to be safe */
    if (RunningAsInit) {
	/*
	 * Install something as /boot/kernel.
	 *
	 * NB: we assume any existing kernel has been saved
	 *     already and the /boot/kernel we remove is empty.
	 */
	vsystem("rm -rf /boot/kernel");
		vsystem("mv /boot/GENERIC /boot/kernel");
    }
    return DITEM_SUCCESS | DITEM_RESTORE;
}

#define	QUEUE_YES	1
#define	QUEUE_NO	0
static int
performNewfs(PartInfo *pi, char *dname, int queue)
{
	char buffer[LINE_MAX];

	if (pi->do_newfs) {
		switch(pi->newfs_type) {
		case NEWFS_UFS:
			snprintf(buffer, LINE_MAX, "%s %s %s %s %s",
			    NEWFS_UFS_CMD,
			    pi->newfs_data.newfs_ufs.softupdates ?  "-U" : "",
			    pi->newfs_data.newfs_ufs.ufs1 ? "-O1" : "-O2",
			    pi->newfs_data.newfs_ufs.user_options,
			    dname);
			break;

		case NEWFS_MSDOS:
			snprintf(buffer, LINE_MAX, "%s %s", NEWFS_MSDOS_CMD,
			    dname);
			break;

		case NEWFS_CUSTOM:
			snprintf(buffer, LINE_MAX, "%s %s",
			    pi->newfs_data.newfs_custom.command, dname);
			break;
		}

		if (queue == QUEUE_YES) {
			command_shell_add(pi->mountpoint, buffer);
			return (0);
		} else
			return (vsystem(buffer));
	}
	return (0);
}

/* Go newfs and/or mount all the filesystems we've been asked to */
int
installFilesystems(dialogMenuItem *self)
{
    int i;
    Disk *disk;
    Chunk *c1, *c2;
    Device **devs;
    PartInfo *root;
    char dname[80];
    Boolean upgrade = FALSE;

    /* If we've already done this, bail out */
    if (!variable_cmp(DISK_LABELLED, "written"))
	return DITEM_SUCCESS;

    upgrade = !variable_cmp(SYSTEM_STATE, "upgrade");
    if (!checkLabels(TRUE))
	return DITEM_FAILURE;

    root = (RootChunk != NULL) ? (PartInfo *)RootChunk->private_data : NULL;

    command_clear();
    if (SwapChunk && RunningAsInit) {
	/* As the very first thing, try to get ourselves some swap space */
	sprintf(dname, "/dev/%s", SwapChunk->name);
	if (!Fake && !file_readable(dname)) {
	    msgConfirm("Unable to find device node for %s in /dev!\n"
		       "The creation of filesystems will be aborted.", dname);
	    return DITEM_FAILURE;
	}

	if (!Fake) {
	    if (!swapon(dname)) {
		dialog_clear_norefresh();
		msgNotify("Added %s as initial swap device", dname);
	    }
	    else {
		msgConfirm("WARNING!  Unable to swap to %s: %s\n"
			   "This may cause the installation to fail at some point\n"
			   "if you don't have a lot of memory.", dname, strerror(errno));
	    }
	}
    }

    if (RootChunk && RunningAsInit) {
	/* Next, create and/or mount the root device */
	sprintf(dname, "/dev/%s", RootChunk->name);
	if (!Fake && !file_readable(dname)) {
	    msgConfirm("Unable to make device node for %s in /dev!\n"
		       "The creation of filesystems will be aborted.", dname);
	    return DITEM_FAILURE | DITEM_RESTORE;
	}
	if (strcmp(root->mountpoint, "/"))
	    msgConfirm("Warning: %s is marked as a root partition but is mounted on %s", RootChunk->name, root->mountpoint);

	if (root->do_newfs && (!upgrade ||
	    !msgNoYes("You are upgrading - are you SURE you want to newfs "
	    "the root partition?"))) {
	    int i;

	    dialog_clear_norefresh();
	    msgNotify("Making a new root filesystem on %s", dname);
	    i = performNewfs(root, dname, QUEUE_NO);
	    if (i) {
		msgConfirm("Unable to make new root filesystem on %s!\n"
			   "Command returned status %d", dname, i);
		return DITEM_FAILURE | DITEM_RESTORE;
	    }
	}
	else {
	    if (!upgrade) {
		msgConfirm("Warning:  Using existing root partition.");
	    }
	    dialog_clear_norefresh();
	    msgNotify("Checking integrity of existing %s filesystem.", dname);
	    i = vsystem("fsck_ffs -y %s", dname);
	    if (i)
		msgConfirm("Warning: fsck returned status of %d for %s.\n"
			   "This partition may be unsafe to use.", i, dname);
	}

	/*
	 * If soft updates was enabled in the editor but we didn't newfs,
	 * use tunefs to update the soft updates flag on the file system.
	 */
	if (!root->do_newfs && root->newfs_type == NEWFS_UFS &&
	    root->newfs_data.newfs_ufs.softupdates) {
		i = vsystem("tunefs -n enable %s", dname);
		if (i)
			msgConfirm("Warning: Unable to enable soft updates"
			    " for root file system on %s", dname);
	}

	/* Switch to block device */
	sprintf(dname, "/dev/%s", RootChunk->name);
	if (Mount("/mnt", dname)) {
	    msgConfirm("Unable to mount the root file system on %s!  Giving up.", dname);
	    return DITEM_FAILURE | DITEM_RESTORE;
	}

	/* Mount devfs for other partitions to mount */
	Mkdir("/mnt/dev");
	if (!Fake) {
	    struct iovec iov[4];

	    iov[0].iov_base = "fstype";
	    iov[0].iov_len = strlen(iov[0].iov_base) + 1;
	    iov[1].iov_base = "devfs";
	    iov[1].iov_len = strlen(iov[1].iov_base) + 1;
	    iov[2].iov_base = "fspath";
	    iov[2].iov_len = strlen(iov[2].iov_base) + 1;
	    iov[3].iov_base = "/mnt/dev";
	    iov[3].iov_len = strlen(iov[3].iov_base) + 1;
	    i = nmount(iov, 4, 0);

	    if (i) {
		dialog_clear_norefresh();
		msgConfirm("Unable to mount DEVFS (error %d)", errno);
		return DITEM_FAILURE | DITEM_RESTORE;
	    }
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
	    return DITEM_FAILURE | DITEM_RESTORE;
	}
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
#ifdef __ia64__
	if (c1->type == part) {
		c2 = c1;
		{
#elif defined(__powerpc__)
	    if (c1->type == apple) {
		for (c2 = c1->part; c2; c2 = c2->next) {
#else
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
#endif
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private_data) {
			PartInfo *tmp = (PartInfo *)c2->private_data;

			/* Already did root */
			if (c2 == RootChunk)
			    continue;

			sprintf(dname, "%s/dev/%s",
			    RunningAsInit ? "/mnt" : "", c2->name);

			if (tmp->do_newfs && (!upgrade ||
			    !msgNoYes("You are upgrading - are you SURE you"
			    " want to newfs /dev/%s?", c2->name)))
				performNewfs(tmp, dname, QUEUE_YES);
			else
			    command_shell_add(tmp->mountpoint,
				"fsck_ffs -y %s/dev/%s", RunningAsInit ?
				"/mnt" : "", c2->name);
#if 0
			if (tmp->soft)
			    command_shell_add(tmp->mountpoint,
			    "tunefs -n enable %s/dev/%s", RunningAsInit ?
			    "/mnt" : "", c2->name);
#endif
			command_func_add(tmp->mountpoint, Mount, c2->name);
		    }
		    else if (c2->type == part && c2->subtype == FS_SWAP) {
			char fname[80];
			int i;

			if (c2 == SwapChunk)
			    continue;
			sprintf(fname, "%s/dev/%s", RunningAsInit ? "/mnt" : "", c2->name);
			i = (Fake || swapon(fname));
			if (!i) {
			    dialog_clear_norefresh();
			    msgNotify("Added %s as an additional swap device", fname);
			}
			else {
			    msgConfirm("Unable to add %s as a swap device: %s", fname, strerror(errno));
			}
		    }
		}
	    }
	    else if (c1->type == fat && c1->private_data &&
		(root->do_newfs || upgrade)) {
		char name[FILENAME_MAX];

		sprintf(name, "%s/%s", RunningAsInit ? "/mnt" : "", ((PartInfo *)c1->private_data)->mountpoint);
		Mkdir(name);
	    }
#if defined(__ia64__)
	    else if (c1->type == efi && c1->private_data) {
		PartInfo *pi = (PartInfo *)c1->private_data;

		sprintf(dname, "%s/dev/%s", RunningAsInit ? "/mnt" : "",
		    c1->name);

		if (pi->do_newfs && (!upgrade ||
		    !msgNoYes("You are upgrading - are you SURE you want to "
		    "newfs /dev/%s?", c1->name)))
			performNewfs(pi, dname, QUEUE_YES);

		command_func_add(pi->mountpoint, Mount_msdosfs, c1->name);
	    }
#endif
	}
    }

    command_sort();
    command_execute();
    dialog_clear_norefresh();
    return DITEM_SUCCESS | DITEM_RESTORE;
}

/* Initialize various user-settable values to their defaults */
int
installVarDefaults(dialogMenuItem *self)
{
    char *cp, ncpus[10];

    /* Set default startup options */
    cp = getsysctlbyname("kern.osrelease");
    variable_set2(VAR_RELNAME,			cp, 0);
    free(cp);
    variable_set2(VAR_CPIO_VERBOSITY,		"high", 0);
    variable_set2(VAR_INSTALL_ROOT,		"/", 0);
    variable_set2(VAR_INSTALL_CFG,		"install.cfg", 0);
    cp = getenv("EDITOR");
    if (!cp)
	cp = "/usr/bin/ee";
    variable_set2(VAR_EDITOR,			cp, 0);
    variable_set2(VAR_FTP_USER,			"ftp", 0);
    variable_set2(VAR_BROWSER_PACKAGE,		"links", 0);
    variable_set2(VAR_BROWSER_BINARY,		"/usr/local/bin/links", 0);
    variable_set2(VAR_FTP_STATE,		"passive", 0);
    variable_set2(VAR_NFS_SECURE,		"NO", -1);
    variable_set2(VAR_NFS_TCP,   		"NO", -1);
    variable_set2(VAR_NFS_V3,   		"YES", -1);
    if (OnVTY)
	    variable_set2(VAR_FIXIT_TTY,		"standard", 0);
    else
	    variable_set2(VAR_FIXIT_TTY,		"serial", 0);
    variable_set2(VAR_PKG_TMPDIR,		"/var/tmp", 0);
    variable_set2(VAR_MEDIA_TIMEOUT,		itoa(MEDIA_TIMEOUT), 0);
    if (getpid() != 1)
	variable_set2(SYSTEM_STATE,		"update", 0);
    else
	variable_set2(SYSTEM_STATE,		"init", 0);
    variable_set2(VAR_NEWFS_ARGS,		"-b 16384 -f 2048", 0);
    variable_set2(VAR_CONSTERM,                 "NO", 0);
    if (NCpus <= 0)
	NCpus = 1;
    snprintf(ncpus, sizeof(ncpus), "%u", NCpus);
    variable_set2(VAR_NCPUS,			ncpus, 0);
    return DITEM_SUCCESS;
}

/* Load the environment up from various system configuration files */
void
installEnvironment(void)
{
    configEnvironmentRC_conf();
    if (file_readable("/etc/resolv.conf"))
	configEnvironmentResolv("/etc/resolv.conf");
}

/* Copy the boot floppy contents into /stand */
Boolean
copySelf(void)
{
    int i;

    if (file_readable("/boot.help"))
	vsystem("cp /boot.help /mnt");
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
	termcap_cons25r_m, termcap_cons25l1, termcap_cons25l1_m,
	termcap_xterm, NULL,
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
