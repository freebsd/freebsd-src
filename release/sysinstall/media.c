/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media.c,v 1.25.2.21 1995/10/22 08:33:16 jkh Exp $
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

char *
cpioVerbosity()
{
    if (!strcmp(variable_get(VAR_CPIO_VERBOSITY), "high"))
	return "-v";
    else if (!strcmp(variable_get(VAR_CPIO_VERBOSITY), "medium"))
	return "-V";
    return "";
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
	dialog_clear();
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
	dialog_clear();
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
	dialog_clear();
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
	dialog_clear();
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

    if (!(str && !strcmp(str, "script") && (cp = variable_get(VAR_FTP_PATH)))) {
	if (!dmenuOpenSimple(&MenuMediaFTP))
	    return RET_FAIL;
	else
	    cp = variable_get(VAR_FTP_PATH);
    }
    if (!cp) {
	dialog_clear();
	msgConfirm("%s not set!  Not setting an FTP installation path, OK?", VAR_FTP_PATH);
	return RET_FAIL;
    }
    else if (!strcmp(cp, "other")) {
	variable_set2(VAR_FTP_PATH, "ftp://");
	cp = variable_get_value(VAR_FTP_PATH, "Please specify the URL of a FreeBSD distribution on a\n"
				"remote ftp site.  This site must accept either anonymous\n"
				"ftp or you should have set an ftp username and password\n"
				"in the Options screen.\n\n"
				"A URL looks like this:  ftp://<hostname>/<path>\n"
				"Where <path> is relative to the anonymous ftp directory or the\n"
				"home directory of the user being logged in as.");
	if (!cp || !*cp)
	    return RET_FAIL;
    }
    if (strncmp("ftp://", cp, 6)) {
	dialog_clear();
	msgConfirm("Sorry, %s is an invalid URL!", cp);
	return RET_FAIL;
    }
    strcpy(ftpDevice.name, cp);

    /* If str == NULL || "script", we were called just to change FTP sites, not network devices */
    if (str && strcmp(str, "script") && !tcpDeviceSelect())
	return RET_FAIL;

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
    variable_set2(VAR_FTP_STATE, "active");
    return mediaSetFTP(str);
}

int
mediaSetFTPPassive(char *str)
{
    variable_set2(VAR_FTP_STATE, "passive");
    return mediaSetFTP(str);
}

int
mediaSetUFS(char *str)
{
    static Device ufsDevice;
    char *val;

    if (!(str && !strcmp(str, "script") && (val = variable_get(VAR_UFS_PATH)))) {
	val = variable_get_value(VAR_UFS_PATH, "Enter a fully qualified pathname for the directory\n"
				 "containing the FreeBSD distribution files:");
	if (!val)
	    return RET_FAIL;
    }
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
    char *cp;

    if (!(str && !strcmp(str, "script") && (cp = variable_get(VAR_NFS_PATH)))) {
	cp = variable_get_value(VAR_NFS_PATH, "Please enter the full NFS file specification for the remote\n"
				"host and directory containing the FreeBSD distribution files.\n"
				"This should be in the format:  hostname:/some/freebsd/dir");
	if (!cp)
	    return RET_FAIL;
    }
    strncpy(nfsDevice.name, cp, DEV_NAME_MAX);
    /* str == NULL means we were just called to change NFS paths, not network interfaces */
    if (str && strcmp(str, "script") && !tcpDeviceSelect())
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
	i = execl("/stand/cpio", "/stand/cpio", "-idum", cpioVerbosity(), "--block-size", mediaTapeBlocksize(), 0);
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
	i = execl("/stand/cpio", "/stand/cpio", "-idum", cpioVerbosity(), "--block-size", mediaTapeBlocksize(), 0);
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

int
mediaGetType(char *unused)
{
    if (!dmenuOpenSimple(&MenuMedia))
	return RET_FAIL;
    return RET_SUCCESS;
}

/* Return TRUE if all the media variables are set up correctly */
Boolean
mediaVerify(void)
{
    if (!mediaDevice) {
	dialog_clear();
	msgConfirm("Media type not set!  Please select a media type\n"
		   "from the Installation menu before proceeding.");
	return mediaGetType(NULL) == RET_SUCCESS;
    }
    return TRUE;
}

/* Set FTP error behavior */
int
mediaSetFtpOnError(char *str)
{
    char *cp = variable_get(VAR_FTP_ONERROR);

    if (!cp) {
	dialog_clear();
	msgConfirm("FTP error handling is not set to anything!");
	return RET_FAIL;
    }
    else {
	if (!strcmp(cp, "abort"))
	    variable_set2(VAR_FTP_ONERROR, "retry");
	else if (!strcmp(cp, "retry"))
	    variable_set2(VAR_FTP_ONERROR, "reselect");
	else /* must be "reselect" - wrap around */
	    variable_set2(VAR_FTP_ONERROR, "abort");
    }
    return RET_SUCCESS;
}

/* Set the FTP username and password fields */
int
mediaSetFtpUserPass(char *str)
{
    char *pass;

    dialog_clear();
    if (variable_get_value(VAR_FTP_USER, "Please enter the username you wish to login as:"))
	pass = variable_get_value(VAR_FTP_PASS, "Please enter the password for this user:");
    else
	pass = NULL;
    dialog_clear();
    return pass ? RET_SUCCESS : RET_FAIL;
}

/* Set CPIO verbosity level */
int
mediaSetCPIOVerbosity(char *str)
{
    char *cp = variable_get(VAR_CPIO_VERBOSITY);

    if (!cp) {
	dialog_clear();
	msgConfirm("CPIO Verbosity is not set to anything!");
	return RET_FAIL;
    }
    else {
	if (!strcmp(cp, "low"))
	    variable_set2(VAR_CPIO_VERBOSITY, "medium");
	else if (!strcmp(cp, "medium"))
	    variable_set2(VAR_CPIO_VERBOSITY, "high");
	else /* must be "high" - wrap around */
	    variable_set2(VAR_CPIO_VERBOSITY, "low");
    }
    return RET_SUCCESS;
}
