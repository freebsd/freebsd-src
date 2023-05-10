/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/queue.h>

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <util.h>

#include "makefs.h"
#include "zfs.h"

#define	VDEV_LABEL_SPACE	\
	((off_t)(VDEV_LABEL_START_SIZE + VDEV_LABEL_END_SIZE))
_Static_assert(VDEV_LABEL_SPACE <= MINDEVSIZE, "");

#define	MINMSSIZE		((off_t)1 << 24) /* 16MB */
#define	DFLTMSSIZE		((off_t)1 << 29) /* 512MB */
#define	MAXMSSIZE		((off_t)1 << 34) /* 16GB */

#define	INDIR_LEVELS		6
/* Indirect blocks are always 128KB. */
#define	BLKPTR_PER_INDIR	(MAXBLOCKSIZE / sizeof(blkptr_t))

struct dnode_cursor {
	char		inddir[INDIR_LEVELS][MAXBLOCKSIZE];
	off_t		indloc;
	off_t		indspace;
	dnode_phys_t	*dnode;
	off_t		dataoff;
	off_t		datablksz;
};

void
zfs_prep_opts(fsinfo_t *fsopts)
{
	zfs_opt_t *zfs;
	size_t align;

	align = alignof(uint64_t);
	zfs = aligned_alloc(align, roundup2(sizeof(*zfs), align));
	if (zfs == NULL)
		err(1, "aligned_alloc");
	memset(zfs, 0, sizeof(*zfs));

	const option_t zfs_options[] = {
		{ '\0', "bootfs", &zfs->bootfs, OPT_STRPTR,
		  0, 0, "Bootable dataset" },
		{ '\0', "mssize", &zfs->mssize, OPT_INT64,
		  MINMSSIZE, MAXMSSIZE, "Metaslab size" },
		{ '\0', "poolname", &zfs->poolname, OPT_STRPTR,
		  0, 0, "ZFS pool name" },
		{ '\0', "rootpath", &zfs->rootpath, OPT_STRPTR,
		  0, 0, "Prefix for all dataset mount points" },
		{ '\0', "ashift", &zfs->ashift, OPT_INT32,
		  MINBLOCKSHIFT, MAXBLOCKSHIFT, "ZFS pool ashift" },
		{ '\0', "nowarn", &zfs->nowarn, OPT_BOOL,
		  0, 0, "Suppress warning about experimental ZFS support" },
		{ .name = NULL }
	};

	STAILQ_INIT(&zfs->datasetdescs);

	fsopts->fs_specific = zfs;
	fsopts->fs_options = copy_opts(zfs_options);
}

int
zfs_parse_opts(const char *option, fsinfo_t *fsopts)
{
	zfs_opt_t *zfs;
	struct dataset_desc *dsdesc;
	char buf[BUFSIZ], *opt, *val;
	int rv;

	zfs = fsopts->fs_specific;

	opt = val = estrdup(option);
	opt = strsep(&val, "=");
	if (strcmp(opt, "fs") == 0) {
		if (val == NULL)
			errx(1, "invalid filesystem parameters `%s'", option);

		/*
		 * Dataset descriptions will be parsed later, in dsl_init().
		 * Just stash them away for now.
		 */
		dsdesc = ecalloc(1, sizeof(*dsdesc));
		dsdesc->params = estrdup(val);
		free(opt);
		STAILQ_INSERT_TAIL(&zfs->datasetdescs, dsdesc, next);
		return (1);
	}
	free(opt);

	rv = set_option(fsopts->fs_options, option, buf, sizeof(buf));
	return (rv == -1 ? 0 : 1);
}

