/* $FreeBSD$ */
/* $Id: isp_freebsd.c,v 1.4 1998/04/15 17:36:08 mjacob Exp $ */
/*
 * Platform (FreeBSD) dependent common attachment code for Qlogic adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <dev/isp/isp_freebsd.h>

static void ispminphys __P((struct buf *));
static u_int32_t isp_adapter_info __P((int));

static struct scsi_adapter isp_switch = {
	ispscsicmd, ispminphys, 0, 0, isp_adapter_info, "isp", { 0, 0 }
};
static struct scsi_device isp_dev = {
	NULL, NULL, NULL, NULL, "isp", 0, { 0, 0 }
};

/*
 * Complete attachment of hardware, include subdevices.
 */
void
isp_attach(isp)
	struct ispsoftc *isp;
{
	struct scsibus_data *scbus;

	scbus = scsi_alloc_bus(); 
	if(!scbus) {
		return;
	}
	isp->isp_state = ISP_RUNSTATE;

	isp->isp_osinfo._link.adapter_unit = isp->isp_osinfo.unit;
	isp->isp_osinfo._link.adapter_softc = isp;
	isp->isp_osinfo._link.adapter = &isp_switch;
	isp->isp_osinfo._link.device = &isp_dev;
	isp->isp_osinfo._link.flags = 0;
	if (isp->isp_type & ISP_HA_FC) {
		isp->isp_osinfo._link.adapter_targ = 0xff;
#if	0
		isp->isp_osinfo._link.openings =
			RQUEST_QUEUE_LEN(isp) / (MAX_FC_TARG-1);
#endif
		scbus->maxtarg = MAX_FC_TARG-1;
	} else {
		isp->isp_osinfo._link.adapter_targ =
			((sdparam *)isp->isp_param)->isp_initiator_id;
#if	0
		isp->isp_osinfo._link.openings =
			RQUEST_QUEUE_LEN(isp) / (MAX_TARGETS-1);
#endif
		scbus->maxtarg = MAX_TARGETS-1;
	}
	/*
	 * Prepare the scsibus_data area for the upperlevel scsi code.
	 */ 
	scbus->adapter_link = &isp->isp_osinfo._link;

	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(scbus);
}


/*
 * minphys our xfers
 *
 * Unfortunately, the buffer pointer describes the target device- not the
 * adapter device, so we can't use the pointer to find out what kind of
 * adapter we are and adjust accordingly.
 */

static void
ispminphys(bp)
	struct buf *bp;
{
	/*
	 * XX: Only the 1020 has a 24 bit limit.
	 */
	if (bp->b_bcount >= (1 << 24)) {
		bp->b_bcount = (1 << 24);
	}
	minphys(bp);
}

static u_int32_t
isp_adapter_info(unit)
	int unit;
{
	/*
 	 * XXX: FIND ISP BASED UPON UNIT AND GET REAL QUEUE LIMIT FROM THAT
	 */
	return (2);
}
