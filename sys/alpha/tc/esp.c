/* $FreeBSD$ */
/*	$NetBSD: esp.c,v 1.8.4.2 1996/09/10 17:28:16 cgd Exp $	*/

/*
 * Copyright (c) 1994 Peter Galbavy
 * Copyright (c) 1995 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Peter Galbavy
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Based on aic6360 by Jarle Greipsland
 *
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <sys/errno.h>
/*#include <sys/ioctl.h>*/
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
/*#include <sys/user.h>*/
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <machine/cpu.h>
#include <machine/clock.h>
#include <alpha/tc/tcreg.h>
#include <alpha/tc/tcvar.h>
#include <alpha/tc/tcdsreg.h>
#include <alpha/tc/tcdsvar.h>
#include <alpha/tc/espreg.h>
#include <alpha/tc/espvar.h>

int esp_debug = 0; /*ESP_SHOWPHASE|ESP_SHOWMISC|ESP_SHOWTRAC|ESP_SHOWCMDS;*/

/*static*/ void	espattach	(device_t, device_t, void *);
/*static*/ int	espmatch	(device_t, void *, void *);
/*static*/ int	espprint	(void *, char *);
/*static*/ u_int	esp_adapter_info(struct esp_softc *);
/*static*/ void	espreadregs	(struct esp_softc *);
/*static*/ void	espselect	(struct esp_softc *,
				     u_char, u_char, u_char *, u_char);
/*static*/ void	esp_scsi_reset	(struct esp_softc *);
/*static*/ void	esp_reset	(struct esp_softc *);
/*static*/ void	esp_init	(struct esp_softc *, int);
/*static*/ int	esp_scsi_cmd	(struct scsi_xfer *);
/*static*/ int	esp_poll	(struct esp_softc *, struct ecb *);
/*static*/ void	esp_sched	(struct esp_softc *);
/*static*/ void	esp_done	(struct ecb *);
/*static*/ void	esp_msgin	(struct esp_softc *);
/*static*/ void	esp_msgout	(struct esp_softc *);
/*static*/ int	espintr		(struct esp_softc *);
/*static*/ void	esp_timeout	(void *arg);
/*static*/ void	esp_abort	(struct esp_softc *, struct ecb *);
static  u_int32_t esp_info	(int);
int esp_stp2cpb(struct esp_softc *, int);
int esp_cpb2stp(struct esp_softc *, int);

static void esp_min_phys (struct  buf *);
static int esp_probe(device_t dev);
static int esp_attach(device_t dev);
static device_method_t esp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		esp_probe),
	DEVMETHOD(device_attach,	esp_attach),
	{0, 0}
};
#define ESP_SOFTC(dev)	(struct esp_softc*) device_get_softc(dev)
static driver_t esp_driver = {
	"esp",
	esp_methods,
	sizeof(struct esp_softc),
};


struct scsi_adapter esp_switch = {
	esp_scsi_cmd,
	esp_min_phys,		/* no max at this level; handled by DMA code */
	NULL,
	NULL,
	esp_info,
	"esp",
};

struct scsi_device esp_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};
static  devclass_t      esp_devclass;
DRIVER_MODULE(esp, tcds, esp_driver, esp_devclass, 0, 0);

static int
esp_probe(device_t dev)
{
	if(strcmp(device_get_name(device_get_parent(dev)),"tcds")){
		return ENXIO;
	} else {
		device_set_desc(dev,"NCR53C94");
		return(0);
	}
}

/*
 * Attach this instance, and then all the sub-devices
 */
int
esp_attach(device_t dev)
{
	struct esp_softc *sc = device_get_softc(dev);
	struct  tcdsdev_attach_args *tcdsdev = device_get_ivars(dev);
	struct scsibus_data *scbus;

	sc->sc_reg = (volatile u_int32_t *)tcdsdev->tcdsda_addr;
	sc->sc_cookie = tcdsdev->tcdsda_cookie;
	sc->sc_dma = tcdsdev->tcdsda_sc;

	tcds_intr_establish(device_get_parent(dev), sc->sc_cookie, 0,(int (*)(void *))espintr, sc);

/*	if (parent->dv_cfdata->cf_driver == &tcds_cd) {
		sc->sc_id = tcdsdev->tcdsda_id;
		sc->sc_freq = tcdsdev->tcdsda_freq;
		} else*/ {
		/* XXX */
		sc->sc_id = 7;
		sc->sc_freq = 25000000;
	}

	/* gimme Mhz */
	sc->sc_freq /= 1000000;
	sc->sc_dma->sc_esp = sc;		/* XXX */

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the esp_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | ESPCFG1_PARENB;
	sc->sc_cfg2 = ESPCFG2_SCSI2;
	sc->sc_cfg3 = 0x4;		/* Save residual byte. XXX??? */
	sc->sc_rev = NCR53C94;

	/*
	 * This is the value used to start sync negotiations
	 * Note that the ESP register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = 1000 / sc->sc_freq;

	sc->sc_maxxfer = 64 * 1024;
	sc->sc_ccf = FREQTOCCF(sc->sc_freq);

	/* The value *must not* be == 1. Make it 2 */
	if (sc->sc_ccf == 1)
		sc->sc_ccf = 2;

	/*
	 * The recommended timeout is 250ms. This register is loaded
	 * with a value calculated as follows, from the docs:
	 *
	 *		(timout period) x (CLK frequency)
	 *	reg = -------------------------------------
	 *		 8192 x (Clock Conversion Factor)
	 *
	 * Since CCF has a linear relation to CLK, this generally computes
	 * to the constant of 153.
	 */
	sc->sc_timeout = ((250 * 1000) * sc->sc_freq) / (8192 * sc->sc_ccf);

	/* CCF register only has 3 bits; 0 is actually 8 */
	sc->sc_ccf &= 7;

	/* Reset state & bus */
	sc->sc_state = 0;
	esp_init(sc, 1);

	printf("esp%d: %dMhz, target %d\n", device_get_unit(dev),sc->sc_freq, sc->sc_id);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_targ = sc->sc_id;
	sc->sc_link.adapter = &esp_switch;
	sc->sc_link.device = &esp_dev;
/*	sc->sc_link.openings = 2;*/
	sc->sc_link.fordriver = 0;

	/*
	 * If the boot path is "esp" at the moment and it's me, then
	 * walk our pointer to the sub-device, ready for the config
	 * below.
	 */
	/*
	 * Now try to attach all the sub-devices
	 */
/*	config_found(self, &sc->sc_link, espprint);*/
	scbus = scsi_alloc_bus();
	if(!scbus)
		return ENXIO;
	scbus->adapter_link = &sc->sc_link;

	scbus->maxtarg = 7;

	if (bootverbose) {
		unsigned t_from = 0;
		unsigned t_to   = scbus->maxtarg;
		unsigned myaddr = sc->sc_id;

		char *txt_and = "";
		printf ("esp%d scanning for targets ", device_get_unit(dev));
		if (t_from < myaddr) {
			printf ("%d..%d ", t_from, myaddr -1);
			txt_and = "and ";
		}
		if (myaddr < t_to)
			printf ("%s%d..%d ", txt_and, myaddr +1, t_to);
	}
		
	scsi_attachdevs (scbus);
	scbus = NULL;   /* Upper-level SCSI code owns this now */
		

	return 0;
}

/*
 * This is the generic esp reset function. It does not reset the SCSI bus,
 * only this controllers, but kills any on-going commands, and also stops
 * and resets the DMA.
 *
 * After reset, registers are loaded with the defaults from the attach
 * routine above.
 */
void
esp_reset(sc)
	struct esp_softc *sc;
{

	/* reset DMA first */
	DMA_RESET(sc->sc_dma);

	/* reset SCSI chip */
	ESPCMD(sc, ESPCMD_RSTCHIP);
	ESPCMD(sc, ESPCMD_NOP);
	DELAY(500);