static void
zfs_size_vdev(fsinfo_t *fsopts)
{
	zfs_opt_t *zfs;
	off_t asize, mssize, vdevsize, vdevsize1;

	zfs = fsopts->fs_specific;

	assert(fsopts->maxsize != 0);
	assert(zfs->ashift != 0);

	/*
	 * Figure out how big the vdev should be.
	 */
	vdevsize = rounddown2(fsopts->maxsize, 1 << zfs->ashift);
	if (vdevsize < MINDEVSIZE)
		errx(1, "maximum image size is too small");
	if (vdevsize < fsopts->minsize || vdevsize > fsopts->maxsize) {
		errx(1, "image size bounds must be multiples of %d",
		    1 << zfs->ashift);
	}
	asize = vdevsize - VDEV_LABEL_SPACE;

	/*
	 * Size metaslabs according to the following heuristic:
	 * - provide at least 8 metaslabs,
	 * - without using a metaslab size larger than 512MB.
	 * This approximates what OpenZFS does without being complicated.  In
	 * practice we expect pools to be expanded upon first use, and OpenZFS
	 * does not resize metaslabs in that case, so there is no right answer
	 * here.  In general we want to provide large metaslabs even if the
	 * image size is small, and 512MB is a reasonable size for pools up to
	 * several hundred gigabytes.
	 *
	 * The user may override this heuristic using the "-o mssize" option.
	 */
	mssize = zfs->mssize;
	if (mssize == 0) {
		mssize = MAX(MIN(asize / 8, DFLTMSSIZE), MINMSSIZE);
		if (!powerof2(mssize))
			mssize = 1l << (flsll(mssize) - 1);
	}
	if (!powerof2(mssize))
		errx(1, "metaslab size must be a power of 2");

	/*
	 * If we have some slop left over, try to cover it by resizing the vdev,
	 * subject to the maxsize and minsize parameters.
	 */
	if (asize % mssize != 0) {
		vdevsize1 = rounddown2(asize, mssize) + VDEV_LABEL_SPACE;
		if (vdevsize1 < fsopts->minsize)
			vdevsize1 = roundup2(asize, mssize) + VDEV_LABEL_SPACE;
		if (vdevsize1 <= fsopts->maxsize)
			vdevsize = vdevsize1;
	}
	asize = vdevsize - VDEV_LABEL_SPACE;

	zfs->asize = asize;
	zfs->vdevsize = vdevsize;
	zfs->mssize = mssize;
	zfs->msshift = flsll(mssize) - 1;
	zfs->mscount = asize / mssize;
}

/*
 * Validate options and set some default values.
 */
static void
zfs_check_opts(fsinfo_t *fsopts)
{
	zfs_opt_t *zfs;

	zfs = fsopts->fs_specific;

	if (fsopts->offset != 0)
		errx(1, "unhandled offset option");
	if (fsopts->maxsize == 0)
		errx(1, "an image size must be specified");

	if (zfs->poolname == NULL)
		errx(1, "a pool name must be specified");
	if (!isalpha(zfs->poolname[0]))
		errx(1, "the pool name must begin with a letter");
	for (size_t i = 0, len = strlen(zfs->poolname); i < len; i++) {
		if (!isalnum(zfs->poolname[i]) && zfs->poolname[i] != '_')
			errx(1, "invalid character '%c' in pool name",
			    zfs->poolname[i]);
	}
	if (strcmp(zfs->poolname, "mirror") == 0 ||
	    strcmp(zfs->poolname, "raidz") == 0 ||
	    strcmp(zfs->poolname, "draid") == 0) {
		errx(1, "pool name '%s' is reserved and cannot be used",
		    zfs->poolname);
	}

	if (zfs->rootpath == NULL)
		easprintf(&zfs->rootpath, "/%s", zfs->poolname);
	if (zfs->rootpath[0] != '/')
		errx(1, "mountpoint `%s' must be absolute", zfs->rootpath);

	if (zfs->ashift == 0)
		zfs->ashift = 12;

	zfs_size_vdev(fsopts);
}

void
zfs_cleanup_opts(fsinfo_t *fsopts)
{
	struct dataset_desc *d, *tmp;
	zfs_opt_t *zfs;

	zfs = fsopts->fs_specific;
	free(zfs->rootpath);
	free(zfs->bootfs);
	free(__DECONST(void *, zfs->poolname));
	STAILQ_FOREACH_SAFE(d, &zfs->datasetdescs, next, tmp) {
		free(d->params);
		free(d);
	}
	free(zfs);
	free(fsopts->fs_options);
}

static size_t
nvlist_size(const nvlist_t *nvl)
{
	return (sizeof(nvl->nv_header) + nvl->nv_size);
}

