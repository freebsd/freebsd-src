/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013  Peter Grehan <grehan@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 * $FreeBSD: head/usr.sbin/bhyve/block_if.c 356523 2020-01-08 22:55:22Z vmaffione $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/usr.sbin/bhyve/block_if.c 356523 2020-01-08 22:55:22Z vmaffione $");

#include <sys/param.h>
#include <errno.h>
#include <dlfcn.h>
#include <regex.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <sys/linker_set.h>

#include "block_if.h"
#include "debug.h"

SET_DECLARE(block_backend_set, struct block_backend);

struct blockif_ctxt *
blockif_open(char *optstr, const char *ident)
{
	struct block_backend **bbe = NULL;
	struct blockif_ctxt *ret = NULL;
	char *optrest;		 	/* if found, optstr without scheme: */
	char *backend_name;	 	
	regex_t	re;
	int res;

	backend_name = "file";
	optrest = strchr(optstr, ':');
	if (optrest != NULL) {
		*optrest = '\0';
		backend_name = optstr;
		optstr = ++optrest;
	}
	/* is optstr now a valid block driver string?? */
	res = regcomp (&re, "^[a-zA-Z0-9]$" , REG_EXTENDED);
	res = regexec (&re, backend_name, 0, NULL, 0);
	regfree (&re);
	if (res) {
		EPRINTLN("%s is not a valid block device descriptor", backend_name);	
		return (NULL);
	}

	/*
	 * Find the block device backend that matches the user-provided
	 * device name. block_backend_set is built using a linker set.
	 */
	SET_FOREACH(bbe, block_backend_set) {
		/*
		 * We match the backend_name against the names of the 
		 * backends until we have a match
		 */
		/*
		 * Local access has a pattern like:
		 *   3:0,virtio-blk,file/somewhere/guest.img
		 *   3:0,virtio-blk,/dev/xxxx
		 * Or new style:
		 *   3:0,virtio-blk,file:file/somewhere/guest.img
		 *   3:0,virtio-blk,file:/dev/xxxxx
		 * For local filesystem references in optstr shall exist.
		 * This is handled by the blk-local backend.
		 * If this does not match then other backends in the block_backend_set have
		 * their bb_open() called. The first one returning a non-NULL backend pointer 
		 * is a match and is used with the specification in optstr
		 */
		if (strcmp(backend_name, (*bbe)->bb_name) == 0) {
			ret = (*bbe)->bb_open(optrest, ident);
			/* fill in the backend that is used to open this request */
			if (ret != NULL) {
				ret->be = *bbe;
				return (ret);
			}
			break;
		}
	
	}
	/* 
	 * We did not find any matching block backend drivers to 'optstr'
	 * Can we dynamically load a driver?
	 */
	char dlpath[MAXPATHLEN] = "/usr/lib/libblock_";
	char dlbb[MAXPATHLEN] = "block_backend_"; 
	void *dlfd;

	strcat(dlpath, backend_name);
	strcat(dlpath, ".so");
	if (( dlfd = dlopen(dlpath, RTLD_NOW)) == NULL) {
		/* report error and exit */
		printf("dlopen error for %s: %s. \n", dlpath, dlerror());
		return (NULL);
	}
	strcat(dlbb, backend_name);
	struct block_backend *dynbb = (struct block_backend*)dlsym( dlfd, dlbb);
	if (dynbb == NULL) {
		/* report error and exit */
		printf("dlsym error for %s: %s. \n", dlbb, dlerror());
		return (NULL);
	}

	/* now execute the open of the new found backend */	
	// DATA_SET(block_backend_set, dynbb);
	ret = dynbb->bb_open(optrest, ident);
	/* fill in the backend that is used to open this request */
	if (ret != NULL) {
		ret->be = dynbb;
		return (ret);
	}
	return (NULL);
}

int
blockif_read(struct blockif_ctxt *bc, struct blockif_req *breq)
{
	return ((bc->be)->bb_read(bc, breq));
}

int
blockif_write(struct blockif_ctxt *bc, struct blockif_req *breq)
{
	return ((bc->be)->bb_write(bc, breq));
}

int
blockif_flush(struct blockif_ctxt *bc, struct blockif_req *breq)
{
	return ((bc->be)->bb_flush(bc, breq));
}

int
blockif_delete(struct blockif_ctxt *bc, struct blockif_req *breq)
{
	return ((bc->be)->bb_delete(bc, breq));
}

int
blockif_cancel(struct blockif_ctxt *bc, struct blockif_req *breq)
{
	return ((bc->be)->bb_cancel(bc, breq));
}

int
blockif_close(struct blockif_ctxt *bc)
{
	return ((bc->be)->bb_close(bc));
}

void
blockif_chs(struct blockif_ctxt *bc, uint16_t *c, uint8_t *h, uint8_t *s)
{
	(bc->be)->bb_chs(bc, c, h, s);
}

off_t
blockif_size(struct blockif_ctxt *bc)
{
	return ((bc->be)->bb_size(bc));
}

int
blockif_sectsz(struct blockif_ctxt *bc)
{
	return ((bc->be)->bb_sectsz(bc));
}

void
blockif_psectsz(struct blockif_ctxt *bc, int *size, int *off)
{
	(bc->be)->bb_psectsz(bc, size, off);
}

int
blockif_queuesz(struct blockif_ctxt *bc)
{
	return ((bc->be)->bb_queuesz(bc));
}

int
blockif_is_ro(struct blockif_ctxt *bc)
{
	return ((bc->be)->bb_is_ro(bc));
}

int
blockif_candelete(struct blockif_ctxt *bc)
{
	return ((bc->be)->bb_candelete(bc));
}
