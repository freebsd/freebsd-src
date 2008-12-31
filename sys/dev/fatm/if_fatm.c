/*-
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * Fore PCA200E driver for NATM
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/fatm/if_fatm.c,v 1.23.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_inet.h"
#include "opt_natm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/endian.h>
#include <sys/sysctl.h>
#include <sys/condvar.h>
#include <vm/uma.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_atm.h>
#include <net/route.h>
#ifdef ENABLE_BPF
#include <net/bpf.h>
#endif
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_atm.h>
#endif

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/utopia/utopia.h>

#include <dev/fatm/if_fatmreg.h>
#include <dev/fatm/if_fatmvar.h>

#include <dev/fatm/firmware.h>

devclass_t fatm_devclass;

static const struct {
	uint16_t	vid;
	uint16_t	did;
	const char	*name;
} fatm_devs[] = {
	{ 0x1127, 0x300,
	  "FORE PCA200E" },
	{ 0, 0, NULL }
};

static const struct rate {
	uint32_t	ratio;
	uint32_t	cell_rate;
} rate_table[] = {
#include <dev/fatm/if_fatm_rate.h>
};
#define RATE_TABLE_SIZE (sizeof(rate_table) / sizeof(rate_table[0]))

SYSCTL_DECL(_hw_atm);

MODULE_DEPEND(fatm, utopia, 1, 1, 1);

static int	fatm_utopia_readregs(struct ifatm *, u_int, uint8_t *, u_int *);
static int	fatm_utopia_writereg(struct ifatm *, u_int, u_int, u_int);

static const struct utopia_methods fatm_utopia_methods = {
	fatm_utopia_readregs,
	fatm_utopia_writereg
};

#define VC_OK(SC, VPI, VCI)						\
	(((VPI) & ~((1 << IFP2IFATM((SC)->ifp)->mib.vpi_bits) - 1)) == 0 &&	\
	 (VCI) != 0 && ((VCI) & ~((1 << IFP2IFATM((SC)->ifp)->mib.vci_bits) - 1)) == 0)

static int fatm_load_vc(struct fatm_softc *sc, struct card_vcc *vc);

/*
 * Probing is easy: step trough the list of known vendor and device
 * ids and compare. If one is found - it's our.
 */
static int
fatm_probe(device_t dev)
{
	int i;

	for (i = 0; fatm_devs[i].name; i++)
		if (pci_get_vendor(dev) == fatm_devs[i].vid &&
		    pci_get_device(dev) == fatm_devs[i].did) {
			device_set_desc(dev, fatm_devs[i].name);
			return (BUS_PROBE_DEFAULT);
		}
	return (ENXIO);
}

/*
 * Function called at completion of a SUNI writeregs/readregs command.
 * This is called from the interrupt handler while holding the softc lock.
 * We use the queue entry as the randevouze point.
 */
static void
fatm_utopia_writeregs_complete(struct fatm_softc *sc, struct cmdqueue *q)
{

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if(H_GETSTAT(q->q.statp) & FATM_STAT_ERROR) {
		sc->istats.suni_reg_errors++;
		q->error = EIO;
	}
	wakeup(q);
}

/*
 * Write a SUNI register. The bits that are 1 in mask are written from val
 * into register reg. We wait for the command to complete by sleeping on
 * the register memory.
 *
 * We assume, that we already hold the softc mutex.
 */
static int
fatm_utopia_writereg(struct ifatm *ifatm, u_int reg, u_int mask, u_int val)
{
	int error;
	struct cmdqueue *q;
	struct fatm_softc *sc;

	sc = ifatm->ifp->if_softc;
	FATM_CHECKLOCK(sc);
	if (!(ifatm->ifp->if_drv_flags & IFF_DRV_RUNNING))
		return (EIO);

	/* get queue element and fill it */
	q = GET_QUEUE(sc->cmdqueue, struct cmdqueue, sc->cmdqueue.head);

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (!(H_GETSTAT(q->q.statp) & FATM_STAT_FREE)) {
		sc->istats.cmd_queue_full++;
		return (EIO);
	}
	NEXT_QUEUE_ENTRY(sc->cmdqueue.head, FATM_CMD_QLEN);

	q->error = 0;
	q->cb = fatm_utopia_writeregs_complete;
	H_SETSTAT(q->q.statp, FATM_STAT_PENDING);
	H_SYNCSTAT_PREWRITE(sc, q->q.statp);

	WRITE4(sc, q->q.card + FATMOC_GETOC3_BUF, 0);
	BARRIER_W(sc);
	WRITE4(sc, q->q.card + FATMOC_OP,
	    FATM_MAKE_SETOC3(reg, val, mask) | FATM_OP_INTERRUPT_SEL);
	BARRIER_W(sc);

	/*
	 * Wait for the command to complete
	 */
	error = msleep(q, &sc->mtx, PZERO | PCATCH, "fatm_setreg", hz);

	switch(error) {

	  case EWOULDBLOCK:
		error = EIO;
		break;

	  case ERESTART:
		error = EINTR;
		break;

	  case 0:
		error = q->error;
		break;
	}

	return (error);
}

/*
 * Function called at completion of a SUNI readregs command.
 * This is called from the interrupt handler while holding the softc lock.
 * We use reg_mem as the randevouze point.
 */
static void
fatm_utopia_readregs_complete(struct fatm_softc *sc, struct cmdqueue *q)
{

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (H_GETSTAT(q->q.statp) & FATM_STAT_ERROR) {
		sc->istats.suni_reg_errors++;
		q->error = EIO;
	}
	wakeup(&sc->reg_mem);
}

/*
 * Read SUNI registers
 *
 * We use a preallocated buffer to read the registers. Therefor we need
 * to protect against multiple threads trying to read registers. We do this
 * with a condition variable and a flag. We wait for the command to complete by sleeping on
 * the register memory.
 *
 * We assume, that we already hold the softc mutex.
 */
static int
fatm_utopia_readregs_internal(struct fatm_softc *sc)
{
	int error, i;
	uint32_t *ptr;
	struct cmdqueue *q;

	/* get the buffer */
	for (;;) {
		if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING))
			return (EIO);
		if (!(sc->flags & FATM_REGS_INUSE))
			break;
		cv_wait(&sc->cv_regs, &sc->mtx);
	}
	sc->flags |= FATM_REGS_INUSE;

	q = GET_QUEUE(sc->cmdqueue, struct cmdqueue, sc->cmdqueue.head);

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (!(H_GETSTAT(q->q.statp) & FATM_STAT_FREE)) {
		sc->istats.cmd_queue_full++;
		return (EIO);
	}
	NEXT_QUEUE_ENTRY(sc->cmdqueue.head, FATM_CMD_QLEN);

	q->error = 0;
	q->cb = fatm_utopia_readregs_complete;
	H_SETSTAT(q->q.statp, FATM_STAT_PENDING);
	H_SYNCSTAT_PREWRITE(sc, q->q.statp);

	bus_dmamap_sync(sc->reg_mem.dmat, sc->reg_mem.map, BUS_DMASYNC_PREREAD);

	WRITE4(sc, q->q.card + FATMOC_GETOC3_BUF, sc->reg_mem.paddr);
	BARRIER_W(sc);
	WRITE4(sc, q->q.card + FATMOC_OP,
	    FATM_OP_OC3_GET_REG | FATM_OP_INTERRUPT_SEL);
	BARRIER_W(sc);

	/*
	 * Wait for the command to complete
	 */
	error = msleep(&sc->reg_mem, &sc->mtx, PZERO | PCATCH,
	    "fatm_getreg", hz);

	switch(error) {

	  case EWOULDBLOCK:
		error = EIO;
		break;

	  case ERESTART:
		error = EINTR;
		break;

	  case 0:
		bus_dmamap_sync(sc->reg_mem.dmat, sc->reg_mem.map,
		    BUS_DMASYNC_POSTREAD);
		error = q->error;
		break;
	}

	if (error != 0) {
		/* declare buffer to be free */
		sc->flags &= ~FATM_REGS_INUSE;
		cv_signal(&sc->cv_regs);
		return (error);
	}

	/* swap if needed */
	ptr = (uint32_t *)sc->reg_mem.mem;
	for (i = 0; i < FATM_NREGS; i++)
		ptr[i] = le32toh(ptr[i]) & 0xff;

	return (0);
}

/*
 * Read SUNI registers for the SUNI module.
 *
 * We assume, that we already hold the mutex.
 */
static int
fatm_utopia_readregs(struct ifatm *ifatm, u_int reg, uint8_t *valp, u_int *np)
{
	int err;
	int i;
	struct fatm_softc *sc;

	if (reg >= FATM_NREGS)
		return (EINVAL);
	if (reg + *np > FATM_NREGS)
		*np = FATM_NREGS - reg;
	sc = ifatm->ifp->if_softc;
	FATM_CHECKLOCK(sc);

	err = fatm_utopia_readregs_internal(sc);
	if (err != 0)
		return (err);

	for (i = 0; i < *np; i++)
		valp[i] = ((uint32_t *)sc->reg_mem.mem)[reg + i];

	/* declare buffer to be free */
	sc->flags &= ~FATM_REGS_INUSE;
	cv_signal(&sc->cv_regs);

	return (0);
}

/*
 * Check whether the hard is beating. We remember the last heart beat and
 * compare it to the current one. If it appears stuck for 10 times, we have
 * a problem.
 *
 * Assume we hold the lock.
 */
static void
fatm_check_heartbeat(struct fatm_softc *sc)
{
	uint32_t h;

	FATM_CHECKLOCK(sc);

	h = READ4(sc, FATMO_HEARTBEAT);
	DBG(sc, BEAT, ("heartbeat %08x", h));

	if (sc->stop_cnt == 10)
		return;

	if (h == sc->heartbeat) {
		if (++sc->stop_cnt == 10) {
			log(LOG_ERR, "i960 stopped???\n");
			WRITE4(sc, FATMO_HIMR, 1);
		}
		return;
	}

	sc->stop_cnt = 0;
	sc->heartbeat = h;
}

/*
 * Ensure that the heart is still beating.
 */
static void
fatm_watchdog(struct ifnet *ifp)
{
	struct fatm_softc *sc = ifp->if_softc;

	FATM_LOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		fatm_check_heartbeat(sc);
		ifp->if_timer = 5;
	}
	FATM_UNLOCK(sc);
}

/*
 * Hard reset the i960 on the board. This is done by initializing registers,
 * clearing interrupts and waiting for the selftest to finish. Not sure,
 * whether all these barriers are actually needed.
 *
 * Assumes that we hold the lock.
 */
static int
fatm_reset(struct fatm_softc *sc)
{
	int w;
	uint32_t val;

	FATM_CHECKLOCK(sc);

	WRITE4(sc, FATMO_APP_BASE, FATMO_COMMON_ORIGIN);
	BARRIER_W(sc);

	WRITE4(sc, FATMO_UART_TO_960, XMIT_READY);
	BARRIER_W(sc);

	WRITE4(sc, FATMO_UART_TO_HOST, XMIT_READY);
	BARRIER_W(sc);

	WRITE4(sc, FATMO_BOOT_STATUS, COLD_START);
	BARRIER_W(sc);

	WRITE1(sc, FATMO_HCR, FATM_HCR_RESET);
	BARRIER_W(sc);

	DELAY(1000);

	WRITE1(sc, FATMO_HCR, 0);
	BARRIER_RW(sc);

	DELAY(1000);

	for (w = 100; w; w--) {
		BARRIER_R(sc);
		val = READ4(sc, FATMO_BOOT_STATUS);
		switch (val) {
		  case SELF_TEST_OK:
			return (0);
		  case SELF_TEST_FAIL:
			return (EIO);
		}
		DELAY(1000);
	}
	return (EIO);
}

/*
 * Stop the card. Must be called WITH the lock held
 * Reset, free transmit and receive buffers. Wakeup everybody who may sleep.
 */
