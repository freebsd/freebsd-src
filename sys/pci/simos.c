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
 *	$Id$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsiconf.h>
#include <scsi/scsi_debug.h>

#include <machine/clock.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <sys/kernel.h>

#include <pci/simos.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/alpha_cpu.h>

#define MAX_SIZE	(64*1024)

struct simos_softc {
	int			sc_unit;
	struct scsi_link	sc_link;
	SimOS_SCSI*		sc_regs;
	int			sc_busy;
	struct scsi_xfer*	sc_head;
	struct scsi_xfer*	sc_tail;
};

struct simos_softc* simosp[10];

static u_long simos_unit;

static char *simos_probe __P((pcici_t tag, pcidi_t type));
static void simos_attach __P((pcici_t config_d, int unit));

struct pci_device simos_driver = {
	"simos",
	simos_probe,
	simos_attach,
	&simos_unit,
	NULL
};
DATA_SET (pcidevice_set, simos_driver);

static int32_t simos_start(struct scsi_xfer * xp);
static void simos_min_phys (struct  buf *bp);
static u_int32_t simos_info(int unit);

static struct scsi_adapter simos_switch =
{
	simos_start,
	simos_min_phys,
	0,
	0,
	simos_info,
	"simos"
};

static struct scsi_device simos_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
	"simos",
};

static char *
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
	struct scsibus_data* scbus;

	sc = malloc(sizeof(struct simos_softc), M_DEVBUF, M_WAITOK);
	simosp[unit] = sc;
	bzero(sc, sizeof *sc);

	sc->sc_unit = unit;
	sc->sc_regs = (SimOS_SCSI*) SIMOS_SCSI_ADDR;
	sc->sc_busy = 0;
	sc->sc_head = 0;
	sc->sc_tail = 0;

	sc->sc_link.adapter_unit = unit;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_targ = SIMOS_SCSI_MAXTARG-1;
	sc->sc_link.fordriver = 0;
	sc->sc_link.adapter = &simos_switch;
	sc->sc_link.device = &simos_dev;
	sc->sc_link.flags = 0;

	scbus = scsi_alloc_bus();
	scbus->adapter_link = &sc->sc_link;
	scbus->maxtarg = 1;
	scbus->maxlun = 0;
	scsi_attachdevs(scbus);
	scbus = 0;
}

static void
simos_enqueue(struct simos_softc* sc, struct scsi_xfer* xp)
{
	if (sc->sc_tail) {
		sc->sc_tail->next = xp;
	} else {
		sc->sc_head = sc->sc_tail = xp;
	}
	xp->next = 0;
}

static void
simos_start_transfer(struct simos_softc* sc)
{
	struct scsi_xfer* xp = sc->sc_head;
	u_int8_t* p;
	int i, count, target;
	vm_offset_t va;
	vm_size_t size;

	if (sc->sc_busy || !xp)
		return;

	sc->sc_busy = TRUE;

	target = xp->sc_link->target;

	/*
	 * Copy the command into SimOS' buffer
	 */
	p = (u_int8_t*) xp->cmd;
	count = xp->cmdlen;
	for (i = 0; i < count; i++)
		sc->sc_regs->cmd[i] = *p++;
	sc->sc_regs->length = count;
	sc->sc_regs->target = target;
	sc->sc_regs->lun = xp->sc_link->lun;

	/*
	 * Setup the segment descriptors.
	 */
	va = (vm_offset_t) xp->data;
	size = xp->datalen;
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
}

static void
simos_done(struct simos_softc* sc)
{
	struct scsi_xfer* xp;
	int done;

	if (!sc->sc_busy)
		return;

	xp = sc->sc_head;

	/*
	 * Spurious interrupt caused by my bogus interrupt broadcasting.
	 */
	if (!sc->sc_regs->done[xp->sc_link->target])
	    return;

	sc->sc_head = xp->next;
	if (!sc->sc_head)
		sc->sc_tail = 0;

	sc->sc_busy = FALSE;

	done = sc->sc_regs->done[xp->sc_link->target];
	if (done >> 16) {
	    /* Error detected */
	    xp->error = XS_TIMEOUT;
	}
	xp->flags |= ITSDONE;

	sc->sc_regs->done[xp->sc_link->target] = 1;
	alpha_wmb();
	
	if (!(xp->flags & SCSI_NOMASK))
		scsi_done(xp);

	if (sc->sc_head)
		simos_start_transfer(sc);
}

static int32_t
simos_start(struct scsi_xfer * xp)
{
	struct simos_softc* sc;
	int flags = xp->flags;
	int retval = 0;
	int s;

	sc = xp->sc_link->adapter_softc;

	/*
	 * Reset (chortle) the adapter.
	 */
	if (flags & SCSI_RESET)
		return COMPLETE;

	/*
	 * Simos doesn't understand some commands
	 */
	if (xp->cmd->opcode == START_STOP || xp->cmd->opcode == PREVENT_ALLOW)
		return COMPLETE;

	s = splbio();
	
	simos_enqueue(sc, xp);
	simos_start_transfer(sc);

	if (flags & SCSI_NOMASK) {
		/*
		 * Poll for result
		 */
		while (!sc->sc_regs->done[xp->sc_link->target])
			;

		simos_done(sc);
		retval = COMPLETE;
	} else
		retval = SUCCESSFULLY_QUEUED;
		
	splx(s);

	return retval;
}

static void
simos_min_phys (struct  buf *bp)
{
	if ((unsigned long)bp->b_bcount > MAX_SIZE) bp->b_bcount = MAX_SIZE;
}

static u_int32_t
simos_info(int unit)
{
	return 1;
}

void
simos_intr(int unit)
{
	/* XXX bogus */
	struct simos_softc* sc = simosp[unit];

	simos_done(sc);
}
