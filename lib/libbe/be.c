/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Kyle J. Kneitinger <kyle@kneit.in>
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

#include <sys/param.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/queue.h>
#include <sys/zfs_context.h>
#include <sys/mntent.h>
#include <sys/zfs_ioctl.h>

#include <libzutil.h>
#include <ctype.h>
#include <libgen.h>
#include <libzfs_core.h>
#include <libzfs_impl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <libzfsbootenv.h>

#include "be.h"
#include "be_impl.h"

struct promote_entry {
	char				name[BE_MAXPATHLEN];
	SLIST_ENTRY(promote_entry)	link;
};

struct be_destroy_data {
	libbe_handle_t			*lbh;
	char				target_name[BE_MAXPATHLEN];
	char				*snapname;
	SLIST_HEAD(, promote_entry)	promotelist;
};

#if SOON
static int be_create_child_noent(libbe_handle_t *lbh, const char *active,
    const char *child_path);
static int be_create_child_cloned(libbe_handle_t *lbh, const char *active);
#endif

/* Arbitrary... should tune */
#define	BE_SNAP_SERIAL_MAX	1024

/*
 * Iterator function for locating the rootfs amongst the children of the
 * zfs_be_root set by loader(8).  data is expected to be a libbe_handle_t *.
 */
static int
be_locate_rootfs(libbe_handle_t *lbh)
{
	struct statfs sfs;
	struct mnttab entry;
	zfs_handle_t *zfs;

	/*
	 * Check first if root is ZFS; if not, we'll bail on rootfs capture.
	 * Unfortunately needed because zfs_path_to_zhandle will emit to
	 * stderr if / isn't actually a ZFS filesystem, which we'd like
	 * to avoid.
	 */
	if (statfs("/", &sfs) == 0) {
		statfs2mnttab(&sfs, &entry);
		if (strcmp(entry.mnt_fstype, MNTTYPE_ZFS) != 0)
			return (1);
	} else
		return (1);
	zfs = zfs_path_to_zhandle(lbh->lzh, "/", ZFS_TYPE_FILESYSTEM);
	if (zfs == NULL)
		return (1);

	strlcpy(lbh->rootfs, zfs_get_name(zfs), sizeof(lbh->rootfs));
	zfs_close(zfs);
	return (0);
}

/*
 * Initializes the libbe context to operate in the root boot environment
 * dataset, for example, zroot/ROOT.
 */
libbe_handle_t *
libbe_init(const char *root)
{
	char altroot[MAXPATHLEN];
	libbe_handle_t *lbh;
	char *poolname, *pos;
	int pnamelen;

	lbh = NULL;
	poolname = pos = NULL;

	/*
	 * If the zfs kmod's not loaded then the later libzfs_init() will load
	 * the module for us, but that's not desirable for a couple reasons.  If
	 * the module's not loaded, there's no pool imported and we're going to
	 * fail anyways.  We also don't really want libbe consumers to have that
	 * kind of side-effect (module loading) in the general case.
	 */
	if (modfind("zfs") < 0)
		goto err;

	if ((lbh = calloc(1, sizeof(libbe_handle_t))) == NULL)
		goto err;

	if ((lbh->lzh = libzfs_init()) == NULL)
		goto err;

	/*
	 * Grab rootfs, we'll work backwards from there if an optional BE root
	 * has not been passed in.
	 */
	if (be_locate_rootfs(lbh) != 0) {
		if (root == NULL)
			goto err;
		*lbh->rootfs = '\0';
	}
	if (root == NULL) {
		/* Strip off the final slash from rootfs to get the be root */
		strlcpy(lbh->root, lbh->rootfs, sizeof(lbh->root));
		pos = strrchr(lbh->root, '/');
		if (pos == NULL)
			goto err;
		*pos = '\0';
	} else
		strlcpy(lbh->root, root, sizeof(lbh->root));

	if ((pos = strchr(lbh->root, '/')) == NULL)
		goto err;

	pnamelen = pos - lbh->root;
	poolname = malloc(pnamelen + 1);
	if (poolname == NULL)
		goto err;

	strlcpy(poolname, lbh->root, pnamelen + 1);
	if ((lbh->active_phandle = zpool_open(lbh->lzh, poolname)) == NULL)
		goto err;
	free(poolname);
	poolname = NULL;

	if (zpool_get_prop(lbh->active_phandle, ZPOOL_PROP_BOOTFS, lbh->bootfs,
	    sizeof(lbh->bootfs), NULL, true) != 0)
		goto err;

	if (zpool_get_prop(lbh->active_phandle, ZPOOL_PROP_ALTROOT,
	    altroot, sizeof(altroot), NULL, true) == 0 &&
	    strcmp(altroot, "-") != 0)
		lbh->altroot_len = strlen(altroot);

	(void) lzbe_get_boot_device(zpool_get_name(lbh->active_phandle),
	    &lbh->bootonce);

	return (lbh);
err:
	if (lbh != NULL) {
		if (lbh->active_phandle != NULL)
			zpool_close(lbh->active_phandle);
		if (lbh->lzh != NULL)
			libzfs_fini(lbh->lzh);
		free(lbh);
	}
	free(poolname);
	return (NULL);
}


/*
 * Free memory allocated by libbe_init()
 */
void
libbe_close(libbe_handle_t *lbh)
{

	if (lbh->active_phandle != NULL)
		zpool_close(lbh->active_phandle);
	libzfs_fini(lbh->lzh);

	free(lbh->bootonce);
	free(lbh);
}

