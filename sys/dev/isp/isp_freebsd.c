/* $Id: $ */
/* release_6_2_99 */
/*
 * Platform (FreeBSD 2.X) dependent common attachment code for Qlogic adapters.
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

#define WATCH_INTERVAL          30

static void ispminphys __P((struct buf *));
static u_int32_t isp_adapter_info __P((int));
static int ispcmd __P((ISP_SCSI_XFER_T *));
static void isp_watch __P((void *arg));

static struct scsi_adapter isp_switch = {
	ispcmd, ispminphys, 0, 0, isp_adapter_info, "isp", { 0, 0 }
};
static struct scsi_device isp_dev = {
	NULL, NULL, NULL, NULL, "isp", 0, { 0, 0 }
};
static int isp_poll __P((struct ispsoftc *, ISP_SCSI_XFER_T *, int));


/*
 * Complete attachment of hardware, include subdevices.
 */
void
isp_attach(struct ispsoftc *isp)
{
	struct scsibus_data *scbus;

	scbus = scsi_alloc_bus(); 
	if(!scbus) {
		return;
	}
	if (isp->isp_state == ISP_INITSTATE)
		isp->isp_state = ISP_RUNSTATE;

        timeout(isp_watch, isp, WATCH_INTERVAL * hz);
	isp->isp_dogactive = 1;

	isp->isp_osinfo._link.adapter_unit = isp->isp_osinfo.unit;
	isp->isp_osinfo._link.adapter_softc = isp;
	isp->isp_osinfo._link.adapter = &isp_switch;
	isp->isp_osinfo._link.device = &isp_dev;
	isp->isp_osinfo._link.flags = 0;
	if (isp->isp_type & ISP_HA_FC) {
		isp->isp_osinfo._link.adapter_targ =
			((fcparam *)isp->isp_param)->isp_loopid;
		scbus->maxtarg = MAX_FC_TARG-1;
	} else {
		int tmp = 0;	/* XXXX: Which Bus? */
		isp->isp_osinfo.delay_throttle_count = 1;
		isp->isp_osinfo._link.adapter_targ =
			((sdparam *)isp->isp_param)->isp_initiator_id;
		scbus->maxtarg = MAX_TARGETS-1;
		(void) isp_control(isp, ISPCTL_RESET_BUS, &tmp);
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
ispminphys(struct buf *bp)
{
	/*
	 * Only the 1020/1040 has a 24 bit limit.
	 */
	if (bp->b_bcount >= (1 << 24)) {
		bp->b_bcount = (1 << 24);
	}
}

static u_int32_t
isp_adapter_info(int unit)
{
	/*
 	 * XXX: FIND ISP BASED UPON UNIT AND GET REAL QUEUE LIMIT FROM THAT
	 */
	return (2);
}

static int
ispcmd(ISP_SCSI_XFER_T *xs)
{
	struct ispsoftc *isp;
	int r, s;

	isp = XS_ISP(xs);
	s = splbio();
	DISABLE_INTS(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			ENABLE_INTS(isp);
			(void) splx(s);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}
		isp->isp_state = ISP_RUNSTATE;
	}
	r = ispscsicmd(xs);
	ENABLE_INTS(isp);
	if (r != CMD_QUEUED || (xs->flags & SCSI_NOMASK) == 0) {
		(void) splx(s);
		return (r);
	}

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if (isp_poll(isp, xs, XS_TIME(xs))) {
		/*
		 * If no other error occurred but we didn't finish,
		 * something bad happened.
		 */
		if (XS_IS_CMD_DONE(xs) == 0) {
			isp->isp_nactive--;
			if (isp->isp_nactive < 0)
				isp->isp_nactive = 0;
			if (XS_NOERR(xs)) {
				isp_lostcmd(isp, xs);
				XS_SETERR(xs, HBA_BOTCH);
			}
		}
	}
	(void) splx(s);
	return (CMD_COMPLETE);
}

static int
isp_poll(struct ispsoftc *isp, ISP_SCSI_XFER_T *xs, int mswait)
{

	while (mswait) {
		/* Try the interrupt handling routine */
		(void)isp_intr((void *)isp);

		/* See if the xs is now done */
		if (XS_IS_CMD_DONE(xs))
			return (0);
		SYS_DELAY(1000);	/* wait one millisecond */
		mswait--;
	}
	return (1);
}

static void
isp_watch(void *arg)
{
	int i;
	struct ispsoftc *isp = arg;
	ISP_SCSI_XFER_T *xs;
	int s;

	/*
	 * Look for completely dead commands (but not polled ones).
	 */
	s = splbio();
	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		if ((xs = (ISP_SCSI_XFER_T *) isp->isp_xflist[i]) == NULL) {
			continue;
		}
		if (XS_TIME(xs) == 0) {
			continue;
		}
		XS_TIME(xs) -= (WATCH_INTERVAL * 1000);

		/*
		 * Avoid later thinking that this
		 * transaction is not being timed.
		 * Then give ourselves to watchdog
		 * periods of grace.
		 */
		if (XS_TIME(xs) == 0)
			XS_TIME(xs) = 1;
		else if (XS_TIME(xs) > -(2 * WATCH_INTERVAL * 1000)) {
			continue;
		}
		if (IS_SCSI(isp)) {
			isp->isp_osinfo.delay_throttle_count = 1;
		}
		if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
			printf("%s: isp_watch failed to abort command\n",
			    isp->isp_name);
			isp_restart(isp);
			break;
		}
	}
	if (isp->isp_osinfo.delay_throttle_count) {
		if (--isp->isp_osinfo.delay_throttle_count == 0) {
			sdparam *sdp = isp->isp_param;
			for (i = 0; i < MAX_TARGETS; i++) {
				sdp->isp_devparam[i].dev_flags |=
					DPARM_WIDE|DPARM_SYNC|DPARM_TQING;
				sdp->isp_devparam[i].dev_update = 1;
			}
			isp->isp_update = 1;
		}
	}
        timeout(isp_watch, isp, WATCH_INTERVAL * hz);
	isp->isp_dogactive = 1;
	splx(s);
}

