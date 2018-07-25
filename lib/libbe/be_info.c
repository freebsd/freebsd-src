/*
 * be_info.c
 *
 * Copyright (c) 2017 Kyle J. Kneitinger <kyle@kneit.in>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "be.h"
#include "be_impl.h"

typedef struct prop_data {
	nvlist_t *list;
	libbe_handle_t *lbh;
} prop_data_t;

static int prop_list_builder_cb(zfs_handle_t *, void *);
static int prop_list_builder(prop_data_t *);

/*
 * Returns the name of the active boot environment
 */
const char *
be_active_name(libbe_handle_t *lbh)
{

	return (strrchr(lbh->rootfs, '/') + sizeof(char));
}


/*
 * Returns full path of the active boot environment
 */
const char *
be_active_path(libbe_handle_t *lbh)
{

	return (lbh->rootfs);
}


/*
 * Returns the path of the boot environment root dataset
 */
const char *
be_root_path(libbe_handle_t *lbh)
{

	return (lbh->root);
}


/*
 * Returns an nvlist of the bootenv's properties
 * TODO: the nvlist should be passed as a param and ints should return status
 */
nvlist_t *
be_get_bootenv_props(libbe_handle_t *lbh)
{
	prop_data_t data;

	data.lbh = lbh;
	prop_list_builder(&data);

	return (data.list);
}


/*
 * Internal callback function used by zfs_iter_filesystems. For each dataset in
 * the bootenv root, populate an nvlist_t of its relevant properties.
 * TODO: should any other properties be included?
 */
static int
prop_list_builder_cb(zfs_handle_t *zfs_hdl, void *data_p)
{
	char buf[512];
	prop_data_t *data;
	libbe_handle_t *lbh;
	nvlist_t *props;
	const char *dataset, *name;
	boolean_t mounted, active, nextboot;

	/*
	 * XXX TODO:
	 *      some system for defining constants for the nvlist keys
	 *      error checking
	 */
	data = (prop_data_t *)data_p;
	lbh = data->lbh;

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);

	dataset = zfs_get_name(zfs_hdl);
	nvlist_add_string(props, "dataset", dataset);

	name = strrchr(dataset, '/') + 1;
	nvlist_add_string(props, "name", name);

	mounted = zfs_prop_get_int(zfs_hdl, ZFS_PROP_MOUNTED);
	nvlist_add_boolean_value(props, "mounted", mounted);

	/* XXX TODO: NOT CORRECT! Must use is_mounted */
	if (mounted) {
		zfs_prop_get(zfs_hdl, ZFS_PROP_MOUNTPOINT, buf, 512,
		    NULL, NULL, 0, 1);
		nvlist_add_string(props, "mountpoint", buf);
	}

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_ORIGIN, buf, 512,
	    NULL, NULL, 0, 1))
		nvlist_add_string(props, "origin", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_CREATION, buf, 512,
	    NULL, NULL, 0, 1))
		nvlist_add_string(props, "creation", buf);

	nvlist_add_boolean_value(props, "active",
	    (strcmp(be_active_path(lbh), dataset) == 0));

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_USED, buf, 512,
	    NULL, NULL, 0, 1))
		nvlist_add_string(props, "used", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_USEDDS, buf, 512,
	    NULL, NULL, 0, 1))
		nvlist_add_string(props, "usedds", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_USEDSNAP, buf, 512,
	    NULL, NULL, 0, 1))
		nvlist_add_string(props, "usedsnap", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_USEDREFRESERV, buf, 512,
	    NULL, NULL, 0, 1))
		nvlist_add_string(props, "usedrefreserv", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_REFERENCED, buf, 512,
	    NULL, NULL, 0, 1))
		nvlist_add_string(props, "referenced", buf);

	/* XXX TODO: Add bootfs info */

	nvlist_add_nvlist(data->list, name, props);

	return (0);
}


/*
 * Updates the properties of each bootenv in the libbe handle
 * XXX TODO: rename to be_proplist_update
 * XXX TODO: ensure that this is always consistent (run after adds, deletes,
 *       renames,etc
 */
static int
prop_list_builder(prop_data_t *data)
{
	zfs_handle_t *root_hdl;

	if (nvlist_alloc(&(data->list), NV_UNIQUE_NAME, KM_SLEEP) != 0)
		/* XXX TODO: actually handle error */
		return (1);

	if ((root_hdl = zfs_open(data->lbh->lzh, data->lbh->root,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (BE_ERR_ZFSOPEN);

	/* XXX TODO: some error checking here */
	zfs_iter_filesystems(root_hdl, prop_list_builder_cb, data);

	zfs_close(root_hdl);

	return (0);
}


/*
 * frees property list and its children
 */
void
be_prop_list_free(nvlist_t *be_list)
{
	nvlist_t *prop_list;
	nvpair_t *be_pair;

	be_pair = nvlist_next_nvpair(be_list, NULL);
	if (nvpair_value_nvlist(be_pair, &prop_list) == 0)
		nvlist_free(prop_list);

	while ((be_pair = nvlist_next_nvpair(be_list, be_pair)) != NULL) {
		if (nvpair_value_nvlist(be_pair, &prop_list) == 0)
			nvlist_free(prop_list);
	}
}


/*
 * Usage
 */
bool
be_exists(libbe_handle_t *lbh, char *be)
{
	char buf[BE_MAXPATHLEN];

	be_root_concat(lbh, be, buf);

	/*
	 * XXX TODO: check mountpoint prop and see if its /, AND that result
	 * with below expression.
	 */
	return (zfs_dataset_exists(lbh->lzh, buf, ZFS_TYPE_DATASET));
}
