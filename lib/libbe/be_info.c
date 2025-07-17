/*
 * Copyright (c) 2017 Kyle J. Kneitinger <kyle@kneit.in>
 * Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
#include <sys/zfs_context.h>
#include <libzfsbootenv.h>

#include "be.h"
#include "be_impl.h"

static int snapshot_proplist_update(zfs_handle_t *hdl, prop_data_t *data);

/*
 * Returns the name of the active boot environment
 */
const char *
be_active_name(libbe_handle_t *lbh)
{

	if (*lbh->rootfs != '\0')
		return (strrchr(lbh->rootfs, '/') + sizeof(char));
	else
		return (lbh->rootfs);
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
 * Returns the name of the next active boot environment
 */
const char *
be_nextboot_name(libbe_handle_t *lbh)
{

	if (*lbh->bootfs != '\0')
		return (strrchr(lbh->bootfs, '/') + sizeof(char));
	else
		return (lbh->bootfs);
}


/*
 * Returns full path of the active boot environment
 */
const char *
be_nextboot_path(libbe_handle_t *lbh)
{

	return (lbh->bootfs);
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
 * Populates dsnvl with one nvlist per bootenv dataset describing the properties
 * of that dataset that we've declared ourselves to care about.
 */
int
be_get_bootenv_props(libbe_handle_t *lbh, nvlist_t *dsnvl)
{
	prop_data_t data;

	data.lbh = lbh;
	data.list = dsnvl;
	data.single_object = false;
	data.bootonce = NULL;
	return (be_proplist_update(&data));
}

int
be_get_dataset_props(libbe_handle_t *lbh, const char *name, nvlist_t *props)
{
	zfs_handle_t *snap_hdl;
	prop_data_t data;
	int ret;

	data.lbh = lbh;
	data.list = props;
	data.single_object = true;
	data.bootonce = NULL;
	if ((snap_hdl = zfs_open(lbh->lzh, name,
	    ZFS_TYPE_FILESYSTEM | ZFS_TYPE_SNAPSHOT)) == NULL)
		return (BE_ERR_ZFSOPEN);

	ret = prop_list_builder_cb(snap_hdl, &data);
	zfs_close(snap_hdl);
	return (ret);
}

int
be_get_dataset_snapshots(libbe_handle_t *lbh, const char *name, nvlist_t *props)
{
	zfs_handle_t *ds_hdl;
	prop_data_t data;
	int ret;

	data.lbh = lbh;
	data.list = props;
	data.single_object = false;
	data.bootonce = NULL;
	if ((ds_hdl = zfs_open(lbh->lzh, name,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (BE_ERR_ZFSOPEN);

	ret = snapshot_proplist_update(ds_hdl, &data);
	zfs_close(ds_hdl);
	return (ret);
}

/*
 * Internal callback function used by zfs_iter_filesystems. For each dataset in
 * the bootenv root, populate an nvlist_t of its relevant properties.
 */
int
prop_list_builder_cb(zfs_handle_t *zfs_hdl, void *data_p)
{
	char buf[512], *mountpoint;
	prop_data_t *data;
	libbe_handle_t *lbh;
	nvlist_t *props;
	const char *dataset, *name;
	boolean_t mounted;

	/*
	 * XXX TODO:
	 *      some system for defining constants for the nvlist keys
	 *      error checking
	 */
	data = (prop_data_t *)data_p;
	lbh = data->lbh;

	if (data->single_object)
		props = data->list;
	else
		nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);

	dataset = zfs_get_name(zfs_hdl);
	nvlist_add_string(props, "dataset", dataset);

	if (data->lbh->bootonce != NULL &&
	    strcmp(dataset, data->lbh->bootonce) == 0)
		nvlist_add_boolean_value(props, "bootonce", true);

	name = strrchr(dataset, '/') + 1;
	nvlist_add_string(props, "name", name);

	mounted = zfs_is_mounted(zfs_hdl, &mountpoint);

	if (mounted)
		nvlist_add_string(props, "mounted", mountpoint);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_MOUNTPOINT, buf, 512,
	    NULL, NULL, 0, 1) == 0)
		nvlist_add_string(props, "mountpoint", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_ORIGIN, buf, 512,
	    NULL, NULL, 0, 1) == 0)
		nvlist_add_string(props, "origin", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_CREATION, buf, 512,
	    NULL, NULL, 0, 1) == 0)
		nvlist_add_string(props, "creation", buf);

	nvlist_add_boolean_value(props, "active",
	    (strcmp(be_active_path(lbh), dataset) == 0));

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_USED, buf, 512,
	    NULL, NULL, 0, 1) == 0)
		nvlist_add_string(props, "used", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_USEDDS, buf, 512,
	    NULL, NULL, 0, 1) == 0)
		nvlist_add_string(props, "usedds", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_USEDSNAP, buf, 512,
	    NULL, NULL, 0, 1) == 0)
		nvlist_add_string(props, "usedsnap", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_USEDREFRESERV, buf, 512,
	    NULL, NULL, 0, 1) == 0)
		nvlist_add_string(props, "usedrefreserv", buf);

	if (zfs_prop_get(zfs_hdl, ZFS_PROP_REFERENCED, buf, 512,
	    NULL, NULL, 0, 1) == 0)
		nvlist_add_string(props, "referenced", buf);

	nvlist_add_boolean_value(props, "nextboot",
	    (strcmp(be_nextboot_path(lbh), dataset) == 0));

	if (!data->single_object)
		nvlist_add_nvlist(data->list, name, props);

	return (0);
}


/*
 * Updates the properties of each bootenv in the libbe handle
 * XXX TODO: ensure that this is always consistent (run after adds, deletes,
 *       renames,etc
 */
int
be_proplist_update(prop_data_t *data)
{
	zfs_handle_t *root_hdl;

	if ((root_hdl = zfs_open(data->lbh->lzh, data->lbh->root,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (BE_ERR_ZFSOPEN);

	/* XXX TODO: some error checking here */
	zfs_iter_filesystems(root_hdl, prop_list_builder_cb, data);

	zfs_close(root_hdl);

	return (0);
}

static int
snapshot_proplist_update(zfs_handle_t *hdl, prop_data_t *data)
{

	return (zfs_iter_snapshots_sorted(hdl, prop_list_builder_cb, data,
	    0, 0));
}


int
be_prop_list_alloc(nvlist_t **be_list)
{

	return (nvlist_alloc(be_list, NV_UNIQUE_NAME, KM_SLEEP));
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
int
be_exists(libbe_handle_t *lbh, const char *be)
{
	char buf[BE_MAXPATHLEN];

	be_root_concat(lbh, be, buf);

	if (!zfs_dataset_exists(lbh->lzh, buf, ZFS_TYPE_DATASET))
		return (BE_ERR_NOENT);

	return (BE_ERR_SUCCESS);
}
