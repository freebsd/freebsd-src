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
 * Cell Processor (CP) initialization routines
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/kernel.h>
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
 * Local functions
 */
static void	fore_get_prom __P((Fore_unit *));


/*
 * Begin CP Initialization
 *
 * This function will poll for the successful downloading and starting of
 * the CP microcode program.  After the microcode is running, we will allocate
 * any needed kernel memory (must do it in non-interrupt mode), build the CP
 * queue configurations and issue an Initialize command to the CP.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_initialize(fup)
	Fore_unit	*fup;
{
	Aali		*aap;
	Init_parms	*inp;
	caddr_t		errmsg;
	u_long		vers;

	/*
	 * Must wait until firmware has been downloaded and is running
	 */
	if (CP_READ(fup->fu_mon->mon_bstat) != BOOT_RUNNING) {

		/*
		 * Try again later
		 */
		fup->fu_thandle = 
			timeout((KTimeout_ret(*) __P((void *)))fore_initialize,
				(void *)fup, hz);
		return;
	} else
		callout_handle_init(&fup->fu_thandle);

	/*
	 * Allocate queues and whatever else is needed
	 */
	if (fore_xmit_allocate(fup)) {
		errmsg = "transmit queue allocation";
		goto failed;
	}
	if (fore_recv_allocate(fup)) {
		errmsg = "receive queue allocation";
		goto failed;
	}
	if (fore_buf_allocate(fup)) {
		errmsg = "buffer supply queue allocation";
		goto failed;
	}
	if (fore_cmd_allocate(fup)) {
		errmsg = "command queue allocation";
		goto failed;
	}

	/*
	 * CP microcode is downloaded - locate shared memory interface 
	 */
	aap = (Aali *)(fup->fu_ram + CP_READ(fup->fu_mon->mon_appl));
	fup->fu_aali = aap;

	/*
	 * Pick out any interesting info from the microcode
	 */
	vers = CP_READ(aap->aali_ucode_ver);
	if (vers < FORE_MIN_UCODE) {
		errmsg = "unsupported microcode version";
		goto failed;
	}
	snprintf(fup->fu_config.ac_firm_vers,
	    sizeof(fup->fu_config.ac_firm_vers), "%ld.%ld.%ld",
		(vers >> 16) & 0xff, (vers >> 8) & 0xff, vers & 0xff);

#ifdef notdef
	/*
	 * Turn on CP debugging
	 */
	aap->aali_hostlog = 1;
#endif

	/*
	 * Build the initialization block
	 */
	inp = &aap->aali_init;
	inp->init_numvcc = CP_WRITE(FORE_MAX_VCC);
	inp->init_cmd_elem = CP_WRITE(CMD_QUELEN);
	inp->init_xmit_elem = CP_WRITE(XMIT_QUELEN);
	inp->init_recv_elem = CP_WRITE(RECV_QUELEN);
	inp->init_recv_ext = CP_WRITE(RECV_EXTRA_SEGS);
	inp->init_xmit_ext = CP_WRITE(XMIT_EXTRA_SEGS);
	inp->init_buf1s.bfs_quelen = CP_WRITE(BUF1_SM_QUELEN);
	inp->init_buf1s.bfs_bufsize = CP_WRITE(BUF1_SM_SIZE);
	inp->init_buf1s.bfs_cppool = CP_WRITE(BUF1_SM_CPPOOL);
	inp->init_buf1s.bfs_entsize = CP_WRITE(BUF1_SM_ENTSIZE);
	inp->init_buf1l.bfs_quelen = CP_WRITE(BUF1_LG_QUELEN);
	inp->init_buf1l.bfs_bufsize = CP_WRITE(BUF1_LG_SIZE);
	inp->init_buf1l.bfs_cppool = CP_WRITE(BUF1_LG_CPPOOL);
	inp->init_buf1l.bfs_entsize = CP_WRITE(BUF1_LG_ENTSIZE);
	inp->init_buf2s.bfs_quelen = CP_WRITE(0);
	inp->init_buf2s.bfs_bufsize = CP_WRITE(0);
	inp->init_buf2s.bfs_cppool = CP_WRITE(0);
	inp->init_buf2s.bfs_entsize = CP_WRITE(0);
	inp->init_buf2l.bfs_quelen = CP_WRITE(0);
	inp->init_buf2l.bfs_bufsize = CP_WRITE(0);
	inp->init_buf2l.bfs_cppool = CP_WRITE(0);
	inp->init_buf2l.bfs_entsize = CP_WRITE(0);

	/*
	 * Enable device interrupts
	 */
	aap->aali_intr_ena = CP_WRITE(1);

	/*
	 * Issue the Initialize command to the CP and wait for
	 * the CP to interrupt to signal completion
	 */
	inp->init_status = CP_WRITE(QSTAT_PENDING);
	inp->init_cmd = CP_WRITE(CMD_INIT | CMD_INTR_REQ);
	return;

