/*-
 * Copyright (c) 2006 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

/*-
 * Copyright (c) 2001-2005, Intel Corporation.
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
 * 3. Neither the name of the Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
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

/*
 * Intel XScale Queue Manager support.
 *
 * Each IXP4XXX device has a hardware block that implements a priority
 * queue manager that is shared between the XScale cpu and the backend
 * devices (such as the NPE).  Queues are accessed by reading/writing
 * special memory locations.  The queue contents are mapped into a shared
 * SRAM region with entries managed in a circular buffer.  The XScale
 * processor can receive interrupts based on queue contents (a condition
 * code determines when interrupts should be delivered).
 *
 * The code here basically replaces the qmgr class in the Intel Access
 * Library (IAL).
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>
#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#include <arm/xscale/ixp425/ixp425_qmgr.h>

/*
 * State per AQM hw queue.
 * This structure holds q configuration and dispatch state.
 */
struct qmgrInfo {
	int		qSizeInWords;		/* queue size in words */

	uint32_t	qOflowStatBitMask;	/* overflow status mask */
	int		qWriteCount;		/* queue write count */

	bus_size_t	qAccRegAddr;		/* access register */
	bus_size_t	qUOStatRegAddr;		/* status register */
	bus_size_t	qConfigRegAddr;		/* config register */
	int		qSizeInEntries;		/* queue size in entries */

	uint32_t	qUflowStatBitMask;	/* underflow status mask */
	int		qReadCount;		/* queue read count */

	/* XXX union */
	uint32_t	qStatRegAddr;
	uint32_t	qStatBitsOffset;
	uint32_t	qStat0BitMask;
	uint32_t	qStat1BitMask;

	uint32_t	intRegCheckMask;	/* interrupt reg check mask */
	void		(*cb)(int, void *);	/* callback function */
	void		*cbarg;			/* callback argument */
	int 		priority;		/* dispatch priority */
#if 0
	/* NB: needed only for A0 parts */
	u_int		statusWordOffset;	/* status word offset */
	uint32_t	statusMask;             /* status mask */    
	uint32_t	statusCheckValue;	/* status check value */
#endif
};

struct ixpqmgr_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct resource		*sc_irq;	/* IRQ resource */
	void			*sc_ih;		/* interrupt handler */
	int			sc_rid;		/* resource id for irq */

	struct qmgrInfo		qinfo[IX_QMGR_MAX_NUM_QUEUES];
	/*
	 * This array contains a list of queue identifiers ordered by
	 * priority. The table is split logically between queue
	 * identifiers 0-31 and 32-63.  To optimize lookups bit masks
	 * are kept for the first-32 and last-32 q's.  When the
	 * table needs to be rebuilt mark rebuildTable and it'll
	 * happen after the next interrupt.
	 */
	int			priorityTable[IX_QMGR_MAX_NUM_QUEUES];
	uint32_t		lowPriorityTableFirstHalfMask;
	uint32_t		uppPriorityTableFirstHalfMask;
	int			rebuildTable;	/* rebuild priorityTable */

	uint32_t		aqmFreeSramAddress;	/* SRAM free space */
};

static int qmgr_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, qmgr, CTLFLAG_RW, &qmgr_debug,
	   0, "IXP425 Q-Manager debug msgs");
TUNABLE_INT("debug.qmgr", &qmgr_debug);
#define	DPRINTF(dev, fmt, ...) do {					\
	if (qmgr_debug) printf(fmt, __VA_ARGS__);			\
} while (0)
#define	DPRINTFn(n, dev, fmt, ...) do {					\
	if (qmgr_debug >= n) printf(fmt, __VA_ARGS__);			\
} while (0)

static struct ixpqmgr_softc *ixpqmgr_sc = NULL;

static void ixpqmgr_rebuild(struct ixpqmgr_softc *);
static void ixpqmgr_intr(void *);

static void aqm_int_enable(struct ixpqmgr_softc *sc, int qId);
static void aqm_int_disable(struct ixpqmgr_softc *sc, int qId);
static void aqm_qcfg(struct ixpqmgr_softc *sc, int qId, u_int ne, u_int nf);
static void aqm_srcsel_write(struct ixpqmgr_softc *sc, int qId, int sourceId);
static void aqm_reset(struct ixpqmgr_softc *sc);

static void
dummyCallback(int qId, void *arg)
{
	/* XXX complain */
}

static uint32_t
aqm_reg_read(struct ixpqmgr_softc *sc, bus_size_t off)
{
	DPRINTFn(9, sc->sc_dev, "%s(0x%x)\n", __func__, (int)off);
	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, off);
}

static void
aqm_reg_write(struct ixpqmgr_softc *sc, bus_size_t off, uint32_t val)
{
	DPRINTFn(9, sc->sc_dev, "%s(0x%x, 0x%x)\n", __func__, (int)off, val);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, off, val);
}

static int
ixpqmgr_probe(device_t dev)
{
	device_set_desc(dev, "IXP425 Q-Manager");
	return 0;
}

