/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.45 1995/05/21 01:56:01 phk Exp $
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
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

Boolean SystemWasInstalled;

static void	make_filesystems(void);
static void	copy_self(void);
static void	cpio_extract(void);
static void	install_configuration_files(void);
static void	do_final_setup(void);

static void
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
	return;

    if (!getenv(DISK_PARTITIONED)) {
	msgConfirm("You need to partition your disk before you can proceed with\nthe installation.");
	return;
    }
    if (!getenv(DISK_LABELLED)) {
	msgConfirm("You need to assign disk labels before you can proceed with\nthe installation.");
	return;
    }

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
	return;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    for (i = 0; devs[i]; i++) {
	Disk *d = (Disk *)devs[i]->private;
	Chunk *c1;

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
		    msgConfirm("Bad144 init on %s returned status of %d!", 
			c1->name, ret);
		ret = vsystem("bad144 -v -s /dev/r%s", c1->name);
		if (ret)
		    msgConfirm("Bad144 scan on %s returned status of %d!", 
			c1->name, ret);
	    }
	}
    }
    make_filesystems();
    copy_self();
    dialog_clear();
    chroot("/mnt");
    chdir("/");
    cpio_extract();
    alreadyDone = TRUE;
}

static void
installFinal(void)
{
    static Boolean alreadyDone = FALSE;

    if (alreadyDone)
	return;
    install_configuration_files();
    do_final_setup();
    alreadyDone = TRUE;
}

/*
 * What happens when we select "GO".  This is broken into a 3 stage installation so that
 * the user can do a full installation but come back here again to load more distributions,
 * perhaps from a different media type.  This would allow, for example, the user to load the
 * majority of the system from CDROM and then use ftp to load just the DES dist.
 */
int
installCommit(char *str)
{
    if (!Dists) {
	msgConfirm("You haven't told me what distributions to load yet!\nPlease select a distribution from the Distributions menu.");
	return 0;
    }
    if (!mediaVerify())
	return 0;

    installInitial();
    distExtractAll();
    installFinal();
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

    command_clear();
    devs = deviceFind(NULL, DEVICE_TYPE_DISK);

    /* First look for the root device and mount it */
    for (i = 0; devs[i]; i++) {
	disk = (Disk *)devs[i]->private;
	msgDebug("Scanning disk %s for root filesystem\n", disk->name);
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && c2->subtype != FS_SWAP &&
			c2->private && c2->flags & CHUNK_IS_ROOT) {
			char dname[40];
			PartInfo *p = (PartInfo *)c2->private;

			if (strcmp(p->mountpoint, "/")) {
			    msgConfirm("Warning: %s is marked as a root partition but is mounted on %s", c2->name, p->mountpoint);
			    continue;
			}
			if (p->newfs) {
			    int i;

			    sprintf(dname, "/dev/r%sa", disk->name);
			    msgNotify("Making a new root filesystem on %s", dname);
			    i = vsystem("%s %s", p->newfs_cmd,dname);
			    if (i) {
				msgConfirm("Unable to make new root filesystem!  Command returned status %d", i);
				return;
			    }
			}
			else
			    msgConfirm("Warning:  You have selected a Read-Only root device\nand may be unable to find the appropriate device entries on it\nif it is from an older pre-slice version of FreeBSD.");
			sprintf(dname, "/dev/%sa", disk->name);
			if (Mount("/mnt", dname)) {
			    msgConfirm("Unable to mount the root file system!  Giving up.");
			    return;
			}
			else {
			    extern int makedevs(void);

			    msgNotify("Making device files");
			    if (Mkdir("/mnt/dev", NULL)
				|| chdir("/mnt/dev")
				|| makedevs())
				msgConfirm("Failed to make some of the devices in /mnt!");
			    if (Mkdir("/mnt/stand", NULL))
				msgConfirm("Unable to make /mnt/stand directory!");
			    chdir("/");
			    break;
			}
		    }
		}
	    }
	}
    }

    /* Now buzz through the rest of the partitions and mount them too */
    for (i = 0; devs[i]; i++) {
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
			    command_shell_add(tmp->mountpoint,
					      "%s /mnt/dev/r%s", tmp->newfs_cmd, c2->name);
			command_func_add(tmp->mountpoint, Mount, c2->name);
		    }
		}
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

    msgNotify("Copying the boot floppy to /stand on root filesystem");
    i = vsystem("find -x /stand | cpio -pdmv /mnt");
    if (i)
	msgConfirm("Copy returned error status of %d!", i);
}

static void
cpio_extract(void)
{
    int i, j, zpid, cpid, pfd[2];

 tryagain:
    while (CpioFD == -1) {
	msgConfirm("Please Insert CPIO floppy in floppy drive 0");
	CpioFD = open("/dev/rfd0", O_RDWR);
	if (CpioFD >= 0)
	    break;
	msgDebug("Error on open of cpio floppy: %s (%d)\n", strerror(errno), errno);
    }
    j = fork();
    if (!j) {
	chdir("/");
	msgNotify("Extracting contents of CPIO floppy...");
	pipe(pfd);
	zpid = fork();
	if (!zpid) {
	    dup2(CpioFD, 0); close(CpioFD);
	    dup2(pfd[1], 1); close(pfd[1]);
	    close(pfd[0]);
	    i = execl("/stand/gunzip", "/stand/gunzip", 0);
	    msgDebug("/stand/gunzip command returns %d status\n", i);
	    exit(i);
	}
	cpid = fork();
	if (!cpid) {
	    dup2(pfd[0], 0); close(pfd[0]);
	    close(CpioFD);
	    close(pfd[1]);
	    if (DebugFD != -1) {
		dup2(DebugFD, 1);
		dup2(DebugFD, 2);
	    }
	    else {
		close(1); open("/dev/null", O_WRONLY);
		dup2(1, 2);
	    }
	    i = execl("/stand/cpio", "/stand/cpio", "-iduvm", 0);
	    msgDebug("/stand/cpio command returns %d status\n", i);
	    exit(i);
	}
	close(pfd[0]);
	close(pfd[1]);
	close(CpioFD);

	i = waitpid(zpid, &j, 0);
	if (i < 0 || _WSTATUS(j)) {
	    dialog_clear();
	    msgConfirm("gunzip returned error status of %d!", _WSTATUS(j));
	    exit(1);
	}
	i = waitpid(cpid, &j, 0);
	if (i < 0 || _WSTATUS(j)) {
	    dialog_clear();
	    msgConfirm("cpio returned error status of %d!", _WSTATUS(j));
	    exit(2);
	}
	exit(0);
    }
    else
	i = wait(&j);
    if (i < 0 || _WSTATUS(j) || access("/OK", R_OK) == -1) {
	dialog_clear();
	msgConfirm("CPIO floppy did not extract properly!  Please verify\nthat your media is correct and try again.");
	close(CpioFD);
	CpioFD = -1;
	goto tryagain;
    }
    unlink("/OK");
}

static void
install_configuration_files(void)
{
}

static void
do_final_setup(void)
{
}