static void
nvlist_copy(const nvlist_t *nvl, char *buf, size_t sz)
{
	assert(sz >= nvlist_size(nvl));

	memcpy(buf, &nvl->nv_header, sizeof(nvl->nv_header));
	memcpy(buf + sizeof(nvl->nv_header), nvl->nv_data, nvl->nv_size);
}

static nvlist_t *
pool_config_nvcreate(zfs_opt_t *zfs)
{
	nvlist_t *featuresnv, *poolnv;

	poolnv = nvlist_create(NV_UNIQUE_NAME);
	nvlist_add_uint64(poolnv, ZPOOL_CONFIG_POOL_TXG, TXG);
	nvlist_add_uint64(poolnv, ZPOOL_CONFIG_VERSION, SPA_VERSION);
	nvlist_add_uint64(poolnv, ZPOOL_CONFIG_POOL_STATE, POOL_STATE_EXPORTED);
	nvlist_add_string(poolnv, ZPOOL_CONFIG_POOL_NAME, zfs->poolname);
	nvlist_add_uint64(poolnv, ZPOOL_CONFIG_POOL_GUID, zfs->poolguid);
	nvlist_add_uint64(poolnv, ZPOOL_CONFIG_TOP_GUID, zfs->vdevguid);
	nvlist_add_uint64(poolnv, ZPOOL_CONFIG_GUID, zfs->vdevguid);
	nvlist_add_uint64(poolnv, ZPOOL_CONFIG_VDEV_CHILDREN, 1);

	featuresnv = nvlist_create(NV_UNIQUE_NAME);
	nvlist_add_nvlist(poolnv, ZPOOL_CONFIG_FEATURES_FOR_READ, featuresnv);
	nvlist_destroy(featuresnv);

	return (poolnv);
}

static nvlist_t *
pool_disk_vdev_config_nvcreate(zfs_opt_t *zfs)
{
	nvlist_t *diskvdevnv;

	assert(zfs->objarrid != 0);

	diskvdevnv = nvlist_create(NV_UNIQUE_NAME);
	nvlist_add_string(diskvdevnv, ZPOOL_CONFIG_TYPE, VDEV_TYPE_DISK);
	nvlist_add_uint64(diskvdevnv, ZPOOL_CONFIG_ASHIFT, zfs->ashift);
	nvlist_add_uint64(diskvdevnv, ZPOOL_CONFIG_ASIZE, zfs->asize);
	nvlist_add_uint64(diskvdevnv, ZPOOL_CONFIG_GUID, zfs->vdevguid);
	nvlist_add_uint64(diskvdevnv, ZPOOL_CONFIG_ID, 0);
	nvlist_add_string(diskvdevnv, ZPOOL_CONFIG_PATH, "/dev/null");
	nvlist_add_uint64(diskvdevnv, ZPOOL_CONFIG_WHOLE_DISK, 1);
	nvlist_add_uint64(diskvdevnv, ZPOOL_CONFIG_CREATE_TXG, TXG);
	nvlist_add_uint64(diskvdevnv, ZPOOL_CONFIG_METASLAB_ARRAY,
	    zfs->objarrid);
	nvlist_add_uint64(diskvdevnv, ZPOOL_CONFIG_METASLAB_SHIFT,
	    zfs->msshift);

	return (diskvdevnv);
}

static nvlist_t *
pool_root_vdev_config_nvcreate(zfs_opt_t *zfs)
{
	nvlist_t *diskvdevnv, *rootvdevnv;

	diskvdevnv = pool_disk_vdev_config_nvcreate(zfs);
	rootvdevnv = nvlist_create(NV_UNIQUE_NAME);

	nvlist_add_uint64(rootvdevnv, ZPOOL_CONFIG_ID, 0);
	nvlist_add_uint64(rootvdevnv, ZPOOL_CONFIG_GUID, zfs->poolguid);
	nvlist_add_string(rootvdevnv, ZPOOL_CONFIG_TYPE, VDEV_TYPE_ROOT);
	nvlist_add_uint64(rootvdevnv, ZPOOL_CONFIG_CREATE_TXG, TXG);
	nvlist_add_nvlist_array(rootvdevnv, ZPOOL_CONFIG_CHILDREN, &diskvdevnv,
	    1);
	nvlist_destroy(diskvdevnv);

	return (rootvdevnv);
}

