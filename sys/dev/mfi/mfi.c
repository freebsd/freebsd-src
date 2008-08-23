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
/*-
 * Copyright (c) 2007 LSI Corp.
 * Copyright (c) 2007 Rajesh Prabhakaran.
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
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/rman.h>
#include <sys/bus_dma.h>
#include <sys/bio.h>
#include <sys/ioccom.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/mfi/mfireg.h>
#include <dev/mfi/mfi_ioctl.h>
#include <dev/mfi/mfivar.h>

static int	mfi_alloc_commands(struct mfi_softc *);
static int	mfi_comms_init(struct mfi_softc *);
static int	mfi_wait_command(struct mfi_softc *, struct mfi_command *);
static int	mfi_get_controller_info(struct mfi_softc *);
static int	mfi_get_log_state(struct mfi_softc *,
		    struct mfi_evt_log_state **);
static int	mfi_parse_entries(struct mfi_softc *, int, int);
static int	mfi_dcmd_command(struct mfi_softc *, struct mfi_command **,
		    uint32_t, void **, size_t);
static void	mfi_data_cb(void *, bus_dma_segment_t *, int, int);
static void	mfi_startup(void *arg);
static void	mfi_intr(void *arg);
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
static int	mfi_linux_ioctl_int(struct cdev *, u_long, caddr_t, int, d_thread_t *);
static void	mfi_timeout(void *);
static int	mfi_user_command(struct mfi_softc *,
		    struct mfi_ioc_passthru *);
static void 	mfi_enable_intr_xscale(struct mfi_softc *sc);
static void 	mfi_enable_intr_ppc(struct mfi_softc *sc);
static int32_t 	mfi_read_fw_status_xscale(struct mfi_softc *sc);
static int32_t 	mfi_read_fw_status_ppc(struct mfi_softc *sc);
static int 	mfi_check_clear_intr_xscale(struct mfi_softc *sc);
static int 	mfi_check_clear_intr_ppc(struct mfi_softc *sc);
static void 	mfi_issue_cmd_xscale(struct mfi_softc *sc,uint32_t bus_add,uint32_t frame_cnt);
static void 	mfi_issue_cmd_ppc(struct mfi_softc *sc,uint32_t bus_add,uint32_t frame_cnt);

SYSCTL_NODE(_hw, OID_AUTO, mfi, CTLFLAG_RD, 0, "MFI driver parameters");
static int	mfi_event_locale = MFI_EVT_LOCALE_ALL;
TUNABLE_INT("hw.mfi.event_locale", &mfi_event_locale);
SYSCTL_INT(_hw_mfi, OID_AUTO, event_locale, CTLFLAG_RW, &mfi_event_locale,
            0, "event message locale");

static int	mfi_event_class = MFI_EVT_CLASS_INFO;
TUNABLE_INT("hw.mfi.event_class", &mfi_event_class);
SYSCTL_INT(_hw_mfi, OID_AUTO, event_class, CTLFLAG_RW, &mfi_event_class,
          0, "event message class");

static int	mfi_max_cmds = 128;
TUNABLE_INT("hw.mfi.max_cmds", &mfi_max_cmds);
SYSCTL_INT(_hw_mfi, OID_AUTO, max_cmds, CTLFLAG_RD, &mfi_max_cmds,
	   0, "Max commands");

/* Management interface */
static d_open_t		mfi_open;
static d_close_t	mfi_close;
static d_ioctl_t	mfi_ioctl;
static d_poll_t		mfi_poll;

static struct cdevsw mfi_cdevsw = {
	.d_version = 	D_VERSION,
	.d_flags =	0,
	.d_open = 	mfi_open,
	.d_close =	mfi_close,
	.d_ioctl =	mfi_ioctl,
	.d_poll =	mfi_poll,
	.d_name =	"mfi",
};

MALLOC_DEFINE(M_MFIBUF, "mfibuf", "Buffers for the MFI driver");

#define MFI_INQ_LENGTH SHORT_INQUIRY_LENGTH

static void
mfi_enable_intr_xscale(struct mfi_softc *sc)
{
	MFI_WRITE4(sc, MFI_OMSK, 0x01);
}

static void
mfi_enable_intr_ppc(struct mfi_softc *sc)
{
	MFI_WRITE4(sc, MFI_ODCR0, 0xFFFFFFFF);
	MFI_WRITE4(sc, MFI_OMSK, ~MFI_1078_EIM);
}

static int32_t
mfi_read_fw_status_xscale(struct mfi_softc *sc)
{
	return MFI_READ4(sc, MFI_OMSG0);
}
 
static int32_t
mfi_read_fw_status_ppc(struct mfi_softc *sc)
{
	return MFI_READ4(sc, MFI_OSP0);
}

static int 
mfi_check_clear_intr_xscale(struct mfi_softc *sc)
{
	int32_t status;

	status = MFI_READ4(sc, MFI_OSTS);
	if ((status & MFI_OSTS_INTR_VALID) == 0)
		return 1;

	MFI_WRITE4(sc, MFI_OSTS, status);
	return 0;
}

static int 
mfi_check_clear_intr_ppc(struct mfi_softc *sc)
{
	int32_t status;

	status = MFI_READ4(sc, MFI_OSTS);
	if (!status)
		return 1; 

	MFI_WRITE4(sc, MFI_ODCR0, status);
	return 0;
}

static void 
mfi_issue_cmd_xscale(struct mfi_softc *sc,uint32_t bus_add,uint32_t frame_cnt)
{
	MFI_WRITE4(sc, MFI_IQP,(bus_add >>3)|frame_cnt);
}
  
static void 
mfi_issue_cmd_ppc(struct mfi_softc *sc,uint32_t bus_add,uint32_t frame_cnt)
{
	MFI_WRITE4(sc, MFI_IQP, (bus_add |frame_cnt <<1)|1 );
}

