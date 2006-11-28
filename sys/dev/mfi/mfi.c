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
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/ioccom.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/mfi/mfi_compat.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/mfi/mfireg.h>
#include <dev/mfi/mfi_ioctl.h>
#include <dev/mfi/mfivar.h>

static int	mfi_alloc_commands(struct mfi_softc *);
static void	mfi_release_command(struct mfi_command *cm);
static int	mfi_comms_init(struct mfi_softc *);
static int	mfi_polled_command(struct mfi_softc *, struct mfi_command *);
static int	mfi_wait_command(struct mfi_softc *, struct mfi_command *);
static int	mfi_get_controller_info(struct mfi_softc *);
static int	mfi_get_log_state(struct mfi_softc *,
		    struct mfi_evt_log_state **);
#ifdef NOTYET
static int	mfi_get_entry(struct mfi_softc *, int);
#endif
static int	mfi_dcmd_command(struct mfi_softc *, struct mfi_command **,
		    uint32_t, void **, size_t);
static void	mfi_data_cb(void *, bus_dma_segment_t *, int, int);
static void	mfi_startup(void *arg);
static void	mfi_intr(void *arg);
static void	mfi_enable_intr(struct mfi_softc *sc);
static void	mfi_ldprobe(struct mfi_softc *sc);
static int	mfi_aen_register(struct mfi_softc *sc, int seq, int locale);
static void	mfi_aen_complete(struct mfi_command *);
static int	mfi_aen_setup(struct mfi_softc *, uint32_t);
static int	mfi_add_ld(struct mfi_softc *sc, int);
static void	mfi_add_ld_complete(struct mfi_command *);
static struct mfi_command * mfi_bio_command(struct mfi_softc *);
static void	mfi_bio_complete(struct mfi_command *);
static int	mfi_mapcmd(struct mfi_softc *, struct mfi_command *);
static int	mfi_send_frame(struct mfi_softc *, struct mfi_command *);
static void	mfi_complete(struct mfi_softc *, struct mfi_command *);
static int	mfi_abort(struct mfi_softc *, struct mfi_command *);
#ifdef notyet
static int	mfi_linux_ioctl_int(dev_t, u_long, caddr_t, int, d_thread_t *);
#endif

/* Management interface */
static d_open_t		mfi_open;
static d_close_t	mfi_close;
static d_ioctl_t	mfi_ioctl;
static d_poll_t		mfi_poll;

#define	MFI_CDEV_MAJOR	177

static struct cdevsw mfi_cdevsw = {
	mfi_open,
	mfi_close,
	noread,
	nowrite,
	mfi_ioctl,
	mfi_poll,
	nommap,
	nostrategy,
	"mfi",
	MFI_CDEV_MAJOR,
	nodump,
	nopsize,
	0
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

	TAILQ_INIT(&sc->mfi_ld_tqh);
	TAILQ_INIT(&sc->mfi_aen_pids);

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

	if ((error = mfi_aen_setup(sc, 0), 0) != 0)
		return (error);

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
mfi_dcmd_command(struct mfi_softc *sc, struct mfi_command **cmp, uint32_t opcode,
    void **bufp, size_t bufsize)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd;
	void *buf = NULL;
	
	cm = mfi_dequeue_free(sc);
	if (cm == NULL)
		return (EBUSY);

	if ((bufsize > 0) && (bufp != NULL)) {
		if (*bufp == NULL) {
			buf = malloc(bufsize, M_MFIBUF, M_NOWAIT|M_ZERO);
			if (buf == NULL) {
				mfi_release_command(cm);
				return (ENOMEM);
			}
			*bufp = buf;
		} else {
			buf = *bufp;
		}
	}

	dcmd =  &cm->cm_frame->dcmd;
	bzero(dcmd->mbox, MFI_MBOX_SIZE);
	dcmd->header.cmd = MFI_CMD_DCMD;
	dcmd->header.timeout = 0;
	dcmd->header.flags = 0;
	dcmd->header.data_len = bufsize;
	dcmd->opcode = opcode;
	cm->cm_sg = &dcmd->sgl;
	cm->cm_total_frame_size = MFI_DCMD_FRAME_SIZE;
	cm->cm_flags = 0;
	cm->cm_data = buf;
	cm->cm_private = buf;
	cm->cm_len = bufsize;

	*cmp = cm;
	if ((bufp != NULL) && (*bufp == NULL) && (buf != NULL))
		*bufp = buf;
	return (0);
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
	struct mfi_command *cm = NULL;
	struct mfi_ctrl_info *ci = NULL;
	uint32_t max_sectors_1, max_sectors_2;
	int error;

