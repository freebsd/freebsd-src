/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <util.h>

#include "makefs.h"
#include "zfs.h"

typedef struct zfs_dsl_dataset {
	zfs_objset_t	*os;		/* referenced objset, may be null */
	dsl_dataset_phys_t *phys;	/* on-disk representation */
	uint64_t	dsid;		/* DSL dataset dnode */

	struct zfs_dsl_dir *dir;	/* containing parent */
} zfs_dsl_dataset_t;

typedef STAILQ_HEAD(zfs_dsl_dir_list, zfs_dsl_dir) zfs_dsl_dir_list_t;

typedef struct zfs_dsl_dir {
	char		*fullname;	/* full dataset name */
	char		*name;		/* basename(fullname) */
	dsl_dir_phys_t	*phys;		/* on-disk representation */
	nvlist_t	*propsnv;	/* properties saved in propszap */

	zfs_dsl_dataset_t *headds;	/* principal dataset, may be null */

	uint64_t	dirid;		/* DSL directory dnode */
	zfs_zap_t	*propszap;	/* dataset properties */
	zfs_zap_t	*childzap;	/* child directories */

	/* DSL directory tree linkage. */
	struct zfs_dsl_dir *parent;
	zfs_dsl_dir_list_t children;
	STAILQ_ENTRY(zfs_dsl_dir) next;
} zfs_dsl_dir_t;

static zfs_dsl_dir_t *dsl_dir_alloc(zfs_opt_t *zfs, const char *name);
static zfs_dsl_dataset_t *dsl_dataset_alloc(zfs_opt_t *zfs, zfs_dsl_dir_t *dir);

static int
nvlist_find_string(nvlist_t *nvl, const char *key, char **retp)
{
	char *str;
	int error, len;

	error = nvlist_find(nvl, key, DATA_TYPE_STRING, NULL, &str, &len);
	if (error == 0) {
		*retp = ecalloc(1, len + 1);
		memcpy(*retp, str, len);
	}
	return (error);
}

static int
nvlist_find_uint64(nvlist_t *nvl, const char *key, uint64_t *retp)
{
	return (nvlist_find(nvl, key, DATA_TYPE_UINT64, NULL, retp, NULL));
}

/*
 * Return an allocated string containing the head dataset's mountpoint,
 * including the root path prefix.
 *
 * If the dataset has a mountpoint property, it is returned.  Otherwise we have
 * to follow ZFS' inheritance rules.
 */
char *
dsl_dir_get_mountpoint(zfs_opt_t *zfs, zfs_dsl_dir_t *dir)
{
	zfs_dsl_dir_t *pdir;
	char *mountpoint, *origmountpoint;

	if (nvlist_find_string(dir->propsnv, "mountpoint", &mountpoint) == 0) {
		if (strcmp(mountpoint, "none") == 0)
			return (NULL);

		/*
		 * nvlist_find_string() does not make a copy.
		 */
		mountpoint = estrdup(mountpoint);
	} else {
		/*
		 * If we don't have a mountpoint, it's inherited from one of our
		 * ancestors.  Walk up the hierarchy until we find it, building
		 * up our mountpoint along the way.  The mountpoint property is
		 * always set for the root dataset.
		 */
		for (pdir = dir->parent, mountpoint = estrdup(dir->name);;) {
			origmountpoint = mountpoint;

			if (nvlist_find_string(pdir->propsnv, "mountpoint",
			    &mountpoint) == 0) {
				easprintf(&mountpoint, "%s%s%s", mountpoint,
				    mountpoint[strlen(mountpoint) - 1] == '/' ?
				    "" : "/", origmountpoint);
				free(origmountpoint);
				break;
			}

			easprintf(&mountpoint, "%s/%s", pdir->name,
			    origmountpoint);
			free(origmountpoint);
			pdir = pdir->parent;
		}
	}
	assert(mountpoint[0] == '/');
	assert(strstr(mountpoint, zfs->rootpath) == mountpoint);

	return (mountpoint);
}

int
dsl_dir_get_canmount(zfs_dsl_dir_t *dir, uint64_t *canmountp)
{
	return (nvlist_find_uint64(dir->propsnv, "canmount", canmountp));
}