	/* do these backwards, and fall through */
	switch (sc->sc_rev) {
	case NCR53C94:
	case ESP200:
		ESP_WRITE_REG(sc, ESP_CFG3, sc->sc_cfg3);
	case ESP100A:
		ESP_WRITE_REG(sc, ESP_CFG2, sc->sc_cfg2);
	case ESP100:
		ESP_WRITE_REG(sc, ESP_CFG1, sc->sc_cfg1);
		ESP_WRITE_REG(sc, ESP_CCF, sc->sc_ccf);
		ESP_WRITE_REG(sc, ESP_SYNCOFF, 0);
		ESP_WRITE_REG(sc, ESP_TIMEOUT, sc->sc_timeout);
		break;
	default:
		printf("%s: unknown revision code, assuming ESP100\n",
			device_get_nameunit(sc->sc_dev));
		ESP_WRITE_REG(sc, ESP_CFG1, sc->sc_cfg1);
		ESP_WRITE_REG(sc, ESP_CCF, sc->sc_ccf);
		ESP_WRITE_REG(sc, ESP_SYNCOFF, 0);
		ESP_WRITE_REG(sc, ESP_TIMEOUT, sc->sc_timeout);
	}
}

/*
 * Reset the SCSI bus, but not the chip
 */
void
esp_scsi_reset(sc)
	struct esp_softc *sc;
{
	/*
	 * XXX STOP DMA FIRST
	 */

	printf("esp: resetting SCSI bus\n");
	ESPCMD(sc, ESPCMD_RSTSCSI);
}

u_int32_t esp_info (int unit)
{
	return (1);   /* may be changed later */
}



/*
 * Initialize esp state machine
 */
void
esp_init(sc, doreset)
	struct esp_softc *sc;
	int doreset;
{
	struct ecb *ecb;
	int r;

	ESP_TRACE(("[ESP_INIT(%d)] ", doreset));

	if (sc->sc_state == 0) {	/* First time through */
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = NULL;
		ecb = sc->sc_ecb;
		bzero(ecb, sizeof(sc->sc_ecb));
		for (r = 0; r < sizeof(sc->sc_ecb) / sizeof(*ecb); r++) {
			TAILQ_INSERT_TAIL(&sc->free_list, ecb, chain);
			ECB_SETQ(ecb, ECB_QFREE);
			ecb++;
		}
		bzero(sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		sc->sc_flags |= ESP_ABORTING;
		sc->sc_state = ESP_IDLE;
		ecb = sc->sc_nexus;
		if (ecb != NULL) {
			ecb->xs->error = XS_TIMEOUT;
			esp_done(ecb);
			sc->sc_nexus = NULL;
		}
		while ((ecb = TAILQ_FIRST(&sc->nexus_list)) != NULL) {
			ecb->xs->error = XS_TIMEOUT;
			esp_done(ecb);
		}
	}

	/*
	 * reset the chip to a known state
	 */
	esp_reset(sc);

	sc->sc_phase = sc->sc_prevphase = INVALID_PHASE;
	for (r = 0; r < 8; r++) {
		struct esp_tinfo *tp = &sc->sc_tinfo[r];
/* XXX - config flags per target: low bits: no reselect; high bits: no synch */
/*		int fl = sc->sc_dev.dv_cfdata->cf_flags;*/
int fl=0;
		tp->flags = ((sc->sc_minsync && !(fl & (1<<(r+8))))
				? T_NEGOTIATE : 0) |
				((fl & (1<<r)) ? T_RSELECTOFF : 0) |
				T_NEED_TO_RESET;
		tp->period = sc->sc_minsync;
		tp->offset = 0;
	}
	sc->sc_flags &= ~ESP_ABORTING;

	if (doreset) {
		sc->sc_state = ESP_SBR;
		ESPCMD(sc, ESPCMD_RSTSCSI);
		return;
	}

	sc->sc_state = ESP_IDLE;
	esp_sched(sc);
	return;
}

/*
 * Read the ESP registers, and save their contents for later use.
 * ESP_STAT, ESP_STEP & ESP_INTR are mostly zeroed out when reading
 * ESP_INTR - so make sure it is the last read.
 *
 * I think that (from reading the docs) most bits in these registers
 * only make sense when he DMA CSR has an interrupt showing. Call only
 * if an interrupt is pending.
 */
void
espreadregs(sc)
	struct esp_softc *sc;
{

	sc->sc_espstat = ESP_READ_REG(sc, ESP_STAT);
	/* Only the stepo bits are of interest */
	sc->sc_espstep = ESP_READ_REG(sc, ESP_STEP) & ESPSTEP_MASK;
	sc->sc_espintr = ESP_READ_REG(sc, ESP_INTR);

	/* Clear the TCDS interrupt bit. */
	(void)tcds_scsi_isintr(sc->sc_dma, 1);

	/*
	 * Determine the SCSI bus phase, return either a real SCSI bus phase
	 * or some pseudo phase we use to detect certain exceptions.
	 */

	sc->sc_phase = (sc->sc_espintr & ESPINTR_DIS)
			? /* Disconnected */ BUSFREE_PHASE
			: sc->sc_espstat & ESPSTAT_PHASE;

	ESP_MISC(("regs[intr=%02x,stat=%02x,step=%02x] ",
		sc->sc_espintr, sc->sc_espstat, sc->sc_espstep));
}

/*
 * Convert Synchronous Transfer Period to chip register Clock Per Byte value.
 */
int
esp_stp2cpb(sc, period)
	struct esp_softc *sc;
	int period;
{
	int v;
	v = (sc->sc_freq * period) / 250;
	if (esp_cpb2stp(sc, v) < period)
		/* Correct round-down error */
		v++;
	return v;
}

/*
 * Convert chip register Clock Per Byte value to Synchronous Transfer Period.
 */
int
esp_cpb2stp(sc, cpb)
	struct esp_softc *sc;
	int cpb;
{
	return ((250 * cpb) / sc->sc_freq);
}

/*
 * Send a command to a target, set the driver state to ESP_SELECTING
 * and let the caller take care of the rest.
 *
 * Keeping this as a function allows me to say that this may be done
 * by DMA instead of programmed I/O soon.
 */
void
espselect(sc, target, lun, cmd, clen)
	struct esp_softc *sc;
	u_char target, lun;
	u_char *cmd;
	u_char clen;
{
	struct esp_tinfo *ti = &sc->sc_tinfo[target];
	int i;

	ESP_TRACE(("[espselect(t%d,l%d,cmd:%x)] ", target, lun, *(u_char *)cmd));

	/* new state ESP_SELECTING */
	sc->sc_state = ESP_SELECTING;

	ESPCMD(sc, ESPCMD_FLUSH);

	/*
	 * The docs say the target register is never reset, and I
	 * can't think of a better place to set it
	 */
	ESP_WRITE_REG(sc, ESP_SELID, target);
	if (ti->flags & T_SYNCMODE) {
		ESP_WRITE_REG(sc, ESP_SYNCOFF, ti->offset);
		ESP_WRITE_REG(sc, ESP_SYNCTP, esp_stp2cpb(sc, ti->period));
	} else {
		ESP_WRITE_REG(sc, ESP_SYNCOFF, 0);
		ESP_WRITE_REG(sc, ESP_SYNCTP, 0);
	}

	/*
	 * Who am I. This is where we tell the target that we are
	 * happy for it to disconnect etc.
	 */
	ESP_WRITE_REG(sc, ESP_FIFO,
		MSG_IDENTIFY(lun, (ti->flags & T_RSELECTOFF)?0:1));

	if (ti->flags & T_NEGOTIATE) {
		/* Arbitrate, select and stop after IDENTIFY message */
		ESPCMD(sc, ESPCMD_SELATNS);
		return;
	}

	/* Now the command into the FIFO */
	for (i = 0; i < clen; i++)
		ESP_WRITE_REG(sc, ESP_FIFO, *cmd++);

	/* And get the targets attention */
	ESPCMD(sc, ESPCMD_SELATN);
}

/*
 * DRIVER FUNCTIONS CALLABLE FROM HIGHER LEVEL DRIVERS
 */

/*
 * Start a SCSI-command
 * This function is called by the higher level SCSI-driver to queue/run
 * SCSI-commands.
 */
int
esp_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct esp_softc *sc = sc_link->adapter_softc;
	struct ecb 	*ecb;
	int s, flags;

	ESP_TRACE(("[esp_scsi_cmd] "));
	ESP_CMDS(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen,
	    sc_link->target));

	flags = xs->flags;

	/* Get a esp command block */
	s = splbio();
	ecb = TAILQ_FIRST(&sc->free_list);
	if (ecb) {
		TAILQ_REMOVE(&sc->free_list, ecb, chain);
		ECB_SETQ(ecb, ECB_QNONE);
	}
	splx(s);

	if (ecb == NULL) {
		ESP_MISC(("TRY_AGAIN_LATER"));
		return TRY_AGAIN_LATER;
	}

	/* Initialize ecb */
	ecb->xs = xs;
	bcopy(xs->cmd, &ecb->cmd, xs->cmdlen);
	ecb->clen = xs->cmdlen;
	ecb->daddr = xs->data;
	ecb->dleft = xs->datalen;
	ecb->stat = 0;

	s = splbio();
	TAILQ_INSERT_TAIL(&sc->ready_list, ecb, chain);
	ECB_SETQ(ecb, ECB_QREADY);
	ecb->timeout_ch = timeout(esp_timeout, ecb, (xs->timeout*hz)/1000);

	if (sc->sc_state == ESP_IDLE)
		esp_sched(sc);

	splx(s);

	if (flags & SCSI_NOMASK) {
		/* Not allowed to use interrupts, use polling instead */
		return esp_poll(sc, ecb);
	}

	ESP_MISC(("SUCCESSFULLY_QUEUED"));
	return SUCCESSFULLY_QUEUED;
}