/*
 * Proxy through to libzfs for the moment.
 */
void
be_nicenum(uint64_t num, char *buf, size_t buflen)
{

	zfs_nicenum(num, buf, buflen);
}

static bool
be_should_promote_clones(zfs_handle_t *zfs_hdl, struct be_destroy_data *bdd)
{
	char *atpos;

	if (zfs_get_type(zfs_hdl) != ZFS_TYPE_SNAPSHOT)
		return (false);

	/*
	 * If we're deleting a snapshot, we need to make sure we only promote
	 * clones that are derived from one of the snapshots we're deleting,
	 * rather than that of a snapshot we're not touching.  This keeps stuff
	 * in a consistent state, making sure that we don't error out unless
	 * we really need to.
	 */
	if (bdd->snapname == NULL)
		return (true);

	atpos = strchr(zfs_get_name(zfs_hdl), '@');
	return (strcmp(atpos + 1, bdd->snapname) == 0);
}

/*
 * This is executed from be_promote_dependent_clones via zfs_iter_dependents,
 * It checks if the dependent type is a snapshot then attempts to find any
 * clones associated with it. Any clones not related to the destroy target are
 * added to the promote list.
 */
static int
be_dependent_clone_cb(zfs_handle_t *zfs_hdl, void *data)
{
	int err;
	bool found;
	const char *name;
	struct nvlist *nvl;
	struct nvpair *nvp;
	struct be_destroy_data *bdd;
	struct promote_entry *entry, *newentry;

	nvp = NULL;
	err = 0;
	bdd = (struct be_destroy_data *)data;

	if (be_should_promote_clones(zfs_hdl, bdd) &&
	    (nvl = zfs_get_clones_nvl(zfs_hdl)) != NULL) {
		while ((nvp = nvlist_next_nvpair(nvl, nvp)) != NULL) {
			name = nvpair_name(nvp);

			/*
			 * Skip if the clone is equal to, or a child of, the
			 * destroy target.
			 */
			if (strncmp(name, bdd->target_name,
			    strlen(bdd->target_name)) == 0 ||
			    strstr(name, bdd->target_name) == name) {
				continue;
			}

			found = false;
			SLIST_FOREACH(entry, &bdd->promotelist, link) {
				if (strcmp(entry->name, name) == 0) {
					found = true;
					break;
				}
			}

			if (found)
				continue;

			newentry = malloc(sizeof(struct promote_entry));
			if (newentry == NULL) {
				err = ENOMEM;
				break;
			}

#define	BE_COPY_NAME(entry, src)	\
	strlcpy((entry)->name, (src), sizeof((entry)->name))
			if (BE_COPY_NAME(newentry, name) >=
			    sizeof(newentry->name)) {
				/* Shouldn't happen. */
				free(newentry);
				err = ENAMETOOLONG;
				break;
			}
#undef BE_COPY_NAME

			/*
			 * We're building up a SLIST here to make sure both that
			 * we get the order right and so that we don't
			 * inadvertently observe the wrong state by promoting
			 * datasets while we're still walking the tree.  The
			 * latter can lead to situations where we promote a BE
			 * then effectively demote it again.
			 */
			SLIST_INSERT_HEAD(&bdd->promotelist, newentry, link);
		}
		nvlist_free(nvl);
	}
	zfs_close(zfs_hdl);
	return (err);
}

/*
 * This is called before a destroy, so that any datasets(environments) that are
 * dependent on this one get promoted before destroying the target.
 */
static int
be_promote_dependent_clones(zfs_handle_t *zfs_hdl, struct be_destroy_data *bdd)
{
	int err;
	zfs_handle_t *clone;
	struct promote_entry *entry;

	snprintf(bdd->target_name, BE_MAXPATHLEN, "%s/", zfs_get_name(zfs_hdl));
	err = zfs_iter_dependents(zfs_hdl, true, be_dependent_clone_cb, bdd);

	/*
	 * Drain the list and walk away from it if we're only deleting a
	 * snapshot.
	 */
	if (bdd->snapname != NULL && !SLIST_EMPTY(&bdd->promotelist))
		err = BE_ERR_HASCLONES;
	while (!SLIST_EMPTY(&bdd->promotelist)) {
		entry = SLIST_FIRST(&bdd->promotelist);
		SLIST_REMOVE_HEAD(&bdd->promotelist, link);

#define	ZFS_GRAB_CLONE()	\
	zfs_open(bdd->lbh->lzh, entry->name, ZFS_TYPE_FILESYSTEM)
		/*
		 * Just skip this part on error, we still want to clean up the
		 * promotion list after the first error.  We'll then preserve it
		 * all the way back.
		 */
		if (err == 0 && (clone = ZFS_GRAB_CLONE()) != NULL) {
			err = zfs_promote(clone);
			if (err != 0)
				err = BE_ERR_DESTROYMNT;
			zfs_close(clone);
		}
#undef ZFS_GRAB_CLONE
		free(entry);
	}

	return (err);
}