int
isp_async(isp, cmd, arg)
	struct ispsoftc *isp;
	ispasync_t cmd;
	void *arg;
{
	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
		if (isp->isp_type & ISP_HA_SCSI) {
			sdparam *sdp = isp->isp_param;
			char *wt;
			int mhz, flags, tgt, period;

			tgt = *((int *) arg);

			flags = sdp->isp_devparam[tgt].cur_dflags;
			period = sdp->isp_devparam[tgt].cur_period;
			if ((flags & DPARM_SYNC) && period &&
			    (sdp->isp_devparam[tgt].cur_offset) != 0) {
				if (sdp->isp_lvdmode) {
					switch (period) {
					case 0xa:
						mhz = 40;
						break;
					case 0xb:
						mhz = 33;
						break;
					case 0xc:
						mhz = 25;
						break;
					default:
						mhz = 1000 / (period * 4);
						break;
					}
				} else {
					mhz = 1000 / (period * 4);
				}
			} else {
				mhz = 0;
			}
			switch (flags & (DPARM_WIDE|DPARM_TQING)) {
			case DPARM_WIDE:
				wt = ", 16 bit wide\n";
				break;
			case DPARM_TQING:
				wt = ", Tagged Queueing Enabled\n";
				break;
			case DPARM_WIDE|DPARM_TQING:
				wt = ", 16 bit wide, Tagged Queueing Enabled\n";
				break;
			default:
				wt = "\n";
				break;
			}
			if (mhz) {
				printf("%s: Target %d at %dMHz Max Offset %d%s",
				    isp->isp_name, tgt, mhz,
				    sdp->isp_devparam[tgt].cur_offset, wt);
			} else {
				printf("%s: Target %d Async Mode%s",
				    isp->isp_name, tgt, wt);
			}
		}
		break;
	case ISPASYNC_BUS_RESET:
		printf("%s: SCSI bus reset detected\n", isp->isp_name);
		break;
	case ISPASYNC_LOOP_DOWN:
		printf("%s: Loop DOWN\n", isp->isp_name);
		break;
	case ISPASYNC_LOOP_UP:
		printf("%s: Loop UP\n", isp->isp_name);
		break;
	case ISPASYNC_PDB_CHANGED:
	if (IS_FC(isp)) {
		const char *fmt = "%s: Target %d (Loop 0x%x) Port ID 0x%x "
		    "role %s %s\n Port WWN 0x%08x%08x\n Node WWN 0x%08x%08x\n";
		const static char *roles[4] = {
		    "No", "Target", "Initiator", "Target/Initiator"
		};
		char *ptr;
		fcparam *fcp = isp->isp_param;
		int tgt = *((int *) arg);
		struct lportdb *lp = &fcp->portdb[tgt]; 

		if (lp->valid) {
			ptr = "arrived";
		} else {
			ptr = "disappeared";
		}
		printf(fmt, isp->isp_name, tgt, lp->loopid, lp->portid,
		    roles[lp->roles & 0x3], ptr,
		    (u_int32_t) (lp->port_wwn >> 32),
		    (u_int32_t) (lp->port_wwn & 0xffffffffLL),
		    (u_int32_t) (lp->node_wwn >> 32),
		    (u_int32_t) (lp->node_wwn & 0xffffffffLL));
		break;
	}
#ifdef	ISP2100_FABRIC
	case ISPASYNC_CHANGE_NOTIFY:
		printf("%s: Name Server Database Changed\n", isp->isp_name);
		break;
	case ISPASYNC_FABRIC_DEV:
	{
		int target;
		struct lportdb *lp;
		sns_scrsp_t *resp = (sns_scrsp_t *) arg;
		u_int32_t portid;
		u_int64_t wwn;
		fcparam *fcp = isp->isp_param;

		portid =
		    (((u_int32_t) resp->snscb_port_id[0]) << 16) |
		    (((u_int32_t) resp->snscb_port_id[1]) << 8) |
		    (((u_int32_t) resp->snscb_port_id[2]));
		wwn =
		    (((u_int64_t)resp->snscb_portname[0]) << 56) |
		    (((u_int64_t)resp->snscb_portname[1]) << 48) |
		    (((u_int64_t)resp->snscb_portname[2]) << 40) |
		    (((u_int64_t)resp->snscb_portname[3]) << 32) |
		    (((u_int64_t)resp->snscb_portname[4]) << 24) |
		    (((u_int64_t)resp->snscb_portname[5]) << 16) |
		    (((u_int64_t)resp->snscb_portname[6]) <<  8) |
		    (((u_int64_t)resp->snscb_portname[7]));
		printf("%s: type 0x%x@portid 0x%x 0x%08x%08x\n",
		    isp->isp_name, resp->snscb_port_type, portid,
		    ((u_int32_t)(wwn >> 32)), ((u_int32_t)(wwn & 0xffffffff)));
		if (resp->snscb_port_type != 2)
			break;
		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == wwn)
				break;
		}
		if (target < MAX_FC_TARG) {
			break;
		}
		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == 0)
				break;
		}
		if (target == MAX_FC_TARG) {
			printf("%s: no more space for fabric devices\n",
			    isp->isp_name);
			return (-1);
		}
		lp->port_wwn = lp->node_wwn = wwn;
		lp->portid = portid;
		break;
	}
#endif
	default:
		return (-1);
	}
	return (0);
}

/*
 * Free any associated resources prior to decommissioning and
 * set the card to a known state (so it doesn't wake up and kick
 * us when we aren't expecting it to).
 *
 * Locks are held before coming here.
 */
void
isp_uninit(isp)
	struct ispsoftc *isp;
{
	int s = splbio();
	/*
	 * Leave with interrupts disabled.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_RESET);
	DISABLE_INTS(isp);

	/*
	 * Turn off the watchdog (if active).
	 */
	if (isp->isp_dogactive) {
		untimeout(isp_watch, isp);
		isp->isp_dogactive = 0;
	}
	/*
	 * And out...
	 */
	splx(s);
}
