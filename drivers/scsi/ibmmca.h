/* 
 * Low Level Driver for the IBM Microchannel SCSI Subsystem
 * (Headerfile, see README.ibmmca for description of the IBM MCA SCSI-driver
 * For use under the GNU General Public License within the Linux-kernel project.
 * This include file works only correctly with kernel 2.4.0 or higher!!! */

#ifndef _IBMMCA_H
#define _IBMMCA_H

/* Common forward declarations for all Linux-versions: */

/* Interfaces to the midlevel Linux SCSI driver */
extern int ibmmca_proc_info (char *, char **, off_t, int, int, int);
extern int ibmmca_detect (Scsi_Host_Template *);
extern int ibmmca_release (struct Scsi_Host *);
extern int ibmmca_command (Scsi_Cmnd *);
extern int ibmmca_queuecommand (Scsi_Cmnd *, void (*done) (Scsi_Cmnd *));
extern int ibmmca_abort (Scsi_Cmnd *);
extern int ibmmca_reset (Scsi_Cmnd *, unsigned int);
extern int ibmmca_biosparam (Disk *, kdev_t, int *);

/*structure for /proc filesystem */
extern struct proc_dir_entry proc_scsi_ibmmca;

/*
 * 2/8/98
 * Note to maintainer of IBMMCA.  Do not change this initializer back to
 * the old format.  Please ask eric@andante.jic.com if you have any questions
 * about this, but it will break things in the future.
 */
#define IBMMCA {                                                      \
          proc_name:      "ibmmca",             /*proc_name*/         \
	  proc_info:	  ibmmca_proc_info,     /*proc info fn*/      \
          name:           "IBM SCSI-Subsystem", /*name*/              \
          detect:         ibmmca_detect,        /*detect fn*/         \
          release:        ibmmca_release,       /*release fn*/        \
          command:        ibmmca_command,       /*command fn*/        \
          queuecommand:   ibmmca_queuecommand,  /*queuecommand fn*/   \
          abort:          ibmmca_abort,         /*abort fn*/          \
          reset:          ibmmca_reset,         /*reset fn*/          \
          bios_param:     ibmmca_biosparam,     /*bios fn*/           \
          can_queue:      16,                   /*can_queue*/         \
          this_id:        7,                    /*set by detect*/     \
          sg_tablesize:   16,                   /*sg_tablesize*/      \
          cmd_per_lun:    1,                    /*cmd_per_lun*/       \
          unchecked_isa_dma: 0,                 /*32-Bit Busmaster */ \
          use_clustering: ENABLE_CLUSTERING     /*use_clustering*/    \
          }

#endif /* _IBMMCA_H */
