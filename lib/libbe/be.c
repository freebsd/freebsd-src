/*
 * be.c
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

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <kenv.h>
#include <libgen.h>
#include <libzfs_core.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "be.h"
#include "be_impl.h"

/*
 * Initializes the libbe context to operate in the root boot environment
 * dataset, for example, zroot/ROOT.
 */
libbe_handle_t *
libbe_init(void)
{
	char buf[BE_MAXPATHLEN];
	struct stat sb;
	dev_t root_dev, boot_dev;
	libbe_handle_t *lbh;
	char *pos;

	// TODO: use errno here??

	/* Verify that /boot and / are mounted on the same filesystem */
	if (stat("/", &sb) != 0) {
		return (NULL);
	}

	root_dev = sb.st_dev;

	if (stat("/boot", &sb) != 0) {
		return (NULL);
	}

	boot_dev = sb.st_dev;

	if (root_dev != boot_dev) {
		fprintf(stderr, "/ and /boot not on same device, quitting\n");
		return (NULL);
	}

	if ((lbh = calloc(1, sizeof(libbe_handle_t))) == NULL) {
		return (NULL);
	}

	if ((lbh->lzh = libzfs_init()) == NULL) {
		free(lbh);
		return (NULL);
	}

	/* Obtain path to active boot environment */
	if ((kenv(KENV_GET, "zfs_be_active", lbh->active,
	    BE_MAXPATHLEN)) == -1) {
		libzfs_fini(lbh->lzh);
		free(lbh);
		return (NULL);
	}

	/* Remove leading 'zfs:' if present, otherwise use value as-is */
	if ((pos = strrchr(lbh->active, ':')) != NULL) {
		strncpy(lbh->active, pos + sizeof(char), BE_MAXPATHLEN);
	}

	/* Obtain path to boot environment root */
	if ((kenv(KENV_GET, "zfs_be_root", lbh->root, BE_MAXPATHLEN)) == -1) {
		libzfs_fini(lbh->lzh);
		free(lbh);
		return (NULL);
	}

	/* Remove leading 'zfs:' if present, otherwise use value as-is */
	if ((pos = strrchr(lbh->root, ':')) != NULL) {
		strncpy(lbh->root, pos + sizeof(char), BE_MAXPATHLEN);
	}

	return (lbh);
}


/*
 * Free memory allocated by libbe_init()
 */
void
libbe_close(libbe_handle_t *lbh)
{
	libzfs_fini(lbh->lzh);
	free(lbh);
}


/*
 * Destroy the boot environment or snapshot specified by the name
 * parameter. Options are or'd together with the possible values:
 * BE_DESTROY_FORCE : forces operation on mounted datasets
 * TODO: Test destroying a non active but mounted be
 */
