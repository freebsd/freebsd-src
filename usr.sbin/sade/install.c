/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.15 1995/05/11 06:47:44 jkh Exp $
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
#include <unistd.h>

Boolean SystemWasInstalled;
struct disk *Disks[100];	/* some ridiculously large number */

static int
installHook(char *str)
{
    int i;
    extern DMenu MenuInstall;

    i = 0;
    /* Clip garbage off the ends */
    string_prune(str);
    str = string_skipwhite(str);
    /* Try and open all the disks */
    while (str) {
	char *cp;

	cp = index(str, '\n');
	if (cp)
	   *cp++ = 0; 
	if (!*str) {
	    beep();
	    return 0;
	}
	Disks[i] = Open_Disk(str);
	if (!Disks[i])
	    msgFatal("Unable to open disk %s!", str);
	++i;
	str = cp;
    }
    Disks[i] = NULL;
    if (!i)
	return 0;

    while (1) {
	/* Now go set up all the MBR partition information */
	for (i = 0; Disks[i]; i++)
	    Disks[i] = device_slice_disk(Disks[i]);

	/* Whap partitions on all the FreeBSD slices created */
	partition_disks();

	/* Try and write it out */
	if (!write_disks()) {
	    int scroll, choice, curr, max;

	    make_filesystems();
	    scroll = choice = curr = max = 0;
	    dmenuOpen(&MenuInstall, &choice, &scroll, &curr, &max);
	    chdir("/mnt");
	    cpio_extract();
	    chroot("/mnt");
	    distExtractAll();
	    install_configuration_files();
	    do_final_setup();
	    SystemWasInstalled = TRUE;
	    break;
	}
	else {
	    dialog_clear();
	    if (msgYesNo("Would you like to go back to the Master Partition Editor?")) {
		for (i = 0; Disks[i]; i++)
		    Free_Disk(Disks[i]);
		break;
	    }
	}
    }
    return SystemWasInstalled;
}

int
installCustom(char *str)
{
    int scroll, choice, curr, max;
    extern DMenu MenuDiskDevices;
    DMenu *menu;
    Device *devs;

    variable_set2("install_type", "custom");
    menu = device_create_disk_menu(&MenuDiskDevices, &devs, installHook);
    if (!menu)
	return 0;
    choice = scroll = curr = max = 0;
    dmenuOpen(menu, &choice, &scroll, &curr, &max);
    free(menu);
    free(devs);
    return SystemWasInstalled;
}

int
installExpress(char *str)
{
    int scroll, choice, curr, max;
    extern DMenu MenuDiskDevices;
    DMenu *menu;
    Device *devs;

    variable_set2("install_type", "express");
    menu = device_create_disk_menu(&MenuDiskDevices, &devs, installHook);
    if (!menu)
	return 0;
    choice = scroll = curr = max = 0;
    dmenuOpen(menu, &choice, &scroll, &curr, &max);
    free(menu);
    free(devs);
    return SystemWasInstalled;
}

int
installMaint(char *str)
{
    msgConfirm("Sorry, maintainance mode is not implemented in this version.");
    return 0;
}

/* Go newfs and/or mount all the filesystems we've been asked to */
void
make_filesystems(void)
{
    int i;

    command_clear();
    for (i = 0; Disks[i]; i++) {
	struct chunk *c1;

	if (!Disks[i]->chunks)
	    msgFatal("No chunk list found for %s!", Disks[i]->name);
	c1 = Disks[i]->chunks->part;
	while (c1) {
	    if (c1->type == freebsd) {
		struct chunk *c2 = c1->part;

		while (c2) {
		    if (c2->type == part && c2->subtype != FS_SWAP &&
			c2->private) {
			PartInfo *tmp = (PartInfo *)c2->private;

			if (tmp->newfs)
			    command_add(tmp->mountpoint,
					"%s %s", tmp->newfs_cmd, c2->name);
			if (strcmp(tmp->mountpoint, "/")) {
			    command_add(tmp->mountpoint,
					"mkdir -p /mnt%s", tmp->mountpoint);
			    command_add(tmp->mountpoint,
					"mount /mnt/dev/%s /mnt%s", c2->name,
					tmp->mountpoint);
			}
			else
			    command_add(tmp->mountpoint,
					"mount /mnt/dev/%s /mnt", c2->name);

		    }
		    c2 = c2->next;
		}
	    }
	    c1 = c1->next;
	}
    }
    command_sort();
    command_execute();
}

void
cpio_extract(void)
{
    int i, j, zpid, cpid, pfd[2];
    extern int wait(int *status);

    while (CpioFD == -1) {
	msgConfirm("Please Insert CPIO floppy in floppy drive 0");
	CpioFD = open("/dev/rfd0", O_RDONLY);
    }
    msgNotify("Extracting contents of CPIO floppy...");
    pipe(pfd);
    zpid = fork();
    if (!zpid) {
	close(0); dup(CpioFD); close(CpioFD);
	close(1); dup(pfd[1]); close(pfd[1]);
	close(pfd[0]);
	i = execl("/stand/gunzip", "/stand/gunzip", 0);
	msgDebug("/stand/gunzip command returns %d status\n", i);
	exit(i);
    }
    cpid = fork();
    if (!cpid) {
	close(0); dup(pfd[0]); close(pfd[0]);
	close(CpioFD);
	close(pfd[1]);
	close(1); open("/dev/null", O_WRONLY);
	i = execl("/stand/cpio", "/stand/cpio", "-iduvm", 0);
	msgDebug("/stand/cpio command returns %d status\n", i);
	exit(i);
    }
    close(pfd[0]);
    close(pfd[1]);
    close(CpioFD);
    i = wait(&j);
    if (i < 0 || j)
	msgFatal("Pid %d, status %d, cpio=%d, gunzip=%d.\nerror:%s",
		 i, j, cpid, zpid, strerror(errno));
    i = wait(&j);
    if (i < 0 || j)
	msgFatal("Pid %d, status %d, cpio=%d, gunzip=%d.\nerror:%s",
		 i, j, cpid, zpid, strerror(errno));
}

void
install_configuration_files(void)
{
}

void
do_final_setup(void)
{
}

