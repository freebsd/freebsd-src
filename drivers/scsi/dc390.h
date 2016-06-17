/***********************************************************************
 *	FILE NAME : DC390.H					       *
 *	     BY   : C.L. Huang					       *
 *	Description: Device Driver for Tekram DC-390(T) PCI SCSI       *
 *		     Bus Master Host Adapter			       *
 ***********************************************************************/
/* $Id: dc390.h,v 2.43.2.22 2000/12/20 00:39:36 garloff Exp $ */

/*
 * DC390/AMD 53C974 driver, header file
 */

#ifndef DC390_H
#define DC390_H

#include <linux/version.h>
#ifndef KERNEL_VERSION
# define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#define DC390_BANNER "Tekram DC390/AM53C974"
#define DC390_VERSION "2.0f 2000-12-20"

/* We don't have eh_abort_handler, eh_device_reset_handler, 
 * eh_bus_reset_handler, eh_host_reset_handler yet! 
 * So long: Use old exception handling :-( */
#define OLD_EH

#if LINUX_VERSION_CODE < KERNEL_VERSION (2,1,70) || defined (OLD_EH)
# define NEW_EH
#else
# define NEW_EH use_new_eh_code: 1,
# define USE_NEW_EH
#endif

#if defined(HOSTS_C) || defined(MODULE) || LINUX_VERSION_CODE > KERNEL_VERSION(2,3,99)

extern int DC390_detect(Scsi_Host_Template *psht);
extern int DC390_queue_command(Scsi_Cmnd *cmd, void (*done)(Scsi_Cmnd *));
extern int DC390_abort(Scsi_Cmnd *cmd);
extern int DC390_reset(Scsi_Cmnd *cmd, unsigned int resetFlags);
extern int DC390_bios_param(Disk *disk, kdev_t devno, int geom[]);

#ifdef MODULE
static int DC390_release(struct Scsi_Host *);
#else
# define DC390_release NULL
#endif

extern int DC390_proc_info(char *buffer, char **start, off_t offset, int length, int hostno, int inout);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,30)
#define DC390_T    {					\
   proc_name:      "tmscsim",                           \
   proc_info:      DC390_proc_info,			\
   name:           DC390_BANNER " V" DC390_VERSION,	\
   detect:         DC390_detect,			\
   release:        DC390_release,			\
   queuecommand:   DC390_queue_command,			\
   abort:          DC390_abort,				\
   reset:          DC390_reset,				\
   bios_param:     DC390_bios_param,			\
   can_queue:      42,					\
   this_id:        7,					\
   sg_tablesize:   SG_ALL,				\
   cmd_per_lun:    16,					\
   NEW_EH						\
   unchecked_isa_dma: 0,				\
   use_clustering: DISABLE_CLUSTERING			\
   }
#else
extern struct proc_dir_entry DC390_proc_scsi_tmscsim;
#define DC390_T    {					\
   proc_dir:       &DC390_proc_scsi_tmscsim,		\
   proc_info:      DC390_proc_info,			\
   name:           DC390_BANNER " V" DC390_VERSION,	\
   detect:         DC390_detect,			\
   release:        DC390_release,			\
   queuecommand:   DC390_queue_command,			\
   abort:          DC390_abort,				\
   reset:          DC390_reset,				\
   bios_param:     DC390_bios_param,			\
   can_queue:      42,					\
   this_id:        7,					\
   sg_tablesize:   SG_ALL,				\
   cmd_per_lun:    16,					\
   NEW_EH						\
   unchecked_isa_dma: 0,				\
   use_clustering: DISABLE_CLUSTERING			\
   }
#endif
#endif /* defined(HOSTS_C) || defined(MODULE) */

#endif /* DC390_H */
