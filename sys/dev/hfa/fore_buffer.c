/*-
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Buffer Supply queue management
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>
#include <dev/pci/pcivar.h>
#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <dev/hfa/fore_var.h>
#include <dev/hfa/fore_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static void	fore_buf_drain(Fore_unit *);
static void	fore_buf_supply_1s(Fore_unit *);
static void	fore_buf_supply_1l(Fore_unit *);


/*
 * Allocate Buffer Supply Queues Data Structures
 *
 * Here we are allocating memory for both Strategy 1 Small and Large
 * structures contiguously.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	0		allocations successful
 *	else		allocation failed
 */
int
fore_buf_allocate(fup)
	Fore_unit	*fup;
{
	caddr_t		memp;

	/*
	 * Allocate non-cacheable memory for buffer supply status words
	 */
	memp = atm_dev_alloc(
			sizeof(Q_status) * (BUF1_SM_QUELEN + BUF1_LG_QUELEN),
			QSTAT_ALIGN, ATM_DEV_NONCACHE);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_buf1s_stat = (Q_status *) memp;
	fup->fu_buf1l_stat = ((Q_status *) memp) + BUF1_SM_QUELEN;

	memp = (caddr_t)vtophys(fup->fu_buf1s_stat);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_buf1s_statd = (Q_status *) memp;
	fup->fu_buf1l_statd = ((Q_status *) memp) + BUF1_SM_QUELEN;

	/*
	 * Allocate memory for buffer supply descriptors
	 */
	memp = atm_dev_alloc(sizeof(Buf_descr) * 
			((BUF1_SM_QUELEN * BUF1_SM_ENTSIZE) + 
			 (BUF1_LG_QUELEN * BUF1_LG_ENTSIZE)),
			BUF_DESCR_ALIGN, 0);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_buf1s_desc = (Buf_descr *) memp;
	fup->fu_buf1l_desc = ((Buf_descr *) memp) + 
			(BUF1_SM_QUELEN * BUF1_SM_ENTSIZE);

	memp = (caddr_t)vtophys(fup->fu_buf1s_desc);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_buf1s_descd = (Buf_descr *) memp;
	fup->fu_buf1l_descd = ((Buf_descr *) memp) + 
			(BUF1_SM_QUELEN * BUF1_SM_ENTSIZE);

	return (0);
}


