/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_LIBZFS_H
#define	_LIBZFS_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <assert.h>
#include <libnvpair.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/fs/zfs.h>
#include <sys/zfs_ioctl.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Miscellaneous ZFS constants
 */
#define	ZFS_MAXNAMELEN		MAXNAMELEN
#define	ZPOOL_MAXNAMELEN	MAXNAMELEN
#define	ZFS_MAXPROPLEN		MAXPATHLEN

/*
 * libzfs errors
 */
enum {
	EZFS_NOMEM = 2000,	/* out of memory */
	EZFS_BADPROP,		/* invalid property value */
	EZFS_PROPREADONLY,	/* cannot set readonly property */
	EZFS_PROPTYPE,		/* property does not apply to dataset type */
	EZFS_PROPNONINHERIT,	/* property is not inheritable */
	EZFS_PROPSPACE,		/* bad quota or reservation */
	EZFS_BADTYPE,		/* dataset is not of appropriate type */
	EZFS_BUSY,		/* pool or dataset is busy */
	EZFS_EXISTS,		/* pool or dataset already exists */
	EZFS_NOENT,		/* no such pool or dataset */
	EZFS_BADSTREAM,		/* bad backup stream */
	EZFS_DSREADONLY,	/* dataset is readonly */
	EZFS_VOLTOOBIG,		/* volume is too large for 32-bit system */
	EZFS_VOLHASDATA,	/* volume already contains data */
	EZFS_INVALIDNAME,	/* invalid dataset name */
	EZFS_BADRESTORE,	/* unable to restore to destination */
	EZFS_BADBACKUP,		/* backup failed */
	EZFS_BADTARGET,		/* bad attach/detach/replace target */
	EZFS_NODEVICE,		/* no such device in pool */
	EZFS_BADDEV,		/* invalid device to add */
	EZFS_NOREPLICAS,	/* no valid replicas */
	EZFS_RESILVERING,	/* currently resilvering */
	EZFS_BADVERSION,	/* unsupported version */
	EZFS_POOLUNAVAIL,	/* pool is currently unavailable */
	EZFS_DEVOVERFLOW,	/* too many devices in one vdev */
	EZFS_BADPATH,		/* must be an absolute path */
	EZFS_CROSSTARGET,	/* rename or clone across pool or dataset */
	EZFS_ZONED,		/* used improperly in local zone */
	EZFS_MOUNTFAILED,	/* failed to mount dataset */
	EZFS_UMOUNTFAILED,	/* failed to unmount dataset */
	EZFS_UNSHARENFSFAILED,	/* unshare(1M) failed */
	EZFS_SHARENFSFAILED,	/* share(1M) failed */
	EZFS_DEVLINKS,		/* failed to create zvol links */
	EZFS_PERM,		/* permission denied */
	EZFS_NOSPC,		/* out of space */
	EZFS_IO,		/* I/O error */
	EZFS_INTR,		/* signal received */
	EZFS_ISSPARE,		/* device is a hot spare */
	EZFS_INVALCONFIG,	/* invalid vdev configuration */
	EZFS_RECURSIVE,		/* recursive dependency */
	EZFS_NOHISTORY,		/* no history object */
	EZFS_UNSHAREISCSIFAILED, /* iscsitgtd failed request to unshare */
	EZFS_SHAREISCSIFAILED,	/* iscsitgtd failed request to share */
	EZFS_POOLPROPS,		/* couldn't retrieve pool props */
	EZFS_POOL_NOTSUP,	/* ops not supported for this type of pool */
	EZFS_POOL_INVALARG,	/* invalid argument for this pool operation */
	EZFS_NAMETOOLONG,	/* dataset name is too long */
	EZFS_UNKNOWN
};

/*
 * Basic handle types
 */
typedef struct zfs_handle zfs_handle_t;
typedef struct zpool_handle zpool_handle_t;
typedef struct libzfs_handle libzfs_handle_t;

/*
 * Library initialization
 */
extern libzfs_handle_t *libzfs_init(void);
extern void libzfs_fini(libzfs_handle_t *);

extern libzfs_handle_t *zpool_get_handle(zpool_handle_t *);
extern libzfs_handle_t *zfs_get_handle(zfs_handle_t *);

extern void libzfs_print_on_error(libzfs_handle_t *, boolean_t);

extern int libzfs_errno(libzfs_handle_t *);
extern const char *libzfs_error_action(libzfs_handle_t *);
extern const char *libzfs_error_description(libzfs_handle_t *);

/*
 * Basic handle functions
 */
