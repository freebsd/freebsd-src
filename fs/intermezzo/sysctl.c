/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 1999 Peter J. Braam <braam@clusterfs.com>
 *
 *   This file is part of InterMezzo, http://www.inter-mezzo.org.
 *
 *   InterMezzo is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   InterMezzo is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with InterMezzo; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Sysctrl entries for Intermezzo!
 */

#define __NO_VERSION__
#include <linux/config.h> /* for CONFIG_PROC_FS */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/swapctl.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <asm/bitops.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/utsname.h>
#include <linux/blk.h>


#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

/* /proc entries */

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_fs_intermezzo;
int intermezzo_mount_get_info( char * buffer, char ** start, off_t offset,
			       int length)
{
	int len=0;

	/* this works as long as we are below 1024 characters! */
	*start = buffer + offset;
	len -= offset;

	if ( len < 0 )
		return -EINVAL;

	return len;
}

#endif


/* SYSCTL below */

static struct ctl_table_header *intermezzo_table_header = NULL;
/* 0x100 to avoid any chance of collisions at any point in the tree with
 * non-directories
 */
#define PSDEV_INTERMEZZO  (0x100)

#define PSDEV_DEBUG	   1      /* control debugging */
#define PSDEV_TRACE	   2      /* control enter/leave pattern */
#define PSDEV_TIMEOUT      3      /* timeout on upcalls to become intrble */
#define PSDEV_HARD         4      /* mount type "hard" or "soft" */
#define PSDEV_NO_FILTER    5      /* controls presto_chk */
#define PSDEV_NO_JOURNAL   6      /* controls presto_chk */
#define PSDEV_NO_UPCALL    7      /* controls lento_upcall */
#define PSDEV_ERRORVAL     8      /* controls presto_debug_fail_blkdev */
#define PSDEV_EXCL_GID     9      /* which GID is ignored by presto */
#define PSDEV_BYTES_TO_CLOSE 11   /* bytes to write before close */

/* These are global presto control options */
#define PRESTO_PRIMARY_CTLCNT 2
static struct ctl_table presto_table[ PRESTO_PRIMARY_CTLCNT + MAX_CHANNEL + 1] =
{
	{PSDEV_DEBUG, "debug", &presto_debug, sizeof(int), 0644, NULL, &proc_dointvec},
	{PSDEV_TRACE, "trace", &presto_print_entry, sizeof(int), 0644, NULL, &proc_dointvec},
};

/*
 * Intalling the sysctl entries: strategy
 * - have templates for each /proc/sys/intermezzo/ entry
 *   such an entry exists for each /dev/presto
 *    (proto_channel_entry)
 * - have a template for the contents of such directories
 *    (proto_psdev_table)
 * - have the master table (presto_table)
 *
 * When installing, malloc, memcpy and fix up the pointers to point to
 * the appropriate constants in izo_channels[your_minor]
 */

static ctl_table proto_psdev_table[] = {
	{PSDEV_HARD, "hard", 0, sizeof(int), 0644, NULL, &proc_dointvec},
	{PSDEV_NO_FILTER, "no_filter", 0, sizeof(int), 0644, NULL, &proc_dointvec},
	{PSDEV_NO_JOURNAL, "no_journal", NULL, sizeof(int), 0644, NULL, &proc_dointvec},
	{PSDEV_NO_UPCALL, "no_upcall", NULL, sizeof(int), 0644, NULL, &proc_dointvec},
	{PSDEV_TIMEOUT, "timeout", NULL, sizeof(int), 0644, NULL, &proc_dointvec},
#ifdef PRESTO_DEBUG
	{PSDEV_ERRORVAL, "errorval", NULL, sizeof(int), 0644, NULL, &proc_dointvec},
#endif
	{ 0 }
};

static ctl_table proto_channel_entry = {
	PSDEV_INTERMEZZO, 0,  NULL, 0, 0555, 0,
};

static ctl_table intermezzo_table[2] = {
	{PSDEV_INTERMEZZO, "intermezzo",    NULL, 0, 0555, presto_table},
	{0}
};

