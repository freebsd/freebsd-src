/* $FreeBSD$ */
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

static void ispminphys(struct buf *);
static u_int32_t isp_adapter_info(int);
static int ispcmd_slow(struct scsi_xfer *);
static int ispcmd(struct scsi_xfer *);
static struct scsi_adapter isp_switch = {
	ispcmd_slow, ispminphys, 0, 0, isp_adapter_info, "isp", { 0, 0 }
};
static struct scsi_device isp_dev = {
	NULL, NULL, NULL, NULL, "isp", 0, { 0, 0 }
};
static int isp_poll(struct ispsoftc *, struct scsi_xfer *, int);
static void isp_watch(void *);
static void isp_command_requeue(void *);
static void isp_internal_restart(void *);

/*
 * Complete attachment of hardware, include subdevices.
 */
void
isp_attach(struct ispsoftc *isp)
{
	struct scsibus_data *scbus, *scbusb = NULL;

	scbus = scsi_alloc_bus(); 
	if(!scbus) {
		return;
	}

	isp->isp_state = ISP_RUNSTATE;

	isp->isp_osinfo._link.adapter_unit = isp->isp_osinfo.unit;
	isp->isp_osinfo._link.adapter_softc = isp;
	isp->isp_osinfo._link.device = &isp_dev;
	isp->isp_osinfo._link.flags = 0;
	isp->isp_osinfo._link.opennings = isp->isp_maxcmds;
	isp->isp_osinfo._link.adapter = &isp_switch;
	isp->isp_osinfo.wqf = isp->isp_osinfo.wqt = NULL; /* XXX 2nd Bus? */

	if (IS_FC(isp)) {
		isp->isp_osinfo._link.adapter_targ =
			((fcparam *)isp->isp_param)->isp_loopid;
		scbus->maxtarg = MAX_FC_TARG-1;
	} else {
		sdparam *sdp = isp->isp_param;
		isp->isp_osinfo.discovered[0] = 1 << sdp->isp_initiator_id;
		isp->isp_osinfo._link.adapter_targ = sdp->isp_initiator_id;
		scbus->maxtarg = MAX_TARGETS-1;
		if (IS_12X0(isp) && (scbusb = scsi_alloc_bus()) != NULL) {
			sdp++;
			isp->isp_osinfo._link_b = isp->isp_osinfo._link;
			isp->isp_osinfo._link_b.adapter_targ =
			    sdp->isp_initiator_id;
			isp->isp_osinfo.discovered[1] =
			    1 << sdp->isp_initiator_id;
			isp->isp_osinfo._link_b.adapter_bus = 1;
			scbusb->maxtarg = MAX_TARGETS-1;
		}
	}


	/*
	 * Send a SCSI Bus Reset (used to be done as part of attach,
	 * but now left to the OS outer layers).
	 */
	if (IS_SCSI(isp)) {
		int bus = 0;
		(void) isp_control(isp, ISPCTL_RESET_BUS, &bus);
		if (IS_12X0(isp)) {
			bus++;
			(void) isp_control(isp, ISPCTL_RESET_BUS, &bus);
		}
		SYS_DELAY(2*1000000);
	}

	/*
	 * Start the watchdog.
	 */
        timeout(isp_watch, isp, WATCH_INTERVAL * hz);
	isp->isp_dogactive = 1;

	/*
	 * Prepare the scsibus_data area for the upperlevel scsi code.
	 */ 
	scbus->adapter_link = &isp->isp_osinfo._link;

	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(scbus);

	/*
	 * Attach second bus if there.
	 */
	if (scbusb) {
		scbusb->adapter_link = &isp->isp_osinfo._link_b;
		scsi_attachdevs(scbusb);
	}
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
	return (2);
}