/*
 * Used when interrupt driven I/O isn't allowed, e.g. during boot.
 */
int
esp_poll(sc, ecb)
	struct esp_softc *sc;
	struct ecb *ecb;
{
	struct scsi_xfer *xs = ecb->xs;
	int count = xs->timeout * 100;

	ESP_TRACE(("[esp_poll] "));
	while (count) {
		if (DMA_ISINTR(sc->sc_dma)) {
			espintr(sc);
		}
#if alternatively
		if (ESP_READ_REG(sc, ESP_STAT) & ESPSTAT_INT)
			espintr(sc);
#endif
		if (xs->flags & ITSDONE)
			break;
		DELAY(10);
		if (sc->sc_state == ESP_IDLE) {
			ESP_TRACE(("[esp_poll: rescheduling] "));
			esp_sched(sc);
		}
		count--;
	}

	if (count == 0) {
		ESP_MISC(("esp_poll: timeout"));
		esp_timeout((caddr_t)ecb);
	}

	return COMPLETE;
}


/*
 * LOW LEVEL SCSI UTILITIES
 */

/*
 * Schedule a scsi operation.  This has now been pulled out of the interrupt
 * handler so that we may call it from esp_scsi_cmd and esp_done.  This may
 * save us an unecessary interrupt just to get things going.  Should only be
 * called when state == ESP_IDLE and at bio pl.
 */
void
esp_sched(sc)
	struct esp_softc *sc;
{
	struct scsi_link *sc_link;
	struct ecb *ecb;
	int t;

	ESP_TRACE(("[esp_sched] "));
	if (sc->sc_state != ESP_IDLE)
		panic("esp_sched: not IDLE (state=%d)", sc->sc_state);

	if (sc->sc_flags & ESP_ABORTING)
		return;

	/*
	 * Find first ecb in ready queue that is for a target/lunit
	 * combinations that is not busy.
	 */
	TAILQ_FOREACH(ecb, &sc->ready_list, chain) {
		sc_link = ecb->xs->sc_link;
		t = sc_link->target;
		if (!(sc->sc_tinfo[t].lubusy & (1 << sc_link->lun))) {
			struct esp_tinfo *ti = &sc->sc_tinfo[t];

			if ((ecb->flags & ECB_QBITS) != ECB_QREADY)
				panic("esp: busy entry on ready list");
			TAILQ_REMOVE(&sc->ready_list, ecb, chain);
			ECB_SETQ(ecb, ECB_QNONE);
			sc->sc_nexus = ecb;
			sc->sc_flags = 0;
			sc->sc_prevphase = INVALID_PHASE;
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			ti->lubusy |= (1<<sc_link->lun);
/*XXX*/if (sc->sc_msgpriq) {
	printf("esp: message queue not empty: %x!\n", sc->sc_msgpriq);
}
/*XXX*/sc->sc_msgpriq = sc->sc_msgout = 0;
			espselect(sc, t, sc_link->lun,
				  (u_char *)&ecb->cmd, ecb->clen);
			break;
		} else
			ESP_MISC(("%d:%d busy\n", t, sc_link->lun));
	}
}

/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
void
esp_done(ecb)
	struct ecb *ecb;
{
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct esp_softc *sc = sc_link->adapter_softc;
	struct esp_tinfo *ti = &sc->sc_tinfo[sc_link->target];

	ESP_TRACE(("[esp_done(error:%x)] ", xs->error));

	untimeout(esp_timeout, ecb, ecb->timeout_ch);

	/*
	 * Now, if we've come here with no error code, i.e. we've kept the
	 * initial XS_NOERROR, and the status code signals that we should
	 * check sense, we'll need to set up a request sense cmd block and
	 * push the command back into the ready queue *before* any other
	 * commands for this target/lunit, else we lose the sense info.
	 * We don't support chk sense conditions for the request sense cmd.
	 */
	if (xs->error == XS_NOERROR) {
		if ((ecb->flags & ECB_ABORTED) != 0) {
			xs->error = XS_TIMEOUT;
		} else if ((ecb->flags & ECB_CHKSENSE) != 0) {
			xs->error = XS_SENSE;
		} else if ((ecb->stat & ST_MASK) == SCSI_CHECK) {
			struct scsi_sense *ss = (void *)&ecb->cmd;
			ESP_MISC(("requesting sense "));
			/* First, save the return values */
			xs->resid = ecb->dleft;
			xs->status = ecb->stat;
			/* Next, setup a request sense command block */
			bzero(ss, sizeof(*ss));
			ss->op_code = REQUEST_SENSE;
			/*ss->byte2 = sc_link->lun << 5;*/
			ss->length = sizeof(struct scsi_sense_data);
			ecb->clen = sizeof(*ss);
			ecb->daddr = (char *)&xs->sense;
			ecb->dleft = sizeof(struct scsi_sense_data);
			ecb->flags |= ECB_CHKSENSE;
/*XXX - must take off queue here */
			if (ecb != sc->sc_nexus) {
				panic("%s: esp_sched: floating ecb %p",
					device_get_nameunit(sc->sc_dev), ecb);
			}
			TAILQ_INSERT_HEAD(&sc->ready_list, ecb, chain);
			ECB_SETQ(ecb, ECB_QREADY);
			ti->lubusy &= ~(1<<sc_link->lun);
			ti->senses++;
			timeout(esp_timeout, ecb, (xs->timeout*hz)/1000);
			if (sc->sc_nexus == ecb) {
				sc->sc_nexus = NULL;
				sc->sc_state = ESP_IDLE;
				esp_sched(sc);
			}
			return;
		} else {
			xs->resid = ecb->dleft;
		}
	}