extern zpool_handle_t *zpool_open(libzfs_handle_t *, const char *);
extern zpool_handle_t *zpool_open_canfail(libzfs_handle_t *, const char *);
extern void zpool_close(zpool_handle_t *);
extern const char *zpool_get_name(zpool_handle_t *);
extern uint64_t zpool_get_guid(zpool_handle_t *);
extern uint64_t zpool_get_space_used(zpool_handle_t *);
extern uint64_t zpool_get_space_total(zpool_handle_t *);
extern int zpool_get_root(zpool_handle_t *, char *, size_t);
extern int zpool_get_state(zpool_handle_t *);
extern uint64_t zpool_get_version(zpool_handle_t *);

/*
 * Iterate over all active pools in the system.
 */
typedef int (*zpool_iter_f)(zpool_handle_t *, void *);
extern int zpool_iter(libzfs_handle_t *, zpool_iter_f, void *);

/*
 * Functions to create and destroy pools
 */
extern int zpool_create(libzfs_handle_t *, const char *, nvlist_t *,
    const char *);
extern int zpool_destroy(zpool_handle_t *);
extern int zpool_add(zpool_handle_t *, nvlist_t *);

/*
 * Functions to manipulate pool and vdev state
 */
extern int zpool_scrub(zpool_handle_t *, pool_scrub_type_t);

extern int zpool_vdev_online(zpool_handle_t *, const char *);
extern int zpool_vdev_offline(zpool_handle_t *, const char *, int);
extern int zpool_vdev_attach(zpool_handle_t *, const char *, const char *,
    nvlist_t *, int);
extern int zpool_vdev_detach(zpool_handle_t *, const char *);
extern int zpool_vdev_remove(zpool_handle_t *, const char *);
extern int zpool_clear(zpool_handle_t *, const char *);
extern nvlist_t *zpool_find_vdev(zpool_handle_t *, const char *, boolean_t *);

/*
 * Functions to manage pool properties
 */
extern int zpool_set_prop(zpool_handle_t *, const char *, const char *);
extern int zpool_get_prop(zpool_handle_t *, zfs_prop_t, char *,
	size_t proplen, zfs_source_t *);
extern const char *zpool_prop_to_name(zpool_prop_t);
extern const char *zpool_prop_values(zpool_prop_t);

/*
 * Pool health statistics.
 */
typedef enum {
	/*
	 * The following correspond to faults as defined in the (fault.fs.zfs.*)
	 * event namespace.  Each is associated with a corresponding message ID.
	 */
	ZPOOL_STATUS_CORRUPT_CACHE,	/* corrupt /kernel/drv/zpool.cache */
	ZPOOL_STATUS_MISSING_DEV_R,	/* missing device with replicas */
	ZPOOL_STATUS_MISSING_DEV_NR,	/* missing device with no replicas */
	ZPOOL_STATUS_CORRUPT_LABEL_R,	/* bad device label with replicas */
	ZPOOL_STATUS_CORRUPT_LABEL_NR,	/* bad device label with no replicas */
	ZPOOL_STATUS_BAD_GUID_SUM,	/* sum of device guids didn't match */
	ZPOOL_STATUS_CORRUPT_POOL,	/* pool metadata is corrupted */
	ZPOOL_STATUS_CORRUPT_DATA,	/* data errors in user (meta)data */
	ZPOOL_STATUS_FAILING_DEV,	/* device experiencing errors */
	ZPOOL_STATUS_VERSION_NEWER,	/* newer on-disk version */
	ZPOOL_STATUS_HOSTID_MISMATCH,	/* last accessed by another system */

	/*
	 * The following are not faults per se, but still an error possibly
	 * requiring administrative attention.  There is no corresponding
	 * message ID.
	 */
	ZPOOL_STATUS_VERSION_OLDER,	/* older on-disk version */
	ZPOOL_STATUS_RESILVERING,	/* device being resilvered */
	ZPOOL_STATUS_OFFLINE_DEV,	/* device online */

	/*
	 * Finally, the following indicates a healthy pool.
	 */
	ZPOOL_STATUS_OK
} zpool_status_t;

extern zpool_status_t zpool_get_status(zpool_handle_t *, char **);
extern zpool_status_t zpool_import_status(nvlist_t *, char **);

/*
 * Statistics and configuration functions.
 */
extern nvlist_t *zpool_get_config(zpool_handle_t *, nvlist_t **);
extern int zpool_refresh_stats(zpool_handle_t *, boolean_t *);
extern int zpool_get_errlog(zpool_handle_t *, nvlist_t **);

