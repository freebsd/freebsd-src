/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media.c,v 1.18 1995/05/26 20:30:58 jkh Exp $
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sysinstall.h"

pid_t getDistpid = 0;

/*
 * This is the generic distribution opening routine.  It returns
 * a file descriptor that refers to a stream of bytes coming from
 * _somewhere_ that can be extracted as a gzip'd tar file.
 */
int
genericGetDist(char *path, Attribs *dist_attrib, Boolean prompt)
{
    int 	fd;
    char 	buf[512];
    struct stat	sb;
    int		pfd[2], numchunks;
    const char *tmp;

    /* reap any previous child corpse - yuck! */
    if (getDistpid) {
	int i, j;

	i = waitpid(getDistpid, &j, 0);
	if (i < 0 || WEXITSTATUS(j))
	    msgNotify("Warning: Previous extraction returned status code %d.", WEXITSTATUS(j));
	getDistpid = 0;
    }

    /* How try to figure out how many pieces to expect */
    if (dist_attrib) {
	tmp = attr_match(dist_attrib, "pieces");
	numchunks = atoi(tmp);
    }
    else
	numchunks = 1;

    if (!path)
	return -1;
    if (stat(path, &sb) == 0) {
	fd = open(path, O_RDONLY, 0);
	return(fd);
    }

    snprintf(buf, 512, "%s.tgz", path);
    if (stat(buf, &sb) == 0) {
	fd = open(buf, O_RDONLY, 0);
	return(fd);
    }

    snprintf(buf, 512, "%s.aa", path);
    if (stat(buf, &sb) == 0 && numchunks == 1) {
	fd = open(buf, O_RDONLY, 0);
	if (fd != -1)
	    return fd;
	else if (!prompt) {
	}
    }

    if (numchunks < 2 && !prompt) {
	if (!getenv(NO_CONFIRMATION))
	    msgConfirm("Cannot find file(s) for distribution in ``%s''!", path);
	else
	    msgDebug("Cannot find file(s) for distribution in ``%s''!\n", path);
	return -1;
    }

    msgDebug("Attempting to concatenate %u chunks\n", numchunks);
    pipe(pfd);
    getDistpid = fork();
    if (!getDistpid) {
	caddr_t		memory;
	int		chunk;
	int		retval;

	dup2(pfd[1], 1); close(pfd[1]);
	close(pfd[0]);
	
	for (chunk = 0; chunk < numchunks; chunk++) {
	    int			fd;
	    unsigned long	len, val;
	    
	    retval = stat(buf, &sb);
	    if ((retval != 0) && (prompt != TRUE))
	    {
		msgConfirm("Cannot find file(s) for distribution in ``%s''!\n", path);
		return -1;
	    } else {
		char *tmp = index(buf, '/');
		tmp++;
		    
		while (retval != 0)
		{
		    if (mediaDevice->shutdown)
			(*mediaDevice->shutdown)(mediaDevice);
		    msgConfirm("Please insert the media with the `%s' file on it\n", tmp);
		    if (mediaDevice->init)
			if (!mediaDevice->init(mediaDevice))
			    return -1;
		    retval = stat(buf, &sb);
		}
	    }
	    
	    snprintf(buf, 512, "%s.%c%c", path, (chunk / 26) + 'a', (chunk % 26) + 'a');
	    if ((fd = open(buf, O_RDONLY)) == -1)
		msgFatal("Cannot find file `%s'!", buf);
	    
	    if (prompt == TRUE) {
		extern int crc(int, unsigned long *, unsigned long *);

		crc(fd, &val, &len);
		msgDebug("crc for %s is %lu %lu\n", buf, val, len);
	    }
	    
	    fstat(fd, &sb);
	    msgDebug("mmap()ing %s (%d)\n", buf, fd);
	    memory = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, (off_t) 0);
	    if (memory == (caddr_t) -1)
		msgFatal("mmap error: %s\n", strerror(errno));
	
	    retval = write(1, memory, sb.st_size);
	    if (retval != sb.st_size)
	    {
		msgConfirm("write didn't write out the complete file!\n(wrote %d bytes of %d bytes)", retval,
			   sb.st_size);
		exit(1);
	    }
	    
	    retval = munmap(memory, sb.st_size);
	    if (retval != 0)
	    {
		msgConfirm("munmap() returned %d", retval);
		exit(1);
	    }
	    close(fd);
	}
	close(1);
	msgDebug("Extract of %s finished!!!\n", path);
	exit(0);
    }
    close(pfd[1]);
    return(pfd[0]);
}

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
	bootCD.get = mediaGetCDROM;
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
						 mediaInitDOS, mediaGetDOS, NULL, mediaShutdownDOS, NULL);
		    mediaDevice->private = c1;
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
    char *cp;

    tcpDeviceSelect(NULL);
    dmenuOpenSimple(&MenuMediaFTP);
    cp = getenv("ftp");
    if (!cp)
	return 0;
    if (!strcmp(cp, "other")) {
	cp = msgGetInput("ftp://", "Please specify the URL of a FreeBSD distribution on a\nremote ftp site.  This site must accept anonymous ftp!\nA URL looks like this:  ftp://<hostname>/<path>");
	if (!cp || strncmp("ftp://", cp, 6))
	    return 0;
	else
	    variable_set2("ftp", cp);
    }

    strcpy(ftpDevice.name, cp);
    ftpDevice.type = DEVICE_TYPE_NETWORK;
    ftpDevice.init = mediaInitFTP;
    ftpDevice.get = mediaGetFTP;
    ftpDevice.close = mediaCloseFTP;
    ftpDevice.shutdown = mediaShutdownFTP;
    ftpDevice.private = mediaDevice;
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

Boolean
mediaExtractDist(char *distname, char *dir, int fd)
{
    int i, j, zpid, cpid, pfd[2];

    if (!dir)
	dir = "/";
    msgWeHaveOutput("Extracting %s into %s directory..", distname, dir);

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

    i = waitpid(zpid, &j, 0);
    if (i < 0) { /* Don't check status - gunzip seems to return a bogus one! */
	dialog_clear();
	msgDebug("wait for gunzip returned status of %d!\n", i);
	return FALSE;
    }
    i = waitpid(cpid, &j, 0);
    if (i < 0 || WEXITSTATUS(j)) {
	dialog_clear();
	msgDebug("cpio returned error status of %d!\n", WEXITSTATUS(j));
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
