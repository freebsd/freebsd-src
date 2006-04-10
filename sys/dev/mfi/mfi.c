/*-
 * Copyright (c) 2006 IronPort Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mfi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/rman.h>
#include <sys/bus_dma.h>
#include <sys/bio.h>
#include <sys/ioccom.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/mfi/mfireg.h>
#include <dev/mfi/mfi_ioctl.h>
#include <dev/mfi/mfivar.h>

static int	mfi_alloc_commands(struct mfi_softc *);
static void	mfi_release_command(struct mfi_command *cm);
static int	mfi_comms_init(struct mfi_softc *);
static int	mfi_polled_command(struct mfi_softc *, struct mfi_command *);
static int	mfi_get_controller_info(struct mfi_softc *);
static void	mfi_data_cb(void *, bus_dma_segment_t *, int, int);
static void	mfi_startup(void *arg);
static void	mfi_intr(void *arg);
static void	mfi_enable_intr(struct mfi_softc *sc);
static void	mfi_ldprobe_inq(struct mfi_softc *sc);
static void	mfi_ldprobe_inq_complete(struct mfi_command *);
static int	mfi_ldprobe_capacity(struct mfi_softc *sc, int id);
static void	mfi_ldprobe_capacity_complete(struct mfi_command *);
static int	mfi_ldprobe_tur(struct mfi_softc *sc, int id);
static void	mfi_ldprobe_tur_complete(struct mfi_command *);
static int	mfi_add_ld(struct mfi_softc *sc, int id, uint64_t, uint32_t);
static struct mfi_command * mfi_bio_command(struct mfi_softc *);
static void	mfi_bio_complete(struct mfi_command *);
static int	mfi_mapcmd(struct mfi_softc *, struct mfi_command *);
static int	mfi_send_frame(struct mfi_softc *, struct mfi_command *);
static void	mfi_complete(struct mfi_softc *, struct mfi_command *);

/* Management interface */
static d_open_t		mfi_open;
static d_close_t	mfi_close;
static d_ioctl_t	mfi_ioctl;

static struct cdevsw mfi_cdevsw = {
	.d_version = 	D_VERSION,
	.d_flags =	0,
	.d_open = 	mfi_open,
	.d_close =	mfi_close,
	.d_ioctl =	mfi_ioctl,
	.d_name =	"mfi",
};

MALLOC_DEFINE(M_MFIBUF, "mfibuf", "Buffers for the MFI driver");

#define MFI_INQ_LENGTH SHORT_INQUIRY_LENGTH 

static int
mfi_transition_firmware(struct mfi_softc *sc)
{
	int32_t fw_state, cur_state;
	int max_wait, i;

	fw_state = MFI_READ4(sc, MFI_OMSG0) & MFI_FWSTATE_MASK;
	while (fw_state != MFI_FWSTATE_READY) {
		if (bootverbose)
			device_printf(sc->mfi_dev, "Waiting for firmware to "
			    "become ready\n");
		cur_state = fw_state;
		switch (fw_state) {
		case MFI_FWSTATE_FAULT:
			device_printf(sc->mfi_dev, "Firmware fault\n");
			return (ENXIO);
		case MFI_FWSTATE_WAIT_HANDSHAKE:
			MFI_WRITE4(sc, MFI_IDB, MFI_FWINIT_CLEAR_HANDSHAKE);
			max_wait = 2;
			break;
		case MFI_FWSTATE_OPERATIONAL:
			MFI_WRITE4(sc, MFI_IDB, MFI_FWINIT_READY);
			max_wait = 10;
			break;
		case MFI_FWSTATE_UNDEFINED:
		case MFI_FWSTATE_BB_INIT:
			max_wait = 2;
			break;
		case MFI_FWSTATE_FW_INIT:
		case MFI_FWSTATE_DEVICE_SCAN:
		case MFI_FWSTATE_FLUSH_CACHE:
			max_wait = 20;
			break;
		default:
			device_printf(sc->mfi_dev,"Unknown firmware state %d\n",
			    fw_state);
			return (ENXIO);
		}
		for (i = 0; i < (max_wait * 10); i++) {
			fw_state = MFI_READ4(sc, MFI_OMSG0) & MFI_FWSTATE_MASK;
			if (fw_state == cur_state)
				DELAY(100000);
			else
				break;
		}
		if (fw_state == cur_state) {
			device_printf(sc->mfi_dev, "firmware stuck in state "
			    "%#x\n", fw_state);
			return (ENXIO);
		}
	}
	return (0);
}

static void
mfi_addr32_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	uint32_t *addr;

	addr = arg;
	*addr = segs[0].ds_addr;
}