int
be_destroy(libbe_handle_t *lbh, char *name, int options)
{
	zfs_handle_t *fs;
	char path[BE_MAXPATHLEN];
	char *p = path;
	int mounted;
	int force = options & BE_DESTROY_FORCE;

	int err = BE_ERR_SUCCESS;

	be_root_concat(lbh, name, path);
	printf("path: %s\n", path);

	if (strchr(name, '@') == NULL) {
		if (!zfs_dataset_exists(lbh->lzh, path, ZFS_TYPE_FILESYSTEM)) {
			return (set_error(lbh, BE_ERR_NOENT));
		}

		if (strcmp(path, lbh->active) == 0) {
			return (set_error(lbh, BE_ERR_DESTROYACT));
		}

		fs = zfs_open(lbh->lzh, p, ZFS_TYPE_FILESYSTEM);
	} else {

		if (!zfs_dataset_exists(lbh->lzh, path, ZFS_TYPE_SNAPSHOT)) {
			return (set_error(lbh, BE_ERR_NOENT));
		}

		fs = zfs_open(lbh->lzh, p, ZFS_TYPE_SNAPSHOT);
	}

	if (fs == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	/* Check if mounted, unmount if force is specified */
	if (mounted = zfs_is_mounted(fs, NULL)) {
		if (force) {
			zfs_unmount(fs, NULL, 0);
		} else {
			return (set_error(lbh, BE_ERR_DESTROYMNT));
		}
	}


	// TODO: convert this to use zfs_iter_children first for deep bes
	// XXX note: errno 16 (device busy) occurs when chilren are present
	if ((err = zfs_destroy(fs, false)) != 0) {
		fprintf(stderr, "delete failed errno: %d\n", errno);
	}

	return (err);
}


int
be_snapshot(libbe_handle_t *lbh, char *source, char *snap_name,
    bool recursive, char *result)
{
	char buf[BE_MAXPATHLEN];
	time_t rawtime;
	int len, err;

	be_root_concat(lbh, source, buf);

	if (!be_exists(lbh, buf)) {
		return (BE_ERR_NOENT);
	}

	if (snap_name != NULL) {
		strcat(buf, "@");
		strcat(buf, snap_name);
		if (result != NULL) {
			snprintf(result, BE_MAXPATHLEN, "%s@%s", source,
			    snap_name);
		}
	} else {
		time(&rawtime);
		len = strlen(buf);
		strftime(buf + len, BE_MAXPATHLEN - len,
		    "@%F-%T", localtime(&rawtime));
		if (result != NULL) {
			strcpy(result, strrchr(buf, '/') + 1);
		}
	}

	if (err = zfs_snapshot(lbh->lzh, buf, recursive, NULL) != 0) {
		switch (err) {
		case EZFS_INVALIDNAME:
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		default:
			// TODO: elaborate return codes
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	return (BE_ERR_SUCCESS);
}


/*
 * Create the boot environment specified by the name parameter
 */
int
be_create(libbe_handle_t *lbh, char *name)
{
	int err;

	err = be_create_from_existing(lbh, name, (char *)be_active_path(lbh));

	return (set_error(lbh, err));
}


static int
be_deep_clone_prop(int prop, void *cb)
{
	int err;
        struct libbe_dccb *dccb = cb;
	zprop_source_t src;
	char pval[BE_MAXPATHLEN];
	char source[BE_MAXPATHLEN];

	/* Skip some properties we don't want to touch */
	switch (prop) {
		case ZFS_PROP_CANMOUNT:
			return (ZPROP_CONT);
			break;
	}

	/* Don't copy readonly properties */
	if (zfs_prop_readonly(prop)) {
		return (ZPROP_CONT);
	}

	if ((err = zfs_prop_get(dccb->zhp, prop, (char *)&pval,
	    sizeof(pval), &src, (char *)&source, sizeof(source), false))) {
		/* Just continue if we fail to read a property */
		return (ZPROP_CONT);
	}
	/* Only copy locally defined properties */
	if (src != ZPROP_SRC_LOCAL) {
		return (ZPROP_CONT);
	}

	nvlist_add_string(dccb->props, zfs_prop_to_name(prop), (char *)pval);

	return (ZPROP_CONT);
}

static int
be_deep_clone(zfs_handle_t *ds, void *data)
{
	int err;
	char be_path[BE_MAXPATHLEN];
	char snap_path[BE_MAXPATHLEN];
	char mp[BE_MAXPATHLEN];
	const char *dspath;
	char *dsname;
	zfs_handle_t *snap_hdl;
	nvlist_t *props;
	struct libbe_deep_clone sdc;
	struct libbe_deep_clone *isdc = (struct libbe_deep_clone *)data;
	struct libbe_dccb dccb;

	dspath = zfs_get_name(ds);
	if ((dsname = strrchr(dspath, '/')) == NULL) {
		return (BE_ERR_UNKNOWN);
	}
	dsname++;
	if (isdc->bename == NULL) {
		snprintf(be_path, sizeof(be_path), "%s/%s", isdc->be_root, dsname);
	} else {
		snprintf(be_path, sizeof(be_path), "%s/%s", isdc->be_root, isdc->bename);
	}
	snprintf(snap_path, sizeof(snap_path), "%s@%s", dspath, isdc->snapname);

	if (zfs_dataset_exists(isdc->lbh->lzh, be_path, ZFS_TYPE_DATASET)) {
		return (set_error(isdc->lbh, BE_ERR_EXISTS));
	}

	if ((snap_hdl =
	    zfs_open(isdc->lbh->lzh, snap_path, ZFS_TYPE_SNAPSHOT)) == NULL) {
		return (set_error(isdc->lbh, BE_ERR_ZFSOPEN));
	}

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_add_string(props, "canmount", "noauto");

	dccb.zhp = ds;
	dccb.props = props;
	if (zprop_iter(be_deep_clone_prop, &dccb, B_FALSE, B_FALSE,
	    ZFS_TYPE_FILESYSTEM) == ZPROP_INVAL) {
		return (-1);
	}

	if (err = zfs_clone(snap_hdl, be_path, props)) {
		switch (err) {
		case EZFS_SUCCESS:
			err = BE_ERR_SUCCESS;
			break;
		default:
			err = BE_ERR_ZFSCLONE;
			break;
		}
	}

	nvlist_free(props);
	zfs_close(snap_hdl);

	sdc.lbh = isdc->lbh;
	sdc.bename = NULL;
	sdc.snapname = isdc->snapname;
	sdc.be_root = (char *)&be_path;

	err = zfs_iter_filesystems(ds, be_deep_clone, &sdc);

	return (err);
}

/*
 * Create the boot environment from pre-existing snapshot
 */
int
be_create_from_existing_snap(libbe_handle_t *lbh, char *name, char *snap)
{
	int err;
	char be_path[BE_MAXPATHLEN];
	char snap_path[BE_MAXPATHLEN];
	char *parentname, *bename, *snapname;
	zfs_handle_t *parent_hdl;
	struct libbe_deep_clone sdc;

	if (err = be_validate_name(lbh, name)) {
		return (set_error(lbh, err));
	}

	if (err = be_root_concat(lbh, snap, snap_path)) {
		return (set_error(lbh, err));
	}

	if (err = be_validate_snap(lbh, snap_path)) {
		return (set_error(lbh, err));
	}

	if (err = be_root_concat(lbh, name, be_path)) {
		return (set_error(lbh, err));
	}

	if ((bename = strrchr(name, '/')) == NULL) {
		bename = name;
	} else {
		bename++;
	}
	if ((parentname = strdup(snap_path)) == NULL) {
		err = BE_ERR_UNKNOWN;
		return (set_error(lbh, err));
	}
	snapname = strchr(parentname, '@');
	if (snapname == NULL) {
		err = BE_ERR_UNKNOWN;
		return (set_error(lbh, err));
	}
	*snapname = '\0';
	snapname++;

	sdc.lbh = lbh;
	sdc.bename = bename;
	sdc.snapname = snapname;
	sdc.be_root = lbh->root;

	parent_hdl = zfs_open(lbh->lzh, parentname, ZFS_TYPE_DATASET);
	err = be_deep_clone(parent_hdl, &sdc);

	return (set_error(lbh, err));
}


/*
 * Create a boot environment from an existing boot environment
 */
int
be_create_from_existing(libbe_handle_t *lbh, char *name, char *old)
{
	int err;
	char buf[BE_MAXPATHLEN];

	if ((err = be_snapshot(lbh, old, NULL, true, (char *)&buf))) {
		return (set_error(lbh, err));
	}

	err = be_create_from_existing_snap(lbh, name, (char *)buf);

	return (set_error(lbh, err));
}


/*
 * Verifies that a snapshot has a valid name, exists, and has a mountpoint of
 * '/'. Returns BE_ERR_SUCCESS (0), upon success, or the relevant BE_ERR_* upon
 * failure. Does not set the internal library error state.
 */
int
be_validate_snap(libbe_handle_t *lbh, char *snap_name)
{
	zfs_handle_t *zfs_hdl;
	char buf[BE_MAXPATHLEN];
	char *delim_pos;
	char *mountpoint;
	int err = BE_ERR_SUCCESS;

	if (strlen(snap_name) >= BE_MAXPATHLEN) {
		return (BE_ERR_PATHLEN);
	}

	if (!zfs_dataset_exists(lbh->lzh, snap_name,
	    ZFS_TYPE_SNAPSHOT)) {
		return (BE_ERR_NOENT);
	}

	strncpy(buf, snap_name, BE_MAXPATHLEN);

	/* Find the base filesystem of the snapshot */
	if ((delim_pos = strchr(buf, '@')) == NULL) {
		return (BE_ERR_INVALIDNAME);
	}
	*delim_pos = '\0';

	if ((zfs_hdl =
	    zfs_open(lbh->lzh, buf, ZFS_TYPE_DATASET)) == NULL) {
		return (BE_ERR_NOORIGIN);
	}

	if (err =
	    zfs_prop_get(zfs_hdl, ZFS_PROP_MOUNTPOINT, buf,
	    BE_MAXPATHLEN,
	    NULL, NULL, 0, 1)) {
		err = BE_ERR_INVORIGIN;
	}

	if ((err != 0) && (strncmp(buf, "/", BE_MAXPATHLEN) != 0)) {
		err = BE_ERR_INVORIGIN;
	}

	zfs_close(zfs_hdl);

	return (err);
}


/*
 * Idempotently appends the name argument to the root boot environment path
 * and copies the resulting string into the result buffer (which is assumed
 * to be at least BE_MAXPATHLEN characters long. Returns BE_ERR_SUCCESS upon
 * success, BE_ERR_PATHLEN if the resulting path is longer than BE_MAXPATHLEN,
 * or BE_ERR_INVALIDNAME if the name is a path that does not begin with
 * zfs_be_root. Does not set internal library error state.
 */
int
be_root_concat(libbe_handle_t *lbh, char *name, char *result)
{
	size_t name_len, root_len;

	name_len = strlen(name);
	root_len = strlen(lbh->root);

	/* Act idempotently; return be name if it is already a full path */
	if (strrchr(name, '/') != NULL) {
		if (strstr(name, lbh->root) != name) {
			return (BE_ERR_INVALIDNAME);
		}

		if (name_len >= BE_MAXPATHLEN) {
			return (BE_ERR_PATHLEN);
		}

		strncpy(result, name, BE_MAXPATHLEN);
		return (BE_ERR_SUCCESS);
	} else if (name_len + root_len + 1 < BE_MAXPATHLEN) {
		snprintf(result, BE_MAXPATHLEN, "%s/%s", lbh->root,
		    name);
		return (BE_ERR_SUCCESS);
	}

	return (BE_ERR_PATHLEN);
}


/*
 * Verifies the validity of a boot environment name (A-Za-z0-9-_.). Returns
 * BE_ERR_SUCCESS (0) if name is valid, otherwise returns BE_ERR_INVALIDNAME.
 * Does not set internal library error state.
 */
int
be_validate_name(libbe_handle_t *lbh, char *name)
{
	for (int i = 0; *name; i++) {
		char c = *(name++);
		if (isalnum(c) || (c == '-') || (c == '_') ||
		    (c == '.')) {
			continue;
		}
		return (BE_ERR_INVALIDNAME);
	}

	return (BE_ERR_SUCCESS);
}


/*
 * usage
 */
int
be_rename(libbe_handle_t *lbh, char *old, char *new)
{
	char full_old[BE_MAXPATHLEN];
	char full_new[BE_MAXPATHLEN];
	zfs_handle_t *zfs_hdl;
	int err;


	if (err = be_root_concat(lbh, old, full_old)) {
		return (set_error(lbh, err));
	}
	if (err = be_root_concat(lbh, new, full_new)) {
		return (set_error(lbh, err));
	}

	if (be_validate_name(lbh, new)) {
		return (BE_ERR_UNKNOWN);
		// TODO set and return correct error
	}

	// check if old is active be
	if (strcmp(full_new, be_active_path(lbh)) == 0) {
		return (BE_ERR_UNKNOWN);
		// TODO set and return correct error
	}

	if (!zfs_dataset_exists(lbh->lzh, full_old, ZFS_TYPE_DATASET)) {
		return (BE_ERR_UNKNOWN);
		// TODO set and return correct error
	}

	if (zfs_dataset_exists(lbh->lzh, full_new, ZFS_TYPE_DATASET)) {
		return (BE_ERR_UNKNOWN);
		// TODO set and return correct error
	}

	// TODO: what about mounted bes?
	//              - if mounted error out unless a force flag is set?


	if ((zfs_hdl = zfs_open(lbh->lzh, full_old,
	    ZFS_TYPE_FILESYSTEM)) == NULL) {
		return (BE_ERR_UNKNOWN);
		// TODO set and return correct error
	}


	// recurse, nounmount, forceunmount
	struct renameflags flags = { 0, 0, 0 };

	// TODO: error log on this call
	err = zfs_rename(zfs_hdl, NULL, full_new, flags);

	zfs_close(zfs_hdl);

	return (set_error(lbh, err));
}


int
be_export(libbe_handle_t *lbh, char *bootenv, int fd)
{
	char snap_name[BE_MAXPATHLEN];
	char buf[BE_MAXPATHLEN];
	zfs_handle_t *zfs;
	int err;

	if (err = be_snapshot(lbh, bootenv, NULL, true, snap_name)) {
		// TODO error handle
		return (-1);
	}

	be_root_concat(lbh, snap_name, buf);

	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_DATASET)) == NULL) {
		return (BE_ERR_ZFSOPEN);
	}

	err = zfs_send_one(zfs, NULL, fd, 0);
	return (err);
}