/*
 * Handle dataset properties that we know about; stash them into an nvlist to be
 * written later to the properties ZAP object.
 *
 * If the set of properties we handle grows too much, we should probably explore
 * using libzfs to manage them.
 */
static void
dsl_dir_set_prop(zfs_opt_t *zfs, zfs_dsl_dir_t *dir, const char *key,
    const char *val)
{
	nvlist_t *nvl;

	nvl = dir->propsnv;
	if (val == NULL || val[0] == '\0')
		errx(1, "missing value for property `%s'", key);
	if (nvpair_find(nvl, key) != NULL)
		errx(1, "property `%s' already set", key);

	if (strcmp(key, "mountpoint") == 0) {
		if (strcmp(val, "none") != 0) {
			if (val[0] != '/')
				errx(1, "mountpoint `%s' is not absolute", val);
			if (strcmp(val, zfs->rootpath) != 0 &&
			    strcmp(zfs->rootpath, "/") != 0 &&
			    (strstr(val, zfs->rootpath) != val ||
			     val[strlen(zfs->rootpath)] != '/')) {
				errx(1, "mountpoint `%s' is not prefixed by "
				    "the root path `%s'", val, zfs->rootpath);
			}
		}
		nvlist_add_string(nvl, key, val);
	} else if (strcmp(key, "atime") == 0 || strcmp(key, "exec") == 0 ||
	    strcmp(key, "setuid") == 0) {
		if (strcmp(val, "on") == 0)
			nvlist_add_uint64(nvl, key, 1);
		else if (strcmp(val, "off") == 0)
			nvlist_add_uint64(nvl, key, 0);
		else
			errx(1, "invalid value `%s' for %s", val, key);
	} else if (strcmp(key, "canmount") == 0) {
		if (strcmp(val, "noauto") == 0)
			nvlist_add_uint64(nvl, key, 2);
		else if (strcmp(val, "on") == 0)
			nvlist_add_uint64(nvl, key, 1);
		else if (strcmp(val, "off") == 0)
			nvlist_add_uint64(nvl, key, 0);
		else
			errx(1, "invalid value `%s' for %s", val, key);
	} else {
		errx(1, "unknown property `%s'", key);
	}
}

static zfs_dsl_dir_t *
dsl_metadir_alloc(zfs_opt_t *zfs, const char *name)
{
	zfs_dsl_dir_t *dir;
	char *path;

	easprintf(&path, "%s/%s", zfs->poolname, name);
	dir = dsl_dir_alloc(zfs, path);
	free(path);
	return (dir);
}

static void
dsl_origindir_init(zfs_opt_t *zfs)
{
	dnode_phys_t *clones;
	uint64_t clonesid;

	zfs->origindsldir = dsl_metadir_alloc(zfs, "$ORIGIN");
	zfs->originds = dsl_dataset_alloc(zfs, zfs->origindsldir);
	zfs->snapds = dsl_dataset_alloc(zfs, zfs->origindsldir);

	clones = objset_dnode_alloc(zfs->mos, DMU_OT_DSL_CLONES, &clonesid);
	zfs->cloneszap = zap_alloc(zfs->mos, clones);
	zfs->origindsldir->phys->dd_clones = clonesid;
}