static void
fatm_stop(struct fatm_softc *sc)
{
	int i;
	struct cmdqueue *q;
	struct rbuf *rb;
	struct txqueue *tx;
	uint32_t stat;

	FATM_CHECKLOCK(sc);

	/* Stop the board */
	utopia_stop(&sc->utopia);
	(void)fatm_reset(sc);

	/* stop watchdog */
	sc->ifp->if_timer = 0;

	if (sc->ifp->if_drv_flags & IFF_DRV_RUNNING) {
		sc->ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
		ATMEV_SEND_IFSTATE_CHANGED(IFP2IFATM(sc->ifp),
		    sc->utopia.carrier == UTP_CARR_OK);

		/*
		 * Collect transmit mbufs, partial receive mbufs and
		 * supplied mbufs
		 */
		for (i = 0; i < FATM_TX_QLEN; i++) {
			tx = GET_QUEUE(sc->txqueue, struct txqueue, i);
			if (tx->m) {
				bus_dmamap_unload(sc->tx_tag, tx->map);
				m_freem(tx->m);
				tx->m = NULL;
			}
		}

		/* Collect supplied mbufs */
		while ((rb = LIST_FIRST(&sc->rbuf_used)) != NULL) {
			LIST_REMOVE(rb, link);
			bus_dmamap_unload(sc->rbuf_tag, rb->map);
			m_free(rb->m);
			rb->m = NULL;
			LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
		}

		/* Unwait any waiters */
		wakeup(&sc->sadi_mem);

		/* wakeup all threads waiting for STAT or REG buffers */
		cv_broadcast(&sc->cv_stat);
		cv_broadcast(&sc->cv_regs);

		sc->flags &= ~(FATM_STAT_INUSE | FATM_REGS_INUSE);

		/* wakeup all threads waiting on commands */
		for (i = 0; i < FATM_CMD_QLEN; i++) {
			q = GET_QUEUE(sc->cmdqueue, struct cmdqueue, i);

			H_SYNCSTAT_POSTREAD(sc, q->q.statp);
			if ((stat = H_GETSTAT(q->q.statp)) != FATM_STAT_FREE) {
				H_SETSTAT(q->q.statp, stat | FATM_STAT_ERROR);
				H_SYNCSTAT_PREWRITE(sc, q->q.statp);
				wakeup(q);
			}
		}
		utopia_reset_media(&sc->utopia);
	}
	sc->small_cnt = sc->large_cnt = 0;

	/* Reset vcc info */
	if (sc->vccs != NULL) {
		sc->open_vccs = 0;
		for (i = 0; i < FORE_MAX_VCC + 1; i++) {
			if (sc->vccs[i] != NULL) {
				if ((sc->vccs[i]->vflags & (FATM_VCC_OPEN |
				    FATM_VCC_TRY_OPEN)) == 0) {
					uma_zfree(sc->vcc_zone, sc->vccs[i]);
					sc->vccs[i] = NULL;
				} else {
					sc->vccs[i]->vflags = 0;
					sc->open_vccs++;
				}
			}
		}
	}

}

/*
 * Load the firmware into the board and save the entry point.
 */
static uint32_t
firmware_load(struct fatm_softc *sc)
{
	struct firmware *fw = (struct firmware *)firmware;

	DBG(sc, INIT, ("loading - entry=%x", fw->entry));
	bus_space_write_region_4(sc->memt, sc->memh, fw->offset, firmware,
	    sizeof(firmware) / sizeof(firmware[0]));
	BARRIER_RW(sc);

	return (fw->entry);
}

/*
 * Read a character from the virtual UART. The availability of a character
 * is signaled by a non-null value of the 32 bit register. The eating of
 * the character by us is signalled to the card by setting that register
 * to zero.
 */
static int
rx_getc(struct fatm_softc *sc)
{
	int w = 50;
	int c;

	while (w--) {
		c = READ4(sc, FATMO_UART_TO_HOST);
		BARRIER_RW(sc);
		if (c != 0) {
			WRITE4(sc, FATMO_UART_TO_HOST, 0);
			DBGC(sc, UART, ("%c", c & 0xff));
			return (c & 0xff);
		}
		DELAY(1000);
	}
	return (-1);
}

/*
 * Eat up characters from the board and stuff them in the bit-bucket.
 */
static void
rx_flush(struct fatm_softc *sc)
{
	int w = 10000;

	while (w-- && rx_getc(sc) >= 0)
		;
}

/* 
 * Write a character to the card. The UART is available if the register
 * is zero.
 */
static int
tx_putc(struct fatm_softc *sc, u_char c)
{
	int w = 10;
	int c1;

	while (w--) {
		c1 = READ4(sc, FATMO_UART_TO_960);
		BARRIER_RW(sc);
		if (c1 == 0) {
			WRITE4(sc, FATMO_UART_TO_960, c | CHAR_AVAIL);
			DBGC(sc, UART, ("%c", c & 0xff));
			return (0);
		}
		DELAY(1000);
	}
	return (-1);
}

/*
 * Start the firmware. This is doing by issuing a 'go' command with
 * the hex entry address of the firmware. Then we wait for the self-test to
 * succeed.
 */
static int
fatm_start_firmware(struct fatm_softc *sc, uint32_t start)
{
	static char hex[] = "0123456789abcdef";
	u_int w, val;

	DBG(sc, INIT, ("starting"));
	rx_flush(sc);
	tx_putc(sc, '\r');
	DELAY(1000);

	rx_flush(sc);

	tx_putc(sc, 'g');
	(void)rx_getc(sc);
	tx_putc(sc, 'o');
	(void)rx_getc(sc);
	tx_putc(sc, ' ');
	(void)rx_getc(sc);

	tx_putc(sc, hex[(start >> 12) & 0xf]);
	(void)rx_getc(sc);
	tx_putc(sc, hex[(start >>  8) & 0xf]);
	(void)rx_getc(sc);
	tx_putc(sc, hex[(start >>  4) & 0xf]);
	(void)rx_getc(sc);
	tx_putc(sc, hex[(start >>  0) & 0xf]);
	(void)rx_getc(sc);

	tx_putc(sc, '\r');
	rx_flush(sc);

	for (w = 100; w; w--) {
		BARRIER_R(sc);
		val = READ4(sc, FATMO_BOOT_STATUS);
		switch (val) {
		  case CP_RUNNING:
			return (0);
		  case SELF_TEST_FAIL:
			return (EIO);
		}
		DELAY(1000);
	}
	return (EIO);
}

/*
 * Initialize one card and host queue.
 */
static void
init_card_queue(struct fatm_softc *sc, struct fqueue *queue, int qlen,
    size_t qel_size, size_t desc_size, cardoff_t off,
    u_char **statpp, uint32_t *cardstat, u_char *descp, uint32_t carddesc)
{
	struct fqelem *el = queue->chunk;

	while (qlen--) {
		el->card = off;
		off += 8;	/* size of card entry */

		el->statp = (uint32_t *)(*statpp);
		(*statpp) += sizeof(uint32_t);
		H_SETSTAT(el->statp, FATM_STAT_FREE);
		H_SYNCSTAT_PREWRITE(sc, el->statp);

		WRITE4(sc, el->card + FATMOS_STATP, (*cardstat));
		(*cardstat) += sizeof(uint32_t);

		el->ioblk = descp;
		descp += desc_size;
		el->card_ioblk = carddesc;
		carddesc += desc_size;

		el = (struct fqelem *)((u_char *)el + qel_size);
	}
	queue->tail = queue->head = 0;
}

/*
 * Issue the initialize operation to the card, wait for completion and
 * initialize the on-board and host queue structures with offsets and
 * addresses.
 */
static int
fatm_init_cmd(struct fatm_softc *sc)
{
	int w, c;
	u_char *statp;
	uint32_t card_stat;
	u_int cnt;
	struct fqelem *el;
	cardoff_t off;

	DBG(sc, INIT, ("command"));
	WRITE4(sc, FATMO_ISTAT, 0);
	WRITE4(sc, FATMO_IMASK, 1);
	WRITE4(sc, FATMO_HLOGGER, 0);

	WRITE4(sc, FATMO_INIT + FATMOI_RECEIVE_TRESHOLD, 0);
	WRITE4(sc, FATMO_INIT + FATMOI_NUM_CONNECT, FORE_MAX_VCC);
	WRITE4(sc, FATMO_INIT + FATMOI_CQUEUE_LEN, FATM_CMD_QLEN);
	WRITE4(sc, FATMO_INIT + FATMOI_TQUEUE_LEN, FATM_TX_QLEN);
	WRITE4(sc, FATMO_INIT + FATMOI_RQUEUE_LEN, FATM_RX_QLEN);
	WRITE4(sc, FATMO_INIT + FATMOI_RPD_EXTENSION, RPD_EXTENSIONS);
	WRITE4(sc, FATMO_INIT + FATMOI_TPD_EXTENSION, TPD_EXTENSIONS);

	/*
	 * initialize buffer descriptors
	 */
	WRITE4(sc, FATMO_INIT + FATMOI_SMALL_B1 + FATMOB_QUEUE_LENGTH,
	    SMALL_SUPPLY_QLEN);
	WRITE4(sc, FATMO_INIT + FATMOI_SMALL_B1 + FATMOB_BUFFER_SIZE,
	    SMALL_BUFFER_LEN);
	WRITE4(sc, FATMO_INIT + FATMOI_SMALL_B1 + FATMOB_POOL_SIZE,
	    SMALL_POOL_SIZE);
	WRITE4(sc, FATMO_INIT + FATMOI_SMALL_B1 + FATMOB_SUPPLY_BLKSIZE,
	    SMALL_SUPPLY_BLKSIZE);

	WRITE4(sc, FATMO_INIT + FATMOI_LARGE_B1 + FATMOB_QUEUE_LENGTH,
	    LARGE_SUPPLY_QLEN);
	WRITE4(sc, FATMO_INIT + FATMOI_LARGE_B1 + FATMOB_BUFFER_SIZE,
	    LARGE_BUFFER_LEN);
	WRITE4(sc, FATMO_INIT + FATMOI_LARGE_B1 + FATMOB_POOL_SIZE,
	    LARGE_POOL_SIZE);
	WRITE4(sc, FATMO_INIT + FATMOI_LARGE_B1 + FATMOB_SUPPLY_BLKSIZE,
	    LARGE_SUPPLY_BLKSIZE);

	WRITE4(sc, FATMO_INIT + FATMOI_SMALL_B2 + FATMOB_QUEUE_LENGTH, 0);
	WRITE4(sc, FATMO_INIT + FATMOI_SMALL_B2 + FATMOB_BUFFER_SIZE, 0);
	WRITE4(sc, FATMO_INIT + FATMOI_SMALL_B2 + FATMOB_POOL_SIZE, 0);
	WRITE4(sc, FATMO_INIT + FATMOI_SMALL_B2 + FATMOB_SUPPLY_BLKSIZE, 0);

	WRITE4(sc, FATMO_INIT + FATMOI_LARGE_B2 + FATMOB_QUEUE_LENGTH, 0);
	WRITE4(sc, FATMO_INIT + FATMOI_LARGE_B2 + FATMOB_BUFFER_SIZE, 0);
	WRITE4(sc, FATMO_INIT + FATMOI_LARGE_B2 + FATMOB_POOL_SIZE, 0);
	WRITE4(sc, FATMO_INIT + FATMOI_LARGE_B2 + FATMOB_SUPPLY_BLKSIZE, 0);

	/*
	 * Start the command
	 */
	BARRIER_W(sc);
	WRITE4(sc, FATMO_INIT + FATMOI_STATUS, FATM_STAT_PENDING);
	BARRIER_W(sc);
	WRITE4(sc, FATMO_INIT + FATMOI_OP, FATM_OP_INITIALIZE);
	BARRIER_W(sc);

	/*
	 * Busy wait for completion
	 */
	w = 100;
	while (w--) {
		c = READ4(sc, FATMO_INIT + FATMOI_STATUS);
		BARRIER_R(sc);
		if (c & FATM_STAT_COMPLETE)
			break;
		DELAY(1000);
	}

	if (c & FATM_STAT_ERROR)
		return (EIO);

	/*
	 * Initialize the queues
	 */
	statp = sc->stat_mem.mem;
	card_stat = sc->stat_mem.paddr;

	/*
	 * Command queue. This is special in that it's on the card.
	 */
	el = sc->cmdqueue.chunk;
	off = READ4(sc, FATMO_COMMAND_QUEUE);
	DBG(sc, INIT, ("cmd queue=%x", off));
	for (cnt = 0; cnt < FATM_CMD_QLEN; cnt++) {
		el = &((struct cmdqueue *)sc->cmdqueue.chunk + cnt)->q;

		el->card = off;
		off += 32;		/* size of card structure */

		el->statp = (uint32_t *)statp;
		statp += sizeof(uint32_t);
		H_SETSTAT(el->statp, FATM_STAT_FREE);
		H_SYNCSTAT_PREWRITE(sc, el->statp);

		WRITE4(sc, el->card + FATMOC_STATP, card_stat);
		card_stat += sizeof(uint32_t);
	}
	sc->cmdqueue.tail = sc->cmdqueue.head = 0;

	/*
	 * Now the other queues. These are in memory
	 */
	init_card_queue(sc, &sc->txqueue, FATM_TX_QLEN,
	    sizeof(struct txqueue), TPD_SIZE,
	    READ4(sc, FATMO_TRANSMIT_QUEUE),
	    &statp, &card_stat, sc->txq_mem.mem, sc->txq_mem.paddr);

	init_card_queue(sc, &sc->rxqueue, FATM_RX_QLEN,
	    sizeof(struct rxqueue), RPD_SIZE,
	    READ4(sc, FATMO_RECEIVE_QUEUE),
	    &statp, &card_stat, sc->rxq_mem.mem, sc->rxq_mem.paddr);

	init_card_queue(sc, &sc->s1queue, SMALL_SUPPLY_QLEN,
	    sizeof(struct supqueue), BSUP_BLK2SIZE(SMALL_SUPPLY_BLKSIZE),
	    READ4(sc, FATMO_SMALL_B1_QUEUE),
	    &statp, &card_stat, sc->s1q_mem.mem, sc->s1q_mem.paddr);

	init_card_queue(sc, &sc->l1queue, LARGE_SUPPLY_QLEN,
	    sizeof(struct supqueue), BSUP_BLK2SIZE(LARGE_SUPPLY_BLKSIZE),
	    READ4(sc, FATMO_LARGE_B1_QUEUE),
	    &statp, &card_stat, sc->l1q_mem.mem, sc->l1q_mem.paddr);

	sc->txcnt = 0;

	return (0);
}

