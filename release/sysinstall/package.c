/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: package.c,v 1.35 1996/04/30 06:13:50 jkh Exp $
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include "sysinstall.h"

/* Like package_extract, but assumes current media device */
int
package_add(char *name)
{
    if (!mediaVerify())
	return DITEM_FAILURE;
    return package_extract(mediaDevice, name, FALSE);
}

Boolean
package_exists(char *name)
{
    int status = vsystem("pkg_info -e %s", name);

    msgDebug("package check for %s returns %s.\n", name,
	     status ? "failure" : "success");
    return !status;
}

/* Extract a package based on a namespec and a media device */
int
package_extract(Device *dev, char *name, Boolean depended)
{
    char path[511];
    int fd, ret;

    /* If necessary, initialize the ldconfig hints */
    if (!file_readable("/var/run/ld.so.hints"))
	vsystem("ldconfig /usr/lib /usr/local/lib /usr/X11R6/lib");

    /* Check to make sure it's not already there */
    msgNotify("Checking for existence of %s package", name);
    if (package_exists(name))
	return DITEM_SUCCESS;

    if (!dev->init(dev)) {
	msgConfirm("Unable to initialize media type for package extract.");
	return DITEM_FAILURE;
    }

    /* Be initially optimistic */
    ret = DITEM_SUCCESS | DITEM_RESTORE;
    /* Make a couple of paranoid locations for temp files to live if user specified none */
    if (!variable_get("PKG_TMPDIR")) {
	Mkdir("/usr/tmp", NULL);
	Mkdir("/var/tmp", NULL);
	/* Set it to a location with as much space as possible */
	variable_set2("PKG_TMPDIR", "/usr/tmp");
    }

    sprintf(path, "packages/All/%s%s", name, strstr(name, ".tgz") ? "" : ".tgz");
    fd = dev->get(dev, path, TRUE);
    if (fd >= 0) {
	int i, tot, pfd[2];
	pid_t pid;

	msgNotify("Adding %s%s\nfrom %s", path, depended ? " (as a dependency)" : "", dev->name);
	pipe(pfd);
	pid = fork();
	if (!pid) {
	    dup2(pfd[0], 0); close(pfd[0]);
	    dup2(DebugFD, 1);
	    close(2);
	    close(pfd[1]);
	    i = execl("/usr/sbin/pkg_add", "/usr/sbin/pkg_add", "-", 0);
	    if (isDebug())
		msgDebug("pkg_add returns %d status\n", i);
	}
	else {
	    char buf[BUFSIZ];
	    WINDOW *w = savescr();

	    close(pfd[0]);
	    tot = 0;
	    while ((i = read(fd, buf, BUFSIZ)) > 0) {
		char line[80];
		int x, len;

		write(pfd[1], buf, i);
		tot += i;
		sprintf(line, "%d bytes read from package %s", tot, name);
		len = strlen(line);
		for (x = len; x < 79; x++)
		    line[x] = ' ';
		line[79] = '\0';
		mvprintw(0, 0, line);
		clrtoeol();
		refresh();
	    }
	    close(pfd[1]);
	    dev->close(dev, fd);
	    mvprintw(0, 0, "Package %s read successfully - waiting for pkg_add", name);
	    refresh();
	    i = waitpid(pid, &tot, 0);
	    if (i < 0 || WEXITSTATUS(tot)) {
		msgNotify("Add of package %s aborted due to some error -\n"
			  "Please check the debug screen for more info.");
	    }
	    else
		msgNotify("Package %s was added successfully", name);
	    sleep(1);
	    restorescr(w);
	}
    }
    else {
	msgDebug("pkg_extract: get operation returned %d\n", fd);
	if (variable_get(VAR_NO_CONFIRM))
	    msgNotify("Unable to fetch package %s from selected media.\n"
		      "No package add will be done.", name);
	else {
	    msgConfirm("Unable to fetch package %s from selected media.\n"
		       "No package add will be done.", name);
	}
	ret = DITEM_FAILURE | DITEM_RESTORE;
    }
    return ret;
}
