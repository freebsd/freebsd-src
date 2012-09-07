/*	$NecBSD: scsi_low_pisa.c,v 1.13.18.1 2001/06/08 06:27:48 honda Exp $	*/
/*	$NetBSD$	*/
/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/module.h>

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

static moduledata_t scsi_low_moduledata = {
	"scsi_low",
	NULL,
	NULL
};

DECLARE_MODULE(scsi_low, scsi_low_moduledata, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(scsi_low, 1);
MODULE_DEPEND(scsi_low, cam, 1, 1, 1);
