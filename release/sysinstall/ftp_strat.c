/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: ftp_strat.c,v 1.7.2.45 1996/05/24 06:08:37 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 * Copyright (c) 1995
 * 	Gary J Palmer. All rights reserved.
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
#include "ftp.h"

Boolean ftpInitted = FALSE;
static FTP_t ftp;
extern int FtpPort;

static char *lastRequest;

static Boolean
get_new_host(Device *dev, Boolean probe)
{
    Boolean i;
    int j;
    char *oldTitle = MenuMediaFTP.title;
    char *cp = variable_get(VAR_FTP_ONERROR);

    if (probe || (cp && strcmp(cp, "reselect")))
	i = TRUE;
    else {
	i = FALSE;
	msgConfirm("The %s file failed to load from the FTP site you\n"
		   "selected.  Please select another one from the FTP menu.", lastRequest ? lastRequest : "requested");
	MenuMediaFTP.title = "Request failed - please select another site";
	j = mediaSetFTP(NULL);
	MenuMediaFTP.title = oldTitle;
	if (DITEM_STATUS(j) == DITEM_SUCCESS) {
	    /* Bounce the link if necessary */
	    if (ftpInitted) {
		msgDebug("Bouncing FTP connection before reselecting new host.\n");
		dev->shutdown(dev);
		i = dev->init(dev);
	    }
	}
	else {
	    msgDebug("User elected not to reselect, shutting down open connection.\n");
	    dev->shutdown(dev);
	}
    }
    return i;
}

/* Should we throw in the towel? */
static Boolean
ftpShouldAbort(Device *dev, int retries)
{
    char *cp, *cp2;
    int maxretries, rval = FALSE;

    cp = variable_get(VAR_FTP_ONERROR);
    cp2 = variable_get(VAR_FTP_RETRIES);
    maxretries = atoi(cp2);
    if (retries > maxretries || (cp && !strcmp(cp, "abort"))) {
	rval = TRUE;
	if (isDebug())
	    msgDebug("Aborting FTP connection.\n");
	dev->shutdown(dev);
	(void)dev->init(dev);
    }
    return rval;
}

Boolean
mediaInitFTP(Device *dev)
{
    int i, retries;
    char *cp, *rel, *hostname, *dir;
    char *user, *login_name, password[80];
    Device *netDevice = (Device *)dev->private;

    if (ftpInitted)
	return TRUE;

    if (isDebug())
	msgDebug("Init routine for FTP called.  Net device is %x\n", netDevice);
    if (!netDevice->init(netDevice)) {
	if (isDebug())
	    msgDebug("InitFTP: Net device init returns FALSE\n");
	return FALSE;
    }

    if (!ftp && (ftp = FtpInit()) == NULL) {
	msgConfirm("FTP initialisation failed!");
	return FALSE;
    }

    cp = variable_get(VAR_FTP_PATH);
    if (!cp) {
	msgConfirm("%s is not set!", VAR_FTP_PATH);
	return FALSE;
    }

    user = variable_get(VAR_FTP_USER);
    if (!user || !*user) {
	snprintf(password, BUFSIZ, "installer@%s", variable_get(VAR_HOSTNAME));
	login_name = "anonymous";
    }
    else {
	login_name = user;
	strcpy(password, variable_get(VAR_FTP_PASS) ? variable_get(VAR_FTP_PASS) : login_name);
    }
    retries = 0;
    hostname = variable_get(VAR_FTP_HOST);
    dir = variable_get(VAR_FTP_DIR);
    if (!hostname || !dir)
	msgFatal("Missing FTP host or directory specification - something's wrong!");

retry:
    msgNotify("Logging in as %s..", login_name);
    if (FtpOpen(ftp, hostname, login_name, password) != 0) {
	if (variable_get(VAR_NO_CONFIRM))
	    msgNotify("Couldn't open FTP connection to %s", hostname);
	else
	    msgConfirm("Couldn't open FTP connection to %s", hostname);
	if (ftpShouldAbort(dev, ++retries) || !get_new_host(dev, FALSE))
	    return FALSE;
	goto retry;
    }

    FtpPassive(ftp, !strcmp(variable_get(VAR_FTP_STATE), "passive"));
    FtpBinary(ftp, 1);
    if (dir && *dir != '\0') {
	msgDebug("Attempt to chdir to distribution in %s\n", dir);
	if ((i = FtpChdir(ftp, dir)) != 0) {
	    if (i == -2 || ftpShouldAbort(dev, ++retries))
		goto punt;
	    else if (get_new_host(dev, FALSE))
		retries = 0;
	    goto retry;
	}
    }

    /* Give it a shot - can't hurt to try and zoom in if we can, unless the release is set to __RELEASE which signifies that it's not set */
    rel = variable_get(VAR_RELNAME);
    if (strcmp(rel, "__RELEASE"))
	i = FtpChdir(ftp, rel);
    else
	i = 0;
    if (i == -2)
	goto punt;
    else if (i == -1)
	msgConfirm("Warning:  Can't CD to `%s' distribution on this\n"
		   "FTP server.  You may need to visit the Options menu\n"
		   "to set the release name explicitly if this FTP server\n"
		   "isn't exporting a CD (or some other custom release) at\n"
		   "the top level as a release tree.", rel);

    if (isDebug())
	msgDebug("mediaInitFTP was successful (logged in and chdir'd)\n");
    ftpInitted = TRUE;
    return TRUE;

punt:
    if (ftp != NULL) {
	FtpClose(ftp);
	ftp = NULL;
    }
    return FALSE;
}

int
mediaGetFTP(Device *dev, char *file, Boolean probe)
{
    int fd;
    int nretries;
    char *fp;
    char buf[PATH_MAX];

    fp = file;
    nretries = 0;

    lastRequest = file;
    while ((fd = FtpGet(ftp, fp)) < 0) {
	/* If a hard fail, try to "bounce" the ftp server to clear it */
	if (fd == -2 && ++nretries < atoi(variable_get(VAR_FTP_RETRIES))) {
	    dev->shutdown(dev);
	    /* If we can't re-initialize, just forget it */
	    if (!dev->init(dev))
		return -2;
	}
	else if (probe || ftpShouldAbort(dev, ++nretries))
	    return -1;
	else {
	    /* Try some alternatives */
	    switch (nretries) {
	    case 1:
		sprintf(buf, "dists/%s", file);
		fp = buf;
		break;

	    case 2:
		sprintf(buf, "%s/%s", variable_get(VAR_RELNAME), file);
		fp = buf;
		break;

	    case 3:
		sprintf(buf, "%s/dists/%s", variable_get(VAR_RELNAME), file);
		fp = buf;
		break;

	    case 4:
		fp = file;
		if (get_new_host(dev, probe)) {
		    nretries = 0;
		    continue;
		}
		else
		    break;
	    }
	}
    }
    return fd;
}

Boolean
mediaCloseFTP(Device *dev, int fd)
{
    if (isDebug())
	msgDebug("FTP Close called\n");
    if (ftp)
	FtpEOF(ftp);
    return FALSE;
}

void
mediaShutdownFTP(Device *dev)
{
    /* Device *netdev = (Device *)dev->private; */

    if (!ftpInitted)
	return;

    if (isDebug())
	msgDebug("FTP shutdown called.  FTP = %x\n", ftp);
    if (ftp != NULL) {
	FtpClose(ftp);
	ftp = NULL;
    }
    /* (*netdev->shutdown)(netdev); */
    ftpInitted = FALSE;
}
