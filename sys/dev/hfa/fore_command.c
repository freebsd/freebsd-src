/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
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
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Command queue management
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
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
#include <netatm/atm_vc.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>
#include <pci/pcivar.h>
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
 * Local variables
 */
static struct t_atm_cause	fore_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	T_ATM_CAUSE_TEMPORARY_FAILURE,
	{0, 0, 0, 0}
};


/*
 * Allocate Command Queue Data Structures
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	0		allocations successful
 *	else		allocation failed
 */
int
fore_cmd_allocate(fup)
	Fore_unit	*fup;
{
	caddr_t		memp;

	/*
	 * Allocate non-cacheable memory for command status words
	 */
	memp = atm_dev_alloc(sizeof(Q_status) * CMD_QUELEN,
			QSTAT_ALIGN, ATM_DEV_NONCACHE);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_cmd_stat = (Q_status *) memp;

	memp = (caddr_t)vtophys(fup->fu_cmd_stat);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_cmd_statd = (Q_status *) memp;

	/*
	 * Allocate memory for statistics buffer
	 */
	memp = atm_dev_alloc(sizeof(Fore_stats), FORE_STATS_ALIGN, 0);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_stats = (Fore_stats *) memp;

	/*
	 * Allocate memory for PROM buffer
	 */
	memp = atm_dev_alloc(sizeof(Fore_prom), FORE_PROM_ALIGN, 0);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_prom = (Fore_prom *) memp;

	return (0);
}


/*
 * Command Queue Initialization
 *
 * Allocate and initialize the host-resident command queue structures
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
fore_cmd_initialize(fup)
	Fore_unit	*fup;
{
	Aali		*aap = fup->fu_aali;
	Cmd_queue	*cqp;
	H_cmd_queue	*hcp;
	Q_status	*qsp;
	Q_status	*qsp_dma;
	int		i;

	/*
	 * Point to CP-resident command queue
	 */
	cqp = (Cmd_queue *)(fup->fu_ram + CP_READ(aap->aali_cmd_q));

	/*
	 * Point to host-resident command queue structures
	 */
	hcp = fup->fu_cmd_q;
	qsp = fup->fu_cmd_stat;
	qsp_dma = fup->fu_cmd_statd;

	/*
	 * Loop thru all queue entries and do whatever needs doing
	 */
	for (i = 0; i < CMD_QUELEN; i++) {

		/*
		 * Set queue status word to free
		 */
		*qsp = QSTAT_FREE;

		/*
		 * Set up host queue entry and link into ring
		 */
		hcp->hcq_cpelem = cqp;
		hcp->hcq_status = qsp;
		if (i == (CMD_QUELEN - 1))
			hcp->hcq_next = fup->fu_cmd_q;
		else
			hcp->hcq_next = hcp + 1;

		/*
		 * Now let the CP into the game
		 */
		cqp->cmdq_status = (CP_dma) CP_WRITE(qsp_dma);

		/*
		 * Bump all queue pointers
		 */
		hcp++;
		qsp++;
		qsp_dma++;
		cqp++;
	}

	/*
	 * Initialize queue pointers
	 */
	fup->fu_cmd_head = fup->fu_cmd_tail = fup->fu_cmd_q;

	return;
}