void
dsl_init(zfs_opt_t *zfs)
{
	zfs_dsl_dir_t *dir;
	struct dataset_desc *d;
	const char *dspropdelim;

	dspropdelim = ";";

	zfs->rootdsldir = dsl_dir_alloc(zfs, NULL);

	nvlist_add_uint64(zfs->rootdsldir->propsnv, "compression",
	    ZIO_COMPRESS_OFF);

	zfs->rootds = dsl_dataset_alloc(zfs, zfs->rootdsldir);
	zfs->rootdsldir->headds = zfs->rootds;

	zfs->mosdsldir = dsl_metadir_alloc(zfs, "$MOS");
	zfs->freedsldir = dsl_metadir_alloc(zfs, "$FREE");
	dsl_origindir_init(zfs);

	/*
	 * Go through the list of user-specified datasets and create DSL objects
	 * for them.
	 */
	STAILQ_FOREACH(d, &zfs->datasetdescs, next) {
		char *dsname, *next, *params, *param, *nextparam;

		params = d->params;
		dsname = strsep(&params, dspropdelim);

		if (strcmp(dsname, zfs->poolname) == 0) {
			/*
			 * This is the root dataset; it's already created, so
			 * we're just setting options.
			 */
			dir = zfs->rootdsldir;
		} else {
			/*
			 * This dataset must be a child of the root dataset.
			 */
			if (strstr(dsname, zfs->poolname) != dsname ||
			    (next = strchr(dsname, '/')) == NULL ||
			    (size_t)(next - dsname) != strlen(zfs->poolname)) {
				errx(1, "dataset `%s' must be a child of `%s'",
				    dsname, zfs->poolname);
			}
			dir = dsl_dir_alloc(zfs, dsname);
			dir->headds = dsl_dataset_alloc(zfs, dir);
		}

		for (nextparam = param = params; nextparam != NULL;) {
			char *key, *val;

			param = strsep(&nextparam, dspropdelim);

			key = val = param;
			key = strsep(&val, "=");
			dsl_dir_set_prop(zfs, dir, key, val);
		}
	}

	/*
	 * Set the root dataset's mount point if the user didn't override the
	 * default.
	 */
	if (nvpair_find(zfs->rootdsldir->propsnv, "mountpoint") == NULL) {
		nvlist_add_string(zfs->rootdsldir->propsnv, "mountpoint",
		    zfs->rootpath);
	}
}

uint64_t
dsl_dir_id(zfs_dsl_dir_t *dir)
{
	return (dir->dirid);
}

uint64_t
dsl_dir_dataset_id(zfs_dsl_dir_t *dir)
{
	return (dir->headds->dsid);
}

static void
dsl_dir_foreach_post(zfs_opt_t *zfs, zfs_dsl_dir_t *dsldir,
    void (*cb)(zfs_opt_t *, zfs_dsl_dir_t *, void *), void *arg)
{
	zfs_dsl_dir_t *cdsldir;

	STAILQ_FOREACH(cdsldir, &dsldir->children, next) {
		dsl_dir_foreach_post(zfs, cdsldir, cb, arg);
	}
	cb(zfs, dsldir, arg);
}

/*
 * Used when the caller doesn't care about the order one way or another.
 */
void
dsl_dir_foreach(zfs_opt_t *zfs, zfs_dsl_dir_t *dsldir,
    void (*cb)(zfs_opt_t *, zfs_dsl_dir_t *, void *), void *arg)
{
	dsl_dir_foreach_post(zfs, dsldir, cb, arg);
}

const char *
dsl_dir_fullname(const zfs_dsl_dir_t *dir)
{
	return (dir->fullname);
}

/*
 * Create a DSL directory, which is effectively an entry in the ZFS namespace.
 * We always create a root DSL directory, whose name is the pool's name, and
 * several metadata directories.
 *
 * Each directory has two ZAP objects, one pointing to child directories, and
 * one for properties (which are inherited by children unless overridden).
 * Directories typically reference a DSL dataset, the "head dataset", which
 * points to an object set.
 */