	xs->flags |= ITSDONE;

#ifdef ESP_DEBUG
	if (esp_debug & ESP_SHOWMISC) {
		printf("err=0x%02x ",xs->error);
		if (xs->error == XS_SENSE) {
			printf("sense=%2x; ", xs->sense.error_code);
		}
	}
	if ((xs->resid || xs->error > XS_SENSE) && esp_debug & ESP_SHOWMISC) {
		if (xs->resid)
			printf("esp_done: resid=%d\n", xs->resid);
		if (xs->error)
			printf("esp_done: error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ECB from whatever queue it's on.
	 */
	switch (ecb->flags & ECB_QBITS) {
	case ECB_QNONE:
		if (ecb != sc->sc_nexus) {
			panic("%s: floating ecb",
				device_get_nameunit(sc->sc_dev));
		}
		sc->sc_nexus = NULL;
		sc->sc_state = ESP_IDLE;
		ti->lubusy &= ~(1<<sc_link->lun);
		esp_sched(sc);
		break;
	case ECB_QREADY:
		TAILQ_REMOVE(&sc->ready_list, ecb, chain);
		break;
	case ECB_QNEXUS:
		TAILQ_REMOVE(&sc->nexus_list, ecb, chain);
		ti->lubusy &= ~(1<<sc_link->lun);
		break;
	case ECB_QFREE:
		panic("%s: dequeue: busy ecb on free list",
			device_get_nameunit(sc->sc_dev));
		break;
	default:
		panic("%s: dequeue: unknown queue %d",
			device_get_nameunit(sc->sc_dev),ecb->flags & ECB_QBITS);
	}

	/* Put it on the free list, and clear flags. */
	TAILQ_INSERT_HEAD(&sc->free_list, ecb, chain);
	ecb->flags = ECB_QFREE;

	ti->cmds++;
	scsi_done(xs);
	return;
}

/*
 * INTERRUPT/PROTOCOL ENGINE
 */

/*
 * Schedule an outgoing message by prioritizing it, and asserting
 * attention on the bus. We can only do this when we are the initiator
 * else there will be an illegal command interrupt.
 */
#define esp_sched_msgout(m) \
	do {				\
		ESP_MISC(("esp_sched_msgout %d ", m)); \
		ESPCMD(sc, ESPCMD_SETATN);	\
		sc->sc_msgpriq |= (m);	\
	} while (0)

#define IS1BYTEMSG(m) (((m) != 1 && (m) < 0x20) || (m) & 0x80)
#define IS2BYTEMSG(m) (((m) & 0xf0) == 0x20)
#define ISEXTMSG(m) ((m) == 1)

/*
 * Get an incoming message as initiator.
 *
 * The SCSI bus must already be in MESSAGE_IN_PHASE and there is a
 * byte in the FIFO
 */
void
esp_msgin(sc)
	register struct esp_softc *sc;
{
	register int v;

	ESP_TRACE(("[esp_msgin(curmsglen:%d)] ", sc->sc_imlen));

	if ((ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) == 0) {
		printf("%s: msgin: no msg byte available\n",
			device_get_nameunit(sc->sc_dev));
		return;
	}

	/*
	 * Prepare for a new message.  A message should (according
	 * to the SCSI standard) be transmitted in one single
	 * MESSAGE_IN_PHASE. If we have been in some other phase,
	 * then this is a new message.
	 */
	if (sc->sc_prevphase != MESSAGE_IN_PHASE) {
		sc->sc_flags &= ~ESP_DROP_MSGI;
		sc->sc_imlen = 0;
	}

	v = ESP_READ_REG(sc, ESP_FIFO);
	ESP_MISC(("<msgbyte:0x%02x>", v));

#if 0
	if (sc->sc_state == ESP_RESELECTED && sc->sc_imlen == 0) {
		/*
		 * Which target is reselecting us? (The ID bit really)
		 */
		sc->sc_selid = v;
		sc->sc_selid &= ~(1<<sc->sc_id);
		ESP_MISC(("selid=0x%2x ", sc->sc_selid));
		return;
	}
#endif

	sc->sc_imess[sc->sc_imlen] = v;

	/*
	 * If we're going to reject the message, don't bother storing
	 * the incoming bytes.  But still, we need to ACK them.
	 */

	if ((sc->sc_flags & ESP_DROP_MSGI)) {
		ESPCMD(sc, ESPCMD_SETATN);
		ESPCMD(sc, ESPCMD_MSGOK);
		printf("<dropping msg byte %x>",
			sc->sc_imess[sc->sc_imlen]);
		return;
	}

	if (sc->sc_imlen >= ESP_MAX_MSG_LEN) {
		esp_sched_msgout(SEND_REJECT);
		sc->sc_flags |= ESP_DROP_MSGI;
	} else {
		sc->sc_imlen++;
		/*
		 * This testing is suboptimal, but most
		 * messages will be of the one byte variety, so
		 * it should not effect performance
		 * significantly.
		 */
		if (sc->sc_imlen == 1 && IS1BYTEMSG(sc->sc_imess[0]))
			goto gotit;
		if (sc->sc_imlen == 2 && IS2BYTEMSG(sc->sc_imess[0]))
			goto gotit;
		if (sc->sc_imlen >= 3 && ISEXTMSG(sc->sc_imess[0]) &&
		    sc->sc_imlen == sc->sc_imess[1] + 2)
			goto gotit;
	}
	/* Ack what we have so far */
	ESPCMD(sc, ESPCMD_MSGOK);
	return;

gotit:
	ESP_MSGS(("gotmsg(%x)", sc->sc_imess[0]));
	/*
	 * Now we should have a complete message (1 byte, 2 byte
	 * and moderately long extended messages).  We only handle
	 * extended messages which total length is shorter than
	 * ESP_MAX_MSG_LEN.  Longer messages will be amputated.
	 */
	if (sc->sc_state == ESP_HASNEXUS) {
		struct ecb *ecb = sc->sc_nexus;
		struct esp_tinfo *ti = &sc->sc_tinfo[ecb->xs->sc_link->target];

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
			ESP_MSGS(("cmdcomplete "));
			if (sc->sc_dleft < 0) {
				struct scsi_link *sc_link = ecb->xs->sc_link;
				printf("esp: %d extra bytes from %d:%d\n",
					-sc->sc_dleft,
					sc_link->target, sc_link->lun);
				sc->sc_dleft = 0;
			}
			ecb->xs->resid = ecb->dleft = sc->sc_dleft;
			sc->sc_flags |= ESP_BUSFREE_OK;
			break;

		case MSG_MESSAGE_REJECT:
			if (esp_debug & ESP_SHOWMSGS)
				printf("%s: our msg rejected by target\n",
				    device_get_nameunit(sc->sc_dev));
#if 1 /* XXX - must remember last message */
sc_print_addr(ecb->xs->sc_link); printf("MSG_MESSAGE_REJECT>>");
#endif
			if (sc->sc_flags & ESP_SYNCHNEGO) {
				ti->period = ti->offset = 0;
				sc->sc_flags &= ~ESP_SYNCHNEGO;
				ti->flags &= ~T_NEGOTIATE;
			}
			/* Not all targets understand INITIATOR_DETECTED_ERR */
			if (sc->sc_msgout == SEND_INIT_DET_ERR)
				esp_sched_msgout(SEND_ABORT);
			break;
		case MSG_NOOP:
			ESP_MSGS(("noop "));
			break;
		case MSG_DISCONNECT:
			ESP_MSGS(("disconnect "));
			ti->dconns++;
			sc->sc_flags |= ESP_DISCON;
			sc->sc_flags |= ESP_BUSFREE_OK;
#define	SDEV_AUTOSAVE	(0x01)
			if ((ecb->xs->sc_link->quirks & SDEV_AUTOSAVE) == 0)
				break;
			/*FALLTHROUGH*/
		case MSG_SAVEDATAPOINTER:
			ESP_MSGS(("save datapointer "));
			ecb->dleft = sc->sc_dleft;
			ecb->daddr = sc->sc_dp;
			break;
		case MSG_RESTOREPOINTERS:
			ESP_MSGS(("restore datapointer "));
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				printf("%s: no DATAPOINTERs to restore\n",
				    device_get_nameunit(sc->sc_dev));
				break;
			}
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			break;
		case MSG_PARITY_ERROR:
			printf("%s:target%d: MSG_PARITY_ERROR\n",
				device_get_nameunit(sc->sc_dev),
				ecb->xs->sc_link->target);
			break;
		case MSG_EXTENDED:
			ESP_MSGS(("extended(%x) ", sc->sc_imess[2]));
			switch (sc->sc_imess[2]) {
			case MSG_EXT_SDTR:
				ESP_MSGS(("SDTR period %d, offset %d ",
					sc->sc_imess[3], sc->sc_imess[4]));
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				if (sc->sc_minsync == 0) {
					/* We won't do synch */
					ti->offset = 0;
					esp_sched_msgout(SEND_SDTR);
				} else if (ti->offset == 0) {
					printf("%s:%d: async\n", "esp",
						ecb->xs->sc_link->target);
					ti->offset = 0;
					sc->sc_flags &= ~ESP_SYNCHNEGO;
				} else if (ti->period > 124) {
					printf("%s:%d: async\n", "esp",
						ecb->xs->sc_link->target);
					ti->offset = 0;
					esp_sched_msgout(SEND_SDTR);
				} else {
					int r = 250/ti->period;
					int s = (100*250)/ti->period - 100*r;
					int p;
					p =  esp_stp2cpb(sc, ti->period);
					ti->period = esp_cpb2stp(sc, p);
#ifdef ESP_DEBUG
					sc_print_addr(ecb->xs->sc_link);
#endif
					if ((sc->sc_flags&ESP_SYNCHNEGO) == 0) {
						/* Target initiated negotiation */
						if (ti->flags & T_SYNCMODE) {
						    ti->flags &= ~T_SYNCMODE;
#ifdef ESP_DEBUG
						    printf("renegotiated ");
#endif
						}
						ESP_WRITE_REG(sc, ESP_SYNCOFF,
							      0);
						ESP_WRITE_REG(sc, ESP_SYNCTP,
							      0);
						/* Clamp to our maxima */
						if (ti->period < sc->sc_minsync)
							ti->period = sc->sc_minsync;
						if (ti->offset > 15)
							ti->offset = 15;
						esp_sched_msgout(SEND_SDTR);
					} else {
						/* we are sync */
						sc->sc_flags &= ~ESP_SYNCHNEGO;
						ESP_WRITE_REG(sc, ESP_SYNCOFF,
							      ti->offset);
						ESP_WRITE_REG(sc, ESP_SYNCTP,
							      p);
						ti->flags |= T_SYNCMODE;
					}
#ifdef ESP_DEBUG
					printf("max sync rate %d.%02dMb/s\n",
						r, s);
#endif
				}
				ti->flags &= ~T_NEGOTIATE;
				break;
			default: /* Extended messages we don't handle */
				ESPCMD(sc, ESPCMD_SETATN);
				break;
			}
			break;
		default:
			ESP_MSGS(("ident "));
			/* thanks for that ident... */
			if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
				ESP_MISC(("unknown "));
printf("%s: unimplemented message: %d\n", device_get_nameunit(sc->sc_dev), sc->sc_imess[0]);
				ESPCMD(sc, ESPCMD_SETATN);
			}
			break;
		}
	} else if (sc->sc_state == ESP_RESELECTED) {
		struct scsi_link *sc_link = NULL;
		struct ecb *ecb;
		struct esp_tinfo *ti;
		u_char lunit;

		if (MSG_ISIDENTIFY(sc->sc_imess[0])) { 	/* Identify? */
			ESP_MISC(("searching "));
			/*
			 * Search wait queue for disconnected cmd
			 * The list should be short, so I haven't bothered with
			 * any more sophisticated structures than a simple
			 * singly linked list.
			 */
			lunit = sc->sc_imess[0] & 0x07;
			TAILQ_FOREACH(ecb, &sc->nexus_list, chain) {
				sc_link = ecb->xs->sc_link;
				if (sc_link->lun == lunit &&
				    sc->sc_selid == (1<<sc_link->target)) {
					TAILQ_REMOVE(&sc->nexus_list, ecb,
					    chain);
					ECB_SETQ(ecb, ECB_QNONE);
					break;
				}
			}

			if (!ecb) {		/* Invalid reselection! */
				esp_sched_msgout(SEND_ABORT);
				printf("esp: invalid reselect (idbit=0x%2x)\n",
				    sc->sc_selid);
			} else {		/* Reestablish nexus */
				/*
				 * Setup driver data structures and
				 * do an implicit RESTORE POINTERS
				 */
				ti = &sc->sc_tinfo[sc_link->target];
				sc->sc_nexus = ecb;
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;
				sc->sc_tinfo[sc_link->target].lubusy
					|= (1<<sc_link->lun);
				if (ti->flags & T_SYNCMODE) {
					ESP_WRITE_REG(sc, ESP_SYNCOFF,
						ti->offset);
					ESP_WRITE_REG(sc, ESP_SYNCTP,
						esp_stp2cpb(sc, ti->period));
				} else {
					ESP_WRITE_REG(sc, ESP_SYNCOFF, 0);
					ESP_WRITE_REG(sc, ESP_SYNCTP, 0);
				}
				ESP_MISC(("... found ecb"));
				sc->sc_state = ESP_HASNEXUS;
			}
		} else {
			printf("%s: bogus reselect (no IDENTIFY) %0x2x\n",
			    device_get_nameunit(sc->sc_dev), sc->sc_selid);
			esp_sched_msgout(SEND_DEV_RESET);
		}
	} else { /* Neither ESP_HASNEXUS nor ESP_RESELECTED! */
		printf("%s: unexpected message in; will send DEV_RESET\n",
		    device_get_nameunit(sc->sc_dev));
		esp_sched_msgout(SEND_DEV_RESET);
	}

	/* Ack last message byte */
	ESPCMD(sc, ESPCMD_MSGOK);

	/* Done, reset message pointer. */
	sc->sc_flags &= ~ESP_DROP_MSGI;
	sc->sc_imlen = 0;
}