static int
ispcmd_slow(struct scsi_xfer *xs)
{
	struct ispsoftc *isp = XS_ISP(xs);
	/*
	 * Have we completed discovery for this adapter?
	 */
	if (IS_SCSI(isp) && (xs->flags & SCSI_NOMASK) == 0) {
		sdparam *sdp = isp->isp_param;
		int s = splbio();
		int chan = XS_CHANNEL(xs), chmax = IS_12X0(isp)? 2 : 1;
		u_int16_t f = DPARM_DEFAULT;

		sdp += chan;
		if (xs->sc_link->quirks & SCSI_Q_NO_SYNC) {
			f ^= DPARM_SYNC;
		}
		if (xs->sc_link->quirks & SCSI_Q_NO_WIDE) {
			f ^= DPARM_WIDE;
		}
		if (xs->sc_link->quirks & SD_Q_NO_TAGS) {
			f ^= DPARM_TQING;
		}
		sdp->isp_devparam[XS_TGT(xs)].dev_flags = f;
		sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
		isp->isp_osinfo.discovered[chan] |= (1 << XS_TGT(xs));
		f = 0xffff ^ (1 << sdp->isp_initiator_id);
		for (chan = 0; chan < chmax; chan++) {
			if (isp->isp_osinfo.discovered[chan] == f)
				break;
		}
		if (chan == chmax) {
			isp_switch.scsi_cmd = ispcmd;
			isp->isp_update = 1;
			if (IS_12X0(isp))
				isp->isp_update |= 2;
		}
		(void) splx(s);
	}
	return (ispcmd(xs));
}

static int
ispcmd(struct scsi_xfer *xs)
{
	struct ispsoftc *isp;
	int result, s;

	isp = XS_ISP(xs);
	s = splbio();
	if (isp->isp_state < ISP_RUNSTATE) {
		DISABLE_INTS(isp);
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			ENABLE_INTS(isp);
			(void) splx(s);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}
		isp->isp_state = ISP_RUNSTATE;
		ENABLE_INTS(isp);
	}

	/*
	 * Check for queue blockage...
	 */
	if (isp->isp_osinfo.blocked) {
		if (xs->flags & SCSI_NOMASK) {
			xs->error = XS_DRIVER_STUFFUP;
			splx(s);
			return (TRY_AGAIN_LATER);
		}
		if (isp->isp_osinfo.wqf != NULL) {
			isp->isp_osinfo.wqt->next = xs;
		} else {
			isp->isp_osinfo.wqf = xs;
		}
		isp->isp_osinfo.wqt = xs;
		xs->next = NULL;
		splx(s);
		return (SUCCESSFULLY_QUEUED);
	}
	DISABLE_INTS(isp);
	result = ispscsicmd(xs);
	ENABLE_INTS(isp);

	if ((xs->flags & SCSI_NOMASK) == 0) {
		switch (result) {
		case CMD_QUEUED:
			result = SUCCESSFULLY_QUEUED;
			break;
		case CMD_EAGAIN:
			result = TRY_AGAIN_LATER;
			break;
		case CMD_RQLATER:
			result = SUCCESSFULLY_QUEUED;
			timeout(isp_command_requeue, xs, hz);
			break;
		case CMD_COMPLETE:
			result = COMPLETE;
			break;
		}
		(void) splx(s);
		return (result);
	}

	switch (result) {
	case CMD_QUEUED:
		result = SUCCESSFULLY_QUEUED;
		break;
	case CMD_RQLATER:
	case CMD_EAGAIN:
		if (XS_NOERR(xs)) {
			xs->error = XS_DRIVER_STUFFUP;
		}
		result = TRY_AGAIN_LATER;
		break;
	case CMD_COMPLETE:
		result = COMPLETE;
		break;
		
	}

	/*
	 * We can't use interrupts so poll on completion.
	 */
	if (result == SUCCESSFULLY_QUEUED) {
		if (isp_poll(isp, xs, xs->timeout)) {
			/*
			 * If no other error occurred but we didn't finish,
			 * something bad happened.
			 */
			if (XS_IS_CMD_DONE(xs) == 0) {
				if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
					isp_restart(isp);
				}
				if (XS_NOERR(xs)) {
					XS_SETERR(xs, HBA_BOTCH);
				}
			}
		}
		result = COMPLETE;
	}
	(void) splx(s);
	return (result);
}

static int
isp_poll(struct ispsoftc *isp, struct scsi_xfer *xs, int mswait)
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
	struct scsi_xfer *xs;
	int s;

	/*
	 * Look for completely dead commands (but not polled ones).
	 */
	s = splbio();
	for (i = 0; i < isp->isp_maxcmds; i++) {
		if ((xs = (struct scsi_xfer *) isp->isp_xflist[i]) == NULL) {
			continue;
		}
		if (xs->timeout == 0 || (xs->flags & SCSI_NOMASK)) {
			continue;
		}
		xs->timeout -= (WATCH_INTERVAL * 1000);

		/*
		 * Avoid later thinking that this
		 * transaction is not being timed.
		 * Then give ourselves to watchdog
		 * periods of grace.
		 */
		if (xs->timeout == 0) {
			xs->timeout = 1;
		} else if (xs->timeout > -(2 * WATCH_INTERVAL * 1000)) {
			continue;
		}
		if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
			printf("%s: isp_watch failed to abort command\n",
			    isp->isp_name);
			isp_restart(isp);
			break;
		}
	}
        timeout(isp_watch, isp, WATCH_INTERVAL * hz);
	isp->isp_dogactive = 1;
	splx(s);
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

