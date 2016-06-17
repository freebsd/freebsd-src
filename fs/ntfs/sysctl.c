/*
 * sysctl.c - System control stuff
 *
 * Copyright (C) 1997 Martin von Löwis
 * Copyright (C) 1997 Régis Duchesne
 */

#include "sysctl.h"

#ifdef DEBUG
#include <linux/locks.h>
#include <linux/sysctl.h>

int ntdebug = 0;

/* Add or remove the debug sysctl
 * Is this really the only file system with sysctls ?
 */
void ntfs_sysctl(int add)
{
#define FS_NTFS             1
	/* Definition of the sysctl */
	static ctl_table ntfs_sysctls[]={
		{FS_NTFS,                  /* ID */
		 "ntfs-debug",             /* name in /proc */
		 &ntdebug,sizeof(ntdebug), /* data ptr, data size */
		 0644,                     /* mode */
		 0,                        /* child */
		 proc_dointvec,            /* proc handler */
		 0,                        /* strategy */
		 0,                        /* proc control block */
		 0,0},                     /* extra */
		{0}
	};
	/* Define the parent file : /proc/sys/fs */
	static ctl_table sysctls_root[]={
		{CTL_FS,
		 "fs",
		 NULL,0,
		 0555,
		 ntfs_sysctls},
		{0}
	};
	static struct ctl_table_header *sysctls_root_header = NULL;

	if(add){
		if(!sysctls_root_header)
			sysctls_root_header = register_sysctl_table(sysctls_root, 0);
	} else if(sysctls_root_header) {
		unregister_sysctl_table(sysctls_root_header);
		sysctls_root_header = NULL;
	}
}
#endif /* DEBUG */