/*
 * Buffer Supply Queues Initialization
 *
 * Allocate and initialize the host-resident buffer supply queue structures
 * and then initialize the CP-resident queue structures.
 * 
 * Called at interrupt level.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_buf_initialize(fup)
	Fore_unit	*fup;
{
	Aali		*aap = fup->fu_aali;
	Buf_queue	*cqp;
	H_buf_queue	*hbp;
	Buf_descr	*bdp;
	Buf_descr	*bdp_dma;
	Q_status	*qsp;
	Q_status	*qsp_dma;
	int		i;

	/*
	 * Initialize Strategy 1 Small Queues
	 */

	/*
	 * Point to CP-resident buffer supply queue
	 */
	cqp = (Buf_queue *)(fup->fu_ram + CP_READ(aap->aali_buf1s_q));

	/*
	 * Point to host-resident buffer supply queue structures
	 */
	hbp = fup->fu_buf1s_q;
	qsp = fup->fu_buf1s_stat;
	qsp_dma = fup->fu_buf1s_statd;
	bdp = fup->fu_buf1s_desc;
	bdp_dma = fup->fu_buf1s_descd;

	/*
	 * Loop thru all queue entries and do whatever needs doing
	 */
	for (i = 0; i < BUF1_SM_QUELEN; i++) {

		/*
		 * Set queue status word to free
		 */
		*qsp = QSTAT_FREE;

		/*
		 * Set up host queue entry and link into ring
		 */
		hbp->hbq_cpelem = cqp;
		hbp->hbq_status = qsp;
		hbp->hbq_descr = bdp;
		hbp->hbq_descr_dma = bdp_dma;
		if (i == (BUF1_SM_QUELEN - 1))
			hbp->hbq_next = fup->fu_buf1s_q;
		else
			hbp->hbq_next = hbp + 1;

		/*
		 * Now let the CP into the game
		 */
		cqp->cq_status = (CP_dma) CP_WRITE(qsp_dma);

		/*
		 * Bump all queue pointers
		 */
		hbp++;
		qsp++;
		qsp_dma++;
		bdp += BUF1_SM_ENTSIZE;
		bdp_dma += BUF1_SM_ENTSIZE;
		cqp++;
	}

	/*
	 * Initialize queue pointers
	 */
	fup->fu_buf1s_head = fup->fu_buf1s_tail = fup->fu_buf1s_q;


	/*
	 * Initialize Strategy 1 Large Queues
	 */

	/*
	 * Point to CP-resident buffer supply queue
	 */
	cqp = (Buf_queue *)(fup->fu_ram + CP_READ(aap->aali_buf1l_q));

	/*
	 * Point to host-resident buffer supply queue structures
	 */
	hbp = fup->fu_buf1l_q;
	qsp = fup->fu_buf1l_stat;
	qsp_dma = fup->fu_buf1l_statd;
	bdp = fup->fu_buf1l_desc;
	bdp_dma = fup->fu_buf1l_descd;

	/*
	 * Loop thru all queue entries and do whatever needs doing
	 */
	for (i = 0; i < BUF1_LG_QUELEN; i++) {

		/*
		 * Set queue status word to free
		 */
		*qsp = QSTAT_FREE;

		/*
		 * Set up host queue entry and link into ring
		 */
		hbp->hbq_cpelem = cqp;
		hbp->hbq_status = qsp;
		hbp->hbq_descr = bdp;
		hbp->hbq_descr_dma = bdp_dma;
		if (i == (BUF1_LG_QUELEN - 1))
			hbp->hbq_next = fup->fu_buf1l_q;
		else
			hbp->hbq_next = hbp + 1;

		/*
		 * Now let the CP into the game
		 */
		cqp->cq_status = (CP_dma) CP_WRITE(qsp_dma);

		/*
		 * Bump all queue pointers
		 */
		hbp++;
		qsp++;
		qsp_dma++;
		bdp += BUF1_LG_ENTSIZE;
		bdp_dma += BUF1_LG_ENTSIZE;
		cqp++;
	}

	/*
	 * Initialize queue pointers
	 */
	fup->fu_buf1l_head = fup->fu_buf1l_tail = fup->fu_buf1l_q;

	return;
}


/*
 * Supply Buffers to CP
 *
 * This function will resupply the CP with buffers to be used to
 * store incoming data.
 *
 * May be called in interrupt state.
 * Must be called with interrupts locked out.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_buf_supply(fup)
	Fore_unit	*fup;
{

	/*
	 * First, clean out the supply queues
	 */
	fore_buf_drain(fup);

	/*
	 * Then, supply the buffers for each queue
	 */
	fore_buf_supply_1s(fup);
	fore_buf_supply_1l(fup);

	return;
}


