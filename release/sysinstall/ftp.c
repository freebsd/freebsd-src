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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <netdb.h>
#include <pwd.h>
#include <ftpio.h>

Boolean ftpInitted = FALSE;
static FILE *OpenConn;
int FtpPort;

/* List of sub directories to look for under a given FTP server. */
const char *ftp_dirs[] = { ".", "releases/"MACHINE, "snapshots/"MACHINE,
    "pub/FreeBSD", "pub/FreeBSD/releases/"MACHINE,
    "pub/FreeBSD/snapshots/"MACHINE, NULL };

/* Brings up attached network device, if any - takes FTP device as arg */
static Boolean
netUp(Device *dev)
{
    Device *netdev = (Device *)dev->private;

    if (netdev)
	return DEVICE_INIT(netdev);
    else
	return TRUE;	/* No net == happy net */
}

/* Brings down attached network device, if any - takes FTP device as arg */
static void
netDown(Device *dev)
{
    Device *netdev = (Device *)dev->private;

    if (netdev)
	DEVICE_SHUTDOWN(netdev);
}

Boolean
mediaInitFTP(Device *dev)
{
    int i, code, af, fdir;
    char *cp, *rel, *hostname, *dir;
    char *user, *login_name, password[80];

    if (ftpInitted)
	return TRUE;

    if (OpenConn) {
	fclose(OpenConn);
	OpenConn = NULL;
    }

    /* If we can't initialize the network, bag it! */
    if (!netUp(dev))
	return FALSE;

try:
    cp = variable_get(VAR_FTP_PATH);
    if (!cp) {
	if (DITEM_STATUS(mediaSetFTP(NULL)) == DITEM_FAILURE || (cp = variable_get(VAR_FTP_PATH)) == NULL) {
	    msgConfirm("Unable to get proper FTP path.  FTP media not initialized.");
	    netDown(dev);
	    return FALSE;
	}
    }

    hostname = variable_get(VAR_FTP_HOST);
    dir = variable_get(VAR_FTP_DIR);
    if (!hostname || !dir) {
	msgConfirm("Missing FTP host or directory specification.  FTP media not initialized,");
	netDown(dev);
	return FALSE;
    }
    user = variable_get(VAR_FTP_USER);
    login_name = (!user || !*user) ? "anonymous" : user;

    if (variable_get(VAR_FTP_PASS))
	SAFE_STRCPY(password, variable_get(VAR_FTP_PASS));
    else if (RunningAsInit)
	sprintf(password, "installer@%s", variable_get(VAR_HOSTNAME));
    else {
	struct passwd *pw;
	char *user;

	pw = getpwuid(getuid());
	user = pw ? pw->pw_name : "ftp";
	sprintf(password, "%s@%s", user, variable_get(VAR_HOSTNAME));
    }
    af = variable_cmp(VAR_IPV6_ENABLE, "YES") ? AF_INET : AF_UNSPEC;
    msgNotify("Logging in to %s@%s..", login_name, hostname);
    if ((OpenConn = ftpLoginAf(hostname, af, login_name, password, FtpPort, isDebug(), &code)) == NULL) {
	msgConfirm("Couldn't open FTP connection to %s:\n  %s.", hostname, ftpErrString(code));
	goto punt;
    }

    ftpPassive(OpenConn, !strcmp(variable_get(VAR_FTP_STATE), "passive"));
    ftpBinary(OpenConn);
    if (dir && *dir != '\0') {
	if ((i = ftpChdir(OpenConn, dir)) != 0) {
	    if (i == 550)
		msgConfirm("No such directory ftp://%s/%s\n"
			   "please check your URL and try again.", hostname, dir);
	    else
		msgConfirm("FTP chdir to ftp://%s/%s returned error status:\n  %s.", hostname, dir, ftpErrString(i));
	    goto punt;
	}
    }

    /*
     * Now that we've verified that the path we're given is ok, let's try to
     * be a bit intelligent in locating the release we are looking for.  First
     * off, if the release is specified as "__RELEASE" or "any", then just
     * assume that the current directory is the one we want and give up.
     */
    rel = variable_get(VAR_RELNAME);
    if (strcmp(rel, "__RELEASE") && strcmp(rel, "any")) {
	/*
	 * Ok, since we have a release variable, let's walk through the list
	 * of directories looking for a release directory.  The first one to
	 * match wins.  For each case, we chdir to ftp_dirs[fdir] first.  If
	 * that fails, we skip to the next one.  Otherwise, we try to chdir to
	 * rel.  If it succeeds we break out.  If it fails, then we go back to
	 * the base directory and try again.  Lots of chdirs, but oh well. :)
	 */
	for (fdir = 0; ftp_dirs[fdir]; fdir++) {
	    if (ftpChdir(OpenConn, (char *)ftp_dirs[fdir]) != 0)
		continue;
	    if (ftpChdir(OpenConn, rel) == 0) {
		ftpInitted = TRUE;
		return TRUE;
	    }
	    else	/* reset to "root" dir for a fresh try */
		ftpChdir(OpenConn, "/");
	}

	/*
	 * If we get here, then all of the directories we tried failed, so
	 * print out the error message and ask the user if they want to try
	 * again.
	 */
	if (!msgYesNo("Warning:  Can't find the `%s' distribution on this\n"
		      "FTP server.  You may need to visit a different server for\n"
		      "the release you are trying to fetch or go to the Options\n"
		      "menu and to set the release name to explicitly match what's\n"
		      "available on %s (or set to \"any\").\n\n"
		      "Would you like to select another FTP server?",
		      rel, hostname)) {
	    variable_unset(VAR_FTP_PATH);
	    if (DITEM_STATUS(mediaSetFTP(NULL)) != DITEM_FAILURE)
		goto try;
	}
    } else {
	ftpInitted = TRUE;
	return TRUE;
    }

punt:
    ftpInitted = FALSE;
    if (OpenConn != NULL) {
	fclose(OpenConn);
	OpenConn = NULL;
    }
    netDown(dev);
    variable_unset(VAR_FTP_PATH);
    return FALSE;
}

