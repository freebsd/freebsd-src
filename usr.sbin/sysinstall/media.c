/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
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
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>

static Boolean got_intr = FALSE;
static Boolean ftp_skip_resolve = FALSE;

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
	DEVICE_SHUTDOWN(mediaDevice);
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
	    msgConfirm("No CD/DVD devices found!  Please check that your system's\n"
		       "configuration is correct and that the CD/DVD drive is of a supported\n"
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
	    return DITEM_FAILURE;
    }
    else
	mediaDevice = devs[0];
    return (mediaDevice ? DITEM_SUCCESS | DITEM_LEAVE_MENU : DITEM_FAILURE);
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
	    return DITEM_FAILURE;
    }
    else
	mediaDevice = devs[0];
    if (mediaDevice)
	mediaDevice->private = NULL;
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE);
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
	    return DITEM_FAILURE;
    }
    else
	mediaDevice = devs[0];
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE);
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
	    return DITEM_FAILURE;
    }
    else
	mediaDevice = devs[0];
    if (mediaDevice) {
	char *val;

	val = msgGetInput("/var/tmp", "Please enter the name of a temporary directory containing\n"
			  "sufficient space for holding the contents of this tape (or\n"
			  "tapes).  The contents of this directory will be removed\n"
			  "after installation, so be sure to specify a directory that\n"
			  "can be erased afterwards!\n");
	if (!val)
	    mediaDevice = NULL;
	else
	    mediaDevice->private = strdup(val);
    }
    return (mediaDevice ? DITEM_LEAVE_MENU : DITEM_FAILURE);
}

/*
 * Return 0 if we successfully found and set the installation type to
 * be an ftp server
 */
int
mediaSetFTP(dialogMenuItem *self)
{
    static Device ftpDevice;
    char *cp, hbuf[MAXHOSTNAMELEN], *hostname, *dir;
    struct addrinfo hints, *res;
    int af;
    extern int FtpPort;
    static Device *networkDev = NULL;

    mediaClose();
    cp = variable_get(VAR_FTP_PATH);
    /* If we've been through here before ... */
    if (networkDev && cp && msgYesNo("Re-use old FTP site selection values?"))
	cp = NULL;
    if (!cp) {
	if (!dmenuOpenSimple(&MenuMediaFTP, FALSE))
	    return DITEM_FAILURE;
	else
	    cp = variable_get(VAR_FTP_PATH);
    }
    if (!cp)
	return DITEM_FAILURE;
    else if (!strcmp(cp, "other")) {
	variable_set2(VAR_FTP_PATH, "ftp://", 0);
	cp = variable_get_value(VAR_FTP_PATH, "Please specify the URL of a FreeBSD distribution on a\n"
				"remote ftp site.  This site must accept either anonymous\n"
				"ftp or you should have set an ftp username and password\n"
				"in the Options screen.\n\n"
				"A URL looks like this:  ftp://<hostname>/<path>\n"
				"Where <path> is relative to the anonymous ftp directory or the\n"
				"home directory of the user being logged in as.", 0);
	if (!cp || !*cp || !strcmp(cp, "ftp://")) {
	    variable_unset(VAR_FTP_PATH);
	    return DITEM_FAILURE;
	}
    }
    if (strncmp("ftp://", cp, 6)) {
	msgConfirm("Sorry, %s is an invalid URL!", cp);
	variable_unset(VAR_FTP_PATH);
	return DITEM_FAILURE;
    }
    SAFE_STRCPY(ftpDevice.name, cp);
    SAFE_STRCPY(hbuf, cp + 6);
    hostname = hbuf;

    if (!networkDev || msgYesNo("You've already done the network configuration once,\n"
				"would you like to skip over it now?") != 0) {
	if (networkDev)
	    DEVICE_SHUTDOWN(networkDev);
	if (!(networkDev = tcpDeviceSelect())) {
	    variable_unset(VAR_FTP_PATH);
	    return DITEM_FAILURE;
	}
    }
    if (!DEVICE_INIT(networkDev)) {
	if (isDebug())
	    msgDebug("mediaSetFTP: Net device init failed.\n");
	variable_unset(VAR_FTP_PATH);
	return DITEM_FAILURE;
    }
    if (*hostname == '[' && (cp = index(hostname + 1, ']')) != NULL &&
	(*++cp == '\0' || *cp == '/' || *cp == ':')) {
	++hostname;
	*(cp - 1) = '\0';
    }
    else
	cp = index(hostname, ':');
    if (cp != NULL && *cp == ':') {
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
    if (!ftp_skip_resolve && variable_get(VAR_NAMESERVER)) {
	msgNotify("Looking up host %s.", hostname);
    	if (isDebug())
	    msgDebug("Starting DNS.\n");
	kickstart_dns();
    	if (isDebug())
	    msgDebug("Looking up hostname, %s, using getaddrinfo(AI_NUMERICHOST).\n", hostname);
	af = variable_cmp(VAR_IPV6_ENABLE, "YES") ? AF_INET : AF_UNSPEC;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
	    if (isDebug())
		msgDebug("Looking up hostname, %s, using getaddrinfo().\n",
			 hostname);
	    hints.ai_flags = AI_PASSIVE;
	    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
		msgConfirm("Cannot resolve hostname `%s'!  Are you sure that"
			" your\nname server, gateway and network interface are"
			" correctly configured?", hostname);
		if (networkDev)
		    DEVICE_SHUTDOWN(networkDev);
		networkDev = NULL;
		variable_unset(VAR_FTP_PATH);
		return DITEM_FAILURE;
	    }
	}
	freeaddrinfo(res);
	if (isDebug())
	    msgDebug("Found DNS entry for %s successfully..\n", hostname);
    }
    variable_set2(VAR_FTP_HOST, hostname, 0);
    variable_set2(VAR_FTP_DIR, dir ? dir : "/", 0);
    variable_set2(VAR_FTP_PORT, itoa(FtpPort), 0);
    ftpDevice.type = DEVICE_TYPE_FTP;
    ftpDevice.init = mediaInitFTP;
    ftpDevice.get = mediaGetFTP;
    ftpDevice.shutdown = mediaShutdownFTP;
    ftpDevice.private = networkDev;
    mediaDevice = &ftpDevice;
    return DITEM_SUCCESS | DITEM_LEAVE_MENU | DITEM_RESTORE;
}

