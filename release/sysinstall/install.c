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
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/param.h>
#define MSDOSFS
#include <sys/mount.h>
#include <ufs/ufs/ufsmount.h>
#include <msdosfs/msdosfsmount.h>
#undef MSDOSFS
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <termios.h>

/* Hack for rsaref package add, which displays interactive license.
 * Used by package.c
 */
int _interactiveHack;
int FixItMode = 0;

static void	create_termcap(void);
static void	fixit_common(void);

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

    /* Don't allow whinging if noWarn is set */
    if (variable_get(VAR_NO_WARN))
	whinge = FALSE;

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
			if (!strcmp(((PartInfo *)c2->private_data)->mountpoint, "/")) {
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
    if (!variable_get(VAR_NO_WARN))
	if (msgYesNo(
	    "Last Chance!  Are you SURE you want continue the installation?\n\n"
	    "If you're running this on a disk with data you wish to save\n"
	    "then WE STRONGLY ENCOURAGE YOU TO MAKE PROPER BACKUPS before\n"
	    "proceeding!\n\n"
	    "We can take no responsibility for lost disk contents!") != 0)
	return DITEM_FAILURE;

    if (DITEM_STATUS(diskLabelCommit(NULL)) != DITEM_SUCCESS) {
	msgConfirm("Couldn't make filesystems properly.  Aborting.");
	return DITEM_FAILURE;
    }

    if (!copySelf()) {
	msgConfirm("installInitial: Couldn't clone the boot floppy onto the\n"
		   "root file system.  Aborting!");
	return DITEM_FAILURE;
    }

    if (chroot("/mnt") == -1) {
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
    systemCreateHoloshell();

    alreadyDone = TRUE;
    return status;
}

int
installFixitHoloShell(dialogMenuItem *self)
{
    FixItMode = 1;
    systemCreateHoloshell();
    return DITEM_SUCCESS;
    FixItMode = 0;
}

int
installFixitCDROM(dialogMenuItem *self)
{
    struct stat sb;

    if (!RunningAsInit)
	return DITEM_SUCCESS;

    variable_set2(SYSTEM_STATE, "fixit", 0);
    (void)unlink("/mnt2");
    (void)rmdir("/mnt2");

    while (1) {
	msgConfirm("Please insert a FreeBSD live filesystem CDROM and press return");
	if (DITEM_STATUS(mediaSetCDROM(NULL)) != DITEM_SUCCESS || !mediaDevice || !mediaDevice->init(mediaDevice)) {
	    /* If we can't initialize it, it's probably not a FreeBSD CDROM so punt on it */
	    mediaClose();
	    if (msgYesNo("Unable to mount the CDROM - do you want to try again?") != 0)
		return DITEM_FAILURE;
	}
	else
	    break;
    }

    /* Since the fixit code expects everything to be in /mnt2, and the CDROM mounting stuff /dist, do
     * a little kludge dance here..
     */
    if (symlink("/dist", "/mnt2")) {
	msgConfirm("Unable to symlink /mnt2 to the CDROM mount point.  Please report this\n"
		   "unexpected failure to freebsd-bugs@FreeBSD.org.");
	return DITEM_FAILURE;
    }

    /*
     * If /tmp points to /mnt2/tmp from a previous fixit floppy session, it's
     * not very good for us if we point it to the CDROM now.  Rather make it
     * a directory in the root MFS then.  Experienced admins will still be
     * able to mount their disk's /tmp over this if they need.
     */
    if (lstat("/tmp", &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFLNK)
	(void)unlink("/tmp");
    Mkdir("/tmp");

    /*
     * Since setuid binaries ignore LD_LIBRARY_PATH, we indeed need the
     * ld.so.hints file.  Fortunately, it's fairly small (~ 3 KB).
     */
    if (!file_readable("/var/run/ld.so.hints")) {
	Mkdir("/var/run");
	if (vsystem("/mnt2/sbin/ldconfig -s /mnt2/usr/lib")) {
	    msgConfirm("Warning: ldconfig could not create the ld.so hints file.\n"
		       "Dynamic executables from the CDROM likely won't work.");
	}
    }

    /* Yet more iggly hardcoded pathnames. */
    Mkdir("/usr/libexec");
    if (!file_readable("/usr/libexec/ld.so") && file_readable("/mnt2/usr/libexec/ld.so")) {
	if (symlink("/mnt2/usr/libexec/ld.so", "/usr/libexec/ld.so"))
	    msgDebug("Couldn't link to ld.so - not necessarily a problem for ELF\n");
    }
    if (!file_readable("/usr/libexec/ld-elf.so.1")) {
	if (symlink("/mnt2/usr/libexec/ld-elf.so.1", "/usr/libexec/ld-elf.so.1")) {
	    msgConfirm("Warning: could not create the symlink for ld-elf.so.1\n"
		       "Dynamic executables from the CDROM likely won't work.");
	}
    }
    /* optional nicety */
    if (!file_readable("/usr/bin/vi"))
	symlink("/mnt2/usr/bin/vi", "/usr/bin/vi");
    fixit_common();
    mediaClose();
    msgConfirm("Please remove the FreeBSD fixit CDROM now.");
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
	if (!mediaDevice->init(mediaDevice)) {
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
	setenv("MAKEDEVPATH", "/sbin:/bin:/stand:"
	       "/mnt2/sbin:/mnt2/bin:/mnt2/stand", 1);
	if (strcmp(variable_get(VAR_FIXIT_TTY), "serial") == 0) {
	    printf("Waiting for fixit shell to exit.\n"
		"When you are done, type ``exit'' to exit\n"
		"the fixit shell and be returned here.\n\n");
	    fflush(stdout);
	}

	/* use the .profile from the fixit medium */
	setenv("HOME", "/mnt2", 1);
	chdir("/mnt2");
	execlp("sh", "-sh", 0);
	msgDebug("fixit shell: Failed to execute shell!\n");
	_exit(1);;
    }
    else {
	if (strcmp(variable_get(VAR_FIXIT_TTY), "standard") == 0) {
	    dialog_clear_norefresh();
	    msgNotify("Waiting for fixit shell to exit.  Go to VTY4 now by\n"
		"typing ALT-F4.  When you are done, type ``exit'' to exit\n"
		"the fixit shell and be returned here\n.");
	}
	(void)waitpid(child, &waitstatus, 0);
	if (strcmp(variable_get(VAR_FIXIT_TTY), "serial") == 0)
	    systemResumeDialog();
    }
    dialog_clear();
}


int
installExpress(dialogMenuItem *self)
{
    int i;

    dialog_clear_norefresh();
    variable_set2(SYSTEM_STATE, "express", 0);
#ifndef __alpha__
    if (DITEM_STATUS((i = diskPartitionEditor(self))) == DITEM_FAILURE)
	return i;
#endif
    
    if (DITEM_STATUS((i = diskLabelEditor(self))) == DITEM_FAILURE)
	return i;

    if (DITEM_STATUS((i = installCommit(self))) == DITEM_SUCCESS) {
	i |= DITEM_LEAVE_MENU;
	/* Set default security level */
	configSecurityModerate(NULL);

	/* Give user the option of one last configuration spree */
	installConfigure();
    }
    return i;
}

/* Standard mode installation */
int
installStandard(dialogMenuItem *self)
{
    int i, tries = 0;
    Device **devs;

    variable_set2(SYSTEM_STATE, "standard", 0);
    dialog_clear_norefresh();
#ifndef __alpha__
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
#endif

#ifdef __alpha__
    msgConfirm("Now you need to create BSD partitions on the disk which you are\n"
	       "installing to.  If you have a reasonable amount of disk space (200MB or more)\n"
	       "and don't have any special requirements, simply use the (A)uto command to\n"
	       "allocate space automatically.  If you have more specific needs or just don't\n"
	       "care for the layout chosen by (A)uto, press F1 for more information on\n"
	       "manual layout.");
#else
    msgConfirm("Now you need to create BSD partitions inside of the fdisk partition(s)\n"
	       "just created.  If you have a reasonable amount of disk space (200MB or more)\n"
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
		   "prompt and go back into the installation menus to try and retry\n"
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
		   "may do so by typing: /stand/sysinstall.");
    }
    if (mediaDevice->type != DEVICE_TYPE_FTP && mediaDevice->type != DEVICE_TYPE_NFS) {
	if (!msgYesNo("Would you like to configure any Ethernet or SLIP/PPP network devices?")) {
	    Device *tmp = tcpDeviceSelect();

	    if (tmp && !((DevInfo *)tmp->private)->use_dhcp && !msgYesNo("Would you like to bring the %s interface up right now?", tmp->name))
		if (!tmp->init(tmp))
		    msgConfirm("Initialization of %s device failed.", tmp->name);
	}
	dialog_clear_norefresh();
    }

    if (msgYesNo("Will this machine be a leaf node (e.g. will not forward packets\n"
		 "between interfaces)?"))
	variable_set2("gateway_enable", "YES", 1);

    dialog_clear_norefresh();
    if (msgYesNo("Do you want to grant only normal users FTP access to this\n"
	         "host (e.g. no anonymous FTP connections)?"))
	configAnonFTP(self);

    dialog_clear_norefresh();
    if (!msgYesNo("Do you want to configure this machine as an NFS server?"))
	configNFSServer(self);

    dialog_clear_norefresh();
    if (!msgYesNo("Do you want to configure this machine as an NFS client?"))
	variable_set2("nfs_client_enable", "YES", 1);

    if (!msgYesNo("Do you want to select a default security profile for\n"
	         "this host (select No for \"medium\" security)?"))
	configSecurityProfile(self);
    else
	configSecurityModerate(self);

    dialog_clear_norefresh();
    if (!msgYesNo("Would you like to customize your system console settings?"))
	dmenuOpenSimple(&MenuSyscons, FALSE);

    dialog_clear_norefresh();
    if (!msgYesNo("Would you like to set this machine's time zone now?"))
	systemExecute("tzsetup");

#ifdef __i386__
    dialog_clear_norefresh();
    if (!msgYesNo("Would you like to enable Linux binary compatibility?"))
	(void)configLinux(self);
#endif

    dialog_clear_norefresh();
    if (!msgYesNo("Does this system have a non-USB mouse attached to it?"))
	dmenuOpenSimple(&MenuMouse, FALSE);

    /* Now would be a good time to checkpoint the configuration data */
    configRC_conf();
    sync();

    if (directory_exists("/usr/X11R6")) {
	dialog_clear_norefresh();
	if (!msgYesNo("Would you like to configure your X server at this time?"))
	    (void)configXSetup(self);
    }

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
	/* Set default security level */
	configSecurityModerate(NULL);

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
 * then use ftp to load just the CRYPTO dist.
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
    if (!mediaDevice->init(mediaDevice)) {
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
    if (!msgYesNo("Visit the general configuration menu for a chance to set\n"
		  "any last options?"))
	dmenuOpenSimple(&MenuConfigure, FALSE);
    configRC_conf();
    sync();
}

int
installFixupBin(dialogMenuItem *self)
{
    Device **devs;
    char *cp;
    int i;
    FILE *fp;
    int kstat = 1;

    /* All of this is done only as init, just to be safe */
    if (RunningAsInit) {
	/* Fix up kernel first */
	if (!file_readable("/kernel")) {
	    char *generic_kernel = "/kernel.GENERIC";
	    if (file_readable(generic_kernel)) {
		if (vsystem("cp -p %s /kernel", generic_kernel)) {
		    msgConfirm("Unable to copy /kernel into place!");
		    return DITEM_FAILURE;
		}
#ifndef __alpha__
                /* Snapshot any boot -c changes back to the new kernel */
		cp = variable_get(VAR_KGET);
		if (cp && (*cp == 'Y' || *cp == 'y')) {
		    if ((kstat = kget("/boot/kernel.conf")) != NULL) {
			msgConfirm("Kernel copied OK, but unable to save boot -c changes\n"
				   "to it.  See the debug screen (ALT-F2) for details.");
		    }
		}
		if ((fp = fopen("/boot/loader.conf", "a")) != NULL) {
		    fprintf(fp, "# -- sysinstall generated deltas -- #\n");
		    if (!kstat)
			fprintf(fp, "userconfig_script_load=\"YES\"\n");
		    if (!OnVTY)
			fprintf(fp, "console=\"comconsole\"\n");
		    fclose(fp);
		}
#endif
	    }
	    else {
		msgConfirm("Can't find a kernel image to link to on the root file system!\n"
			   "You're going to have a hard time getting this system to\n"
			   "boot from the hard disk, I'm afraid!");
		return DITEM_FAILURE;
	    }
	}
	
	/* BOGON #1: Resurrect /dev after bin distribution screws it up */
	dialog_clear_norefresh();
	msgNotify("Remaking all devices.. Please wait!");
	if (vsystem("cd /dev; sh MAKEDEV all")) {
	    msgConfirm("MAKEDEV returned non-zero status");
	    return DITEM_FAILURE | DITEM_RESTORE;
	}

	dialog_clear_norefresh();
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
		    dialog_clear_norefresh();
		    msgNotify("Making slice entries for %s", c1->name);
		    if (vsystem("cd /dev; sh MAKEDEV %sh", c1->name)) {
			msgConfirm("Unable to make slice entries for %s!", c1->name);
			return DITEM_FAILURE | DITEM_RESTORE;
		    }
		}
	    }
	}
	
	/* BOGON #2: We leave /etc in a bad state */
	chmod("/etc", 0755);
	
	/* BOGON #3: No /var/db/mountdtab complains */
	Mkdir("/var/db");
	creat("/var/db/mountdtab", 0644);
	
	/* BOGON #4: /compat created by default in root fs */
	Mkdir("/usr/compat");
	vsystem("ln -s /usr/compat /compat");
	
	/* BOGON #5: aliases database not build for bin */
	vsystem("newaliases");

	/* Now run all the mtree stuff to fix things up */
        vsystem("mtree -deU -f /etc/mtree/BSD.root.dist -p /");
        vsystem("mtree -deU -f /etc/mtree/BSD.var.dist -p /var");
        vsystem("mtree -deU -f /etc/mtree/BSD.usr.dist -p /usr");

	/* Do all the last ugly work-arounds here */
    }
    return DITEM_SUCCESS | DITEM_RESTORE;
}

