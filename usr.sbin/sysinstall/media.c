/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media.c,v 1.40 1996/06/16 21:57:31 jkh Exp $
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

#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "sysinstall.h"

static int
genericHook(dialogMenuItem *self, DeviceType type)
{
    Device **devs;

    devs = deviceFind(self->prompt, type);
    if (devs)
	mediaDevice = devs[0];
    return (devs ? DITEM_LEAVE_MENU : DITEM_FAILURE);
}

static int
cdromHook(dialogMenuItem *self)
{
    return genericHook(self, DEVICE_TYPE_CDROM);
}

char *
cpioVerbosity()
{
    char *cp = variable_get(VAR_CPIO_VERBOSITY);

    if (cp && !strcmp(cp, "high"))
	return "-v";
    else if (cp && !strcmp(cp, "medium"))
	return "-V";
    return "";
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a CD.
 */
int
mediaSetCDROM(dialogMenuItem *self)
{
    Device **devs;
    int cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_CDROM);
    cnt = deviceCount(devs);
    if (!cnt) {
	if (self)	/* Interactive? */
	    msgConfirm("No CDROM devices found!  Please check that your system's\n"
		       "configuration is correct and that the CDROM drive is of a supported\n"
		       "type.  For more information, consult the hardware guide\n"
		       "in the Doc menu.");
	return DITEM_FAILURE | DITEM_CONTINUE;
    }
    else if (cnt > 1) {
	DMenu *menu;
	int status;
	
	menu = deviceCreateMenu(&MenuMediaCDROM, DEVICE_TYPE_CDROM, cdromHook, NULL);
	if (!menu)
	    msgFatal("Unable to create CDROM menu!  Something is seriously wrong.");
	status = dmenuOpenSimple(menu);
	free(menu);
	if (!status)
	    return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    }
    else
	mediaDevice = devs[0];
    return (mediaDevice ? DITEM_SUCCESS | DITEM_LEAVE_MENU : DITEM_FAILURE) | DITEM_RESTORE | DITEM_RECREATE;
}

static int
floppyHook(dialogMenuItem *self)
{
    return genericHook(self, DEVICE_TYPE_FLOPPY);
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a floppy
 */
int
mediaSetFloppy(dialogMenuItem *self)
{
    Device **devs;
    int cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_FLOPPY);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No floppy devices found!  Please check that your system's configuration\n"
		   "is correct.  For more information, consult the hardware guide in the Doc\n"
		   "menu.");
	return DITEM_FAILURE | DITEM_CONTINUE;
    }
    else if (cnt > 1) {
	DMenu *menu;
	int status;

	menu = deviceCreateMenu(&MenuMediaFloppy, DEVICE_TYPE_FLOPPY, floppyHook, NULL);
	if (!menu)
	    msgFatal("Unable to create Floppy menu!  Something is seriously wrong.");
	status = dmenuOpenSimple(menu);
	free(menu);
	if (!status)
	    return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    }
    else
	mediaDevice = devs[0];
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE) | DITEM_RESTORE | DITEM_RECREATE;
}

static int
DOSHook(dialogMenuItem *self)
{
    return genericHook(self, DEVICE_TYPE_DOS);
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a DOS partition.
 */
int
mediaSetDOS(dialogMenuItem *self)
{
    Device **devs;
    int cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_DOS);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No DOS primary partitions found!  This installation method is unavailable");
	return DITEM_FAILURE | DITEM_CONTINUE;
    }
    else if (cnt > 1) {
	DMenu *menu;
	int status;

	menu = deviceCreateMenu(&MenuMediaDOS, DEVICE_TYPE_DOS, DOSHook, NULL);
	if (!menu)
	    msgFatal("Unable to create DOS menu!  Something is seriously wrong.");
	status = dmenuOpenSimple(menu);
	free(menu);
	if (!status)
	    return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    }
    else
	mediaDevice = devs[0];
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE) | DITEM_RESTORE | DITEM_RECREATE;
}