static int
be_destroy_cb(zfs_handle_t *zfs_hdl, void *data)
{
	char path[BE_MAXPATHLEN];
	struct be_destroy_data *bdd;
	zfs_handle_t *snap;
	int err;

	bdd = (struct be_destroy_data *)data;
	if (bdd->snapname == NULL) {
		err = zfs_iter_children(zfs_hdl, be_destroy_cb, data);
		if (err != 0)
			return (err);
		return (zfs_destroy(zfs_hdl, false));
	}
	/* If we're dealing with snapshots instead, delete that one alone */
	err = zfs_iter_filesystems(zfs_hdl, be_destroy_cb, data);
	if (err != 0)
		return (err);
	/*
	 * This part is intentionally glossing over any potential errors,
	 * because there's a lot less potential for errors when we're cleaning
	 * up snapshots rather than a full deep BE.  The primary error case
	 * here being if the snapshot doesn't exist in the first place, which
	 * the caller will likely deem insignificant as long as it doesn't
	 * exist after the call.  Thus, such a missing snapshot shouldn't jam
	 * up the destruction.
	 */
	snprintf(path, sizeof(path), "%s@%s", zfs_get_name(zfs_hdl),
	    bdd->snapname);
	if (!zfs_dataset_exists(bdd->lbh->lzh, path, ZFS_TYPE_SNAPSHOT))
		return (0);
	snap = zfs_open(bdd->lbh->lzh, path, ZFS_TYPE_SNAPSHOT);
	if (snap != NULL)
		zfs_destroy(snap, false);
	return (0);
}

#define	BE_DESTROY_WANTORIGIN	(BE_DESTROY_ORIGIN | BE_DESTROY_AUTOORIGIN)
/*
 * Destroy the boot environment or snapshot specified by the name
 * parameter. Options are or'd together with the possible values:
 * BE_DESTROY_FORCE : forces operation on mounted datasets
 * BE_DESTROY_ORIGIN: destroy the origin snapshot as well
 */
static int
be_destroy_internal(libbe_handle_t *lbh, const char *name, int options,
    bool odestroyer)
{
	struct be_destroy_data bdd;
	char origin[BE_MAXPATHLEN], path[BE_MAXPATHLEN];
	zfs_handle_t *fs;
	char *snapdelim;
	int err, force, mounted;
	size_t rootlen;

	bdd.lbh = lbh;
	bdd.snapname = NULL;
	SLIST_INIT(&bdd.promotelist);
	force = options & BE_DESTROY_FORCE;
	*origin = '\0';

	be_root_concat(lbh, name, path);

	if ((snapdelim = strchr(path, '@')) == NULL) {
		if (!zfs_dataset_exists(lbh->lzh, path, ZFS_TYPE_FILESYSTEM))
			return (set_error(lbh, BE_ERR_NOENT));

		if (strcmp(path, lbh->rootfs) == 0 ||
		    strcmp(path, lbh->bootfs) == 0)
			return (set_error(lbh, BE_ERR_DESTROYACT));

		fs = zfs_open(lbh->lzh, path, ZFS_TYPE_FILESYSTEM);
		if (fs == NULL)
			return (set_error(lbh, BE_ERR_ZFSOPEN));

		/* Don't destroy a mounted dataset unless force is specified */
		if ((mounted = zfs_is_mounted(fs, NULL)) != 0) {
			if (force) {
				zfs_unmount(fs, NULL, 0);
			} else {
				free(bdd.snapname);
				return (set_error(lbh, BE_ERR_DESTROYMNT));
			}
		}

		/* Handle destroying bootonce */
		if (lbh->bootonce != NULL &&
		    strcmp(path, lbh->bootonce) == 0)
			(void) lzbe_set_boot_device(
			    zpool_get_name(lbh->active_phandle), lzbe_add, NULL);
	} else {
		/*
		 * If we're initially destroying a snapshot, origin options do
		 * not make sense.  If we're destroying the origin snapshot of
		 * a BE, we want to maintain the options in case we need to
		 * fake success after failing to promote.
		 */
		if (!odestroyer)
			options &= ~BE_DESTROY_WANTORIGIN;
		if (!zfs_dataset_exists(lbh->lzh, path, ZFS_TYPE_SNAPSHOT))
			return (set_error(lbh, BE_ERR_NOENT));

		bdd.snapname = strdup(snapdelim + 1);
		if (bdd.snapname == NULL)
			return (set_error(lbh, BE_ERR_NOMEM));
		*snapdelim = '\0';
		fs = zfs_open(lbh->lzh, path, ZFS_TYPE_DATASET);
		if (fs == NULL) {
			free(bdd.snapname);
			return (set_error(lbh, BE_ERR_ZFSOPEN));
		}
	}

	/*
	 * Whether we're destroying a BE or a single snapshot, we need to walk
	 * the tree of what we're going to destroy and promote everything in our
	 * path so that we can make it happen.
	 */
	if ((err = be_promote_dependent_clones(fs, &bdd)) != 0) {
		free(bdd.snapname);

		/*
		 * If we're just destroying the origin of some other dataset
		 * we were invoked to destroy, then we just ignore
		 * BE_ERR_HASCLONES and return success unless the caller wanted
		 * to force the issue.
		 */
		if (odestroyer && err == BE_ERR_HASCLONES &&
		    (options & BE_DESTROY_AUTOORIGIN) != 0)
			return (0);
		return (set_error(lbh, err));
	}

	/*
	 * This was deferred until after we promote all of the derivatives so
	 * that we grab the new origin after everything's settled down.
	 */
	if ((options & BE_DESTROY_WANTORIGIN) != 0 &&
	    zfs_prop_get(fs, ZFS_PROP_ORIGIN, origin, sizeof(origin),
	    NULL, NULL, 0, 1) != 0 &&
	    (options & BE_DESTROY_ORIGIN) != 0)
		return (set_error(lbh, BE_ERR_NOORIGIN));

	/*
	 * If the caller wants auto-origin destruction and the origin
	 * name matches one of our automatically created snapshot names
	 * (i.e. strftime("%F-%T") with a serial at the end), then
	 * we'll set the DESTROY_ORIGIN flag and nuke it
	 * be_is_auto_snapshot_name is exported from libbe(3) so that
	 * the caller can determine if it needs to warn about the origin
	 * not being destroyed or not.
	 */
	if ((options & BE_DESTROY_AUTOORIGIN) != 0 && *origin != '\0' &&
	    be_is_auto_snapshot_name(lbh, origin))
		options |= BE_DESTROY_ORIGIN;

	err = be_destroy_cb(fs, &bdd);
	zfs_close(fs);
	free(bdd.snapname);
	if (err != 0) {
		/* Children are still present or the mount is referenced */
		if (err == EBUSY)
			return (set_error(lbh, BE_ERR_DESTROYMNT));
		return (set_error(lbh, BE_ERR_UNKNOWN));
	}

	if ((options & BE_DESTROY_ORIGIN) == 0)
		return (0);

	/* The origin can't possibly be shorter than the BE root */
	rootlen = strlen(lbh->root);
	if (*origin == '\0' || strlen(origin) <= rootlen + 1)
		return (set_error(lbh, BE_ERR_INVORIGIN));

	/*
	 * We'll be chopping off the BE root and running this back through
	 * be_destroy, so that we properly handle the origin snapshot whether
	 * it be that of a deep BE or not.
	 */
	if (strncmp(origin, lbh->root, rootlen) != 0 || origin[rootlen] != '/')
		return (0);

	return (be_destroy_internal(lbh, origin + rootlen + 1,
	    options & ~BE_DESTROY_ORIGIN, true));
}

