/*
 * Copyright (c) 2017 Kyle J. Kneitinger <kyle@kneit.in>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _LIBBE_IMPL_H
#define _LIBBE_IMPL_H

#include <libzfs.h>

#include "be.h"

struct libbe_handle {
	char root[BE_MAXPATHLEN];
	char rootfs[BE_MAXPATHLEN];
	char bootfs[BE_MAXPATHLEN];
	char *bootonce;
	size_t altroot_len;
	zpool_handle_t *active_phandle;
	libzfs_handle_t *lzh;
	be_error_t error;
	bool print_on_err;
};

struct libbe_deep_clone {
	libbe_handle_t *lbh;
	const char *bename;
	const char *snapname;
	int depth;
	int depth_limit;
};

struct libbe_dccb {
	libbe_handle_t *lbh;
	zfs_handle_t *zhp;
	nvlist_t *props;
};

typedef struct prop_data {
	nvlist_t *list;
	libbe_handle_t *lbh;
	bool single_object;	/* list will contain props directly */
	char *bootonce;
} prop_data_t;

int prop_list_builder_cb(zfs_handle_t *, void *);
int be_proplist_update(prop_data_t *);

char *be_mountpoint_augmented(libbe_handle_t *lbh, char *mountpoint);

/* Clobbers any previous errors */
int set_error(libbe_handle_t *, be_error_t);

#endif  /* _LIBBE_IMPL_H */