int
be_import(libbe_handle_t *lbh, char *bootenv, int fd)
{
	char buf[BE_MAXPATHLEN];
	time_t rawtime;
	nvlist_t *props;
	zfs_handle_t *zfs;
	int err, len;

	// TODO: this is a very likely name for someone to already have used
	if (err = be_root_concat(lbh, "be_import_temp", buf)) {
		// TODO error handle
		return (-1);
	}

	time(&rawtime);
	len = strlen(buf);
	strftime(buf + len, BE_MAXPATHLEN - len,
	    "@%F-%T", localtime(&rawtime));


	// lzc_receive(SNAPNAME, PROPS, ORIGIN, FORCE, fd)) {
	if (err = lzc_receive(buf, NULL, NULL, false, fd)) {
		/* TODO: go through libzfs_core's recv_impl and find returned
		 * errors and set appropriate BE_ERR
		 * edit: errors are not in libzfs_core, my assumption is
		 *  that they use libzfs errors
		 * note: 17 is err for dataset already existing */
		return (err);
	}

	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_SNAPSHOT)) == NULL) {
		// TODO correct error
		return (-1);
	}

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_add_string(props, "canmount", "noauto");
	nvlist_add_string(props, "mountpoint", "/");

	be_root_concat(lbh, bootenv, buf);

	err = zfs_clone(zfs, buf, props);
	zfs_close(zfs);

	nvlist_free(props);

	// TODO: recursively delete be_import_temp dataset

	return (err);
}