static int
mfi_transition_firmware(struct mfi_softc *sc)
{
	int32_t fw_state, cur_state;
	int max_wait, i;

	fw_state = sc->mfi_read_fw_status(sc)& MFI_FWSTATE_MASK;
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
			fw_state = sc->mfi_read_fw_status(sc) & MFI_FWSTATE_MASK;
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
	int frames, unit, max_fw_sge;
    device_printf(sc->mfi_dev, "Megaraid SAS driver Ver 2.00 \n");

	mtx_init(&sc->mfi_io_lock, "MFI I/O lock", NULL, MTX_DEF);
	sx_init(&sc->mfi_config_lock, "MFI config");
	TAILQ_INIT(&sc->mfi_ld_tqh);
	TAILQ_INIT(&sc->mfi_aen_pids);
	TAILQ_INIT(&sc->mfi_cam_ccbq);

	mfi_initq_free(sc);
	mfi_initq_ready(sc);
	mfi_initq_busy(sc);
	mfi_initq_bio(sc);

	if (sc->mfi_flags & MFI_FLAGS_1064R) {
		sc->mfi_enable_intr = mfi_enable_intr_xscale;
		sc->mfi_read_fw_status = mfi_read_fw_status_xscale;
		sc->mfi_check_clear_intr = mfi_check_clear_intr_xscale;
		sc->mfi_issue_cmd = mfi_issue_cmd_xscale;
	}
	else {
		sc->mfi_enable_intr =  mfi_enable_intr_ppc;
 		sc->mfi_read_fw_status = mfi_read_fw_status_ppc;
		sc->mfi_check_clear_intr = mfi_check_clear_intr_ppc;
		sc->mfi_issue_cmd = mfi_issue_cmd_ppc;
	}


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
	status = sc->mfi_read_fw_status(sc);
	sc->mfi_max_fw_cmds = status & MFI_FWSTATE_MAXCMD_MASK;
	max_fw_sge = (status & MFI_FWSTATE_MAXSGL_MASK) >> 16;
	sc->mfi_max_sge = min(max_fw_sge, ((MAXPHYS / PAGE_SIZE) + 1));

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
				sc->mfi_max_sge,	/* nsegments */
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
	 * lower 4GB for efficiency.  Calculate the size of the commands at
	 * the same time; each command is one 64 byte frame plus a set of
         * additional frames for holding sg lists or other data.
	 * The assumption here is that the SG list will start at the second
	 * frame and not use the unused bytes in the first frame.  While this
	 * isn't technically correct, it simplifies the calculation and allows
	 * for command frames that might be larger than an mfi_io_frame.
	 */
	if (sizeof(bus_addr_t) == 8) {
		sc->mfi_sge_size = sizeof(struct mfi_sg64);
		sc->mfi_flags |= MFI_FLAGS_SG64;
	} else {
		sc->mfi_sge_size = sizeof(struct mfi_sg32);
	}
	frames = (sc->mfi_sge_size * sc->mfi_max_sge - 1) / MFI_FRAME_SIZE + 2;
	sc->mfi_cmd_size = frames * MFI_FRAME_SIZE;
	framessz = sc->mfi_cmd_size * sc->mfi_max_fw_cmds;
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

	mtx_lock(&sc->mfi_io_lock);
	if ((error = mfi_aen_setup(sc, 0), 0) != 0) {
		mtx_unlock(&sc->mfi_io_lock);
		return (error);
	}
	mtx_unlock(&sc->mfi_io_lock);

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
	    NULL, mfi_intr, sc, &sc->mfi_intr)) {
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
	if (unit == 0)
		make_dev_alias(sc->mfi_cdev, "megaraid_sas_ioctl_node");
	if (sc->mfi_cdev != NULL)
		sc->mfi_cdev->si_drv1 = sc;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(sc->mfi_dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->mfi_dev)),
	    OID_AUTO, "delete_busy_volumes", CTLFLAG_RW,
	    &sc->mfi_delete_busy_volumes, 0, "Allow removal of busy volumes");
	SYSCTL_ADD_INT(device_get_sysctl_ctx(sc->mfi_dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->mfi_dev)),
	    OID_AUTO, "keep_deleted_volumes", CTLFLAG_RW,
	    &sc->mfi_keep_deleted_volumes, 0,
	    "Don't detach the mfid device for a busy volume that is deleted");

	device_add_child(sc->mfi_dev, "mfip", -1);
	bus_generic_attach(sc->mfi_dev);

	/* Start the timeout watchdog */
	callout_init(&sc->mfi_watchdog_callout, CALLOUT_MPSAFE);
	callout_reset(&sc->mfi_watchdog_callout, MFI_CMD_TIMEOUT * hz,
	    mfi_timeout, sc);

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
	ncmds = MIN(mfi_max_cmds, sc->mfi_max_fw_cmds);
	if (bootverbose)
		device_printf(sc->mfi_dev, "Max fw cmds= %d, sizing driver "
		   "pool to %d\n", sc->mfi_max_fw_cmds, ncmds);

	sc->mfi_commands = malloc(sizeof(struct mfi_command) * ncmds, M_MFIBUF,
	    M_WAITOK | M_ZERO);

	for (i = 0; i < ncmds; i++) {
		cm = &sc->mfi_commands[i];
		cm->cm_frame = (union mfi_frame *)((uintptr_t)sc->mfi_frames +
		    sc->mfi_cmd_size * i);
		cm->cm_frame_busaddr = sc->mfi_frames_busaddr +
		    sc->mfi_cmd_size * i;
		cm->cm_frame->header.context = i;
		cm->cm_sense = &sc->mfi_sense[i];
		cm->cm_sense_busaddr= sc->mfi_sense_busaddr + MFI_SENSE_LEN * i;
		cm->cm_sc = sc;
		cm->cm_index = i;
		if (bus_dmamap_create(sc->mfi_buffer_dmat, 0,
		    &cm->cm_dmamap) == 0)
			mfi_release_command(cm);
		else
			break;
		sc->mfi_total_cmds++;
	}

	return (0);
}

void
mfi_release_command(struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	uint32_t *hdr_data;

	/*
	 * Zero out the important fields of the frame, but make sure the
	 * context field is preserved.  For efficiency, handle the fields
	 * as 32 bit words.  Clear out the first S/G entry too for safety.
	 */
	hdr = &cm->cm_frame->header;
	if (cm->cm_data != NULL && hdr->sg_count) {
		cm->cm_sg->sg32[0].len = 0;
		cm->cm_sg->sg32[0].addr = 0;
	}

	hdr_data = (uint32_t *)cm->cm_frame;
	hdr_data[0] = 0;	/* cmd, sense_len, cmd_status, scsi_status */
	hdr_data[1] = 0;	/* target_id, lun_id, cdb_len, sg_count */
	hdr_data[4] = 0;	/* flags, timeout */
	hdr_data[5] = 0;	/* data_len */

	cm->cm_extra_frames = 0;
	cm->cm_flags = 0;
	cm->cm_complete = NULL;
	cm->cm_private = NULL;
	cm->cm_data = NULL;
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
	
	mtx_assert(&sc->mfi_io_lock, MA_OWNED);
	
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

	mtx_lock(&sc->mfi_io_lock);
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
	cm->cm_data = NULL;
	cm->cm_flags = MFI_CMD_POLLED;

	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "failed to send init command\n");
		mtx_unlock(&sc->mfi_io_lock);
		return (error);
	}
	mfi_release_command(cm);
	mtx_unlock(&sc->mfi_io_lock);

	return (0);
}

static int
mfi_get_controller_info(struct mfi_softc *sc)
{
	struct mfi_command *cm = NULL;
	struct mfi_ctrl_info *ci = NULL;
	uint32_t max_sectors_1, max_sectors_2;
	int error;

	mtx_lock(&sc->mfi_io_lock);
	error = mfi_dcmd_command(sc, &cm, MFI_DCMD_CTRL_GETINFO,
	    (void **)&ci, sizeof(*ci));
	if (error)
		goto out;
	cm->cm_flags = MFI_CMD_DATAIN | MFI_CMD_POLLED;

	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Failed to get controller info\n");
		sc->mfi_max_io = (sc->mfi_max_sge - 1) * PAGE_SIZE /
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
	mtx_unlock(&sc->mfi_io_lock);
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
	class_locale.members.locale = mfi_event_locale;
	class_locale.members.class  = mfi_event_class;

	if (seq_start == 0) {
		error = mfi_get_log_state(sc, &log_state);
		if (error) {
			if (log_state)
				free(log_state, M_MFIBUF);
			return (error);
		}

		/*
		 * Walk through any events that fired since the last
		 * shutdown.
		 */
		mfi_parse_entries(sc, log_state->shutdown_seq_num,
		    log_state->newest_seq_num);
		seq = log_state->newest_seq_num;
	} else
		seq = seq_start;
	mfi_aen_register(sc, seq, class_locale.word);
	free(log_state, M_MFIBUF);

	return 0;
}