	error = mfi_dcmd_command(sc, &cm, MFI_DCMD_CTRL_GETINFO,
	    (void **)&ci, sizeof(*ci));
	if (error)
		goto out;
	cm->cm_flags = MFI_CMD_DATAIN | MFI_CMD_POLLED;

	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Controller info buffer map failed\n");
		free(ci, M_MFIBUF);
		mfi_release_command(cm);
		return (error);
	}

	/* It's ok if this fails, just use default info instead */
	if ((error = mfi_polled_command(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Failed to get controller info\n");
		sc->mfi_max_io = (sc->mfi_total_sgl - 1) * PAGE_SIZE /
		    MFI_SECTOR_LEN;
		error = 0;
		goto out;
	}

	bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->mfi_buffer_dmat, cm->cm_dmamap);

	max_sectors_1 = (1 << ci->stripe_sz_ops.min) * ci->max_strips_per_io;
	max_sectors_2 = ci->max_request_size;
	sc->mfi_max_io = min(max_sectors_1, max_sectors_2);

out:
	if (ci)
		free(ci, M_MFIBUF);
	if (cm)
		mfi_release_command(cm);
	return (error);
}

static int
mfi_get_log_state(struct mfi_softc *sc, struct mfi_evt_log_state **log_state)
{
	struct mfi_command *cm = NULL;
	int error;

	error = mfi_dcmd_command(sc, &cm, MFI_DCMD_CTRL_EVENT_GETINFO,
	    (void **)log_state, sizeof(**log_state));
	if (error)
		goto out;
	cm->cm_flags = MFI_CMD_DATAIN | MFI_CMD_POLLED;

	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Log state buffer map failed\n");
		goto out;
	}

	if ((error = mfi_polled_command(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Failed to get log state\n");
		goto out;
	}

	bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->mfi_buffer_dmat, cm->cm_dmamap);

out:
	if (cm)
		mfi_release_command(cm);

	return (error);
}

