/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media.c,v 1.13 1995/05/21 19:28:05 jkh Exp $
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

#include <stdio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sysinstall.h"

static int
genericHook(char *str, DeviceType type)
{
    Device **devs;

    /* Clip garbage off the ends */
    string_prune(str);
    str = string_skipwhite(str);
    if (!*str)
	return 0;
    devs = deviceFind(str, type);
    if (devs)
	mediaDevice = devs[0];
    return devs ? 1 : 0;
}

static int
cdromHook(char *str)
{
    return genericHook(str, DEVICE_TYPE_CDROM);
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a CD.
 */
int
mediaSetCDROM(char *str)
{
    Device **devs;
    int cnt;

    if (OnCDROM == TRUE) {
	static Device bootCD;

	/* This may need to be extended a little, but the basic idea is sound */
	strcpy(bootCD.name, "bootCD");
	bootCD.type = DEVICE_TYPE_CDROM;
	bootCD.init = NULL;
	bootCD.get = mediaGetCDROM;
	bootCD.close = NULL;
	mediaDevice = &bootCD;
	return 1;
    }
    else {
	devs = deviceFind(NULL, DEVICE_TYPE_CDROM);
	cnt = deviceCount(devs);
	if (!cnt) {
	    msgConfirm("No CDROM devices found!  Please check that your system's\nconfiguration is correct and that the CDROM drive is of a supported\ntype.  For more information, consult the hardware guide\nin the Doc menu.");
	    return 0;
        }
	else if (cnt > 1) {
	    DMenu *menu;

	    menu = deviceCreateMenu(&MenuMediaCDROM, DEVICE_TYPE_CDROM, cdromHook);
	    if (!menu)
		msgFatal("Unable to create CDROM menu!  Something is seriously wrong.");
	    dmenuOpenSimple(menu);
	    free(menu);
	}
	else
	    mediaDevice = devs[0];
    }
    return mediaDevice ? 1 : 0;
}

static int
floppyHook(char *str)
{
    return genericHook(str, DEVICE_TYPE_FLOPPY);
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a floppy
 */
int
mediaSetFloppy(char *str)
{
    Device **devs;
    int cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_FLOPPY);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No floppy devices found!  Please check that your system's\nconfiguration is correct.  For more information, consult the hardware guide\nin the Doc menu.");
	return 0;
    }
    else if (cnt > 1) {
	DMenu *menu;

	menu = deviceCreateMenu(&MenuMediaFloppy, DEVICE_TYPE_FLOPPY, floppyHook);
	if (!menu)
	    msgFatal("Unable to create Floppy menu!  Something is seriously wrong.");
	dmenuOpenSimple(menu);
	free(menu);
    }
    else
	mediaDevice = devs[0];
    return mediaDevice ? 1 : 0;
}

static int
DOSHook(char *str)
{
    return genericHook(str, DEVICE_TYPE_DOS);
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a DOS partition.
 */
int
mediaSetDOS(char *str)
{
    Device **devs;
    Disk *d;
    Chunk *c1;
    int i, cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_DOS);
    cnt = deviceCount(devs);
    if (cnt > 1) {
	DMenu *menu;

	menu = deviceCreateMenu(&MenuMediaDOS, DEVICE_TYPE_DOS, DOSHook);
	if (!menu)
	    msgFatal("Unable to create DOS menu!  Something is seriously wrong.");
	dmenuOpenSimple(menu);
	free(menu);
    }
    else if (cnt) {
	mediaDevice = devs[0];
	return 1;
    }
    else {
	devs = deviceFind(NULL, DEVICE_TYPE_DISK);
	if (!devs) {
	    msgConfirm("No disk devices found!");
	    return 0;
	}
	/* Now go chewing through looking for a DOS FAT partition */
	for (i = 0; devs[i]; i++) {
	    d = (Disk *)devs[i]->private;
	    /* Now try to find a DOS partition */
	    for (c1 = d->chunks->part; c1; c1 = c1->next) {
		if (c1->type == fat) {
		    /* Got one! */
		    mediaDevice = deviceRegister(c1->name, c1->name, c1->name, DEVICE_TYPE_DOS, TRUE,
						 mediaInitDOS, mediaGetDOS, mediaCloseDOS, NULL);
		    msgDebug("Found a DOS partition %s on drive %s\n", c1->name, d->name);
		    break;
		}
	    }
	}
    }
    if (!mediaDevice)
	msgConfirm("No DOS primary partitions found!  This installation method is unavailable");
    return mediaDevice ? 1 : 0;
}

