/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: package.c,v 1.9 1995/10/22 01:32:58 jkh Exp $
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
#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include "sysinstall.h"

static char *make_playpen(char *pen, size_t sz);

/* Like package_extract, but assumes current media device */
int
package_add(char *name)
{
    if (!mediaVerify())
	return RET_FAIL;
    return package_extract(mediaDevice, name);
}

/* Extract a package based on a namespec and a media device */
int
package_extract(Device *dev, char *name)
{
    char path[511];
    char pen[FILENAME_MAX];
    char *where;
    int i, fd, ret;

    /* Check to make sure it's not already there */
    if (!vsystem("pkg_info -e %s", name))
	return RET_SUCCESS;

    if (!dev->init(dev)) {
	msgConfirm("Unable to initialize media type for package add.");
	return RET_FAIL;
    }

    ret = RET_FAIL;
    sprintf(path, "packages/All/%s%s", name, strstr(name, ".tgz") ? "" : ".tgz");
    msgDebug("pkg_extract: Attempting to fetch %s\n", path);
    fd = dev->get(dev, path, TRUE);
    if (fd >= 0) {
	pid_t tpid;

	msgNotify("Fetching %s from %s", path, dev->name);
	pen[0] = '\0';
	if ((where = make_playpen(pen, 0)) != NULL) {
	    if (isDebug())
		msgDebug("Working in temporary directory %s, will return to %s\n", pen, where);
	    tpid = fork();
	    if (!tpid) {
		dup2(fd, 0);
		i = vsystem("tar %s-xzf -", !strcmp(variable_get(VAR_CPIO_VERBOSITY), "high") ? "-v " : "");
		if (isDebug())
		    msgDebug("tar command returns %d status\n", i);
		exit(i);
	    }
	    else {
		int pstat;
		
		tpid = waitpid(tpid, &pstat, 0);
		if (vsystem("(pwd; cat +CONTENTS) | pkg_add %s-S",
			    !strcmp(variable_get(VAR_CPIO_VERBOSITY), "high") ? "-v " : ""))
		    msgConfirm("An error occurred while trying to pkg_add %s.\n"
			       "Please check debugging screen for possible further details.", path);
		else
		    ret = RET_SUCCESS;
		close(fd);
	    }
	    if (chdir(where) == -1)
		msgFatal("Unable to get back to where I was before, Jojo! (That was: %s)", where);
	    vsystem("rm -rf %s", pen);
	    if (isDebug())
		msgDebug("Nuked pen: %s\n", pen);
	}
	else
	    msgConfirm("Unable to find a temporary location to unpack this stuff in.\n"
		       "You must simply not have enough space or you've configured your\n"
		       "system oddly.  Sorry!");
	dev->close(dev, fd);
	if (dev->type == DEVICE_TYPE_TAPE)
	    unlink(path);
    }
    else
	msgDebug("pkg_extract: get operation returned %d\n", fd);
    return ret;
}

static size_t
min_free(char *tmpdir)
{
    struct statfs buf;

    if (statfs(tmpdir, &buf) != 0) {
	msgDebug("Error in statfs, errno = %d\n", errno);
	return -1;
    }
    return buf.f_bavail * buf.f_bsize;
}

/* Find a good place to play. */
static char *
find_play_pen(char *pen, size_t sz)
{
    struct stat sb;

    if (pen[0] && stat(pen, &sb) != RET_FAIL && (min_free(pen) >= sz))
	return pen;
    else if (stat("/var/tmp", &sb) != RET_FAIL && min_free("/var/tmp") >= sz)
	strcpy(pen, "/var/tmp/instmp.XXXXXX");
    else if (stat("/tmp", &sb) != RET_FAIL && min_free("/tmp") >= sz)
	strcpy(pen, "/tmp/instmp.XXXXXX");
    else if ((stat("/usr/tmp", &sb) == RET_SUCCESS || mkdir("/usr/tmp", 01777) == RET_SUCCESS) &&
	     min_free("/usr/tmp") >= sz)
	strcpy(pen, "/usr/tmp/instmp.XXXXXX");
    else {
	msgConfirm("Can't find enough temporary space to extract the files, please try\n"
		   "This again after your system is up (you can run /stand/sysinstall\n"
		   "directly) and you've had a chance to point /var/tmp somewhere with\n"
		   "sufficient temporary space available.");
	return NULL;
    }
    return pen;
}

/*
 * Make a temporary directory to play in and chdir() to it, returning
 * pathname of previous working directory.
 */
static char *
make_playpen(char *pen, size_t sz)
{
    static char Previous[FILENAME_MAX];

    if (!find_play_pen(pen, sz))
	return NULL;

    if (!mktemp(pen)) {
	msgConfirm("Can't mktemp '%s'.", pen);
	return NULL;
    }
    if (mkdir(pen, 0755) == RET_FAIL) {
	msgConfirm("Can't mkdir '%s'.", pen);
	return NULL;
    }
    if (isDebug()) {
	if (sz)
	    msgDebug("Requested space: %d bytes, free space: %d bytes in %s\n", (int)sz, min_free(pen), pen);
    }
    if (min_free(pen) < sz) {
	rmdir(pen);
	msgConfirm("Not enough free space to create: `%s'\n"
		   "Please try this again after your system is up (you can run\n"
		   "/stand/sysinstall directly) and you've had a chance to point\n"
		   "/var/tmp somewhere with sufficient temporary space available.");
        return NULL;
    }
    if (!getcwd(Previous, FILENAME_MAX)) {
	msgConfirm("getcwd");
	return NULL;
    }
    if (chdir(pen) == RET_FAIL)
	msgConfirm("Can't chdir to '%s'.", pen);
    return Previous;
}