FILE *
mediaGetFTP(Device *dev, char *file, Boolean probe)
{
    int nretries = 1;
    FILE *fp;
    char *try, buf[PATH_MAX];

    if (!OpenConn) {
	msgDebug("No FTP connection open, can't get file %s\n", file);
	return NULL;
    }

    try = file;
    while ((fp = ftpGet(OpenConn, try, 0)) == NULL) {
	int ftperr = ftpErrno(OpenConn);

	/* If a hard fail, try to "bounce" the ftp server to clear it */
	if (ftperr != 550) {
	    if (ftperr != 421)	/* Timeout? */
		variable_unset(VAR_FTP_PATH);
	    /* If we can't re-initialize, just forget it */
	    DEVICE_SHUTDOWN(dev);
	    if (!DEVICE_INIT(dev)) {
		netDown(dev);
		if (OpenConn) {
		    fclose(OpenConn);
		    OpenConn = NULL;
		}
		variable_unset(VAR_FTP_PATH);
		return NULL;
	    }
	}
	else if (probe)
	    return NULL;
	else {
	    /* Try some alternatives */
	    switch (nretries++) {
	    case 1:
		sprintf(buf, "releases/%s", file);
		try = buf;
		break;

	    case 2:
		sprintf(buf, "%s/%s", variable_get(VAR_RELNAME), file);
		try = buf;
		break;

	    case 3:
		sprintf(buf, "%s/releases/%s", variable_get(VAR_RELNAME), file);
		try = buf;
		break;

	    case 4:
		try = file;
		break;
	    }
	}
    }
    return fp;
}

void
mediaShutdownFTP(Device *dev)
{
    if (!ftpInitted)
	return;

    if (OpenConn != NULL) {
	fclose(OpenConn);
	OpenConn = NULL;
    }
    ftpInitted = FALSE;
}