/*
 * Supply Strategy 1 Small Buffers to CP
 *
 * May be called in interrupt state.
 * Must be called with interrupts locked out.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
static void
fore_buf_supply_1s(fup)
	Fore_unit	*fup;
{
	H_buf_queue	*hbp;
	Buf_queue	*cqp;
	Buf_descr	*bdp;
	Buf_handle	*bhp;
	KBuffer		*m;
	int		nvcc, nbuf, i;

	/*
	 * Figure out how many buffers we should be giving to the CP.
	 * We're basing this calculation on the current number of open
	 * VCCs thru this device, with certain minimum and maximum values
	 * enforced.  This will then allow us to figure out how many more 
	 * buffers we need to supply to the CP.  This will be rounded up 
	 * to fill a supply queue entry.
	 */
	nvcc = MAX(fup->fu_open_vcc, BUF_MIN_VCC);
	nbuf = nvcc * 4;
	nbuf = MIN(nbuf, BUF1_SM_CPPOOL);
	nbuf -= fup->fu_buf1s_cnt;
	nbuf = roundup(nbuf, BUF1_SM_ENTSIZE);

	/*
	 * OK, now supply the buffers to the CP
	 */
	while (nbuf > 0) {

		/*
		 * Acquire a supply queue entry
		 */
		hbp = fup->fu_buf1s_tail;
		if (!((*hbp->hbq_status) & QSTAT_FREE))
			break;
		bdp = hbp->hbq_descr;

		/*
		 * Get a buffer for each descriptor in the queue entry
		 */
		for (i = 0; i < BUF1_SM_ENTSIZE; i++, bdp++) {
			caddr_t		cp;

			/*
			 * Get a small buffer
			 */
			KB_ALLOCPKT(m, BUF1_SM_SIZE, KB_F_NOWAIT, KB_T_DATA);
			if (m == 0) {
				break;
			}
			KB_HEADSET(m, BUF1_SM_DOFF);

			/*
			 * Point to buffer handle structure
			 */
			bhp = (Buf_handle *)((caddr_t)m + BUF1_SM_HOFF);
			bhp->bh_type = BHT_S1_SMALL;

			/*
			 * Setup buffer descriptor
			 */
			bdp->bsd_handle = bhp;
			KB_DATASTART(m, cp, caddr_t);
			bhp->bh_dma = bdp->bsd_buffer = vtophys(cp);
			if (bdp->bsd_buffer == 0) {
				/*
				 * Unable to assign dma address - free up
				 * this descriptor's buffer
				 */
				fup->fu_stats->st_drv.drv_bf_segdma++;
				KB_FREEALL(m);
				break;
			}

			/*
			 * All set, so queue buffer (handle)
			 */
			ENQUEUE(bhp, Buf_handle, bh_qelem, fup->fu_buf1s_bq);
		}

		/*
		 * If we we're not able to fill all the descriptors for
		 * an entry, free up what's been partially built
		 */
		if (i != BUF1_SM_ENTSIZE) {
			caddr_t		cp;

			/*
			 * Clean up each used descriptor
			 */
			for (bdp = hbp->hbq_descr; i; i--, bdp++) {

				bhp = bdp->bsd_handle;

				DEQUEUE(bhp, Buf_handle, bh_qelem, 
					fup->fu_buf1s_bq);

				m = (KBuffer *)
					((caddr_t)bhp - BUF1_SM_HOFF);
				KB_DATASTART(m, cp, caddr_t);
				KB_FREEALL(m);
			}
			break;
		}

		/*
		 * Finally, we've got an entry ready for the CP.
		 * So claim the host queue entry and setup the CP-resident
		 * queue entry.  The CP will (potentially) grab the supplied
		 * buffers when the descriptor pointer is set.
		 */
		fup->fu_buf1s_tail = hbp->hbq_next;
		(*hbp->hbq_status) = QSTAT_PENDING;
		cqp = hbp->hbq_cpelem;
		cqp->cq_descr = (CP_dma) CP_WRITE((u_long)hbp->hbq_descr_dma);

		/*
		 * Update counters, etc for supplied buffers
		 */
		fup->fu_buf1s_cnt += BUF1_SM_ENTSIZE;
		nbuf -= BUF1_SM_ENTSIZE;
	}

	return;
}