/* Fix side-effects from the the XFree86 installation */
int
installFixupXFree(dialogMenuItem *self)
{
    /* BOGON #1:  XFree86 requires various specialized fixups */
    if (directory_exists("/usr/X11R6")) {
	dialog_clear_norefresh();
	msgNotify("Fixing permissions in XFree86 tree..");
	vsystem("chmod -R a+r /usr/X11R6");
	vsystem("find /usr/X11R6 -type d | xargs chmod a+x");

	/* Also do bogus minimal package registration so ports don't whine */
	if (file_readable("/usr/X11R6/lib/X11/pkgreg.tar.gz")) {
	    dialog_clear_norefresh();
	    msgNotify("Installing package metainfo..");
	    vsystem("tar xpzf /usr/X11R6/lib/X11/pkgreg.tar.gz -C / && rm /usr/X11R6/lib/X11/pkgreg.tar.gz");
	}
    }
    return DITEM_SUCCESS | DITEM_RESTORE;
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
    char dname[80];
    extern int MakeDevChunk(Chunk *c, char *n);
    Boolean upgrade = FALSE;

    /* If we've already done this, bail out */
    if (!variable_cmp(DISK_LABELLED, "written"))
	return DITEM_SUCCESS;

    upgrade = !variable_cmp(SYSTEM_STATE, "upgrade");
    if (!checkLabels(TRUE, &rootdev, &swapdev, &usrdev, &vardev))
	return DITEM_FAILURE;

    if (rootdev)
	root = (PartInfo *)rootdev->private_data;
    else
	root = NULL;

    command_clear();
    if (swapdev && RunningAsInit) {
	/* As the very first thing, try to get ourselves some swap space */
	sprintf(dname, "/dev/%s", swapdev->name);
	if (!Fake && (!MakeDevChunk(swapdev, "/dev") || !file_readable(dname))) {
	    msgConfirm("Unable to make device node for %s in /dev!\n"
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

    if (rootdev && RunningAsInit) {
	/* Next, create and/or mount the root device */
	sprintf(dname, "/dev/%s", rootdev->name);
	if (!Fake && (!MakeDevChunk(rootdev, "/dev") || !file_readable(dname))) {
	    msgConfirm("Unable to make device node for %s in /dev!\n"
		       "The creation of filesystems will be aborted.", dname);
	    return DITEM_FAILURE | DITEM_RESTORE;
	}
	if (strcmp(root->mountpoint, "/"))
	    msgConfirm("Warning: %s is marked as a root partition but is mounted on %s", rootdev->name, root->mountpoint);

	if (root->newfs && (!upgrade || !msgYesNo("You are upgrading - are you SURE you want to newfs the root partition?"))) {
	    int i;

	    dialog_clear_norefresh();
	    msgNotify("Making a new root filesystem on %s", dname);
	    i = vsystem("%s %s", root->newfs_cmd, dname);
	    if (i) {
		msgConfirm("Unable to make new root filesystem on %s!\n"
			   "Command returned status %d", dname, i);
		return DITEM_FAILURE | DITEM_RESTORE;
	    }
	}
	else {
	    if (!upgrade) {
		msgConfirm("Warning:  Using existing root partition.  It will be assumed\n"
			   "that you have the appropriate device entries already in /dev.");
	    }
	    dialog_clear_norefresh();
	    msgNotify("Checking integrity of existing %s filesystem.", dname);
	    i = vsystem("fsck -y %s", dname);
	    if (i)
		msgConfirm("Warning: fsck returned status of %d for %s.\n"
			   "This partition may be unsafe to use.", i, dname);
	}

	/* Switch to block device */
	sprintf(dname, "/dev/%s", rootdev->name);
	if (Mount("/mnt", dname)) {
	    msgConfirm("Unable to mount the root file system on %s!  Giving up.", dname);
	    return DITEM_FAILURE | DITEM_RESTORE;
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
	if (RunningAsInit && root && (root->newfs || upgrade)) {
	    Mkdir("/mnt/dev");
	    if (!Fake)
		MakeDevDisk(disk, "/mnt/dev");
	}
	else if (!RunningAsInit && !Fake)
	    MakeDevDisk(disk, "/dev");

	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private_data) {
			PartInfo *tmp = (PartInfo *)c2->private_data;

			/* Already did root */
			if (c2 == rootdev)
			    continue;

			if (tmp->newfs && (!upgrade || !msgYesNo("You are upgrading - are you SURE you want to newfs /dev/%s?", c2->name)))
			    command_shell_add(tmp->mountpoint, "%s %s/dev/%s", tmp->newfs_cmd, RunningAsInit ? "/mnt" : "", c2->name);
			else
			    command_shell_add(tmp->mountpoint, "fsck -y %s/dev/%s", RunningAsInit ? "/mnt" : "", c2->name);
			command_func_add(tmp->mountpoint, Mount, c2->name);
		    }
		    else if (c2->type == part && c2->subtype == FS_SWAP) {
			char fname[80];
			int i;

			if (c2 == swapdev)
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
	    else if (c1->type == fat && c1->private_data && (root->newfs || upgrade)) {
		char name[FILENAME_MAX];

		sprintf(name, "%s/%s", RunningAsInit ? "/mnt" : "", ((PartInfo *)c1->private_data)->mountpoint);
		Mkdir(name);
	    }
	}
    }

    if (RunningAsInit) {
	dialog_clear_norefresh();
	msgNotify("Copying initial device files..");
	/* Copy the boot floppy's dev files */
	if ((root->newfs || upgrade) && vsystem("find -x /dev | cpio %s -pdum /mnt", cpioVerbosity())) {
	    msgConfirm("Couldn't clone the /dev files!");
	    return DITEM_FAILURE | DITEM_RESTORE;
	}
    }

    command_sort();
    command_execute();
    dialog_clear_norefresh();
    return DITEM_SUCCESS | DITEM_RESTORE;
}

static char *
getRelname(void)
{
    static char buf[64];
    int sz = (sizeof buf) - 1;

    if (sysctlbyname("kern.osrelease", buf, &sz, NULL, 0) != -1) {
	buf[sz] = '\0';
	return buf;
    }
    else
	return "<unknown>";
}

/* Initialize various user-settable values to their defaults */
int
installVarDefaults(dialogMenuItem *self)
{
    char *cp;

    /* Set default startup options */
    variable_set2(VAR_RELNAME,			getRelname(), 0);
    variable_set2(VAR_CPIO_VERBOSITY,		"high", 0);
    variable_set2(VAR_KGET,			"YES", 0);
    variable_set2(VAR_TAPE_BLOCKSIZE,		DEFAULT_TAPE_BLOCKSIZE, 0);
    variable_set2(VAR_INSTALL_ROOT,		"/", 0);
    variable_set2(VAR_INSTALL_CFG,		"install.cfg", 0);
    variable_set2(VAR_TRY_DHCP,			"NO", 0);	/* For now */
    variable_set2(VAR_TRY_RTSOL,		"NO", 0);	/* For now */
    cp = getenv("EDITOR");
    if (!cp)
	cp = "/usr/bin/ee";
    variable_set2(VAR_EDITOR,			cp, 0);
    variable_set2(VAR_FTP_USER,			"ftp", 0);
    variable_set2(VAR_BROWSER_PACKAGE,		"lynx", 0);
    variable_set2(VAR_BROWSER_BINARY,		"/usr/local/bin/lynx", 0);
    variable_set2(VAR_FTP_STATE,		"passive", 0);
    variable_set2(VAR_NFS_SECURE,		"NO", -1);
    if (OnVTY)
	    variable_set2(VAR_FIXIT_TTY,		"standard", 0);
    else
	    variable_set2(VAR_FIXIT_TTY,		"serial", 0);
    variable_set2(VAR_PKG_TMPDIR,		"/usr/tmp", 0);
    variable_set2(VAR_MEDIA_TIMEOUT,		itoa(MEDIA_TIMEOUT), 0);
    if (getpid() != 1)
	variable_set2(SYSTEM_STATE,		"update", 0);
    else
	variable_set2(SYSTEM_STATE,		"init", 0);
    variable_set2(VAR_NEWFS_ARGS,		"-b 8192 -f 1024", 0);
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
