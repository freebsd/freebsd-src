/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media.c,v 1.25.2.10 1995/10/16 23:02:24 jkh Exp $
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

    devs = deviceFind(NULL, DEVICE_TYPE_CDROM);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No CDROM devices found!  Please check that your system's\n"
		   "configuration is correct and that the CDROM drive is of a supported\n"
		   "type.  For more information, consult the hardware guide\n"
		   "in the Doc menu.");
	return RET_FAIL;
    }
    else if (cnt > 1) {
	DMenu *menu;
	int status;
	
	menu = deviceCreateMenu(&MenuMediaCDROM, DEVICE_TYPE_CDROM, cdromHook);
	if (!menu)
	    msgFatal("Unable to create CDROM menu!  Something is seriously wrong.");
	status = dmenuOpenSimple(menu);
	free(menu);
	if (!status)
	    return RET_FAIL;
    }
    else
	mediaDevice = devs[0];
    return mediaDevice ? RET_DONE : RET_FAIL;
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
	msgConfirm("No floppy devices found!  Please check that your system's configuration\n"
		   "is correct.  For more information, consult the hardware guide in the Doc\n"
		   "menu.");
	return RET_FAIL;
    }
    else if (cnt > 1) {
	DMenu *menu;
	int status;

	menu = deviceCreateMenu(&MenuMediaFloppy, DEVICE_TYPE_FLOPPY, floppyHook);
	if (!menu)
	    msgFatal("Unable to create Floppy menu!  Something is seriously wrong.");
	status = dmenuOpenSimple(menu);
	free(menu);
	if (!status)
	    return RET_FAIL;
    }
    else
	mediaDevice = devs[0];
    return mediaDevice ? RET_DONE : RET_FAIL;
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
    int cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_DOS);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No DOS primary partitions found!  This installation method is unavailable");
	return RET_FAIL;
    }
    else if (cnt > 1) {
	DMenu *menu;
	int status;

	menu = deviceCreateMenu(&MenuMediaDOS, DEVICE_TYPE_DOS, DOSHook);
	if (!menu)
	    msgFatal("Unable to create DOS menu!  Something is seriously wrong.");
	status = dmenuOpenSimple(menu);
	free(menu);
	if (!status)
	    return RET_FAIL;
    }
    else
	mediaDevice = devs[0];
    return mediaDevice ? RET_DONE : RET_FAIL;
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
	msgConfirm("No tape drive devices found!  Please check that your system's configuration\n"
		   "is correct.  For more information, consult the hardware guide in the Doc\n"
		   "menu.");
	return RET_FAIL;
    }
    else if (cnt > 1) {
	DMenu *menu;
	int status;

	menu = deviceCreateMenu(&MenuMediaTape, DEVICE_TYPE_TAPE, tapeHook);
	if (!menu)
	    msgFatal("Unable to create tape drive menu!  Something is seriously wrong.");
	status = dmenuOpenSimple(menu);
	free(menu);
	if (!status)
	    return RET_FAIL;
    }
    else
	mediaDevice = devs[0];
    if (mediaDevice) {
	char *val;

	val = msgGetInput("/usr/tmp", "Please enter the name of a temporary directory containing\n"
			  "sufficient space for holding the contents of this tape (or\n"
			  "tapes).  The contents of this directory will be removed\n"
			  "after installation, so be sure to specify a directory that\n"
			  "can be erased afterwards!\n");
	if (!val)
	    mediaDevice = NULL;
	else
	    mediaDevice->private = strdup(val);
    }
    return mediaDevice ? RET_DONE : RET_FAIL;
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

    if (!dmenuOpenSimple(&MenuMediaFTP))
	return RET_FAIL;
    if (!strcmp(cp = variable_get("ftp"), "other")) {
	variable_set2("ftp", "ftp://");
	if (variable_get_value("ftp", "Please specify the URL of a FreeBSD distribution on a\n"
			       "remote ftp site.  This site must accept either anonymous\n"
			       "ftp or you should have set an ftp username and password\n"
			       "in the Options screen.\n\n"
			       "A URL looks like this:  ftp://<hostname>/<path>\n"
			       "Where <path> is relative to the anonymous ftp directory or the\n"
			       "home directory of the user being logged in as.") == RET_SUCCESS)
	    cp = variable_get("ftp");
	else
	    return RET_FAIL;
    }
    if (!cp || strncmp("ftp://", cp, 6))
	return RET_FAIL;
    strcpy(ftpDevice.name, cp);
    /* XXX hack: if str == NULL, we were called by an ftp strategy routine and don't need to reinit all */
    if (!str)
	return RET_DONE;
    if (RunningAsInit) {
	if (!tcpDeviceSelect())
	    return RET_FAIL;
    }
    else
	mediaDevice = NULL;
    ftpDevice.type = DEVICE_TYPE_FTP;
    ftpDevice.init = mediaInitFTP;
    ftpDevice.get = mediaGetFTP;
    ftpDevice.close = mediaCloseFTP;
    ftpDevice.shutdown = mediaShutdownFTP;
    ftpDevice.private = mediaDevice; /* Set to network device by tcpDeviceSelect() */
    mediaDevice = &ftpDevice;
    return RET_DONE;
}

