#ifndef _QLOGICFAS_H
#define _QLOGICFAS_H

int qlogicfas_detect(Scsi_Host_Template * );
int qlogicfas_release(struct Scsi_Host *);
const char * qlogicfas_info(struct Scsi_Host *);
int qlogicfas_command(Scsi_Cmnd *);
int qlogicfas_queuecommand(Scsi_Cmnd *, void (* done)(Scsi_Cmnd *));
int qlogicfas_abort(Scsi_Cmnd *);
int qlogicfas_reset(Scsi_Cmnd *, unsigned int);
int qlogicfas_biosparam(Disk *, kdev_t, int[]);

#ifndef NULL
#define NULL (0)
#endif

#ifdef PCMCIA
#define __QLINIT __devinit
#else
#define __QLINIT __init
#endif

#define QLOGICFAS {		\
	detect:         qlogicfas_detect,	\
        release:        qlogicfas_release,      \
	info:           qlogicfas_info,		\
	command:        qlogicfas_command, 	\
	queuecommand:   qlogicfas_queuecommand,	\
	abort:          qlogicfas_abort,	\
	reset:          qlogicfas_reset,	\
	bios_param:     qlogicfas_biosparam,	\
	can_queue:      0,			\
	this_id:        -1,			\
	sg_tablesize:   SG_ALL,			\
	cmd_per_lun:    1,			\
	use_clustering: DISABLE_CLUSTERING	\
}

#endif /* _QLOGICFAS_H */



