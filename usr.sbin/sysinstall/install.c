/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.63 1995/05/28 09:31:32 jkh Exp $
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
#include <unistd.h>

Boolean SystemWasInstalled;

static void	make_filesystems(void);
static void	copy_self(void);
static void	root_extract(void);

static Chunk *rootdev;

static Boolean
checkLabels(void)
{
    Device **devs;
    Disk *disk;
    Chunk *c1, *c2, *swapdev = NULL;
    int i;

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
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private && c2->flags & CHUNK_IS_ROOT) {
			rootdev = c2;
			break;
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
			break;
		    }
		}
	    }
	}
    }

    if (!rootdev) {
	msgConfirm("No root device found - you must label a partition as /\n in the label editor.");
	return FALSE;
    }
    if (!swapdev) {
	msgConfirm("No swap devices found - you must create at least one\nswap partition.");
	return FALSE;
    }
    return TRUE;
}

static Boolean
installInitial(void)
{
    extern u_char boot1[], boot2[];
    extern u_char mbr[], bteasy17[];
    u_char *mbrContents;
    Device **devs;
    int i;
    static Boolean alreadyDone = FALSE;
    char *cp;

    if (alreadyDone)
	return TRUE;

    if (!getenv(DISK_PARTITIONED)) {
	msgConfirm("You need to partition your disk before you can proceed with\nthe installation.");
	return FALSE;
    }
    if (!getenv(DISK_LABELLED)) {
	msgConfirm("You need to assign disk labels before you can proceed with\nthe installation.");
	return FALSE;
    }
    if (!checkLabels())
	return FALSE;

    /* Figure out what kind of MBR the user wants */
    dmenuOpenSimple(&MenuMBRType);
    mbrContents = NULL;
    cp = getenv("bootManager");
    if (cp) {
	if (!strcmp(cp, "bteasy"))
	    mbrContents = bteasy17;
	else if (!strcmp(cp, "mbr"))
	    mbrContents = mbr;
    }

    /* If we refuse to proceed, bail. */
    if (msgYesNo("Last Chance!  Are you SURE you want continue the installation?\n\nIf you're running this on an existing system, we STRONGLY\nencourage you to make proper backups before proceeding.\nWe take no responsibility for lost disk contents!"))
	return FALSE;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    for (i = 0; devs[i]; i++) {
	Chunk *c1;
	Disk *d = (Disk *)devs[i]->private;

	if (!devs[i]->enabled)
	    continue;

	if (mbrContents) {
	    Set_Boot_Mgr(d, mbrContents);
	    mbrContents = NULL;
	}
	Set_Boot_Blocks(d, boot1, boot2);
	msgNotify("Writing partition information to drive %s", d->name);
	Write_Disk(d);

	/* Now scan for bad blocks, if necessary */
	for (c1 = d->chunks->part; c1; c1 = c1->next) {
	    if (c1->flags & CHUNK_BAD144) {
		int ret;

		msgNotify("Running bad block scan on partition %s", c1->name);
		ret = vsystem("bad144 -v /dev/r%s 1234", c1->name);
		if (ret)
		    msgConfirm("Bad144 init on %s returned status of %d!", c1->name, ret);
		ret = vsystem("bad144 -v -s /dev/r%s", c1->name);
		if (ret)
		    msgConfirm("Bad144 scan on %s returned status of %d!", c1->name, ret);
	    }
	}
    }
    make_filesystems();
    copy_self();
    dialog_clear();
    chroot("/mnt");
    chdir("/");
    variable_set2(RUNNING_ON_ROOT, "yes");
    /* stick a helpful shell over on the 4th VTY */
    msgDebug("Sticking a potentially helpful shell over on the 4th screen\n");
    if (!fork()) {
	int i, fd;

	for (i = 0; i < 64; i++)
	    close(i);
	fd = open("/dev/ttyv3", O_RDWR);
	ioctl(0, TIOCSCTTY, &fd);
	dup2(0, 1);
	dup2(0, 2);
	execlp("sh", "-sh", 0);
	exit(1);
    }
    /* Copy the /etc files into their rightful place */
    vsystem("(cd /stand; find etc | cpio -o) | (cd /; cpio -idmv)");
    root_extract();
    alreadyDone = TRUE;
    return TRUE;
}

/*
 * What happens when we select "Install".  This is broken into a 3 stage installation so that
 * the user can do a full installation but come back here again to load more distributions,
 * perhaps from a different media type.  This would allow, for example, the user to load the
 * majority of the system from CDROM and then use ftp to load just the DES dist.
 */
int
installCommit(char *str)
{
    FILE *fp;
    static Boolean hostsModified = FALSE;

    if (!Dists) {
	msgConfirm("You haven't told me what distributions to load yet!\nPlease select a distribution from the Distributions menu.");
	return 0;
    }
    if (!mediaVerify())
	return 0;

    if (RunningAsInit) {
	if (!installInitial())
	    return 0;
	configFstab();
	configResolv();
    }
    distExtractAll();

    /* Tack ourselves at the end of /etc/hosts */
    if (RunningAsInit && getenv(VAR_IPADDR) && !hostsModified) {
	fp = fopen("/etc/hosts", "a");
	fprintf(fp, "%s\t\t%s\n", getenv(VAR_IPADDR), getenv(VAR_HOSTNAME));
	fclose(fp);
	hostsModified = TRUE;
    }
    msgConfirm("Installation completed successfully.\nHit return now to go back to the main menu.");
    SystemWasInstalled = TRUE;
    return 0;
}