int
mediaSetFTPActive(char *str)
{
    OptFlags &= OPT_FTP_ACTIVE;
    return mediaSetFTP(str);
}

int
mediaSetFTPPassive(char *str)
{
    OptFlags &= OPT_FTP_PASSIVE;
    return mediaSetFTP(str);
}

int
mediaSetUFS(char *str)
{
    static Device ufsDevice;
    char *val;

    val = msgGetInput(NULL, "Enter a fully qualified pathname for the directory\n"
		      "containing the FreeBSD distribution files:");
    if (!val)
	return RET_FAIL;
    strcpy(ufsDevice.name, "ufs");
    ufsDevice.type = DEVICE_TYPE_UFS;
    ufsDevice.init = dummyInit;
    ufsDevice.get = mediaGetUFS;
    ufsDevice.close = dummyClose;
    ufsDevice.shutdown = dummyShutdown;
    ufsDevice.private = strdup(val);
    mediaDevice = &ufsDevice;
    return RET_DONE;
}

int
mediaSetNFS(char *str)
{
    static Device nfsDevice;
    char *val;

    val = msgGetInput(nfsDevice.name, "Please enter the full NFS file specification for the remote\n"
		      "host and directory containing the FreeBSD distribution files.\n"
		      "This should be in the format:  hostname:/some/freebsd/dir");
    if (!val)
	return RET_FAIL;
    strncpy(nfsDevice.name, val, DEV_NAME_MAX);
    if (!tcpDeviceSelect())
	return RET_FAIL;
    nfsDevice.type = DEVICE_TYPE_NFS;
    nfsDevice.init = mediaInitNFS;
    nfsDevice.get = mediaGetNFS;
    nfsDevice.close = dummyClose;
    nfsDevice.shutdown = mediaShutdownNFS;
    nfsDevice.private = mediaDevice;
    mediaDevice = &nfsDevice;
    return RET_DONE;
}

Boolean
mediaExtractDistBegin(char *dir, int *fd, int *zpid, int *cpid)
{
    int i, pfd[2],qfd[2];

    if (!dir)
	dir = "/";
    Mkdir(dir, NULL);
    chdir(dir);
    pipe(pfd);
    pipe(qfd);
    *zpid = fork();
    if (!*zpid) {
	dup2(qfd[0], 0); close(qfd[0]);
	dup2(pfd[1], 1); close(pfd[1]);
	if (DebugFD != -1)
	    dup2(DebugFD, 2);
	else {
	    close(2);
	    open("/dev/null", O_WRONLY);
	}
	close(qfd[1]);
	close(pfd[0]);
	i = execl("/stand/gunzip", "/stand/gunzip", 0);
	if (isDebug())
	    msgDebug("/stand/gunzip command returns %d status\n", i);
	exit(i);
    }
    *fd = qfd[1];
    close(qfd[0]);
    *cpid = fork();
    if (!*cpid) {
	dup2(pfd[0], 0); close(pfd[0]);
	close(pfd[1]);
	close(qfd[1]);
	if (DebugFD != -1) {
	    dup2(DebugFD, 1);
	    dup2(DebugFD, 2);
	}
	else {
	    close(1); open("/dev/null", O_WRONLY);
	    dup2(1, 2);
	}
	i = execl("/stand/cpio", "/stand/cpio", "-idum", CPIO_VERBOSITY, "--block-size", mediaTapeBlocksize(), 0);
	if (isDebug())
	    msgDebug("/stand/cpio command returns %d status\n", i);
	exit(i);
    }
    close(pfd[0]);
    close(pfd[1]);
    return TRUE;
}

