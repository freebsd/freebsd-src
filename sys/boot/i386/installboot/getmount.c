/*	$NetBSD: getmount.c,v 1.3 1998/03/01 13:22:55 fvdl Exp $ */

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <ufs/ufs/ufsmount.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include "installboot.h"

static int tempmounted = 0;

/* make block device name from character device name */
static char *
getbdev(dev)
	char *dev;
{
	static char bdiskdev[MAXPATHLEN];

	if (strncmp(dev, "/dev/r", 6)) {
		warnx("bad device name %s", dev);
		return (0);
	}
	sprintf(bdiskdev, "/dev/%s", dev + 6);
	return (bdiskdev);
}

/*
 * create mountpoint and mount given block device there, return
 * mountpoint
 */
static char *
dotempmount(bdiskdev)
	char *bdiskdev;
{
	static char dir[] = "/tmp/installbootXXXXXX";
	struct ufs_args data;

	if (mktemp(dir) == NULL) {
		warnx("mktemp failed");
		return (0);
	}
	if (mkdir(dir, 0700)) {
		warn("could not create temporary dir %s", dir);
		return (0);
	}
	bzero(&data, sizeof(data));
	data.fspec = bdiskdev;

	/* this code if FFS only */
	if (mount("ufs", dir, 0, &data) == -1) {
		warn("mount %s->%s failed", bdiskdev, dir);
		rmdir(dir);
		return (0);
	}
	if (verbose)
		fprintf(stderr, "mounted %s at %s\n", bdiskdev, dir);
	tempmounted = 1;
	return (dir);
}

/*
 * Find out if given character device is already mounted. If not, mount it
 * temporarily.
 */
char           *
getmountpoint(diskdev)
	char *diskdev;
{
	char *bdiskdev;
	struct statfs *buf;
	int num, i;

	bdiskdev = getbdev(diskdev);
	if (bdiskdev == NULL)
		return (0);

	num = getmntinfo(&buf, MNT_WAIT);
	if (num == 0) {
		warn("getmntinfo");
		return (0);
	}
	for (i = 0; i < num; i++)
		if (strncmp(bdiskdev, buf[i].f_mntfromname, MNAMELEN) == 0) {
			int j;

			/* Device is mounted. If there are more devices mounted
			 at the same point, the fs could be hidden. Don't think
			 too much about layering order - simply refuse. */
			for (j = 0; j < num; j++)
				if ((i != j) && (strncmp(buf[i].f_mntonname,
							 buf[j].f_mntonname,
							 MNAMELEN) == 0)) {
					warnx("there is more than 1 mount at %s",
					      buf[i].f_mntonname);
					return (0);
				}
			/* this code is FFS only */
			if (strncmp(buf[i].f_fstypename, "ufs", MFSNAMELEN)) {
				warnx("%s: must be a FFS filesystem", bdiskdev);
				return (0);
			}
			return (buf[i].f_mntonname);
		}
	if (verbose)
		fprintf(stderr, "%s is not mounted\n", bdiskdev);
	return (dotempmount(bdiskdev));
}

void 
cleanupmount(dir)
	char *dir;
{
	if (tempmounted) {
		if (verbose)
			fprintf(stderr, "unmounting\n");
		unmount(dir, 0);
		rmdir(dir);
		tempmounted = 0;
	}
}
