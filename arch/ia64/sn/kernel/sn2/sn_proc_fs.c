/*
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */
#include <linux/config.h>
#include <asm/uaccess.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/sn_cpuid.h>


static int partition_id_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data) {

	return sprintf(page, "%d\n", sn_local_partid());
}

static struct proc_dir_entry * sgi_proc_dir;

void
register_sn_partition_id(void) {
	struct proc_dir_entry *entry;

	if (!sgi_proc_dir) {
		sgi_proc_dir = proc_mkdir("sgi_sn", 0);
	}
	entry = create_proc_entry("partition_id", 0444, sgi_proc_dir);
	if (entry) {
		entry->nlink = 1;
		entry->data = 0;
		entry->read_proc = partition_id_read_proc;
		entry->write_proc = NULL;
	}
}

static int
system_serial_number_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data) {
	return sprintf(page, "%s\n", sn_system_serial_number());
}

static int
licenseID_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data) {
	return sprintf(page, "0x%lx\n",sn_partition_serial_number_val());
}

void
register_sn_serial_numbers(void) {
	struct proc_dir_entry *entry;

	if (!sgi_proc_dir) {
		sgi_proc_dir = proc_mkdir("sgi_sn", 0);
	}
	entry = create_proc_entry("system_serial_number", 0444, sgi_proc_dir);
	if (entry) {
		entry->nlink = 1;
		entry->data = 0;
		entry->read_proc = system_serial_number_read_proc;
		entry->write_proc = NULL;
	}
	entry = create_proc_entry("licenseID", 0444, sgi_proc_dir);
	if (entry) {
		entry->nlink = 1;
		entry->data = 0;
		entry->read_proc = licenseID_read_proc;
		entry->write_proc = NULL;
	}
}

// Disable forced interrupts, but leave the code in, just in case.
int sn_force_interrupt_flag = 0;

static int
sn_force_interrupt_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data) {
	if (sn_force_interrupt_flag) {
		return sprintf(page, "Force interrupt is enabled\n");
	}
	return sprintf(page, "Force interrupt is disabled\n");
}

static int 
sn_force_interrupt_write_proc(struct file *file, const char *buffer,
                                        unsigned long count, void *data)
{
	if (*buffer == '0') {
		sn_force_interrupt_flag = 0;
	} else {
		sn_force_interrupt_flag = 1;
	}
	return 1;
}

void
register_sn_force_interrupt(void) {
	struct proc_dir_entry *entry;

	if (!sgi_proc_dir) {
		sgi_proc_dir = proc_mkdir("sgi_sn", 0);
	}
	entry = create_proc_entry("sn_force_interrupt",0444, sgi_proc_dir);
	if (entry) {
		entry->nlink = 1;
		entry->data = 0;
		entry->read_proc = sn_force_interrupt_read_proc;
		entry->write_proc = sn_force_interrupt_write_proc;
	}
}

extern int sn_linkstats_get(char *);
extern int sn_linkstats_reset(unsigned long);

static int
sn_linkstats_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data) {
       
	return sn_linkstats_get(page);
}

static int 
sn_linkstats_write_proc(struct file *file, const char *buffer,
                                        unsigned long count, void *data)
{
	char		s[64];
	unsigned long	msecs;
	int		e = count;

	if (copy_from_user(s, buffer, count < sizeof(s) ? count : sizeof(s)))
		e = -EFAULT;
	else {
		if (sscanf(s, "%lu", &msecs) != 1 || msecs < 5)
			/* at least 5 milliseconds between updates */
			e = -EINVAL;
		else
			sn_linkstats_reset(msecs);
	}

	return e;
}

void
register_sn_linkstats(void) {
	struct proc_dir_entry *entry;

	if (!sgi_proc_dir) {
		sgi_proc_dir = proc_mkdir("sgi_sn", 0);
	}
	entry = create_proc_entry("linkstats", 0444, sgi_proc_dir);
	if (entry) {
		entry->nlink = 1;
		entry->data = 0;
		entry->read_proc = sn_linkstats_read_proc;
		entry->write_proc = sn_linkstats_write_proc;
	}
}

#define SHUB_MAX_VERSION 3
static struct proc_dir_entry **proc_entries;
static char* shub_revision[SHUB_MAX_VERSION+1] = {
	"unknown",
	"1.0",
	"1.1",
	"1.2"
};

static int
read_shub_info_entry(char* page, char **start, off_t off, int count, int *eof,
		     void* data) {
	unsigned long id;
	int rev;
	int nasid = (long) data; /* Data contains NASID of this node */
	int len = 0;

	id = REMOTE_HUB_L(nasid, SH_SHUB_ID);
	rev = (id & SH_SHUB_ID_REVISION_MASK) >> SH_SHUB_ID_REVISION_SHFT;
	len += sprintf(&page[len], "type     : SHub\n");
	len += sprintf(&page[len], "revision : %s\n", 
		       (rev <= SHUB_MAX_VERSION) ? shub_revision[rev] : "unknown");
	len += sprintf(&page[len], "nasid    : %d\n", nasid);

	return len;
}

static void
register_sn_nodes(void) {
	struct proc_dir_entry **entp;
	cnodeid_t cnodeid;
	nasid_t nasid;
	char name[11];

	if (!sgi_proc_dir) {
		sgi_proc_dir = proc_mkdir("sgi_sn", 0);
	}

	proc_entries = kmalloc(numnodes * sizeof(struct proc_dir_entry *),
			       GFP_KERNEL);

	for (cnodeid = 0, entp = proc_entries;
	     cnodeid < numnodes;
	     cnodeid++, entp++) {
		sprintf(name, "node%d", cnodeid);
		*entp = proc_mkdir(name, sgi_proc_dir);
		nasid = cnodeid_to_nasid(cnodeid);
		create_proc_read_entry(
			"hubinfo", 0, *entp, read_shub_info_entry,
			(void*) (long) nasid);
	}
}

void
register_sn_procfs(void) {
	register_sn_partition_id();
	register_sn_serial_numbers();
	register_sn_force_interrupt();
	register_sn_linkstats();
	register_sn_nodes();
}

#endif /* CONFIG_PROC_FS */