static void
ixpqmgr_attach(device_t dev)
{
	struct ixpqmgr_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));
	int i;

	ixpqmgr_sc = sc;

	sc->sc_dev = dev;
	sc->sc_iot = sa->sc_iot;
	if (bus_space_map(sc->sc_iot, IXP425_QMGR_HWBASE, IXP425_QMGR_SIZE,
	    0, &sc->sc_ioh))
		panic("%s: Cannot map registers", device_get_name(dev));

	/* NB: we only use the lower 32 q's */
	sc->sc_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_rid,
	    IXP425_INT_QUE1_32, IXP425_INT_QUE33_64, 2, RF_ACTIVE);
	if (!sc->sc_irq)
		panic("Unable to allocate the qmgr irqs.\n");
	/* XXX could be a source of entropy */
	bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_NET | INTR_MPSAFE,
		NULL, ixpqmgr_intr, NULL, &sc->sc_ih);

	/* NB: softc is pre-zero'd */
	for (i = 0; i < IX_QMGR_MAX_NUM_QUEUES; i++) {
	    struct qmgrInfo *qi = &sc->qinfo[i];

	    qi->cb = dummyCallback;
	    qi->priority = IX_QMGR_Q_PRIORITY_0;	/* default priority */
	    /* 
	     * There are two interrupt registers, 32 bits each. One
	     * for the lower queues(0-31) and one for the upper
	     * queues(32-63). Therefore need to mod by 32 i.e the
	     * min upper queue identifier.
	     */
	    qi->intRegCheckMask = (1<<(i%(IX_QMGR_MIN_QUEUPP_QID)));

	    /*
	     * Register addresses and bit masks are calculated and
	     * stored here to optimize QRead, QWrite and QStatusGet
	     * functions.
	     */

	    /* AQM Queue access reg addresses, per queue */
	    qi->qAccRegAddr = IX_QMGR_Q_ACCESS_ADDR_GET(i);
	    qi->qAccRegAddr = IX_QMGR_Q_ACCESS_ADDR_GET(i);
	    qi->qConfigRegAddr = IX_QMGR_Q_CONFIG_ADDR_GET(i);

	    /* AQM Queue lower-group (0-31), only */
	    if (i < IX_QMGR_MIN_QUEUPP_QID) {
		/* AQM Q underflow/overflow status reg address, per queue */
		qi->qUOStatRegAddr = IX_QMGR_QUEUOSTAT0_OFFSET +
		    ((i / IX_QMGR_QUEUOSTAT_NUM_QUE_PER_WORD) * 
		     sizeof(uint32_t));

		/* AQM Q underflow status bit masks for status reg per queue */
		qi->qUflowStatBitMask = 
		    (IX_QMGR_UNDERFLOW_BIT_OFFSET + 1) <<
		    ((i & (IX_QMGR_QUEUOSTAT_NUM_QUE_PER_WORD - 1)) *
		     (32 / IX_QMGR_QUEUOSTAT_NUM_QUE_PER_WORD));

		/* AQM Q overflow status bit masks for status reg, per queue */
		qi->qOflowStatBitMask = 
		    (IX_QMGR_OVERFLOW_BIT_OFFSET + 1) <<
		    ((i & (IX_QMGR_QUEUOSTAT_NUM_QUE_PER_WORD - 1)) *
		     (32 / IX_QMGR_QUEUOSTAT_NUM_QUE_PER_WORD));

		/* AQM Q lower-group (0-31) status reg addresses, per queue */
		qi->qStatRegAddr = IX_QMGR_QUELOWSTAT0_OFFSET +
		    ((i / IX_QMGR_QUELOWSTAT_NUM_QUE_PER_WORD) *
		     sizeof(uint32_t));

		/* AQM Q lower-group (0-31) status register bit offset */
		qi->qStatBitsOffset =
		    (i & (IX_QMGR_QUELOWSTAT_NUM_QUE_PER_WORD - 1)) * 
		    (32 / IX_QMGR_QUELOWSTAT_NUM_QUE_PER_WORD);
	    } else { /* AQM Q upper-group (32-63), only */
		qi->qUOStatRegAddr = 0;		/* XXX */

		/* AQM Q upper-group (32-63) Nearly Empty status reg bitmasks */
		qi->qStat0BitMask = (1 << (i - IX_QMGR_MIN_QUEUPP_QID));

		/* AQM Q upper-group (32-63) Full status register bitmasks */
		qi->qStat1BitMask = (1 << (i - IX_QMGR_MIN_QUEUPP_QID));
	    }
	}
	
	sc->aqmFreeSramAddress = 0x100;	/* Q buffer space starts at 0x2100 */

	ixpqmgr_rebuild(sc);		/* build inital priority table */
	aqm_reset(sc);			/* reset h/w */
}

static void
ixpqmgr_detach(device_t dev)
{
	struct ixpqmgr_softc *sc = device_get_softc(dev);

	aqm_reset(sc);		/* disable interrupts */
	bus_teardown_intr(dev, sc->sc_irq, sc->sc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, sc->sc_rid, sc->sc_irq);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, IXP425_QMGR_SIZE);
}