int
mediaSetFTPActive(dialogMenuItem *self)
{
    variable_set2(VAR_FTP_STATE, "active", 0);
    return mediaSetFTP(self);
}

int
mediaSetFTPPassive(dialogMenuItem *self)
{
    variable_set2(VAR_FTP_STATE, "passive", 0);
    return mediaSetFTP(self);
}

int mediaSetHTTP(dialogMenuItem *self)
{
    Boolean tmp;
    int result;
    char *cp, *idx, hbuf[MAXHOSTNAMELEN], *hostname;
    int HttpPort;
    int what = DITEM_RESTORE;


    tmp = ftp_skip_resolve;
    ftp_skip_resolve = TRUE;
    result = mediaSetFTP(self);
    ftp_skip_resolve = tmp;

    if (DITEM_STATUS(result) != DITEM_SUCCESS)
	return result;
 
    cp = variable_get_value(VAR_HTTP_PROXY,
	"Please enter the address of the HTTP proxy in this format:\n"
	" hostname:port (the ':port' is optional, default is 3128)",0);
    if (!cp)
	return DITEM_FAILURE;
    SAFE_STRCPY(hbuf, cp);
    hostname = hbuf;
    if (*hostname == '[' && (idx = index(hostname + 1, ']')) != NULL &&
	(*++idx == '\0' || *idx == ':')) {
	++hostname;
	*(idx - 1) = '\0';
    } else
	idx = index(hostname, ':');
    if (idx == NULL || *idx != ':')
	HttpPort = 3128;		/* try this as default */
    else {
	*(idx++) = '\0';
	HttpPort = strtol(idx, 0, 0);
    }

    variable_set2(VAR_HTTP_HOST, hostname, 0);
    variable_set2(VAR_HTTP_PORT, itoa(HttpPort), 0);
    if (isDebug()) {
      msgDebug("VAR_FTP_PATH : %s\n",variable_get(VAR_FTP_PATH));
      msgDebug("VAR_HTTP_HOST, _PORT: %s:%s\n",variable_get(VAR_HTTP_HOST),
                                             variable_get(VAR_HTTP_PORT));
    }

    /* mediaDevice has been set by mediaSetFTP(), overwrite partly: */
    mediaDevice->type = DEVICE_TYPE_HTTP;
    mediaDevice->init = mediaInitHTTP;
    mediaDevice->get = mediaGetHTTP;
    mediaDevice->shutdown = dummyShutdown;
    return DITEM_SUCCESS | DITEM_LEAVE_MENU | what;
}
   

