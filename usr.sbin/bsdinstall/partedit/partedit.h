/*-
 * Copyright (c) 2011 Nathan Whitehorn
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
 * $FreeBSD$
 */

#ifndef _PARTEDIT_PARTEDIT_H
#define _PARTEDIT_PARTEDIT_H

#include <sys/queue.h>
#include <inttypes.h>
#include <fstab.h>

struct gprovider;
struct gmesh;
struct ggeom;

TAILQ_HEAD(pmetadata_head, partition_metadata);
extern struct pmetadata_head part_metadata;

struct partition_metadata {
	char *name;		/* name of this partition, as in GEOM */
	
	struct fstab *fstab;	/* fstab data for this partition */
	char *newfs;		/* shell command to initialize partition */
	
	int bootcode;

	TAILQ_ENTRY(partition_metadata) metadata;
};

struct partition_metadata *get_part_metadata(const char *name, int create);
void delete_part_metadata(const char *name);

int part_wizard(void);

/* gpart operations */
void gpart_delete(struct gprovider *pp);
void gpart_destroy(struct ggeom *lg_geom, int force);
void gpart_edit(struct gprovider *pp);
void gpart_create(struct gprovider *pp, char *default_type, char *default_size,
    char *default_mountpoint, char **output, int interactive);
intmax_t gpart_max_free(struct ggeom *gp, intmax_t *start);
void gpart_revert(struct gprovider *pp);
void gpart_revert_all(struct gmesh *mesh);
void gpart_commit(struct gmesh *mesh);
int gpart_partition(const char *lg_name, const char *scheme);
void set_default_part_metadata(const char *name, const char *scheme,
    const char *type, const char *mountpoint, int newfs);

/* machine-dependent bootability checks */
const char *default_scheme(void);
int is_scheme_bootable(const char *part_type);
size_t bootpart_size(const char *part_type);
const char *bootcode_path(const char *part_type);
const char *partcode_path(const char *part_type);

#endif