Boolean
mediaExtractDistEnd(int zpid, int cpid)
{
    int i,j;

    i = waitpid(zpid, &j, 0);
    if (i < 0) { /* Don't check status - gunzip seems to return a bogus one! */
	dialog_clear();
	if (isDebug())
	    msgDebug("wait for gunzip returned status of %d!\n", i);
	return FALSE;
    }
    i = waitpid(cpid, &j, 0);
    if (i < 0 || WEXITSTATUS(j)) {
	dialog_clear();
	if (isDebug())
	    msgDebug("cpio returned error status of %d!\n", WEXITSTATUS(j));
	return FALSE;
    }
    return TRUE;
}


Boolean
mediaExtractDist(char *dir, int fd)
{
    int i, j, zpid, cpid, pfd[2];

    if (!dir)
	dir = "/";

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
	if (isDebug())
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
	i = execl("/stand/cpio", "/stand/cpio", "-idum", CPIO_VERBOSITY, "--block-size", mediaTapeBlocksize(), 0);
	if (isDebug())
	    msgDebug("/stand/cpio command returns %d status\n", i);
	exit(i);
    }
    close(pfd[0]);
    close(pfd[1]);

    i = waitpid(zpid, &j, 0);
    if (i < 0) { /* Don't check status - gunzip seems to return a bogus one! */
	dialog_clear();
	if (isDebug())
	    msgDebug("wait for gunzip returned status of %d!\n", i);
	return FALSE;
    }
    i = waitpid(cpid, &j, 0);
    if (i < 0 || WEXITSTATUS(j)) {
	dialog_clear();
	if (isDebug())
	    msgDebug("cpio returned error status of %d!\n", WEXITSTATUS(j));
	return FALSE;
    }
    return TRUE;
}

Boolean
mediaGetType(void)
{
    if (!dmenuOpenSimple(&MenuMedia))
	return FALSE;
    return TRUE;
}

/* Return TRUE if all the media variables are set up correctly */
Boolean
mediaVerify(void)
{
    if (!mediaDevice) {
	msgConfirm("Media type not set!  Please select a media type\n"
		   "from the Installation menu before proceeding.");
	return mediaGetType();
    }
    return TRUE;
}

/* Set the FTP username and password fields */
int
mediaSetFtpUserPass(char *str)
{
    int i;

    dialog_clear();
    if (variable_get_value(FTP_USER, "Please enter the username you wish to login as") == RET_SUCCESS)
	i = variable_get_value(FTP_PASS, "Please enter the password for this user.");
    else
	i = RET_FAIL;
    dialog_clear();
    return i;
}

/* Set CPIO verbosity level */
int
mediaSetCPIOVerbosity(char *str)
{
    char *cp = variable_get(CPIO_VERBOSITY_LEVEL);

    if (!cp) {
	msgConfirm("CPIO Verbosity is not set to anything!");
	return RET_FAIL;
    }
    else {
	if (!strcmp(cp, "low"))
	    variable_set2(CPIO_VERBOSITY_LEVEL, "medium");
	else if (!strcmp(cp, "medium"))
	    variable_set2(CPIO_VERBOSITY_LEVEL, "high");
	else /* must be "high" - wrap around */
	    variable_set2(CPIO_VERBOSITY_LEVEL, "low");
    }
    return RET_SUCCESS;
}
