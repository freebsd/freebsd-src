/*
 * advansys.h - Linux Host Driver for AdvanSys SCSI Adapters
 * 
 * Copyright (c) 1995-2000 Advanced System Products, Inc.
 * Copyright (c) 2000-2001 ConnectCom Solutions, Inc.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 *
 * As of March 8, 2000 Advanced System Products, Inc. (AdvanSys)
 * changed its name to ConnectCom Solutions, Inc.
 *
 * There is an AdvanSys Linux WWW page at:
 *  http://www.connectcom.net/downloads/software/os/linux.html
 *  http://www.advansys.com/linux.html
 *
 * The latest released version of the AdvanSys driver is available at:
 *  ftp://ftp.advansys.com/pub/linux/linux.tgz
 *  ftp://ftp.connectcom.net/pub/linux/linux.tgz
 *
 * Please send questions, comments, bug reports to:
 *  linux@connectcom.net or bfrey@turbolinux.com.cn
 */

#ifndef _ADVANSYS_H
#define _ADVANSYS_H

#include <linux/config.h>
#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif /* LINUX_VERSION_CODE */

/* Convert Linux Version, Patch-level, Sub-level to LINUX_VERSION_CODE. */
#define ASC_LINUX_VERSION(V, P, S)    (((V) * 65536) + ((P) * 256) + (S))
/* Driver supported only in version 2.2 and version >= 2.4. */
#if LINUX_VERSION_CODE < ASC_LINUX_VERSION(2,2,0) || \
    (LINUX_VERSION_CODE > ASC_LINUX_VERSION(2,3,0) && \
     LINUX_VERSION_CODE < ASC_LINUX_VERSION(2,4,0))
#error "AdvanSys driver supported only in 2.2 and 2.4 or greater kernels."
#endif
#define ASC_LINUX_KERNEL22 (LINUX_VERSION_CODE < ASC_LINUX_VERSION(2,4,0))
#define ASC_LINUX_KERNEL24 (LINUX_VERSION_CODE >= ASC_LINUX_VERSION(2,4,0))

/*
 * Scsi_Host_Template function prototypes.
 */
int advansys_detect(Scsi_Host_Template *);
int advansys_release(struct Scsi_Host *);
const char *advansys_info(struct Scsi_Host *);
int advansys_queuecommand(Scsi_Cmnd *, void (* done)(Scsi_Cmnd *));
int advansys_reset(Scsi_Cmnd *);
int advansys_biosparam(Disk *, kdev_t, int[]);
#ifdef CONFIG_PROC_FS
#if LINUX_VERSION_CODE < ASC_LINUX_VERSION(2,3,28)
extern struct proc_dir_entry proc_scsi_advansys;
#endif /* version < v2.3.28 */
int advansys_proc_info(char *, char **, off_t, int, int, int);
#else /* !defined(CONFIG_PROC_FS) */
#define advansys_proc_info      NULL
#endif /* !defined(CONFIG_PROC_FS) */

/* init/main.c setup function */
void advansys_setup(char *, int *);

/*
 * AdvanSys Host Driver Scsi_Host_Template (struct SHT) from hosts.h.
 */
#if ASC_LINUX_KERNEL24
#define ADVANSYS { \
    proc_name:                  "advansys", \
    proc_info:                  advansys_proc_info, \
    name:                       "advansys", \
    detect:                     advansys_detect, \
    release:                    advansys_release, \
    info:                       advansys_info, \
    queuecommand:               advansys_queuecommand, \
    use_new_eh_code:		1, \
    eh_bus_reset_handler:	advansys_reset, \
    bios_param:                 advansys_biosparam, \
    /* \
     * Because the driver may control an ISA adapter 'unchecked_isa_dma' \
     * must be set. The flag will be cleared in advansys_detect for non-ISA \
     * adapters. Refer to the comment in scsi_module.c for more information. \
     */ \
    unchecked_isa_dma:          1, \
    /* \
     * All adapters controlled by this driver are capable of large \
     * scatter-gather lists. According to the mid-level SCSI documentation \
     * this obviates any performance gain provided by setting \
     * 'use_clustering'. But empirically while CPU utilization is increased \
     * by enabling clustering, I/O throughput increases as well. \
     */ \
    use_clustering:             ENABLE_CLUSTERING, \
}
#elif ASC_LINUX_KERNEL22
#define ADVANSYS { \
    proc_info:                  advansys_proc_info, \
    name:                       "advansys", \
    detect:                     advansys_detect, \
    release:                    advansys_release, \
    info:                       advansys_info, \
    queuecommand:               advansys_queuecommand, \
    use_new_eh_code:		1, \
    eh_bus_reset_handler:	advansys_reset, \
    bios_param:                 advansys_biosparam, \
    /* \
     * Because the driver may control an ISA adapter 'unchecked_isa_dma' \
     * must be set. The flag will be cleared in advansys_detect for non-ISA \
     * adapters. Refer to the comment in scsi_module.c for more information. \
     */ \
    unchecked_isa_dma:          1, \
    /* \
     * All adapters controlled by this driver are capable of large \
     * scatter-gather lists. According to the mid-level SCSI documentation \
     * this obviates any performance gain provided by setting \
     * 'use_clustering'. But empirically while CPU utilization is increased \
     * by enabling clustering, I/O throughput increases as well. \
     */ \
    use_clustering:             ENABLE_CLUSTERING, \
}
#endif
#endif /* _ADVANSYS_H */