/*
 * Send the highest priority, scheduled message
 */
void
esp_msgout(sc)
	register struct esp_softc *sc;
{
	struct esp_tinfo *ti;
	struct ecb *ecb;
	size_t size;

	ESP_TRACE(("[esp_msgout(priq:%x, prevphase:%x)]", sc->sc_msgpriq, sc->sc_prevphase));

	if (sc->sc_prevphase != MESSAGE_OUT_PHASE) {
		/* Pick up highest priority message */
		sc->sc_msgout = sc->sc_msgpriq & -sc->sc_msgpriq;
		sc->sc_omlen = 1;		/* "Default" message len */
		switch (sc->sc_msgout) {
		case SEND_SDTR:
			ecb = sc->sc_nexus;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			sc->sc_omess[0] = MSG_EXTENDED;
			sc->sc_omess[1] = 3;
			sc->sc_omess[2] = MSG_EXT_SDTR;
			sc->sc_omess[3] = ti->period;
			sc->sc_omess[4] = ti->offset;
			sc->sc_omlen = 5;
			break;
		case SEND_IDENTIFY:
			if (sc->sc_state != ESP_HASNEXUS) {
				printf("esp at line %d: no nexus", __LINE__);
			}
			ecb = sc->sc_nexus;
			sc->sc_omess[0] = MSG_IDENTIFY(ecb->xs->sc_link->lun,0);
			break;
		case SEND_DEV_RESET:
			sc->sc_omess[0] = MSG_BUS_DEV_RESET;
			sc->sc_flags |= ESP_BUSFREE_OK;
			ecb = sc->sc_nexus;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			ti->flags &= ~T_SYNCMODE;
			ti->flags |= T_NEGOTIATE;
			break;
		case SEND_PARITY_ERROR:
			sc->sc_omess[0] = MSG_PARITY_ERROR;
			break;
		case SEND_ABORT:
			sc->sc_omess[0] = MSG_ABORT;
			sc->sc_flags |= ESP_BUSFREE_OK;
			break;
		case SEND_INIT_DET_ERR:
			sc->sc_omess[0] = MSG_INITIATOR_DET_ERR;
			break;
		case SEND_REJECT:
			sc->sc_omess[0] = MSG_MESSAGE_REJECT;
			break;
		default:
			sc->sc_omess[0] = MSG_NOOP;
			break;
		}
		sc->sc_omp = sc->sc_omess;
	}

#if 1
	/* (re)send the message */
	size = min(sc->sc_omlen, sc->sc_maxxfer);
	DMA_SETUP(sc->sc_dma, &sc->sc_omp, &sc->sc_omlen, 0, &size);
	/* Program the SCSI counter */
	ESP_WRITE_REG(sc, ESP_TCL, size);
	ESP_WRITE_REG(sc, ESP_TCM, size >> 8);
	if (sc->sc_cfg2 & ESPCFG2_FE) {
		ESP_WRITE_REG(sc, ESP_TCH, size >> 16);
	}
	/* load the count in */
	ESPCMD(sc, ESPCMD_NOP|ESPCMD_DMA);
	ESPCMD(sc, (size==0?ESPCMD_TRPAD:ESPCMD_TRANS)|ESPCMD_DMA);
	DMA_GO(sc->sc_dma);
#else
	{	int i;
		ESPCMD(sc, ESPCMD_FLUSH);
		for (i = 0; i < sc->sc_omlen; i++)
			ESP_WRITE_REG(sc, FIFO, sc->sc_omess[i]);
		ESPCMD(sc, ESPCMD_TRANS);
#if test_stuck_on_msgout
printf("<<XXXmsgoutdoneXXX>>");
#endif
	}
#endif
}