/*
 * Import and export functions
 */
extern int zpool_export(zpool_handle_t *);
extern int zpool_import(libzfs_handle_t *, nvlist_t *, const char *,
    const char *);

/*
 * Search for pools to import
 */
extern nvlist_t *zpool_find_import(libzfs_handle_t *, int, char **);

/*
 * Miscellaneous pool functions
 */
extern char *zpool_vdev_name(libzfs_handle_t *, zpool_handle_t *, nvlist_t *);
extern int zpool_upgrade(zpool_handle_t *);
extern int zpool_get_history(zpool_handle_t *, nvlist_t **);
extern void zpool_log_history(libzfs_handle_t *, int, char **, const char *,
    boolean_t, boolean_t);
extern void zpool_obj_to_path(zpool_handle_t *, uint64_t, uint64_t, char *,
    size_t len);

/*
 * Basic handle manipulations.  These functions do not create or destroy the
 * underlying datasets, only the references to them.
 */
extern zfs_handle_t *zfs_open(libzfs_handle_t *, const char *, int);
extern void zfs_close(zfs_handle_t *);
extern zfs_type_t zfs_get_type(const zfs_handle_t *);
extern const char *zfs_get_name(const zfs_handle_t *);

/*
 * Property management functions.  Some functions are shared with the kernel,
 * and are found in sys/fs/zfs.h.
 */
extern const char *zfs_prop_to_name(zfs_prop_t);
extern int zfs_prop_set(zfs_handle_t *, const char *, const char *);
extern int zfs_prop_get(zfs_handle_t *, zfs_prop_t, char *, size_t,
    zfs_source_t *, char *, size_t, boolean_t);
extern int zfs_prop_get_numeric(zfs_handle_t *, zfs_prop_t, uint64_t *,
    zfs_source_t *, char *, size_t);
extern uint64_t zfs_prop_get_int(zfs_handle_t *, zfs_prop_t);
extern const char *zfs_prop_get_string(zfs_handle_t *, zfs_prop_t);
extern int zfs_prop_inherit(zfs_handle_t *, const char *);
extern const char *zfs_prop_values(zfs_prop_t);
extern int zfs_prop_valid_for_type(zfs_prop_t, int);
extern const char *zfs_prop_default_string(zfs_prop_t prop);
extern uint64_t zfs_prop_default_numeric(zfs_prop_t);
extern int zfs_prop_is_string(zfs_prop_t prop);
extern const char *zfs_prop_column_name(zfs_prop_t);
extern boolean_t zfs_prop_align_right(zfs_prop_t);
extern void nicebool(int value, char *buf, size_t buflen);

typedef struct zfs_proplist {
	zfs_prop_t	pl_prop;
	char		*pl_user_prop;
	struct zfs_proplist *pl_next;
	boolean_t	pl_all;
	size_t		pl_width;
	boolean_t	pl_fixed;
} zfs_proplist_t;

typedef zfs_proplist_t zpool_proplist_t;

extern int zfs_get_proplist(libzfs_handle_t *, char *, zfs_proplist_t **);
extern int zpool_get_proplist(libzfs_handle_t *, char *, zpool_proplist_t **);
extern int zfs_expand_proplist(zfs_handle_t *, zfs_proplist_t **);
extern int zpool_expand_proplist(zpool_handle_t *, zpool_proplist_t **);
extern void zfs_free_proplist(zfs_proplist_t *);
extern nvlist_t *zfs_get_user_props(zfs_handle_t *);

#define	ZFS_MOUNTPOINT_NONE	"none"
#define	ZFS_MOUNTPOINT_LEGACY	"legacy"

/*
 * Functions for printing properties from zfs/zpool
 */
typedef struct libzfs_get_cbdata {
	int cb_sources;
	int cb_columns[4];
	int cb_colwidths[5];
	boolean_t cb_scripted;
	boolean_t cb_literal;
	boolean_t cb_first;
	zfs_proplist_t *cb_proplist;
} libzfs_get_cbdata_t;

void libzfs_print_one_property(const char *, libzfs_get_cbdata_t *,
    const char *, const char *, zfs_source_t, const char *);

#define	GET_COL_NAME		1
#define	GET_COL_PROPERTY	2
#define	GET_COL_VALUE		3
#define	GET_COL_SOURCE		4

/*
 * Iterator functions.
 */