/*
 * Supply Strategy 1 Large Buffers to CP
 *
 * May be called in interrupt state.
 * Must be called with interrupts locked out.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
static void
fore_buf_supply_1l(fup)
	Fore_unit	*fup;
{
	H_buf_queue	*hbp;
	Buf_queue	*cqp;
	Buf_descr	*bdp;
	Buf_handle	*bhp;
	KBuffer		*m;
	int		nvcc, nbuf, i;

	/*
	 * Figure out how many buffers we should be giving to the CP.
	 * We're basing this calculation on the current number of open
	 * VCCs thru this device, with certain minimum and maximum values
	 * enforced.  This will then allow us to figure out how many more 
	 * buffers we need to supply to the CP.  This will be rounded up 
	 * to fill a supply queue entry.
	 */
	nvcc = MAX(fup->fu_open_vcc, BUF_MIN_VCC);
	nbuf = nvcc * 4 * RECV_MAX_SEGS;
	nbuf = MIN(nbuf, BUF1_LG_CPPOOL);
	nbuf -= fup->fu_buf1l_cnt;
	nbuf = roundup(nbuf, BUF1_LG_ENTSIZE);

	/*
	 * OK, now supply the buffers to the CP
	 */
	while (nbuf > 0) {

		/*
		 * Acquire a supply queue entry
		 */
		hbp = fup->fu_buf1l_tail;
		if (!((*hbp->hbq_status) & QSTAT_FREE))
			break;
		bdp = hbp->hbq_descr;

		/*
		 * Get a buffer for each descriptor in the queue entry
		 */
		for (i = 0; i < BUF1_LG_ENTSIZE; i++, bdp++) {
			caddr_t		cp;

			/*
			 * Get a cluster buffer
			 */
			KB_ALLOCEXT(m, BUF1_LG_SIZE, KB_F_NOWAIT, KB_T_DATA);
			if (m == 0) {
				break;
			}
			KB_HEADSET(m, BUF1_LG_DOFF);

			/*
			 * Point to buffer handle structure
			 */
			bhp = (Buf_handle *)((caddr_t)m + BUF1_LG_HOFF);
			bhp->bh_type = BHT_S1_LARGE;

			/*
			 * Setup buffer descriptor
			 */
			bdp->bsd_handle = bhp;
			KB_DATASTART(m, cp, caddr_t);
			bhp->bh_dma = bdp->bsd_buffer = vtophys(cp);
			if (bdp->bsd_buffer == 0) {
				/*
				 * Unable to assign dma address - free up
				 * this descriptor's buffer
				 */
				fup->fu_stats->st_drv.drv_bf_segdma++;
				KB_FREEALL(m);
				break;
			}

			/*
			 * All set, so queue buffer (handle)
			 */
			ENQUEUE(bhp, Buf_handle, bh_qelem, fup->fu_buf1l_bq);
		}

		/*
		 * If we we're not able to fill all the descriptors for
		 * an entry, free up what's been partially built
		 */
		if (i != BUF1_LG_ENTSIZE) {
			caddr_t		cp;

			/*
			 * Clean up each used descriptor
			 */
			for (bdp = hbp->hbq_descr; i; i--, bdp++) {
				bhp = bdp->bsd_handle;

				DEQUEUE(bhp, Buf_handle, bh_qelem, 
					fup->fu_buf1l_bq);

				m = (KBuffer *)
					((caddr_t)bhp - BUF1_LG_HOFF);
				KB_DATASTART(m, cp, caddr_t);
				KB_FREEALL(m);
			}
			break;
		}

		/*
		 * Finally, we've got an entry ready for the CP.
		 * So claim the host queue entry and setup the CP-resident
		 * queue entry.  The CP will (potentially) grab the supplied
		 * buffers when the descriptor pointer is set.
		 */
		fup->fu_buf1l_tail = hbp->hbq_next;
		(*hbp->hbq_status) = QSTAT_PENDING;
		cqp = hbp->hbq_cpelem;
		cqp->cq_descr = (CP_dma) CP_WRITE((u_long)hbp->hbq_descr_dma);

		/*
		 * Update counters, etc for supplied buffers
		 */
		fup->fu_buf1l_cnt += BUF1_LG_ENTSIZE;
		nbuf -= BUF1_LG_ENTSIZE;
	}

	return;
}


