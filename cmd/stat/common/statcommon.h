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
 * Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012 by Delphix. All rights reserved.
 */

/*
 * Common routines for acquiring snapshots of kstats for
 * iostat, mpstat, and vmstat.
 */

#ifndef	_STATCOMMON_H
#define	_STATCOMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <kstat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/dnlc.h>
#include <sys/sysinfo.h>
#include <sys/processor.h>
#include <sys/pset.h>
#include <sys/avl.h>

/* No CPU present at this CPU position */
#define	ID_NO_CPU -1
/* CPU belongs to no pset (we number this as "pset 0")  */
#define	ID_NO_PSET 0
/* CPU is usable */
#define	CPU_ONLINE(s) ((s) == P_ONLINE || (s) == P_NOINTR)
/* will the CPU have kstats */
#define	CPU_ACTIVE(c) (CPU_ONLINE((c)->cs_state) && (c)->cs_id != ID_NO_CPU)
/* IO device has no identified ID */
#define	IODEV_NO_ID -1
/* no limit to iodevs to collect */
#define	UNLIMITED_IODEVS ((size_t)-1)

#define	NODATE	0	/* Default:  No time stamp */
#define	DDATE	1	/* Standard date format */
#define	UDATE	2	/* Internal representation of Unix time */


enum snapshot_types {
	/* All CPUs separately */
	SNAP_CPUS 		= 1 << 0,
	/* Aggregated processor sets */
	SNAP_PSETS		= 1 << 1,
	/* sys-wide stats including aggregated CPU stats */
	SNAP_SYSTEM		= 1 << 2,
	/* interrupt sources and counts */
	SNAP_INTERRUPTS 	= 1 << 3,
	/* disk etc. stats */
	SNAP_IODEVS		= 1 << 4,
	/* disk controller aggregates */
	SNAP_CONTROLLERS	= 1 << 5,
	/* mpxio L I (multipath) paths: -X: Lun,LunInitiator */
	SNAP_IOPATHS_LI		= 1 << 6,
	/* mpxio LTI (multipath) paths: -Y: Lun,LunTarget,LunTargetInitiator */
	SNAP_IOPATHS_LTI	= 1 << 7,
	/* disk error stats */
	SNAP_IODEV_ERRORS	= 1 << 8,
	/* pretty names for iodevs */
	SNAP_IODEV_PRETTY	= 1 << 9,
	/* devid for iodevs */
	SNAP_IODEV_DEVID	= 1 << 10
};

struct cpu_snapshot {
	/* may be ID_NO_CPU if no CPU present */
	processorid_t cs_id;
	/* may be ID_NO_PSET if no pset */
	psetid_t cs_pset_id;
	/* as in p_online(2) */
	int cs_state;
	/* stats for this CPU */
	kstat_t cs_vm;
	kstat_t cs_sys;
};

struct pset_snapshot {
	/* ID may be zero to indicate the "none set" */
	psetid_t ps_id;
	/* number of CPUs in set */
	size_t ps_nr_cpus;
	/* the CPUs in this set */
	struct cpu_snapshot **ps_cpus;
};

struct intr_snapshot {
	/* name of interrupt source */
	char is_name[KSTAT_STRLEN];
	/* total number of interrupts from this source */
	ulong_t is_total;
};

struct sys_snapshot {
	sysinfo_t ss_sysinfo;
	vminfo_t ss_vminfo;
	struct nc_stats ss_nc;
	/* vm/sys stats aggregated across all CPUs */
	kstat_t ss_agg_vm;
	kstat_t ss_agg_sys;
	/* ticks since boot */
	ulong_t ss_ticks;
	long ss_deficit;
};

/* order is significant (see sort_before()) */
enum iodev_type {
	IODEV_CONTROLLER	= 1 << 0,
	IODEV_DISK		= 1 << 1,
	IODEV_PARTITION		= 1 << 2,
	IODEV_TAPE		= 1 << 3,
	IODEV_NFS		= 1 << 4,
	IODEV_IOPATH_LT		= 1 << 5,	/* synthetic LunTarget */
	IODEV_IOPATH_LI		= 1 << 6,	/* synthetic LunInitiator */
	IODEV_IOPATH_LTI	= 1 << 7,	/* LunTgtInitiator (pathinfo) */
	IODEV_UNKNOWN		= 1 << 8
};

/* identify a disk, partition, etc. */
struct iodev_id {
	int id;
	/* target id (for disks) */
	char tid[KSTAT_STRLEN];
};

/*
 * Used for disks, partitions, tapes, nfs, controllers, iopaths
 * Each entry can be a branch of a tree; for example, the disks
 * of a controller constitute the children of the controller
 * iodev_snapshot. This relationship is not strictly maintained
 * if is_pretty can't be found.
 */
