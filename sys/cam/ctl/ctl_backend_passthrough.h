#ifndef	_CTL_B_
#define	_CTL_B_
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/condvar.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <cam/cam.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_error.h>


typedef enum {
	CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED	= 0x01,
	CTL_BE_PASSTHROUGH_LUN_CONFIG_ERR	= 0x02,
	CTL_BE_PASSTHROUGH_LUN_WAITING	= 0x04
} ctl_be_passthrough_lun_flags;

struct ctl_be_passthrough_lun {
	struct ctl_lun_create_params params;
	char lunname[32];
	struct cam_periph *periph;
	uint64_t size_bytes;
	uint64_t size_blocks;
	int num_threads;
	struct ctl_be_passthrough_softc *softc;
	ctl_be_passthrough_lun_flags flags;
	STAILQ_ENTRY(ctl_be_passthrough_lun) links;
	struct ctl_be_lun cbe_lun;
	STAILQ_HEAD(, ctl_io_hdr) cont_queue;
	struct mtx_padalign queue_lock;
};




struct ctl_be_passthrough_softc {
	struct mtx lock;
	int num_luns;
	STAILQ_HEAD(, ctl_be_passthrough_lun) lun_list;
};


int ctl_backend_passthrough_create(struct cam_periph *periph,struct ctl_lun_req *lun_req);
#endif
