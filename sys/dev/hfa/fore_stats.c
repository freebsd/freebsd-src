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
 * Device statistics routines
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
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
 * Get device statistics from CP
 * 
 * This function will issue a GET_STATS command to the CP in order to
 * initiate the DMA transfer of the CP's statistics structure to the host.
 * We will then sleep pending command completion.  This must only be called
 * from the ioctl system call handler.
 *
 * Called at splnet.
 *
 * Arguments:
 *	fup	pointer to device unit structure
 *
 * Returns:
 *	0	stats retrieval successful
 *	errno 	stats retrieval failed - reason indicated
 *
 */
int
fore_get_stats(fup)
	Fore_unit	*fup;
{
	H_cmd_queue	*hcp;
	Cmd_queue	*cqp;
	int		s, sst;

	ATM_DEBUG1("fore_get_stats: fup=%p\n", fup);

	/*
	 * Make sure device has been initialized
	 */
	if ((fup->fu_flags & CUF_INITED) == 0) {
		return (EIO);
	}

	/*
	 * If someone has already initiated a stats request, we'll
	 * just wait for that one to complete
	 */
	s = splimp();
	if (fup->fu_flags & FUF_STATCMD) {

#if (defined(BSD) && (BSD >= 199103))
		sst = tsleep((caddr_t)&fup->fu_stats, PWAIT|PCATCH, "fore", 0);
#else
		sst = sleep((caddr_t)&fup->fu_stats, PWAIT|PCATCH);
		if (sst != 0)
			sst = EINTR;
#endif
		(void) splx(s);
		return (sst ? sst : fup->fu_stats_ret);
	}

	/*
	 * Limit stats gathering to once a second or so
	 */
	if (time_second == fup->fu_stats_time) {
		(void) splx(s);
		return (0);
	} else
		fup->fu_stats_time = time_second;

	/*
	 * Queue command at end of command queue
	 */
	hcp = fup->fu_cmd_tail;
	if ((*hcp->hcq_status) & QSTAT_FREE) {
		void	*dma;

		/*
		 * Queue entry available, so set our view of things up
		 */
		hcp->hcq_code = CMD_GET_STATS;
		hcp->hcq_arg = NULL;
		fup->fu_cmd_tail = hcp->hcq_next;

		/*
		 * Now set the CP-resident queue entry - the CP will grab
		 * the command when the op-code is set.
		 */
		cqp = hcp->hcq_cpelem;
		(*hcp->hcq_status) = QSTAT_PENDING;

		dma = DMA_GET_ADDR(fup->fu_stats, sizeof(Fore_cp_stats),
			FORE_STATS_ALIGN, 0);
		if (dma == NULL) {
			fup->fu_stats->st_drv.drv_cm_nodma++;
			(void) splx(s);
			return (EIO);
		}
		fup->fu_statsd = dma;
		cqp->cmdq_stats.stats_buffer = (CP_dma) CP_WRITE(dma);

		fup->fu_flags |= FUF_STATCMD;
		cqp->cmdq_stats.stats_cmd = 
			CP_WRITE(CMD_GET_STATS | CMD_INTR_REQ);

		/*
		 * Now wait for command to finish
		 */
#if (defined(BSD) && (BSD >= 199103))
		sst = tsleep((caddr_t)&fup->fu_stats, PWAIT|PCATCH, "fore", 0);
#else
		sst = sleep((caddr_t)&fup->fu_stats, PWAIT|PCATCH);
		if (sst != 0)
			sst = EINTR;
#endif
		(void) splx(s);
		return (sst ? sst : fup->fu_stats_ret);

	} else {
		/*
		 * Command queue full
		 */
		fup->fu_stats->st_drv.drv_cm_full++;
		(void) splx(s);
		return (EIO);
	}
}