int
be_add_child(libbe_handle_t *lbh, char *child_path, bool cp_if_exists)
{
	char active[BE_MAXPATHLEN];
	char buf[BE_MAXPATHLEN];
	nvlist_t *props;
	zfs_handle_t *zfs;
	struct stat sb;
	int err;

	/* Require absolute paths */
	if (*child_path != '/') {
		/* TODO: create appropriate error */
		return (-1);
	}

	strncpy(active, be_active_path(lbh), BE_MAXPATHLEN);
	strcpy(buf, active);

	/* Create non-mountable parent dataset(s) */
	char *s = child_path;
	for (char *p; (p = strchr(s+1, '/')) != NULL; s = p) {
		size_t len = p - s;
		strncat(buf, s, len);

		nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
		nvlist_add_string(props, "canmount", "off");
		nvlist_add_string(props, "mountpoint", "none");
		zfs_create(lbh->lzh, buf, ZFS_TYPE_DATASET, props);
		nvlist_free(props);
	}


	/* Path does not exist as a descendent of / yet */
	int pos = strlen(active);

	/* TODO: Verify that resulting str is less than BE_MAXPATHLEN */
	strncpy(&active[pos], child_path, BE_MAXPATHLEN-pos);


	if (stat(child_path, &sb) != 0) {
		/* Verify that error is ENOENT */
		if (errno != 2) {
			/* TODO: create appropriate error */
			return (-1);
		}

		nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
		nvlist_add_string(props, "canmount", "noauto");
		nvlist_add_string(props, "mountpoint", child_path);

		// create
		if (err =
		    zfs_create(lbh->lzh, active, ZFS_TYPE_DATASET, props)) {
			/* TODO handle error */
			return (-1);
		}
		nvlist_free(props);

		if ((zfs =
		    zfs_open(lbh->lzh, active, ZFS_TYPE_DATASET)) == NULL) {
			/* TODO handle error */
			return (-1);
		}

		// set props
		if (err = zfs_prop_set(zfs, "canmount", "noauto")) {
			/* TODO handle error */
			return (-1);
		}
	} else if (cp_if_exists) {
		/* Path is already a descendent of / and should be copied */



		// TODO

		/*
		 * Establish if the existing path is a zfs dataset or just
		 * the subdirectory of one
		 */


		// TODO: use mktemp
		long int snap_name = random();

		snprintf(buf, BE_MAXPATHLEN, "%s@%ld", child_path, snap_name);

		if (err = zfs_snapshot(lbh->lzh, buf, false, NULL)) {
			// TODO correct error
			return (-1);
		}

		// clone
		if ((zfs =
		    zfs_open(lbh->lzh, buf, ZFS_TYPE_SNAPSHOT)) == NULL) {
			// TODO correct error
			return (-1);
		}

		if (err = zfs_clone(zfs, active, NULL)) {
			// TODO correct error
			return (-1);
		}


		// set props
	} else {
		/* TODO: error code for exists, but not cp? */
		return (-1);
	}


	return (BE_ERR_SUCCESS);
}


