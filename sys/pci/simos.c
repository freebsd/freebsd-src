/*-
 * Copyright (c) 1998 Doug Rabson
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
 *
 * $FreeBSD$
 */

#ifdef COMPILING_LINT
#warning "The simos driver is broken and is not compiled with LINT"
#else

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <machine/clock.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <sys/kernel.h>

#include <pci/simos.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#ifndef COMPAT_OLDPCI
#error "The simos device requires the old pci compatibility shims"
#endif

#include <machine/alpha_cpu.h>

struct simos_softc {
	int			sc_unit;
	SimOS_SCSI*		sc_regs;

	/*
	 * SimOS only supports one pending command.
	 */
	struct cam_sim	       *sc_sim;
	struct cam_path	       *sc_path;
	struct ccb_scsiio      *sc_pending;
};

struct simos_softc* simosp[10];

static u_long simos_unit;

static const char *simos_probe(pcici_t tag, pcidi_t type);
static void simos_attach(pcici_t config_d, int unit);
static void simos_action(struct cam_sim *sim, union ccb *ccb);
static void simos_poll(struct cam_sim *sim);

struct pci_device simos_driver = {
	"simos",
	simos_probe,
	simos_attach,
	&simos_unit,
	NULL
};
COMPAT_PCI_DRIVER (simos, simos_driver);

static const char *
simos_probe(pcici_t tag, pcidi_t type)
{       
	switch (type) {
	case 0x1291|(0x1291<<16):
		return "SimOS SCSI";
	default:
		return NULL;
	}
		
}

static void    
simos_attach(pcici_t config_id, int unit)
{
	struct simos_softc* sc;
	struct cam_devq *devq;

	sc = malloc(sizeof(struct simos_softc), M_DEVBUF, M_ZERO);
	simosp[unit] = sc;

	sc->sc_unit = unit;
	sc->sc_regs = (SimOS_SCSI*) SIMOS_SCSI_ADDR;
	sc->sc_pending = 0;

	devq = cam_simq_alloc(/*maxopenings*/1);
	if (devq == NULL)
		return;

	sc->sc_sim = cam_sim_alloc(simos_action, simos_poll, "simos", sc, unit,
				   /*untagged*/1, /*tagged*/0, devq);
	if (sc->sc_sim == NULL) {
		cam_simq_free(devq);
		return;
	}

	if (xpt_bus_register(sc->sc_sim, /*bus*/0) != CAM_SUCCESS) {
		cam_sim_free(sc->sc_sim, /*free_devq*/TRUE);
		return;
	}

	if (xpt_create_path(&sc->sc_path, /*periph*/NULL,
			    cam_sim_path(sc->sc_sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sc->sc_sim));
		cam_sim_free(sc->sc_sim, /*free_devq*/TRUE);
		return;
	}

	alpha_register_pci_scsi(config_id->bus, config_id->slot, sc->sc_sim);

	return;
}

static void
simos_start(struct simos_softc* sc, struct ccb_scsiio *csio)
{
	struct scsi_generic *cmd;
	int cmdlen;
	caddr_t data;
	int datalen;
	int s;
	u_int8_t* p;
	int i, count, target;
	vm_offset_t va;
	vm_size_t size;

	cmd = (struct scsi_generic *) &csio->cdb_io.cdb_bytes;
	cmdlen = csio->cdb_len;
	data = csio->data_ptr;
	datalen = csio->dxfer_len;

	/*
	 * Simos doesn't understand some commands
	 */
	if (cmd->opcode == START_STOP || cmd->opcode == PREVENT_ALLOW
	    || cmd->opcode == SYNCHRONIZE_CACHE) {
		csio->ccb_h.status = CAM_REQ_CMP;
		xpt_done((union ccb *) csio);
		return;
	}

	if (sc->sc_pending) {
		/*
		 * Don't think this can happen.
		 */
		printf("simos_start: can't start command while one is pending\n");
		csio->ccb_h.status = CAM_BUSY;
		xpt_done((union ccb *) csio);
		return;
	}

	s = splcam();
	
	csio->ccb_h.status |= CAM_SIM_QUEUED;
	sc->sc_pending = csio;

	target = csio->ccb_h.target_id;

	/*
	 * Copy the command into SimOS' buffer
	 */
	p = (u_int8_t*) cmd;
	count = cmdlen;
	for (i = 0; i < count; i++)
		sc->sc_regs->cmd[i] = *p++;
	sc->sc_regs->length = count;
	sc->sc_regs->target = target;
	sc->sc_regs->lun = csio->ccb_h.target_lun;

	/*
	 * Setup the segment descriptors.
	 */
	va = (vm_offset_t) data;
	size = datalen;
	i = 0;
	while (size > 0) {
		vm_size_t len = PAGE_SIZE - (va & PAGE_MASK);
		if (len > size)
			len = size;
		sc->sc_regs->sgMap[i].pAddr = vtophys(va);
		sc->sc_regs->sgMap[i].len = len;
		size -= len;
		va += len;
		i++;
	}
	sc->sc_regs->sgLen = i;

	/*
	 * Start the i/o.
	 */
	alpha_wmb();
	sc->sc_regs->startIO = 1;
	alpha_wmb();

	splx(s);
}

static void
simos_done(struct simos_softc* sc)
{
	struct ccb_scsiio* csio = sc->sc_pending;
	int s, done;

	/*
	 * Spurious interrupt caused by my bogus interrupt broadcasting.
	 */
	if (!csio)
		return;

	sc->sc_pending = 0;

	done = sc->sc_regs->done[csio->ccb_h.target_id];
	if (!done)
		return;

	s = splcam();

	if (done >> 16)
		/* Error detected */
		csio->ccb_h.status = CAM_CMD_TIMEOUT;
	else
		csio->ccb_h.status = CAM_REQ_CMP;

	/*
	 * Ack the interrupt to clear it.
	 */
	sc->sc_regs->done[csio->ccb_h.target_id] = 1;
	alpha_wmb();
	
	xpt_done((union ccb *) csio);

	splx(s);
}

static void
simos_action(struct cam_sim *sim, union ccb *ccb)
{
	struct simos_softc* sc = (struct simos_softc *)sim->softc;

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct ccb_scsiio *csio;

		csio = &ccb->csio;
		simos_start(sc, csio);
		break;
	}

	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);

		ccg->heads = 64;
		ccg->secs_per_track = 32;

		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	case XPT_RESET_BUS:
	{
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;
		
		cpi->version_num = 1; /* XXX??? */
		cpi->max_target = 2;
		cpi->max_lun = 0;
		cpi->initiator_id = 7;
		cpi->bus_id = sim->bus_id;
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "SimOS", HBA_IDLEN);
		strncpy(cpi->dev_name, sim->sim_name, DEV_IDLEN);
		cpi->unit_number = sim->unit_number;

		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

static void
simos_poll(struct cam_sim *sim)
{       
	simos_done(cam_sim_softc(sim));
}

void
simos_intr(int unit)
{
	/* XXX bogus */
	struct simos_softc* sc = simosp[unit];

	simos_done(sc);
}


#endif /* !COMPILING_LINT */