/*
 * Create the pool's "config" object, which contains an nvlist describing pool
 * parameters and the vdev topology.  It is similar but not identical to the
 * nvlist stored in vdev labels.  The main difference is that vdev labels do not
 * describe the full vdev tree and in particular do not contain the "root"
 * meta-vdev.
 */
static void
pool_init_objdir_config(zfs_opt_t *zfs, zfs_zap_t *objdir)
{
	dnode_phys_t *dnode;
	nvlist_t *poolconfig, *vdevconfig;
	void *configbuf;
	uint64_t dnid;
	off_t configloc, configblksz;
	int error;

	dnode = objset_dnode_bonus_alloc(zfs->mos, DMU_OT_PACKED_NVLIST,
	    DMU_OT_PACKED_NVLIST_SIZE, sizeof(uint64_t), &dnid);

	poolconfig = pool_config_nvcreate(zfs);

	vdevconfig = pool_root_vdev_config_nvcreate(zfs);
	nvlist_add_nvlist(poolconfig, ZPOOL_CONFIG_VDEV_TREE, vdevconfig);
	nvlist_destroy(vdevconfig);

	error = nvlist_export(poolconfig);
	if (error != 0)
		errc(1, error, "nvlist_export");

	configblksz = nvlist_size(poolconfig);
	configloc = objset_space_alloc(zfs, zfs->mos, &configblksz);
	configbuf = ecalloc(1, configblksz);
	nvlist_copy(poolconfig, configbuf, configblksz);

	vdev_pwrite_dnode_data(zfs, dnode, configbuf, configblksz, configloc);

	dnode->dn_datablkszsec = configblksz >> MINBLOCKSHIFT;
	dnode->dn_flags = DNODE_FLAG_USED_BYTES;
	*(uint64_t *)DN_BONUS(dnode) = nvlist_size(poolconfig);

	zap_add_uint64(objdir, DMU_POOL_CONFIG, dnid);

	nvlist_destroy(poolconfig);
	free(configbuf);
}

/*
 * Add objects block pointer list objects, used for deferred frees.  We don't do
 * anything with them, but they need to be present or OpenZFS will refuse to
 * import the pool.
 */
static void
pool_init_objdir_bplists(zfs_opt_t *zfs __unused, zfs_zap_t *objdir)
{
	uint64_t dnid;

	(void)objset_dnode_bonus_alloc(zfs->mos, DMU_OT_BPOBJ, DMU_OT_BPOBJ_HDR,
	    BPOBJ_SIZE_V2, &dnid);
	zap_add_uint64(objdir, DMU_POOL_FREE_BPOBJ, dnid);

	(void)objset_dnode_bonus_alloc(zfs->mos, DMU_OT_BPOBJ, DMU_OT_BPOBJ_HDR,
	    BPOBJ_SIZE_V2, &dnid);
	zap_add_uint64(objdir, DMU_POOL_SYNC_BPLIST, dnid);
}

/*
 * Add required feature metadata objects.  We don't know anything about ZFS
 * features, so the objects are just empty ZAPs.
 */
static void
pool_init_objdir_feature_maps(zfs_opt_t *zfs, zfs_zap_t *objdir)
{
	dnode_phys_t *dnode;
	uint64_t dnid;

	dnode = objset_dnode_alloc(zfs->mos, DMU_OTN_ZAP_METADATA, &dnid);
	zap_add_uint64(objdir, DMU_POOL_FEATURES_FOR_READ, dnid);
	zap_write(zfs, zap_alloc(zfs->mos, dnode));

	dnode = objset_dnode_alloc(zfs->mos, DMU_OTN_ZAP_METADATA, &dnid);
	zap_add_uint64(objdir, DMU_POOL_FEATURES_FOR_WRITE, dnid);
	zap_write(zfs, zap_alloc(zfs->mos, dnode));

	dnode = objset_dnode_alloc(zfs->mos, DMU_OTN_ZAP_METADATA, &dnid);
	zap_add_uint64(objdir, DMU_POOL_FEATURE_DESCRIPTIONS, dnid);
	zap_write(zfs, zap_alloc(zfs->mos, dnode));
}