/*
 * Read PROM. Called only from attach code. Here we spin because the interrupt
 * handler is not yet set up.
 */
static int
fatm_getprom(struct fatm_softc *sc)
{
	int i;
	struct prom *prom;
	struct cmdqueue *q;

	DBG(sc, INIT, ("reading prom"));
	q = GET_QUEUE(sc->cmdqueue, struct cmdqueue, sc->cmdqueue.head);
	NEXT_QUEUE_ENTRY(sc->cmdqueue.head, FATM_CMD_QLEN);

	q->error = 0;
	q->cb = NULL;;
	H_SETSTAT(q->q.statp, FATM_STAT_PENDING);
	H_SYNCSTAT_PREWRITE(sc, q->q.statp);

	bus_dmamap_sync(sc->prom_mem.dmat, sc->prom_mem.map,
	    BUS_DMASYNC_PREREAD);

	WRITE4(sc, q->q.card + FATMOC_GPROM_BUF, sc->prom_mem.paddr);
	BARRIER_W(sc);
	WRITE4(sc, q->q.card + FATMOC_OP, FATM_OP_GET_PROM_DATA);
	BARRIER_W(sc);

	for (i = 0; i < 1000; i++) {
		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		if (H_GETSTAT(q->q.statp) &
		    (FATM_STAT_COMPLETE | FATM_STAT_ERROR))
			break;
		DELAY(1000);
	}
	if (i == 1000) {
		if_printf(sc->ifp, "getprom timeout\n");
		return (EIO);
	}
	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (H_GETSTAT(q->q.statp) & FATM_STAT_ERROR) {
		if_printf(sc->ifp, "getprom error\n");
		return (EIO);
	}
	H_SETSTAT(q->q.statp, FATM_STAT_FREE);
	H_SYNCSTAT_PREWRITE(sc, q->q.statp);
	NEXT_QUEUE_ENTRY(sc->cmdqueue.tail, FATM_CMD_QLEN);

	bus_dmamap_sync(sc->prom_mem.dmat, sc->prom_mem.map,
	    BUS_DMASYNC_POSTREAD);


#ifdef notdef
	{
		u_int i;

		printf("PROM: ");
		u_char *ptr = (u_char *)sc->prom_mem.mem;
		for (i = 0; i < sizeof(struct prom); i++)
			printf("%02x ", *ptr++);
		printf("\n");
	}
#endif

	prom = (struct prom *)sc->prom_mem.mem;

	bcopy(prom->mac + 2, IFP2IFATM(sc->ifp)->mib.esi, 6);
	IFP2IFATM(sc->ifp)->mib.serial = le32toh(prom->serial);
	IFP2IFATM(sc->ifp)->mib.hw_version = le32toh(prom->version);
	IFP2IFATM(sc->ifp)->mib.sw_version = READ4(sc, FATMO_FIRMWARE_RELEASE);

	if_printf(sc->ifp, "ESI=%02x:%02x:%02x:%02x:%02x:%02x "
	    "serial=%u hw=0x%x sw=0x%x\n", IFP2IFATM(sc->ifp)->mib.esi[0],
	    IFP2IFATM(sc->ifp)->mib.esi[1], IFP2IFATM(sc->ifp)->mib.esi[2], IFP2IFATM(sc->ifp)->mib.esi[3],
	    IFP2IFATM(sc->ifp)->mib.esi[4], IFP2IFATM(sc->ifp)->mib.esi[5], IFP2IFATM(sc->ifp)->mib.serial,
	    IFP2IFATM(sc->ifp)->mib.hw_version, IFP2IFATM(sc->ifp)->mib.sw_version);

	return (0);
}

/*
 * This is the callback function for bus_dmamap_load. We assume, that we
 * have a 32-bit bus and so have always one segment.
 */
static void
dmaload_helper(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *ptr = (bus_addr_t *)arg;

	if (error != 0) {
		printf("%s: error=%d\n", __func__, error);
		return;
	}
	KASSERT(nsegs == 1, ("too many DMA segments"));
	KASSERT(segs[0].ds_addr <= 0xffffffff, ("DMA address too large %lx",
	    (u_long)segs[0].ds_addr));

	*ptr = segs[0].ds_addr;
}

/*
 * Allocate a chunk of DMA-able memory and map it.
 */
static int
alloc_dma_memory(struct fatm_softc *sc, const char *nm, struct fatm_mem *mem)
{
	int error;

	mem->mem = NULL;

	if (bus_dma_tag_create(sc->parent_dmat, mem->align, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, mem->size, 1, BUS_SPACE_MAXSIZE_32BIT,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &mem->dmat)) {
		if_printf(sc->ifp, "could not allocate %s DMA tag\n",
		    nm);
		return (ENOMEM);
	}

	error = bus_dmamem_alloc(mem->dmat, &mem->mem, 0, &mem->map);
	if (error) {
		if_printf(sc->ifp, "could not allocate %s DMA memory: "
		    "%d\n", nm, error);
		bus_dma_tag_destroy(mem->dmat);
		mem->mem = NULL;
		return (error);
	}

	error = bus_dmamap_load(mem->dmat, mem->map, mem->mem, mem->size,
	    dmaload_helper, &mem->paddr, BUS_DMA_NOWAIT);
	if (error) {
		if_printf(sc->ifp, "could not load %s DMA memory: "
		    "%d\n", nm, error);
		bus_dmamem_free(mem->dmat, mem->mem, mem->map);
		bus_dma_tag_destroy(mem->dmat);
		mem->mem = NULL;
		return (error);
	}

	DBG(sc, DMA, ("DMA %s V/P/S/Z %p/%lx/%x/%x", nm, mem->mem,
	    (u_long)mem->paddr, mem->size, mem->align));

	return (0);
}

#ifdef TEST_DMA_SYNC
static int
alloc_dma_memoryX(struct fatm_softc *sc, const char *nm, struct fatm_mem *mem)
{
	int error;

	mem->mem = NULL;

	if (bus_dma_tag_create(NULL, mem->align, 0,
	    BUS_SPACE_MAXADDR_24BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, mem->size, 1, mem->size,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &mem->dmat)) {
		if_printf(sc->ifp, "could not allocate %s DMA tag\n",
		    nm);
		return (ENOMEM);
	}

	mem->mem = contigmalloc(mem->size, M_DEVBUF, M_WAITOK,
	    BUS_SPACE_MAXADDR_24BIT, BUS_SPACE_MAXADDR_32BIT, mem->align, 0);

	error = bus_dmamap_create(mem->dmat, 0, &mem->map);
	if (error) {
		if_printf(sc->ifp, "could not allocate %s DMA map: "
		    "%d\n", nm, error);
		contigfree(mem->mem, mem->size, M_DEVBUF);
		bus_dma_tag_destroy(mem->dmat);
		mem->mem = NULL;
		return (error);
	}

	error = bus_dmamap_load(mem->dmat, mem->map, mem->mem, mem->size,
	    dmaload_helper, &mem->paddr, BUS_DMA_NOWAIT);
	if (error) {
		if_printf(sc->ifp, "could not load %s DMA memory: "
		    "%d\n", nm, error);
		bus_dmamap_destroy(mem->dmat, mem->map);
		contigfree(mem->mem, mem->size, M_DEVBUF);
		bus_dma_tag_destroy(mem->dmat);
		mem->mem = NULL;
		return (error);
	}

	DBG(sc, DMA, ("DMAX %s V/P/S/Z %p/%lx/%x/%x", nm, mem->mem,
	    (u_long)mem->paddr, mem->size, mem->align));

	printf("DMAX: %s V/P/S/Z %p/%lx/%x/%x", nm, mem->mem,
	    (u_long)mem->paddr, mem->size, mem->align);

	return (0);
}
#endif /* TEST_DMA_SYNC */

/*
 * Destroy all resources of an dma-able memory chunk
 */
static void
destroy_dma_memory(struct fatm_mem *mem)
{
	if (mem->mem != NULL) {
		bus_dmamap_unload(mem->dmat, mem->map);
		bus_dmamem_free(mem->dmat, mem->mem, mem->map);
		bus_dma_tag_destroy(mem->dmat);
		mem->mem = NULL;
	}
}
#ifdef TEST_DMA_SYNC
static void
destroy_dma_memoryX(struct fatm_mem *mem)
{
	if (mem->mem != NULL) {
		bus_dmamap_unload(mem->dmat, mem->map);
		bus_dmamap_destroy(mem->dmat, mem->map);
		contigfree(mem->mem, mem->size, M_DEVBUF);
		bus_dma_tag_destroy(mem->dmat);
		mem->mem = NULL;
	}
}
#endif /* TEST_DMA_SYNC */

/*
 * Try to supply buffers to the card if there are free entries in the queues
 */
static void
fatm_supply_small_buffers(struct fatm_softc *sc)
{
	int nblocks, nbufs;
	struct supqueue *q;
	struct rbd *bd;
	int i, j, error, cnt;
	struct mbuf *m;
	struct rbuf *rb;
	bus_addr_t phys;

	nbufs = max(4 * sc->open_vccs, 32);
	nbufs = min(nbufs, SMALL_POOL_SIZE);
	nbufs -= sc->small_cnt;

	nblocks = (nbufs + SMALL_SUPPLY_BLKSIZE - 1) / SMALL_SUPPLY_BLKSIZE;
	for (cnt = 0; cnt < nblocks; cnt++) {
		q = GET_QUEUE(sc->s1queue, struct supqueue, sc->s1queue.head);

		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		if (H_GETSTAT(q->q.statp) != FATM_STAT_FREE)
			break;

		bd = (struct rbd *)q->q.ioblk;

		for (i = 0; i < SMALL_SUPPLY_BLKSIZE; i++) {
			if ((rb = LIST_FIRST(&sc->rbuf_free)) == NULL) {
				if_printf(sc->ifp, "out of rbufs\n");
				break;
			}
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
				break;
			}
			MH_ALIGN(m, SMALL_BUFFER_LEN);
			error = bus_dmamap_load(sc->rbuf_tag, rb->map,
			    m->m_data, SMALL_BUFFER_LEN, dmaload_helper,
			    &phys, BUS_DMA_NOWAIT);
			if (error) {
				if_printf(sc->ifp,
				    "dmamap_load mbuf failed %d", error);
				m_freem(m);
				LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
				break;
			}
			bus_dmamap_sync(sc->rbuf_tag, rb->map,
			    BUS_DMASYNC_PREREAD);

			LIST_REMOVE(rb, link);
			LIST_INSERT_HEAD(&sc->rbuf_used, rb, link);

			rb->m = m;
			bd[i].handle = rb - sc->rbufs;
			H_SETDESC(bd[i].buffer, phys);
		}

		if (i < SMALL_SUPPLY_BLKSIZE) {
			for (j = 0; j < i; j++) {
				rb = sc->rbufs + bd[j].handle;
				bus_dmamap_unload(sc->rbuf_tag, rb->map);
				m_free(rb->m);
				rb->m = NULL;

				LIST_REMOVE(rb, link);
				LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
			}
			break;
		}
		H_SYNCQ_PREWRITE(&sc->s1q_mem, bd,
		    sizeof(struct rbd) * SMALL_SUPPLY_BLKSIZE);

		H_SETSTAT(q->q.statp, FATM_STAT_PENDING);
		H_SYNCSTAT_PREWRITE(sc, q->q.statp);

		WRITE4(sc, q->q.card, q->q.card_ioblk);
		BARRIER_W(sc);

		sc->small_cnt += SMALL_SUPPLY_BLKSIZE;

		NEXT_QUEUE_ENTRY(sc->s1queue.head, SMALL_SUPPLY_QLEN);
	}
}