/* support for external setting and getting of opts. */
/* particularly via ioctl. The Right way to do this is via sysctl,
 * but that will have to wait until intermezzo gets its own nice set of
 * sysctl IDs
 */
/* we made these separate as setting may in future be more restricted
 * than getting
 */
#ifdef RON_MINNICH
int dosetopt(int minor, struct psdev_opt *opt)
{
	int retval = 0;
	int newval = opt->optval;

	ENTRY;

	switch(opt->optname) {

	case PSDEV_TIMEOUT:
		izo_channels[minor].uc_timeout = newval;
		break;

	case PSDEV_HARD:
		izo_channels[minor].uc_hard = newval;
		break;

	case PSDEV_NO_FILTER:
		izo_channels[minor].uc_no_filter = newval;
		break;

	case PSDEV_NO_JOURNAL:
		izo_channels[minor].uc_no_journal = newval;
		break;

	case PSDEV_NO_UPCALL:
		izo_channels[minor].uc_no_upcall = newval;
		break;

#ifdef PRESTO_DEBUG
	case PSDEV_ERRORVAL: {
		/* If we have a positive arg, set a breakpoint for that
		 * value.  If we have a negative arg, make that device
		 * read-only.  FIXME  It would be much better to only
		 * allow setting the underlying device read-only for the
		 * current presto cache.
		 */
		int errorval = izo_channels[minor].uc_errorval;
		if (errorval < 0) {
			if (newval == 0)
				set_device_ro(-errorval, 0);
			else
				CERROR("device %s already read only\n",
				       kdevname(-errorval));
		} else {
			if (newval < 0)
				set_device_ro(-newval, 1);
			izo_channels[minor].uc_errorval = newval;
			CDEBUG(D_PSDEV, "setting errorval to %d\n", newval);
		}

		break;
	}
#endif

	case PSDEV_TRACE:
	case PSDEV_DEBUG:
	case PSDEV_BYTES_TO_CLOSE:
	default:
		CDEBUG(D_PSDEV,
		       "ioctl: dosetopt: minor %d, bad optname 0x%x, \n",
		       minor, opt->optname);

		retval = -EINVAL;
	}

	EXIT;
	return retval;
}

int dogetopt(int minor, struct psdev_opt *opt)
{
	int retval = 0;

	ENTRY;

	switch(opt->optname) {

	case PSDEV_TIMEOUT:
		opt->optval = izo_channels[minor].uc_timeout;
		break;

	case PSDEV_HARD:
		opt->optval = izo_channels[minor].uc_hard;
		break;

	case PSDEV_NO_FILTER:
		opt->optval = izo_channels[minor].uc_no_filter;
		break;

	case PSDEV_NO_JOURNAL:
		opt->optval = izo_channels[minor].uc_no_journal;
		break;

	case PSDEV_NO_UPCALL:
		opt->optval = izo_channels[minor].uc_no_upcall;
		break;

#ifdef PSDEV_DEBUG
	case PSDEV_ERRORVAL: {
		int errorval = izo_channels[minor].uc_errorval;
		if (errorval < 0 && is_read_only(-errorval))
			CERROR("device %s has been set read-only\n",
			       kdevname(-errorval));
		opt->optval = izo_channels[minor].uc_errorval;
		break;
	}
#endif

	case PSDEV_TRACE:
	case PSDEV_DEBUG:
	case PSDEV_BYTES_TO_CLOSE:
	default:
		CDEBUG(D_PSDEV,
		       "ioctl: dogetopt: minor %d, bad optval 0x%x, \n",
		       minor, opt->optname);

		retval = -EINVAL;
	}

	EXIT;
	return retval;
}
#endif


/* allocate the tables for the presto devices. We need
 * sizeof(proto_channel_table)/sizeof(proto_channel_table[0])
 * entries for each dev
 */