static zfs_dsl_dir_t *
dsl_dir_alloc(zfs_opt_t *zfs, const char *name)
{
	zfs_dsl_dir_list_t l, *lp;
	zfs_dsl_dir_t *dir, *parent;
	dnode_phys_t *dnode;
	char *dirname, *nextdir, *origname;
	uint64_t childid, propsid;

	dir = ecalloc(1, sizeof(*dir));

	dnode = objset_dnode_bonus_alloc(zfs->mos, DMU_OT_DSL_DIR,
	    DMU_OT_DSL_DIR, sizeof(dsl_dir_phys_t), &dir->dirid);
	dir->phys = (dsl_dir_phys_t *)DN_BONUS(dnode);

	dnode = objset_dnode_alloc(zfs->mos, DMU_OT_DSL_PROPS, &propsid);
	dir->propszap = zap_alloc(zfs->mos, dnode);

	dnode = objset_dnode_alloc(zfs->mos, DMU_OT_DSL_DIR_CHILD_MAP,
	    &childid);
	dir->childzap = zap_alloc(zfs->mos, dnode);

	dir->propsnv = nvlist_create(NV_UNIQUE_NAME);
	STAILQ_INIT(&dir->children);

	dir->phys->dd_child_dir_zapobj = childid;
	dir->phys->dd_props_zapobj = propsid;

	if (name == NULL) {
		/*
		 * This is the root DSL directory.
		 */
		dir->name = estrdup(zfs->poolname);
		dir->fullname = estrdup(zfs->poolname);
		dir->parent = NULL;
		dir->phys->dd_parent_obj = 0;

		assert(zfs->rootdsldir == NULL);
		zfs->rootdsldir = dir;
		return (dir);
	}

	/*
	 * Insert the new directory into the hierarchy.  Currently this must be
	 * done in order, e.g., when creating pool/a/b, pool/a must already
	 * exist.
	 */
	STAILQ_INIT(&l);
	STAILQ_INSERT_HEAD(&l, zfs->rootdsldir, next);
	origname = dirname = nextdir = estrdup(name);
	for (lp = &l;; lp = &parent->children) {
		dirname = strsep(&nextdir, "/");
		if (nextdir == NULL)
			break;

		STAILQ_FOREACH(parent, lp, next) {
			if (strcmp(parent->name, dirname) == 0)
				break;
		}
		if (parent == NULL) {
			errx(1, "no parent at `%s' for filesystem `%s'",
			    dirname, name);
		}
	}

	dir->fullname = estrdup(name);
	dir->name = estrdup(dirname);
	free(origname);
	STAILQ_INSERT_TAIL(lp, dir, next);
	zap_add_uint64(parent->childzap, dir->name, dir->dirid);

	dir->parent = parent;
	dir->phys->dd_parent_obj = parent->dirid;
	return (dir);
}

void
dsl_dir_size_add(zfs_dsl_dir_t *dir, uint64_t bytes)
{
	dir->phys->dd_used_bytes += bytes;
	dir->phys->dd_compressed_bytes += bytes;
	dir->phys->dd_uncompressed_bytes += bytes;
}

/*
 * Convert dataset properties into entries in the DSL directory's properties
 * ZAP.
 */
static void
dsl_dir_finalize_props(zfs_dsl_dir_t *dir)
{
	for (nvp_header_t *nvh = NULL;
	    (nvh = nvlist_next_nvpair(dir->propsnv, nvh)) != NULL;) {
		nv_string_t *nvname;
		nv_pair_data_t *nvdata;
		char *name;

		nvname = (nv_string_t *)(nvh + 1);
		nvdata = (nv_pair_data_t *)(&nvname->nv_data[0] +
		    NV_ALIGN4(nvname->nv_size));

		name = nvstring_get(nvname);
		switch (nvdata->nv_type) {
		case DATA_TYPE_UINT64: {
			uint64_t val;

			memcpy(&val, &nvdata->nv_data[0], sizeof(uint64_t));
			zap_add_uint64(dir->propszap, name, val);
			break;
		}
		case DATA_TYPE_STRING: {
			nv_string_t *nvstr;
			char *val;

			nvstr = (nv_string_t *)&nvdata->nv_data[0];
			val = nvstring_get(nvstr);
			zap_add_string(dir->propszap, name, val);
			free(val);
			break;
		}
		default:
			assert(0);
		}
		free(name);
	}
}

static void
dsl_dir_finalize(zfs_opt_t *zfs, zfs_dsl_dir_t *dir, void *arg __unused)
{
	char key[32];
	zfs_dsl_dir_t *cdir;
	dnode_phys_t *snapnames;
	zfs_dsl_dataset_t *headds;
	zfs_objset_t *os;
	uint64_t bytes, snapnamesid;

	dsl_dir_finalize_props(dir);
	zap_write(zfs, dir->propszap);
	zap_write(zfs, dir->childzap);

	headds = dir->headds;
	if (headds == NULL)
		return;
	os = headds->os;
	if (os == NULL)
		return;

	snapnames = objset_dnode_alloc(zfs->mos, DMU_OT_DSL_DS_SNAP_MAP,
	    &snapnamesid);
	zap_write(zfs, zap_alloc(zfs->mos, snapnames));

	dir->phys->dd_head_dataset_obj = headds->dsid;
	dir->phys->dd_clone_parent_obj = zfs->snapds->dsid;
	headds->phys->ds_prev_snap_obj = zfs->snapds->dsid;
	headds->phys->ds_snapnames_zapobj = snapnamesid;
	objset_root_blkptr_copy(os, &headds->phys->ds_bp);

	zfs->snapds->phys->ds_num_children++;
	snprintf(key, sizeof(key), "%jx", (uintmax_t)headds->dsid);
	zap_add_uint64(zfs->cloneszap, key, headds->dsid);

	bytes = objset_space(os);
	headds->phys->ds_used_bytes = bytes;
	headds->phys->ds_uncompressed_bytes = bytes;
	headds->phys->ds_compressed_bytes = bytes;

	STAILQ_FOREACH(cdir, &dir->children, next) {
		/*
		 * The root directory needs a special case: the amount of
		 * space used for the MOS isn't known until everything else is
		 * finalized, so it can't be accounted in the MOS directory's
		 * parent until then.
		 */
		if (dir == zfs->rootdsldir && cdir == zfs->mosdsldir)
			continue;
		bytes += cdir->phys->dd_used_bytes;
	}
	dsl_dir_size_add(dir, bytes);
}