failed:
	/*
	 * Initialization failure
	 */
	fore_interface_free(fup);
	log(LOG_ERR, "fore initialization failed: intf=%s%d, err=%s\n",
		fup->fu_pif.pif_name, fup->fu_pif.pif_unit, errmsg);
	return;
}


/*
 * Complete CP Initialization 
 *
 * Called after the CP has successfully completed processing of the 
 * Initialize command.  We will now finish off our part of the 
 * initialization process by setting up all the host-based queue 
 * management structures.
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
fore_initialize_complete(fup)
	Fore_unit	*fup;
{
	Aali		*aap = fup->fu_aali;

	/*
	 * Log an initialization failure
	 */
	if (CP_READ(aap->aali_init.init_status) & QSTAT_ERROR) {

		log(LOG_ERR, 
			"fore initialization failed: intf=%s%d, hbeat=0x%lx\n",
			fup->fu_pif.pif_name, fup->fu_pif.pif_unit,
			(u_long)CP_READ(aap->aali_heartbeat));
		return;
	}

	ATM_DEBUG1("heap=0x%lx\n", aap->aali_heap);
	ATM_DEBUG1("heaplen=0x%lx\n", aap->aali_heaplen);
	ATM_DEBUG1("cmd_q=0x%lx\n", aap->aali_cmd_q);
	ATM_DEBUG1("xmit_q=0x%lx\n", aap->aali_xmit_q);
	ATM_DEBUG1("recv_q=0x%lx\n", aap->aali_recv_q);
	ATM_DEBUG1("buf1s_q=0x%lx\n", aap->aali_buf1s_q);
	ATM_DEBUG1("buf1l_q=0x%lx\n", aap->aali_buf1l_q);
	ATM_DEBUG1("buf2s_q=0x%lx\n", aap->aali_buf2s_q);
	ATM_DEBUG1("buf2l_q=0x%lx\n", aap->aali_buf2l_q);

	/*
	 * Initialize all of our queues
	 */
	fore_xmit_initialize(fup);
	fore_recv_initialize(fup);
	fore_buf_initialize(fup);
	fore_cmd_initialize(fup);

	/*
	 * Mark device initialization completed
	 */
	fup->fu_flags |= CUF_INITED;

	fore_get_prom(fup);
	return;
}


/*
 * Get device PROM values from CP
 * 
 * This function will issue a GET_PROM command to the CP in order to
 * initiate the DMA transfer of the CP's PROM structure to the host.
 * This will be called after CP initialization has completed.
 * There is (currently) no retry if this fails.
 *
 * Called at interrupt level.
 *
 * Arguments:
 *	fup	pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
static void
fore_get_prom(fup)
	Fore_unit	*fup;
{
	H_cmd_queue	*hcp;
	Cmd_queue	*cqp;

	/*
	 * Queue command at end of command queue
	 */
	hcp = fup->fu_cmd_tail;
	if ((*hcp->hcq_status) & QSTAT_FREE) {

		/*
		 * Queue entry available, so set our view of things up
		 */
		hcp->hcq_code = CMD_GET_PROM;
		hcp->hcq_arg = NULL;
		fup->fu_cmd_tail = hcp->hcq_next;

		/*
		 * Now set the CP-resident queue entry - the CP will grab
		 * the command when the op-code is set.
		 */
		cqp = hcp->hcq_cpelem;
		(*hcp->hcq_status) = QSTAT_PENDING;

		fup->fu_promd = DMA_GET_ADDR(fup->fu_prom, sizeof(Fore_prom),
			FORE_PROM_ALIGN, 0);
		if (fup->fu_promd == NULL) {
			fup->fu_stats->st_drv.drv_cm_nodma++;
			return;
		}
		cqp->cmdq_prom.prom_buffer = (CP_dma) CP_WRITE(fup->fu_promd);
		cqp->cmdq_prom.prom_cmd = CP_WRITE(CMD_GET_PROM | CMD_INTR_REQ);

	} else {
		/*
		 * Command queue full
		 */
		fup->fu_stats->st_drv.drv_cm_full++;
	}

	return;
}

