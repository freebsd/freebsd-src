/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media.c,v 1.86 1997/08/01 04:41:38 jkh Exp $
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
#include <signal.h>
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
#include <resolv.h>

static Boolean got_intr = FALSE;

/* timeout handler */
static void
handle_intr(int sig)
{
    msgDebug("User generated interrupt.\n");
    got_intr = TRUE;
}

static int
check_for_interrupt(void)
{
    if (got_intr) {
	got_intr = FALSE;
	return TRUE;
    }
    return FALSE;
}

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

static void
kickstart_dns(void)
{
    static Boolean initted = FALSE;
    int time;
    char *cp;

    cp = variable_get(VAR_MEDIA_TIMEOUT);
    if (!cp)
	time = MEDIA_TIMEOUT;
    else
	time = atoi(cp);
    if (!time)
	time = 100;
    if (!initted) {
	res_init();
	_res.retry = 2;	/* 2 times seems a reasonable number to me */
	_res.retrans = time / 2; /* so spend half our alloted time on each try */
	initted = TRUE;
    }
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

void
mediaClose(void)
{
    if (mediaDevice)
	mediaDevice->shutdown(mediaDevice);
    mediaDevice = NULL;
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

    mediaClose();
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
	status = dmenuOpenSimple(menu, FALSE);
	free(menu);
	if (!status)
	    return DITEM_FAILURE | DITEM_RESTORE;
    }
    else
	mediaDevice = devs[0];
    return (mediaDevice ? DITEM_SUCCESS | DITEM_LEAVE_MENU : DITEM_FAILURE) | DITEM_RESTORE;
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

    mediaClose();
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
	status = dmenuOpenSimple(menu, FALSE);
	free(menu);
	if (!status)
	    return DITEM_FAILURE | DITEM_RESTORE;
    }
    else
	mediaDevice = devs[0];
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE) | DITEM_RESTORE;
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

    mediaClose();
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
	status = dmenuOpenSimple(menu, FALSE);
	free(menu);
	if (!status)
	    return DITEM_FAILURE | DITEM_RESTORE;
    }
    else
	mediaDevice = devs[0];
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE) | DITEM_RESTORE;
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

    mediaClose();
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
	status = dmenuOpenSimple(menu, FALSE);
	free(menu);
	if (!status)
	    return DITEM_FAILURE | DITEM_RESTORE;
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
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE) | DITEM_RESTORE;
}

/*
 * Return 0 if we successfully found and set the installation type to
 * be an ftp server
 */