int
be_destroy(libbe_handle_t *lbh, const char *name, int options)
{

	/*
	 * The consumer must not set both BE_DESTROY_AUTOORIGIN and
	 * BE_DESTROY_ORIGIN.  Internally, we'll set the latter from the former.
	 * The latter should imply that we must succeed at destroying the
	 * origin, or complain otherwise.
	 */
	if ((options & BE_DESTROY_WANTORIGIN) == BE_DESTROY_WANTORIGIN)
		return (set_error(lbh, BE_ERR_UNKNOWN));
	return (be_destroy_internal(lbh, name, options, false));
}

static void
be_setup_snapshot_name(libbe_handle_t *lbh, char *buf, size_t buflen)
{
	time_t rawtime;
	int len, serial;

	time(&rawtime);
	len = strlen(buf);
	len += strftime(buf + len, buflen - len, "@%F-%T", localtime(&rawtime));
	/* No room for serial... caller will do its best */
	if (buflen - len < 2)
		return;

	for (serial = 0; serial < BE_SNAP_SERIAL_MAX; ++serial) {
		snprintf(buf + len, buflen - len, "-%d", serial);
		if (!zfs_dataset_exists(lbh->lzh, buf, ZFS_TYPE_SNAPSHOT))
			return;
	}
}

bool
be_is_auto_snapshot_name(libbe_handle_t *lbh __unused, const char *name)
{
	const char *snap;
	int day, hour, minute, month, second, serial, year;

	if ((snap = strchr(name, '@')) == NULL)
		return (false);
	++snap;
	/* We'll grab the individual components and do some light validation. */
	if (sscanf(snap, "%d-%d-%d-%d:%d:%d-%d", &year, &month, &day, &hour,
	    &minute, &second, &serial) != 7)
		return (false);
	return (year >= 1970) && (month >= 1 && month <= 12) &&
	    (day >= 1 && day <= 31) && (hour >= 0 && hour <= 23) &&
	    (minute >= 0 && minute <= 59) && (second >= 0 && second <= 60) &&
	    serial >= 0;
}