/*
 * Restart function for a command to be requeued later.
 */
static void
isp_command_requeue(void *arg)
{
	struct scsi_xfer *xs = arg;
	struct ispsoftc *isp = XS_ISP(xs);
	int s = splbio();
	switch (ispcmd_slow(xs)) {
	case SUCCESSFULLY_QUEUED:
		printf("%s: isp_command_reque: queued %d.%d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;
	case TRY_AGAIN_LATER:
		printf("%s: EAGAIN for %d.%d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		/* FALLTHROUGH */
	case COMPLETE:
		/* can only be an error */
		if (XS_NOERR(xs))
			XS_SETERR(xs, XS_DRIVER_STUFFUP);
		XS_CMD_DONE(xs);
		break;
	}
	(void) splx(s);
}

/*
 * Restart function after a LOOP UP event (e.g.),
 * done as a timeout for some hysteresis.
 */
static void
isp_internal_restart(void *arg)
{
	struct ispsoftc *isp = arg;
	int result, nrestarted = 0, s;

	s = splbio();
	if (isp->isp_osinfo.blocked == 0) {
		struct scsi_xfer *xs;
		while ((xs = isp->isp_osinfo.wqf) != NULL) {
			isp->isp_osinfo.wqf = xs->next;
			xs->next = NULL;
			DISABLE_INTS(isp);
			result = ispscsicmd(xs);
			ENABLE_INTS(isp);
			if (result != CMD_QUEUED) {
				printf("%s: botched command restart (0x%x)\n",
				    isp->isp_name, result);
				if (XS_NOERR(xs))
					XS_SETERR(xs, XS_DRIVER_STUFFUP);
				XS_CMD_DONE(xs);
			}
			nrestarted++;
		}
		printf("%s: requeued %d commands\n", isp->isp_name, nrestarted);
	}
	(void) splx(s);
}

int
isp_async(struct ispsoftc *isp, ispasync_t cmd, void *arg)
{
	int bus, tgt;
	int s = splbio();
	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
	if (IS_SCSI(isp) && isp->isp_dblev) {
		sdparam *sdp = isp->isp_param;
		char *wt;
		int mhz, flags, period;

		tgt = *((int *) arg);
		bus = (tgt >> 16) & 0xffff;
		tgt &= 0xffff;

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
			printf("%s: Bus %d Target %d at %dMHz Max "
			    "Offset %d%s", isp->isp_name, bus, tgt, mhz,
			    sdp->isp_devparam[tgt].cur_offset, wt);
		} else {
			printf("%s: Bus %d Target %d Async Mode%s",
			    isp->isp_name, bus, tgt, wt);
		}
		break;
	}
	case ISPASYNC_BUS_RESET:
		if (arg)
			bus = *((int *) arg);
		else
			bus = 0;
		printf("%s: SCSI bus %d reset detected\n", isp->isp_name, bus);
		break;
	case ISPASYNC_LOOP_DOWN:
		/*
		 * Hopefully we get here in time to minimize the number
		 * of commands we are firing off that are sure to die.
		 */
		isp->isp_osinfo.blocked = 1;
		printf("%s: Loop DOWN\n", isp->isp_name);
		break;
        case ISPASYNC_LOOP_UP:
		isp->isp_osinfo.blocked = 0;
		timeout(isp_internal_restart, isp, 1);
		printf("%s: Loop UP\n", isp->isp_name);
		break;
	case ISPASYNC_PDB_CHANGED:
	if (IS_FC(isp) && isp->isp_dblev) {
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
		printf("%s: Fabric Device (Type 0x%x)@PortID 0x%x WWN "
		    "0x%08x%08x\n", isp->isp_name, resp->snscb_port_type,
		    portid, ((u_int32_t)(wwn >> 32)),
		    ((u_int32_t)(wwn & 0xffffffff)));
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
		break;
	}
	(void) splx(s);
	return (0);
}