/*
 * This is the most critical part of the driver, and has to know
 * how to deal with *all* error conditions and phases from the SCSI
 * bus. If there are no errors and the DMA was active, then call the
 * DMA pseudo-interrupt handler. If this returns 1, then that was it
 * and we can return from here without further processing.
 *
 * Most of this needs verifying.
 */
int
espintr(sc)
	register struct esp_softc *sc;
{
	register struct ecb *ecb;
	register struct scsi_link *sc_link;
	struct esp_tinfo *ti;
	int loop;
	size_t size;

	ESP_TRACE(("[espintr]"));

	/*
	 * I have made some (maybe seriously flawed) assumptions here,
	 * but basic testing (uncomment the printf() below), show that
	 * certainly something happens when this loop is here.
	 *
	 * The idea is that many of the SCSI operations take very little
	 * time, and going away and getting interrupted is too high an
	 * overhead to pay. For example, selecting, sending a message
	 * and command and then doing some work can be done in one "pass".
	 *
	 * The DELAY is not variable because I do not understand that the
	 * DELAY loop should be fixed-time regardless of CPU speed, but
	 * I am *assuming* that the faster SCSI processors get things done
	 * quicker (sending a command byte etc), and so there is no
	 * need to be too slow.
	 *
	 * This is a heuristic. It is 2 when at 20Mhz, 2 at 25Mhz and 1
	 * at 40Mhz. This needs testing.
	 */
	for (loop = 0; 1;loop++, DELAY(50/sc->sc_freq)) {
		/* a feeling of deja-vu */
		if (!DMA_ISINTR(sc->sc_dma))
			return (loop != 0);
#if 0
		if (loop)
			printf("*");
#endif

		/* and what do the registers say... */
		espreadregs(sc);

		sc->sc_intrcnt.ev_count++;

		/*
		 * At the moment, only a SCSI Bus Reset or Illegal
		 * Command are classed as errors. A disconnect is a
		 * valid condition, and we let the code check is the
		 * "ESP_BUSFREE_OK" flag was set before declaring it
		 * and error.
		 *
		 * Also, the status register tells us about "Gross
		 * Errors" and "Parity errors". Only the Gross Error
		 * is really bad, and the parity errors are dealt
		 * with later
		 *
		 * TODO
		 *	If there are too many parity error, go to slow
		 *	cable mode ?
		 */

		/* SCSI Reset */
		if (sc->sc_espintr & ESPINTR_SBR) {
			if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			if (sc->sc_state != ESP_SBR) {
				printf("%s: SCSI bus reset\n",
					device_get_nameunit(sc->sc_dev));
				esp_init(sc, 0); /* Restart everything */
				return 1;
			}
#if 0
	/*XXX*/		printf("<expected bus reset: "
				"[intr %x, stat %x, step %d]>\n",
				sc->sc_espintr, sc->sc_espstat,
				sc->sc_espstep);
#endif
			if (sc->sc_nexus)
				panic("%s: nexus in reset state",
				      device_get_nameunit(sc->sc_dev));
			sc->sc_state = ESP_IDLE;
			esp_sched(sc);
			return 1;
		}

		ecb = sc->sc_nexus;

#define ESPINTR_ERR (ESPINTR_SBR|ESPINTR_ILL)
		if (sc->sc_espintr & ESPINTR_ERR ||
		    sc->sc_espstat & ESPSTAT_GE) {

			if (sc->sc_espstat & ESPSTAT_GE) {
				/* no target ? */
				if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
					ESPCMD(sc, ESPCMD_FLUSH);
					DELAY(1);
				}
				if (sc->sc_state == ESP_HASNEXUS ||
				    sc->sc_state == ESP_SELECTING) {
					ecb->xs->error = XS_DRIVER_STUFFUP;
					esp_done(ecb);
				}
				return 1;
			}

			if (sc->sc_espintr & ESPINTR_ILL) {
				/* illegal command, out of sync ? */
				printf("%s: illegal command: 0x%x (state %d, phase %x, prevphase %x)\n",
					device_get_nameunit(sc->sc_dev), sc->sc_lastcmd,
					sc->sc_state, sc->sc_phase,
					sc->sc_prevphase);
				if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
					ESPCMD(sc, ESPCMD_FLUSH);
					DELAY(1);
				}
				esp_init(sc, 0); /* Restart everything */
				return 1;
			}
		}

		/*
		 * Call if DMA is active.
		 *
		 * If DMA_INTR returns true, then maybe go 'round the loop
		 * again in case there is no more DMA queued, but a phase
		 * change is expected.
		 */
		if (DMA_ISACTIVE(sc->sc_dma)) {
			DMA_INTR(sc->sc_dma);
			/* If DMA active here, then go back to work... */
			if (DMA_ISACTIVE(sc->sc_dma))
				return 1;

			if (sc->sc_dleft == 0 &&
			    (sc->sc_espstat & ESPSTAT_TC) == 0)
				printf("%s: !TC [intr %x, stat %x, step %d]"
				       " prevphase %x, resid %x\n",
					device_get_nameunit(sc->sc_dev),
					sc->sc_espintr,
					sc->sc_espstat,
					sc->sc_espstep,
					sc->sc_prevphase,
					ecb?ecb->dleft:-1);
		}

#if 0	/* Unreliable on some ESP revisions? */
		if ((sc->sc_espstat & ESPSTAT_INT) == 0) {
			printf("%s: spurious interrupt\n", device_get_nameunit(sc->sc_dev));
			return 1;
		}