/* Go newfs and/or mount all the filesystems we've been asked to */
static void
make_filesystems(void)
{
    int i;
    Disk *disk;
    Chunk *c1, *c2;
    Device **devs;
    char dname[40];
    PartInfo *p = (PartInfo *)rootdev->private;

    command_clear();
    devs = deviceFind(NULL, DEVICE_TYPE_DISK);

    /* First, create and mount the root device */
    if (strcmp(p->mountpoint, "/"))
	msgConfirm("Warning: %s is marked as a root partition but is mounted on %s", rootdev->name, p->mountpoint);

    if (p->newfs) {
	int i;

	sprintf(dname, "/dev/r%sa", rootdev->disk->name);
	msgNotify("Making a new root filesystem on %s", dname);
	i = vsystem("%s %s", p->newfs_cmd, dname);
	if (i) {
	    msgConfirm("Unable to make new root filesystem!  Command returned status %d", i);
	    return;
	}
    }
    else {
	msgConfirm("Warning:  You have selected a Read-Only root device\nand may be unable to find the appropriate device entries on it\nif it is from an older pre-slice version of FreeBSD.");
	sprintf(dname, "/dev/r%sa", rootdev->disk->name);
	msgNotify("Checking integrity of existing %s filesystem", dname);
	i = vsystem("fsck -y %s", dname);
	if (i)
	    msgConfirm("Warning: fsck returned status off %d - this partition may be\nunsafe to use.", i);
    }
    sprintf(dname, "/dev/%sa", rootdev->disk->name);
    if (Mount("/mnt", dname)) {
	msgConfirm("Unable to mount the root file system!  Giving up.");
	return;
    }
    else {
	extern int makedevs(void);

	msgNotify("Making device files");
	if (Mkdir("/mnt/dev", NULL) || chdir("/mnt/dev") || makedevs())
	    msgConfirm("Failed to make some of the devices in /mnt!");
	if (Mkdir("/mnt/stand", NULL)) {
	    msgConfirm("Unable to make /mnt/stand directory!");
	    return;
	}
	chdir("/");
    }

    /* Now buzz through the rest of the partitions and mount them too */
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;

	disk = (Disk *)devs[i]->private;
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);

	/* Make the proper device mount points in /mnt/dev */
	MakeDevDisk(disk, "/mnt/dev");

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
	    else if (c1->type == fat) {
		PartInfo *tmp = (PartInfo *)c1->private;

		if (!tmp)
		    continue;
		command_func_add(tmp->mountpoint, Mount_DOS, c1->name);
	    }
	}
    }
    command_sort();
    command_execute();
}

/* Copy the boot floppy contents into /stand */
static void
copy_self(void)
{
    int i;

    msgWeHaveOutput("Copying the boot floppy to /stand on root filesystem");
    i = vsystem("find -x /stand | cpio -pdmV /mnt");
    if (i)
	msgConfirm("Copy returned error status of %d!", i);
    /* copy up the etc files */
    (void)vsystem("(cd /mnt/stand; find etc) | cpio -pdmv /mnt");
}

static void loop_on_root_floppy();

static void
root_extract(void)
{
    int fd;

    if (OnCDROM) {
	fd = open("/floppies/root.flp", O_RDONLY);
	(void)mediaExtractDist("root.flp", "/", fd);
	return;
    }
    if (mediaDevice) {
	switch(mediaDevice->type) {

	case DEVICE_TYPE_DOS:
	case DEVICE_TYPE_FTP:
	case DEVICE_TYPE_DISK:
	case DEVICE_TYPE_NETWORK:
	case DEVICE_TYPE_CDROM:
	    if (mediaDevice->init)
		if (!(*mediaDevice->init)(mediaDevice))
		    break;
	    fd = (*mediaDevice->get)("floppies/root.flp");
	    if (fd != -1) {
		msgNotify("Loading root floppy from %s", mediaDevice->name);
		(void)mediaExtractDist("root.flp", "/", fd);
		if (mediaDevice->close)
		    (*mediaDevice->close)(mediaDevice, fd);
		else
		    close(fd);
		if (mediaDevice->shutdown)
		    (*mediaDevice->shutdown)(mediaDevice);
	    } else {
		if (mediaDevice->shutdown)
		    (*mediaDevice->shutdown)(mediaDevice);
	        loop_on_root_floppy();
	    }
	    break;

	case DEVICE_TYPE_FLOPPY:
	default:
	    loop_on_root_floppy();
	    break;
	}
    }
    else
	loop_on_root_floppy();
}

static void
loop_on_root_floppy(void)
{
    int fd;

    fd = getRootFloppy();
    if (fd != -1)
	mediaExtractDist("root.flp", "/", fd);
}