static int
tapeHook(char *str)
{
    return genericHook(str, DEVICE_TYPE_TAPE);
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a tape drive.
 */
int
mediaSetTape(char *str)
{
    Device **devs;
    int cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_TAPE);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No tape drive devices found!  Please check that your system's\nconfiguration is correct.  For more information, consult the hardware guide\nin the Doc menu.");
	return 0;
    }
    else if (cnt > 1) {
	DMenu *menu;

	menu = deviceCreateMenu(&MenuMediaTape, DEVICE_TYPE_TAPE, tapeHook);
	if (!menu)
	    msgFatal("Unable to create tape drive menu!  Something is seriously wrong.");
	dmenuOpenSimple(menu);
	free(menu);
    }
    else
	mediaDevice = devs[0];
    return mediaDevice ? 1 : 0;
}

/*
 * Return 0 if we successfully found and set the installation type to
 * be an ftp server
 */
int
mediaSetFTP(char *str)
{
    static Device ftpDevice;
    Device *devp;
    char *cp;

    devp = tcpDeviceSelect();
    if (!devp)
	return 0;
    dmenuOpenSimple(&MenuMediaFTP);
    cp = getenv("ftp");
    if (!cp)
	return 0;
    strcpy(ftpDevice.name, cp);
    ftpDevice.type = DEVICE_TYPE_NETWORK;
    ftpDevice.init = mediaInitFTP;
    ftpDevice.get = mediaGetFTP;
    ftpDevice.close = mediaCloseFTP;
    ftpDevice.private = devp;
    mediaDevice = &ftpDevice;
    return 1;
}

/*
 * Return 0 if we successfully found and set the installation type to
 * be some sort of mounted filesystem (it's also mounted at this point)
 */
int
mediaSetFS(char *str)
{
    return 0;
}

int
mediaOpen(char *parent, char *me)
{
    char distname[FILENAME_MAX];
    int fd;

    if (parent)
	snprintf(distname, FILENAME_MAX, "%s%s", parent, me);
    else
	snprintf(distname, FILENAME_MAX, "%s/%s", me, me);
    msgNotify("Attempting to open %s distribution", distname);
    fd = (*mediaDevice->get)(distname);
    return fd;
}

void
mediaClose(void)
{
    if (mediaDevice->close)
	(*mediaDevice->close)(mediaDevice);
    mediaDevice = NULL;
}

Boolean
mediaExtractDist(char *dir, int fd)
{
    int i, j, zpid, cpid, pfd[2];

    if (!dir)
	dir = "/";
    msgNotify("Extracting into %s directory..", dir);
    Mkdir(dir, NULL);
    chdir(dir);
    pipe(pfd);
    zpid = fork();
    if (!zpid) {
	dup2(fd, 0); close(fd);
	dup2(pfd[1], 1); close(pfd[1]);
	if (DebugFD != -1)
	    dup2(DebugFD, 2);
	else {
	    close(2);
	    open("/dev/null", O_WRONLY);
	}
	close(pfd[0]);
	i = execl("/stand/gunzip", "/stand/gunzip", 0);
	msgDebug("/stand/gunzip command returns %d status\n", i);
	exit(i);
    }
    cpid = fork();
    if (!cpid) {
	dup2(pfd[0], 0); close(pfd[0]);
	close(fd);
	close(pfd[1]);
	if (DebugFD != -1) {
	    dup2(DebugFD, 1);
	    dup2(DebugFD, 2);
	}
	else {
	    close(1); open("/dev/null", O_WRONLY);
	    dup2(1, 2);
	}
	i = execl("/stand/cpio", "/stand/cpio", "-iduvm", "-H", "tar", 0);
	msgDebug("/stand/cpio command returns %d status\n", i);
	exit(i);
    }
    close(pfd[0]);
    close(pfd[1]);
    close(fd);

    i = waitpid(zpid, &j, 0);
    if (i < 0 || _WSTATUS(j)) {
	dialog_clear();
	msgConfirm("gunzip returned error status of %d!", _WSTATUS(j));
	return FALSE;
    }
    i = waitpid(cpid, &j, 0);
    if (i < 0 || _WSTATUS(j)) {
	dialog_clear();
	msgConfirm("cpio returned error status of %d!", _WSTATUS(j));
	return FALSE;
    }
    return TRUE;
}

Boolean
mediaGetType(void)
{
    dmenuOpenSimple(&MenuMedia);
    return TRUE;
}

/* Return TRUE if all the media variables are set up correctly */
Boolean
mediaVerify(void)
{
    if (!mediaDevice) {
	msgConfirm("Media type not set!  Please select a media type\nfrom the Installation menu before proceeding.");
	return FALSE;
    }
    return TRUE;
}