struct iodev_snapshot {
	/* original kstat name */
	char is_name[KSTAT_STRLEN];
	/* type of kstat */
	enum iodev_type is_type;
	/* ID if meaningful */
	struct iodev_id is_id;
	/* parent ID if meaningful */
	struct iodev_id is_parent_id;
	/* user-friendly name if found */
	char *is_pretty;
	/* device ID if applicable */
	char *is_devid;
	/* mount-point if applicable */
	char *is_dname;
	/* number of direct children */
	int is_nr_children;
	/* children of this I/O device */
	struct iodev_snapshot *is_children;
	/* standard I/O stats */
	kstat_io_t is_stats;
	/* iodev error stats */
	kstat_t is_errors;
	/* creation time of the stats */
	hrtime_t is_crtime;
	/* time at which iodev snapshot was taken */
	hrtime_t is_snaptime;
	/* kstat module */
	char is_module[KSTAT_STRLEN];
	/* kstat instance */
	int is_instance;
	/* kstat (only used temporarily) */
	kstat_t *is_ksp;
	struct iodev_snapshot *is_prev;
	struct iodev_snapshot *is_next;
	/* AVL structures to speedup insertion */
	avl_tree_t *avl_list;	/* list this element belongs to */
	avl_node_t avl_link;
};

/* which iodevs to show. */
struct iodev_filter {
	/* nr. of iodevs to choose */
	size_t if_max_iodevs;
	/* bit mask of enum io_types to allow */
	int if_allowed_types;
	/* should we show floppy ? if_names can override this */
	int if_skip_floppy;
	/* nr. of named iodevs */
	size_t if_nr_names;
	char **if_names;
};

/* The primary structure of a system snapshot. */
struct snapshot {
	/* what types were *requested* */
	enum snapshot_types s_types;
	size_t s_nr_cpus;
	struct cpu_snapshot *s_cpus;
	size_t s_nr_psets;
	struct pset_snapshot *s_psets;
	size_t s_nr_intrs;
	struct intr_snapshot *s_intrs;
	size_t s_nr_iodevs;
	struct iodev_snapshot *s_iodevs;
	size_t s_iodevs_is_name_maxlen;
	struct sys_snapshot s_sys;
	struct biostats s_biostats;
	size_t s_nr_active_cpus;
};

/* print a message and exit with failure */
void fail(int do_perror, char *message, ...);

/* strdup str, or exit with failure */
char *safe_strdup(char *str);

/* malloc successfully, or exit with failure */
void *safe_alloc(size_t size);

/*
 * Copy a kstat from src to dst. If the source kstat contains no data,
 * then set the destination kstat data to NULL and size to zero.
 * Returns 0 on success.
 */
int kstat_copy(const kstat_t *src, kstat_t *dst);

/*
 * Look up the named kstat, and give the ui64 difference i.e.
 * new - old, or if old is NULL, return new.
 */
uint64_t kstat_delta(kstat_t *old, kstat_t *new, char *name);

/* Return the number of ticks delta between two hrtime_t values. */
uint64_t hrtime_delta(hrtime_t old, hrtime_t new);

/*
 * Add the integer-valued stats from "src" to the
 * existing ones in "dst". If "dst" does not contain
 * stats, then a kstat_copy() is performed.
 */
int kstat_add(const kstat_t *src, kstat_t *dst);

/* return the number of CPUs with kstats (i.e. present and online) */
int nr_active_cpus(struct snapshot *ss);

/*
 * Return the difference in CPU ticks between the two sys
 * kstats.
 */
uint64_t cpu_ticks_delta(kstat_t *old, kstat_t *new);

/*
 * Open the kstat chain. Cannot fail.
 */
kstat_ctl_t *open_kstat(void);

/*
 * Return a struct snapshot based on the snapshot_types parameter
 * passed in. iodev_filter may be NULL in which case all iodevs
 * are selected if SNAP_IODEVS is passed.
 */
struct snapshot *acquire_snapshot(kstat_ctl_t *, int, struct iodev_filter *);

/* free a snapshot */
void free_snapshot(struct snapshot *ss);

typedef void (*snapshot_cb)(void *old, void *new, void *data);

/*
 * Call the call back for each pair of data items of the given type,
 * passing the data pointer passed in as well. If an item has been
 * added, the first pointer will be NULL; if removed, the second pointer
 * will be NULL.
 *
 * A non-zero return value indicates configuration has changed.
 */
int snapshot_walk(enum snapshot_types type, struct snapshot *old,
    struct snapshot *new, snapshot_cb cb, void *data);

/*
 * Output a line detailing any configuration changes such as a CPU
 * brought online, etc, bracketed by << >>.
 */
void snapshot_report_changes(struct snapshot *old, struct snapshot *new);

/* Return non-zero if configuration has changed. */
int snapshot_has_changed(struct snapshot *old, struct snapshot *new);

/* free the given iodev */
void free_iodev(struct iodev_snapshot *iodev);

/* acquire the I/O devices */
int acquire_iodevs(struct snapshot *ss, kstat_ctl_t *kc,
    struct iodev_filter *df);

/* strcmp-style I/O device comparator */
int iodev_cmp(struct iodev_snapshot *io1, struct iodev_snapshot *io2);

/* sleep until *wakeup + interval, keeping cadence where desired */
void sleep_until(hrtime_t *wakeup, hrtime_t interval, int forever,
    int *caught_cont);

/* signal handler - so we can be aware of SIGCONT */
void cont_handler(int sig_number);

/* Print a timestamp in either Unix or standard format. */
void print_timestamp(uint_t);

#ifdef __cplusplus
}
#endif

#endif /* _STATCOMMON_H */