int
be_activate(libbe_handle_t *lbh, char *bootenv, bool temporary)
{
	char be_path[BE_MAXPATHLEN];
	char buf[BE_MAXPATHLEN];
	zpool_handle_t *zph;
	uint64_t pool_guid;
	uint64_t vdev_guid;
	int zfs_fd;
	int len;
	int err;

	be_root_concat(lbh, bootenv, be_path);


	/* Note: be_exists fails if mountpoint is not / */
	if (!be_exists(lbh, be_path)) {
		return (BE_ERR_NOENT);
	}

	if (temporary) {
		// TODO: give proper attribution to author(s) of zfsbootcfg
		// for this snippet

		if (kenv(KENV_GET, "vfs.zfs.boot.primary_pool", buf,
		    sizeof(buf)) <= 0) {
			return (1);
		}
		pool_guid = strtoumax(buf, NULL, 10);
		if (pool_guid == 0) {
			return (1);
		}

		if (kenv(KENV_GET, "vfs.zfs.boot.primary_vdev", buf,
		    sizeof(buf)) <= 0) {
			return (1);
		}
		vdev_guid = strtoumax(buf, NULL, 10);
		if (vdev_guid == 0) {
			return (1);
		}

		/* Expected format according to zfsbootcfg(8) man */
		strcpy(buf, "zfs:");
		strcat(buf, be_path);
		strcat(buf, ":");

		if (zpool_nextboot(lbh->lzh, pool_guid, vdev_guid, buf) != 0) {
			perror("ZFS_IOC_NEXTBOOT failed");
			return (1);
		}

		return (BE_ERR_SUCCESS);
	} else {
		/* Obtain bootenv zpool */
		strncpy(buf, be_path, BE_MAXPATHLEN);
		*(strchr(buf, '/')) = '\0';

		if ((zph = zpool_open(lbh->lzh, buf)) == NULL) {
			// TODO: create error for this
			return (-1);
		}
		printf("asdf\n");

		err = zpool_set_prop(zph, "bootfs", be_path);
		zpool_close(zph);

		switch (err) {
		case 0:
			return (BE_ERR_SUCCESS);

		default:
			// TODO correct errors
			return (-1);
		}
	}
}
