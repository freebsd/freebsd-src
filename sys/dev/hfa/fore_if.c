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
 * Network interface layer support
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_ioctl.h>
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
 * Handle netatm core service interface ioctl requests 
 *
 * Called at splnet.
 *
 * Arguments:
 *	code		ioctl function (sub)code
 *	data		data to/from ioctl
 *	arg		optional code-specific argument
 *
 * Returns:
 *	0 		request processed successfully
 *	error		request failed - reason code
 */
int
fore_atm_ioctl(code, data, arg)
	int	code;
	caddr_t	data;
	caddr_t	arg;
{
	struct atminfreq	*aip = (struct atminfreq *)data;
	struct atm_pif		*pip;
	Fore_unit		*fup;
	caddr_t			buf = aip->air_buf_addr;
	struct air_vinfo_rsp	*avr;
	int			count, len, buf_len = aip->air_buf_len;
	int			err = 0;
	char			ifname[2*IFNAMSIZ];
 

	ATM_DEBUG2("fore_atm_ioctl: code=%d, opcode=%d\n", 
		code, aip->air_opcode);

	switch ( aip->air_opcode ) {

	case AIOCS_INF_VST:
		/*
		 * Get vendor statistics
		 */
		pip = (struct atm_pif *)arg;
		fup = (Fore_unit *)pip;
		if ( pip == NULL )
			return ( ENXIO );
		snprintf ( ifname, sizeof(ifname),
		    "%s%d", pip->pif_name, pip->pif_unit );

		/*
		 * Cast response structure onto user's buffer
		 */
		avr = (struct air_vinfo_rsp *)buf;

		/*
		 * How large is the response structure?
		 */
		len = sizeof(struct air_vinfo_rsp);

		/*
		 * Sanity check - enough room for response structure?
		 */
		if ( buf_len < len )
			return ( ENOSPC );

		/*
		 * Copy interface name into response structure
		 */
		if ((err = copyout ( ifname, avr->avsp_intf, IFNAMSIZ)) != 0)
			break;

		/*
		 * Advance the buffer address and decrement the size
		 */
		buf += len;
		buf_len -= len;

		/*
		 * Get the vendor stats from the hardware
		 */
		count = 0;
		if ( ( err = fore_get_stats ( fup ) ) == 0 )
		{
			/*
			 * Stick as much of it as we have room for 
			 * into the response
			 */
			count = min ( sizeof(Fore_stats), buf_len );

			/*
			 * Copy stats into user's buffer. Return value is
			 * amount of data copied.
			 */
			if ((err = copyout((caddr_t)fup->fu_stats, buf, count)) != 0)
				break;
			buf += count;
			buf_len -= count;
			if ( count < sizeof(Fore_stats) )
				err = ENOSPC;
		}

		/*
		 * Record amount we're returning as vendor info...
		 */
		if ((err = copyout(&count, &avr->avsp_len, sizeof(int))) != 0)
			break;

		/*
		 * Update the reply pointers and lengths
		 */
		aip->air_buf_addr = buf;
		aip->air_buf_len = buf_len;
		break;

	default:
		err = ENOSYS;		/* Operation not supported */
		break;
	}

	return (err);
}


/*
 * Free Fore-specific device resources
 * 
 * Frees all dynamically acquired resources for a device unit.  Before
 * this function is called, the CP will have been reset and our interrupt
 * vectors removed.
 *
 * Arguments:
 *	fup	pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
void
fore_interface_free(fup)
	Fore_unit	*fup;
{

	/*
	 * Free up all of our allocated memory
	 */
	fore_xmit_free(fup);
	fore_recv_free(fup);
	fore_buf_free(fup);
	fore_cmd_free(fup);

	/*
	 * Clear device initialized
	 */
	if (fup->fu_flags & CUF_INITED) {
		fup->fu_flags &= ~CUF_INITED;
	}

	if (fup->fu_flags & FUF_STATCMD) {
		DMA_FREE_ADDR(fup->fu_stats, fup->fu_statsd,
			sizeof(Fore_cp_stats), 0);
		fup->fu_flags &= ~FUF_STATCMD;
	}
	return;
}