static void
pool_init_objdir_dsl(zfs_opt_t *zfs, zfs_zap_t *objdir)
{
	zap_add_uint64(objdir, DMU_POOL_ROOT_DATASET,
	    dsl_dir_id(zfs->rootdsldir));
}

static void
pool_init_objdir_poolprops(zfs_opt_t *zfs, zfs_zap_t *objdir)
{
	dnode_phys_t *dnode;
	uint64_t id;

	dnode = objset_dnode_alloc(zfs->mos, DMU_OT_POOL_PROPS, &id);
	zap_add_uint64(objdir, DMU_POOL_PROPS, id);

	zfs->poolprops = zap_alloc(zfs->mos, dnode);
}

/*
 * Initialize the MOS object directory, the root of virtually all of the pool's
 * data and metadata.
 */
static void
pool_init_objdir(zfs_opt_t *zfs)
{
	zfs_zap_t *zap;
	dnode_phys_t *objdir;

	objdir = objset_dnode_lookup(zfs->mos, DMU_POOL_DIRECTORY_OBJECT);

	zap = zap_alloc(zfs->mos, objdir);
	pool_init_objdir_config(zfs, zap);
	pool_init_objdir_bplists(zfs, zap);
	pool_init_objdir_feature_maps(zfs, zap);
	pool_init_objdir_dsl(zfs, zap);
	pool_init_objdir_poolprops(zfs, zap);
	zap_write(zfs, zap);
}

/*
 * Initialize the meta-object set (MOS) and immediately write out several
 * special objects whose contents are already finalized, including the object
 * directory.
 *
 * Once the MOS is finalized, it'll look roughly like this:
 *
 *	object directory (ZAP)
 *	|-> vdev config object (nvlist)
 *	|-> features for read
 *	|-> features for write
 *	|-> feature descriptions
 *	|-> sync bplist
 *	|-> free bplist
 *	|-> pool properties
 *	L-> root DSL directory
 *	    |-> DSL child directory (ZAP)
 *	    |   |-> $MOS (DSL dir)
 *	    |   |   |-> child map
 *	    |   |   L-> props (ZAP)
 *	    |   |-> $FREE (DSL dir)
 *	    |   |   |-> child map
 *	    |   |   L-> props (ZAP)
 *	    |   |-> $ORIGIN (DSL dir)
 *	    |   |   |-> child map
 *	    |   |   |-> dataset
 *	    |   |   |   L-> deadlist
 *	    |   |   |-> snapshot
 *	    |   |   |   |-> deadlist
 *	    |   |   |   L-> snapshot names
 *	    |   |   |-> props (ZAP)
 *	    |   |   L-> clones (ZAP)
 *	    |   |-> dataset 1 (DSL dir)
 *	    |   |   |-> DSL dataset
 *	    |   |   |   |-> snapshot names
 *	    |   |   |   L-> deadlist
 *	    |   |   |-> child map
 *	    |   |   |   L-> ...
 *	    |   |   L-> props
 *	    |   |-> dataset 2
 *	    |   |   L-> ...
 *	    |   |-> ...
 *	    |   L-> dataset n
 *	    |-> DSL root dataset
 *	    |   |-> snapshot names
 *	    |   L-> deadlist
 *	    L-> props (ZAP)
 *	space map object array
 *	|-> space map 1
 *	|-> space map 2
 *	|-> ...
 *	L-> space map n (zfs->mscount)
 *
 * The space map object array is pointed to by the "msarray" property in the
 * pool configuration.
 */
static void
pool_init(zfs_opt_t *zfs)
{
	uint64_t dnid;

	zfs->poolguid = ((uint64_t)random() << 32) | random();
	zfs->vdevguid = ((uint64_t)random() << 32) | random();

	zfs->mos = objset_alloc(zfs, DMU_OST_META);

	(void)objset_dnode_alloc(zfs->mos, DMU_OT_OBJECT_DIRECTORY, &dnid);
	assert(dnid == DMU_POOL_DIRECTORY_OBJECT);

	(void)objset_dnode_alloc(zfs->mos, DMU_OT_OBJECT_ARRAY, &zfs->objarrid);

	dsl_init(zfs);

	pool_init_objdir(zfs);
}