typedef int (*zfs_iter_f)(zfs_handle_t *, void *);
extern int zfs_iter_root(libzfs_handle_t *, zfs_iter_f, void *);
extern int zfs_iter_children(zfs_handle_t *, zfs_iter_f, void *);
extern int zfs_iter_dependents(zfs_handle_t *, boolean_t, zfs_iter_f, void *);
extern int zfs_iter_filesystems(zfs_handle_t *, zfs_iter_f, void *);
extern int zfs_iter_snapshots(zfs_handle_t *, zfs_iter_f, void *);

/*
 * Functions to create and destroy datasets.
 */
extern int zfs_create(libzfs_handle_t *, const char *, zfs_type_t,
    nvlist_t *);
extern int zfs_destroy(zfs_handle_t *);
extern int zfs_destroy_snaps(zfs_handle_t *, char *);
extern int zfs_clone(zfs_handle_t *, const char *, nvlist_t *);
extern int zfs_snapshot(libzfs_handle_t *, const char *, boolean_t);
extern int zfs_rollback(zfs_handle_t *, zfs_handle_t *, int);
extern int zfs_rename(zfs_handle_t *, const char *, int);
extern int zfs_send(zfs_handle_t *, const char *, int);
extern int zfs_receive(libzfs_handle_t *, const char *, int, int, int,
    boolean_t, int);
extern int zfs_promote(zfs_handle_t *);

/*
 * Miscellaneous functions.
 */
extern const char *zfs_type_to_name(zfs_type_t);
extern void zfs_refresh_properties(zfs_handle_t *);
extern int zfs_name_valid(const char *, zfs_type_t);
extern int zfs_disable(zfs_handle_t *);
extern int zfs_enable(zfs_handle_t *);
extern zfs_handle_t *zfs_path_to_zhandle(libzfs_handle_t *, char *, zfs_type_t);

/*
 * Mount support functions.
 */
extern boolean_t is_mounted(libzfs_handle_t *, const char *special, char **);
extern boolean_t zfs_is_mounted(zfs_handle_t *, char **);
extern int zfs_mount(zfs_handle_t *, const char *, int);
extern int zfs_unmount(zfs_handle_t *, const char *, int);
extern int zfs_unmountall(zfs_handle_t *, int);

/*
 * Share support functions.
 */
extern boolean_t zfs_is_shared(zfs_handle_t *);
extern int zfs_share(zfs_handle_t *);
extern int zfs_unshare(zfs_handle_t *);

/*
 * Protocol-specifc share support functions.
 */
extern boolean_t zfs_is_shared_nfs(zfs_handle_t *, char **);
extern int zfs_share_nfs(zfs_handle_t *);
extern int zfs_unshare_nfs(zfs_handle_t *, const char *);
extern int zfs_unshareall_nfs(zfs_handle_t *);
extern boolean_t zfs_is_shared_iscsi(zfs_handle_t *);
extern int zfs_share_iscsi(zfs_handle_t *);
extern int zfs_unshare_iscsi(zfs_handle_t *);

/*
 * FreeBSD-specific jail support function.
 */
extern int zfs_jail(zfs_handle_t *, int, int);

/*
 * When dealing with nvlists, verify() is extremely useful
 */
#ifndef verify
#ifdef NDEBUG
#define	verify(EX)	((void)(EX))
#else
#define	verify(EX)	assert(EX)
#endif
#endif

/*
 * Utility function to convert a number to a human-readable form.
 */
extern void zfs_nicenum(uint64_t, char *, size_t);
extern int zfs_nicestrtonum(libzfs_handle_t *, const char *, uint64_t *);

/*
 * Pool destroy special.  Remove the device information without destroying
 * the underlying dataset.
 */
extern int zfs_remove_link(zfs_handle_t *);

/*
 * Given a device or file, determine if it is part of a pool.
 */
extern int zpool_in_use(libzfs_handle_t *, int, pool_state_t *, char **,
    boolean_t *);

/*
 * ftyp special.  Read the label from a given device.
 */
extern int zpool_read_label(int, nvlist_t **);

/*
 * Create and remove zvol /dev links.
 */
extern int zpool_create_zvol_links(zpool_handle_t *);
extern int zpool_remove_zvol_links(zpool_handle_t *);

/*
 * Enable and disable datasets within a pool by mounting/unmounting and
 * sharing/unsharing them.
 */
extern int zpool_enable_datasets(zpool_handle_t *, const char *, int);
extern int zpool_disable_datasets(zpool_handle_t *, boolean_t);

#ifdef	__FreeBSD__
extern int zmount(const char *, const char *, int, char *, char *, int, char *,
    int);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBZFS_H */