static int
tapeHook(dialogMenuItem *self)
{
    return genericHook(self, DEVICE_TYPE_TAPE);
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a tape drive.
 */
int
mediaSetTape(dialogMenuItem *self)
{
    Device **devs;
    int cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_TAPE);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No tape drive devices found!  Please check that your system's configuration\n"
		   "is correct.  For more information, consult the hardware guide in the Doc\n"
		   "menu.");
	return DITEM_FAILURE | DITEM_CONTINUE;
    }
    else if (cnt > 1) {
	DMenu *menu;
	int status;

	menu = deviceCreateMenu(&MenuMediaTape, DEVICE_TYPE_TAPE, tapeHook, NULL);
	if (!menu)
	    msgFatal("Unable to create tape drive menu!  Something is seriously wrong.");
	status = dmenuOpenSimple(menu);
	free(menu);
	if (!status)
	    return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
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
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE) | DITEM_RESTORE | DITEM_RECREATE;
}

/*
 * Return 0 if we successfully found and set the installation type to
 * be an ftp server
 */
int
mediaSetFTP(dialogMenuItem *self)
{
    static Device ftpDevice;
    char *cp, *hostname, *dir;
    extern int FtpPort;

    if (!dmenuOpenSimple(&MenuMediaFTP))
	return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    else
	cp = variable_get(VAR_FTP_PATH);
    if (!cp) {
	msgConfirm("%s not set!  Not setting an FTP installation path, OK?", VAR_FTP_PATH);
	return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
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
	    return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    }
    if (strncmp("ftp://", cp, 6)) {
	msgConfirm("Sorry, %s is an invalid URL!", cp);
	return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    }
    strcpy(ftpDevice.name, cp);

    if (!tcpDeviceSelect())
	return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    if (!mediaDevice || !mediaDevice->init(mediaDevice)) {
	if (isDebug())
	    msgDebug("mediaSetFTP: Net device init failed.\n");
	return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    }
    hostname = cp + 6;
    if ((cp = index(hostname, ':')) != NULL) {
	*(cp++) = '\0';
	FtpPort = strtol(cp, 0, 0);
    }
    else
	FtpPort = 21;
    if ((dir = index(cp ? cp : hostname, '/')) != NULL)
	*(dir++) = '\0';
    if (isDebug()) {
	msgDebug("hostname = `%s'\n", hostname);
	msgDebug("dir = `%s'\n", dir ? dir : "/");
	msgDebug("port # = `%d'\n", FtpPort);
    }
    msgNotify("Looking up host %s..", hostname);
    if ((gethostbyname(hostname) == NULL) && (inet_addr(hostname) == INADDR_NONE)) {
	msgConfirm("Cannot resolve hostname `%s'!  Are you sure that your\n"
		   "name server, gateway and network interface are correctly configured?", hostname);
	return DITEM_FAILURE | DITEM_RESTORE | DITEM_RECREATE;
    }
    variable_set2(VAR_FTP_HOST, hostname);
    variable_set2(VAR_FTP_DIR, dir ? dir : "/");
    variable_set2(VAR_FTP_PORT, itoa(port));
    ftpDevice.type = DEVICE_TYPE_FTP;
    ftpDevice.init = mediaInitFTP;
    ftpDevice.get = mediaGetFTP;
    ftpDevice.close = mediaCloseFTP;
    ftpDevice.shutdown = mediaShutdownFTP;
    ftpDevice.private = mediaDevice; /* Set to network device by tcpDeviceSelect() */
    mediaDevice = &ftpDevice;
    return DITEM_LEAVE_MENU | DITEM_RESTORE | DITEM_RECREATE;
}

int
mediaSetFTPActive(dialogMenuItem *self)
{
    variable_set2(VAR_FTP_STATE, "active");
    return mediaSetFTP(self);
}

int
mediaSetFTPPassive(dialogMenuItem *self)
{
    variable_set2(VAR_FTP_STATE, "passive");
    return mediaSetFTP(self);
}

