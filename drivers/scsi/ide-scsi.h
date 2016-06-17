/*
 * linux/drivers/scsi/ide-scsi.h
 *
 * Copyright (C) 1996, 1997 Gadi Oxman <gadio@netvision.net.il>
 */

#ifndef IDESCSI_H
#define IDESCSI_H

extern int idescsi_detect (Scsi_Host_Template *host_template);
extern int idescsi_release (struct Scsi_Host *host);
extern const char *idescsi_info (struct Scsi_Host *host);
extern int idescsi_ioctl (Scsi_Device *dev, int cmd, void *arg);
extern int idescsi_queue (Scsi_Cmnd *cmd, void (*done)(Scsi_Cmnd *));
extern int idescsi_abort (Scsi_Cmnd *cmd);
extern int idescsi_reset (Scsi_Cmnd *cmd, unsigned int resetflags);
extern int idescsi_bios (Disk *disk, kdev_t dev, int *parm);

#define IDESCSI  {							\
	name:            "idescsi",		/* name		*/	\
	detect:          idescsi_detect,	/* detect	*/	\
	release:         idescsi_release,	/* release	*/	\
	info:            idescsi_info,		/* info		*/	\
	ioctl:           idescsi_ioctl,		/* ioctl        */	\
	queuecommand:    idescsi_queue,		/* queuecommand */	\
	abort:           idescsi_abort,		/* abort	*/	\
	reset:           idescsi_reset,		/* reset	*/	\
	bios_param:      idescsi_bios,		/* bios_param	*/	\
	can_queue:       10,			/* can_queue	*/	\
	this_id:         -1,			/* this_id	*/	\
	sg_tablesize:    256,			/* sg_tablesize	*/	\
	cmd_per_lun:     5,			/* cmd_per_lun	*/	\
	use_clustering:  DISABLE_CLUSTERING,	/* clustering	*/	\
	emulated:        1			/* emulated     */	\
}

#endif /* IDESCSI_H */