static void
pool_labels_write(zfs_opt_t *zfs)
{
	uberblock_t *ub;
	vdev_label_t *label;
	nvlist_t *poolconfig, *vdevconfig;
	int error;

	label = ecalloc(1, sizeof(*label));

	/*
	 * Assemble the vdev configuration and store it in the label.
	 */
	poolconfig = pool_config_nvcreate(zfs);
	vdevconfig = pool_disk_vdev_config_nvcreate(zfs);
	nvlist_add_nvlist(poolconfig, ZPOOL_CONFIG_VDEV_TREE, vdevconfig);
	nvlist_destroy(vdevconfig);

	error = nvlist_export(poolconfig);
	if (error != 0)
		errc(1, error, "nvlist_export");
	nvlist_copy(poolconfig, label->vl_vdev_phys.vp_nvlist,
	    sizeof(label->vl_vdev_phys.vp_nvlist));
	nvlist_destroy(poolconfig);

	/*
	 * Fill out the uberblock.  Just make each one the same.  The embedded
	 * checksum is calculated in vdev_label_write().
	 */
	for (size_t uoff = 0; uoff < sizeof(label->vl_uberblock);
	    uoff += (1 << zfs->ashift)) {
		ub = (uberblock_t *)(&label->vl_uberblock[0] + uoff);
		ub->ub_magic = UBERBLOCK_MAGIC;
		ub->ub_version = SPA_VERSION;
		ub->ub_txg = TXG;
		ub->ub_guid_sum = zfs->poolguid + zfs->vdevguid;
		ub->ub_timestamp = 0;

		ub->ub_software_version = SPA_VERSION;
		ub->ub_mmp_magic = MMP_MAGIC;
		ub->ub_mmp_delay = 0;
		ub->ub_mmp_config = 0;
		ub->ub_checkpoint_txg = 0;
		objset_root_blkptr_copy(zfs->mos, &ub->ub_rootbp);
	}

	/*
	 * Write out four copies of the label: two at the beginning of the vdev
	 * and two at the end.
	 */
	for (int i = 0; i < VDEV_LABELS; i++)
		vdev_label_write(zfs, i, label);

	free(label);
}

static void
pool_fini(zfs_opt_t *zfs)
{
	zap_write(zfs, zfs->poolprops);
	dsl_write(zfs);
	objset_write(zfs, zfs->mos);
	pool_labels_write(zfs);
}

struct dnode_cursor *
dnode_cursor_init(zfs_opt_t *zfs, zfs_objset_t *os, dnode_phys_t *dnode,
    off_t size, off_t blksz)
{
	struct dnode_cursor *c;
	uint64_t nbppindir, indlevel, ndatablks, nindblks;

	assert(dnode->dn_nblkptr == 1);
	assert(blksz <= MAXBLOCKSIZE);

	if (blksz == 0) {
		/* Must be between 1<<ashift and 128KB. */
		blksz = MIN(MAXBLOCKSIZE, MAX(1 << zfs->ashift,
		    powerof2(size) ? size : (1l << flsll(size))));
	}
	assert(powerof2(blksz));

	/*
	 * Do we need indirect blocks?  Figure out how many levels are needed
	 * (indlevel == 1 means no indirect blocks) and how much space is needed
	 * (it has to be allocated up-front to break the dependency cycle
	 * described in objset_write()).
	 */
	ndatablks = size == 0 ? 0 : howmany(size, blksz);
	nindblks = 0;
	for (indlevel = 1, nbppindir = 1; ndatablks > nbppindir; indlevel++) {
		nbppindir *= BLKPTR_PER_INDIR;
		nindblks += howmany(ndatablks, indlevel * nbppindir);
	}
	assert(indlevel < INDIR_LEVELS);

	dnode->dn_nlevels = (uint8_t)indlevel;
	dnode->dn_maxblkid = ndatablks > 0 ? ndatablks - 1 : 0;
	dnode->dn_datablkszsec = blksz >> MINBLOCKSHIFT;

	c = ecalloc(1, sizeof(*c));
	if (nindblks > 0) {
		c->indspace = nindblks * MAXBLOCKSIZE;
		c->indloc = objset_space_alloc(zfs, os, &c->indspace);
	}
	c->dnode = dnode;
	c->dataoff = 0;
	c->datablksz = blksz;

	return (c);
}

