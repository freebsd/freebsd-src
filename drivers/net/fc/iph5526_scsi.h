#ifndef IPH5526_SCSI_H
#define IPH5526_SCSI_H

#define IPH5526_CAN_QUEUE	32
#define IPH5526_SCSI_FC { 						 				\
        name:                   "Interphase 5526 Fibre Channel SCSI Adapter",   \
        detect:                 iph5526_detect,                  \
        release:                iph5526_release,                 \
        info:                   iph5526_info,                    \
        queuecommand:           iph5526_queuecommand,            \
		bios_param:				iph5526_biosparam,               \
        can_queue:              IPH5526_CAN_QUEUE,               \
        this_id:                -1,                              \
        sg_tablesize:           255,                             \
        cmd_per_lun:            8,                               \
        use_clustering:         DISABLE_CLUSTERING,              \
        eh_abort_handler:       iph5526_abort,                   \
        eh_device_reset_handler:NULL,                            \
        eh_bus_reset_handler:   NULL,                            \
        eh_host_reset_handler:  NULL,                            \
}

int iph5526_detect(Scsi_Host_Template *tmpt);
int iph5526_queuecommand(Scsi_Cmnd *Cmnd, void (*done) (Scsi_Cmnd *));
int iph5526_release(struct Scsi_Host *host);
int iph5526_abort(Scsi_Cmnd *Cmnd);
const char *iph5526_info(struct Scsi_Host *host);
int iph5526_biosparam(Disk * disk, kdev_t n, int ip[]);

#endif