int
ixpqmgr_qconfig(int qId, int qEntries, int ne, int nf, int srcSel,
    void (*cb)(int, void *), void *cbarg)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;
	struct qmgrInfo *qi = &sc->qinfo[qId];

	DPRINTF(sc->sc_dev, "%s(%u, %u, %u, %u, %u, %p, %p)\n",
	    __func__, qId, qEntries, ne, nf, srcSel, cb, cbarg);

	/* NB: entry size is always 1 */
	qi->qSizeInWords = qEntries;

	qi->qReadCount = 0;
	qi->qWriteCount = 0;
	qi->qSizeInEntries = qEntries;	/* XXX kept for code clarity */

	if (cb == NULL) {
	    /* Reset to dummy callback */
	    qi->cb = dummyCallback;
	    qi->cbarg = 0;
	} else {
	    qi->cb = cb;
	    qi->cbarg = cbarg;
	}

	/* Write the config register; NB must be AFTER qinfo setup */
	aqm_qcfg(sc, qId, ne, nf);
	/*
	 * Account for space just allocated to queue.
	 */
	sc->aqmFreeSramAddress += (qi->qSizeInWords * sizeof(uint32_t));

	/* Set the interrupt source if this queue is in the range 0-31 */
	if (qId < IX_QMGR_MIN_QUEUPP_QID)
	    aqm_srcsel_write(sc, qId, srcSel);

	if (cb != NULL)				/* Enable the interrupt */
	    aqm_int_enable(sc, qId);

	sc->rebuildTable = TRUE;

	return 0;		/* XXX */
}

int
ixpqmgr_qwrite(int qId, uint32_t entry)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;
	struct qmgrInfo *qi = &sc->qinfo[qId];

	DPRINTFn(3, sc->sc_dev, "%s(%u, 0x%x) writeCount %u size %u\n",
	    __func__, qId, entry, qi->qWriteCount, qi->qSizeInEntries);

	/* write the entry */
	aqm_reg_write(sc, qi->qAccRegAddr, entry);

	/* NB: overflow is available for lower queues only */
	if (qId < IX_QMGR_MIN_QUEUPP_QID) {
	    int qSize = qi->qSizeInEntries;
	    /*
	     * Increment the current number of entries in the queue
	     * and check for overflow .
	     */
	    if (qi->qWriteCount++ == qSize) {	/* check for overflow */
		uint32_t status = aqm_reg_read(sc, qi->qUOStatRegAddr);
		int qPtrs;

		/*
		 * Read the status twice because the status may 
		 * not be immediately ready after the write operation
		 */
		if ((status & qi->qOflowStatBitMask) ||
		    ((status = aqm_reg_read(sc, qi->qUOStatRegAddr)) & qi->qOflowStatBitMask)) {
		    /*
		     * The queue is full, clear the overflow status bit if set.
		     */
		    aqm_reg_write(sc, qi->qUOStatRegAddr,
			status & ~qi->qOflowStatBitMask);
		    qi->qWriteCount = qSize;
		    DPRINTFn(5, sc->sc_dev,
			"%s(%u, 0x%x) Q full, overflow status cleared\n",
			__func__, qId, entry);
		    return ENOSPC;
		}
		/*
		 * No overflow occured : someone is draining the queue
		 * and the current counter needs to be
		 * updated from the current number of entries in the queue
		 */

		/* calculate number of words in q */
		qPtrs = aqm_reg_read(sc, qi->qConfigRegAddr);
		DPRINTFn(2, sc->sc_dev,
		    "%s(%u, 0x%x) Q full, no overflow status, qConfig 0x%x\n",
		    __func__, qId, entry, qPtrs);
		qPtrs = (qPtrs - (qPtrs >> 7)) & 0x7f; 

		if (qPtrs == 0) {
		    /*
		     * The queue may be full at the time of the 
		     * snapshot. Next access will check 
		     * the overflow status again.
		     */
		    qi->qWriteCount = qSize;
		} else {
		    /* convert the number of words to a number of entries */
		    qi->qWriteCount = qPtrs & (qSize - 1);
		}
	    }
	}
	return 0;
}

int
ixpqmgr_qread(int qId, uint32_t *entry)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;
	struct qmgrInfo *qi = &sc->qinfo[qId];
	bus_size_t off = qi->qAccRegAddr;

	*entry = aqm_reg_read(sc, off);

	/*
	 * Reset the current read count : next access to the read function 
	 * will force a underflow status check.
	 */
	qi->qReadCount = 0;

	/* Check if underflow occurred on the read */
	if (*entry == 0 && qId < IX_QMGR_MIN_QUEUPP_QID) {
	    /* get the queue status */
	    uint32_t status = aqm_reg_read(sc, qi->qUOStatRegAddr);

	    if (status & qi->qUflowStatBitMask) { /* clear underflow status */
		aqm_reg_write(sc, qi->qUOStatRegAddr,
		    status &~ qi->qUflowStatBitMask);
		return ENOSPC;
	    }
	}
	return 0;
}