/*
 * Drain Command Queue
 *
 * This function will process and free all completed entries at the head 
 * of the command queue. 
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
fore_cmd_drain(fup)
	Fore_unit	*fup;
{
	H_cmd_queue	*hcp;
	Fore_vcc	*fvp;

	/*
	 * Process each completed entry
	 * ForeThought 4 may set QSTAT_ERROR without QSTAT_COMPLETED.
	 */
	while (*fup->fu_cmd_head->hcq_status & (QSTAT_COMPLETED | QSTAT_ERROR)) {

		hcp = fup->fu_cmd_head;

		/*
		 * Process command completion
		 */
		switch (hcp->hcq_code) {

		case CMD_ACT_VCCIN:
		case CMD_ACT_VCCOUT:
			fvp = hcp->hcq_arg;
			if (*hcp->hcq_status & QSTAT_ERROR) {
				/*
				 * VCC activation failed - just abort vcc
				 */
				if (fvp)
					atm_cm_abort(fvp->fv_connvc,
						&fore_cause);
				fup->fu_pif.pif_cmderrors++;
			} else {
				/*
				 * Successful VCC activation
				 */
				if (fvp) {
					fvp->fv_state = CVS_ACTIVE;
					fup->fu_open_vcc++;
				}
			}
			break;

		case CMD_DACT_VCCIN:
		case CMD_DACT_VCCOUT:
			fvp = hcp->hcq_arg;
			if (*hcp->hcq_status & QSTAT_ERROR) {
				/*
				 * VCC dactivation failed - whine
				 */
				log(LOG_ERR, 
				   "fore_cmd_drain: DACT failed, vcc=(%d,%d)\n",
					fvp->fv_connvc->cvc_vcc->vc_vpi,
					fvp->fv_connvc->cvc_vcc->vc_vci);
				fup->fu_pif.pif_cmderrors++;
			} else {
				/*
				 * Successful VCC dactivation - so what?
				 */
			}
			break;

		case CMD_GET_STATS:
			if (*hcp->hcq_status & QSTAT_ERROR) {
				/*
				 * Couldn't get stats
				 */
				fup->fu_pif.pif_cmderrors++;
				fup->fu_stats_ret = EIO;
			} else {
				/*
				 * Stats are now in unit buffer
				 */
				fup->fu_stats_ret = 0;
			}
			fup->fu_flags &= ~FUF_STATCMD;

			/*
			 * Flush received stats data
			 */
#ifdef VAC
			if (vac)
				vac_pageflush((addr_t)fup->fu_stats);
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
			/*
			 * Little endian machines receives the stats in
			 * wrong byte order. Instead of swapping in user
			 * land, swap here so that everything going out
			 * of the kernel is in correct host order.
			 */
			{
				u_long *bp = (u_long *)fup->fu_stats;
				int	loop;

				for ( loop = 0; loop < sizeof(Fore_cp_stats)/
				    sizeof(long); loop++, bp++ )
					*bp = ntohl(*bp);
			}
#endif	/* BYTE_ORDER == LITTLE_ENDIAN */

			/*
			 * Poke whoever is waiting on the stats
			 */
			wakeup(&fup->fu_stats);
			break;

		case CMD_GET_PROM:
			if (fup->fu_ft4)
				goto unknown;
			goto prom;

		case CMD_GET_PROM4:
			if (!fup->fu_ft4)
				goto unknown;
		prom:
			if (*hcp->hcq_status & QSTAT_ERROR) {
				/*
				 * Couldn't get PROM data
				 */
				fup->fu_pif.pif_cmderrors++;
				log(LOG_ERR, 
				    "fore_cmd_drain: %s%d: GET_PROM failed\n",
					fup->fu_pif.pif_name,
					fup->fu_pif.pif_unit);
			} else {
				Fore_prom	*fp = fup->fu_prom;

				/*
				 * Flush received PROM data
				 */
#ifdef VAC
				if (vac)
					vac_pageflush((addr_t)fp);
#endif
				/*
				 * Copy PROM info into config areas
				 */
				bcopy(&fp->pr_mac[2],
					&fup->fu_pif.pif_macaddr,
					sizeof(struct mac_addr));
				fup->fu_config.ac_macaddr = 
					fup->fu_pif.pif_macaddr;
				snprintf(fup->fu_config.ac_hard_vers,
				    sizeof(fup->fu_config.ac_hard_vers),
					"%ld.%ld.%ld",
					(fp->pr_hwver >> 16) & 0xff,
					(fp->pr_hwver >> 8) & 0xff,
					fp->pr_hwver & 0xff);
				fup->fu_config.ac_serial = fp->pr_serno;
			}
			break;

		default:
		unknown:
			log(LOG_ERR, "fore_cmd_drain: unknown command %ld\n",
				hcp->hcq_code);
		}

		/*
		 * Mark this entry free for use and bump head pointer
		 * to the next entry in the queue
		 */
		*hcp->hcq_status = QSTAT_FREE;
		fup->fu_cmd_head = hcp->hcq_next;
	}

	return;
}


/*
 * Free Command Queue Data Structures
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_cmd_free(fup)
	Fore_unit	*fup;
{
	H_cmd_queue	*hcp;

	/*
	 * Deal with any commands left on the queue
	 */
	if (fup->fu_flags & CUF_INITED) {
		while (*fup->fu_cmd_head->hcq_status != QSTAT_FREE) {
			hcp = fup->fu_cmd_head;

			switch (hcp->hcq_code) {

			case CMD_GET_STATS:
				/*
				 * Just in case someone is sleeping on this
				 */
				fup->fu_stats_ret = EIO;
				wakeup(&fup->fu_stats);
				break;
			}

			*hcp->hcq_status = QSTAT_FREE;
			fup->fu_cmd_head = hcp->hcq_next;
		}
	}

	/*
	 * Free the statistics buffer
	 */
	if (fup->fu_stats) {
		atm_dev_free(fup->fu_stats);
		fup->fu_stats = NULL;
	}

	/*
	 * Free the PROM buffer
	 */
	if (fup->fu_prom) {
		atm_dev_free(fup->fu_prom);
		fup->fu_prom = NULL;
	}

	/*
	 * Free the status words
	 */
	if (fup->fu_cmd_stat) {
		atm_dev_free((volatile void *)fup->fu_cmd_stat);
		fup->fu_cmd_stat = NULL;
		fup->fu_cmd_statd = NULL;
	}

	return;
}