int
mfi_attach(struct mfi_softc *sc)
{
	uint32_t status;
	int error, commsz, framessz, sensesz;
	int frames, unit;

	mtx_init(&sc->mfi_io_lock, "MFI I/O lock", NULL, MTX_DEF);
	TAILQ_INIT(&sc->mfi_ld_tqh);

	mfi_initq_free(sc);
	mfi_initq_ready(sc);
	mfi_initq_busy(sc);
	mfi_initq_bio(sc);

	/* Before we get too far, see if the firmware is working */
	if ((error = mfi_transition_firmware(sc)) != 0) {
		device_printf(sc->mfi_dev, "Firmware not in READY state, "
		    "error %d\n", error);
		return (ENXIO);
	}

	/*
	 * Get information needed for sizing the contiguous memory for the
	 * frame pool.  Size down the sgl parameter since we know that
	 * we will never need more than what's required for MAXPHYS.
	 * It would be nice if these constants were available at runtime
	 * instead of compile time.
	 */
	status = MFI_READ4(sc, MFI_OMSG0);
	sc->mfi_max_fw_cmds = status & MFI_FWSTATE_MAXCMD_MASK;
	sc->mfi_max_fw_sgl = (status & MFI_FWSTATE_MAXSGL_MASK) >> 16;
	sc->mfi_total_sgl = min(sc->mfi_max_fw_sgl, ((MAXPHYS / PAGE_SIZE) +1));

	/*
	 * Create the dma tag for data buffers.  Used both for block I/O
	 * and for various internal data queries.
	 */
	if (bus_dma_tag_create( sc->mfi_parent_dmat,	/* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsize */
				sc->mfi_total_sgl,	/* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
				BUS_DMA_ALLOCNOW,	/* flags */
				busdma_lock_mutex,	/* lockfunc */
				&sc->mfi_io_lock,	/* lockfuncarg */
				&sc->mfi_buffer_dmat)) {
		device_printf(sc->mfi_dev, "Cannot allocate buffer DMA tag\n");
		return (ENOMEM);
	}

	/*
	 * Allocate DMA memory for the comms queues.  Keep it under 4GB for
	 * efficiency.  The mfi_hwcomms struct includes space for 1 reply queue
	 * entry, so the calculated size here will be will be 1 more than
	 * mfi_max_fw_cmds.  This is apparently a requirement of the hardware.
	 */
	commsz = (sizeof(uint32_t) * sc->mfi_max_fw_cmds) +
	    sizeof(struct mfi_hwcomms);
	if (bus_dma_tag_create( sc->mfi_parent_dmat,	/* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				commsz,			/* maxsize */
				1,			/* msegments */
				commsz,			/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&sc->mfi_comms_dmat)) {
		device_printf(sc->mfi_dev, "Cannot allocate comms DMA tag\n");
		return (ENOMEM);
	}
	if (bus_dmamem_alloc(sc->mfi_comms_dmat, (void **)&sc->mfi_comms,
	    BUS_DMA_NOWAIT, &sc->mfi_comms_dmamap)) {
		device_printf(sc->mfi_dev, "Cannot allocate comms memory\n");
		return (ENOMEM);
	}
	bzero(sc->mfi_comms, commsz);
	bus_dmamap_load(sc->mfi_comms_dmat, sc->mfi_comms_dmamap,
	    sc->mfi_comms, commsz, mfi_addr32_cb, &sc->mfi_comms_busaddr, 0);

	/*
	 * Allocate DMA memory for the command frames.  Keep them in the
	 * lower 4GB for efficiency.  Calculate the size of the frames at
	 * the same time; the frame is 64 bytes plus space for the SG lists.
	 * The assumption here is that the SG list will start at the second
	 * 64 byte segment of the frame and not use the unused bytes in the
	 * frame.  While this might seem wasteful, apparently the frames must
	 * be 64 byte aligned, so any savings would be negated by the extra
	 * alignment padding.
	 */
	if (sizeof(bus_addr_t) == 8) {
		sc->mfi_sgsize = sizeof(struct mfi_sg64);
		sc->mfi_flags |= MFI_FLAGS_SG64;
	} else {
		sc->mfi_sgsize = sizeof(struct mfi_sg32);
	}
	frames = (sc->mfi_sgsize * sc->mfi_total_sgl + MFI_FRAME_SIZE - 1) /
	    MFI_FRAME_SIZE + 1;
	sc->mfi_frame_size = frames * MFI_FRAME_SIZE;
	framessz = sc->mfi_frame_size * sc->mfi_max_fw_cmds;
	if (bus_dma_tag_create( sc->mfi_parent_dmat,	/* parent */
				64, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				framessz,		/* maxsize */
				1,			/* nsegments */
				framessz,		/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&sc->mfi_frames_dmat)) {
		device_printf(sc->mfi_dev, "Cannot allocate frame DMA tag\n");
		return (ENOMEM);
	}
	if (bus_dmamem_alloc(sc->mfi_frames_dmat, (void **)&sc->mfi_frames,
	    BUS_DMA_NOWAIT, &sc->mfi_frames_dmamap)) {
		device_printf(sc->mfi_dev, "Cannot allocate frames memory\n");
		return (ENOMEM);
	}
	bzero(sc->mfi_frames, framessz);
	bus_dmamap_load(sc->mfi_frames_dmat, sc->mfi_frames_dmamap,
	    sc->mfi_frames, framessz, mfi_addr32_cb, &sc->mfi_frames_busaddr,0);

	/*
	 * Allocate DMA memory for the frame sense data.  Keep them in the
	 * lower 4GB for efficiency
	 */
	sensesz = sc->mfi_max_fw_cmds * MFI_SENSE_LEN;
	if (bus_dma_tag_create( sc->mfi_parent_dmat,	/* parent */
				4, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				sensesz,		/* maxsize */
				1,			/* nsegments */
				sensesz,		/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&sc->mfi_sense_dmat)) {
		device_printf(sc->mfi_dev, "Cannot allocate sense DMA tag\n");
		return (ENOMEM);
	}
	if (bus_dmamem_alloc(sc->mfi_sense_dmat, (void **)&sc->mfi_sense,
	    BUS_DMA_NOWAIT, &sc->mfi_sense_dmamap)) {
		device_printf(sc->mfi_dev, "Cannot allocate sense memory\n");
		return (ENOMEM);
	}
	bus_dmamap_load(sc->mfi_sense_dmat, sc->mfi_sense_dmamap,
	    sc->mfi_sense, sensesz, mfi_addr32_cb, &sc->mfi_sense_busaddr, 0);

	if ((error = mfi_alloc_commands(sc)) != 0)
		return (error);

	if ((error = mfi_comms_init(sc)) != 0)
		return (error);

	if ((error = mfi_get_controller_info(sc)) != 0)
		return (error);

#if 0
	if ((error = mfi_setup_aen(sc)) != 0)
		return (error);
#endif

	/*
	 * Set up the interrupt handler.  XXX This should happen in
	 * mfi_pci.c
	 */
	sc->mfi_irq_rid = 0;
	if ((sc->mfi_irq = bus_alloc_resource_any(sc->mfi_dev, SYS_RES_IRQ,
	    &sc->mfi_irq_rid, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(sc->mfi_dev, "Cannot allocate interrupt\n");
		return (EINVAL);
	}
	if (bus_setup_intr(sc->mfi_dev, sc->mfi_irq, INTR_MPSAFE|INTR_TYPE_BIO,
	    mfi_intr, sc, &sc->mfi_intr)) {
		device_printf(sc->mfi_dev, "Cannot set up interrupt\n");
		return (EINVAL);
	}

	/* Register a config hook to probe the bus for arrays */
	sc->mfi_ich.ich_func = mfi_startup;
	sc->mfi_ich.ich_arg = sc;
	if (config_intrhook_establish(&sc->mfi_ich) != 0) {
		device_printf(sc->mfi_dev, "Cannot establish configuration "
		    "hook\n");
		return (EINVAL);
	}

	/*
	 * Register a shutdown handler.
	 */
	if ((sc->mfi_eh = EVENTHANDLER_REGISTER(shutdown_final, mfi_shutdown,
	    sc, SHUTDOWN_PRI_DEFAULT)) == NULL) {
		device_printf(sc->mfi_dev, "Warning: shutdown event "
		    "registration failed\n");
	}

	/*
	 * Create the control device for doing management
	 */
	unit = device_get_unit(sc->mfi_dev);
	sc->mfi_cdev = make_dev(&mfi_cdevsw, unit, UID_ROOT, GID_OPERATOR,
	    0640, "mfi%d", unit);
	if (sc->mfi_cdev != NULL)
		sc->mfi_cdev->si_drv1 = sc;

	return (0);
}

static int
mfi_alloc_commands(struct mfi_softc *sc)
{
	struct mfi_command *cm;
	int i, ncmds;

	/*
	 * XXX Should we allocate all the commands up front, or allocate on
	 * demand later like 'aac' does?
	 */
	ncmds = sc->mfi_max_fw_cmds;
	sc->mfi_commands = malloc(sizeof(struct mfi_command) * ncmds, M_MFIBUF,
	    M_WAITOK | M_ZERO);

	for (i = 0; i < ncmds; i++) {
		cm = &sc->mfi_commands[i];
		cm->cm_frame = (union mfi_frame *)((uintptr_t)sc->mfi_frames + 
		    sc->mfi_frame_size * i);
		cm->cm_frame_busaddr = sc->mfi_frames_busaddr +
		    sc->mfi_frame_size * i;
		cm->cm_frame->header.context = i;
		cm->cm_sense = &sc->mfi_sense[i];
		cm->cm_sense_busaddr= sc->mfi_sense_busaddr + MFI_SENSE_LEN * i;
		cm->cm_sc = sc;
		if (bus_dmamap_create(sc->mfi_buffer_dmat, 0,
		    &cm->cm_dmamap) == 0)
			mfi_release_command(cm);
		else
			break;
		sc->mfi_total_cmds++;
	}

	return (0);
}

static void
mfi_release_command(struct mfi_command *cm)
{
	uint32_t *hdr_data;

	/*
	 * Zero out the important fields of the frame, but make sure the
	 * context field is preserved
	 */
	hdr_data = (uint32_t *)cm->cm_frame;
	hdr_data[0] = 0;
	hdr_data[1] = 0;

	cm->cm_extra_frames = 0;
	cm->cm_flags = 0;
	cm->cm_complete = NULL;
	cm->cm_private = NULL;
	cm->cm_sg = 0;
	cm->cm_total_frame_size = 0;
	mfi_enqueue_free(cm);
}

static int
mfi_comms_init(struct mfi_softc *sc)
{
	struct mfi_command *cm;
	struct mfi_init_frame *init;
	struct mfi_init_qinfo *qinfo;
	int error;

	if ((cm = mfi_dequeue_free(sc)) == NULL)
		return (EBUSY);

	/*
	 * Abuse the SG list area of the frame to hold the init_qinfo
	 * object;
	 */
	init = &cm->cm_frame->init;
	qinfo = (struct mfi_init_qinfo *)((uintptr_t)init + MFI_FRAME_SIZE);

	bzero(qinfo, sizeof(struct mfi_init_qinfo));
	qinfo->rq_entries = sc->mfi_max_fw_cmds + 1;
	qinfo->rq_addr_lo = sc->mfi_comms_busaddr +
	    offsetof(struct mfi_hwcomms, hw_reply_q);
	qinfo->pi_addr_lo = sc->mfi_comms_busaddr +
	    offsetof(struct mfi_hwcomms, hw_pi);
	qinfo->ci_addr_lo = sc->mfi_comms_busaddr +
	    offsetof(struct mfi_hwcomms, hw_ci);

	init->header.cmd = MFI_CMD_INIT;
	init->header.data_len = sizeof(struct mfi_init_qinfo);
	init->qinfo_new_addr_lo = cm->cm_frame_busaddr + MFI_FRAME_SIZE;

	if ((error = mfi_polled_command(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "failed to send init command\n");
		return (error);
	}
	mfi_release_command(cm);

	return (0);
}

static int
mfi_get_controller_info(struct mfi_softc *sc)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd;
	struct mfi_ctrl_info *ci;
	uint32_t max_sectors_1, max_sectors_2;
	int error;

	if ((cm = mfi_dequeue_free(sc)) == NULL)
		return (EBUSY);

	ci = malloc(sizeof(struct mfi_ctrl_info), M_MFIBUF, M_NOWAIT | M_ZERO);
	if (ci == NULL) {
		mfi_release_command(cm);
		return (ENOMEM);
	}

	dcmd = &cm->cm_frame->dcmd;
	bzero(dcmd->mbox, MFI_MBOX_SIZE);
	dcmd->header.cmd = MFI_CMD_DCMD;
	dcmd->header.timeout = 0;
	dcmd->header.data_len = sizeof(struct mfi_ctrl_info);
	dcmd->opcode = MFI_DCMD_CTRL_GETINFO;
	cm->cm_sg = &dcmd->sgl;
	cm->cm_total_frame_size = MFI_DCMD_FRAME_SIZE;
	cm->cm_flags = MFI_CMD_DATAIN | MFI_CMD_POLLED;
	cm->cm_data = ci;
	cm->cm_len = sizeof(struct mfi_ctrl_info);

	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Controller info buffer map failed");
		free(ci, M_MFIBUF);
		mfi_release_command(cm);
		return (error);
	}

	/* It's ok if this fails, just use default info instead */
	if ((error = mfi_polled_command(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Failed to get controller info\n");
		sc->mfi_max_io = (sc->mfi_total_sgl - 1) * PAGE_SIZE /
		    MFI_SECTOR_LEN;
		free(ci, M_MFIBUF);
		mfi_release_command(cm);
		return (0);
	}

	bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->mfi_buffer_dmat, cm->cm_dmamap);

	max_sectors_1 = (1 << ci->stripe_sz_ops.min) * ci->max_strips_per_io;
	max_sectors_2 = ci->max_request_size;
	sc->mfi_max_io = min(max_sectors_1, max_sectors_2);

	free(ci, M_MFIBUF);
	mfi_release_command(cm);

	return (error);
}

static int
mfi_polled_command(struct mfi_softc *sc, struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	int tm = MFI_POLL_TIMEOUT_SECS * 1000000;

	hdr = &cm->cm_frame->header;
	hdr->cmd_status = 0xff;
	hdr->flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;

	mfi_send_frame(sc, cm);

	while (hdr->cmd_status == 0xff) {
		DELAY(1000);
		tm -= 1000;
		if (tm <= 0)
			break;
	}

	if (hdr->cmd_status == 0xff) {
		device_printf(sc->mfi_dev, "Frame %p timed out\n", hdr);
		return (ETIMEDOUT);
	}

	return (0);
}

void
mfi_free(struct mfi_softc *sc)
{
	struct mfi_command *cm;
	int i;

	if (sc->mfi_cdev != NULL)
		destroy_dev(sc->mfi_cdev);

	if (sc->mfi_total_cmds != 0) {
		for (i = 0; i < sc->mfi_total_cmds; i++) {
			cm = &sc->mfi_commands[i];
			bus_dmamap_destroy(sc->mfi_buffer_dmat, cm->cm_dmamap);
		}
		free(sc->mfi_commands, M_MFIBUF);
	}

	if (sc->mfi_intr)
		bus_teardown_intr(sc->mfi_dev, sc->mfi_irq, sc->mfi_intr);
	if (sc->mfi_irq != NULL)
		bus_release_resource(sc->mfi_dev, SYS_RES_IRQ, sc->mfi_irq_rid,
		    sc->mfi_irq);

	if (sc->mfi_sense_busaddr != 0)
		bus_dmamap_unload(sc->mfi_sense_dmat, sc->mfi_sense_dmamap);
	if (sc->mfi_sense != NULL)
		bus_dmamem_free(sc->mfi_sense_dmat, sc->mfi_sense,
		    sc->mfi_sense_dmamap);
	if (sc->mfi_sense_dmat != NULL)
		bus_dma_tag_destroy(sc->mfi_sense_dmat);

	if (sc->mfi_frames_busaddr != 0)
		bus_dmamap_unload(sc->mfi_frames_dmat, sc->mfi_frames_dmamap);
	if (sc->mfi_frames != NULL)
		bus_dmamem_free(sc->mfi_frames_dmat, sc->mfi_frames,
		    sc->mfi_frames_dmamap);
	if (sc->mfi_frames_dmat != NULL)
		bus_dma_tag_destroy(sc->mfi_frames_dmat);

	if (sc->mfi_comms_busaddr != 0)
		bus_dmamap_unload(sc->mfi_comms_dmat, sc->mfi_comms_dmamap);
	if (sc->mfi_comms != NULL)
		bus_dmamem_free(sc->mfi_comms_dmat, sc->mfi_comms,
		    sc->mfi_comms_dmamap);
	if (sc->mfi_comms_dmat != NULL)
		bus_dma_tag_destroy(sc->mfi_comms_dmat);

	if (sc->mfi_buffer_dmat != NULL)
		bus_dma_tag_destroy(sc->mfi_buffer_dmat);
	if (sc->mfi_parent_dmat != NULL)
		bus_dma_tag_destroy(sc->mfi_parent_dmat);

	if (mtx_initialized(&sc->mfi_io_lock))
		mtx_destroy(&sc->mfi_io_lock);

	return;
}

static void
mfi_startup(void *arg)
{
	struct mfi_softc *sc;

	sc = (struct mfi_softc *)arg;

	config_intrhook_disestablish(&sc->mfi_ich);

	mfi_enable_intr(sc);
	mfi_ldprobe_inq(sc);
}

static void
mfi_intr(void *arg)
{
	struct mfi_softc *sc;
	struct mfi_command *cm;
	uint32_t status, pi, ci, context;

	sc = (struct mfi_softc *)arg;

	status = MFI_READ4(sc, MFI_OSTS);
	if ((status & MFI_OSTS_INTR_VALID) == 0)
		return;
	MFI_WRITE4(sc, MFI_OSTS, status);

	pi = sc->mfi_comms->hw_pi;
	ci = sc->mfi_comms->hw_ci;

	mtx_lock(&sc->mfi_io_lock);
	while (ci != pi) {
		context = sc->mfi_comms->hw_reply_q[ci];
		sc->mfi_comms->hw_reply_q[ci] = 0xffffffff;
		if (context == 0xffffffff) {
			device_printf(sc->mfi_dev, "mfi_intr: invalid context "
			    "pi= %d ci= %d\n", pi, ci);
		} else {
			cm = &sc->mfi_commands[context];
			mfi_remove_busy(cm);
			mfi_complete(sc, cm);
		}
		ci++;
		if (ci == (sc->mfi_max_fw_cmds + 1)) {
			ci = 0;
		}
	}
	mtx_unlock(&sc->mfi_io_lock);

	sc->mfi_comms->hw_ci = ci;

	return;
}

int
mfi_shutdown(struct mfi_softc *sc)
{
	struct mfi_dcmd_frame *dcmd;
	struct mfi_command *cm;
	int error;

	if ((cm = mfi_dequeue_free(sc)) == NULL)
		return (EBUSY);

	/* AEN? */

	dcmd = &cm->cm_frame->dcmd;
	bzero(dcmd->mbox, MFI_MBOX_SIZE);
	dcmd->header.cmd = MFI_CMD_DCMD;
	dcmd->header.sg_count = 0;
	dcmd->header.flags = MFI_FRAME_DIR_NONE;
	dcmd->header.timeout = 0;
	dcmd->header.data_len = 0;
	dcmd->opcode = MFI_DCMD_CTRL_SHUTDOWN;

	if ((error = mfi_polled_command(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Failed to shutdown controller\n");
	}

	return (error);
}

static void
mfi_enable_intr(struct mfi_softc *sc)
{

	MFI_WRITE4(sc, MFI_OMSK, 0x01);
}

static void
mfi_ldprobe_inq(struct mfi_softc *sc)
{
	struct mfi_command *cm;
	struct mfi_pass_frame *pass;
	char *inq;
	int i;

	/* Probe all possible targets with a SCSI INQ command */
	mtx_lock(&sc->mfi_io_lock);
	sc->mfi_probe_count = 0;
	for (i = 0; i < MFI_MAX_CHANNEL_DEVS; i++) {
		inq = malloc(MFI_INQ_LENGTH, M_MFIBUF, M_NOWAIT|M_ZERO);
		if (inq == NULL)
			break;
		cm = mfi_dequeue_free(sc);
		if (cm == NULL) {
			free(inq, M_MFIBUF);
			msleep(mfi_startup, &sc->mfi_io_lock, 0, "mfistart",
			    5 * hz);
			i--;
			continue;
		}
		pass = &cm->cm_frame->pass;
		pass->header.cmd = MFI_CMD_LD_SCSI_IO;
		pass->header.target_id = i;
		pass->header.lun_id = 0;
		pass->header.cdb_len = 6;
		pass->header.timeout = 0;
		pass->header.data_len = MFI_INQ_LENGTH;
		bzero(pass->cdb, 16);
		pass->cdb[0] = INQUIRY;
		pass->cdb[4] = MFI_INQ_LENGTH;
		pass->header.sense_len = MFI_SENSE_LEN;
		pass->sense_addr_lo = cm->cm_sense_busaddr;
		pass->sense_addr_hi = 0;
		cm->cm_complete = mfi_ldprobe_inq_complete;
		cm->cm_private = inq;
		cm->cm_sg = &pass->sgl;
		cm->cm_total_frame_size = MFI_PASS_FRAME_SIZE;
		cm->cm_flags |= MFI_CMD_DATAIN;
		cm->cm_data = inq;
		cm->cm_len = MFI_INQ_LENGTH;
		sc->mfi_probe_count++;
		mfi_enqueue_ready(cm);
		mfi_startio(sc);
	}

	/* Sleep while the arrays are attaching */
	msleep(mfi_startup, &sc->mfi_io_lock, 0, "mfistart", 60 * hz);
	mtx_unlock(&sc->mfi_io_lock);

	return;
}

static void
mfi_ldprobe_inq_complete(struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	struct mfi_softc *sc;
	struct scsi_inquiry_data *inq;

	sc = cm->cm_sc;
	inq = cm->cm_private;
	hdr = &cm->cm_frame->header;

	if ((hdr->cmd_status != MFI_STAT_OK) || (hdr->scsi_status != 0x00) ||
	    (SID_TYPE(inq) != T_DIRECT)) {
		free(inq, M_MFIBUF);
		mfi_release_command(cm);
		if (--sc->mfi_probe_count <= 0)
			wakeup(mfi_startup);
		return;
	}

	free(inq, M_MFIBUF);
	mfi_release_command(cm);
	mfi_ldprobe_tur(sc, hdr->target_id);
}

static int
mfi_ldprobe_tur(struct mfi_softc *sc, int id)
{
	struct mfi_command *cm;
	struct mfi_pass_frame *pass;

	cm = mfi_dequeue_free(sc);
	if (cm == NULL)
		return (EBUSY);
	pass = &cm->cm_frame->pass;
	pass->header.cmd = MFI_CMD_LD_SCSI_IO;
	pass->header.target_id = id;
	pass->header.lun_id = 0;
	pass->header.cdb_len = 6;
	pass->header.timeout = 0;
	pass->header.data_len = 0;
	bzero(pass->cdb, 16);
	pass->cdb[0] = TEST_UNIT_READY;
	pass->header.sense_len = MFI_SENSE_LEN;
	pass->sense_addr_lo = cm->cm_sense_busaddr;
	pass->sense_addr_hi = 0;
	cm->cm_complete = mfi_ldprobe_tur_complete;
	cm->cm_total_frame_size = MFI_PASS_FRAME_SIZE;
	cm->cm_flags = 0;
	mfi_enqueue_ready(cm);
	mfi_startio(sc);

	return (0);
}

static void
mfi_ldprobe_tur_complete(struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	struct mfi_softc *sc;

	sc = cm->cm_sc;
	hdr = &cm->cm_frame->header;

	if ((hdr->cmd_status != MFI_STAT_OK) || (hdr->scsi_status != 0x00)) {
		device_printf(sc->mfi_dev, "Logical disk %d is not ready, "
		    "cmd_status= %d scsi_status= %d\n", hdr->target_id,
		    hdr->cmd_status, hdr->scsi_status);
		mfi_print_sense(sc, cm->cm_sense);
		mfi_release_command(cm);
		if (--sc->mfi_probe_count <= 0)
			wakeup(mfi_startup);
		return;
	}
	mfi_release_command(cm);
	mfi_ldprobe_capacity(sc, hdr->target_id);
}

static int
mfi_ldprobe_capacity(struct mfi_softc *sc, int id)
{
	struct mfi_command *cm;
	struct mfi_pass_frame *pass;
	struct scsi_read_capacity_data_long *cap;

	cap = malloc(sizeof(*cap), M_MFIBUF, M_NOWAIT|M_ZERO);
	if (cap == NULL)
		return (ENOMEM);
	cm = mfi_dequeue_free(sc);
	if (cm == NULL) {
		free(cap, M_MFIBUF);
		return (EBUSY);
	}
	pass = &cm->cm_frame->pass;
	pass->header.cmd = MFI_CMD_LD_SCSI_IO;
	pass->header.target_id = id;
	pass->header.lun_id = 0;
	pass->header.cdb_len = 6;
	pass->header.timeout = 0;
	pass->header.data_len = sizeof(*cap);
	bzero(pass->cdb, 16);
	pass->cdb[0] = 0x9e;	/* READ CAPACITY 16 */
	pass->cdb[13] = sizeof(*cap);
	pass->header.sense_len = MFI_SENSE_LEN;
	pass->sense_addr_lo = cm->cm_sense_busaddr;
	pass->sense_addr_hi = 0;
	cm->cm_complete = mfi_ldprobe_capacity_complete;
	cm->cm_private = cap;
	cm->cm_sg = &pass->sgl;
	cm->cm_total_frame_size = MFI_PASS_FRAME_SIZE;
	cm->cm_flags |= MFI_CMD_DATAIN;
	cm->cm_data = cap;
	cm->cm_len = sizeof(*cap);
	mfi_enqueue_ready(cm);
	mfi_startio(sc);

	return (0);
}

static void
mfi_ldprobe_capacity_complete(struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	struct mfi_softc *sc;
	struct scsi_read_capacity_data_long *cap;
	uint64_t sectors;
	uint32_t secsize;
	int target;

	sc = cm->cm_sc;
	cap = cm->cm_private;
	hdr = &cm->cm_frame->header;

	if ((hdr->cmd_status != MFI_STAT_OK) || (hdr->scsi_status != 0x00)) {
		device_printf(sc->mfi_dev, "Failed to read capacity for "
		    "logical disk\n");
		device_printf(sc->mfi_dev, "cmd_status= %d scsi_status= %d\n",
		    hdr->cmd_status, hdr->scsi_status);
		free(cap, M_MFIBUF);
		mfi_release_command(cm);
		if (--sc->mfi_probe_count <= 0)
			wakeup(mfi_startup);
		return;
	}
	target = hdr->target_id;
	sectors = scsi_8btou64(cap->addr);
	secsize = scsi_4btoul(cap->length);
	free(cap, M_MFIBUF);
	mfi_release_command(cm);
	mfi_add_ld(sc, target, sectors, secsize);
	if (--sc->mfi_probe_count <= 0)
		wakeup(mfi_startup);

	return;
}

static int
mfi_add_ld(struct mfi_softc *sc, int id, uint64_t sectors, uint32_t secsize)
{
	struct mfi_ld *ld;
	device_t child;

	if ((secsize == 0) || (sectors == 0)) {
		device_printf(sc->mfi_dev, "Invalid capacity parameters for "
		      "logical disk %d\n", id);
		return (EINVAL);
	}

	ld = malloc(sizeof(struct mfi_ld), M_MFIBUF, M_NOWAIT|M_ZERO);
	if (ld == NULL) {
		device_printf(sc->mfi_dev, "Cannot allocate ld\n");
		return (ENOMEM);
	}

	if ((child = device_add_child(sc->mfi_dev, "mfid", -1)) == NULL) {
		device_printf(sc->mfi_dev, "Failed to add logical disk\n");
		free(ld, M_MFIBUF);
		return (EINVAL);
	}

	ld->ld_id = id;
	ld->ld_disk = child;
	ld->ld_secsize = secsize;
	ld->ld_sectors = sectors;

	device_set_ivars(child, ld);
	device_set_desc(child, "MFI Logical Disk");
	mtx_unlock(&sc->mfi_io_lock);
	mtx_lock(&Giant);
	bus_generic_attach(sc->mfi_dev);
	mtx_unlock(&Giant);
	mtx_lock(&sc->mfi_io_lock);

	return (0);
}

static struct mfi_command *
mfi_bio_command(struct mfi_softc *sc)
{
	struct mfi_io_frame *io;
	struct mfi_command *cm;
	struct bio *bio;
	int flags, blkcount;;

	if ((cm = mfi_dequeue_free(sc)) == NULL)
		return (NULL);

	if ((bio = mfi_dequeue_bio(sc)) == NULL) {
		mfi_release_command(cm);
		return (NULL);
	}

	io = &cm->cm_frame->io;
	switch (bio->bio_cmd & 0x03) {
	case BIO_READ:
		io->header.cmd = MFI_CMD_LD_READ;
		flags = MFI_CMD_DATAIN;
		break;
	case BIO_WRITE:
		io->header.cmd = MFI_CMD_LD_WRITE;
		flags = MFI_CMD_DATAOUT;
		break;
	default:
		panic("Invalid bio command");
	}

	/* Cheat with the sector length to avoid a non-constant division */
	blkcount = (bio->bio_bcount + MFI_SECTOR_LEN - 1) / MFI_SECTOR_LEN;
	io->header.target_id = (uintptr_t)bio->bio_driver1;
	io->header.timeout = 0;
	io->header.flags = 0;
	io->header.sense_len = MFI_SENSE_LEN;
	io->header.data_len = blkcount;
	io->sense_addr_lo = cm->cm_sense_busaddr;
	io->sense_addr_hi = 0;
	io->lba_hi = (bio->bio_pblkno & 0xffffffff00000000) >> 32;
	io->lba_lo = bio->bio_pblkno & 0xffffffff;
	cm->cm_complete = mfi_bio_complete;
	cm->cm_private = bio;
	cm->cm_data = bio->bio_data;
	cm->cm_len = bio->bio_bcount;
	cm->cm_sg = &io->sgl;
	cm->cm_total_frame_size = MFI_IO_FRAME_SIZE;
	cm->cm_flags = flags;

	return (cm);
}

static void
mfi_bio_complete(struct mfi_command *cm)
{
	struct bio *bio;
	struct mfi_frame_header *hdr;
	struct mfi_softc *sc;

	bio = cm->cm_private;
	hdr = &cm->cm_frame->header;
	sc = cm->cm_sc;

	if ((hdr->cmd_status != 0) || (hdr->scsi_status != 0)) {
		bio->bio_flags |= BIO_ERROR;
		bio->bio_error = EIO;
		device_printf(sc->mfi_dev, "I/O error, status= %d "
		    "scsi_status= %d\n", hdr->cmd_status, hdr->scsi_status);
		mfi_print_sense(cm->cm_sc, cm->cm_sense);
	}

	mfi_release_command(cm);
	mfi_disk_complete(bio);
}

void
mfi_startio(struct mfi_softc *sc)
{
	struct mfi_command *cm;

	for (;;) {
		/* Don't bother if we're short on resources */
		if (sc->mfi_flags & MFI_FLAGS_QFRZN)
			break;

		/* Try a command that has already been prepared */
		cm = mfi_dequeue_ready(sc);

		/* Nope, so look for work on the bioq */
		if (cm == NULL)
			cm = mfi_bio_command(sc);

		/* No work available, so exit */
		if (cm == NULL)
			break;

		/* Send the command to the controller */
		if (mfi_mapcmd(sc, cm) != 0) {
			mfi_requeue_ready(cm);
			break;
		}
	}
}

static int
mfi_mapcmd(struct mfi_softc *sc, struct mfi_command *cm)
{
	int error, polled;

	if (cm->cm_data != NULL) {
		polled = (cm->cm_flags & MFI_CMD_POLLED) ? BUS_DMA_NOWAIT : 0;
		error = bus_dmamap_load(sc->mfi_buffer_dmat, cm->cm_dmamap,
		    cm->cm_data, cm->cm_len, mfi_data_cb, cm, polled);
		if (error == EINPROGRESS) {
			sc->mfi_flags |= MFI_FLAGS_QFRZN;
			return (0);
		}
	} else {
		mfi_enqueue_busy(cm);
		error = mfi_send_frame(sc, cm);
	}

	return (error);
}

static void
mfi_data_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct mfi_frame_header *hdr;
	struct mfi_command *cm;
	union mfi_sgl *sgl;
	struct mfi_softc *sc;
	int i, dir;

	if (error)
		return;

	cm = (struct mfi_command *)arg;
	sc = cm->cm_sc;
	hdr = &cm->cm_frame->header;
	sgl = cm->cm_sg;

	if ((sc->mfi_flags & MFI_FLAGS_SG64) == 0) {
		for (i = 0; i < nsegs; i++) {
			sgl->sg32[i].addr = segs[i].ds_addr;
			sgl->sg32[i].len = segs[i].ds_len;
		}
	} else {
		for (i = 0; i < nsegs; i++) {
			sgl->sg64[i].addr = segs[i].ds_addr;
			sgl->sg64[i].len = segs[i].ds_len;
		}
		hdr->flags |= MFI_FRAME_SGL64;
	}
	hdr->sg_count = nsegs;

	dir = 0;
	if (cm->cm_flags & MFI_CMD_DATAIN) {
		dir |= BUS_DMASYNC_PREREAD;
		hdr->flags |= MFI_FRAME_DIR_READ;
	}
	if (cm->cm_flags & MFI_CMD_DATAOUT) {
		dir |= BUS_DMASYNC_PREWRITE;
		hdr->flags |= MFI_FRAME_DIR_WRITE;
	}
	bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap, dir);
	cm->cm_flags |= MFI_CMD_MAPPED;

	/*
	 * Instead of calculating the total number of frames in the
	 * compound frame, it's already assumed that there will be at
	 * least 1 frame, so don't compensate for the modulo of the
	 * following division.
	 */
	cm->cm_total_frame_size += (sc->mfi_sgsize * nsegs);
	cm->cm_extra_frames = (cm->cm_total_frame_size - 1) / MFI_FRAME_SIZE;

	/* The caller will take care of delivering polled commands */
	if ((cm->cm_flags & MFI_CMD_POLLED) == 0) {
		mfi_enqueue_busy(cm);
		mfi_send_frame(sc, cm);
	}

	return;
}

static int
mfi_send_frame(struct mfi_softc *sc, struct mfi_command *cm)
{

	/*
	 * The bus address of the command is aligned on a 64 byte boundary,
	 * leaving the least 6 bits as zero.  For whatever reason, the
	 * hardware wants the address shifted right by three, leaving just
	 * 3 zero bits.  These three bits are then used to indicate how many
	 * 64 byte frames beyond the first one are used in the command.  The
	 * extra frames are typically filled with S/G elements.  The extra
	 * frames must also be contiguous.  Thus, a compound frame can be at
	 * most 512 bytes long, allowing for up to 59 32-bit S/G elements or
	 * 39 64-bit S/G elements for block I/O commands.  This means that
	 * I/O transfers of 256k and higher simply are not possible, which
	 * is quite odd for such a modern adapter.
	 */
	MFI_WRITE4(sc, MFI_IQP, (cm->cm_frame_busaddr >> 3) |
	    cm->cm_extra_frames);
	return (0);
}

static void
mfi_complete(struct mfi_softc *sc, struct mfi_command *cm)
{
	int dir;

	if ((cm->cm_flags & MFI_CMD_MAPPED) != 0) {
		dir = 0;
		if (cm->cm_flags & MFI_CMD_DATAIN)
			dir |= BUS_DMASYNC_POSTREAD;
		if (cm->cm_flags & MFI_CMD_DATAOUT)
			dir |= BUS_DMASYNC_POSTWRITE;

		bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap, dir);
		bus_dmamap_unload(sc->mfi_buffer_dmat, cm->cm_dmamap);
		cm->cm_flags &= ~MFI_CMD_MAPPED;
	}

	if (cm->cm_complete != NULL)
		cm->cm_complete(cm);

	sc->mfi_flags &= ~MFI_FLAGS_QFRZN;
	mfi_startio(sc);
}

int
mfi_dump_blocks(struct mfi_softc *sc, int id, uint64_t lba, void *virt, int len)
{
	struct mfi_command *cm;
	struct mfi_io_frame *io;
	int error;

	if ((cm = mfi_dequeue_free(sc)) == NULL)
		return (EBUSY);

	io = &cm->cm_frame->io;
	io->header.cmd = MFI_CMD_LD_WRITE;
	io->header.target_id = id;
	io->header.timeout = 0;
	io->header.flags = 0;
	io->header.sense_len = MFI_SENSE_LEN;
	io->header.data_len = (len + MFI_SECTOR_LEN - 1) / MFI_SECTOR_LEN;
	io->sense_addr_lo = cm->cm_sense_busaddr;
	io->sense_addr_hi = 0;
	io->lba_hi = (lba & 0xffffffff00000000) >> 32;
	io->lba_lo = lba & 0xffffffff;
	cm->cm_data = virt;
	cm->cm_len = len;
	cm->cm_sg = &io->sgl;
	cm->cm_total_frame_size = MFI_IO_FRAME_SIZE;
	cm->cm_flags = MFI_CMD_POLLED | MFI_CMD_DATAOUT;

	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		mfi_release_command(cm);
		return (error);
	}

	error = mfi_polled_command(sc, cm);
	bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->mfi_buffer_dmat, cm->cm_dmamap);
	mfi_release_command(cm);

	return (error);
}

static int
mfi_open(struct cdev *dev, int flags, int fmt, d_thread_t *td)
{
	struct mfi_softc *sc;

	sc = dev->si_drv1;
	sc->mfi_flags |= MFI_FLAGS_OPEN;

	return (0);
}

static int
mfi_close(struct cdev *dev, int flags, int fmt, d_thread_t *td)
{
	struct mfi_softc *sc;

	sc = dev->si_drv1;
	sc->mfi_flags &= ~MFI_FLAGS_OPEN;

	return (0);
}

static int
mfi_ioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag, d_thread_t *td)
{
	struct mfi_softc *sc;
	union mfi_statrequest *ms;
	int error;

	sc = dev->si_drv1;
	error = 0;

	switch (cmd) {
	case MFIIO_STATS:
		ms = (union mfi_statrequest *)arg;
		switch (ms->ms_item) {
		case MFIQ_FREE:
		case MFIQ_BIO:
		case MFIQ_READY:
		case MFIQ_BUSY:
			bcopy(&sc->mfi_qstat[ms->ms_item], &ms->ms_qstat,
			    sizeof(struct mfi_qstat));
			break;
		default:
			error = ENOENT;
			break;
		}
		break;
	default:
		error = ENOENT;
		break;
	}

	return (error);
}