int
ixpqmgr_qreadm(int qId, uint32_t n, uint32_t *p)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;
	struct qmgrInfo *qi = &sc->qinfo[qId];
	uint32_t entry;
	bus_size_t off = qi->qAccRegAddr;

	entry = aqm_reg_read(sc, off);
	while (--n) {
	    if (entry == 0) {
		/* if we read a NULL entry, stop. We have underflowed */
		break;
	    }
	    *p++ = entry;	/* store */
	    entry = aqm_reg_read(sc, off);
	}
	*p = entry;

	/*
	 * Reset the current read count : next access to the read function 
	 * will force a underflow status check.
	 */
	qi->qReadCount = 0;

	/* Check if underflow occurred on the read */
	if (entry == 0 && qId < IX_QMGR_MIN_QUEUPP_QID) {
	    /* get the queue status */
	    uint32_t status = aqm_reg_read(sc, qi->qUOStatRegAddr);

	    if (status & qi->qUflowStatBitMask) { /* clear underflow status */
		aqm_reg_write(sc, qi->qUOStatRegAddr,
		    status &~ qi->qUflowStatBitMask);
		return ENOSPC;
	    }
	}
	return 0;
}

uint32_t
ixpqmgr_getqstatus(int qId)
{
#define	QLOWSTATMASK \
    ((1 << (32 / IX_QMGR_QUELOWSTAT_NUM_QUE_PER_WORD)) - 1)
	struct ixpqmgr_softc *sc = ixpqmgr_sc;
	const struct qmgrInfo *qi = &sc->qinfo[qId];
	uint32_t status;

	if (qId < IX_QMGR_MIN_QUEUPP_QID) {
	    /* read the status of a queue in the range 0-31 */
	    status = aqm_reg_read(sc, qi->qStatRegAddr);

	    /* mask out the status bits relevant only to this queue */
	    status = (status >> qi->qStatBitsOffset) & QLOWSTATMASK;
	} else { /* read status of a queue in the range 32-63 */
	    status = 0;
	    if (aqm_reg_read(sc, IX_QMGR_QUEUPPSTAT0_OFFSET)&qi->qStat0BitMask)
		status |= IX_QMGR_Q_STATUS_NE_BIT_MASK;	/* nearly empty */
	    if (aqm_reg_read(sc, IX_QMGR_QUEUPPSTAT1_OFFSET)&qi->qStat1BitMask)
		status |= IX_QMGR_Q_STATUS_F_BIT_MASK;	/* full */
	}
	return status;
#undef QLOWSTATMASK
}

uint32_t
ixpqmgr_getqconfig(int qId)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;

	return aqm_reg_read(sc, IX_QMGR_Q_CONFIG_ADDR_GET(qId));
}

void
ixpqmgr_dump(void)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;
	int i, a;

	/* status registers */
	printf("0x%04x: %08x %08x %08x %08x\n"
		, 0x400
		, aqm_reg_read(sc, 0x400)
		, aqm_reg_read(sc, 0x400+4)
		, aqm_reg_read(sc, 0x400+8)
		, aqm_reg_read(sc, 0x400+12)
	);
	printf("0x%04x: %08x %08x %08x %08x\n"
		, 0x410
		, aqm_reg_read(sc, 0x410)
		, aqm_reg_read(sc, 0x410+4)
		, aqm_reg_read(sc, 0x410+8)
		, aqm_reg_read(sc, 0x410+12)
	);
	printf("0x%04x: %08x %08x %08x %08x\n"
		, 0x420
		, aqm_reg_read(sc, 0x420)
		, aqm_reg_read(sc, 0x420+4)
		, aqm_reg_read(sc, 0x420+8)
		, aqm_reg_read(sc, 0x420+12)
	);
	printf("0x%04x: %08x %08x %08x %08x\n"
		, 0x430
		, aqm_reg_read(sc, 0x430)
		, aqm_reg_read(sc, 0x430+4)
		, aqm_reg_read(sc, 0x430+8)
		, aqm_reg_read(sc, 0x430+12)
	);
	/* q configuration registers */
	for (a = 0x2000; a < 0x20ff; a += 32)
		printf("0x%04x: %08x %08x %08x %08x %08x %08x %08x %08x\n"
			, a
			, aqm_reg_read(sc, a)
			, aqm_reg_read(sc, a+4)
			, aqm_reg_read(sc, a+8)
			, aqm_reg_read(sc, a+12)
			, aqm_reg_read(sc, a+16)
			, aqm_reg_read(sc, a+20)
			, aqm_reg_read(sc, a+24)
			, aqm_reg_read(sc, a+28)
		);
	/* allocated SRAM */
	for (i = 0x100; i < sc->aqmFreeSramAddress; i += 32) {
		a = 0x2000 + i;
		printf("0x%04x: %08x %08x %08x %08x %08x %08x %08x %08x\n"
			, a
			, aqm_reg_read(sc, a)
			, aqm_reg_read(sc, a+4)
			, aqm_reg_read(sc, a+8)
			, aqm_reg_read(sc, a+12)
			, aqm_reg_read(sc, a+16)
			, aqm_reg_read(sc, a+20)
			, aqm_reg_read(sc, a+24)
			, aqm_reg_read(sc, a+28)
		);
	}
	for (i = 0; i < 16; i++) {
		printf("Q[%2d] config 0x%08x status 0x%02x  "
		       "Q[%2d] config 0x%08x status 0x%02x\n"
		    , i, ixpqmgr_getqconfig(i), ixpqmgr_getqstatus(i)
		    , i+16, ixpqmgr_getqconfig(i+16), ixpqmgr_getqstatus(i+16)
		);
	}
}