static int
mfi_wait_command(struct mfi_softc *sc, struct mfi_command *cm)
{

	mtx_assert(&sc->mfi_io_lock, MA_OWNED);
	cm->cm_complete = NULL;


	/*
	 * MegaCli can issue a DCMD of 0.  In this case do nothing
	 * and return 0 to it as status
	 */
	if (cm->cm_frame->dcmd.opcode == 0) {
		cm->cm_frame->header.cmd_status = MFI_STAT_OK;
		cm->cm_error = 0;
		return (cm->cm_error);
	}
	mfi_enqueue_ready(cm);
	mfi_startio(sc);
	if ((cm->cm_flags & MFI_CMD_COMPLETED) == 0)
		msleep(cm, &sc->mfi_io_lock, PRIBIO, "mfiwait", 0);
	return (cm->cm_error);
}

void
mfi_free(struct mfi_softc *sc)
{
	struct mfi_command *cm;
	int i;

	callout_drain(&sc->mfi_watchdog_callout);

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

	if (mtx_initialized(&sc->mfi_io_lock)) {
		mtx_destroy(&sc->mfi_io_lock);
		sx_destroy(&sc->mfi_config_lock);
	}

	return;
}

static void
mfi_startup(void *arg)
{
	struct mfi_softc *sc;

	sc = (struct mfi_softc *)arg;

	config_intrhook_disestablish(&sc->mfi_ich);

	sc->mfi_enable_intr(sc);
	sx_xlock(&sc->mfi_config_lock);
	mtx_lock(&sc->mfi_io_lock);
	mfi_ldprobe(sc);
	mtx_unlock(&sc->mfi_io_lock);
	sx_xunlock(&sc->mfi_config_lock);
}

static void
mfi_intr(void *arg)
{
	struct mfi_softc *sc;
	struct mfi_command *cm;
	uint32_t pi, ci, context;

	sc = (struct mfi_softc *)arg;

	if (sc->mfi_check_clear_intr(sc))
		return;

	pi = sc->mfi_comms->hw_pi;
	ci = sc->mfi_comms->hw_ci;
	mtx_lock(&sc->mfi_io_lock);
	while (ci != pi) {
		context = sc->mfi_comms->hw_reply_q[ci];
		if (context < sc->mfi_max_fw_cmds) {
			cm = &sc->mfi_commands[context];
			mfi_remove_busy(cm);
			cm->cm_error = 0;
			mfi_complete(sc, cm);
		}
		if (++ci == (sc->mfi_max_fw_cmds + 1)) {
			ci = 0;
		}
	}

	sc->mfi_comms->hw_ci = ci;

	/* Give defered I/O a chance to run */
	if (sc->mfi_flags & MFI_FLAGS_QFRZN)
		sc->mfi_flags &= ~MFI_FLAGS_QFRZN;
	mfi_startio(sc);
	mtx_unlock(&sc->mfi_io_lock);

	return;
}

int
mfi_shutdown(struct mfi_softc *sc)
{
	struct mfi_dcmd_frame *dcmd;
	struct mfi_command *cm;
	int error;

	mtx_lock(&sc->mfi_io_lock);
	error = mfi_dcmd_command(sc, &cm, MFI_DCMD_CTRL_SHUTDOWN, NULL, 0);
	if (error) {
		mtx_unlock(&sc->mfi_io_lock);
		return (error);
	}

	if (sc->mfi_aen_cm != NULL)
		mfi_abort(sc, sc->mfi_aen_cm);

	dcmd = &cm->cm_frame->dcmd;
	dcmd->header.flags = MFI_FRAME_DIR_NONE;
	cm->cm_flags = MFI_CMD_POLLED;
	cm->cm_data = NULL;

	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "Failed to shutdown controller\n");
	}

	mfi_release_command(cm);
	mtx_unlock(&sc->mfi_io_lock);
	return (error);
}

static void
mfi_ldprobe(struct mfi_softc *sc)
{
	struct mfi_frame_header *hdr;
	struct mfi_command *cm = NULL;
	struct mfi_ld_list *list = NULL;
	struct mfi_disk *ld;
	int error, i;

	sx_assert(&sc->mfi_config_lock, SA_XLOCKED);
	mtx_assert(&sc->mfi_io_lock, MA_OWNED);

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

	for (i = 0; i < list->ld_count; i++) {
		TAILQ_FOREACH(ld, &sc->mfi_ld_tqh, ld_link) {
			if (ld->ld_id == list->ld_list[i].ld.v.target_id)
				goto skip_add;
		}
		mfi_add_ld(sc, list->ld_list[i].ld.v.target_id);
	skip_add:;
	}
out:
	if (list)
		free(list, M_MFIBUF);
	if (cm)
		mfi_release_command(cm);

	return;
}

/*
 * The timestamp is the number of seconds since 00:00 Jan 1, 2000.  If
 * the bits in 24-31 are all set, then it is the number of seconds since
 * boot.
 */
static const char *
format_timestamp(uint32_t timestamp)
{
	static char buffer[32];

	if ((timestamp & 0xff000000) == 0xff000000)
		snprintf(buffer, sizeof(buffer), "boot + %us", timestamp &
		    0x00ffffff);
	else
		snprintf(buffer, sizeof(buffer), "%us", timestamp);
	return (buffer);
}

static const char *
format_class(int8_t class)
{
	static char buffer[6];

	switch (class) {
	case MFI_EVT_CLASS_DEBUG:
		return ("debug");
	case MFI_EVT_CLASS_PROGRESS:
		return ("progress");
	case MFI_EVT_CLASS_INFO:
		return ("info");
	case MFI_EVT_CLASS_WARNING:
		return ("WARN");
	case MFI_EVT_CLASS_CRITICAL:
		return ("CRIT");
	case MFI_EVT_CLASS_FATAL:
		return ("FATAL");
	case MFI_EVT_CLASS_DEAD:
		return ("DEAD");
	default:
		snprintf(buffer, sizeof(buffer), "%d", class);
		return (buffer);
	}
}

static void
mfi_decode_evt(struct mfi_softc *sc, struct mfi_evt_detail *detail)
{

	device_printf(sc->mfi_dev, "%d (%s/0x%04x/%s) - %s\n", detail->seq,
	    format_timestamp(detail->time), detail->class.members.locale,
	    format_class(detail->class.members.class), detail->description);
}

static int
mfi_aen_register(struct mfi_softc *sc, int seq, int locale)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd;
	union mfi_evt current_aen, prior_aen;
	struct mfi_evt_detail *ed = NULL;
	int error = 0;

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
	if (error) {
		goto out;
	}

	dcmd = &cm->cm_frame->dcmd;
	((uint32_t *)&dcmd->mbox)[0] = seq;
	((uint32_t *)&dcmd->mbox)[1] = locale;
	cm->cm_flags = MFI_CMD_DATAIN;
	cm->cm_complete = mfi_aen_complete;

	sc->mfi_aen_cm = cm;

	mfi_enqueue_ready(cm);
	mfi_startio(sc);