int
be_snapshot(libbe_handle_t *lbh, const char *source, const char *snap_name,
    bool recursive, char *result)
{
	char buf[BE_MAXPATHLEN];
	int err;

	be_root_concat(lbh, source, buf);

	if ((err = be_exists(lbh, buf)) != 0)
		return (set_error(lbh, err));

	if (snap_name != NULL) {
		if (strlcat(buf, "@", sizeof(buf)) >= sizeof(buf))
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		if (strlcat(buf, snap_name, sizeof(buf)) >= sizeof(buf))
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		if (result != NULL)
			snprintf(result, BE_MAXPATHLEN, "%s@%s", source,
			    snap_name);
	} else {
		be_setup_snapshot_name(lbh, buf, sizeof(buf));

		if (result != NULL && strlcpy(result, strrchr(buf, '/') + 1,
		    sizeof(buf)) >= sizeof(buf))
			return (set_error(lbh, BE_ERR_INVALIDNAME));
	}
	if ((err = zfs_snapshot(lbh->lzh, buf, recursive, NULL)) != 0) {
		switch (err) {
		case EZFS_INVALIDNAME:
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		default:
			/*
			 * The other errors that zfs_ioc_snapshot might return
			 * shouldn't happen if we've set things up properly, so
			 * we'll gloss over them and call it UNKNOWN as it will
			 * require further triage.
			 */
			if (errno == ENOTSUP)
				return (set_error(lbh, BE_ERR_NOPOOL));
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	return (BE_ERR_SUCCESS);
}


/*
 * Create the boot environment specified by the name parameter
 */
int
be_create(libbe_handle_t *lbh, const char *name)
{
	int err;

	err = be_create_from_existing(lbh, name, be_active_path(lbh));

	return (set_error(lbh, err));
}

static int
be_deep_clone_prop(int prop, void *cb)
{
	int err;
        struct libbe_dccb *dccb;
	zprop_source_t src;
	char pval[BE_MAXPATHLEN];
	char source[BE_MAXPATHLEN];
	char *val;

	dccb = cb;
	/* Skip some properties we don't want to touch */
	switch (prop) {
		/*
		 * libzfs insists on these being naturally inherited in the
		 * cloning process.
		 */
	case ZFS_PROP_KEYFORMAT:
	case ZFS_PROP_KEYLOCATION:
	case ZFS_PROP_ENCRYPTION:
	case ZFS_PROP_PBKDF2_ITERS:

		/* FALLTHROUGH */
	case ZFS_PROP_CANMOUNT:		/* Forced by libbe */
		return (ZPROP_CONT);
	}

	/* Don't copy readonly properties */
	if (zfs_prop_readonly(prop))
		return (ZPROP_CONT);

	if ((err = zfs_prop_get(dccb->zhp, prop, (char *)&pval,
	    sizeof(pval), &src, (char *)&source, sizeof(source), false)))
		/* Just continue if we fail to read a property */
		return (ZPROP_CONT);

	/*
	 * Only copy locally defined or received properties.  This continues
	 * to avoid temporary/default/local properties intentionally without
	 * breaking received datasets.
	 */
	if (src != ZPROP_SRC_LOCAL && src != ZPROP_SRC_RECEIVED)
		return (ZPROP_CONT);

	/* Augment mountpoint with altroot, if needed */
	val = pval;
	if (prop == ZFS_PROP_MOUNTPOINT)
		val = be_mountpoint_augmented(dccb->lbh, val);

	nvlist_add_string(dccb->props, zfs_prop_to_name(prop), val);

	return (ZPROP_CONT);
}

/*
 * Return the corresponding boot environment path for a given
 * dataset path, the constructed path is placed in 'result'.
 *
 * example: say our new boot environment name is 'bootenv' and
 *          the dataset path is 'zroot/ROOT/default/data/set'.
 *
 * result should produce: 'zroot/ROOT/bootenv/data/set'
 */
static int
be_get_path(struct libbe_deep_clone *ldc, const char *dspath, char *result, int result_size)
{
	char *pos;
	char *child_dataset;

	/* match the root path for the boot environments */
	pos = strstr(dspath, ldc->lbh->root);

	/* no match, different pools? */
	if (pos == NULL)
		return (BE_ERR_BADPATH);

	/* root path of the new boot environment */
	snprintf(result, result_size, "%s/%s", ldc->lbh->root, ldc->bename);

        /* gets us to the parent dataset, the +1 consumes a trailing slash */
	pos += strlen(ldc->lbh->root) + 1;

	/* skip the parent dataset */
	if ((child_dataset = strchr(pos, '/')) != NULL)
		strlcat(result, child_dataset, result_size);

	return (BE_ERR_SUCCESS);
}

static int
be_clone_cb(zfs_handle_t *ds, void *data)
{
	int err;
	char be_path[BE_MAXPATHLEN];
	char snap_path[BE_MAXPATHLEN];
	const char *dspath;
	zfs_handle_t *snap_hdl;
	nvlist_t *props;
	struct libbe_deep_clone *ldc;
	struct libbe_dccb dccb;

	ldc = (struct libbe_deep_clone *)data;
	dspath = zfs_get_name(ds);

	snprintf(snap_path, sizeof(snap_path), "%s@%s", dspath, ldc->snapname);

	/* construct the boot environment path from the dataset we're cloning */
	if (be_get_path(ldc, dspath, be_path, sizeof(be_path)) != BE_ERR_SUCCESS)
		return (BE_ERR_UNKNOWN);

	/* the dataset to be created (i.e. the boot environment) already exists */
	if (zfs_dataset_exists(ldc->lbh->lzh, be_path, ZFS_TYPE_DATASET))
		return (BE_ERR_EXISTS);

	/* no snapshot found for this dataset, silently skip it */
	if (!zfs_dataset_exists(ldc->lbh->lzh, snap_path, ZFS_TYPE_SNAPSHOT))
		return (0);

	if ((snap_hdl =
	    zfs_open(ldc->lbh->lzh, snap_path, ZFS_TYPE_SNAPSHOT)) == NULL)
		return (BE_ERR_ZFSOPEN);

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_add_string(props, "canmount", "noauto");

	dccb.lbh = ldc->lbh;
	dccb.zhp = ds;
	dccb.props = props;
	if (zprop_iter(be_deep_clone_prop, &dccb, B_FALSE, B_FALSE,
	    ZFS_TYPE_FILESYSTEM) == ZPROP_INVAL)
		return (-1);

	if ((err = zfs_clone(snap_hdl, be_path, props)) != 0)
		return (BE_ERR_ZFSCLONE);

	nvlist_free(props);
	zfs_close(snap_hdl);

	if (ldc->depth_limit == -1 || ldc->depth < ldc->depth_limit) {
		ldc->depth++;
		err = zfs_iter_filesystems(ds, be_clone_cb, ldc);
		ldc->depth--;
	}

	return (err);
}

/*
 * Create a boot environment with a given name from a given snapshot.
 * Snapshots can be in the format 'zroot/ROOT/default@snapshot' or
 * 'default@snapshot'. In the latter case, 'default@snapshot' will be prepended
 * with the root path that libbe was initailized with.
*/
static int
be_clone(libbe_handle_t *lbh, const char *bename, const char *snapshot, int depth)
{
	int err;
	char snap_path[BE_MAXPATHLEN];
	char *parentname, *snapname;
	zfs_handle_t *parent_hdl;
	struct libbe_deep_clone ldc;

        /* ensure the boot environment name is valid */
	if ((err = be_validate_name(lbh, bename)) != 0)
		return (set_error(lbh, err));

	/*
	 * prepend the boot environment root path if we're
	 * given a partial snapshot name.
	 */
	if ((err = be_root_concat(lbh, snapshot, snap_path)) != 0)
		return (set_error(lbh, err));

	/* ensure the snapshot exists */
	if ((err = be_validate_snap(lbh, snap_path)) != 0)
		return (set_error(lbh, err));

        /* get a copy of the snapshot path so we can disect it */
	if ((parentname = strdup(snap_path)) == NULL)
		return (set_error(lbh, BE_ERR_UNKNOWN));

        /* split dataset name from snapshot name */
	snapname = strchr(parentname, '@');
	if (snapname == NULL) {
		free(parentname);
		return (set_error(lbh, BE_ERR_UNKNOWN));
	}
	*snapname = '\0';
	snapname++;

        /* set-up the boot environment */
        ldc.lbh = lbh;
        ldc.bename = bename;
        ldc.snapname = snapname;
	ldc.depth = 0;
	ldc.depth_limit = depth;

        /* the boot environment will be cloned from this dataset */
	parent_hdl = zfs_open(lbh->lzh, parentname, ZFS_TYPE_DATASET);

        /* create the boot environment */
	err = be_clone_cb(parent_hdl, &ldc);

	free(parentname);
	return (set_error(lbh, err));
}

/*
 * Create a boot environment from pre-existing snapshot, specifying a depth.
 */
int be_create_depth(libbe_handle_t *lbh, const char *bename,
		    const char *snap, int depth)
{
	return (be_clone(lbh, bename, snap, depth));
}

/*
 * Create the boot environment from pre-existing snapshot
 */
int
be_create_from_existing_snap(libbe_handle_t *lbh, const char *bename,
    const char *snap)
{
	return (be_clone(lbh, bename, snap, -1));
}


/*
 * Create a boot environment from an existing boot environment
 */
int
be_create_from_existing(libbe_handle_t *lbh, const char *bename, const char *old)
{
	int err;
	char snap[BE_MAXPATHLEN];

	if ((err = be_snapshot(lbh, old, NULL, true, snap)) != 0)
		return (set_error(lbh, err));

        err = be_clone(lbh, bename, snap, -1);

	return (set_error(lbh, err));
}


/*
 * Verifies that a snapshot has a valid name, exists, and has a mountpoint of
 * '/'. Returns BE_ERR_SUCCESS (0), upon success, or the relevant BE_ERR_* upon
 * failure. Does not set the internal library error state.
 */
int
be_validate_snap(libbe_handle_t *lbh, const char *snap_name)
{

	if (strlen(snap_name) >= BE_MAXPATHLEN)
		return (BE_ERR_PATHLEN);

	if (!zfs_name_valid(snap_name, ZFS_TYPE_SNAPSHOT))
		return (BE_ERR_INVALIDNAME);

	if (!zfs_dataset_exists(lbh->lzh, snap_name,
	    ZFS_TYPE_SNAPSHOT))
		return (BE_ERR_NOENT);

	return (BE_ERR_SUCCESS);
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
be_root_concat(libbe_handle_t *lbh, const char *name, char *result)
{
	size_t name_len, root_len;

	name_len = strlen(name);
	root_len = strlen(lbh->root);

	/* Act idempotently; return be name if it is already a full path */
	if (strrchr(name, '/') != NULL) {
		if (strstr(name, lbh->root) != name)
			return (BE_ERR_INVALIDNAME);

		if (name_len >= BE_MAXPATHLEN)
			return (BE_ERR_PATHLEN);

		strlcpy(result, name, BE_MAXPATHLEN);
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
 * BE_ERR_SUCCESS (0) if name is valid, otherwise returns BE_ERR_INVALIDNAME
 * or BE_ERR_PATHLEN.
 * Does not set internal library error state.
 */
int
be_validate_name(libbe_handle_t *lbh, const char *name)
{

	/*
	 * Impose the additional restriction that the entire dataset name must
	 * not exceed the maximum length of a dataset, i.e. MAXNAMELEN.
	 */
	if (strlen(lbh->root) + 1 + strlen(name) > MAXNAMELEN)
		return (BE_ERR_PATHLEN);

	if (!zfs_name_valid(name, ZFS_TYPE_DATASET))
		return (BE_ERR_INVALIDNAME);

	/*
	 * ZFS allows spaces in boot environment names, but the kernel can't
	 * handle booting from such a dataset right now.  vfs.root.mountfrom
	 * is defined to be a space-separated list, and there's no protocol for
	 * escaping whitespace in the path component of a dev:path spec.  So
	 * while loader can handle this situation alright, it can't safely pass
	 * it on to mountroot.
	 */
	if (strchr(name, ' ') != NULL)
		return (BE_ERR_INVALIDNAME);

	return (BE_ERR_SUCCESS);
}


/*
 * usage
 */
int
be_rename(libbe_handle_t *lbh, const char *old, const char *new)
{
	char full_old[BE_MAXPATHLEN];
	char full_new[BE_MAXPATHLEN];
	zfs_handle_t *zfs_hdl;
	int err;

	/*
	 * be_validate_name is documented not to set error state, so we should
	 * do so here.
	 */
	if ((err = be_validate_name(lbh, new)) != 0)
		return (set_error(lbh, err));
	if ((err = be_root_concat(lbh, old, full_old)) != 0)
		return (set_error(lbh, err));
	if ((err = be_root_concat(lbh, new, full_new)) != 0)
		return (set_error(lbh, err));

	if (!zfs_dataset_exists(lbh->lzh, full_old, ZFS_TYPE_DATASET))
		return (set_error(lbh, BE_ERR_NOENT));

	if (zfs_dataset_exists(lbh->lzh, full_new, ZFS_TYPE_DATASET))
		return (set_error(lbh, BE_ERR_EXISTS));

	if ((zfs_hdl = zfs_open(lbh->lzh, full_old,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	/* recurse, nounmount, forceunmount */
	struct renameflags flags = {
		.nounmount = 1,
	};
	err = zfs_rename(zfs_hdl, full_new, flags);
	if (err != 0)
		goto error;

	/* handle renaming bootonce */
	if (lbh->bootonce != NULL &&
	    strcmp(full_old, lbh->bootonce) == 0)
		err = be_activate(lbh, new, true);

error:
	zfs_close(zfs_hdl);
	return (set_error(lbh, err));
}


int
be_export(libbe_handle_t *lbh, const char *bootenv, int fd)
{
	char snap_name[BE_MAXPATHLEN];
	char buf[BE_MAXPATHLEN];
	zfs_handle_t *zfs;
	sendflags_t flags = { 0 };
	int err;

	if ((err = be_snapshot(lbh, bootenv, NULL, true, snap_name)) != 0)
		/* Use the error set by be_snapshot */
		return (err);

	be_root_concat(lbh, snap_name, buf);

	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_DATASET)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	err = zfs_send_one(zfs, NULL, fd, &flags, /* redactbook */ NULL);
	zfs_close(zfs);

	return (err);
}


int
be_import(libbe_handle_t *lbh, const char *bootenv, int fd)
{
	char buf[BE_MAXPATHLEN];
	nvlist_t *props;
	zfs_handle_t *zfs;
	recvflags_t flags = { .nomount = 1 };
	int err;

	be_root_concat(lbh, bootenv, buf);

	if ((err = zfs_receive(lbh->lzh, buf, NULL, &flags, fd, NULL)) != 0) {
		switch (err) {
		case EINVAL:
			return (set_error(lbh, BE_ERR_NOORIGIN));
		case ENOENT:
			return (set_error(lbh, BE_ERR_NOENT));
		case EIO:
			return (set_error(lbh, BE_ERR_IO));
		default:
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_FILESYSTEM)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_add_string(props, "canmount", "noauto");
	nvlist_add_string(props, "mountpoint", "none");

	err = zfs_prop_set_list(zfs, props);
	nvlist_free(props);

	zfs_close(zfs);

	if (err != 0)
		return (set_error(lbh, BE_ERR_UNKNOWN));

	return (0);
}

#if SOON
static int
be_create_child_noent(libbe_handle_t *lbh, const char *active,
    const char *child_path)
{
	nvlist_t *props;
	zfs_handle_t *zfs;
	int err;

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_add_string(props, "canmount", "noauto");
	nvlist_add_string(props, "mountpoint", child_path);

	/* Create */
	if ((err = zfs_create(lbh->lzh, active, ZFS_TYPE_DATASET,
	    props)) != 0) {
		switch (err) {
		case EZFS_EXISTS:
			return (set_error(lbh, BE_ERR_EXISTS));
		case EZFS_NOENT:
			return (set_error(lbh, BE_ERR_NOENT));
		case EZFS_BADTYPE:
		case EZFS_BADVERSION:
			return (set_error(lbh, BE_ERR_NOPOOL));
		case EZFS_BADPROP:
		default:
			/* We set something up wrong, probably... */
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}
	nvlist_free(props);

	if ((zfs = zfs_open(lbh->lzh, active, ZFS_TYPE_DATASET)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	/* Set props */
	if ((err = zfs_prop_set(zfs, "canmount", "noauto")) != 0) {
		zfs_close(zfs);
		/*
		 * Similar to other cases, this shouldn't fail unless we've
		 * done something wrong.  This is a new dataset that shouldn't
		 * have been mounted anywhere between creation and now.
		 */
		if (err == EZFS_NOMEM)
			return (set_error(lbh, BE_ERR_NOMEM));
		return (set_error(lbh, BE_ERR_UNKNOWN));
	}
	zfs_close(zfs);
	return (BE_ERR_SUCCESS);
}

static int
be_create_child_cloned(libbe_handle_t *lbh, const char *active)
{
	char buf[BE_MAXPATHLEN], tmp[BE_MAXPATHLEN];
	zfs_handle_t *zfs;
	int err;

	/* XXX TODO ? */

	/*
	 * Establish if the existing path is a zfs dataset or just
	 * the subdirectory of one
	 */
	strlcpy(tmp, "tmp/be_snap.XXXXX", sizeof(tmp));
	if (mktemp(tmp) == NULL)
		return (set_error(lbh, BE_ERR_UNKNOWN));

	be_root_concat(lbh, tmp, buf);
	printf("Here %s?\n", buf);
	if ((err = zfs_snapshot(lbh->lzh, buf, false, NULL)) != 0) {
		switch (err) {
		case EZFS_INVALIDNAME:
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		default:
			/*
			 * The other errors that zfs_ioc_snapshot might return
			 * shouldn't happen if we've set things up properly, so
			 * we'll gloss over them and call it UNKNOWN as it will
			 * require further triage.
			 */
			if (errno == ENOTSUP)
				return (set_error(lbh, BE_ERR_NOPOOL));
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	/* Clone */
	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_SNAPSHOT)) == NULL)
		return (BE_ERR_ZFSOPEN);

	if ((err = zfs_clone(zfs, active, NULL)) != 0)
		/* XXX TODO correct error */
		return (set_error(lbh, BE_ERR_UNKNOWN));

	/* set props */
	zfs_close(zfs);
	return (BE_ERR_SUCCESS);
}

int
be_add_child(libbe_handle_t *lbh, const char *child_path, bool cp_if_exists)
{
	struct stat sb;
	char active[BE_MAXPATHLEN], buf[BE_MAXPATHLEN];
	nvlist_t *props;
	const char *s;

	/* Require absolute paths */
	if (*child_path != '/')
		return (set_error(lbh, BE_ERR_BADPATH));

	strlcpy(active, be_active_path(lbh), BE_MAXPATHLEN);
	strcpy(buf, active);

	/* Create non-mountable parent dataset(s) */
	s = child_path;
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
	if (strlcat(active, child_path, BE_MAXPATHLEN) >= BE_MAXPATHLEN)
		return (set_error(lbh, BE_ERR_PATHLEN));

	if (stat(child_path, &sb) != 0) {
		/* Verify that error is ENOENT */
		if (errno != ENOENT)
			return (set_error(lbh, BE_ERR_UNKNOWN));
		return (be_create_child_noent(lbh, active, child_path));
	} else if (cp_if_exists)
		/* Path is already a descendent of / and should be copied */
		return (be_create_child_cloned(lbh, active));
	return (set_error(lbh, BE_ERR_EXISTS));
}
#endif	/* SOON */

/*
 * Deactivate old BE dataset; currently just sets canmount=noauto or
 * resets boot once configuration.
 */
int
be_deactivate(libbe_handle_t *lbh, const char *ds, bool temporary)
{
	zfs_handle_t *zfs;

	if (temporary) {
		return (lzbe_set_boot_device(
		    zpool_get_name(lbh->active_phandle), lzbe_add, NULL));
	}

	if ((zfs = zfs_open(lbh->lzh, ds, ZFS_TYPE_DATASET)) == NULL)
		return (1);
	if (zfs_prop_set(zfs, "canmount", "noauto") != 0)
		return (1);
	zfs_close(zfs);
	return (0);
}

static int
be_zfs_promote_cb(zfs_handle_t *zhp, void *data)
{
	char origin[BE_MAXPATHLEN];
	bool *found_origin = (bool *)data;
	int err;

	if (zfs_prop_get(zhp, ZFS_PROP_ORIGIN, origin, sizeof(origin),
	    NULL, NULL, 0, true) == 0) {
		*found_origin = true;
		err = zfs_promote(zhp);
		if (err)
			return (err);
	}

	return (zfs_iter_filesystems(zhp, be_zfs_promote_cb, data));
}

static int
be_zfs_promote(zfs_handle_t *zhp, bool *found_origin)
{
	*found_origin = false;
	return (be_zfs_promote_cb(zhp, (void *)found_origin));
}

int
be_activate(libbe_handle_t *lbh, const char *bootenv, bool temporary)
{
	char be_path[BE_MAXPATHLEN];
	zfs_handle_t *zhp;
	int err;
	bool found_origin;

	be_root_concat(lbh, bootenv, be_path);

	/* Note: be_exists fails if mountpoint is not / */
	if ((err = be_exists(lbh, be_path)) != 0)
		return (set_error(lbh, err));

	if (temporary) {
		return (lzbe_set_boot_device(
		    zpool_get_name(lbh->active_phandle), lzbe_add, be_path));
	} else {
		if (strncmp(lbh->bootfs, "-", 1) != 0 &&
		    be_deactivate(lbh, lbh->bootfs, false) != 0)
			return (-1);

		/* Obtain bootenv zpool */
		err = zpool_set_prop(lbh->active_phandle, "bootfs", be_path);
		if (err)
			return (-1);

		for (;;) {
			zhp = zfs_open(lbh->lzh, be_path, ZFS_TYPE_FILESYSTEM);
			if (zhp == NULL)
				return (-1);

			err = be_zfs_promote(zhp, &found_origin);

			zfs_close(zhp);
			if (!found_origin)
				break;
			if (err)
				return (err);
		}

		if (err)
			return (-1);
	}

	return (BE_ERR_SUCCESS);
}
