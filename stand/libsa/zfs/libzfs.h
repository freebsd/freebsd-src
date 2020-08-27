/*-
 * Copyright (c) 2012 Andriy Gapon <avg@FreeBSD.org>
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

#include <zfsimpl.h>

#ifdef LOADER_GELI_SUPPORT
#include <crypto/intake.h>
#endif

#ifndef _BOOT_LIBZFS_H_
#define _BOOT_LIBZFS_H_

#define	ZFS_MAXNAMELEN	256

/*
 * ZFS fully-qualified device descriptor.
 */
struct zfs_devdesc {
	struct devdesc	dd;		/* Must be first. */
	uint64_t	pool_guid;
	uint64_t	root_guid;
};

/* nvp implementation version */
#define	NV_VERSION		0

/* nvlist persistent unique name flags, stored in nvl_nvflags */
#define	NV_UNIQUE_NAME		0x1
#define	NV_UNIQUE_NAME_TYPE	0x2

#define	NV_ALIGN4(x)		(((x) + 3) & ~3)

/*
 * nvlist header.
 * nvlist has 4 bytes header followed by version and flags, then nvpairs
 * and the list is terminated by double zero.
 */
typedef struct {
	char nvh_encoding;
	char nvh_endian;
	char nvh_reserved1;
	char nvh_reserved2;
} nvs_header_t;

typedef struct {
	nvs_header_t nv_header;
	size_t nv_asize;
	size_t nv_size;
	uint8_t *nv_data;
	uint8_t *nv_idx;
} nvlist_t;

/*
 * nvpair header.
 * nvpair has encoded and decoded size
 * name string (size and data)
 * data type and number of elements
 * data
 */
typedef struct {
	unsigned encoded_size;
	unsigned decoded_size;
} nvp_header_t;

/*
 * nvlist stream head.
 */
typedef struct {
	unsigned nvl_version;
	unsigned nvl_nvflag;
	nvp_header_t nvl_pair;
} nvs_data_t;

typedef struct {
	unsigned nv_size;
	uint8_t nv_data[];	/* NV_ALIGN4(string) */
} nv_string_t;

typedef struct {
	unsigned nv_type;	/* data_type_t */
	unsigned nv_nelem;	/* number of elements */
	uint8_t nv_data[];	/* data stream */
} nv_pair_data_t;

nvlist_t *nvlist_create(int);
void nvlist_destroy(nvlist_t *);
nvlist_t *nvlist_import(const uint8_t *, char, char);
int nvlist_remove(nvlist_t *, const char *, data_type_t);
void nvlist_print(nvlist_t *, unsigned int);
int nvlist_find(const nvlist_t *, const char *, data_type_t,
    int *, void *, int *);
int nvlist_next(nvlist_t *);

int	zfs_parsedev(struct zfs_devdesc *dev, const char *devspec,
		     const char **path);
char	*zfs_fmtdev(void *vdev);
int	zfs_nextboot(void *vdev, char *buf, size_t size);
int	zfs_probe_dev(const char *devname, uint64_t *pool_guid);
int	zfs_list(const char *name);
uint64_t ldi_get_size(void *);
void	init_zfs_boot_options(const char *currdev);
int	zfs_bootenv(const char *name);
int	zfs_belist_add(const char *name, uint64_t __unused);
int	zfs_set_env(void);

extern struct devsw zfs_dev;
extern struct fs_ops zfs_fsops;

#endif /*_BOOT_LIBZFS_H_*/