int
mediaSetFTP(dialogMenuItem *self)
{
    static Device ftpDevice;
    char *cp, hostname[MAXHOSTNAMELEN], *dir;
    extern int FtpPort;
    static Device *networkDev = NULL;
    int what = DITEM_RESTORE;

    mediaClose();
    cp = variable_get(VAR_FTP_PATH);
    /* If we've been through here before ... */
    if (!variable_get(VAR_NONINTERACTIVE))
	if (networkDev && cp && msgYesNo("Re-use old FTP site selection values?"))
	    cp = NULL;

    if (!cp) {
	dialog_clear_norefresh();
	if (!dmenuOpenSimple(&MenuMediaFTP, FALSE))
	    return DITEM_FAILURE | DITEM_RESTORE;
	else
	    cp = variable_get(VAR_FTP_PATH);
	what = DITEM_RESTORE;
    }
    if (!cp)
	return DITEM_FAILURE | what;
    else if (!strcmp(cp, "other")) {
	variable_set2(VAR_FTP_PATH, "ftp://");
	dialog_clear_norefresh();
	cp = variable_get_value(VAR_FTP_PATH, "Please specify the URL of a FreeBSD distribution on a\n"
				"remote ftp site.  This site must accept either anonymous\n"
				"ftp or you should have set an ftp username and password\n"
				"in the Options screen.\n\n"
				"A URL looks like this:  ftp://<hostname>/<path>\n"
				"Where <path> is relative to the anonymous ftp directory or the\n"
				"home directory of the user being logged in as.");
	if (!cp || !*cp || !strcmp(cp, "ftp://")) {
	    variable_unset(VAR_FTP_PATH);
	    return DITEM_FAILURE | what;
	}
    }
    if (strncmp("ftp://", cp, 6)) {
	msgConfirm("Sorry, %s is an invalid URL!", cp);
	variable_unset(VAR_FTP_PATH);
	return DITEM_FAILURE | what;
    }
    SAFE_STRCPY(ftpDevice.name, cp);
    SAFE_STRCPY(hostname, cp + 6);

    dialog_clear_norefresh();
    if (!networkDev || msgYesNo("You've already done the network configuration once,\n"
				"would you like to skip over it now?") != 0) {
	if (networkDev)
	    networkDev->shutdown(networkDev);
	if (!(networkDev = tcpDeviceSelect())) {
	    variable_unset(VAR_FTP_PATH);
	    return DITEM_FAILURE | what;
	}
    }
    if (!networkDev->init(networkDev)) {
	if (isDebug())
	    msgDebug("mediaSetFTP: Net device init failed.\n");
	variable_unset(VAR_FTP_PATH);
	return DITEM_FAILURE | what;
    }
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
    if (variable_get(VAR_NAMESERVER)) {
	kickstart_dns();
	if ((inet_addr(hostname) == INADDR_NONE) && (gethostbyname(hostname) == NULL)) {
	    msgConfirm("Cannot resolve hostname `%s'!  Are you sure that your\n"
		       "name server, gateway and network interface are correctly configured?", hostname);
	    if (networkDev)
		networkDev->shutdown(networkDev);
	    networkDev = NULL;
	    variable_unset(VAR_FTP_PATH);
	    return DITEM_FAILURE | what;
	}
	else
	    msgDebug("Found DNS entry for %s successfully..", hostname);
    }
    variable_set2(VAR_FTP_HOST, hostname);
    variable_set2(VAR_FTP_DIR, dir ? dir : "/");
    variable_set2(VAR_FTP_PORT, itoa(FtpPort));
    ftpDevice.type = DEVICE_TYPE_FTP;
    ftpDevice.init = mediaInitFTP;
    ftpDevice.get = mediaGetFTP;
    ftpDevice.shutdown = mediaShutdownFTP;
    ftpDevice.private = networkDev;
    mediaDevice = &ftpDevice;
    return DITEM_SUCCESS | DITEM_LEAVE_MENU | what;
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

    mediaClose();
    dialog_clear_norefresh();
    cp = variable_get_value(VAR_UFS_PATH, "Enter a fully qualified pathname for the directory\n"
			    "containing the FreeBSD distribution files:");
    if (!cp)
	return DITEM_FAILURE;
    strcpy(ufsDevice.name, "ufs");
    ufsDevice.type = DEVICE_TYPE_UFS;
    ufsDevice.init = dummyInit;
    ufsDevice.get = mediaGetUFS;
    ufsDevice.shutdown = dummyShutdown;
    ufsDevice.private = strdup(cp);
    mediaDevice = &ufsDevice;
    return DITEM_LEAVE_MENU;
}

int
mediaSetNFS(dialogMenuItem *self)
{
    static Device nfsDevice;
    static Device *networkDev = NULL;
    char *cp, *idx;
    char hostname[MAXHOSTNAMELEN];
    mediaClose();
    dialog_clear_norefresh();
    cp = variable_get_value(VAR_NFS_PATH, "Please enter the full NFS file specification for the remote\n"
			    "host and directory containing the FreeBSD distribution files.\n"
			    "This should be in the format:  hostname:/some/freebsd/dir");
    if (!cp)
	return DITEM_FAILURE;
    SAFE_STRCPY(hostname, cp);
    if (!(idx = index(hostname, ':'))) {
	msgConfirm("Invalid NFS path specification.  Must be of the form:\n"
		   "host:/full/pathname/to/FreeBSD/distdir");
	return DITEM_FAILURE;
    }
    SAFE_STRCPY(nfsDevice.name, hostname);
    *idx = '\0';
    if (!networkDev || msgYesNo("You've already done the network configuration once,\n"
				"would you like to skip over it now?") != 0) {
	if (networkDev)
	    networkDev->shutdown(networkDev);
	if (!(networkDev = tcpDeviceSelect()))
	    return DITEM_FAILURE;
    }
    if (!networkDev->init(networkDev)) {
	if (isDebug())
	    msgDebug("mediaSetNFS: Net device init failed\n");
    }
    if (variable_get(VAR_NAMESERVER)) {
	kickstart_dns();
	if ((inet_addr(hostname) == INADDR_NONE) && (gethostbyname(hostname) == NULL)) {
	    msgConfirm("Cannot resolve hostname `%s'!  Are you sure that your\n"
		       "name server, gateway and network interface are correctly configured?", hostname);
	    if (networkDev)
		networkDev->shutdown(networkDev);
	    networkDev = NULL;
	    variable_unset(VAR_NFS_PATH);
	    return DITEM_FAILURE;
	}
	else
	    msgDebug("Found DNS entry for %s successfully..", hostname);
    }
    variable_set2(VAR_NFS_HOST, hostname);
    nfsDevice.type = DEVICE_TYPE_NFS;
    nfsDevice.init = mediaInitNFS;
    nfsDevice.get = mediaGetNFS;
    nfsDevice.shutdown = mediaShutdownNFS;
    nfsDevice.private = networkDev;
    mediaDevice = &nfsDevice;
    return DITEM_LEAVE_MENU;
}

