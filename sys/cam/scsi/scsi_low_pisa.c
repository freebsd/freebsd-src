/*	$FreeBSD$	*/
/*	$NecBSD: scsi_low_pisa.c,v 1.13 1998/11/26 14:26:11 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef	__NetBSD__
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isa/pisaif.h>

#include <machine/dvcfg.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_disk.h>

#include <i386/Cbus/dev/scsi_low.h>
#include <i386/Cbus/dev/scsi_low_pisa.h>

#define	SCSIBUS_RESCAN

int
scsi_low_deactivate_pisa(dh)
	pisa_device_handle_t dh;
{
	struct scsi_low_softc *sc = PISA_DEV_SOFTC(dh);

	if (scsi_low_deactivate(sc) != 0)
		return EBUSY;
	return 0;
}

int
scsi_low_activate_pisa(dh)
	pisa_device_handle_t dh;
{
	struct scsi_low_softc *sc = PISA_DEV_SOFTC(dh);
	slot_device_res_t dr = PISA_RES_DR(dh);

	sc->sl_cfgflags = DVCFG_MKCFG(DVCFG_MAJOR(sc->sl_cfgflags), \
				      DVCFG_MINOR(PISA_DR_DVCFG(dr)));
	sc->sl_irq = PISA_DR_IRQ(dr);

	if (scsi_low_activate(sc) != 0)
		return EBUSY;

	/* rescan the scsi bus */
#ifdef	SCSIBUS_RESCAN
	if (scsi_low_is_busy(sc) == 0 &&
	    PISA_RES_EVENT(dh) == PISA_EVENT_INSERT)
		scsi_probe_busses((int) sc->sl_si.si_splp->scsipi_scsi.scsibus,
				  -1, -1);
#endif
	return 0;
}

int
scsi_low_notify_pisa(dh, ev)
	pisa_device_handle_t dh;
	pisa_event_t ev;
{
	struct scsi_low_softc *sc = PISA_DEV_SOFTC(dh);

	switch(ev)
	{
	case PISA_EVENT_QUERY_SUSPEND:
		if (scsi_low_is_busy(sc) != 0)
			return SD_EVENT_STATUS_BUSY;
		break;

	default:
		break;
	}
	return 0;
}
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__ 
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#if __FreeBSD_version >= 500001
#include <sys/bio.h>
#endif
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/device_port.h>

#include <cam/scsi/scsi_low.h>
#include <cam/scsi/scsi_low_pisa.h>

int
scsi_low_deactivate_pisa(sc)
	struct scsi_low_softc *sc;
{

	if (scsi_low_deactivate(sc) != 0)
		return EBUSY;
	return 0;
}

int
scsi_low_activate_pisa(sc, flags)
	struct scsi_low_softc *sc;
	int flags;
{

	sc->sl_cfgflags = ((sc->sl_cfgflags & 0xffff0000) |
			   (flags & 0x00ff));

	if (scsi_low_activate(sc) != 0)
		return EBUSY;
	return 0;
}
#endif	/* __FreeBSD__ */