void
ixpqmgr_notify_enable(int qId, int srcSel)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;
#if 0
	/* Calculate the checkMask and checkValue for this q */
	aqm_calc_statuscheck(sc, qId, srcSel);
#endif
	/* Set the interrupt source if this queue is in the range 0-31 */
	if (qId < IX_QMGR_MIN_QUEUPP_QID)
	    aqm_srcsel_write(sc, qId, srcSel);

	/* Enable the interrupt */
	aqm_int_enable(sc, qId);
}

void
ixpqmgr_notify_disable(int qId)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;

	aqm_int_disable(sc, qId);
}

/*
 * Rebuild the priority table used by the dispatcher.
 */
static void
ixpqmgr_rebuild(struct ixpqmgr_softc *sc)
{
	int q, pri;
	int lowQuePriorityTableIndex, uppQuePriorityTableIndex;
	struct qmgrInfo *qi;

	sc->lowPriorityTableFirstHalfMask = 0;
	sc->uppPriorityTableFirstHalfMask = 0;
	
	lowQuePriorityTableIndex = 0;
	uppQuePriorityTableIndex = 32;
	for (pri = 0; pri < IX_QMGR_NUM_PRIORITY_LEVELS; pri++) {
	    /* low priority q's */
	    for (q = 0; q < IX_QMGR_MIN_QUEUPP_QID; q++) {
		qi = &sc->qinfo[q];
		if (qi->priority == pri) { 
		    /*
		     * Build the priority table bitmask which match the
		     * queues of the first half of the priority table.
		     */
		    if (lowQuePriorityTableIndex < 16) {
			sc->lowPriorityTableFirstHalfMask |=
			    qi->intRegCheckMask;
		    }
		    sc->priorityTable[lowQuePriorityTableIndex++] = q;
		}
	    }
	    /* high priority q's */
	    for (; q < IX_QMGR_MAX_NUM_QUEUES; q++) {
		qi = &sc->qinfo[q];
		if (qi->priority == pri) {
		    /*
		     * Build the priority table bitmask which match the
		     * queues of the first half of the priority table .
		     */
		    if (uppQuePriorityTableIndex < 48) {
			sc->uppPriorityTableFirstHalfMask |=
			    qi->intRegCheckMask;
		    }
		    sc->priorityTable[uppQuePriorityTableIndex++] = q;
		}
	    }
	}
	sc->rebuildTable = FALSE;
}

/*
 * Count the number of leading zero bits in a word,
 * and return the same value than the CLZ instruction.
 * Note this is similar to the standard ffs function but
 * it counts zero's from the MSB instead of the LSB.
 *
 * word (in)    return value (out)
 * 0x80000000   0
 * 0x40000000   1
 * ,,,          ,,,
 * 0x00000002   30
 * 0x00000001   31
 * 0x00000000   32
 *
 * The C version of this function is used as a replacement 
 * for system not providing the equivalent of the CLZ 
 * assembly language instruction.
 *
 * Note that this version is big-endian
 */
static unsigned int
_lzcount(uint32_t word)
{
	unsigned int lzcount = 0;

	if (word == 0)
	    return 32;
	while ((word & 0x80000000) == 0) {
	    word <<= 1;
	    lzcount++;
	}
	return lzcount;
}