static int
mfi_aen_setup(struct mfi_softc *sc, uint32_t seq_start)
{
	struct mfi_evt_log_state *log_state = NULL;
	union mfi_evt class_locale;
	int error = 0;
	uint32_t seq;

	class_locale.members.reserved = 0;
	class_locale.members.locale = MFI_EVT_LOCALE_ALL;
	class_locale.members.class  = MFI_EVT_CLASS_DEBUG;

	if (seq_start == 0) {
		error = mfi_get_log_state(sc, &log_state);
		if (error) {
			if (log_state)
				free(log_state, M_MFIBUF);
			return (error);
		}
		/*
		 * Don't run them yet since we can't parse them.
		 * We can indirectly get the contents from
		 * the AEN mechanism via setting it lower then
		 * current.  The firmware will iterate through them.
		 */
#ifdef NOTYET
		for (seq = log_state->shutdown_seq_num;
		     seq <= log_state->newest_seq_num; seq++) {
			mfi_get_entry(sc, seq);
		}
#endif

		seq = log_state->shutdown_seq_num + 1;
	} else
		seq = seq_start;
	mfi_aen_register(sc, seq, class_locale.word);
	free(log_state, M_MFIBUF);

	return 0;
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

static int
mfi_wait_command(struct mfi_softc *sc, struct mfi_command *cm)
{
	int error, s;

	cm->cm_complete = NULL;

	s = splbio();
	mfi_enqueue_ready(cm);
	mfi_startio(sc);
	error = tsleep(cm, PRIBIO, "mfiwait", 0);
	splx(s);
	return (error);
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

	return;
}

static void
mfi_startup(void *arg)
{
	struct mfi_softc *sc;

	sc = (struct mfi_softc *)arg;

	config_intrhook_disestablish(&sc->mfi_ich);

	mfi_enable_intr(sc);
	mfi_ldprobe(sc);
}

static void
mfi_intr(void *arg)
{
	struct mfi_softc *sc;
	struct mfi_command *cm;
	uint32_t status, pi, ci, context;
	int s;

	sc = (struct mfi_softc *)arg;

	status = MFI_READ4(sc, MFI_OSTS);
	if ((status & MFI_OSTS_INTR_VALID) == 0)
		return;
	MFI_WRITE4(sc, MFI_OSTS, status);

	pi = sc->mfi_comms->hw_pi;
	ci = sc->mfi_comms->hw_ci;
	s = splbio();
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
	splx(s);

	sc->mfi_comms->hw_ci = ci;

	return;
}

int
mfi_shutdown(struct mfi_softc *sc)
{
	struct mfi_dcmd_frame *dcmd;
	struct mfi_command *cm;
	int error;

	error = mfi_dcmd_command(sc, &cm, MFI_DCMD_CTRL_SHUTDOWN, NULL, 0);
	if (error)
		return (error);

	if (sc->mfi_aen_cm != NULL)
		mfi_abort(sc, sc->mfi_aen_cm);

	dcmd = &cm->cm_frame->dcmd;
	dcmd->header.flags = MFI_FRAME_DIR_NONE;

	if ((error = mfi_polled_command(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Failed to shutdown controller\n");
	}

	mfi_release_command(cm);
	return (error);
}

static void
mfi_enable_intr(struct mfi_softc *sc)
{

	MFI_WRITE4(sc, MFI_OMSK, 0x01);
}

static void
mfi_ldprobe(struct mfi_softc *sc)
{
	struct mfi_frame_header *hdr;
	struct mfi_command *cm = NULL;
	struct mfi_ld_list *list = NULL;
	int error, i;

	error = mfi_dcmd_command(sc, &cm, MFI_DCMD_LD_GET_LIST,
	    (void **)&list, sizeof(*list));
	if (error)
		goto out;

	cm->cm_flags = MFI_CMD_DATAIN;
	if (mfi_wait_command(sc, cm) != 0) {
		device_printf(sc->mfi_dev, "Failed to get device listing\n");
		goto out;
	}

	hdr = &cm->cm_frame->header;
	if (hdr->cmd_status != MFI_STAT_OK) {
		device_printf(sc->mfi_dev, "MFI_DCMD_LD_GET_LIST failed %x\n",
		    hdr->cmd_status);
		goto out;
	}

	for (i = 0; i < list->ld_count; i++)
		mfi_add_ld(sc, list->ld_list[i].ld.target_id);
out:
	if (list)
		free(list, M_MFIBUF);
	if (cm)
		mfi_release_command(cm);
	return;
}

#ifdef NOTYET
static void
mfi_decode_log(struct mfi_softc *sc, struct mfi_log_detail *detail)
{
        switch (detail->arg_type) {
	default:
		device_printf(sc->mfi_dev, "%d - Log entry type %d\n",
		    detail->seq,
		    detail->arg_type
		);
		break;
	}
}
#endif

static void
mfi_decode_evt(struct mfi_softc *sc, struct mfi_evt_detail *detail)
{
	switch (detail->arg_type) {
	case MR_EVT_ARGS_NONE:
		device_printf(sc->mfi_dev, "%d - %s\n",
		    detail->seq,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_CDB_SENSE:
		device_printf(sc->mfi_dev, "%d - PD %02d(e%d/s%d) CDB %*D"
		    "Sense %*D\n: %s\n",
		    detail->seq,
		    detail->args.cdb_sense.pd.device_id,
		    detail->args.cdb_sense.pd.enclosure_index,
		    detail->args.cdb_sense.pd.slot_number,
		    detail->args.cdb_sense.cdb_len,
		    detail->args.cdb_sense.cdb,
		    ":",
		    detail->args.cdb_sense.sense_len,
		    detail->args.cdb_sense.sense,
		    ":",
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_LD:
		device_printf(sc->mfi_dev, "%d - VD %02d/%d "
		    "event: %s\n",
		    detail->seq,
		    detail->args.ld.ld_index,
		    detail->args.ld.target_id,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_LD_COUNT:
		device_printf(sc->mfi_dev, "%d - VD %02d/%d "
		    "count %lld: %s\n",
		    detail->seq,
		    detail->args.ld_count.ld.ld_index,
		    detail->args.ld_count.ld.target_id,
		    (long long)detail->args.ld_count.count,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_LD_LBA:
		device_printf(sc->mfi_dev, "%d - VD %02d/%d "
		    "lba %lld: %s\n",
		    detail->seq,
		    detail->args.ld_lba.ld.ld_index,
		    detail->args.ld_lba.ld.target_id,
		    (long long)detail->args.ld_lba.lba,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_LD_OWNER:
		device_printf(sc->mfi_dev, "%d - VD %02d/%d "
		    "owner changed: prior %d, new %d: %s\n",
		    detail->seq,
		    detail->args.ld_owner.ld.ld_index,
		    detail->args.ld_owner.ld.target_id,
		    detail->args.ld_owner.pre_owner,
		    detail->args.ld_owner.new_owner,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_LD_LBA_PD_LBA:
		device_printf(sc->mfi_dev, "%d - VD %02d/%d "
		    "lba %lld, physical drive PD %02d(e%d/s%d) lba %lld: %s\n",
		    detail->seq,
		    detail->args.ld_lba_pd_lba.ld.ld_index,
		    detail->args.ld_lba_pd_lba.ld.target_id,
		    (long long)detail->args.ld_lba_pd_lba.ld_lba,
		    detail->args.ld_lba_pd_lba.pd.device_id,
		    detail->args.ld_lba_pd_lba.pd.enclosure_index,
		    detail->args.ld_lba_pd_lba.pd.slot_number,
		    (long long)detail->args.ld_lba_pd_lba.pd_lba,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_LD_PROG:
		device_printf(sc->mfi_dev, "%d - VD %02d/%d "
		    "progress %d%% in %ds: %s\n",
		    detail->seq,
		    detail->args.ld_prog.ld.ld_index,
		    detail->args.ld_prog.ld.target_id,
		    detail->args.ld_prog.prog.progress/655,
		    detail->args.ld_prog.prog.elapsed_seconds,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_LD_STATE:
		device_printf(sc->mfi_dev, "%d - VD %02d/%d "
		    "state prior %d new %d: %s\n",
		    detail->seq,
		    detail->args.ld_state.ld.ld_index,
		    detail->args.ld_state.ld.target_id,
		    detail->args.ld_state.prev_state,
		    detail->args.ld_state.new_state,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_LD_STRIP:
		device_printf(sc->mfi_dev, "%d - VD %02d/%d "
		    "strip %lld: %s\n",
		    detail->seq,
		    detail->args.ld_strip.ld.ld_index,
		    detail->args.ld_strip.ld.target_id,
		    (long long)detail->args.ld_strip.strip,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_PD:
		device_printf(sc->mfi_dev, "%d - PD %02d(e%d/s%d) "
		    "event: %s\n",
		    detail->seq,
		    detail->args.pd.device_id,
		    detail->args.pd.enclosure_index,
		    detail->args.pd.slot_number,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_PD_ERR:
		device_printf(sc->mfi_dev, "%d - PD %02d(e%d/s%d) "
		    "err %d: %s\n",
		    detail->seq,
		    detail->args.pd_err.pd.device_id,
		    detail->args.pd_err.pd.enclosure_index,
		    detail->args.pd_err.pd.slot_number,
		    detail->args.pd_err.err,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_PD_LBA:
		device_printf(sc->mfi_dev, "%d - PD %02d(e%d/s%d) "
		    "lba %lld: %s\n",
		    detail->seq,
		    detail->args.pd_lba.pd.device_id,
		    detail->args.pd_lba.pd.enclosure_index,
		    detail->args.pd_lba.pd.slot_number,
		    (long long)detail->args.pd_lba.lba,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_PD_LBA_LD:
		device_printf(sc->mfi_dev, "%d - PD %02d(e%d/s%d) "
		    "lba %lld VD %02d/%d: %s\n",
		    detail->seq,
		    detail->args.pd_lba_ld.pd.device_id,
		    detail->args.pd_lba_ld.pd.enclosure_index,
		    detail->args.pd_lba_ld.pd.slot_number,
		    (long long)detail->args.pd_lba.lba,
		    detail->args.pd_lba_ld.ld.ld_index,
		    detail->args.pd_lba_ld.ld.target_id,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_PD_PROG:
		device_printf(sc->mfi_dev, "%d - PD %02d(e%d/s%d) "
		    "progress %d%% seconds %ds: %s\n",
		    detail->seq,
		    detail->args.pd_prog.pd.device_id,
		    detail->args.pd_prog.pd.enclosure_index,
		    detail->args.pd_prog.pd.slot_number,
		    detail->args.pd_prog.prog.progress/655,
		    detail->args.pd_prog.prog.elapsed_seconds,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_PD_STATE:
		device_printf(sc->mfi_dev, "%d - PD %02d(e%d/s%d) "
		    "state prior %d new %d: %s\n",
		    detail->seq,
		    detail->args.pd_prog.pd.device_id,
		    detail->args.pd_prog.pd.enclosure_index,
		    detail->args.pd_prog.pd.slot_number,
		    detail->args.pd_state.prev_state,
		    detail->args.pd_state.new_state,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_PCI:
		device_printf(sc->mfi_dev, "%d - PCI 0x04%x 0x04%x "
		    "0x04%x 0x04%x: %s\n",
		    detail->seq,
		    detail->args.pci.venderId,
		    detail->args.pci.deviceId,
		    detail->args.pci.subVenderId,
		    detail->args.pci.subDeviceId,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_RATE:
		device_printf(sc->mfi_dev, "%d - Rebuild rate %d: %s\n",
		    detail->seq,
		    detail->args.rate,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_TIME:
		device_printf(sc->mfi_dev, "%d - Adapter ticks %d "
		    "elapsed %ds: %s\n",
		    detail->seq,
		    detail->args.time.rtc,
		    detail->args.time.elapsedSeconds,
		    detail->description
		    );
		break;
	case MR_EVT_ARGS_ECC:
		device_printf(sc->mfi_dev, "%d - Adapter ECC %x,%x: %s: %s\n",
		    detail->seq,
		    detail->args.ecc.ecar,
		    detail->args.ecc.elog,
		    detail->args.ecc.str,
		    detail->description
		    );
		break;
	default:
		device_printf(sc->mfi_dev, "%d - Type %d: %s\n",
		    detail->seq,
		    detail->arg_type, detail->description
		    );
	}
}

static int
mfi_aen_register(struct mfi_softc *sc, int seq, int locale)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd;
	union mfi_evt current_aen, prior_aen;
	struct mfi_evt_detail *ed = NULL;
	int error;

	current_aen.word = locale;
	if (sc->mfi_aen_cm != NULL) {
		prior_aen.word =
		    ((uint32_t *)&sc->mfi_aen_cm->cm_frame->dcmd.mbox)[1];
		if (prior_aen.members.class <= current_aen.members.class &&
		    !((prior_aen.members.locale & current_aen.members.locale)
		    ^current_aen.members.locale)) {
			return (0);
		} else {
			prior_aen.members.locale |= current_aen.members.locale;
			if (prior_aen.members.class
			    < current_aen.members.class)
				current_aen.members.class =
				    prior_aen.members.class;
			mfi_abort(sc, sc->mfi_aen_cm);
		}
	}

	error = mfi_dcmd_command(sc, &cm, MFI_DCMD_CTRL_EVENT_WAIT,
	    (void **)&ed, sizeof(*ed));
	if (error)
		return (error);

	dcmd = &cm->cm_frame->dcmd;
	((uint32_t *)&dcmd->mbox)[0] = seq;
	((uint32_t *)&dcmd->mbox)[1] = locale;
	cm->cm_flags = MFI_CMD_DATAIN;
	cm->cm_complete = mfi_aen_complete;

	sc->mfi_aen_cm = cm;

	mfi_enqueue_ready(cm);
	mfi_startio(sc);

	return (0);
}

static void
mfi_aen_complete(struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	struct mfi_softc *sc;
	struct mfi_evt_detail *detail;
	struct mfi_aen *mfi_aen_entry;
	int seq = 0, aborted = 0;
	int s;

	sc = cm->cm_sc;
	hdr = &cm->cm_frame->header;

	if (sc->mfi_aen_cm == NULL)
		return;

	if (sc->mfi_aen_cm->cm_aen_abort || hdr->cmd_status == 0xff) {
		sc->mfi_aen_cm->cm_aen_abort = 0;
		aborted = 1;
	} else {
		sc->mfi_aen_triggered = 1;
		if (sc->mfi_poll_waiting)
			selwakeup(&sc->mfi_select);
		detail = cm->cm_data;
		mfi_decode_evt(sc, detail);
		seq = detail->seq + 1;
		s = splbio();
		TAILQ_FOREACH(mfi_aen_entry, &sc->mfi_aen_pids, aen_link) {
			TAILQ_REMOVE(&sc->mfi_aen_pids, mfi_aen_entry,
			    aen_link);
			psignal(mfi_aen_entry->p, SIGIO);
			free(mfi_aen_entry, M_MFIBUF);
		}
		splx(s);
	}

	free(cm->cm_data, M_MFIBUF);
	sc->mfi_aen_cm = NULL;
	wakeup(&sc->mfi_aen_cm);
	mfi_release_command(cm);

	/* set it up again so the driver can catch more events */
	if (!aborted) {
		mfi_aen_setup(sc, seq);
	}
}

#ifdef NOTYET
static int
mfi_get_entry(struct mfi_softc *sc, int seq)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd;
	struct mfi_log_detail *ed;
	int error;

	if ((cm = mfi_dequeue_free(sc)) == NULL) {
		return (EBUSY);
	}

	ed = malloc(sizeof(struct mfi_log_detail), M_MFIBUF, M_NOWAIT | M_ZERO);
	if (ed == NULL) {
		mfi_release_command(cm);
		return (ENOMEM);
	}

	dcmd = &cm->cm_frame->dcmd;
	bzero(dcmd->mbox, MFI_MBOX_SIZE);
	dcmd->header.cmd = MFI_CMD_DCMD;
	dcmd->header.timeout = 0;
	dcmd->header.data_len = sizeof(struct mfi_log_detail);
	dcmd->opcode = MFI_DCMD_CTRL_EVENT_GET;
	((uint32_t *)&dcmd->mbox)[0] = seq;
	((uint32_t *)&dcmd->mbox)[1] = MFI_EVT_LOCALE_ALL;
	cm->cm_sg = &dcmd->sgl;
	cm->cm_total_frame_size = MFI_DCMD_FRAME_SIZE;
	cm->cm_flags = MFI_CMD_DATAIN | MFI_CMD_POLLED;
	cm->cm_data = ed;
	cm->cm_len = sizeof(struct mfi_evt_detail);

	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Controller info buffer map failed");
		free(ed, M_MFIBUF);
		mfi_release_command(cm);
		return (error);
	}

	if ((error = mfi_polled_command(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Failed to get controller entry\n");
		sc->mfi_max_io = (sc->mfi_total_sgl - 1) * PAGE_SIZE /
		    MFI_SECTOR_LEN;
		free(ed, M_MFIBUF);
		mfi_release_command(cm);
		return (0);
	}

	bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->mfi_buffer_dmat, cm->cm_dmamap);

	mfi_decode_log(sc, ed);

	free(cm->cm_data, M_MFIBUF);
	mfi_release_command(cm);
	return (0);
}
#endif

static int
mfi_add_ld(struct mfi_softc *sc, int id)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd = NULL;
	struct mfi_ld_info *ld_info = NULL;
	int error;

	error = mfi_dcmd_command(sc, &cm, MFI_DCMD_LD_GET_INFO,
	    (void **)&ld_info, sizeof(*ld_info));
	if (error) {
		device_printf(sc->mfi_dev,
		    "Failed to allocate for MFI_DCMD_LD_GET_INFO %d\n", error);
		if (ld_info)
			free(ld_info, M_MFIBUF);
		return (error);
	}
	cm->cm_flags = MFI_CMD_DATAIN;
	dcmd = &cm->cm_frame->dcmd;
	dcmd->mbox[0] = id;
	if (mfi_wait_command(sc, cm) != 0) {
		device_printf(sc->mfi_dev,
		    "Failed to get logical drive: %d\n", id);
		free(ld_info, M_MFIBUF);
		return (0);
	}

	mfi_add_ld_complete(cm);
	return (0);
}

static void
mfi_add_ld_complete(struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	struct mfi_ld_info *ld_info;
	struct mfi_softc *sc;
	struct mfi_ld *ld;
	device_t child;

	sc = cm->cm_sc;
	hdr = &cm->cm_frame->header;
	ld_info = cm->cm_private;

	if (hdr->cmd_status != MFI_STAT_OK) {
		free(ld_info, M_MFIBUF);
		mfi_release_command(cm);
		return;
	}
	mfi_release_command(cm);

	ld = malloc(sizeof(struct mfi_ld), M_MFIBUF, M_NOWAIT|M_ZERO);
	if (ld == NULL) {
		device_printf(sc->mfi_dev, "Cannot allocate ld\n");
		free(ld_info, M_MFIBUF);
		return;
	}

	if ((child = device_add_child(sc->mfi_dev, "mfid", -1)) == NULL) {
		device_printf(sc->mfi_dev, "Failed to add logical disk\n");
		free(ld, M_MFIBUF);
		free(ld_info, M_MFIBUF);
		return;
	}

	ld->ld_id = ld_info->ld_config.properties.ld.target_id;
	ld->ld_disk = child;
	ld->ld_info = ld_info;

	device_set_ivars(child, ld);
	device_set_desc(child, "MFI Logical Disk");
	bus_generic_attach(sc->mfi_dev);
}

static struct mfi_command *
mfi_bio_command(struct mfi_softc *sc)
{
	struct mfi_io_frame *io;
	struct mfi_command *cm;
	struct bio *bio;
	int flags, blkcount;

	if ((cm = mfi_dequeue_free(sc)) == NULL)
		return (NULL);

	if ((bio = mfi_dequeue_bio(sc)) == NULL) {
		mfi_release_command(cm);
		return (NULL);
	}

	io = &cm->cm_frame->io;
	if (BIO_IS_READ(bio)) {
		io->header.cmd = MFI_CMD_LD_READ;
		flags = MFI_CMD_DATAIN;
	} else {
		io->header.cmd = MFI_CMD_LD_WRITE;
		flags = MFI_CMD_DATAOUT;
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
	int s;

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
	s = splbio();
	MFI_WRITE4(sc, MFI_IQP, (cm->cm_frame_busaddr >> 3) |
	    cm->cm_extra_frames);
	splx(s);
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
	else
		wakeup(cm);

	sc->mfi_flags &= ~MFI_FLAGS_QFRZN;
	mfi_startio(sc);
}

static int
mfi_abort(struct mfi_softc *sc, struct mfi_command *cm_abort)
{
	struct mfi_command *cm;
	struct mfi_abort_frame *abort;

	if ((cm = mfi_dequeue_free(sc)) == NULL) {
		return (EBUSY);
	}

	abort = &cm->cm_frame->abort;
	abort->header.cmd = MFI_CMD_ABORT;
	abort->header.flags = 0;
	abort->abort_context = cm_abort->cm_frame->header.context;
	abort->abort_mfi_addr_lo = cm_abort->cm_frame_busaddr;
	abort->abort_mfi_addr_hi = 0;
	cm->cm_data = NULL;

	sc->mfi_aen_cm->cm_aen_abort = 1;
	mfi_mapcmd(sc, cm);
	mfi_polled_command(sc, cm);
	mfi_release_command(cm);

	while (sc->mfi_aen_cm != NULL) {
		tsleep(&sc->mfi_aen_cm, 0, "mfiabort", 5 * hz);
	}

	return (0);
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
mfi_open(dev_t dev, int flags, int fmt, d_thread_t *td)
{
	struct mfi_softc *sc;

	sc = dev->si_drv1;
	sc->mfi_flags |= MFI_FLAGS_OPEN;

	return (0);
}

static int
mfi_close(dev_t dev, int flags, int fmt, d_thread_t *td)
{
	struct mfi_softc *sc;
	struct mfi_aen *mfi_aen_entry;
	int s;

	sc = dev->si_drv1;
	sc->mfi_flags &= ~MFI_FLAGS_OPEN;

	s = splbio();
	TAILQ_FOREACH(mfi_aen_entry, &sc->mfi_aen_pids, aen_link) {
		if (mfi_aen_entry->p == curproc) {
			TAILQ_REMOVE(&sc->mfi_aen_pids, mfi_aen_entry,
			    aen_link);
			free(mfi_aen_entry, M_MFIBUF);
		}
	}
	splx(s);
	return (0);
}

static int
mfi_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, d_thread_t *td)
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
			error = ENOIOCTL;
			break;
		}
		break;

#ifdef notyet
	case 0xc1144d01: /* Firmware Linux ioctl shim */
		{
			devclass_t devclass;
			struct mfi_linux_ioc_packet l_ioc;
			int adapter;

			devclass = devclass_find("mfi");
			if (devclass == NULL)
				return (ENOENT);

			error = copyin(arg, &l_ioc, sizeof(l_ioc));
			if (error)
				return (error);
			adapter = l_ioc.lioc_adapter_no;
			sc = devclass_get_softc(devclass, adapter);
			if (sc == NULL)
				return (ENOENT);
			return (mfi_linux_ioctl_int(sc->mfi_cdev,
			    cmd, arg, flag, td));
			break;
		}
	case 0x400c4d03: /* AEN Linux ioctl shim */
		{
			devclass_t devclass;
			struct mfi_linux_ioc_aen l_aen;
			int adapter;

			devclass = devclass_find("mfi");
			if (devclass == NULL)
				return (ENOENT);

			error = copyin(arg, &l_aen, sizeof(l_aen));
			if (error)
				return (error);
			adapter = l_aen.laen_adapter_no;
			sc = devclass_get_softc(devclass, adapter);
			if (sc == NULL)
				return (ENOENT);
			return (mfi_linux_ioctl_int(sc->mfi_cdev,
			    cmd, arg, flag, td));
			break;
		}
#endif
	default:
		error = ENOENT;
		break;
	}

	return (error);
}

#ifdef notyet
static int
mfi_linux_ioctl_int(dev_t dev, u_long cmd, caddr_t arg, int flag, d_thread_t *td)
{
	struct mfi_softc *sc;
	struct mfi_linux_ioc_packet l_ioc;
	struct mfi_linux_ioc_aen l_aen;
	struct mfi_command *cm = NULL;
	struct mfi_aen *mfi_aen_entry;
	uint32_t *sense_ptr;
	uint32_t context;
	uint8_t *data = NULL, *temp;
	int i;
	int s;
	int error;

	sc = dev->si_drv1;
	error = 0;
	switch (cmd) {
	case 0xc1144d01: /* Firmware Linux ioctl shim */
		error = copyin(arg, &l_ioc, sizeof(l_ioc));
		if (error != 0)
			return (error);

		if (l_ioc.lioc_sge_count > MAX_LINUX_IOCTL_SGE) {
			return (EINVAL);
		}

		if ((cm = mfi_dequeue_free(sc)) == NULL) {
			return (EBUSY);
		}

		/*
		 * save off original context since copying from user
		 * will clobber some data
		 */
		context = cm->cm_frame->header.context;

		bcopy(l_ioc.lioc_frame.raw, cm->cm_frame,
		      l_ioc.lioc_sgl_off); /* Linux can do 2 frames ? */
		cm->cm_total_frame_size = l_ioc.lioc_sgl_off;
		cm->cm_sg =
		    (union mfi_sgl *)&cm->cm_frame->bytes[l_ioc.lioc_sgl_off];
		cm->cm_flags = MFI_CMD_DATAIN | MFI_CMD_DATAOUT
			| MFI_CMD_POLLED;
		cm->cm_len = cm->cm_frame->header.data_len;
		cm->cm_data = data = malloc(cm->cm_len, M_MFIBUF,
					    M_WAITOK | M_ZERO);

		/* restore header context */
		cm->cm_frame->header.context = context;

		temp = data;
		for (i = 0; i < l_ioc.lioc_sge_count; i++) {
			error = copyin(l_ioc.lioc_sgl[i].iov_base,
			       temp,
			       l_ioc.lioc_sgl[i].iov_len);
			if (error != 0) {
				device_printf(sc->mfi_dev,
				    "Copy in failed");
				goto out;
			}
			temp = &temp[l_ioc.lioc_sgl[i].iov_len];
		}

		if (l_ioc.lioc_sense_len) {
			sense_ptr =
			    (void *)&cm->cm_frame->bytes[l_ioc.lioc_sense_off];
			*sense_ptr = cm->cm_sense_busaddr;
		}

		if ((error = mfi_mapcmd(sc, cm)) != 0) {
			device_printf(sc->mfi_dev,
			    "Controller info buffer map failed");
			goto out;
		}

		if ((error = mfi_polled_command(sc, cm)) != 0) {
			device_printf(sc->mfi_dev,
			    "Controller polled failed");
			goto out;
		}

		bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap,
				BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->mfi_buffer_dmat, cm->cm_dmamap);

		temp = data;
		for (i = 0; i < l_ioc.lioc_sge_count; i++) {
			error = copyout(temp,
				l_ioc.lioc_sgl[i].iov_base,
				l_ioc.lioc_sgl[i].iov_len);
			if (error != 0) {
				device_printf(sc->mfi_dev,
				    "Copy out failed");
				goto out;
			}
			temp = &temp[l_ioc.lioc_sgl[i].iov_len];
		}

		if (l_ioc.lioc_sense_len) {
			/* copy out sense */
			sense_ptr = (void *)
			    &l_ioc.lioc_frame.raw[l_ioc.lioc_sense_off];
			temp = 0;
			temp += cm->cm_sense_busaddr;
			error = copyout(temp, sense_ptr,
			    l_ioc.lioc_sense_len);
			if (error != 0) {
				device_printf(sc->mfi_dev,
				    "Copy out failed");
				goto out;
			}
		}

		error = copyout(&cm->cm_frame->header.cmd_status,
			&((struct mfi_linux_ioc_packet*)arg)
			->lioc_frame.hdr.cmd_status,
			1);
		if (error != 0) {
			device_printf(sc->mfi_dev,
				      "Copy out failed");
			goto out;
		}

out:
		if (data)
			free(data, M_MFIBUF);
		if (cm) {
			mfi_release_command(cm);
		}

		return (error);
	case 0x400c4d03: /* AEN Linux ioctl shim */
		error = copyin(arg, &l_aen, sizeof(l_aen));
		if (error != 0)
			return (error);
		printf("AEN IMPLEMENTED for pid %d\n", curproc->p_pid);
		mfi_aen_entry = malloc(sizeof(struct mfi_aen), M_MFIBUF,
		    M_WAITOK);
		if (mfi_aen_entry != NULL) {
			mfi_aen_entry->p = curproc;
			s = splbio();
			TAILQ_INSERT_TAIL(&sc->mfi_aen_pids, mfi_aen_entry,
			    aen_link);
			splx(s);
		}
		error = mfi_aen_register(sc, l_aen.laen_seq_num,
		    l_aen.laen_class_locale);

		if (error != 0) {
			s = splbio();
			TAILQ_REMOVE(&sc->mfi_aen_pids, mfi_aen_entry,
			    aen_link);
			splx(s);
			free(mfi_aen_entry, M_MFIBUF);
		}

		return (error);
	default:
		device_printf(sc->mfi_dev, "IOCTL 0x%lx not handled\n", cmd);
		error = ENOENT;
		break;
	}

	return (error);
}
#endif

static int
mfi_poll(dev_t dev, int poll_events, d_thread_t *td)
{
	struct mfi_softc *sc;
	int revents = 0;

	sc = dev->si_drv1;

	if (poll_events & (POLLIN | POLLRDNORM)) {
		if (sc->mfi_aen_triggered != 0)
			revents |= poll_events & (POLLIN | POLLRDNORM);
		if (sc->mfi_aen_triggered == 0 && sc->mfi_aen_cm == NULL) {
			revents |= POLLERR;
		}
	}

	if (revents == 0) {
		if (poll_events & (POLLIN | POLLRDNORM)) {
			sc->mfi_poll_waiting = 1;
			selrecord(td, &sc->mfi_select);
			sc->mfi_poll_waiting = 0;
		}
	}

	return revents;
}