/*
 * Try to supply buffers to the card if there are free entries in the queues
 * We assume that all buffers are within the address space accessible by the
 * card (32-bit), so we don't need bounce buffers.
 */
static void
fatm_supply_large_buffers(struct fatm_softc *sc)
{
	int nbufs, nblocks, cnt;
	struct supqueue *q;
	struct rbd *bd;
	int i, j, error;
	struct mbuf *m;
	struct rbuf *rb;
	bus_addr_t phys;

	nbufs = max(4 * sc->open_vccs, 32);
	nbufs = min(nbufs, LARGE_POOL_SIZE);
	nbufs -= sc->large_cnt;

	nblocks = (nbufs + LARGE_SUPPLY_BLKSIZE - 1) / LARGE_SUPPLY_BLKSIZE;

	for (cnt = 0; cnt < nblocks; cnt++) {
		q = GET_QUEUE(sc->l1queue, struct supqueue, sc->l1queue.head);

		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		if (H_GETSTAT(q->q.statp) != FATM_STAT_FREE)
			break;

		bd = (struct rbd *)q->q.ioblk;

		for (i = 0; i < LARGE_SUPPLY_BLKSIZE; i++) {
			if ((rb = LIST_FIRST(&sc->rbuf_free)) == NULL) {
				if_printf(sc->ifp, "out of rbufs\n");
				break;
			}
			if ((m = m_getcl(M_DONTWAIT, MT_DATA,
			    M_PKTHDR)) == NULL) {
				LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
				break;
			}
			/* No MEXT_ALIGN */
			m->m_data += MCLBYTES - LARGE_BUFFER_LEN;
			error = bus_dmamap_load(sc->rbuf_tag, rb->map,
			    m->m_data, LARGE_BUFFER_LEN, dmaload_helper,
			    &phys, BUS_DMA_NOWAIT);
			if (error) {
				if_printf(sc->ifp,
				    "dmamap_load mbuf failed %d", error);
				m_freem(m);
				LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
				break;
			}

			bus_dmamap_sync(sc->rbuf_tag, rb->map,
			    BUS_DMASYNC_PREREAD);

			LIST_REMOVE(rb, link);
			LIST_INSERT_HEAD(&sc->rbuf_used, rb, link);

			rb->m = m;
			bd[i].handle = rb - sc->rbufs;
			H_SETDESC(bd[i].buffer, phys);
		}

		if (i < LARGE_SUPPLY_BLKSIZE) {
			for (j = 0; j < i; j++) {
				rb = sc->rbufs + bd[j].handle;
				bus_dmamap_unload(sc->rbuf_tag, rb->map);
				m_free(rb->m);
				rb->m = NULL;

				LIST_REMOVE(rb, link);
				LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
			}
			break;
		}
		H_SYNCQ_PREWRITE(&sc->l1q_mem, bd,
		    sizeof(struct rbd) * LARGE_SUPPLY_BLKSIZE);

		H_SETSTAT(q->q.statp, FATM_STAT_PENDING);
		H_SYNCSTAT_PREWRITE(sc, q->q.statp);
		WRITE4(sc, q->q.card, q->q.card_ioblk);
		BARRIER_W(sc);

		sc->large_cnt += LARGE_SUPPLY_BLKSIZE;

		NEXT_QUEUE_ENTRY(sc->l1queue.head, LARGE_SUPPLY_QLEN);
	}
}


/*
 * Actually start the card. The lock must be held here.
 * Reset, load the firmware, start it, initializes queues, read the PROM
 * and supply receive buffers to the card.
 */
static void
fatm_init_locked(struct fatm_softc *sc)
{
	struct rxqueue *q;
	int i, c, error;
	uint32_t start;

	DBG(sc, INIT, ("initialize"));
	if (sc->ifp->if_drv_flags & IFF_DRV_RUNNING)
		fatm_stop(sc);

	/*
	 * Hard reset the board
	 */
	if (fatm_reset(sc))
		return;

	start = firmware_load(sc);
	if (fatm_start_firmware(sc, start) || fatm_init_cmd(sc) ||
	    fatm_getprom(sc)) {
		fatm_reset(sc);
		return;
	}

	/*
	 * Handle media
	 */
	c = READ4(sc, FATMO_MEDIA_TYPE);
	switch (c) {

	  case FORE_MT_TAXI_100:
		IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_TAXI_100;
		IFP2IFATM(sc->ifp)->mib.pcr = 227273;
		break;

	  case FORE_MT_TAXI_140:
		IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_TAXI_140;
		IFP2IFATM(sc->ifp)->mib.pcr = 318181;
		break;

	  case FORE_MT_UTP_SONET:
		IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_UTP_155;
		IFP2IFATM(sc->ifp)->mib.pcr = 353207;
		break;

	  case FORE_MT_MM_OC3_ST:
	  case FORE_MT_MM_OC3_SC:
		IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_MM_155;
		IFP2IFATM(sc->ifp)->mib.pcr = 353207;
		break;

	  case FORE_MT_SM_OC3_ST:
	  case FORE_MT_SM_OC3_SC:
		IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_SM_155;
		IFP2IFATM(sc->ifp)->mib.pcr = 353207;
		break;

	  default:
		log(LOG_ERR, "fatm: unknown media type %d\n", c);
		IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_UNKNOWN;
		IFP2IFATM(sc->ifp)->mib.pcr = 353207;
		break;
	}
	sc->ifp->if_baudrate = 53 * 8 * IFP2IFATM(sc->ifp)->mib.pcr;
	utopia_init_media(&sc->utopia);

	/*
	 * Initialize the RBDs
	 */
	for (i = 0; i < FATM_RX_QLEN; i++) {
		q = GET_QUEUE(sc->rxqueue, struct rxqueue, i);
		WRITE4(sc, q->q.card + 0, q->q.card_ioblk);
	}
	BARRIER_W(sc);

	/*
	 * Supply buffers to the card
	 */
	fatm_supply_small_buffers(sc);
	fatm_supply_large_buffers(sc);

	/*
	 * Now set flags, that we are ready
	 */
	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;

	/*
	 * Start the watchdog timer
	 */
	sc->ifp->if_timer = 5;

	/* start SUNI */
	utopia_start(&sc->utopia);

	ATMEV_SEND_IFSTATE_CHANGED(IFP2IFATM(sc->ifp),
	    sc->utopia.carrier == UTP_CARR_OK);

	/* start all channels */
	for (i = 0; i < FORE_MAX_VCC + 1; i++)
		if (sc->vccs[i] != NULL) {
			sc->vccs[i]->vflags |= FATM_VCC_REOPEN;
			error = fatm_load_vc(sc, sc->vccs[i]);
			if (error != 0) {
				if_printf(sc->ifp, "reopening %u "
				    "failed: %d\n", i, error);
				sc->vccs[i]->vflags &= ~FATM_VCC_REOPEN;
			}
		}

	DBG(sc, INIT, ("done"));
}

/*
 * This is the exported as initialisation function.
 */
static void
fatm_init(void *p)
{
	struct fatm_softc *sc = p;

	FATM_LOCK(sc);
	fatm_init_locked(sc);
	FATM_UNLOCK(sc);
}

/************************************************************/
/*
 * The INTERRUPT handling
 */
/*
 * Check the command queue. If a command was completed, call the completion
 * function for that command.
 */
static void
fatm_intr_drain_cmd(struct fatm_softc *sc)
{
	struct cmdqueue *q;
	int stat;

	/*
	 * Drain command queue
	 */
	for (;;) {
		q = GET_QUEUE(sc->cmdqueue, struct cmdqueue, sc->cmdqueue.tail);

		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		stat = H_GETSTAT(q->q.statp);

		if (stat != FATM_STAT_COMPLETE &&
		   stat != (FATM_STAT_COMPLETE | FATM_STAT_ERROR) &&
		   stat != FATM_STAT_ERROR)
			break;

		(*q->cb)(sc, q);

		H_SETSTAT(q->q.statp, FATM_STAT_FREE);
		H_SYNCSTAT_PREWRITE(sc, q->q.statp);

		NEXT_QUEUE_ENTRY(sc->cmdqueue.tail, FATM_CMD_QLEN);
	}
}

/*
 * Drain the small buffer supply queue.
 */
static void
fatm_intr_drain_small_buffers(struct fatm_softc *sc)
{
	struct supqueue *q;
	int stat;

	for (;;) {
		q = GET_QUEUE(sc->s1queue, struct supqueue, sc->s1queue.tail);

		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		stat = H_GETSTAT(q->q.statp);

		if ((stat & FATM_STAT_COMPLETE) == 0)
			break;
		if (stat & FATM_STAT_ERROR)
			log(LOG_ERR, "%s: status %x\n", __func__, stat);

		H_SETSTAT(q->q.statp, FATM_STAT_FREE);
		H_SYNCSTAT_PREWRITE(sc, q->q.statp);

		NEXT_QUEUE_ENTRY(sc->s1queue.tail, SMALL_SUPPLY_QLEN);
	}
}

/*
 * Drain the large buffer supply queue.
 */
static void
fatm_intr_drain_large_buffers(struct fatm_softc *sc)
{
	struct supqueue *q;
	int stat;

	for (;;) {
		q = GET_QUEUE(sc->l1queue, struct supqueue, sc->l1queue.tail);

		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		stat = H_GETSTAT(q->q.statp);

		if ((stat & FATM_STAT_COMPLETE) == 0)
			break;
		if (stat & FATM_STAT_ERROR)
			log(LOG_ERR, "%s status %x\n", __func__, stat);

		H_SETSTAT(q->q.statp, FATM_STAT_FREE);
		H_SYNCSTAT_PREWRITE(sc, q->q.statp);

		NEXT_QUEUE_ENTRY(sc->l1queue.tail, LARGE_SUPPLY_QLEN);
	}
}

/*
 * Check the receive queue. Send any received PDU up the protocol stack
 * (except when there was an error or the VCI appears to be closed. In this
 * case discard the PDU).
 */
static void
fatm_intr_drain_rx(struct fatm_softc *sc)
{
	struct rxqueue *q;
	int stat, mlen;
	u_int i;
	uint32_t h;
	struct mbuf *last, *m0;
	struct rpd *rpd;
	struct rbuf *rb;
	u_int vci, vpi, pt;
	struct atm_pseudohdr aph;
	struct ifnet *ifp;
	struct card_vcc *vc;

	for (;;) {
		q = GET_QUEUE(sc->rxqueue, struct rxqueue, sc->rxqueue.tail);

		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		stat = H_GETSTAT(q->q.statp);

		if ((stat & FATM_STAT_COMPLETE) == 0)
			break;

		rpd = (struct rpd *)q->q.ioblk;
		H_SYNCQ_POSTREAD(&sc->rxq_mem, rpd, RPD_SIZE);

		rpd->nseg = le32toh(rpd->nseg);
		mlen = 0;
		m0 = last = 0;
		for (i = 0; i < rpd->nseg; i++) {
			rb = sc->rbufs + rpd->segment[i].handle;
			if (m0 == NULL) {
				m0 = last = rb->m;
			} else {
				last->m_next = rb->m;
				last = rb->m;
			}
			last->m_next = NULL;
			if (last->m_flags & M_EXT)
				sc->large_cnt--;
			else
				sc->small_cnt--;
			bus_dmamap_sync(sc->rbuf_tag, rb->map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->rbuf_tag, rb->map);
			rb->m = NULL;

			LIST_REMOVE(rb, link);
			LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);

			last->m_len = le32toh(rpd->segment[i].length);
			mlen += last->m_len;
		}

		m0->m_pkthdr.len = mlen;
		m0->m_pkthdr.rcvif = sc->ifp;

		h = le32toh(rpd->atm_header);
		vpi = (h >> 20) & 0xff;
		vci = (h >> 4 ) & 0xffff;
		pt  = (h >> 1 ) & 0x7;

		/*
		 * Locate the VCC this packet belongs to
		 */
		if (!VC_OK(sc, vpi, vci))
			vc = NULL;
		else if ((vc = sc->vccs[vci]) == NULL ||
		    !(sc->vccs[vci]->vflags & FATM_VCC_OPEN)) {
			sc->istats.rx_closed++;
			vc = NULL;
		}

		DBG(sc, RCV, ("RCV: vc=%u.%u pt=%u mlen=%d %s", vpi, vci,
		    pt, mlen, vc == NULL ? "dropped" : ""));

		if (vc == NULL) {
			m_freem(m0);
		} else {
#ifdef ENABLE_BPF
			if (!(vc->param.flags & ATMIO_FLAG_NG) &&
			    vc->param.aal == ATMIO_AAL_5 &&
			    (vc->param.flags & ATM_PH_LLCSNAP))
				BPF_MTAP(sc->ifp, m0);
#endif

			ATM_PH_FLAGS(&aph) = vc->param.flags;
			ATM_PH_VPI(&aph) = vpi;
			ATM_PH_SETVCI(&aph, vci);

			ifp = sc->ifp;
			ifp->if_ipackets++;

			vc->ipackets++;
			vc->ibytes += m0->m_pkthdr.len;

			atm_input(ifp, &aph, m0, vc->rxhand);
		}

		H_SETSTAT(q->q.statp, FATM_STAT_FREE);
		H_SYNCSTAT_PREWRITE(sc, q->q.statp);

		WRITE4(sc, q->q.card, q->q.card_ioblk);
		BARRIER_W(sc);

		NEXT_QUEUE_ENTRY(sc->rxqueue.tail, FATM_RX_QLEN);
	}
}