static void
ixpqmgr_intr(void *arg)
{
	struct ixpqmgr_softc *sc = ixpqmgr_sc;
	uint32_t intRegVal;                /* Interrupt reg val */
	struct qmgrInfo *qi;
	int priorityTableIndex;		/* Priority table index */
	int qIndex;			/* Current queue being processed */

	/* Read the interrupt register */
	intRegVal = aqm_reg_read(sc, IX_QMGR_QINTREG0_OFFSET);
	/* Write back to clear interrupt */
	aqm_reg_write(sc, IX_QMGR_QINTREG0_OFFSET, intRegVal);

	DPRINTFn(5, sc->sc_dev, "%s: ISR0 0x%x ISR1 0x%x\n",
	    __func__, intRegVal, aqm_reg_read(sc, IX_QMGR_QINTREG1_OFFSET));

	/* No queue has interrupt register set */
	if (intRegVal != 0) {
		/* get the first queue Id from the interrupt register value */
		qIndex = (32 - 1) - _lzcount(intRegVal);

		DPRINTFn(2, sc->sc_dev, "%s: ISR0 0x%x qIndex %u\n",
		    __func__, intRegVal, qIndex);

		/*
		 * Optimize for single callback case.
		 */
		 qi = &sc->qinfo[qIndex];
		 if (intRegVal == qi->intRegCheckMask) {
		    /*
		     * Only 1 queue event triggered a notification.
		     * Call the callback function for this queue
		     */
		    qi->cb(qIndex, qi->cbarg);
		 } else {
		     /*
		      * The event is triggered by more than 1 queue,
		      * the queue search will start from the beginning
		      * or the middle of the priority table.
		      *
		      * The search will end when all the bits of the interrupt
		      * register are cleared. There is no need to maintain
		      * a seperate value and test it at each iteration.
		      */
		     if (intRegVal & sc->lowPriorityTableFirstHalfMask) {
			 priorityTableIndex = 0;
		     } else {
			 priorityTableIndex = 16;
		     }
		     /*
		      * Iterate over the priority table until all the bits
		      * of the interrupt register are cleared.
		      */
		     do {
			 qIndex = sc->priorityTable[priorityTableIndex++];
			 qi = &sc->qinfo[qIndex];

			 /* If this queue caused this interrupt to be raised */
			 if (intRegVal & qi->intRegCheckMask) {
			     /* Call the callback function for this queue */
			     qi->cb(qIndex, qi->cbarg);
			     /* Clear the interrupt register bit */
			     intRegVal &= ~qi->intRegCheckMask;
			 }
		      } while (intRegVal);
		 }
	 }

	/* Rebuild the priority table if needed */
	if (sc->rebuildTable)
	    ixpqmgr_rebuild(sc);
}

#if 0
/*
 * Generate the parameters used to check if a Q's status matches
 * the specified source select.  We calculate which status word
 * to check (statusWordOffset), the value to check the status
 * against (statusCheckValue) and the mask (statusMask) to mask
 * out all but the bits to check in the status word.
 */
static void
aqm_calc_statuscheck(int qId, IxQMgrSourceId srcSel)
{
	struct qmgrInfo *qi = &qinfo[qId];
	uint32_t shiftVal;
       
	if (qId < IX_QMGR_MIN_QUEUPP_QID) {
	    switch (srcSel) {
	    case IX_QMGR_Q_SOURCE_ID_E:
		qi->statusCheckValue = IX_QMGR_Q_STATUS_E_BIT_MASK;
		qi->statusMask = IX_QMGR_Q_STATUS_E_BIT_MASK;
		break;
	    case IX_QMGR_Q_SOURCE_ID_NE:
		qi->statusCheckValue = IX_QMGR_Q_STATUS_NE_BIT_MASK;
		qi->statusMask = IX_QMGR_Q_STATUS_NE_BIT_MASK;
		break;
	    case IX_QMGR_Q_SOURCE_ID_NF:
		qi->statusCheckValue = IX_QMGR_Q_STATUS_NF_BIT_MASK;
		qi->statusMask = IX_QMGR_Q_STATUS_NF_BIT_MASK;
		break;
	    case IX_QMGR_Q_SOURCE_ID_F:
		qi->statusCheckValue = IX_QMGR_Q_STATUS_F_BIT_MASK;
		qi->statusMask = IX_QMGR_Q_STATUS_F_BIT_MASK;
		break;
	    case IX_QMGR_Q_SOURCE_ID_NOT_E:
		qi->statusCheckValue = 0;
		qi->statusMask = IX_QMGR_Q_STATUS_E_BIT_MASK;
		break;
	    case IX_QMGR_Q_SOURCE_ID_NOT_NE:
		qi->statusCheckValue = 0;
		qi->statusMask = IX_QMGR_Q_STATUS_NE_BIT_MASK;
		break;
	    case IX_QMGR_Q_SOURCE_ID_NOT_NF:
		qi->statusCheckValue = 0;
		qi->statusMask = IX_QMGR_Q_STATUS_NF_BIT_MASK;
		break;
	    case IX_QMGR_Q_SOURCE_ID_NOT_F:
		qi->statusCheckValue = 0;
		qi->statusMask = IX_QMGR_Q_STATUS_F_BIT_MASK;
		break;
	    default:
		/* Should never hit */
		IX_OSAL_ASSERT(0);
		break;
	    }

	    /* One nibble of status per queue so need to shift the
	     * check value and mask out to the correct position.
	     */
	    shiftVal = (qId % IX_QMGR_QUELOWSTAT_NUM_QUE_PER_WORD) * 
		IX_QMGR_QUELOWSTAT_BITS_PER_Q;

	    /* Calculate the which status word to check from the qId,
	     * 8 Qs status per word
	     */
	    qi->statusWordOffset = qId / IX_QMGR_QUELOWSTAT_NUM_QUE_PER_WORD;

	    qi->statusCheckValue <<= shiftVal;
	    qi->statusMask <<= shiftVal;
	} else {
	    /* One status word */
	    qi->statusWordOffset = 0;
	    /* Single bits per queue and int source bit hardwired  NE,
	     * Qs start at 32.
	     */
	    qi->statusMask = 1 << (qId - IX_QMGR_MIN_QUEUPP_QID);
	    qi->statusCheckValue = qi->statusMask;
	}
}
#endif

