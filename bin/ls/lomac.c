/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: lomac.c,v 1.5 2001/11/26 19:25:52 bfeldman Exp $
 */

/*
 * This file encapsulates ls's use of LOMAC's ioctl interface.  ls uses
 * this interface to determine the LOMAC attributes of files.
 */

#include <sys/cdefs.h>
 __FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/lomacio.h>

#include <err.h>
#include <fts.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "lomac.h"

#define LOMAC_DEVICE "/dev/lomac"

static int devlomac; 			/* file descriptor for LOMAC_DEVICE */
static struct lomac_fioctl2 ioctl_args;

/* lomac_start()
 *
 * in:     nothing
 * out:    nothing
 * return: nothing
 *
 * Makes `devlomac' a fd to LOMAC_DEVICE
 */

void 
lomac_start(void)
{
	if ((devlomac = open(LOMAC_DEVICE, O_RDWR)) == -1)
		err(1, "cannot open %s", LOMAC_DEVICE);
}

/* lomac_stop()
 *
 * in:     nothing
 * out:    nothing
 * return: nothing
 *
 * Closes `devlomac', the fd to LOMAC_DEVICE.
 */

void 
lomac_stop(void)
{
	if (close(devlomac) == -1)
		err(1, "cannot close %s", LOMAC_DEVICE);
}

/* get_lattr()
 *
 * in:     ent - FTSENT describing file whose LOMAC attributes we wish to know
 * out:    nothing
 * return: a string describing `ent's LOMAC attributes
 *
 * This function uses LOMAC's ioctl interface to determine the LOMAC
 * attributes of the file described by `ent'.  
 *
 * This function dynamically allocates memory for the attribute strings.
 * The caller is responsible for eventually deallocating these strings.
 */

char *
get_lattr(FTSENT *ent)
{
	char *lattr;

#ifdef NOT_NOW
	printf("p%d n%d\n", ent->fts_pathlen, ent->fts_namelen);
	printf("ftscycle %x\n", ent->fts_cycle);
	printf("ftsparent %x\n", ent->fts_parent);
	printf("ftslink %x\n", ent->fts_link);
	printf("ftsnumber %x\n", ent->fts_number);
	printf("ftslevel %x\n", ent->fts_level);
	if (ent->fts_pathlen > 0)
		printf("%x : %s\n", ent->fts_path, ent->fts_path);
	else
		printf("length 0 path\n");
	if (ent->fts_namelen > 0)
		printf("%x : %s\n", ent->fts_name, ent->fts_name);
	else
		printf("length 0 name\n");
#endif
	/*
	 * We use ent->fts_level to determine whether or not ent->fts_path
	 * is valid.  This is a hack, but the FTS code doesn't seem to
	 * NULL the first byte of fts_path or zero fts_pathlen when fts_path
	 * is invalid, so there didn't seem to be a better way of doing it.
	 */
	if (ent->fts_level > 0) {
		strncpy(ioctl_args.path, ent->fts_path, MAXPATHLEN - 1);
		strncat(ioctl_args.path, "/",
		    MAXPATHLEN - strlen(ioctl_args.path) - 1);
		strncat(ioctl_args.path, ent->fts_accpath,
		    MAXPATHLEN - strlen(ioctl_args.path) - 1);
	} else
		strncpy(ioctl_args.path, ent->fts_accpath, MAXPATHLEN - 1);
	if (ioctl(devlomac, LIOGETFLATTR, &ioctl_args) == -1)
		err(1, NULL);

	/* we use ioctl_args.path as scratch space to build lattr */
	if (ioctl_args.flags != 0)
		asprintf(&lattr, "%d.%x", ioctl_args.level, ioctl_args.flags);
	else
		asprintf(&lattr, "%d", ioctl_args.level);

	if (lattr == NULL)
		err(1, NULL);
	return (lattr);
}