/*
 * Check the transmit queue. Free the mbuf chains that we were transmitting.
 */
static void
fatm_intr_drain_tx(struct fatm_softc *sc)
{
	struct txqueue *q;
	int stat;

	/*
	 * Drain tx queue
	 */
	for (;;) {
		q = GET_QUEUE(sc->txqueue, struct txqueue, sc->txqueue.tail);

		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		stat = H_GETSTAT(q->q.statp);

		if (stat != FATM_STAT_COMPLETE &&
		    stat != (FATM_STAT_COMPLETE | FATM_STAT_ERROR) &&
		    stat != FATM_STAT_ERROR)
			break;

		H_SETSTAT(q->q.statp, FATM_STAT_FREE);
		H_SYNCSTAT_PREWRITE(sc, q->q.statp);

		bus_dmamap_sync(sc->tx_tag, q->map, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->tx_tag, q->map);

		m_freem(q->m);
		q->m = NULL;
		sc->txcnt--;

		NEXT_QUEUE_ENTRY(sc->txqueue.tail, FATM_TX_QLEN);
	}
}

/*
 * Interrupt handler
 */
static void
fatm_intr(void *p)
{
	struct fatm_softc *sc = (struct fatm_softc *)p;

	FATM_LOCK(sc);
	if (!READ4(sc, FATMO_PSR)) {
		FATM_UNLOCK(sc);
		return;
	}
	WRITE4(sc, FATMO_HCR, FATM_HCR_CLRIRQ);

	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		FATM_UNLOCK(sc);
		return;
	}
	fatm_intr_drain_cmd(sc);
	fatm_intr_drain_rx(sc);
	fatm_intr_drain_tx(sc);
	fatm_intr_drain_small_buffers(sc);
	fatm_intr_drain_large_buffers(sc);
	fatm_supply_small_buffers(sc);
	fatm_supply_large_buffers(sc);

	FATM_UNLOCK(sc);

	if (sc->retry_tx && _IF_QLEN(&sc->ifp->if_snd))
		(*sc->ifp->if_start)(sc->ifp);
}

/*
 * Get device statistics. This must be called with the softc locked.
 * We use a preallocated buffer, so we need to protect this buffer.
 * We do this by using a condition variable and a flag. If the flag is set
 * the buffer is in use by one thread (one thread is executing a GETSTAT
 * card command). In this case all other threads that are trying to get
 * statistics block on that condition variable. When the thread finishes
 * using the buffer it resets the flag and signals the condition variable. This
 * will wakeup the next thread that is waiting for the buffer. If the interface
 * is stopped the stopping function will broadcast the cv. All threads will
 * find that the interface has been stopped and return.
 *
 * Aquiring of the buffer is done by the fatm_getstat() function. The freeing
 * must be done by the caller when he has finished using the buffer.
 */
static void
fatm_getstat_complete(struct fatm_softc *sc, struct cmdqueue *q)
{

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (H_GETSTAT(q->q.statp) & FATM_STAT_ERROR) {
		sc->istats.get_stat_errors++;
		q->error = EIO;
	}
	wakeup(&sc->sadi_mem);
}
static int
fatm_getstat(struct fatm_softc *sc)
{
	int error;
	struct cmdqueue *q;

	/*
	 * Wait until either the interface is stopped or we can get the
	 * statistics buffer
	 */
	for (;;) {
		if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING))
			return (EIO);
		if (!(sc->flags & FATM_STAT_INUSE))
			break;
		cv_wait(&sc->cv_stat, &sc->mtx);
	}
	sc->flags |= FATM_STAT_INUSE;

	q = GET_QUEUE(sc->cmdqueue, struct cmdqueue, sc->cmdqueue.head);

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (!(H_GETSTAT(q->q.statp) & FATM_STAT_FREE)) {
		sc->istats.cmd_queue_full++;
		return (EIO);
	}
	NEXT_QUEUE_ENTRY(sc->cmdqueue.head, FATM_CMD_QLEN);

	q->error = 0;
	q->cb = fatm_getstat_complete;
	H_SETSTAT(q->q.statp, FATM_STAT_PENDING);
	H_SYNCSTAT_PREWRITE(sc, q->q.statp);

	bus_dmamap_sync(sc->sadi_mem.dmat, sc->sadi_mem.map,
	    BUS_DMASYNC_PREREAD);

	WRITE4(sc, q->q.card + FATMOC_GSTAT_BUF,
	    sc->sadi_mem.paddr);
	BARRIER_W(sc);
	WRITE4(sc, q->q.card + FATMOC_OP,
	    FATM_OP_REQUEST_STATS | FATM_OP_INTERRUPT_SEL);
	BARRIER_W(sc);

	/*
	 * Wait for the command to complete
	 */
	error = msleep(&sc->sadi_mem, &sc->mtx, PZERO | PCATCH,
	    "fatm_stat", hz);

	switch (error) {

	  case EWOULDBLOCK:
		error = EIO;
		break;

	  case ERESTART:
		error = EINTR;
		break;

	  case 0:
		bus_dmamap_sync(sc->sadi_mem.dmat, sc->sadi_mem.map,
		    BUS_DMASYNC_POSTREAD);
		error = q->error;
		break;
	}

	/*
	 * Swap statistics
	 */
	if (q->error == 0) {
		u_int i;
		uint32_t *p = (uint32_t *)sc->sadi_mem.mem;

		for (i = 0; i < sizeof(struct fatm_stats) / sizeof(uint32_t);
		    i++, p++)
			*p = be32toh(*p);
	}

	return (error);
}

/*
 * Create a copy of a single mbuf. It can have either internal or
 * external data, it may have a packet header. External data is really
 * copied, so the new buffer is writeable.
 */
static struct mbuf *
copy_mbuf(struct mbuf *m)
{
	struct mbuf *new;

	MGET(new, M_DONTWAIT, MT_DATA);
	if (new == NULL)
		return (NULL);

	if (m->m_flags & M_PKTHDR) {
		M_MOVE_PKTHDR(new, m);
		if (m->m_len > MHLEN) {
			MCLGET(new, M_TRYWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(new);
				return (NULL);
			}
		}
	} else {
		if (m->m_len > MLEN) {
			MCLGET(new, M_TRYWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(new);
				return (NULL);
			}
		}
	}

	bcopy(m->m_data, new->m_data, m->m_len);
	new->m_len = m->m_len;
	new->m_flags &= ~M_RDONLY;

	return (new);
}

/*
 * All segments must have a four byte aligned buffer address and a four
 * byte aligned length. Step through an mbuf chain and check these conditions.
 * If the buffer address is not aligned and this is a normal mbuf, move
 * the data down. Else make a copy of the mbuf with aligned data.
 * If the buffer length is not aligned steel data from the next mbuf.
 * We don't need to check whether this has more than one external reference,
 * because steeling data doesn't change the external cluster.
 * If the last mbuf is not aligned, fill with zeroes.
 *
 * Return packet length (well we should have this in the packet header),
 * but be careful not to count the zero fill at the end.
 *
 * If fixing fails free the chain and zero the pointer.
 *
 * We assume, that aligning the virtual address also aligns the mapped bus
 * address.
 */
static u_int
fatm_fix_chain(struct fatm_softc *sc, struct mbuf **mp)
{
	struct mbuf *m = *mp, *prev = NULL, *next, *new;
	u_int mlen = 0, fill = 0;	
	int first, off;
	u_char *d, *cp;

	do {
		next = m->m_next;

		if ((uintptr_t)mtod(m, void *) % 4 != 0 ||
		   (m->m_len % 4 != 0 && next)) {
			/*
			 * Needs fixing
			 */
			first = (m == *mp);

			d = mtod(m, u_char *);
			if ((off = (uintptr_t)(void *)d % 4) != 0) {
				if (M_WRITABLE(m)) {
					sc->istats.fix_addr_copy++;
					bcopy(d, d - off, m->m_len);
					m->m_data = (caddr_t)(d - off);
				} else {
					if ((new = copy_mbuf(m)) == NULL) {
						sc->istats.fix_addr_noext++;
						goto fail;
					}
					sc->istats.fix_addr_ext++;
					if (prev)
						prev->m_next = new;
					new->m_next = next;
					m_free(m);
					m = new;
				}
			}

			if ((off = m->m_len % 4) != 0) {
				if (!M_WRITABLE(m)) {
					if ((new = copy_mbuf(m)) == NULL) {
						sc->istats.fix_len_noext++;
						goto fail;
					}
					sc->istats.fix_len_copy++;
					if (prev)
						prev->m_next = new;
					new->m_next = next;
					m_free(m);
					m = new;
				} else
					sc->istats.fix_len++;
				d = mtod(m, u_char *) + m->m_len;
				off = 4 - off;
				while (off) {
					if (next == NULL) {
						*d++ = 0;
						fill++;
					} else if (next->m_len == 0) {
						sc->istats.fix_empty++;
						next = m_free(next);
						continue;
					} else {
						cp = mtod(next, u_char *);
						*d++ = *cp++;
						next->m_len--;
						next->m_data = (caddr_t)cp;
					}
					off--;
					m->m_len++;
				}
			}

			if (first)
				*mp = m;
		}

		mlen += m->m_len;
		prev = m;
	} while ((m = next) != NULL);

	return (mlen - fill);

  fail:
	m_freem(*mp);
	*mp = NULL;
	return (0);
}

/*
 * The helper function is used to load the computed physical addresses
 * into the transmit descriptor.
 */
static void
fatm_tpd_load(void *varg, bus_dma_segment_t *segs, int nsegs,
    bus_size_t mapsize, int error)
{
	struct tpd *tpd = varg;

	if (error)
		return;

	KASSERT(nsegs <= TPD_EXTENSIONS + TXD_FIXED, ("too many segments"));

	tpd->spec = 0;
	while (nsegs--) {
		H_SETDESC(tpd->segment[tpd->spec].buffer, segs->ds_addr);
		H_SETDESC(tpd->segment[tpd->spec].length, segs->ds_len);
		tpd->spec++;
		segs++;
	}
}

/*
 * Start output.
 *
 * Note, that we update the internal statistics without the lock here.
 */