/*
 * Drain Buffer Supply Queues
 *
 * This function will free all completed entries at the head of each
 * buffer supply queue.  Since we consider the CP to "own" the buffers
 * once we put them on a supply queue and since a completed supply queue 
 * entry is only telling us that the CP has accepted the buffers that we 
 * gave to it, there's not much to do here.
 *
 * May be called in interrupt state.
 * Must be called with interrupts locked out.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
static void
fore_buf_drain(fup)
	Fore_unit	*fup;
{
	H_buf_queue	*hbp;

	/*
	 * Drain Strategy 1 Small Queue
	 */

	/*
	 * Process each completed entry
	 */
	while (*fup->fu_buf1s_head->hbq_status & QSTAT_COMPLETED) {

		hbp = fup->fu_buf1s_head;

		if (*hbp->hbq_status & QSTAT_ERROR) {
			/*
			 * XXX - what does this mean???
			 */
			log(LOG_ERR, "fore_buf_drain: buf1s queue error\n");
		}

		/*
		 * Mark this entry free for use and bump head pointer
		 * to the next entry in the queue
		 */
		*hbp->hbq_status = QSTAT_FREE;
		fup->fu_buf1s_head = hbp->hbq_next;
	}


	/*
	 * Drain Strategy 1 Large Queue
	 */

	/*
	 * Process each completed entry
	 */
	while (*fup->fu_buf1l_head->hbq_status & QSTAT_COMPLETED) {

		hbp = fup->fu_buf1l_head;

		if (*hbp->hbq_status & QSTAT_ERROR) {
			/*
			 * XXX - what does this mean???
			 */
			log(LOG_ERR, "fore_buf_drain: buf1l queue error\n");
		}

		/*
		 * Mark this entry free for use and bump head pointer
		 * to the next entry in the queue
		 */
		*hbp->hbq_status = QSTAT_FREE;
		fup->fu_buf1l_head = hbp->hbq_next;
	}

	return;
}


/*
 * Free Buffer Supply Queue Data Structures
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_buf_free(fup)
	Fore_unit	*fup;
{
	Buf_handle	*bhp;
	KBuffer		*m;

	/*
	 * Free any previously supplied and not returned buffers
	 */
	if (fup->fu_flags & CUF_INITED) {

		/*
		 * Run through Strategy 1 Small queue
		 */
		while ((bhp = Q_HEAD(fup->fu_buf1s_bq, Buf_handle)) != NULL) {
			caddr_t		cp;

			/*
			 * Back off to buffer
			 */
			m = (KBuffer *)((caddr_t)bhp - BUF1_SM_HOFF);

			/*
			 * Dequeue handle and free buffer
			 */
			DEQUEUE(bhp, Buf_handle, bh_qelem, fup->fu_buf1s_bq);

			KB_DATASTART(m, cp, caddr_t);
			KB_FREEALL(m);
		}

		/*
		 * Run through Strategy 1 Large queue
		 */
		while ((bhp = Q_HEAD(fup->fu_buf1l_bq, Buf_handle)) != NULL) {
			caddr_t		cp;

			/*
			 * Back off to buffer
			 */
			m = (KBuffer *)((caddr_t)bhp - BUF1_LG_HOFF);

			/*
			 * Dequeue handle and free buffer
			 */
			DEQUEUE(bhp, Buf_handle, bh_qelem, fup->fu_buf1l_bq);

			KB_DATASTART(m, cp, caddr_t);
			KB_FREEALL(m);
		}
	}

	/*
	 * Free the status words
	 */
	if (fup->fu_buf1s_stat) {
		atm_dev_free((volatile void *)fup->fu_buf1s_stat);
		fup->fu_buf1s_stat = NULL;
		fup->fu_buf1s_statd = NULL;
		fup->fu_buf1l_stat = NULL;
		fup->fu_buf1l_statd = NULL;
	}

	/*
	 * Free the transmit descriptors
	 */
	if (fup->fu_buf1s_desc) {
		atm_dev_free(fup->fu_buf1s_desc);
		fup->fu_buf1s_desc = NULL;
		fup->fu_buf1s_descd = NULL;
		fup->fu_buf1l_desc = NULL;
		fup->fu_buf1l_descd = NULL;
	}

	return;
}

