/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media_strategy.c,v 1.28 1995/05/26 20:30:59 jkh Exp $
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

Boolean
mediaInitFTP(Device *dev)
{
    int i;
    char *url, *hostname, *dir;
    char *my_name, email[BUFSIZ];
    Device *netDevice = (Device *)dev->private;

    if (ftpInitted)
	return TRUE;

    if (netDevice->init)
	if (!(*netDevice->init)(netDevice))
	    return FALSE;

    if ((ftp = FtpInit()) == NULL) {
	msgConfirm("FTP initialisation failed!");
	return FALSE;
    }

    url = getenv("ftp");
    if (!url)
	return FALSE;
    my_name = getenv(VAR_HOSTNAME);
    if (strncmp("ftp://", url, 6) != NULL) {
	msgConfirm("Invalid URL (`%s') passed to FTP routines!\n(must start with `ftp://')", url);
	return FALSE;
    }

    msgDebug("Using URL `%s'\n", url);
    hostname = url + 6;
    if ((dir = index(hostname, '/')) != NULL)
	*(dir++) = '\0';
    strcpy(dev->name, hostname);
    msgDebug("hostname = `%s'\n", hostname);
    msgDebug("dir = `%s'\n", dir ? dir : "/");
    msgNotify("Looking up host %s..", hostname);
    if ((gethostbyname(hostname) == NULL) && (inet_addr(hostname) == INADDR_NONE)) {
	msgConfirm("Cannot resolve hostname `%s'!  Are you sure your name server\nand/or gateway values are set properly?", hostname);
	return FALSE;
    }

    snprintf(email, BUFSIZ, "installer@%s", my_name);
    msgDebug("Using fake e-mail `%s'\n", email);

    msgNotify("Logging in as anonymous.");
    if ((i = FtpOpen(ftp, hostname, "anonymous", email)) != 0) {
	msgConfirm("Couldn't open FTP connection to %s: %s (%u)\n", hostname, strerror(i), i);
	return FALSE;
    }

    if (getenv("ftpPassive"))
	FtpPassive(ftp, 1);
    FtpBinary(ftp, 1);
    if (dir && *dir != '\0') {
	msgNotify("CD to distribution in ~ftp/%s", dir);
	FtpChdir(ftp, dir);
    }
    msgDebug("leaving mediaInitFTP!\n");
    ftpInitted = TRUE;
    return TRUE;
}

static pid_t ftppid = 0;

int
mediaGetFTP(char *dist, char *path)
{
    int 	fd;
    char 	buf[512];
    int		pfd[2], numchunks;
    const char *tmp;
    Attribs	*dist_attr;

    if (!path)
	path = "";
    msgNotify("Attempting to retreive `%s' over FTP", dist);
    snprintf(buf, PATH_MAX, "/stand/info/%s%s.inf", path, dist);
    if (!access(buf, R_OK)) {
	msgDebug("Parsing attributes file for %s\n", dist);
	dist_attr = safe_malloc(sizeof(Attribs) * MAX_ATTRIBS);
	if (attr_parse(&dist_attr, buf) == 0) {
	    msgConfirm("Cannot load information file for %s distribution!\nPlease verify that your media is valid and try again.", dist);
	    return -1;
	}
   
	msgDebug("Looking for attribute `pieces'\n");
	tmp = attr_match(dist_attr, "pieces");
	numchunks = atoi(tmp);
    }
    else
	numchunks = 0;
    msgDebug("Attempting to extract distribution from %u files\n", numchunks ? numchunks : 1);

    /* Take the lack of an info file to mean we're a fully qualified name */
    if (!numchunks) {
	sprintf(buf, "%s%s", path, dist);
	return(FtpGet(ftp, buf));
    }
    else if (numchunks == 1) {
	snprintf(buf, 512, "%s%s.aa", path, dist);
	return(FtpGet(ftp, buf));
    }

    /* reap the previous child corpse - yuck! */
    if (ftppid) {
	int i, j;

	i = waitpid(ftppid, &j, 0);
	if (i < 0 || WEXITSTATUS(j)) {
	    msgConfirm("Previous FTP transaction returned status code %d - aborting\ntransfer.", WEXITSTATUS(j));
	    ftppid = 0;
	    return -1;
	}
	ftppid = 0;
    }
    pipe(pfd);
    ftppid = fork();
    if (!ftppid) {
	int		chunk;
	int		retval;

	dup2(pfd[1], 1); close(pfd[1]);
	close(pfd[0]);
	
	for (chunk = 0; chunk < numchunks; chunk++) {
	    char buffer[10240];
	    int n;
	    
	    snprintf(buf, 512, "%s%s.%c%c", path, dist, (chunk / 26) + 'a', (chunk % 26) + 'a');
	    fd = FtpGet(ftp, buf);

	    if (fd < 0)
	    {
		msgConfirm("FtpGet failed to retreive piece `%s' in the %s distribution!\nAborting the transfer", chunk, dist);
		exit(1);
	    }
	    
	    while ((n = read(fd, buffer, 10240))>0)
	    {
		retval = write(1, buffer, n);
		if (retval != n)
		{
		    msgConfirm("Write failure on transfer! (wrote %d bytes of %d bytes)", retval, n);
		    exit(1);
		}
		
	    }
	    /* Close all but the last, since the last will get closed by mediaCloseFTP */
	    if (chunk + 1 != numchunks)
		FtpEOF(ftp);
	    close(fd);
	}
	close(1);
	msgDebug("Extract of %s finished with success!!!\n", dist);
	exit(0);
    }
    close(pfd[1]);
    return(pfd[0]);
}

Boolean
mediaCloseFTP(Device *dev, int fd)
{
    FtpEOF(ftp);
    close(fd);
    return TRUE;
}

void
mediaShutdownFTP(Device *dev)
{
    Device *netdev = (Device *)dev->private;

    if (!ftpInitted)
	return;

    if (ftp != NULL) {
	FtpClose(ftp);
	ftp = NULL;
    }
    if (ftppid) {
	int i, j;

	i = waitpid(ftppid, &j, 0);
	if (i < 0 || WEXITSTATUS(j))
	    msgConfirm("Warning: Last FTP transaction returned status code %d.", WEXITSTATUS(j));
	ftppid = 0;
    }
    if (netdev->shutdown)
	(*netdev->shutdown)(netdev);
    ftpInitted = FALSE;
}