void
dsl_write(zfs_opt_t *zfs)
{
	zfs_zap_t *snapnameszap;
	dnode_phys_t *snapnames;
	uint64_t snapmapid;

	/*
	 * Perform accounting, starting from the leaves of the DSL directory
	 * tree.  Accounting for $MOS is done later, once we've finished
	 * allocating space.
	 */
	dsl_dir_foreach_post(zfs, zfs->rootdsldir, dsl_dir_finalize, NULL);

	snapnames = objset_dnode_alloc(zfs->mos, DMU_OT_DSL_DS_SNAP_MAP,
	    &snapmapid);
	snapnameszap = zap_alloc(zfs->mos, snapnames);
	zap_add_uint64(snapnameszap, "$ORIGIN", zfs->snapds->dsid);
	zap_write(zfs, snapnameszap);

	zfs->origindsldir->phys->dd_head_dataset_obj = zfs->originds->dsid;
	zfs->originds->phys->ds_prev_snap_obj = zfs->snapds->dsid;
	zfs->originds->phys->ds_snapnames_zapobj = snapmapid;

	zfs->snapds->phys->ds_next_snap_obj = zfs->originds->dsid;
	assert(zfs->snapds->phys->ds_num_children > 0);
	zfs->snapds->phys->ds_num_children++;

	zap_write(zfs, zfs->cloneszap);

	/* XXX-MJ dirs and datasets are leaked */
}

void
dsl_dir_dataset_write(zfs_opt_t *zfs, zfs_objset_t *os, zfs_dsl_dir_t *dir)
{
	dir->headds->os = os;
	objset_write(zfs, os);
}

bool
dsl_dir_has_dataset(zfs_dsl_dir_t *dir)
{
	return (dir->headds != NULL);
}

bool
dsl_dir_dataset_has_objset(zfs_dsl_dir_t *dir)
{
	return (dsl_dir_has_dataset(dir) && dir->headds->os != NULL);
}

static zfs_dsl_dataset_t *
dsl_dataset_alloc(zfs_opt_t *zfs, zfs_dsl_dir_t *dir)
{
	zfs_dsl_dataset_t *ds;
	dnode_phys_t *dnode;
	uint64_t deadlistid;

	ds = ecalloc(1, sizeof(*ds));

	dnode = objset_dnode_bonus_alloc(zfs->mos, DMU_OT_DSL_DATASET,
	    DMU_OT_DSL_DATASET, sizeof(dsl_dataset_phys_t), &ds->dsid);
	ds->phys = (dsl_dataset_phys_t *)DN_BONUS(dnode);

	dnode = objset_dnode_bonus_alloc(zfs->mos, DMU_OT_DEADLIST,
	    DMU_OT_DEADLIST_HDR, sizeof(dsl_deadlist_phys_t), &deadlistid);
	zap_write(zfs, zap_alloc(zfs->mos, dnode));

	ds->phys->ds_dir_obj = dir->dirid;
	ds->phys->ds_deadlist_obj = deadlistid;
	ds->phys->ds_creation_txg = TXG - 1;
	if (ds != zfs->snapds)
		ds->phys->ds_prev_snap_txg = TXG - 1;
	ds->phys->ds_guid = ((uint64_t)random() << 32) | random();
	ds->dir = dir;

	return (ds);
}
