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
 * Interrupt processing
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
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

#if defined(sun)
/*
 * Polling interrupt routine
 * 
 * Polling interrupts are handled by calling all interrupt service 
 * routines for a given level until someone claims to have "handled" the 
 * interrupt.
 *
 * Called at interrupt level.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	1 		an interrupt has been serviced
 *	0		no interrupts serviced
 *
 */
int
fore_poll()
{
	int	serviced = 0;
	int	unit;

	/*
	 * See if any of our devices are interrupting
	 */
	for ( unit = 0; unit < fore_nunits; unit++ )
	{
		Fore_unit	*fup = fore_units[unit];

		if (fup == NULL)
			continue;

		serviced += fore_intr((void *)fup);
	}

	/*
	 * Indicate if we handled an interrupt
	 */
	return (serviced ? 1 : 0);
}
#endif	/* defined(sun) */


/*
 * Device interrupt routine
 * 
 * Called at interrupt level.
 *
 * Arguments:
 *	arg		pointer to device unit structure
 *
 * Returns:
 *	1 		device interrupt was serviced
 *	0		no interrupts serviced
 *
 */
#if (defined(BSD) && (BSD <= 199306))
int
#else
void
#endif
fore_intr(arg)
	void	*arg;
{
	Fore_unit	*fup = arg;
	Aali	*aap;
#if (defined(BSD) && (BSD <= 199306))
	int	serviced = 0;
#endif

	/*
	 * Try to prevent stuff happening after we've paniced
	 */
	if (panicstr) {
		goto done;
	}

	/*
	 * Get to the microcode shared memory interface
	 */
	if ((aap = fup->fu_aali) == NULL)
		goto done;

	/*
	 * Has this card issued an interrupt??
	 */
	if (*fup->fu_psr) {

		/*
		 * Indicate that we've serviced an interrupt. 
		 */
#if (defined(BSD) && (BSD <= 199306))
		serviced = 1;
#endif

		/*
		 * Clear the device interrupt
		 */
		if (fup->fu_config.ac_device == DEV_FORE_PCA200E)
			PCA200E_HCR_SET(*fup->fu_ctlreg, PCA200E_CLR_HBUS_INT);
		aap->aali_intr_sent = CP_WRITE(0);

		/*
		 * Reset the watchdog timer
		 */
		fup->fu_timer = FORE_WATCHDOG;

		/*
		 * Device initialization handled separately
		 */
		if ((fup->fu_flags & CUF_INITED) == 0) {

			if (fup->fu_ft4)
				/* may not happen */
				goto done;

			/*
			 * We're just initializing device now, so see if
			 * the initialization command has completed
			 */
			if (CP_READ(aap->aali_init.init_status) & 
						QSTAT_COMPLETED)
				fore_initialize_complete(fup);

			/*
			 * If we're still not inited, none of the host
			 * queues are setup yet
			 */
			if ((fup->fu_flags & CUF_INITED) == 0)
				goto done;
		}

		/*
		 * Drain the queues of completed work
		 */
		fore_cmd_drain(fup);
		fore_recv_drain(fup);
		fore_xmit_drain(fup);

		/*
		 * Supply more buffers to the CP
		 */
		fore_buf_supply(fup);
	}

done:
#if (defined(BSD) && (BSD <= 199306))
	return(serviced);
#else
	return;
#endif
}


/*
 * Watchdog timeout routine
 * 
 * Called when we haven't heard from the card in a while.  Just in case
 * we missed an interrupt, we'll drain the queues and try to resupply the
 * CP with more receive buffers.  If the CP is partially wedged, hopefully
 * this will be enough to get it going again.
 *
 * Called with interrupts locked out.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
void
fore_watchdog(fup)
	Fore_unit	*fup;
{
	/*
	 * Try to prevent stuff happening after we've paniced
	 */
	if (panicstr) {
		return;
	}

	/*
	 * Reset the watchdog timer
	 */
	fup->fu_timer = FORE_WATCHDOG;

	/*
	 * If the device is initialized, nudge it (wink, wink)
	 */
	if (fup->fu_flags & CUF_INITED) {

		/*
		 * Drain the queues of completed work
		 */
		fore_cmd_drain(fup);
		fore_recv_drain(fup);
		fore_xmit_drain(fup);

		/*
		 * Supply more buffers to the CP
		 */
		fore_buf_supply(fup);
	}

	return;
}