static int
fatm_tx(struct fatm_softc *sc, struct mbuf *m, struct card_vcc *vc, u_int mlen)
{
	struct txqueue *q;
	u_int nblks;
	int error, aal, nsegs;
	struct tpd *tpd;

	/*
	 * Get a queue element.
	 * If there isn't one - try to drain the transmit queue
	 * We used to sleep here if that doesn't help, but we
	 * should not sleep here, because we are called with locks.
	 */
	q = GET_QUEUE(sc->txqueue, struct txqueue, sc->txqueue.head);

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (H_GETSTAT(q->q.statp) != FATM_STAT_FREE) {
		fatm_intr_drain_tx(sc);
		H_SYNCSTAT_POSTREAD(sc, q->q.statp);
		if (H_GETSTAT(q->q.statp) != FATM_STAT_FREE) {
			if (sc->retry_tx) {
				sc->istats.tx_retry++;
				IF_PREPEND(&sc->ifp->if_snd, m);
				return (1);
			}
			sc->istats.tx_queue_full++;
			m_freem(m);
			return (0);
		}
		sc->istats.tx_queue_almost_full++;
	}

	tpd = q->q.ioblk;

	m->m_data += sizeof(struct atm_pseudohdr);
	m->m_len -= sizeof(struct atm_pseudohdr);

#ifdef ENABLE_BPF
	if (!(vc->param.flags & ATMIO_FLAG_NG) &&
	    vc->param.aal == ATMIO_AAL_5 &&
	    (vc->param.flags & ATM_PH_LLCSNAP))
		BPF_MTAP(sc->ifp, m);
#endif

	/* map the mbuf */
	error = bus_dmamap_load_mbuf(sc->tx_tag, q->map, m,
	    fatm_tpd_load, tpd, BUS_DMA_NOWAIT);
	if(error) {
		sc->ifp->if_oerrors++;
		if_printf(sc->ifp, "mbuf loaded error=%d\n", error);
		m_freem(m);
		return (0);
	}
	nsegs = tpd->spec;

	bus_dmamap_sync(sc->tx_tag, q->map, BUS_DMASYNC_PREWRITE);

	/*
	 * OK. Now go and do it.
	 */
	aal = (vc->param.aal == ATMIO_AAL_5) ? 5 : 0;

	H_SETSTAT(q->q.statp, FATM_STAT_PENDING);
	H_SYNCSTAT_PREWRITE(sc, q->q.statp);
	q->m = m;

	/*
	 * If the transmit queue is almost full, schedule a
	 * transmit interrupt so that transmit descriptors can
	 * be recycled.
	 */
	H_SETDESC(tpd->spec, TDX_MKSPEC((sc->txcnt >=
	    (4 * FATM_TX_QLEN) / 5), aal, nsegs, mlen));
	H_SETDESC(tpd->atm_header, TDX_MKHDR(vc->param.vpi,
	    vc->param.vci, 0, 0));

	if (vc->param.traffic == ATMIO_TRAFFIC_UBR)
		H_SETDESC(tpd->stream, 0);
	else {
		u_int i;

		for (i = 0; i < RATE_TABLE_SIZE; i++)
			if (rate_table[i].cell_rate < vc->param.tparam.pcr)
				break;
		if (i > 0)
			i--;
		H_SETDESC(tpd->stream, rate_table[i].ratio);
	}
	H_SYNCQ_PREWRITE(&sc->txq_mem, tpd, TPD_SIZE);

	nblks = TDX_SEGS2BLKS(nsegs);

	DBG(sc, XMIT, ("XMIT: mlen=%d spec=0x%x nsegs=%d blocks=%d",
	    mlen, le32toh(tpd->spec), nsegs, nblks));

	WRITE4(sc, q->q.card + 0, q->q.card_ioblk | nblks);
	BARRIER_W(sc);

	sc->txcnt++;
	sc->ifp->if_opackets++;
	vc->obytes += m->m_pkthdr.len;
	vc->opackets++;

	NEXT_QUEUE_ENTRY(sc->txqueue.head, FATM_TX_QLEN);

	return (0);
}

static void
fatm_start(struct ifnet *ifp)
{
	struct atm_pseudohdr aph;
	struct fatm_softc *sc;
	struct mbuf *m;
	u_int mlen, vpi, vci;
	struct card_vcc *vc;

	sc = ifp->if_softc;

	while (1) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		/*
		 * Loop through the mbuf chain and compute the total length
		 * of the packet. Check that all data pointer are
		 * 4 byte aligned. If they are not, call fatm_mfix to
		 * fix that problem. This comes more or less from the
		 * en driver.
		 */
		mlen = fatm_fix_chain(sc, &m);
		if (m == NULL)
			continue;

		if (m->m_len < sizeof(struct atm_pseudohdr) &&
		    (m = m_pullup(m, sizeof(struct atm_pseudohdr))) == NULL)
			continue;

		aph = *mtod(m, struct atm_pseudohdr *);
		mlen -= sizeof(struct atm_pseudohdr);

		if (mlen == 0) {
			m_freem(m);
			continue;
		}
		if (mlen > FATM_MAXPDU) {
			sc->istats.tx_pdu2big++;
			m_freem(m);
			continue;
		}

		vci = ATM_PH_VCI(&aph);
		vpi = ATM_PH_VPI(&aph);

		/*
		 * From here on we need the softc
		 */
		FATM_LOCK(sc);
		if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			FATM_UNLOCK(sc);
			m_freem(m);
			break;
		}
		if (!VC_OK(sc, vpi, vci) || (vc = sc->vccs[vci]) == NULL ||
		    !(vc->vflags & FATM_VCC_OPEN)) {
			FATM_UNLOCK(sc);
			m_freem(m);
			continue;
		}
		if (fatm_tx(sc, m, vc, mlen)) {
			FATM_UNLOCK(sc);
			break;
		}
		FATM_UNLOCK(sc);
	}
}

/*
 * VCC managment
 *
 * This may seem complicated. The reason for this is, that we need an
 * asynchronuous open/close for the NATM VCCs because our ioctl handler
 * is called with the radix node head of the routing table locked. Therefor
 * we cannot sleep there and wait for the open/close to succeed. For this
 * reason we just initiate the operation from the ioctl.
 */

/*
 * Command the card to open/close a VC.
 * Return the queue entry for waiting if we are succesful.
 */
static struct cmdqueue *
fatm_start_vcc(struct fatm_softc *sc, u_int vpi, u_int vci, uint32_t cmd,
    u_int mtu, void (*func)(struct fatm_softc *, struct cmdqueue *))
{
	struct cmdqueue *q;

	q = GET_QUEUE(sc->cmdqueue, struct cmdqueue, sc->cmdqueue.head);

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (!(H_GETSTAT(q->q.statp) & FATM_STAT_FREE)) {
		sc->istats.cmd_queue_full++;
		return (NULL);
	}
	NEXT_QUEUE_ENTRY(sc->cmdqueue.head, FATM_CMD_QLEN);

	q->error = 0;
	q->cb = func;
	H_SETSTAT(q->q.statp, FATM_STAT_PENDING);
	H_SYNCSTAT_PREWRITE(sc, q->q.statp);

	WRITE4(sc, q->q.card + FATMOC_ACTIN_VPVC, MKVPVC(vpi, vci));
	BARRIER_W(sc);
	WRITE4(sc, q->q.card + FATMOC_ACTIN_MTU, mtu);
	BARRIER_W(sc);
	WRITE4(sc, q->q.card + FATMOC_OP, cmd);
	BARRIER_W(sc);

	return (q);
}

/*
 * The VC has been opened/closed and somebody has been waiting for this.
 * Wake him up.
 */
static void
fatm_cmd_complete(struct fatm_softc *sc, struct cmdqueue *q)
{

	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (H_GETSTAT(q->q.statp) & FATM_STAT_ERROR) {
		sc->istats.get_stat_errors++;
		q->error = EIO;
	}
	wakeup(q);
}

/*
 * Open complete
 */
static void
fatm_open_finish(struct fatm_softc *sc, struct card_vcc *vc)
{
	vc->vflags &= ~FATM_VCC_TRY_OPEN;
	vc->vflags |= FATM_VCC_OPEN;

	if (vc->vflags & FATM_VCC_REOPEN) {
		vc->vflags &= ~FATM_VCC_REOPEN;
		return;
	}

	/* inform management if this is not an NG
	 * VCC or it's an NG PVC. */
	if (!(vc->param.flags & ATMIO_FLAG_NG) ||
	    (vc->param.flags & ATMIO_FLAG_PVC))
		ATMEV_SEND_VCC_CHANGED(IFP2IFATM(sc->ifp), 0, vc->param.vci, 1);
}

/*
 * The VC that we have tried to open asynchronuosly has been opened.
 */
static void
fatm_open_complete(struct fatm_softc *sc, struct cmdqueue *q)
{
	u_int vci;
	struct card_vcc *vc;

	vci = GETVCI(READ4(sc, q->q.card + FATMOC_ACTIN_VPVC));
	vc = sc->vccs[vci];
	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (H_GETSTAT(q->q.statp) & FATM_STAT_ERROR) {
		sc->istats.get_stat_errors++;
		sc->vccs[vci] = NULL;
		uma_zfree(sc->vcc_zone, vc);
		if_printf(sc->ifp, "opening VCI %u failed\n", vci);
		return;
	}
	fatm_open_finish(sc, vc);
}

/*
 * Wait on the queue entry until the VCC is opened/closed.
 */
static int
fatm_waitvcc(struct fatm_softc *sc, struct cmdqueue *q)
{
	int error;

	/*
	 * Wait for the command to complete
	 */
	error = msleep(q, &sc->mtx, PZERO | PCATCH, "fatm_vci", hz);

	if (error != 0)
		return (error);
	return (q->error);
}

/*
 * Start to open a VCC. This just initiates the operation.
 */
static int
fatm_open_vcc(struct fatm_softc *sc, struct atmio_openvcc *op)
{
	int error;
	struct card_vcc *vc;

	/*
	 * Check parameters
	 */
	if ((op->param.flags & ATMIO_FLAG_NOTX) &&
	    (op->param.flags & ATMIO_FLAG_NORX))
		return (EINVAL);

	if (!VC_OK(sc, op->param.vpi, op->param.vci))
		return (EINVAL);
	if (op->param.aal != ATMIO_AAL_0 && op->param.aal != ATMIO_AAL_5)
		return (EINVAL);

	vc = uma_zalloc(sc->vcc_zone, M_NOWAIT | M_ZERO);
	if (vc == NULL)
		return (ENOMEM);

	error = 0;

	FATM_LOCK(sc);
	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		error = EIO;
		goto done;
	}
	if (sc->vccs[op->param.vci] != NULL) {
		error = EBUSY;
		goto done;
	}
	vc->param = op->param;
	vc->rxhand = op->rxhand;

	switch (op->param.traffic) {

	  case ATMIO_TRAFFIC_UBR:
		break;

	  case ATMIO_TRAFFIC_CBR:
		if (op->param.tparam.pcr == 0 ||
		    op->param.tparam.pcr > IFP2IFATM(sc->ifp)->mib.pcr) {
			error = EINVAL;
			goto done;
		}
		break;

	  default:
		error = EINVAL;
		goto done;
	}
	vc->ibytes = vc->obytes = 0;
	vc->ipackets = vc->opackets = 0;

	vc->vflags = FATM_VCC_TRY_OPEN;
	sc->vccs[op->param.vci] = vc;
	sc->open_vccs++;

	error = fatm_load_vc(sc, vc);
	if (error != 0) {
		sc->vccs[op->param.vci] = NULL;
		sc->open_vccs--;
		goto done;
	}

	/* don't free below */
	vc = NULL;

  done:
	FATM_UNLOCK(sc);
	if (vc != NULL)
		uma_zfree(sc->vcc_zone, vc);
	return (error);
}

/*
 * Try to initialize the given VC
 */
static int
fatm_load_vc(struct fatm_softc *sc, struct card_vcc *vc)
{
	uint32_t cmd;
	struct cmdqueue *q;
	int error;

	/* Command and buffer strategy */
	cmd = FATM_OP_ACTIVATE_VCIN | FATM_OP_INTERRUPT_SEL | (0 << 16);
	if (vc->param.aal == ATMIO_AAL_0)
		cmd |= (0 << 8);
	else
		cmd |= (5 << 8);

	q = fatm_start_vcc(sc, vc->param.vpi, vc->param.vci, cmd, 1,
	    (vc->param.flags & ATMIO_FLAG_ASYNC) ?
	    fatm_open_complete : fatm_cmd_complete);
	if (q == NULL)
		return (EIO);

	if (!(vc->param.flags & ATMIO_FLAG_ASYNC)) {
		error = fatm_waitvcc(sc, q);
		if (error != 0)
			return (error);
		fatm_open_finish(sc, vc);
	}
	return (0);
}

/*
 * Finish close
 */
static void
fatm_close_finish(struct fatm_softc *sc, struct card_vcc *vc)
{
	/* inform management of this is not an NG
	 * VCC or it's an NG PVC. */
	if (!(vc->param.flags & ATMIO_FLAG_NG) ||
	    (vc->param.flags & ATMIO_FLAG_PVC))
		ATMEV_SEND_VCC_CHANGED(IFP2IFATM(sc->ifp), 0, vc->param.vci, 0);

	sc->vccs[vc->param.vci] = NULL;
	sc->open_vccs--;

	uma_zfree(sc->vcc_zone, vc);
}

/*
 * The VC has been closed.
 */
static void
fatm_close_complete(struct fatm_softc *sc, struct cmdqueue *q)
{
	u_int vci;
	struct card_vcc *vc;

	vci = GETVCI(READ4(sc, q->q.card + FATMOC_ACTIN_VPVC));
	vc = sc->vccs[vci];
	H_SYNCSTAT_POSTREAD(sc, q->q.statp);
	if (H_GETSTAT(q->q.statp) & FATM_STAT_ERROR) {
		sc->istats.get_stat_errors++;
		/* keep the VCC in that state */
		if_printf(sc->ifp, "closing VCI %u failed\n", vci);
		return;
	}

	fatm_close_finish(sc, vc);
}