static void
aqm_int_enable(struct ixpqmgr_softc *sc, int qId)
{
	bus_size_t reg;
	uint32_t v;
	
	if (qId < IX_QMGR_MIN_QUEUPP_QID)
	    reg = IX_QMGR_QUEIEREG0_OFFSET;
	else
	    reg = IX_QMGR_QUEIEREG1_OFFSET;
	v = aqm_reg_read(sc, reg);
	aqm_reg_write(sc, reg, v | (1 << (qId % IX_QMGR_MIN_QUEUPP_QID)));

	DPRINTF(sc->sc_dev, "%s(%u) 0x%lx: 0x%x => 0x%x\n",
	    __func__, qId, reg, v, aqm_reg_read(sc, reg));
}

static void
aqm_int_disable(struct ixpqmgr_softc *sc, int qId)
{
	bus_size_t reg;
	uint32_t v;

	if (qId < IX_QMGR_MIN_QUEUPP_QID)
	    reg = IX_QMGR_QUEIEREG0_OFFSET;
	else
	    reg = IX_QMGR_QUEIEREG1_OFFSET;
	v = aqm_reg_read(sc, reg);
	aqm_reg_write(sc, reg, v &~ (1 << (qId % IX_QMGR_MIN_QUEUPP_QID)));

	DPRINTF(sc->sc_dev, "%s(%u) 0x%lx: 0x%x => 0x%x\n",
	    __func__, qId, reg, v, aqm_reg_read(sc, reg));
}

static unsigned
log2(unsigned n)
{
	unsigned count;
	/*
	 * N.B. this function will return 0 if supplied 0.
	 */
	for (count = 0; n/2; count++)
	    n /= 2;
	return count;
}

static __inline unsigned
toAqmEntrySize(int entrySize)
{
	/* entrySize  1("00"),2("01"),4("10") */
	return log2(entrySize);
}

static __inline unsigned
toAqmBufferSize(unsigned bufferSizeInWords)
{
	/* bufferSize 16("00"),32("01),64("10"),128("11") */
	return log2(bufferSizeInWords / IX_QMGR_MIN_BUFFER_SIZE);
}

static __inline unsigned
toAqmWatermark(int watermark)
{
	/*
	 * Watermarks 0("000"),1("001"),2("010"),4("011"),
	 * 8("100"),16("101"),32("110"),64("111")
	 */
	return log2(2 * watermark);
}

static void
aqm_qcfg(struct ixpqmgr_softc *sc, int qId, u_int ne, u_int nf)
{
	const struct qmgrInfo *qi = &sc->qinfo[qId];
	uint32_t qCfg;
	uint32_t baseAddress;

	/* Build config register */
	qCfg = ((toAqmEntrySize(1) & IX_QMGR_ENTRY_SIZE_MASK) <<
		    IX_QMGR_Q_CONFIG_ESIZE_OFFSET)
	     | ((toAqmBufferSize(qi->qSizeInWords) & IX_QMGR_SIZE_MASK) <<
		    IX_QMGR_Q_CONFIG_BSIZE_OFFSET);

	/* baseAddress, calculated relative to start address */
	baseAddress = sc->aqmFreeSramAddress;
		       
	/* base address must be word-aligned */
	KASSERT((baseAddress % IX_QMGR_BASE_ADDR_16_WORD_ALIGN) == 0,
	    ("address not word-aligned"));

	/* Now convert to a 16 word pointer as required by QUECONFIG register */
	baseAddress >>= IX_QMGR_BASE_ADDR_16_WORD_SHIFT;
	qCfg |= baseAddress << IX_QMGR_Q_CONFIG_BADDR_OFFSET;

	/* set watermarks */
	qCfg |= (toAqmWatermark(ne) << IX_QMGR_Q_CONFIG_NE_OFFSET)
	     |  (toAqmWatermark(nf) << IX_QMGR_Q_CONFIG_NF_OFFSET);

	DPRINTF(sc->sc_dev, "%s(%u, %u, %u) 0x%x => 0x%x @ 0x%x\n",
	    __func__, qId, ne, nf,
	    aqm_reg_read(sc, IX_QMGR_Q_CONFIG_ADDR_GET(qId)),
	    qCfg, IX_QMGR_Q_CONFIG_ADDR_GET(qId));

	aqm_reg_write(sc, IX_QMGR_Q_CONFIG_ADDR_GET(qId), qCfg);
}

