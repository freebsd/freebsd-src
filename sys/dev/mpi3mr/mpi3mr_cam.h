/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2025, Broadcom Inc. All rights reserved.
 * Support: <fbsd-storage-driver.pdl@broadcom.com>
 *
 * Authors: Sumit Saxena <sumit.saxena@broadcom.com>
 *	    Chandrakanth Patil <chandrakanth.patil@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 3. Neither the name of the Broadcom Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 *
 * Mail to: Broadcom Inc 1320 Ridder Park Dr, San Jose, CA 95131
 *
 * Broadcom Inc. (Broadcom) MPI3MR Adapter FreeBSD
 */
#include "mpi3mr.h"

struct mpi3mr_fw_event_work;

struct mpi3mr_throttle_group_info {
	U8 io_divert;
	U16 fw_qd;
	U16 modified_qd;
	U16 id;
	U32 high;
	U32 low;
	mpi3mr_atomic_t pend_large_data_sz;
};

struct mpi3mr_tgt_dev_sassata {
	U64 sas_address;
	U16 dev_info;
};

struct mpi3mr_tgt_dev_pcie {
	U32 mdts;
	U16 capb;
	U8 pgsz;
	U8 abort_to;
	U8 reset_to;
	U16 dev_info;
};

struct mpi3mr_tgt_dev_volume {
	U8 state;
	U16 tg_id;
	U32 tg_high;
	U32 tg_low;
	struct mpi3mr_throttle_group_info *tg;
};

typedef union _mpi3mr_form_spec_inf {
	struct mpi3mr_tgt_dev_sassata sassata_inf;
	struct mpi3mr_tgt_dev_pcie pcie_inf;
	struct mpi3mr_tgt_dev_volume vol_inf;
} mpi3mr_form_spec_inf;

struct mpi3mr_target {
	uint16_t dev_handle;
	uint16_t slot;
	uint16_t per_id;
	uint8_t dev_type;
	volatile uint8_t	is_hidden;
	volatile uint8_t	dev_removed;
	U8 dev_removedelay;
	mpi3mr_atomic_t block_io;
	uint8_t exposed_to_os;
	uint16_t qdepth;
	uint64_t wwid;
	mpi3mr_form_spec_inf dev_spec;
	uint16_t	tid;
	uint16_t        exp_dev_handle;
	uint16_t        phy_num;
	uint64_t	sasaddr;
	uint16_t	parent_handle;
	uint64_t	parent_sasaddr;
	uint32_t	parent_devinfo;
	mpi3mr_atomic_t outstanding;
	uint8_t		scsi_req_desc_type;
	TAILQ_ENTRY(mpi3mr_target)	tgt_next;
	uint16_t	handle;
	uint8_t		link_rate;
	uint8_t		encl_level_valid;
	uint8_t		encl_level;
	char		connector_name[4];
	uint64_t	devname;
	uint32_t	devinfo;
	uint16_t	encl_handle;
	uint16_t	encl_slot;
	uint8_t		flags;
#define MPI3MRSAS_TARGET_INREMOVAL	(1 << 3)
	uint8_t		io_throttle_enabled;
	uint8_t		io_divert;
	struct mpi3mr_throttle_group_info *throttle_group;
	uint64_t	q_depth;
	enum mpi3mr_target_state state;
	uint16_t	ws_len;
};

struct mpi3mr_cam_softc {
	struct mpi3mr_softc	*sc;
	u_int			flags;
#define MPI3MRSAS_IN_DISCOVERY	(1 << 0)
#define MPI3MRSAS_IN_STARTUP	(1 << 1)
#define MPI3MRSAS_DISCOVERY_TIMEOUT_PENDING	(1 << 2)
#define MPI3MRSAS_QUEUE_FROZEN	(1 << 3)
#define	MPI3MRSAS_SHUTDOWN		(1 << 4)
	u_int			maxtargets;
	struct cam_devq		*devq;
	struct cam_sim		*sim;
	struct cam_path		*path;
	struct intr_config_hook	sas_ich;
	struct callout		discovery_callout;
	struct mpi3mr_event_handle	*mpi3mr_eh;

	u_int                   startup_refcount;
	struct proc             *sysctl_proc;
	struct taskqueue	*ev_tq;
	struct task		ev_task;
	TAILQ_HEAD(, mpi3mr_fw_event_work)	ev_queue;
	TAILQ_HEAD(, mpi3mr_target)		tgt_list;
};

MALLOC_DECLARE(M_MPI3MRSAS);

static __inline void
mpi3mr_set_ccbstatus(union ccb *ccb, int status)
{
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	ccb->ccb_h.status |= status;
}

static __inline int
mpi3mr_get_ccbstatus(union ccb *ccb)
{
	return (ccb->ccb_h.status & CAM_STATUS_MASK);
}

static __inline void mpi3mr_print_cdb(union ccb *ccb)
{
	struct ccb_scsiio *csio;
	struct mpi3mr_cam_softc *cam_sc;
	struct cam_sim *sim;
	int i;
	
	sim = xpt_path_sim(ccb->ccb_h.path);
	cam_sc = cam_sim_softc(sim);

	csio = &ccb->csio;

	mpi3mr_dprint(cam_sc->sc, MPI3MR_INFO, "tgtID: %d  CDB: ", csio->ccb_h.target_id);
	for (i = 0; i < csio->cdb_len; i++)
		printf("%x ", csio->cdb_io.cdb_bytes[i]);
	
	printf("\n");
}

void mpi3mr_rescan_target(struct mpi3mr_softc *sc, struct mpi3mr_target *targ);
void mpi3mr_discovery_end(struct mpi3mr_cam_softc *sassc);
void mpi3mr_prepare_for_tm(struct mpi3mr_softc *sc, struct mpi3mr_cmd *tm,
    struct mpi3mr_target *target, lun_id_t lun_id);
void mpi3mr_startup_increment(struct mpi3mr_cam_softc *sassc);
void mpi3mr_startup_decrement(struct mpi3mr_cam_softc *sassc);

void mpi3mr_firmware_event_work(void *arg, int pending);
int mpi3mr_check_id(struct mpi3mr_cam_softc *sassc, int id);
int
mpi3mr_cam_attach(struct mpi3mr_softc *sc);
int
mpi3mr_cam_detach(struct mpi3mr_softc *sc);
void
mpi3mr_evt_handler(struct mpi3mr_softc *sc, uintptr_t data,
    MPI3_EVENT_NOTIFICATION_REPLY *event);
