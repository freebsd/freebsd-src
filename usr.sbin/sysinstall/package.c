/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: package.c,v 1.27 1995/11/12 20:47:15 jkh Exp $
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
    int fd, ret;

    /* If necessary, initialize the ldconfig hints */
    if (!file_readable("/var/run/ld.so.hints"))
	vsystem("ldconfig /usr/lib /usr/local/lib /usr/X11R6/lib");

    msgNotify("Checking for existence of %s package", name);
    /* Check to make sure it's not already there */
    if (!vsystem("pkg_info -e %s", name)) {
	msgDebug("package %s marked as already installed - return SUCCESS.\n", name);
	return RET_SUCCESS;
    }

    if (!dev->init(dev)) {
	dialog_clear();
	msgConfirm("Unable to initialize media type for package extract.");
	return RET_FAIL;
    }

    ret = RET_FAIL;
    /* Make a couple of paranoid locations for temp files to live if user specified none */
    if (!variable_get("PKG_TMPDIR")) {
	Mkdir("/usr/tmp", NULL);
	Mkdir("/var/tmp", NULL);
    }

    sprintf(path, "packages/All/%s%s", name, strstr(name, ".tgz") ? "" : ".tgz");
    msgNotify("Adding %s\nfrom %s", path, dev->name);
    fd = dev->get(dev, path, TRUE);
    if (fd >= 0) {
	pen[0] = '\0';
	if ((where = make_playpen(pen, 0)) != NULL) {
	    if (mediaExtractDist(pen, fd)) {
		if (file_readable("+CONTENTS")) {
		    /* Set some hints for pkg_add so that it knows how we got here in case of any depends */
		    switch (mediaDevice->type) {
		    case DEVICE_TYPE_FTP:
			if (variable_get(VAR_FTP_PATH)) {
			    char ftppath[512];
			    
			    /* Special case to leave hint for pkg_add that this is an FTP install */
			    sprintf(ftppath, "%spackages/All/", variable_get(VAR_FTP_PATH));
			    variable_set2("PKG_ADD_BASE", ftppath);
			}
			break;

		    case DEVICE_TYPE_DOS:
			variable_set2("PKG_PATH", "/dos/freebsd/packages/All:/dos/packages/All");
			break;

		    case DEVICE_TYPE_CDROM:
			variable_set2("PKG_PATH", "/cdrom/packages/All:/cdrom/usr/ports/packages/All");
			break;

		    default:
			variable_set2("PKG_PATH", "/dist/packages/All:/dist/freebsd/packages/All");
			break;
		    }

		    if (vsystem("(pwd; cat +CONTENTS) | pkg_add %s-S",
				!strcmp(variable_get(VAR_CPIO_VERBOSITY), "high") ? "-v " : "")) {
			dialog_clear();
			if (!variable_get(VAR_NO_CONFIRM))
			    msgConfirm("An error occurred while trying to pkg_add %s.\n"
				       "Please check debugging screen for possible further details.", name);
			else
			    msgNotify("An error occurred while trying to pkg_add %s.", name);
		    }
		    else {
			msgNotify("Package %s added successfully!", name);
			ret = RET_SUCCESS;
		    }
		}
		else {
		    dialog_clear();
		    if (!variable_get(VAR_NO_CONFIRM))
			msgConfirm("The package specified (%s) has no CONTENTS file.  This means\n"
				   "that there was either a media error of some sort or the package\n"
				   "file itself is corrupted.  It is also possible that you simply\n"
				   "ran out of temporary space and need to go to the options editor\n"
				   "to select a package temp directory with more space.  Either way,\n"
				   "you may wish to look into the problem and try again.", name);
		    else
			msgNotify("The package specified (%s) has no CONTENTS file.  Skipping.", name);
		}
	    }
	    else {
		ret = RET_FAIL;
		if (!variable_get(VAR_NO_CONFIRM))
		    msgConfirm("Unable to extract the contents of package %s.  This means\n"
			       "that there was either a media error of some sort or the package\n"
			       "file itself is corrupted.\n"
			       "You may wish to look into this and try again.", name);
		else
		    msgNotify("Unable to extract the contents of package %s.  Skipping.", name);
	    }
	    if (chdir(where) == -1)
		msgFatal("Unable to get back to where I was before, Jojo! (That was: %s)", where);
	    vsystem("rm -rf %s", pen);
	    if (isDebug())
		msgDebug("Nuked pen: %s\n", pen);
	}
	else {
	    dialog_clear();
	    msgConfirm("Unable to find a temporary location to unpack this stuff in.\n"
		       "You must simply not have enough space or you've configured your\n"
		       "system oddly.  Sorry!");
	    ret = RET_FAIL;
	}
	dev->close(dev, fd);
	if (dev->type == DEVICE_TYPE_TAPE)
	    unlink(path);
    }
    else {
	msgDebug("pkg_extract: get operation returned %d\n", fd);
	if (variable_get(VAR_NO_CONFIRM))
	    msgNotify("Unable to fetch package %s from selected media.\n"
		      "No package add will be done.", name);
	else {
	    dialog_clear();
	    msgConfirm("Unable to fetch package %s from selected media.\n"
		       "No package add will be done.", name);
	}
    }
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
	dialog_clear();
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
	dialog_clear();
	msgConfirm("Can't mktemp '%s'.", pen);
	return NULL;
    }
    if (mkdir(pen, 0755) == RET_FAIL) {
	dialog_clear();
	msgConfirm("Can't mkdir '%s'.", pen);
	return NULL;
    }
    if (isDebug()) {
	if (sz)
	    msgDebug("Requested space: %d bytes, free space: %d bytes in %s\n", (int)sz, min_free(pen), pen);
    }
    if (min_free(pen) < sz) {
	rmdir(pen);
	dialog_clear();
	msgConfirm("Not enough free space to create: `%s'\n"
		   "Please try this again after your system is up (you can run\n"
		   "/stand/sysinstall directly) and you've had a chance to point\n"
		   "/var/tmp somewhere with sufficient temporary space available.");
        return NULL;
    }
    if (!getcwd(Previous, FILENAME_MAX)) {
	dialog_clear();
	msgConfirm("getcwd");
	return NULL;
    }
    if (chdir(pen) == RET_FAIL) {
	dialog_clear();
	msgConfirm("Can't chdir to '%s'.", pen);
    }
    return Previous;
}