int
mediaSetUFS(dialogMenuItem *self)
{
    static Device ufsDevice;
    char *cp;

    cp = variable_get_value(VAR_UFS_PATH, "Enter a fully qualified pathname for the directory\n"
			    "containing the FreeBSD distribution files:");
    if (!cp)
	return DITEM_FAILURE;
    strcpy(ufsDevice.name, "ufs");
    ufsDevice.type = DEVICE_TYPE_UFS;
    ufsDevice.init = dummyInit;
    ufsDevice.get = mediaGetUFS;
    ufsDevice.close = dummyClose;
    ufsDevice.shutdown = dummyShutdown;
    ufsDevice.private = strdup(cp);
    mediaDevice = &ufsDevice;
    return DITEM_LEAVE_MENU;
}

int
mediaSetNFS(dialogMenuItem *self)
{
    static Device nfsDevice;
    char *cp, *idx;

    cp = variable_get_value(VAR_NFS_PATH, "Please enter the full NFS file specification for the remote\n"
			    "host and directory containing the FreeBSD distribution files.\n"
			    "This should be in the format:  hostname:/some/freebsd/dir");
    if (!cp)
	return DITEM_FAILURE;
    if (!(idx = index(cp, ':'))) {
	msgConfirm("Invalid NFS path specification.  Must be of the form:\n"
		   "host:/full/pathname/to/FreeBSD/distdir");
	return DITEM_FAILURE;
    }
    strncpy(nfsDevice.name, cp, DEV_NAME_MAX);
    /* str == NULL means we were just called to change NFS paths, not network interfaces */
    if (!tcpDeviceSelect())
	return DITEM_FAILURE;
    if (!mediaDevice || !mediaDevice->init(mediaDevice)) {
	if (isDebug())
	    msgDebug("mediaSetNFS: Net device init failed\n");
	return DITEM_FAILURE;
    }
    *idx = '\0';
    msgNotify("Looking up host %s..", cp);
    if ((gethostbyname(cp) == NULL) && (inet_addr(cp) == INADDR_NONE)) {
	msgConfirm("Cannot resolve hostname `%s'!  Are you sure that your\n"
		   "name server, gateway and network interface are correctly configured?", cp);
	return DITEM_FAILURE;
    }
    variable_set2(VAR_NFS_HOST, cp);
    nfsDevice.type = DEVICE_TYPE_NFS;
    nfsDevice.init = mediaInitNFS;
    nfsDevice.get = mediaGetNFS;
    nfsDevice.close = dummyClose;
    nfsDevice.shutdown = mediaShutdownNFS;
    nfsDevice.private = mediaDevice;
    mediaDevice = &nfsDevice;
    return DITEM_LEAVE_MENU;
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
	char *gunzip = RunningAsInit ? "/stand/gunzip" : "/usr/bin/gunzip";

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
	i = execl(gunzip, gunzip, 0);
	if (isDebug())
	    msgDebug("%s command returns %d status\n", gunzip, i);
	exit(i);
    }
    *fd = qfd[1];
    close(qfd[0]);
    *cpid = fork();
    if (!*cpid) {
	char *cpio = RunningAsInit ? "/stand/cpio" : "/usr/bin/cpio";

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
	if (strlen(cpioVerbosity()))
	    i = execl(cpio, cpio, "-idum", cpioVerbosity(), "--block-size", mediaTapeBlocksize(), 0);
	else
	    i = execl(cpio, cpio, "-idum", "--block-size", mediaTapeBlocksize(), 0);
	if (isDebug())
	    msgDebug("%s command returns %d status\n", cpio, i);
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
    /* Don't check status - gunzip seems to return a bogus one! */
    if (i < 0) {
	if (isDebug())
	    msgDebug("wait for gunzip returned status of %d!\n", i);
	return FALSE;
    }
    i = waitpid(cpid, &j, 0);
    if (i < 0 || WEXITSTATUS(j)) {
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
	char *gunzip = RunningAsInit ? "/stand/gunzip" : "/usr/bin/gunzip";

	dup2(fd, 0); close(fd);
	dup2(pfd[1], 1); close(pfd[1]);
	if (DebugFD != -1)
	    dup2(DebugFD, 2);
	else {
	    close(2);
	    open("/dev/null", O_WRONLY);
	}
	close(pfd[0]);
	i = execl(gunzip, gunzip, 0);
	if (isDebug())
	    msgDebug("%s command returns %d status\n", gunzip, i);
	exit(i);
    }
    cpid = fork();
    if (!cpid) {
	char *cpio = RunningAsInit ? "/stand/cpio" : "/usr/bin/cpio";

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
	if (strlen(cpioVerbosity()))
	    i = execl(cpio, cpio, "-idum", cpioVerbosity(), "--block-size", mediaTapeBlocksize(), 0);
	else
	    i = execl(cpio, cpio, "-idum", "--block-size", mediaTapeBlocksize(), 0);
	if (isDebug())
	    msgDebug("%s command returns %d status\n", cpio, i);
	exit(i);
    }
    close(pfd[0]);
    close(pfd[1]);

    i = waitpid(zpid, &j, 0);
    /* Don't check status - gunzip seems to return a bogus one! */
    if (i < 0) {
	if (isDebug())
	    msgDebug("wait for gunzip returned status of %d!\n", i);
	return FALSE;
    }
    i = waitpid(cpid, &j, 0);
    if (i < 0 || WEXITSTATUS(j)) {
	if (isDebug())
	    msgDebug("cpio returned error status of %d!\n", WEXITSTATUS(j));
	return FALSE;
    }
    return TRUE;
}