int
mediaSetUFS(dialogMenuItem *self)
{
    static Device ufsDevice;
    struct statfs st;
    char *cp;

    mediaClose();
    cp = variable_get_value(VAR_UFS_PATH, "Enter a fully qualified pathname for the directory\n"
			    "containing the FreeBSD distribution files:", 0);
    if (!cp)
	return DITEM_FAILURE;

    /* If they gave us a CDROM or something, try and pick a better name */
    if (statfs(cp, &st))
	strcpy(ufsDevice.name, "ufs");
    else
	strcpy(ufsDevice.name, st.f_fstypename);

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
    char hostname[MAXPATHLEN];

    mediaClose();
    cp = variable_get_value(VAR_NFS_PATH, "Please enter the full NFS file specification for the remote\n"
			    "host and directory containing the FreeBSD distribution files.\n"
			    "This should be in the format:  hostname:/some/freebsd/dir", 0);
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
	    DEVICE_SHUTDOWN(networkDev);
	if (!(networkDev = tcpDeviceSelect()))
	    return DITEM_FAILURE;
    }
    if (!DEVICE_INIT(networkDev)) {
	if (isDebug())
	    msgDebug("mediaSetNFS: Net device init failed\n");
    }
    if (variable_get(VAR_NAMESERVER)) {
	kickstart_dns();
	if ((inet_addr(hostname) == INADDR_NONE) && (gethostbyname(hostname) == NULL)) {
	    msgConfirm("Cannot resolve hostname `%s'!  Are you sure that your\n"
		       "name server, gateway and network interface are correctly configured?", hostname);
	    if (networkDev)
		DEVICE_SHUTDOWN(networkDev);
	    networkDev = NULL;
	    variable_unset(VAR_NFS_PATH);
	    return DITEM_FAILURE;
	}
	else {
	    if (isDebug())
		msgDebug("Found DNS entry for %s successfully..\n", hostname);
	}
    }
    variable_set2(VAR_NFS_HOST, hostname, 0);
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
	char *unzipper = RunningAsInit ? "/stand/" UNZIPPER
	    : "/usr/bin/" UNZIPPER;

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
	i = execl(unzipper, unzipper, (char *)0);
	if (isDebug())
	    msgDebug("%s command returns %d status\n", unzipper, i);
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
	    i = execl(cpio, cpio, "-idum", cpioVerbosity(), "--block-size", mediaTapeBlocksize(), (char *)0);
	else
	    i = execl(cpio, cpio, "-idum", "--block-size", mediaTapeBlocksize(), (char *)0);
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
	    msgDebug("wait for %s returned status of %d!\n",
		USE_GZIP ? "gunzip" : "bunzip2", i);
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
	char *unzipper = RunningAsInit ? "/stand/" UNZIPPER
	    : "/usr/bin/" UNZIPPER;

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
	i = execl(unzipper, unzipper, (char *)0);
	if (isDebug())
	    msgDebug("%s command returns %d status\n", unzipper, i);
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
	    i = execl(cpio, cpio, "-idum", cpioVerbosity(), "--block-size", mediaTapeBlocksize(), (char *)0);
	else
	    i = execl(cpio, cpio, "-idum", "--block-size", mediaTapeBlocksize(), (char *)0);
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
    (void)sigemptyset(&new.sa_mask);
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
	    msgDebug("wait for %s returned status of %d!\n",
		USE_GZIP ? "gunzip" : "bunzip2", i);
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
    return ((dmenuOpenSimple(&MenuMedia, FALSE) && mediaDevice) ? DITEM_SUCCESS : DITEM_FAILURE);
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

    if (variable_get_value(VAR_FTP_USER, "Please enter the username you wish to login as:", 0)) {
	DialogInputAttrs |= DITEM_NO_ECHO;
	pass = variable_get_value(VAR_FTP_PASS, "Please enter the password for this user:", 0);
	DialogInputAttrs &= ~DITEM_NO_ECHO;
    }
    else
	pass = NULL;
    return (pass ? DITEM_SUCCESS : DITEM_FAILURE);
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
	    variable_set2(VAR_CPIO_VERBOSITY, "medium", 0);
	else if (!strcmp(cp, "medium"))
	    variable_set2(VAR_CPIO_VERBOSITY, "high", 0);
	else /* must be "high" - wrap around */
	    variable_set2(VAR_CPIO_VERBOSITY, "low", 0);
    }
    return DITEM_SUCCESS;
}

/* A generic open which follows a well-known "path" of places to look */
FILE *
mediaGenericGet(char *base, const char *file)
{
    char	buf[PATH_MAX];

    snprintf(buf, PATH_MAX, "%s/%s", base, file);
    if (file_readable(buf))
	return fopen(buf, "r");
    snprintf(buf, PATH_MAX, "%s/FreeBSD/%s", base, file);
    if (file_readable(buf))
	return fopen(buf, "r");
    snprintf(buf, PATH_MAX, "%s/releases/%s", base, file);
    if (file_readable(buf))
	return fopen(buf, "r");
    snprintf(buf, PATH_MAX, "%s/%s/%s", base, variable_get(VAR_RELNAME), file);
    if (file_readable(buf))
	return fopen(buf, "r");
    snprintf(buf, PATH_MAX, "%s/releases/%s/%s", base, variable_get(VAR_RELNAME), file);
    return fopen(buf, "r");
}

