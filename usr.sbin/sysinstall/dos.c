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
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <grp.h>

#define MSDOSFS
#include <sys/mount.h>
#undef MSDOSFS

static Boolean DOSMounted;

Boolean
mediaInitDOS(Device *dev)
{
    struct msdosfs_args	args;

    if (DOSMounted)
	return TRUE;

    if (Mkdir("/dos", NULL))
	return FALSE;

    args.fspec = dev->devname;
    args.uid = args.gid = 0;

    if (mount(MOUNT_MSDOS, "/dos", MNT_RDONLY, (caddr_t)&args) == -1) {
	msgConfirm("Error mounting %s on /dos: %s (%u)\n", dev, strerror(errno), errno);
	return FALSE;
    }
    DOSMounted = TRUE;
    return TRUE;
}

int
mediaGetDOS(char *dist, char *path)
{
    char		buf[PATH_MAX];
    Attribs		*dist_attr = NULL;
    int			retval;

    dist_attr = safe_malloc(sizeof(Attribs) * MAX_ATTRIBS);
    snprintf(buf, PATH_MAX, "/stand/info/%s.inf", dist);
    if (!access(buf, R_OK) && attr_parse(&dist_attr, buf) == 0) {
	msgConfirm("Cannot load information file for %s distribution!\nPlease verify that your media is valid and try again.", dist);
	free(dist_attr);
	return FALSE;
    }
    snprintf(buf, PATH_MAX, "/dos/%s%s", path ? path : "", dist);
    retval = genericGetDist(buf, dist_attr, FALSE);
    free(dist_attr);
    return retval;
}

void
mediaShutdownDOS(Device *dev)
{
    extern int getDistpid;

    if (!DOSMounted)
	return;
    if (getDistpid) {
	int i, j;

	i = waitpid(getDistpid, &j, 0);
	if (i < 0 || WEXITSTATUS(j)) {
	    msgConfirm("Warning: Last extraction returned status code %d.", WEXITSTATUS(j));
	    getDistpid = 0;
	}
	getDistpid = 0;
    }
    msgDebug("Unmounting /dos\n");
    if (unmount("/dos", 0) != 0)
	msgConfirm("Could not unmount the DOS partition: %s\n", strerror(errno));
    msgDebug("Unmount returned\n");
    DOSMounted = FALSE;
    return;
}