/*
 * Initiate closing a VCC
 */
static int
fatm_close_vcc(struct fatm_softc *sc, struct atmio_closevcc *cl)
{
	int error;
	struct cmdqueue *q;
	struct card_vcc *vc;

	if (!VC_OK(sc, cl->vpi, cl->vci))
		return (EINVAL);

	error = 0;

	FATM_LOCK(sc);
	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		error = EIO;
		goto done;
	}
	vc = sc->vccs[cl->vci];
	if (vc == NULL || !(vc->vflags & (FATM_VCC_OPEN | FATM_VCC_TRY_OPEN))) {
		error = ENOENT;
		goto done;
	}

	q = fatm_start_vcc(sc, cl->vpi, cl->vci, 
	    FATM_OP_DEACTIVATE_VCIN | FATM_OP_INTERRUPT_SEL, 1,
	    (vc->param.flags & ATMIO_FLAG_ASYNC) ?
	    fatm_close_complete : fatm_cmd_complete);
	if (q == NULL) {
		error = EIO;
		goto done;
	}

	vc->vflags &= ~(FATM_VCC_OPEN | FATM_VCC_TRY_OPEN);
	vc->vflags |= FATM_VCC_TRY_CLOSE;

	if (!(vc->param.flags & ATMIO_FLAG_ASYNC)) {
		error = fatm_waitvcc(sc, q);
		if (error != 0)
			goto done;

		fatm_close_finish(sc, vc);
	}

  done:
	FATM_UNLOCK(sc);
	return (error);
}

/*
 * IOCTL handler
 */
static int
fatm_ioctl(struct ifnet *ifp, u_long cmd, caddr_t arg)
{
	int error;
	struct fatm_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)arg;
	struct ifreq *ifr = (struct ifreq *)arg;
	struct atmio_closevcc *cl = (struct atmio_closevcc *)arg;
	struct atmio_openvcc *op = (struct atmio_openvcc *)arg;
	struct atmio_vcctable *vtab;

	error = 0;
	switch (cmd) {

	  case SIOCATMOPENVCC:		/* kernel internal use */
		error = fatm_open_vcc(sc, op);
		break;

	  case SIOCATMCLOSEVCC:		/* kernel internal use */
		error = fatm_close_vcc(sc, cl);
		break;

	  case SIOCSIFADDR:
		FATM_LOCK(sc);
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
			fatm_init_locked(sc);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		  case AF_INET:
		  case AF_INET6:
			ifa->ifa_rtrequest = atm_rtrequest;
			break;
#endif
		  default:
			break;
		}
		FATM_UNLOCK(sc);
		break;

	  case SIOCSIFFLAGS:
		FATM_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				fatm_init_locked(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				fatm_stop(sc);
			}
		}
		FATM_UNLOCK(sc);
		break;

	  case SIOCGIFMEDIA:
	  case SIOCSIFMEDIA:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			error = ifmedia_ioctl(ifp, ifr, &sc->media, cmd);
		else
			error = EINVAL;
		break;

	  case SIOCATMGVCCS:
		/* return vcc table */
		vtab = atm_getvccs((struct atmio_vcc **)sc->vccs,
		    FORE_MAX_VCC + 1, sc->open_vccs, &sc->mtx, 1);
		error = copyout(vtab, ifr->ifr_data, sizeof(*vtab) +
		    vtab->count * sizeof(vtab->vccs[0]));
		free(vtab, M_DEVBUF);
		break;

	  case SIOCATMGETVCCS:	/* internal netgraph use */
		vtab = atm_getvccs((struct atmio_vcc **)sc->vccs,
		    FORE_MAX_VCC + 1, sc->open_vccs, &sc->mtx, 0);
		if (vtab == NULL) {
			error = ENOMEM;
			break;
		}
		*(void **)arg = vtab;
		break;

	  default:
		DBG(sc, IOCTL, ("+++ cmd=%08lx arg=%p", cmd, arg));
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * Detach from the interface and free all resources allocated during
 * initialisation and later.
 */
static int
fatm_detach(device_t dev)
{
	u_int i;
	struct rbuf *rb;
	struct fatm_softc *sc;
	struct txqueue *tx;

	sc = device_get_softc(dev);

	if (device_is_alive(dev)) {
		FATM_LOCK(sc);
		fatm_stop(sc);
		utopia_detach(&sc->utopia);
		FATM_UNLOCK(sc);
		atm_ifdetach(sc->ifp);		/* XXX race */
	}

	if (sc->ih != NULL)
		bus_teardown_intr(dev, sc->irqres, sc->ih);

	while ((rb = LIST_FIRST(&sc->rbuf_used)) != NULL) {
		if_printf(sc->ifp, "rbuf %p still in use!\n", rb);
		bus_dmamap_unload(sc->rbuf_tag, rb->map);
		m_freem(rb->m);
		LIST_REMOVE(rb, link);
		LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
	}

	if (sc->txqueue.chunk != NULL) {
		for (i = 0; i < FATM_TX_QLEN; i++) {
			tx = GET_QUEUE(sc->txqueue, struct txqueue, i);
			bus_dmamap_destroy(sc->tx_tag, tx->map);
		}
	}

	while ((rb = LIST_FIRST(&sc->rbuf_free)) != NULL) {
		bus_dmamap_destroy(sc->rbuf_tag, rb->map);
		LIST_REMOVE(rb, link);
	}

	if (sc->rbufs != NULL)
		free(sc->rbufs, M_DEVBUF);
	if (sc->vccs != NULL) {
		for (i = 0; i < FORE_MAX_VCC + 1; i++)
			if (sc->vccs[i] != NULL) {
				uma_zfree(sc->vcc_zone, sc->vccs[i]);
				sc->vccs[i] = NULL;
			}
		free(sc->vccs, M_DEVBUF);
	}
	if (sc->vcc_zone != NULL)
		uma_zdestroy(sc->vcc_zone);

	if (sc->l1queue.chunk != NULL)
		free(sc->l1queue.chunk, M_DEVBUF);
	if (sc->s1queue.chunk != NULL)
		free(sc->s1queue.chunk, M_DEVBUF);
	if (sc->rxqueue.chunk != NULL)
		free(sc->rxqueue.chunk, M_DEVBUF);
	if (sc->txqueue.chunk != NULL)
		free(sc->txqueue.chunk, M_DEVBUF);
	if (sc->cmdqueue.chunk != NULL)
		free(sc->cmdqueue.chunk, M_DEVBUF);

	destroy_dma_memory(&sc->reg_mem);
	destroy_dma_memory(&sc->sadi_mem);
	destroy_dma_memory(&sc->prom_mem);
#ifdef TEST_DMA_SYNC
	destroy_dma_memoryX(&sc->s1q_mem);
	destroy_dma_memoryX(&sc->l1q_mem);
	destroy_dma_memoryX(&sc->rxq_mem);
	destroy_dma_memoryX(&sc->txq_mem);
	destroy_dma_memoryX(&sc->stat_mem);
#endif

	if (sc->tx_tag != NULL)
		if (bus_dma_tag_destroy(sc->tx_tag))
			printf("tx DMA tag busy!\n");

	if (sc->rbuf_tag != NULL)
		if (bus_dma_tag_destroy(sc->rbuf_tag))
			printf("rbuf DMA tag busy!\n");

	if (sc->parent_dmat != NULL)
		if (bus_dma_tag_destroy(sc->parent_dmat))
			printf("parent DMA tag busy!\n");

	if (sc->irqres != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irqres);

	if (sc->memres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->memid, sc->memres);

	(void)sysctl_ctx_free(&sc->sysctl_ctx);

	cv_destroy(&sc->cv_stat);
	cv_destroy(&sc->cv_regs);

	mtx_destroy(&sc->mtx);

	if_free(sc->ifp);

	return (0);
}

/*
 * Sysctl handler
 */
static int
fatm_sysctl_istats(SYSCTL_HANDLER_ARGS)
{
	struct fatm_softc *sc = arg1;
	u_long *ret;
	int error;

	ret = malloc(sizeof(sc->istats), M_TEMP, M_WAITOK);

	FATM_LOCK(sc);
	bcopy(&sc->istats, ret, sizeof(sc->istats));
	FATM_UNLOCK(sc);

	error = SYSCTL_OUT(req, ret, sizeof(sc->istats));
	free(ret, M_TEMP);

	return (error);
}

/*
 * Sysctl handler for card statistics
 * This is disable because it destroys the PHY statistics.
 */
static int
fatm_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	struct fatm_softc *sc = arg1;
	int error;
	const struct fatm_stats *s;
	u_long *ret;
	u_int i;

	ret = malloc(sizeof(u_long) * FATM_NSTATS, M_TEMP, M_WAITOK);

	FATM_LOCK(sc);

	if ((error = fatm_getstat(sc)) == 0) {
		s = sc->sadi_mem.mem;
		i = 0;
		ret[i++] = s->phy_4b5b.crc_header_errors;
		ret[i++] = s->phy_4b5b.framing_errors;
		ret[i++] = s->phy_oc3.section_bip8_errors;
		ret[i++] = s->phy_oc3.path_bip8_errors;
		ret[i++] = s->phy_oc3.line_bip24_errors;
		ret[i++] = s->phy_oc3.line_febe_errors;
		ret[i++] = s->phy_oc3.path_febe_errors;
		ret[i++] = s->phy_oc3.corr_hcs_errors;
		ret[i++] = s->phy_oc3.ucorr_hcs_errors;
		ret[i++] = s->atm.cells_transmitted;
		ret[i++] = s->atm.cells_received;
		ret[i++] = s->atm.vpi_bad_range;
		ret[i++] = s->atm.vpi_no_conn;
		ret[i++] = s->atm.vci_bad_range;
		ret[i++] = s->atm.vci_no_conn;
		ret[i++] = s->aal0.cells_transmitted;
		ret[i++] = s->aal0.cells_received;
		ret[i++] = s->aal0.cells_dropped;
		ret[i++] = s->aal4.cells_transmitted;
		ret[i++] = s->aal4.cells_received;
		ret[i++] = s->aal4.cells_crc_errors;
		ret[i++] = s->aal4.cels_protocol_errors;
		ret[i++] = s->aal4.cells_dropped;
		ret[i++] = s->aal4.cspdus_transmitted;
		ret[i++] = s->aal4.cspdus_received;
		ret[i++] = s->aal4.cspdus_protocol_errors;
		ret[i++] = s->aal4.cspdus_dropped;
		ret[i++] = s->aal5.cells_transmitted;
		ret[i++] = s->aal5.cells_received;
		ret[i++] = s->aal5.congestion_experienced;
		ret[i++] = s->aal5.cells_dropped;
		ret[i++] = s->aal5.cspdus_transmitted;
		ret[i++] = s->aal5.cspdus_received;
		ret[i++] = s->aal5.cspdus_crc_errors;
		ret[i++] = s->aal5.cspdus_protocol_errors;
		ret[i++] = s->aal5.cspdus_dropped;
		ret[i++] = s->aux.small_b1_failed;
		ret[i++] = s->aux.large_b1_failed;
		ret[i++] = s->aux.small_b2_failed;
		ret[i++] = s->aux.large_b2_failed;
		ret[i++] = s->aux.rpd_alloc_failed;
		ret[i++] = s->aux.receive_carrier;
	}
	/* declare the buffer free */
	sc->flags &= ~FATM_STAT_INUSE;
	cv_signal(&sc->cv_stat);

	FATM_UNLOCK(sc);

	if (error == 0)
		error = SYSCTL_OUT(req, ret, sizeof(u_long) * FATM_NSTATS);
	free(ret, M_TEMP);

	return (error);
}

#define MAXDMASEGS 32		/* maximum number of receive descriptors */

/*
 * Attach to the device.
 *
 * We assume, that there is a global lock (Giant in this case) that protects
 * multiple threads from entering this function. This makes sense, doesn't it?
 */