static void
_dnode_cursor_flush(zfs_opt_t *zfs, struct dnode_cursor *c, int levels)
{
	blkptr_t *bp, *pbp;
	void *buf;
	uint64_t fill;
	off_t blkid, blksz, loc;

	assert(levels > 0);
	assert(levels <= c->dnode->dn_nlevels - 1);

	blksz = MAXBLOCKSIZE;
	blkid = (c->dataoff / c->datablksz) / BLKPTR_PER_INDIR;
	for (int level = 1; level <= levels; level++) {
		buf = c->inddir[level - 1];

		if (level == c->dnode->dn_nlevels - 1) {
			pbp = &c->dnode->dn_blkptr[0];
		} else {
			uint64_t iblkid;

			iblkid = blkid & (BLKPTR_PER_INDIR - 1);
			pbp = (blkptr_t *)
			    &c->inddir[level][iblkid * sizeof(blkptr_t)];
		}

		/*
		 * Space for indirect blocks is allocated up-front; see the
		 * comment in objset_write().
		 */
		loc = c->indloc;
		c->indloc += blksz;
		assert(c->indspace >= blksz);
		c->indspace -= blksz;

		bp = buf;
		fill = 0;
		for (size_t i = 0; i < BLKPTR_PER_INDIR; i++)
			fill += BP_GET_FILL(&bp[i]);

		vdev_pwrite_dnode_indir(zfs, c->dnode, level, fill, buf, blksz,
		    loc, pbp);
		memset(buf, 0, MAXBLOCKSIZE);

		blkid /= BLKPTR_PER_INDIR;
	}
}

blkptr_t *
dnode_cursor_next(zfs_opt_t *zfs, struct dnode_cursor *c, off_t off)
{
	off_t blkid, l1id;
	int levels;

	if (c->dnode->dn_nlevels == 1) {
		assert(off < MAXBLOCKSIZE);
		return (&c->dnode->dn_blkptr[0]);
	}

	assert(off % c->datablksz == 0);

	/* Do we need to flush any full indirect blocks? */
	if (off > 0) {
		blkid = off / c->datablksz;
		for (levels = 0; levels < c->dnode->dn_nlevels - 1; levels++) {
			if (blkid % BLKPTR_PER_INDIR != 0)
				break;
			blkid /= BLKPTR_PER_INDIR;
		}
		if (levels > 0)
			_dnode_cursor_flush(zfs, c, levels);
	}

	c->dataoff = off;
	l1id = (off / c->datablksz) & (BLKPTR_PER_INDIR - 1);
	return ((blkptr_t *)&c->inddir[0][l1id * sizeof(blkptr_t)]);
}

void
dnode_cursor_finish(zfs_opt_t *zfs, struct dnode_cursor *c)
{
	int levels;

	levels = c->dnode->dn_nlevels - 1;
	if (levels > 0)
		_dnode_cursor_flush(zfs, c, levels);
	assert(c->indspace == 0);
	free(c);
}

void
zfs_makefs(const char *image, const char *dir, fsnode *root, fsinfo_t *fsopts)
{
	zfs_opt_t *zfs;
	int dirfd;

	zfs = fsopts->fs_specific;

	/*
	 * Use a fixed seed to provide reproducible pseudo-random numbers for
	 * on-disk structures when needed (e.g., GUIDs, ZAP hash salts).
	 */
	srandom(1729);

	zfs_check_opts(fsopts);

	if (!zfs->nowarn) {
		fprintf(stderr,
		    "ZFS support is currently considered experimental. "
		    "Do not use it for anything critical.\n");
	}

	dirfd = open(dir, O_DIRECTORY | O_RDONLY);
	if (dirfd < 0)
		err(1, "open(%s)", dir);

	vdev_init(zfs, image);
	pool_init(zfs);
	fs_build(zfs, dirfd, root);
	pool_fini(zfs);
	vdev_fini(zfs);
}