#endif

		/*
		 * check for less serious errors
		 */
		if (sc->sc_espstat & ESPSTAT_PE) {
			printf("%s: SCSI bus parity error\n",
				device_get_nameunit(sc->sc_dev));
			if (sc->sc_prevphase == MESSAGE_IN_PHASE)
				esp_sched_msgout(SEND_PARITY_ERROR);
			else
				esp_sched_msgout(SEND_INIT_DET_ERR);
		}

		if (sc->sc_espintr & ESPINTR_DIS) {
			ESP_MISC(("<DISC [intr %x, stat %x, step %d]>",
				sc->sc_espintr,sc->sc_espstat,sc->sc_espstep));
			if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			/*
			 * This command must (apparently) be issued within
			 * 250mS of a disconnect. So here you are...
			 */
			ESPCMD(sc, ESPCMD_ENSEL);
			if (sc->sc_state != ESP_IDLE) {
				if ((sc->sc_flags & ESP_SYNCHNEGO)) {
#ifdef ESP_DEBUG
					if (ecb)
						sc_print_addr(ecb->xs->sc_link);
					printf("sync nego not completed!\n");
#endif
					sc->sc_flags &= ~ESP_SYNCHNEGO;
					sc->sc_tinfo[ecb->xs->sc_link->target].offset = 0;
					sc->sc_tinfo[ecb->xs->sc_link->target].flags &= ~T_NEGOTIATE;
				}
			/*XXX*/sc->sc_msgpriq = sc->sc_msgout = 0;

				/* it may be OK to disconnect */
				if (!(sc->sc_flags & ESP_BUSFREE_OK)) {
					if (sc->sc_state == ESP_HASNEXUS) {
						sc_print_addr(ecb->xs->sc_link);
						printf("disconnect without"
							"warning\n");
					}
					ecb->xs->error = XS_TIMEOUT;
				} else if (sc->sc_flags & ESP_DISCON) {
					TAILQ_INSERT_HEAD(&sc->nexus_list, ecb, chain);
					ECB_SETQ(ecb, ECB_QNEXUS);
					sc->sc_nexus = NULL;
					sc->sc_flags &= ~ESP_DISCON;
					sc->sc_state = ESP_IDLE;
#if ESP_DEBUG
if ((esp_debug & 0x10000) && ecb->dleft == 0) {
	printf("%s: silly disconnect (ecb %p [stat %x])\n",
		device_get_nameunit(sc->sc_dev), ecb, ecb->stat);
}
#endif
					esp_sched(sc);
					return 1;
				}

				esp_done(ecb);
				return 1;
			}
			printf("%s: DISCONNECT in IDLE state!\n",
				device_get_nameunit(sc->sc_dev));
		}

		/* did a message go out OK ? This must be broken */
		if (sc->sc_prevphase == MESSAGE_OUT_PHASE &&
		    sc->sc_phase != MESSAGE_OUT_PHASE) {
			/* we have sent it */
			if (sc->sc_msgout == SEND_SDTR &&
			    (sc->sc_flags & ESP_SYNCHNEGO) == 0) {
				/* We've just accepted new sync parameters */
				sc->sc_tinfo[ecb->xs->sc_link->target].flags |=
					T_SYNCMODE;
if (ecb) sc_print_addr(ecb->xs->sc_link);else printf("NO nexus: ");
printf("target put in SYNC mode\n");
			}
			sc->sc_msgpriq &= ~sc->sc_msgout;
			sc->sc_msgout = 0;
		}

		switch (sc->sc_state) {

		case ESP_SBR:
			printf("%s: waiting for SCSI Bus Reset to happen\n",
				device_get_nameunit(sc->sc_dev));
			return 1;

		case ESP_RESELECTED:
			/*
			 * we must be continuing a message ?
			 */
			if (sc->sc_phase != MESSAGE_IN_PHASE) {
				printf("%s: target didn't identify\n",
					device_get_nameunit(sc->sc_dev));
				esp_init(sc, 1);
				return 1;
			}
printf("<<RESELECT CONT'd>>");
#if XXXX
			esp_msgin(sc);
			if (sc->sc_state != ESP_HASNEXUS) {
				/* IDENTIFY fail?! */
				printf("%s: identify failed\n",
					device_get_nameunit(sc->sc_dev));
				esp_init(sc, 1);
				return 1;
			}
#endif
			break;

		case ESP_IDLE:
if (sc->sc_flags & ESP_ICCS) printf("[[esp: BUMMER]]");
		case ESP_SELECTING:

			if (sc->sc_espintr & ESPINTR_RESEL) {
				/*
				 * If we're trying to select a
				 * target ourselves, push our command
				 * back into the ready list.
				 */
				if (sc->sc_state == ESP_SELECTING) {
					ESP_MISC(("backoff selector "));
					sc_link = sc->sc_nexus->xs->sc_link;
					ti = &sc->sc_tinfo[sc_link->target];
					TAILQ_INSERT_HEAD(&sc->ready_list,
					    sc->sc_nexus, chain);
					ECB_SETQ(sc->sc_nexus, ECB_QREADY);
					ti->lubusy &= ~(1<<sc_link->lun);
					ecb = sc->sc_nexus = NULL;
				}
				sc->sc_state = ESP_RESELECTED;
				if (sc->sc_phase != MESSAGE_IN_PHASE) {
					/*
					 * Things are seriously fucked up.
					 * Pull the brakes, i.e. reset
					 */
					printf("%s: target didn't identify\n",
						device_get_nameunit(sc->sc_dev));
					esp_init(sc, 1);
					return 1;
				}
				if ((ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) != 2) {
					printf("%s: RESELECT: %d bytes in FIFO!\n",
						device_get_nameunit(sc->sc_dev),
						ESP_READ_REG(sc, ESP_FFLAG) &
						ESPFIFO_FF);
					esp_init(sc, 1);
					return 1;
				}
				sc->sc_selid = ESP_READ_REG(sc, ESP_FIFO);
				sc->sc_selid &= ~(1<<sc->sc_id);
				ESP_MISC(("selid=0x%2x ", sc->sc_selid));
				esp_msgin(sc);	/* Handle identify message */
				if (sc->sc_state != ESP_HASNEXUS) {
					/* IDENTIFY fail?! */
					printf("%s: identify failed\n",
						device_get_nameunit(sc->sc_dev));
					esp_init(sc, 1);
					return 1;
				}
				continue; /* ie. next phase expected soon */
			}

#define	ESPINTR_DONE	(ESPINTR_FC|ESPINTR_BS)
			if ((sc->sc_espintr & ESPINTR_DONE) == ESPINTR_DONE) {
				ecb = sc->sc_nexus;
				if (!ecb)
					panic("esp: not nexus at sc->sc_nexus");

				sc_link = ecb->xs->sc_link;
				ti = &sc->sc_tinfo[sc_link->target];

				switch (sc->sc_espstep) {
				case 0:
					printf("%s: select timeout/no disconnect\n",
						device_get_nameunit(sc->sc_dev));
					esp_abort(sc, ecb);
					return 1;
				case 1:
					if ((ti->flags & T_NEGOTIATE) == 0) {
						printf("%s: step 1 & !NEG\n",
						device_get_nameunit(sc->sc_dev));
						esp_abort(sc, ecb);
						return 1;
					}
					if (sc->sc_phase != MESSAGE_OUT_PHASE) {
						printf("%s: !MSGOUT\n",
						device_get_nameunit(sc->sc_dev));
						esp_abort(sc, ecb);
						return 1;
					}
					/* Start negotiating */
					ti->period = sc->sc_minsync;
					ti->offset = 15;
					sc->sc_msgpriq = SEND_SDTR;
					sc->sc_flags |= ESP_SYNCHNEGO;
					break;
				case 3:
					/*
					 * Grr, this is supposed to mean
					 * "target left command phase
					 *  prematurely". It seems to happen
					 * regularly when sync mode is on.
					 * Look at FIFO to see if command
					 * went out.
					 * (Timing problems?)
					 */
					if ((ESP_READ_REG(sc, ESP_FFLAG)&ESPFIFO_FF) == 0) {
						/* Hope for the best.. */
						break;
					}
					printf("(%s:%d:%d): selection failed;"
						" %d left in FIFO "
						"[intr %x, stat %x, step %d]\n",
						device_get_nameunit(sc->sc_dev),
						sc_link->target,
						sc_link->lun,
						ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF,
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
					ESPCMD(sc, ESPCMD_FLUSH);
					sc->sc_flags |= ESP_ABORTING;
					esp_sched_msgout(SEND_ABORT);
					return 1;
				case 2:
					/* Select stuck at Command Phase */
					ESPCMD(sc, ESPCMD_FLUSH);
				case 4:
					/* So far, everything went fine */
					sc->sc_msgpriq = 0;
					break;
				}
#if 0
/* Why set msgpriq? (and not raise ATN) */
				if (ecb->xs->flags & SCSI_RESET)
					sc->sc_msgpriq = SEND_DEV_RESET;
				else if (ti->flags & T_NEGOTIATE)
					sc->sc_msgpriq =
					    SEND_IDENTIFY | SEND_SDTR;
				else
					sc->sc_msgpriq = SEND_IDENTIFY;
#endif
				sc->sc_state = ESP_HASNEXUS;
				/*???sc->sc_flags = 0; */
				sc->sc_prevphase = INVALID_PHASE; /* ?? */
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;
				ti->lubusy |= (1<<sc_link->lun);
				break;
			} else {
				printf("%s: unexpected status after select"
					": [intr %x, stat %x, step %x]\n",
					device_get_nameunit(sc->sc_dev),
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
				esp_abort(sc, ecb);
			}
			if (sc->sc_state == ESP_IDLE) {
				printf("%s: stray interrupt\n",
					device_get_nameunit(sc->sc_dev));
				return 0;
			}
			break;

		case ESP_HASNEXUS:
			if (sc->sc_flags & ESP_ICCS) {
				unsigned char msg;

				sc->sc_flags &= ~ESP_ICCS;

				if (!(sc->sc_espintr & ESPINTR_DONE)) {
					printf("%s: ICCS: "
					      ": [intr %x, stat %x, step %x]\n",
						device_get_nameunit(sc->sc_dev),
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
				}
				if ((ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) != 2) {
					printf("%s: ICCS: expected 2, got %d "
					      ": [intr %x, stat %x, step %x]\n",
						device_get_nameunit(sc->sc_dev),
						ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF,
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
					ESPCMD(sc, ESPCMD_FLUSH);
					esp_abort(sc, ecb);
					return 1;
				}
				ecb->stat = ESP_READ_REG(sc, ESP_FIFO);
				msg = ESP_READ_REG(sc, ESP_FIFO);
				ESP_PHASE(("<stat:(%x,%x)>", ecb->stat, msg));
				if (msg == MSG_CMDCOMPLETE) {
					sc->sc_flags |= ESP_BUSFREE_OK;
					ecb->xs->resid = ecb->dleft = sc->sc_dleft;
				} else
					printf("%s: STATUS_PHASE: msg %d\n",
					device_get_nameunit(sc->sc_dev), msg);
				ESPCMD(sc, ESPCMD_MSGOK);
				continue; /* ie. wait for disconnect */
			}
			break;
		default:
			panic("%s: invalid state: %d",
			      device_get_nameunit(sc->sc_dev), sc->sc_state);
		}

		/*
		 * Driver is now in state ESP_HASNEXUS, i.e. we
		 * have a current command working the SCSI bus.
		 */
		if (sc->sc_state != ESP_HASNEXUS || ecb == NULL) {
			panic("esp no nexus");
		}

		switch (sc->sc_phase) {
		case MESSAGE_OUT_PHASE:
			ESP_PHASE(("MESSAGE_OUT_PHASE "));
			esp_msgout(sc);
			sc->sc_prevphase = MESSAGE_OUT_PHASE;
			break;
		case MESSAGE_IN_PHASE:
			ESP_PHASE(("MESSAGE_IN_PHASE "));
			if (sc->sc_espintr & ESPINTR_BS) {
				ESPCMD(sc, ESPCMD_FLUSH);
				sc->sc_flags |= ESP_WAITI;
				ESPCMD(sc, ESPCMD_TRANS);
			} else if (sc->sc_espintr & ESPINTR_FC) {
				if ((sc->sc_flags & ESP_WAITI) == 0) {
					printf("%s: MSGIN: unexpected FC bit: "
						"[intr %x, stat %x, step %x]\n",
					device_get_nameunit(sc->sc_dev),
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
				}
				sc->sc_flags &= ~ESP_WAITI;
				esp_msgin(sc);
			} else {
				printf("%s: MSGIN: weird bits: "
					"[intr %x, stat %x, step %x]\n",
					device_get_nameunit(sc->sc_dev),
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
			}
			sc->sc_prevphase = MESSAGE_IN_PHASE;
			break;
		case COMMAND_PHASE: {
			/* well, this means send the command again */
			u_char *cmd = (u_char *)&ecb->cmd;
			int i;

			ESP_PHASE(("COMMAND_PHASE 0x%02x (%d) ",
				ecb->cmd.opcode, ecb->clen));
			if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			/* Now the command into the FIFO */
			for (i = 0; i < ecb->clen; i++)
				ESP_WRITE_REG(sc, ESP_FIFO, *cmd++);
			ESPCMD(sc, ESPCMD_TRANS);
			sc->sc_prevphase = COMMAND_PHASE;
			}
			break;
		case DATA_OUT_PHASE:
			ESP_PHASE(("DATA_OUT_PHASE [%d] ",  sc->sc_dleft));
			ESPCMD(sc, ESPCMD_FLUSH);
			size = min(sc->sc_dleft, sc->sc_maxxfer);
			DMA_SETUP(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft,
				  0, &size);
			sc->sc_prevphase = DATA_OUT_PHASE;
			goto setup_xfer;
		case DATA_IN_PHASE:
			ESP_PHASE(("DATA_IN_PHASE "));
			if (sc->sc_rev == ESP100)
				ESPCMD(sc, ESPCMD_FLUSH);
			size = min(sc->sc_dleft, sc->sc_maxxfer);
			DMA_SETUP(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft,
				  1, &size);
			sc->sc_prevphase = DATA_IN_PHASE;
		setup_xfer:
			/* Program the SCSI counter */
			ESP_WRITE_REG(sc, ESP_TCL, size);
			ESP_WRITE_REG(sc, ESP_TCM, size >> 8);
			if (sc->sc_cfg2 & ESPCFG2_FE) {
				ESP_WRITE_REG(sc, ESP_TCH, size >> 16);
			}
			/* load the count in */
			ESPCMD(sc, ESPCMD_NOP|ESPCMD_DMA);

			/*
			 * Note that if `size' is 0, we've already transceived
			 * all the bytes we want but we're still in DATA PHASE.
			 * Apparently, the device needs padding. Also, a
			 * transfer size of 0 means "maximum" to the chip
			 * DMA logic.
			 */
			ESPCMD(sc,
			       (size==0?ESPCMD_TRPAD:ESPCMD_TRANS)|ESPCMD_DMA);
			DMA_GO(sc->sc_dma);
			return 1;
		case STATUS_PHASE:
			ESP_PHASE(("STATUS_PHASE "));
			sc->sc_flags |= ESP_ICCS;
			ESPCMD(sc, ESPCMD_ICCS);
			sc->sc_prevphase = STATUS_PHASE;
			break;
		case INVALID_PHASE:
			break;
		case BUSFREE_PHASE:
			if (sc->sc_flags & ESP_BUSFREE_OK) {
				/*It's fun the 1st time.. */
				sc->sc_flags &= ~ESP_BUSFREE_OK;
			}
			break;
		default:
			panic("esp: bogus bus phase\n");
		}
	}
	panic("esp: should not get here..");
}

void
esp_abort(sc, ecb)
	struct esp_softc *sc;
	struct ecb *ecb;
{
	if (ecb == sc->sc_nexus) {
		if (sc->sc_state == ESP_HASNEXUS) {
			sc->sc_flags |= ESP_ABORTING;
			esp_sched_msgout(SEND_ABORT);
		}
	} else {
		if (sc->sc_state == ESP_IDLE)
			esp_sched(sc);
	}
}

void
esp_timeout(arg)
	void *arg;
{
	int s = splbio();
	struct ecb *ecb = (struct ecb *)arg;
	struct esp_softc *sc;
	struct scsi_xfer *xs = ecb->xs;

	sc = xs->sc_link->adapter_softc;
	sc_print_addr(xs->sc_link);
again:
	printf("%s: timed out [ecb %p (flags 0x%x, dleft %x, stat %x)], "
	       "<state %d, nexus %p, phase(c %x, p %x), resid %x, msg(q %x,o %x) %s>",
		device_get_nameunit(sc->sc_dev),
		ecb, ecb->flags, ecb->dleft, ecb->stat,
		sc->sc_state, sc->sc_nexus, sc->sc_phase, sc->sc_prevphase,
		sc->sc_dleft, sc->sc_msgpriq, sc->sc_msgout,
		DMA_ISACTIVE(sc->sc_dma) ? "DMA active" : "");
#if ESP_DEBUG > 0
	printf("TRACE: %s.", ecb->trace);
#endif

	if (ecb->flags & ECB_ABORTED) {
		/* abort timed out */
		printf(" AGAIN\n");
		esp_init(sc, 1);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		xs->error = XS_TIMEOUT;
		ecb->flags |= ECB_ABORTED;
		esp_abort(sc, ecb);
		/* 2 secs for the abort */
		if ((xs->flags & SCSI_NOMASK) == 0)
			timeout(esp_timeout, ecb, 2 * hz);
		else {
			int count = 200000;
			while (count) {
				if (DMA_ISINTR(sc->sc_dma)) {
					espintr(sc);
				}
				if (xs->flags & ITSDONE)
					break;
				DELAY(10);
				--count;
			}
			if (count == 0)
				goto again;
		}
	}
	splx(s);
}

static void 
esp_min_phys (struct  buf *bp)
{
	
}
