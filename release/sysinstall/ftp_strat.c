/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: ftp_strat.c,v 1.7.2.11 1995/10/16 23:02:18 jkh Exp $
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <netdb.h>
#include "ftp.h"

Boolean ftpInitted;
static FTP_t ftp;
extern int FtpPort;

static Boolean
get_new_host(Device *dev)
{
    Boolean i;
    char *oldTitle = MenuMediaFTP.title;

    if (dev->flags & OPT_EXPLORATORY_GET)
	return FALSE;

    MenuMediaFTP.title = "Request failed - please select another site";
    i = mediaSetFTP(NULL);
    MenuMediaFTP.title = oldTitle;
    if (i == RET_SUCCESS) {
	char *cp = variable_get(FTP_USER);

	if (cp && *cp)
	    (void)mediaSetFtpUserPass(NULL);
	/* Bounce the link */
	dev->shutdown(dev);
	i = dev->init(dev);
    }
    else
	dev->shutdown(dev);
    return i;
}

Boolean
mediaInitFTP(Device *dev)
{
    int i, retries, max_retries = MAX_FTP_RETRIES;
    char *cp, *hostname, *dir;
    char *user, *login_name, password[80], url[BUFSIZ];
    Device *netDevice = (Device *)dev->private;

    if (ftpInitted)
	return TRUE;

    if (netDevice && !netDevice->init(netDevice))
	return FALSE;

    if ((ftp = FtpInit()) == NULL) {
	msgConfirm("FTP initialisation failed!");
	goto punt;
    }
    if (isDebug())
	msgDebug("Initialized FTP library.\n");

    cp = variable_get("ftp");
    if (!cp)
	goto punt;
    if (isDebug())
	msgDebug("Attempting to open connection for: %s\n", cp);
    hostname = variable_get(VAR_HOSTNAME);
    if (strncmp("ftp://", cp, 6) != NULL) {
	msgConfirm("Invalid URL: %s\n(A URL must start with `ftp://' here)", cp);
	goto punt;
    }
    strncpy(url, cp, BUFSIZ);
    if (isDebug())
	msgDebug("Using URL `%s'\n", url);
    hostname = url + 6;
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
		   "name server, gateway and network interface are configured?", hostname);
	goto punt;
    }
    user = variable_get(FTP_USER);
    if (!user || !*user) {
	snprintf(password, BUFSIZ, "installer@%s", variable_get(VAR_HOSTNAME));
	login_name = "anonymous";
    }
    else {
	login_name = user;
	strcpy(password, variable_get(FTP_PASS) ? variable_get(FTP_PASS) : login_name);
    }
    retries = i = 0;
retry:
    msgNotify("Logging in as %s..", login_name);
    if ((i = FtpOpen(ftp, hostname, login_name, password)) != 0) {
	if (optionIsSet(OPT_NO_CONFIRM))
	    msgNotify("Couldn't open FTP connection to %s\n", hostname);
	else
	    msgConfirm("Couldn't open FTP connection to %s\n", hostname);
	if (++retries > max_retries) {
	    if (optionIsSet(OPT_FTP_ABORT) || !get_new_host(dev))
		return FALSE;
	    retries = 0;
	}
	goto retry;
    }

    FtpPassive(ftp, optionIsSet(OPT_FTP_PASSIVE) ? 1 : 0);
    FtpBinary(ftp, 1);
    if (dir && *dir != '\0') {
	msgNotify("CD to distribution in ~ftp/%s", dir);
	if ((i = FtpChdir(ftp, dir)) != 0) {
	    if (++retries > max_retries) {
		if (optionIsSet(OPT_FTP_ABORT)) {
		    FtpClose(ftp);
		    ftp = NULL;
		    return FALSE;
		}
		else if (!get_new_host(dev))
		    return FALSE;
		else
		    retries = 0;
	    }
	    goto retry;
	}
    }

    if (isDebug())
	msgDebug("leaving mediaInitFTP!\n");
    ftpInitted = TRUE;
    return TRUE;

punt:
    FtpClose(ftp);
    ftp = NULL;
    /* We used to shut down network here - not anymore */
    return FALSE;
}

int
mediaGetFTP(Device *dev, char *file, Attribs *dist_attrs)
{
    int fd;
    int nretries = 0, max_retries = MAX_FTP_RETRIES;
    char *fp;
    char buf[PATH_MAX];

    fp = file;
    while ((fd = FtpGet(ftp, fp)) < 0) {
	/* If a hard fail, try to "bounce" the ftp server to clear it */
	if (fd == -2)
	    return -2;
	else if (++nretries > max_retries) {
	    if (optionIsSet(OPT_FTP_ABORT) || !get_new_host(dev))
		return -1;
	    nretries = 0;
	    fp = file;
	}
	else {
	    /* Try some bogus alternatives */
	    if (nretries == 1)
		sprintf(buf, "dists/%s", file);
	    else if (nretries == 2)
		sprintf(buf, "%s/%s", variable_get(RELNAME), file);
	    else if (nretries == 3)
		sprintf(buf, "%s/dists/%s", variable_get(RELNAME), file);
	    else
		sprintf(buf, file);
	    fp = buf;
	}
    }
    return fd;
}

Boolean
mediaCloseFTP(Device *dev, int fd)
{
    FtpEOF(ftp);
    if (!close(fd))
	return (TRUE);
    return FALSE;
}

void
mediaShutdownFTP(Device *dev)
{
    /* Device *netdev = (Device *)dev->private; */

    if (!ftpInitted)
	return;

    if (ftp != NULL) {
	FtpClose(ftp);
	ftp = NULL;
    }
    /* (*netdev->shutdown)(netdev); */
    ftpInitted = FALSE;
}