Boolean
mediaExtractDistBegin(char *dir, int *fd, int *zpid, int *cpid)
{
    int i, pfd[2],qfd[2];

    if (!dir)
	dir = "/";
    Mkdir(dir);
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
    /* Don't check exit status - gunzip seems to return a bogus one! */
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
mediaExtractDist(char *dir, char *dist, FILE *fp)
{
    int i, j, total, seconds, zpid, cpid, pfd[2], qfd[2];
    char buf[BUFSIZ];
    struct timeval start, stop;
    struct sigaction new, old;

    if (!dir)
	dir = "/";

    Mkdir(dir);
    chdir(dir);
    pipe(pfd);	/* read end */
    pipe(qfd);	/* write end */
    zpid = fork();
    if (!zpid) {
	char *gunzip = RunningAsInit ? "/stand/gunzip" : "/usr/bin/gunzip";

	fclose(fp);
	close(qfd[1]);
	dup2(qfd[0], 0); close(qfd[0]);

	close(pfd[0]); 
	dup2(pfd[1], 1); close(pfd[1]);

	if (DebugFD != -1)
	    dup2(DebugFD, 2);
	else {
	    close(2);
	    open("/dev/null", O_WRONLY);
	}
	i = execl(gunzip, gunzip, 0);
	if (isDebug())
	    msgDebug("%s command returns %d status\n", gunzip, i);
	exit(i);
    }
    cpid = fork();
    if (!cpid) {
	char *cpio = RunningAsInit ? "/stand/cpio" : "/usr/bin/cpio";

	close(pfd[1]);
	dup2(pfd[0], 0); close(pfd[0]);
	close (qfd[0]); close(qfd[1]);
	fclose(fp);
	if (DebugFD != -1) {
	    dup2(DebugFD, 1);
	    dup2(DebugFD, 2);
	}
	else {
	    dup2(open("/dev/null", O_WRONLY), 1);
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
    close(pfd[0]); close(pfd[1]);
    close(qfd[0]);

    total = 0;
    (void)gettimeofday(&start, (struct timezone *)0);

    /* Make ^C abort the current transfer rather than the whole show */
    new.sa_handler = handle_intr;
    new.sa_flags = 0;
    new.sa_mask = 0;
    sigaction(SIGINT, &new, &old);

    while ((i = fread(buf, 1, BUFSIZ, fp)) > 0) {
	if (check_for_interrupt()) {
	    msgConfirm("Failure to read from media:  User interrupt.");
	    break;
	}
	if (write(qfd[1], buf, i) != i) {
	    msgConfirm("Write error on transfer to cpio process, try of %d bytes.", i);
	    break;
	}
	else {
	    (void)gettimeofday(&stop, (struct timezone *)0);
	    stop.tv_sec = stop.tv_sec - start.tv_sec;
	    stop.tv_usec = stop.tv_usec - start.tv_usec;
	    if (stop.tv_usec < 0)
		stop.tv_sec--, stop.tv_usec += 1000000;
	    seconds = stop.tv_sec + (stop.tv_usec / 1000000.0);
	    if (!seconds)
		seconds = 1;
	    total += i;
	    msgInfo("%10d bytes read from %s dist @ %.1f KB/sec.",
		    total, dist, (total / seconds) / 1024.0);
	}
    }
    sigaction(SIGINT, &old, NULL);	/* restore sigint */
    close(qfd[1]);

    i = waitpid(zpid, &j, 0);
    /* Don't check exit status - gunzip seems to return a bogus one! */
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
    return ((dmenuOpenSimple(&MenuMedia, FALSE) && mediaDevice) ? DITEM_SUCCESS : DITEM_FAILURE) | DITEM_RESTORE;
}

/* Return TRUE if all the media variables are set up correctly */
Boolean
mediaVerify(void)
{
    if (!mediaDevice)
	return (DITEM_STATUS(mediaGetType(NULL)) == DITEM_SUCCESS);
    return TRUE;
}

/* Set the FTP username and password fields */
int
mediaSetFTPUserPass(dialogMenuItem *self)
{
    char *pass;

    dialog_clear_norefresh();
    if (variable_get_value(VAR_FTP_USER, "Please enter the username you wish to login as:")) {
	dialog_clear_norefresh();
	DialogInputAttrs |= DITEM_NO_ECHO;
	pass = variable_get_value(VAR_FTP_PASS, "Please enter the password for this user:");
	DialogInputAttrs &= ~DITEM_NO_ECHO;
    }
    else
	pass = NULL;
    return (pass ? DITEM_SUCCESS : DITEM_FAILURE) | DITEM_RESTORE;
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