static int
fatm_attach(device_t dev)
{
	struct ifnet *ifp;
	struct fatm_softc *sc;
	int unit;
	uint16_t cfg;
	int error = 0;
	struct rbuf *rb;
	u_int i;
	struct txqueue *tx;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	ifp = sc->ifp = if_alloc(IFT_ATM);
	if (ifp == NULL) {
		error = ENOSPC;
		goto fail;
	}

	IFP2IFATM(sc->ifp)->mib.device = ATM_DEVICE_PCA200E;
	IFP2IFATM(sc->ifp)->mib.serial = 0;
	IFP2IFATM(sc->ifp)->mib.hw_version = 0;
	IFP2IFATM(sc->ifp)->mib.sw_version = 0;
	IFP2IFATM(sc->ifp)->mib.vpi_bits = 0;
	IFP2IFATM(sc->ifp)->mib.vci_bits = FORE_VCIBITS;
	IFP2IFATM(sc->ifp)->mib.max_vpcs = 0;
	IFP2IFATM(sc->ifp)->mib.max_vccs = FORE_MAX_VCC;
	IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_UNKNOWN;
	IFP2IFATM(sc->ifp)->phy = &sc->utopia;

	LIST_INIT(&sc->rbuf_free);
	LIST_INIT(&sc->rbuf_used);

	/*
	 * Initialize mutex and condition variables.
	 */
	mtx_init(&sc->mtx, device_get_nameunit(dev),
	    MTX_NETWORK_LOCK, MTX_DEF);

	cv_init(&sc->cv_stat, "fatm_stat");
	cv_init(&sc->cv_regs, "fatm_regs");

	sysctl_ctx_init(&sc->sysctl_ctx);

	/*
	 * Make the sysctl tree
	 */
	if ((sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw_atm), OID_AUTO,
	    device_get_nameunit(dev), CTLFLAG_RD, 0, "")) == NULL)
		goto fail;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "istats", CTLFLAG_RD, sc, 0, fatm_sysctl_istats,
	    "LU", "internal statistics") == NULL)
		goto fail;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "stats", CTLFLAG_RD, sc, 0, fatm_sysctl_stats,
	    "LU", "card statistics") == NULL)
		goto fail;

	if (SYSCTL_ADD_INT(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "retry_tx", CTLFLAG_RW, &sc->retry_tx, 0,
	    "retry flag") == NULL)
		goto fail;

#ifdef FATM_DEBUG
	if (SYSCTL_ADD_UINT(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "debug", CTLFLAG_RW, &sc->debug, 0, "debug flags")
	    == NULL)
		goto fail;
	sc->debug = FATM_DEBUG;
#endif

	/*
	 * Network subsystem stuff
	 */
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_SIMPLEX;
	ifp->if_ioctl = fatm_ioctl;
	ifp->if_start = fatm_start;
	ifp->if_watchdog = fatm_watchdog;
	ifp->if_init = fatm_init;
	ifp->if_linkmib = &IFP2IFATM(sc->ifp)->mib;
	ifp->if_linkmiblen = sizeof(IFP2IFATM(sc->ifp)->mib);

	/*
	 * Enable memory and bustmaster
	 */
	cfg = pci_read_config(dev, PCIR_COMMAND, 2);
	cfg |= PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, cfg, 2);

	/*
	 * Map memory
	 */
	cfg = pci_read_config(dev, PCIR_COMMAND, 2);
	if (!(cfg & PCIM_CMD_MEMEN)) {
		if_printf(ifp, "failed to enable memory mapping\n");
		error = ENXIO;
		goto fail;
	}
	sc->memid = 0x10;
	sc->memres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->memid,
	    RF_ACTIVE);
	if (sc->memres == NULL) {
		if_printf(ifp, "could not map memory\n");
		error = ENXIO;
		goto fail;
	}
	sc->memh = rman_get_bushandle(sc->memres);
	sc->memt = rman_get_bustag(sc->memres);

	/*
	 * Convert endianess of slave access
	 */
	cfg = pci_read_config(dev, FATM_PCIR_MCTL, 1);
	cfg |= FATM_PCIM_SWAB;
	pci_write_config(dev, FATM_PCIR_MCTL, cfg, 1);

	/*
	 * Allocate interrupt (activate at the end)
	 */
	sc->irqid = 0;
	sc->irqres = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irqid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irqres == NULL) {
		if_printf(ifp, "could not allocate irq\n");
		error = ENXIO;
		goto fail;
	}

	/*
	 * Allocate the parent DMA tag. This is used simply to hold overall
	 * restrictions for the controller (and PCI bus) and is never used
	 * to do anything.
	 */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, BUS_SPACE_MAXSIZE_32BIT, MAXDMASEGS,
	    BUS_SPACE_MAXSIZE_32BIT, 0, NULL, NULL,
	    &sc->parent_dmat)) {
		if_printf(ifp, "could not allocate parent DMA tag\n");
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Allocate the receive buffer DMA tag. This tag must map a maximum of
	 * a mbuf cluster.
	 */
	if (bus_dma_tag_create(sc->parent_dmat, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, MCLBYTES, 1, MCLBYTES, 0, 
	    NULL, NULL, &sc->rbuf_tag)) {
		if_printf(ifp, "could not allocate rbuf DMA tag\n");
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Allocate the transmission DMA tag. Must add 1, because
	 * rounded up PDU will be 65536 bytes long.
	 */
	if (bus_dma_tag_create(sc->parent_dmat, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    FATM_MAXPDU + 1, TPD_EXTENSIONS + TXD_FIXED, MCLBYTES, 0,
	    NULL, NULL, &sc->tx_tag)) {
		if_printf(ifp, "could not allocate tx DMA tag\n");
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Allocate DMAable memory.
	 */
	sc->stat_mem.size = sizeof(uint32_t) * (FATM_CMD_QLEN + FATM_TX_QLEN
	    + FATM_RX_QLEN + SMALL_SUPPLY_QLEN + LARGE_SUPPLY_QLEN);
	sc->stat_mem.align = 4;

	sc->txq_mem.size = FATM_TX_QLEN * TPD_SIZE;
	sc->txq_mem.align = 32;

	sc->rxq_mem.size = FATM_RX_QLEN * RPD_SIZE;
	sc->rxq_mem.align = 32;

	sc->s1q_mem.size = SMALL_SUPPLY_QLEN *
	    BSUP_BLK2SIZE(SMALL_SUPPLY_BLKSIZE);
	sc->s1q_mem.align = 32;

	sc->l1q_mem.size = LARGE_SUPPLY_QLEN *
	    BSUP_BLK2SIZE(LARGE_SUPPLY_BLKSIZE);
	sc->l1q_mem.align = 32;

#ifdef TEST_DMA_SYNC
	if ((error = alloc_dma_memoryX(sc, "STATUS", &sc->stat_mem)) != 0 ||
	    (error = alloc_dma_memoryX(sc, "TXQ", &sc->txq_mem)) != 0 ||
	    (error = alloc_dma_memoryX(sc, "RXQ", &sc->rxq_mem)) != 0 ||
	    (error = alloc_dma_memoryX(sc, "S1Q", &sc->s1q_mem)) != 0 ||
	    (error = alloc_dma_memoryX(sc, "L1Q", &sc->l1q_mem)) != 0)
		goto fail;
#else
	if ((error = alloc_dma_memory(sc, "STATUS", &sc->stat_mem)) != 0 ||
	    (error = alloc_dma_memory(sc, "TXQ", &sc->txq_mem)) != 0 ||
	    (error = alloc_dma_memory(sc, "RXQ", &sc->rxq_mem)) != 0 ||
	    (error = alloc_dma_memory(sc, "S1Q", &sc->s1q_mem)) != 0 ||
	    (error = alloc_dma_memory(sc, "L1Q", &sc->l1q_mem)) != 0)
		goto fail;
#endif

	sc->prom_mem.size = sizeof(struct prom);
	sc->prom_mem.align = 32;
	if ((error = alloc_dma_memory(sc, "PROM", &sc->prom_mem)) != 0)
		goto fail;

	sc->sadi_mem.size = sizeof(struct fatm_stats);
	sc->sadi_mem.align = 32;
	if ((error = alloc_dma_memory(sc, "STATISTICS", &sc->sadi_mem)) != 0)
		goto fail;

	sc->reg_mem.size = sizeof(uint32_t) * FATM_NREGS;
	sc->reg_mem.align = 32;
	if ((error = alloc_dma_memory(sc, "REGISTERS", &sc->reg_mem)) != 0)
		goto fail;

	/*
	 * Allocate queues
	 */
	sc->cmdqueue.chunk = malloc(FATM_CMD_QLEN * sizeof(struct cmdqueue),
	    M_DEVBUF, M_ZERO | M_WAITOK);
	sc->txqueue.chunk = malloc(FATM_TX_QLEN * sizeof(struct txqueue),
	    M_DEVBUF, M_ZERO | M_WAITOK);
	sc->rxqueue.chunk = malloc(FATM_RX_QLEN * sizeof(struct rxqueue),
	    M_DEVBUF, M_ZERO | M_WAITOK);
	sc->s1queue.chunk = malloc(SMALL_SUPPLY_QLEN * sizeof(struct supqueue),
	    M_DEVBUF, M_ZERO | M_WAITOK);
	sc->l1queue.chunk = malloc(LARGE_SUPPLY_QLEN * sizeof(struct supqueue),
	    M_DEVBUF, M_ZERO | M_WAITOK);

	sc->vccs = malloc((FORE_MAX_VCC + 1) * sizeof(sc->vccs[0]),
	    M_DEVBUF, M_ZERO | M_WAITOK);
	sc->vcc_zone = uma_zcreate("FATM vccs", sizeof(struct card_vcc),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (sc->vcc_zone == NULL) {
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Allocate memory for the receive buffer headers. The total number
	 * of headers should probably also include the maximum number of
	 * buffers on the receive queue.
	 */
	sc->rbuf_total = SMALL_POOL_SIZE + LARGE_POOL_SIZE;
	sc->rbufs = malloc(sc->rbuf_total * sizeof(struct rbuf),
	    M_DEVBUF, M_ZERO | M_WAITOK);

	/*
	 * Put all rbuf headers on the free list and create DMA maps.
	 */
	for (rb = sc->rbufs, i = 0; i < sc->rbuf_total; i++, rb++) {
		if ((error = bus_dmamap_create(sc->rbuf_tag, 0, &rb->map))) {
			if_printf(sc->ifp, "creating rx map: %d\n",
			    error);
			goto fail;
		}
		LIST_INSERT_HEAD(&sc->rbuf_free, rb, link);
	}

	/*
	 * Create dma maps for transmission. In case of an error, free the
	 * allocated DMA maps, because on some architectures maps are NULL
	 * and we cannot distinguish between a failure and a NULL map in
	 * the detach routine.
	 */
	for (i = 0; i < FATM_TX_QLEN; i++) {
		tx = GET_QUEUE(sc->txqueue, struct txqueue, i);
		if ((error = bus_dmamap_create(sc->tx_tag, 0, &tx->map))) {
			if_printf(sc->ifp, "creating tx map: %d\n",
			    error);
			while (i > 0) {
				tx = GET_QUEUE(sc->txqueue, struct txqueue,
				    i - 1);
				bus_dmamap_destroy(sc->tx_tag, tx->map);
				i--;
			}
			goto fail;
		}
	}

	utopia_attach(&sc->utopia, IFP2IFATM(sc->ifp), &sc->media, &sc->mtx,
	    &sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    &fatm_utopia_methods);
	sc->utopia.flags |= UTP_FL_NORESET | UTP_FL_POLL_CARRIER;

	/*
	 * Attach the interface
	 */
	atm_ifattach(ifp);
	ifp->if_snd.ifq_maxlen = 512;

#ifdef ENABLE_BPF
	bpfattach(ifp, DLT_ATM_RFC1483, sizeof(struct atmllc));
#endif

	error = bus_setup_intr(dev, sc->irqres, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, fatm_intr, sc, &sc->ih);
	if (error) {
		if_printf(ifp, "couldn't setup irq\n");
		goto fail;
	}

  fail:
	if (error)
		fatm_detach(dev);

	return (error);
}

#if defined(FATM_DEBUG) && 0
static void
dump_s1_queue(struct fatm_softc *sc)
{
	int i;
	struct supqueue *q;

	for(i = 0; i < SMALL_SUPPLY_QLEN; i++) {
		q = GET_QUEUE(sc->s1queue, struct supqueue, i);
		printf("%2d: card=%x(%x,%x) stat=%x\n", i,
		    q->q.card,
		    READ4(sc, q->q.card),
		    READ4(sc, q->q.card + 4),
		    *q->q.statp);
	}
}
#endif

/*
 * Driver infrastructure.
 */
static device_method_t fatm_methods[] = {
	DEVMETHOD(device_probe,		fatm_probe),
	DEVMETHOD(device_attach,	fatm_attach),
	DEVMETHOD(device_detach,	fatm_detach),
	{ 0, 0 }
};
static driver_t fatm_driver = {
	"fatm",
	fatm_methods,
	sizeof(struct fatm_softc),
};

DRIVER_MODULE(fatm, pci, fatm_driver, fatm_devclass, 0, 0);