int /* __init */ init_intermezzo_sysctl(void)
{
	int i;
	int total_dev = MAX_CHANNEL;
	int entries_per_dev = sizeof(proto_psdev_table) /
		sizeof(proto_psdev_table[0]);
	int total_entries = entries_per_dev * total_dev;
	ctl_table *dev_ctl_table;

	PRESTO_ALLOC(dev_ctl_table, sizeof(ctl_table) * total_entries);

	if (! dev_ctl_table) {
		CERROR("WARNING: presto couldn't allocate dev_ctl_table\n");
		EXIT;
		return -ENOMEM;
	}

	/* now fill in the entries ... we put the individual presto<x>
	 * entries at the end of the table, and the per-presto stuff
	 * starting at the front.  We assume that the compiler makes
	 * this code more efficient, but really, who cares ... it
	 * happens once per reboot.
	 */
	for(i = 0; i < total_dev; i++) {
		/* entry for this /proc/sys/intermezzo/intermezzo"i" */
		ctl_table *psdev = &presto_table[i + PRESTO_PRIMARY_CTLCNT];
		/* entries for the individual "files" in this "directory" */
		ctl_table *psdev_entries = &dev_ctl_table[i * entries_per_dev];
		/* init the psdev and psdev_entries with the prototypes */
		*psdev = proto_channel_entry;
		memcpy(psdev_entries, proto_psdev_table,
		       sizeof(proto_psdev_table));
		/* now specialize them ... */
		/* the psdev has to point to psdev_entries, and fix the number */
		psdev->ctl_name = psdev->ctl_name + i + 1; /* sorry */

		PRESTO_ALLOC((void*)psdev->procname, PROCNAME_SIZE);
		if (!psdev->procname) {
			PRESTO_FREE(dev_ctl_table,
				    sizeof(ctl_table) * total_entries);
			return -ENOMEM;
		}
		sprintf((char *) psdev->procname, "intermezzo%d", i);
		/* hook presto into */
		psdev->child = psdev_entries;

		/* now for each psdev entry ... */
		psdev_entries[0].data = &(izo_channels[i].uc_hard);
		psdev_entries[1].data = &(izo_channels[i].uc_no_filter);
		psdev_entries[2].data = &(izo_channels[i].uc_no_journal);
		psdev_entries[3].data = &(izo_channels[i].uc_no_upcall);
		psdev_entries[4].data = &(izo_channels[i].uc_timeout);
#ifdef PRESTO_DEBUG
		psdev_entries[5].data = &(izo_channels[i].uc_errorval);
#endif
	}


#ifdef CONFIG_SYSCTL
	if ( !intermezzo_table_header )
		intermezzo_table_header =
			register_sysctl_table(intermezzo_table, 0);
#endif
#ifdef CONFIG_PROC_FS
	proc_fs_intermezzo = proc_mkdir("intermezzo", proc_root_fs);
	proc_fs_intermezzo->owner = THIS_MODULE;
	create_proc_info_entry("mounts", 0, proc_fs_intermezzo, 
			       intermezzo_mount_get_info);
#endif
	return 0;
}

void cleanup_intermezzo_sysctl(void)
{
	int total_dev = MAX_CHANNEL;
	int entries_per_dev = sizeof(proto_psdev_table) /
		sizeof(proto_psdev_table[0]);
	int total_entries = entries_per_dev * total_dev;
	int i;

#ifdef CONFIG_SYSCTL
	if ( intermezzo_table_header )
		unregister_sysctl_table(intermezzo_table_header);
	intermezzo_table_header = NULL;
#endif
	for(i = 0; i < total_dev; i++) {
		/* entry for this /proc/sys/intermezzo/intermezzo"i" */
		ctl_table *psdev = &presto_table[i + PRESTO_PRIMARY_CTLCNT];
		PRESTO_FREE(psdev->procname, PROCNAME_SIZE);
	}
	/* presto_table[PRESTO_PRIMARY_CTLCNT].child points to the
	 * dev_ctl_table previously allocated in init_intermezzo_psdev()
	 */
	PRESTO_FREE(presto_table[PRESTO_PRIMARY_CTLCNT].child, sizeof(ctl_table) * total_entries);

#if CONFIG_PROC_FS
	remove_proc_entry("mounts", proc_fs_intermezzo);
	remove_proc_entry("intermezzo", proc_root_fs);
#endif
}