int
mediaGetType(dialogMenuItem *self)
{
    int i;

    i = dmenuOpenSimple(&MenuMedia) ? DITEM_SUCCESS : DITEM_FAILURE;
    return i | DITEM_RESTORE | DITEM_RECREATE;
}

/* Return TRUE if all the media variables are set up correctly */
Boolean
mediaVerify(void)
{
    if (!mediaDevice) {
	msgConfirm("Media type not set!  Please select a media type\n"
		   "from the Installation menu before proceeding.");
	return DITEM_STATUS(mediaGetType(NULL)) == DITEM_SUCCESS;
    }
    return TRUE;
}

/* Set FTP error behavior */
int
mediaSetFtpOnError(dialogMenuItem *self)
{
    char *cp = variable_get(VAR_FTP_ONERROR);

    if (!cp) {
	msgConfirm("FTP error handling is not set to anything!");
	return DITEM_FAILURE;
    }
    else {
	if (!strcmp(cp, "abort"))
	    variable_set2(VAR_FTP_ONERROR, "retry");
	else if (!strcmp(cp, "retry"))
	    variable_set2(VAR_FTP_ONERROR, "reselect");
	else /* must be "reselect" - wrap around */
	    variable_set2(VAR_FTP_ONERROR, "abort");
    }
    return DITEM_SUCCESS;
}

/* Set the FTP username and password fields */
int
mediaSetFtpUserPass(dialogMenuItem *self)
{
    char *pass;

    if (variable_get_value(VAR_FTP_USER, "Please enter the username you wish to login as:"))
	pass = variable_get_value(VAR_FTP_PASS, "Please enter the password for this user:");
    else
	pass = NULL;
    return pass ? DITEM_SUCCESS : DITEM_FAILURE;
}

/* Set CPIO verbosity level */
int
mediaSetCPIOVerbosity(dialogMenuItem *self)
{
    char *cp = variable_get(VAR_CPIO_VERBOSITY);

    if (!cp) {
	msgConfirm("CPIO Verbosity is not set to anything!");
	return DITEM_FAILURE;
    }
    else {
	if (!strcmp(cp, "low"))
	    variable_set2(VAR_CPIO_VERBOSITY, "medium");
	else if (!strcmp(cp, "medium"))
	    variable_set2(VAR_CPIO_VERBOSITY, "high");
	else /* must be "high" - wrap around */
	    variable_set2(VAR_CPIO_VERBOSITY, "low");
    }
    return DITEM_SUCCESS;
}
