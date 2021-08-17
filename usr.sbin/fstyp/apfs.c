/*
 * Copyright (c) 2019 Conrad Meyer <cem@FreeBSD.org>.  All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fstyp.h"

/*
 * This really detects the container format, which might be best supported by
 * geom_part or a special GEOM class.
 *
 * https://developer.apple.com/support/downloads/Apple-File-System-Reference.pdf
 */

#define	NX_CKSUM_SZ		8

typedef uint64_t nx_oid_t;

typedef uint64_t nx_xid_t;

struct nx_obj {
	uint8_t		o_cksum[NX_CKSUM_SZ];	/* Fletcher 64 */
	nx_oid_t	o_oid;
	nx_xid_t	o_xid;
	uint32_t	o_type;
	uint32_t	o_subtype;
};

/* nx_obj::o_oid */
#define	OID_NX_SUPERBLOCK	1

/* nx_obj::o_type: */
#define	OBJECT_TYPE_MASK		0x0000ffff
#define	OBJECT_TYPE_NX_SUPERBLOCK	0x00000001
#define	OBJECT_TYPE_FLAGS_MASK		0xffff0000
#define	OBJ_STORAGETYPE_MASK		0xc0000000
#define	OBJECT_TYPE_FLAGS_DEFINED_MASK	0xf8000000
#define	OBJ_STORAGE_VIRTUAL		0x00000000
#define	OBJ_STORAGE_EPHEMERAL		0x80000000
#define	OBJ_STORAGE_PHYSICAL		0x40000000
#define	OBJ_NOHEADER			0x20000000
#define	OBJ_ENCRYPTED			0x10000000
#define	OBJ_NONPERSISTENT		0x08000000

struct nx_superblock {
	struct nx_obj	nx_o;
	char		nx_magic[4];
	/* ... other stuff that doesn't matter */
};

int
fstyp_apfs(FILE *fp, char *label, size_t size)
{
	struct nx_superblock *csb;
	int retval;

	retval = 1;
	csb = read_buf(fp, 0, sizeof(*csb));
	if (csb == NULL)
		goto fail;

	/* Ideally, checksum the SB here. */
	if (strncmp(csb->nx_magic, "NXSB", 4) != 0 ||
	    csb->nx_o.o_oid != OID_NX_SUPERBLOCK ||
	    (csb->nx_o.o_type & OBJECT_TYPE_MASK) != OBJECT_TYPE_NX_SUPERBLOCK)
		goto fail;

	retval = 0;

	/* No label support yet. */
	(void)size;
	(void)label;

fail:
	free(csb);
	return (retval);
}