static void
aqm_srcsel_write(struct ixpqmgr_softc *sc, int qId, int sourceId)
{
	bus_size_t off;
	uint32_t v;

	/*
	 * Calculate the register offset; multiple queues split across registers
	 */
	off = IX_QMGR_INT0SRCSELREG0_OFFSET +
	    ((qId / IX_QMGR_INTSRC_NUM_QUE_PER_WORD) * sizeof(uint32_t));

	v = aqm_reg_read(sc, off);
	if (off == IX_QMGR_INT0SRCSELREG0_OFFSET && qId == 0) {
	    /* Queue 0 at INT0SRCSELREG should not corrupt the value bit-3  */
	    v |= 0x7;
	} else {     
	  const uint32_t bpq = 32 / IX_QMGR_INTSRC_NUM_QUE_PER_WORD;
	  uint32_t mask;
	  int qshift;

	  qshift = (qId & (IX_QMGR_INTSRC_NUM_QUE_PER_WORD-1)) * bpq;
	  mask = ((1 << bpq) - 1) << qshift;	/* q's status mask */

	  /* merge sourceId */
	  v = (v &~ mask) | ((sourceId << qshift) & mask);
	}

	DPRINTF(sc->sc_dev, "%s(%u, %u) 0x%x => 0x%x @ 0x%lx\n",
	    __func__, qId, sourceId, aqm_reg_read(sc, off), v, off);
	aqm_reg_write(sc, off, v);
}

/*
 * Reset AQM registers to default values.
 */
static void
aqm_reset(struct ixpqmgr_softc *sc)
{
	int i;

	/* Reset queues 0..31 status registers 0..3 */
	aqm_reg_write(sc, IX_QMGR_QUELOWSTAT0_OFFSET,
		IX_QMGR_QUELOWSTAT_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_QUELOWSTAT1_OFFSET,
		IX_QMGR_QUELOWSTAT_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_QUELOWSTAT2_OFFSET,
		IX_QMGR_QUELOWSTAT_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_QUELOWSTAT3_OFFSET,
		IX_QMGR_QUELOWSTAT_RESET_VALUE);

	/* Reset underflow/overflow status registers 0..1 */
	aqm_reg_write(sc, IX_QMGR_QUEUOSTAT0_OFFSET,
		IX_QMGR_QUEUOSTAT_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_QUEUOSTAT1_OFFSET,
		IX_QMGR_QUEUOSTAT_RESET_VALUE);
	
	/* Reset queues 32..63 nearly empty status registers */
	aqm_reg_write(sc, IX_QMGR_QUEUPPSTAT0_OFFSET,
		IX_QMGR_QUEUPPSTAT0_RESET_VALUE);

	/* Reset queues 32..63 full status registers */
	aqm_reg_write(sc, IX_QMGR_QUEUPPSTAT1_OFFSET,
		IX_QMGR_QUEUPPSTAT1_RESET_VALUE);

	/* Reset int0 status flag source select registers 0..3 */
	aqm_reg_write(sc, IX_QMGR_INT0SRCSELREG0_OFFSET,
			     IX_QMGR_INT0SRCSELREG_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_INT0SRCSELREG1_OFFSET,
			     IX_QMGR_INT0SRCSELREG_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_INT0SRCSELREG2_OFFSET,
			     IX_QMGR_INT0SRCSELREG_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_INT0SRCSELREG3_OFFSET,
			     IX_QMGR_INT0SRCSELREG_RESET_VALUE);
	     
	/* Reset queue interrupt enable register 0..1 */
	aqm_reg_write(sc, IX_QMGR_QUEIEREG0_OFFSET,
		IX_QMGR_QUEIEREG_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_QUEIEREG1_OFFSET,
		IX_QMGR_QUEIEREG_RESET_VALUE);

	/* Reset queue interrupt register 0..1 */
	aqm_reg_write(sc, IX_QMGR_QINTREG0_OFFSET, IX_QMGR_QINTREG_RESET_VALUE);
	aqm_reg_write(sc, IX_QMGR_QINTREG1_OFFSET, IX_QMGR_QINTREG_RESET_VALUE);

	/* Reset queue configuration words 0..63 */
	for (i = 0; i < IX_QMGR_MAX_NUM_QUEUES; i++)
	    aqm_reg_write(sc, sc->qinfo[i].qConfigRegAddr,
		IX_QMGR_QUECONFIG_RESET_VALUE);

	/* XXX zero SRAM to simplify debugging */
	for (i = IX_QMGR_QUEBUFFER_SPACE_OFFSET;
	     i < IX_QMGR_AQM_SRAM_SIZE_IN_BYTES; i += sizeof(uint32_t))
	    aqm_reg_write(sc, i, 0);
}

static device_method_t ixpqmgr_methods[] = {
	DEVMETHOD(device_probe,		ixpqmgr_probe),
	DEVMETHOD(device_attach,	ixpqmgr_attach),
	DEVMETHOD(device_detach,	ixpqmgr_detach),

	{ 0, 0 }
};

static driver_t ixpqmgr_driver = {
	"ixpqmgr",
	ixpqmgr_methods,
	sizeof(struct ixpqmgr_softc),
};
static devclass_t ixpqmgr_devclass;

DRIVER_MODULE(ixpqmgr, ixp, ixpqmgr_driver, ixpqmgr_devclass, 0, 0);