out:
	return (error);
}

static void
mfi_aen_complete(struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	struct mfi_softc *sc;
	struct mfi_evt_detail *detail;
	struct mfi_aen *mfi_aen_entry, *tmp;
	int seq = 0, aborted = 0;

	sc = cm->cm_sc;
	hdr = &cm->cm_frame->header;

	if (sc->mfi_aen_cm == NULL)
		return;

	if (sc->mfi_aen_cm->cm_aen_abort || hdr->cmd_status == 0xff) {
		sc->mfi_aen_cm->cm_aen_abort = 0;
		aborted = 1;
	} else {
		sc->mfi_aen_triggered = 1;
		if (sc->mfi_poll_waiting) {
			sc->mfi_poll_waiting = 0;
			selwakeup(&sc->mfi_select);
		}
		detail = cm->cm_data;
		/*
		 * XXX If this function is too expensive or is recursive, then
		 * events should be put onto a queue and processed later.
		 */
		mfi_decode_evt(sc, detail);
		seq = detail->seq + 1;
		TAILQ_FOREACH_SAFE(mfi_aen_entry, &sc->mfi_aen_pids, aen_link, tmp) {
			TAILQ_REMOVE(&sc->mfi_aen_pids, mfi_aen_entry,
			    aen_link);
			PROC_LOCK(mfi_aen_entry->p);
			psignal(mfi_aen_entry->p, SIGIO);
			PROC_UNLOCK(mfi_aen_entry->p);
			free(mfi_aen_entry, M_MFIBUF);
		}
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

#define MAX_EVENTS 15

static int
mfi_parse_entries(struct mfi_softc *sc, int start_seq, int stop_seq)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd;
	struct mfi_evt_list *el;
	union mfi_evt class_locale;
	int error, i, seq, size;

	class_locale.members.reserved = 0;
	class_locale.members.locale = mfi_event_locale;
	class_locale.members.class  = mfi_event_class;

	size = sizeof(struct mfi_evt_list) + sizeof(struct mfi_evt_detail)
		* (MAX_EVENTS - 1);
	el = malloc(size, M_MFIBUF, M_NOWAIT | M_ZERO);
	if (el == NULL)
		return (ENOMEM);

	for (seq = start_seq;;) {
		if ((cm = mfi_dequeue_free(sc)) == NULL) {
			free(el, M_MFIBUF);
			return (EBUSY);
		}

		dcmd = &cm->cm_frame->dcmd;
		bzero(dcmd->mbox, MFI_MBOX_SIZE);
		dcmd->header.cmd = MFI_CMD_DCMD;
		dcmd->header.timeout = 0;
		dcmd->header.data_len = size;
		dcmd->opcode = MFI_DCMD_CTRL_EVENT_GET;
		((uint32_t *)&dcmd->mbox)[0] = seq;
		((uint32_t *)&dcmd->mbox)[1] = class_locale.word;
		cm->cm_sg = &dcmd->sgl;
		cm->cm_total_frame_size = MFI_DCMD_FRAME_SIZE;
		cm->cm_flags = MFI_CMD_DATAIN | MFI_CMD_POLLED;
		cm->cm_data = el;
		cm->cm_len = size;

		if ((error = mfi_mapcmd(sc, cm)) != 0) {
			device_printf(sc->mfi_dev,
			    "Failed to get controller entries\n");
			mfi_release_command(cm);
			break;
		}

		bus_dmamap_sync(sc->mfi_buffer_dmat, cm->cm_dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->mfi_buffer_dmat, cm->cm_dmamap);

		if (dcmd->header.cmd_status == MFI_STAT_NOT_FOUND) {
			mfi_release_command(cm);
			break;
		}
		if (dcmd->header.cmd_status != MFI_STAT_OK) {
			device_printf(sc->mfi_dev,
			    "Error %d fetching controller entries\n",
			    dcmd->header.cmd_status);
			mfi_release_command(cm);
			break;
		}
		mfi_release_command(cm);

		for (i = 0; i < el->count; i++) {
			/*
			 * If this event is newer than 'stop_seq' then
			 * break out of the loop.  Note that the log
			 * is a circular buffer so we have to handle
			 * the case that our stop point is earlier in
			 * the buffer than our start point.
			 */
			if (el->event[i].seq >= stop_seq) {
				if (start_seq <= stop_seq)
					break;
				else if (el->event[i].seq < start_seq)
					break;
			}
			mfi_decode_evt(sc, &el->event[i]);
		}
		seq = el->event[el->count - 1].seq + 1;
	}

	free(el, M_MFIBUF);
	return (0);
}

static int
mfi_add_ld(struct mfi_softc *sc, int id)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd = NULL;
	struct mfi_ld_info *ld_info = NULL;
	int error;

	mtx_assert(&sc->mfi_io_lock, MA_OWNED);

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

	mtx_unlock(&sc->mfi_io_lock);
	mtx_lock(&Giant);
	if ((child = device_add_child(sc->mfi_dev, "mfid", -1)) == NULL) {
		device_printf(sc->mfi_dev, "Failed to add logical disk\n");
		free(ld_info, M_MFIBUF);
		mtx_unlock(&Giant);
		mtx_lock(&sc->mfi_io_lock);
		return;
	}

	device_set_ivars(child, ld_info);
	device_set_desc(child, "MFI Logical Disk");
	bus_generic_attach(sc->mfi_dev);
	mtx_unlock(&Giant);
	mtx_lock(&sc->mfi_io_lock);
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
	struct ccb_hdr *ccbh;

	for (;;) {
		/* Don't bother if we're short on resources */
		if (sc->mfi_flags & MFI_FLAGS_QFRZN)
			break;

		/* Try a command that has already been prepared */
		cm = mfi_dequeue_ready(sc);

		if (cm == NULL) {
			if ((ccbh = TAILQ_FIRST(&sc->mfi_cam_ccbq)) != NULL)
				cm = sc->mfi_cam_start(ccbh);
		}

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

	mtx_assert(&sc->mfi_io_lock, MA_OWNED);

	if (cm->cm_data != NULL) {
		polled = (cm->cm_flags & MFI_CMD_POLLED) ? BUS_DMA_NOWAIT : 0;
		error = bus_dmamap_load(sc->mfi_buffer_dmat, cm->cm_dmamap,
		    cm->cm_data, cm->cm_len, mfi_data_cb, cm, polled);
		if (error == EINPROGRESS) {
			sc->mfi_flags |= MFI_FLAGS_QFRZN;
			return (0);
		}
	} else {
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

	cm = (struct mfi_command *)arg;
	sc = cm->cm_sc;
	hdr = &cm->cm_frame->header;
	sgl = cm->cm_sg;

	if (error) {
		printf("error %d in callback\n", error);
		cm->cm_error = error;
		mfi_complete(sc, cm);
		return;
	}

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
	cm->cm_total_frame_size += (sc->mfi_sge_size * nsegs);
	cm->cm_extra_frames = (cm->cm_total_frame_size - 1) / MFI_FRAME_SIZE;

	mfi_send_frame(sc, cm);

	return;
}

static int
mfi_send_frame(struct mfi_softc *sc, struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	int tm = MFI_POLL_TIMEOUT_SECS * 1000;

	hdr = &cm->cm_frame->header;

	if ((cm->cm_flags & MFI_CMD_POLLED) == 0) {
		cm->cm_timestamp = time_uptime;
		mfi_enqueue_busy(cm);
	} else {
		hdr->cmd_status = 0xff;
		hdr->flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;
	}

	/*
	 * The bus address of the command is aligned on a 64 byte boundary,
	 * leaving the least 6 bits as zero.  For whatever reason, the
	 * hardware wants the address shifted right by three, leaving just
	 * 3 zero bits.  These three bits are then used as a prefetching
	 * hint for the hardware to predict how many frames need to be
	 * fetched across the bus.  If a command has more than 8 frames
	 * then the 3 bits are set to 0x7 and the firmware uses other
	 * information in the command to determine the total amount to fetch.
	 * However, FreeBSD doesn't support I/O larger than 128K, so 8 frames
	 * is enough for both 32bit and 64bit systems.
	 */
	if (cm->cm_extra_frames > 7)
		cm->cm_extra_frames = 7;

	sc->mfi_issue_cmd(sc,cm->cm_frame_busaddr,cm->cm_extra_frames);

	if ((cm->cm_flags & MFI_CMD_POLLED) == 0)
		return (0);

	/* This is a polled command, so busy-wait for it to complete. */
	while (hdr->cmd_status == 0xff) {
		DELAY(1000);
		tm -= 1;
		if (tm <= 0)
			break;
	}

	if (hdr->cmd_status == 0xff) {
		device_printf(sc->mfi_dev, "Frame %p timed out "
			      "command 0x%X\n", hdr, cm->cm_frame->dcmd.opcode);
		return (ETIMEDOUT);
	}

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

	cm->cm_flags |= MFI_CMD_COMPLETED;

	if (cm->cm_complete != NULL)
		cm->cm_complete(cm);
	else
		wakeup(cm);
}

static int
mfi_abort(struct mfi_softc *sc, struct mfi_command *cm_abort)
{
	struct mfi_command *cm;
	struct mfi_abort_frame *abort;
	int i = 0;

	mtx_assert(&sc->mfi_io_lock, MA_OWNED);

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
	cm->cm_flags = MFI_CMD_POLLED;

	sc->mfi_aen_cm->cm_aen_abort = 1;
	mfi_mapcmd(sc, cm);
	mfi_release_command(cm);

	while (i < 5 && sc->mfi_aen_cm != NULL) {
		msleep(&sc->mfi_aen_cm, &sc->mfi_io_lock, 0, "mfiabort", 5 * hz);
		i++;
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

	error = mfi_mapcmd(sc, cm);
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
	int error;

	sc = dev->si_drv1;

	mtx_lock(&sc->mfi_io_lock);
	if (sc->mfi_detaching)
		error = ENXIO;
	else {
		sc->mfi_flags |= MFI_FLAGS_OPEN;
		error = 0;
	}
	mtx_unlock(&sc->mfi_io_lock);

	return (error);
}

static int
mfi_close(struct cdev *dev, int flags, int fmt, d_thread_t *td)
{
	struct mfi_softc *sc;
	struct mfi_aen *mfi_aen_entry, *tmp;

	sc = dev->si_drv1;

	mtx_lock(&sc->mfi_io_lock);
	sc->mfi_flags &= ~MFI_FLAGS_OPEN;

	TAILQ_FOREACH_SAFE(mfi_aen_entry, &sc->mfi_aen_pids, aen_link, tmp) {
		if (mfi_aen_entry->p == curproc) {
			TAILQ_REMOVE(&sc->mfi_aen_pids, mfi_aen_entry,
			    aen_link);
			free(mfi_aen_entry, M_MFIBUF);
		}
	}
	mtx_unlock(&sc->mfi_io_lock);
	return (0);
}

static int
mfi_config_lock(struct mfi_softc *sc, uint32_t opcode)
{

	switch (opcode) {
	case MFI_DCMD_LD_DELETE:
	case MFI_DCMD_CFG_ADD:
	case MFI_DCMD_CFG_CLEAR:
		sx_xlock(&sc->mfi_config_lock);
		return (1);
	default:
		return (0);
	}
}

static void
mfi_config_unlock(struct mfi_softc *sc, int locked)
{

	if (locked)
		sx_xunlock(&sc->mfi_config_lock);
}

/* Perform pre-issue checks on commands from userland and possibly veto them. */
static int
mfi_check_command_pre(struct mfi_softc *sc, struct mfi_command *cm)
{
	struct mfi_disk *ld, *ld2;
	int error;

	mtx_assert(&sc->mfi_io_lock, MA_OWNED);
	error = 0;
	switch (cm->cm_frame->dcmd.opcode) {
	case MFI_DCMD_LD_DELETE:
		TAILQ_FOREACH(ld, &sc->mfi_ld_tqh, ld_link) {
			if (ld->ld_id == cm->cm_frame->dcmd.mbox[0])
				break;
		}
		if (ld == NULL)
			error = ENOENT;
		else
			error = mfi_disk_disable(ld);
		break;
	case MFI_DCMD_CFG_CLEAR:
		TAILQ_FOREACH(ld, &sc->mfi_ld_tqh, ld_link) {
			error = mfi_disk_disable(ld);
			if (error)
				break;
		}
		if (error) {
			TAILQ_FOREACH(ld2, &sc->mfi_ld_tqh, ld_link) {
				if (ld2 == ld)
					break;
				mfi_disk_enable(ld2);
			}
		}
		break;
	default:
		break;
	}
	return (error);
}

/* Perform post-issue checks on commands from userland. */
static void
mfi_check_command_post(struct mfi_softc *sc, struct mfi_command *cm)
{
	struct mfi_disk *ld, *ldn;

	switch (cm->cm_frame->dcmd.opcode) {
	case MFI_DCMD_LD_DELETE:
		TAILQ_FOREACH(ld, &sc->mfi_ld_tqh, ld_link) {
			if (ld->ld_id == cm->cm_frame->dcmd.mbox[0])
				break;
		}
		KASSERT(ld != NULL, ("volume dissappeared"));
		if (cm->cm_frame->header.cmd_status == MFI_STAT_OK) {
			mtx_unlock(&sc->mfi_io_lock);
			mtx_lock(&Giant);
			device_delete_child(sc->mfi_dev, ld->ld_dev);
			mtx_unlock(&Giant);
			mtx_lock(&sc->mfi_io_lock);
		} else
			mfi_disk_enable(ld);
		break;
	case MFI_DCMD_CFG_CLEAR:
		if (cm->cm_frame->header.cmd_status == MFI_STAT_OK) {
			mtx_unlock(&sc->mfi_io_lock);
			mtx_lock(&Giant);
			TAILQ_FOREACH_SAFE(ld, &sc->mfi_ld_tqh, ld_link, ldn) {
				device_delete_child(sc->mfi_dev, ld->ld_dev);
			}
			mtx_unlock(&Giant);
			mtx_lock(&sc->mfi_io_lock);
		} else {
			TAILQ_FOREACH(ld, &sc->mfi_ld_tqh, ld_link)
				mfi_disk_enable(ld);
		}
		break;
	case MFI_DCMD_CFG_ADD:
		mfi_ldprobe(sc);
		break;
	}
}

static int
mfi_user_command(struct mfi_softc *sc, struct mfi_ioc_passthru *ioc)
{
	struct mfi_command *cm;
	struct mfi_dcmd_frame *dcmd;
	void *ioc_buf = NULL;
	uint32_t context;
	int error = 0, locked;


	if (ioc->buf_size > 0) {
		ioc_buf = malloc(ioc->buf_size, M_MFIBUF, M_WAITOK);
		if (ioc_buf == NULL) {
			return (ENOMEM);
		}
		error = copyin(ioc->buf, ioc_buf, ioc->buf_size);
		if (error) {
			device_printf(sc->mfi_dev, "failed to copyin\n");
			free(ioc_buf, M_MFIBUF);
			return (error);
		}
	}

	locked = mfi_config_lock(sc, ioc->ioc_frame.opcode);

	mtx_lock(&sc->mfi_io_lock);
	while ((cm = mfi_dequeue_free(sc)) == NULL)
		msleep(mfi_user_command, &sc->mfi_io_lock, 0, "mfiioc", hz);

	/* Save context for later */
	context = cm->cm_frame->header.context;

	dcmd = &cm->cm_frame->dcmd;
	bcopy(&ioc->ioc_frame, dcmd, sizeof(struct mfi_dcmd_frame));

	cm->cm_sg = &dcmd->sgl;
	cm->cm_total_frame_size = MFI_DCMD_FRAME_SIZE;
	cm->cm_data = ioc_buf;
	cm->cm_len = ioc->buf_size;

	/* restore context */
	cm->cm_frame->header.context = context;

	/* Cheat since we don't know if we're writing or reading */
	cm->cm_flags = MFI_CMD_DATAIN | MFI_CMD_DATAOUT;

	error = mfi_check_command_pre(sc, cm);
	if (error)
		goto out;

	error = mfi_wait_command(sc, cm);
	if (error) {
		device_printf(sc->mfi_dev, "ioctl failed %d\n", error);
		goto out;
	}
	bcopy(dcmd, &ioc->ioc_frame, sizeof(struct mfi_dcmd_frame));
	mfi_check_command_post(sc, cm);
out:
	mfi_release_command(cm);
	mtx_unlock(&sc->mfi_io_lock);
	mfi_config_unlock(sc, locked);
	if (ioc->buf_size > 0)
		error = copyout(ioc_buf, ioc->buf, ioc->buf_size);
	if (ioc_buf)
		free(ioc_buf, M_MFIBUF);
	return (error);
}

#ifdef __amd64__
#define	PTRIN(p)		((void *)(uintptr_t)(p))
#else
#define	PTRIN(p)		(p)
#endif

static int
mfi_ioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag, d_thread_t *td)
{
	struct mfi_softc *sc;
	union mfi_statrequest *ms;
	struct mfi_ioc_packet *ioc;
#ifdef __amd64__
	struct mfi_ioc_packet32 *ioc32;
#endif
	struct mfi_ioc_aen *aen;
	struct mfi_command *cm = NULL;
	uint32_t context;
	uint8_t *sense_ptr;
	uint8_t *data = NULL, *temp;
	int i;
	struct mfi_ioc_passthru *iop = (struct mfi_ioc_passthru *)arg;
#ifdef __amd64__
	struct mfi_ioc_passthru32 *iop32 = (struct mfi_ioc_passthru32 *)arg;
	struct mfi_ioc_passthru iop_swab;
#endif
	int error, locked;

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
	case MFIIO_QUERY_DISK:
	{
		struct mfi_query_disk *qd;
		struct mfi_disk *ld;

		qd = (struct mfi_query_disk *)arg;
		mtx_lock(&sc->mfi_io_lock);
		TAILQ_FOREACH(ld, &sc->mfi_ld_tqh, ld_link) {
			if (ld->ld_id == qd->array_id)
				break;
		}
		if (ld == NULL) {
			qd->present = 0;
			mtx_unlock(&sc->mfi_io_lock);
			return (0);
		}
		qd->present = 1;
		if (ld->ld_flags & MFI_DISK_FLAGS_OPEN)
			qd->open = 1;
		bzero(qd->devname, SPECNAMELEN + 1);
		snprintf(qd->devname, SPECNAMELEN, "mfid%d", ld->ld_unit);
		mtx_unlock(&sc->mfi_io_lock);
		break;
	}
	case MFI_CMD:
#ifdef __amd64__
	case MFI_CMD32:
#endif
		{
		devclass_t devclass;
		ioc = (struct mfi_ioc_packet *)arg;
		int adapter;

		adapter = ioc->mfi_adapter_no;
		if (device_get_unit(sc->mfi_dev) == 0 && adapter != 0) {
			devclass = devclass_find("mfi");
			sc = devclass_get_softc(devclass, adapter);
		}
		mtx_lock(&sc->mfi_io_lock);
		if ((cm = mfi_dequeue_free(sc)) == NULL) {
			mtx_unlock(&sc->mfi_io_lock);
			return (EBUSY);
		}
		mtx_unlock(&sc->mfi_io_lock);
		locked = 0;

		/*
		 * save off original context since copying from user
		 * will clobber some data
		 */
		context = cm->cm_frame->header.context;

		bcopy(ioc->mfi_frame.raw, cm->cm_frame,
		      2 * MFI_DCMD_FRAME_SIZE);  /* this isn't quite right */
		cm->cm_total_frame_size = (sizeof(union mfi_sgl) * ioc->mfi_sge_count) + ioc->mfi_sgl_off;
		if (ioc->mfi_sge_count) {
			cm->cm_sg =
			    (union mfi_sgl *)&cm->cm_frame->bytes[ioc->mfi_sgl_off];
		}
		cm->cm_flags = 0;
		if (cm->cm_frame->header.flags & MFI_FRAME_DATAIN)
			cm->cm_flags |= MFI_CMD_DATAIN;
		if (cm->cm_frame->header.flags & MFI_FRAME_DATAOUT)
			cm->cm_flags |= MFI_CMD_DATAOUT;
		/* Legacy app shim */
		if (cm->cm_flags == 0)
			cm->cm_flags |= MFI_CMD_DATAIN | MFI_CMD_DATAOUT;
		cm->cm_len = cm->cm_frame->header.data_len;
		if (cm->cm_flags & (MFI_CMD_DATAIN | MFI_CMD_DATAOUT)) {
			cm->cm_data = data = malloc(cm->cm_len, M_MFIBUF,
			    M_WAITOK | M_ZERO);
			if (cm->cm_data == NULL) {
				device_printf(sc->mfi_dev, "Malloc failed\n");
				goto out;
			}
		} else {
			cm->cm_data = 0;
		}

		/* restore header context */
		cm->cm_frame->header.context = context;

		temp = data;
		if (cm->cm_flags & MFI_CMD_DATAOUT) {
			for (i = 0; i < ioc->mfi_sge_count; i++) {
#ifdef __amd64__
				if (cmd == MFI_CMD) {
					/* Native */
					error = copyin(ioc->mfi_sgl[i].iov_base,
					       temp,
					       ioc->mfi_sgl[i].iov_len);
				} else {
					void *temp_convert;
					/* 32bit */
					ioc32 = (struct mfi_ioc_packet32 *)ioc;
					temp_convert =
					    PTRIN(ioc32->mfi_sgl[i].iov_base);
					error = copyin(temp_convert,
					       temp,
					       ioc32->mfi_sgl[i].iov_len);
				}
#else
				error = copyin(ioc->mfi_sgl[i].iov_base,
				       temp,
				       ioc->mfi_sgl[i].iov_len);
#endif
				if (error != 0) {
					device_printf(sc->mfi_dev,
					    "Copy in failed\n");
					goto out;
				}
				temp = &temp[ioc->mfi_sgl[i].iov_len];
			}
		}

		if (cm->cm_frame->header.cmd == MFI_CMD_DCMD)
			locked = mfi_config_lock(sc, cm->cm_frame->dcmd.opcode);

		mtx_lock(&sc->mfi_io_lock);
		error = mfi_check_command_pre(sc, cm);
		if (error) {
			mtx_unlock(&sc->mfi_io_lock);
			goto out;
		}

		if ((error = mfi_wait_command(sc, cm)) != 0) {
			device_printf(sc->mfi_dev,
			    "Controller polled failed\n");
			mtx_unlock(&sc->mfi_io_lock);
			goto out;
		}

		mfi_check_command_post(sc, cm);
		mtx_unlock(&sc->mfi_io_lock);

		temp = data;
		if (cm->cm_flags & MFI_CMD_DATAIN) {
			for (i = 0; i < ioc->mfi_sge_count; i++) {
#ifdef __amd64__
				if (cmd == MFI_CMD) {
					/* Native */
					error = copyout(temp,
						ioc->mfi_sgl[i].iov_base,
						ioc->mfi_sgl[i].iov_len);
				} else {
					void *temp_convert;
					/* 32bit */
					ioc32 = (struct mfi_ioc_packet32 *)ioc;
					temp_convert =
					    PTRIN(ioc32->mfi_sgl[i].iov_base);
					error = copyout(temp,
						temp_convert,
						ioc32->mfi_sgl[i].iov_len);
				}
#else
				error = copyout(temp,
					ioc->mfi_sgl[i].iov_base,
					ioc->mfi_sgl[i].iov_len);
#endif
				if (error != 0) {
					device_printf(sc->mfi_dev,
					    "Copy out failed\n");
					goto out;
				}
				temp = &temp[ioc->mfi_sgl[i].iov_len];
			}
		}

		if (ioc->mfi_sense_len) {
			/* copy out sense */
			sense_ptr = &((struct mfi_ioc_packet*)arg)
			    ->mfi_frame.raw[0];
			error = copyout(cm->cm_sense, sense_ptr,
			    ioc->mfi_sense_len);
			if (error != 0) {
				device_printf(sc->mfi_dev,
				    "Copy out failed\n");
				goto out;
			}
		}

		ioc->mfi_frame.hdr.cmd_status = cm->cm_frame->header.cmd_status;
out:
		mfi_config_unlock(sc, locked);
		if (data)
			free(data, M_MFIBUF);
		if (cm) {
			mtx_lock(&sc->mfi_io_lock);
			mfi_release_command(cm);
			mtx_unlock(&sc->mfi_io_lock);
		}

		break;
		}
	case MFI_SET_AEN:
		aen = (struct mfi_ioc_aen *)arg;
		error = mfi_aen_register(sc, aen->aen_seq_num,
		    aen->aen_class_locale);

		break;
	case MFI_LINUX_CMD_2: /* Firmware Linux ioctl shim */
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
	case MFI_LINUX_SET_AEN_2: /* AEN Linux ioctl shim */
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
#ifdef __amd64__
	case MFIIO_PASSTHRU32:
		iop_swab.ioc_frame	= iop32->ioc_frame;
		iop_swab.buf_size	= iop32->buf_size;
		iop_swab.buf		= PTRIN(iop32->buf);
		iop			= &iop_swab;
		/* FALLTHROUGH */
#endif
	case MFIIO_PASSTHRU:
		error = mfi_user_command(sc, iop);
#ifdef __amd64__
		if (cmd == MFIIO_PASSTHRU32)
			iop32->ioc_frame = iop_swab.ioc_frame;
#endif
		break;
	default:
		device_printf(sc->mfi_dev, "IOCTL 0x%lx not handled\n", cmd);
		error = ENOENT;
		break;
	}

	return (error);
}

static int
mfi_linux_ioctl_int(struct cdev *dev, u_long cmd, caddr_t arg, int flag, d_thread_t *td)
{
	struct mfi_softc *sc;
	struct mfi_linux_ioc_packet l_ioc;
	struct mfi_linux_ioc_aen l_aen;
	struct mfi_command *cm = NULL;
	struct mfi_aen *mfi_aen_entry;
	uint8_t *sense_ptr;
	uint32_t context;
	uint8_t *data = NULL, *temp;
	int i;
	int error, locked;

	sc = dev->si_drv1;
	error = 0;
	switch (cmd) {
	case MFI_LINUX_CMD_2: /* Firmware Linux ioctl shim */
		error = copyin(arg, &l_ioc, sizeof(l_ioc));
		if (error != 0)
			return (error);

		if (l_ioc.lioc_sge_count > MAX_LINUX_IOCTL_SGE) {
			return (EINVAL);
		}

		mtx_lock(&sc->mfi_io_lock);
		if ((cm = mfi_dequeue_free(sc)) == NULL) {
			mtx_unlock(&sc->mfi_io_lock);
			return (EBUSY);
		}
		mtx_unlock(&sc->mfi_io_lock);
		locked = 0;

		/*
		 * save off original context since copying from user
		 * will clobber some data
		 */
		context = cm->cm_frame->header.context;

		bcopy(l_ioc.lioc_frame.raw, cm->cm_frame,
		      2 * MFI_DCMD_FRAME_SIZE);	/* this isn't quite right */
		cm->cm_total_frame_size = (sizeof(union mfi_sgl) * l_ioc.lioc_sge_count) + l_ioc.lioc_sgl_off;
		if (l_ioc.lioc_sge_count)
			cm->cm_sg =
			    (union mfi_sgl *)&cm->cm_frame->bytes[l_ioc.lioc_sgl_off];
		cm->cm_flags = 0;
		if (cm->cm_frame->header.flags & MFI_FRAME_DATAIN)
			cm->cm_flags |= MFI_CMD_DATAIN;
		if (cm->cm_frame->header.flags & MFI_FRAME_DATAOUT)
			cm->cm_flags |= MFI_CMD_DATAOUT;
		cm->cm_len = cm->cm_frame->header.data_len;
		if (cm->cm_flags & (MFI_CMD_DATAIN | MFI_CMD_DATAOUT)) {
			cm->cm_data = data = malloc(cm->cm_len, M_MFIBUF,
			    M_WAITOK | M_ZERO);
			if (cm->cm_data == NULL) {
				device_printf(sc->mfi_dev, "Malloc failed\n");
				goto out;
			}
		} else {
			cm->cm_data = 0;
		}

		/* restore header context */
		cm->cm_frame->header.context = context;

		temp = data;
		if (cm->cm_flags & MFI_CMD_DATAOUT) {
			for (i = 0; i < l_ioc.lioc_sge_count; i++) {
				error = copyin(PTRIN(l_ioc.lioc_sgl[i].iov_base),
				       temp,
				       l_ioc.lioc_sgl[i].iov_len);
				if (error != 0) {
					device_printf(sc->mfi_dev,
					    "Copy in failed\n");
					goto out;
				}
				temp = &temp[l_ioc.lioc_sgl[i].iov_len];
			}
		}

		if (cm->cm_frame->header.cmd == MFI_CMD_DCMD)
			locked = mfi_config_lock(sc, cm->cm_frame->dcmd.opcode);

		mtx_lock(&sc->mfi_io_lock);
		error = mfi_check_command_pre(sc, cm);
		if (error) {
			mtx_unlock(&sc->mfi_io_lock);
			goto out;
		}

		if ((error = mfi_wait_command(sc, cm)) != 0) {
			device_printf(sc->mfi_dev,
			    "Controller polled failed\n");
			mtx_unlock(&sc->mfi_io_lock);
			goto out;
		}

		mfi_check_command_post(sc, cm);
		mtx_unlock(&sc->mfi_io_lock);

		temp = data;
		if (cm->cm_flags & MFI_CMD_DATAIN) {
			for (i = 0; i < l_ioc.lioc_sge_count; i++) {
				error = copyout(temp,
					PTRIN(l_ioc.lioc_sgl[i].iov_base),
					l_ioc.lioc_sgl[i].iov_len);
				if (error != 0) {
					device_printf(sc->mfi_dev,
					    "Copy out failed\n");
					goto out;
				}
				temp = &temp[l_ioc.lioc_sgl[i].iov_len];
			}
		}

		if (l_ioc.lioc_sense_len) {
			/* copy out sense */
			sense_ptr = &((struct mfi_linux_ioc_packet*)arg)
			    ->lioc_frame.raw[0];
			error = copyout(cm->cm_sense, sense_ptr,
			    l_ioc.lioc_sense_len);
			if (error != 0) {
				device_printf(sc->mfi_dev,
				    "Copy out failed\n");
				goto out;
			}
		}

		error = copyout(&cm->cm_frame->header.cmd_status,
			&((struct mfi_linux_ioc_packet*)arg)
			->lioc_frame.hdr.cmd_status,
			1);
		if (error != 0) {
			device_printf(sc->mfi_dev,
				      "Copy out failed\n");
			goto out;
		}

out:
		mfi_config_unlock(sc, locked);
		if (data)
			free(data, M_MFIBUF);
		if (cm) {
			mtx_lock(&sc->mfi_io_lock);
			mfi_release_command(cm);
			mtx_unlock(&sc->mfi_io_lock);
		}

		return (error);
	case MFI_LINUX_SET_AEN_2: /* AEN Linux ioctl shim */
		error = copyin(arg, &l_aen, sizeof(l_aen));
		if (error != 0)
			return (error);
		printf("AEN IMPLEMENTED for pid %d\n", curproc->p_pid);
		mfi_aen_entry = malloc(sizeof(struct mfi_aen), M_MFIBUF,
		    M_WAITOK);
		mtx_lock(&sc->mfi_io_lock);
		if (mfi_aen_entry != NULL) {
			mfi_aen_entry->p = curproc;
			TAILQ_INSERT_TAIL(&sc->mfi_aen_pids, mfi_aen_entry,
			    aen_link);
		}
		error = mfi_aen_register(sc, l_aen.laen_seq_num,
		    l_aen.laen_class_locale);

		if (error != 0) {
			TAILQ_REMOVE(&sc->mfi_aen_pids, mfi_aen_entry,
			    aen_link);
			free(mfi_aen_entry, M_MFIBUF);
		}
		mtx_unlock(&sc->mfi_io_lock);

		return (error);
	default:
		device_printf(sc->mfi_dev, "IOCTL 0x%lx not handled\n", cmd);
		error = ENOENT;
		break;
	}

	return (error);
}

static int
mfi_poll(struct cdev *dev, int poll_events, struct thread *td)
{
	struct mfi_softc *sc;
	int revents = 0;

	sc = dev->si_drv1;

	if (poll_events & (POLLIN | POLLRDNORM)) {
		if (sc->mfi_aen_triggered != 0) {
			revents |= poll_events & (POLLIN | POLLRDNORM);
			sc->mfi_aen_triggered = 0;
		}
		if (sc->mfi_aen_triggered == 0 && sc->mfi_aen_cm == NULL) {
			revents |= POLLERR;
		}
	}

	if (revents == 0) {
		if (poll_events & (POLLIN | POLLRDNORM)) {
			sc->mfi_poll_waiting = 1;
			selrecord(td, &sc->mfi_select);
		}
	}

	return revents;
}


static void
mfi_dump_all(void)
{
	struct mfi_softc *sc;
	struct mfi_command *cm;
	devclass_t dc;
	time_t deadline;
	int timedout;
	int i;

	dc = devclass_find("mfi");
	if (dc == NULL) {
		printf("No mfi dev class\n");
		return;
	}

	for (i = 0; ; i++) {
		sc = devclass_get_softc(dc, i);
		if (sc == NULL)
			break;
		device_printf(sc->mfi_dev, "Dumping\n\n");
		timedout = 0;
		deadline = time_uptime - MFI_CMD_TIMEOUT;
		mtx_lock(&sc->mfi_io_lock);
		TAILQ_FOREACH(cm, &sc->mfi_busy, cm_link) {
			if (cm->cm_timestamp < deadline) {
				device_printf(sc->mfi_dev,
				    "COMMAND %p TIMEOUT AFTER %d SECONDS\n", cm,
				    (int)(time_uptime - cm->cm_timestamp));
				MFI_PRINT_CMD(cm);
				timedout++;
			}
		}

#if 0
		if (timedout)
			MFI_DUMP_CMDS(SC);
#endif

		mtx_unlock(&sc->mfi_io_lock);
	}

	return;
}

static void
mfi_timeout(void *data)
{
	struct mfi_softc *sc = (struct mfi_softc *)data;
	struct mfi_command *cm;
	time_t deadline;
	int timedout = 0;

	deadline = time_uptime - MFI_CMD_TIMEOUT;
	mtx_lock(&sc->mfi_io_lock);
	TAILQ_FOREACH(cm, &sc->mfi_busy, cm_link) {
		if (sc->mfi_aen_cm == cm)
			continue;
		if ((sc->mfi_aen_cm != cm) && (cm->cm_timestamp < deadline)) {
			device_printf(sc->mfi_dev,
			    "COMMAND %p TIMEOUT AFTER %d SECONDS\n", cm,
			    (int)(time_uptime - cm->cm_timestamp));
			MFI_PRINT_CMD(cm);
			MFI_VALIDATE_CMD(sc, cm);
			timedout++;
		}
	}

#if 0
	if (timedout)
		MFI_DUMP_CMDS(SC);
#endif

	mtx_unlock(&sc->mfi_io_lock);

	callout_reset(&sc->mfi_watchdog_callout, MFI_CMD_TIMEOUT * hz,
	    mfi_timeout, sc);

	if (0)
		mfi_dump_all();
	return;
}
