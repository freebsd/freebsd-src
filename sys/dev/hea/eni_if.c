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
 * Efficient ENI Adapter Support
 * -----------------------------
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
#include <netinet/in.h>
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

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_suni.h>
#include <dev/hea/eni_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

static void	eni_get_stats __P((Eni_unit *));

/*
 * SUNI statistics counters take one of three forms:
 *	single byte value 	(0x0 - 0xff)
 *	two byte value		(0x0 - 0xffff)
 *	two + 1/2 (three) byte value
 *				(0x0 - 0x0fffff)
 */
#define	READ_ONE(x)	( (eup->eu_suni[(x)] & 0xff) )

#define	READ_TWO(x)	( (eup->eu_suni[(x)+1] & 0xff) << 8 | \
			  (eup->eu_suni[(x)] & 0xff) )

#define	READ_THREE(x)	( (eup->eu_suni[(x)+2] & 0xf) << 16 | \
			  (eup->eu_suni[(x)+1] & 0xff) << 8 | \
			  (eup->eu_suni[(x)] & 0xff) )

/*
 * Do an initial read of the error counters without saving them.
 * In effect, this will "zero" our idea of the number of errors
 * which have occurred since the driver was loaded.
 *
 * Arguments:
 *	eup		pointer to per unit structure
 *
 * Returns:
 *	none
 *
 */
void
eni_zero_stats ( eup )
	Eni_unit *eup;
{
	int	val;

	/*
	 * Write the SUNI master control register which
	 * will cause all the statistics counters to be
	 * loaded.
	 */
	eup->eu_suni[SUNI_MASTER_REG] = eup->eu_suni[SUNI_MASTER_REG];

	/*
	 * Delay to allow for counter load time...
	 */
	DELAY ( SUNI_DELAY );

	/*
	 * Statistics counters contain the number of events
	 * since the last time the counter was read.
	 */
	val = READ_TWO ( SUNI_SECT_BIP_REG );		/* oc3_sect_bip8 */
	val = READ_TWO ( SUNI_PATH_BIP_REG );		/* oc3_path_bip8 */
	val = READ_THREE ( SUNI_LINE_BIP_REG );		/* oc3_line_bip24 */
	val = READ_THREE ( SUNI_LINE_FEBE_REG );	/* oc3_line_febe */
	val = READ_TWO ( SUNI_PATH_FEBE_REG );		/* oc3_path_febe */
	val = READ_ONE ( SUNI_HECS_REG );		/* oc3_hec_corr */
	val = READ_ONE ( SUNI_UHECS_REG );		/* oc3_hec_uncorr */
}

/*
 * Retrieve SUNI stats
 *
 * Arguments:
 *	eup		pointer to per unit structure
 *
 * Returns:
 *	none
 *
 */
static void
eni_get_stats ( eup )
	Eni_unit	*eup;
{
	/*
	 * Write the SUNI master control register which
	 * will cause all the statistics counters to be
	 * loaded.
	 */
	eup->eu_suni[SUNI_MASTER_REG] = eup->eu_suni[SUNI_MASTER_REG];

	/*
	 * Delay to allow for counter load time...
	 */
	DELAY ( 10 );

	/*
	 * Statistics counters contain the number of events
	 * since the last time the counter was read.
	 */
	eup->eu_stats.eni_st_oc3.oc3_sect_bip8 +=
			READ_TWO ( SUNI_SECT_BIP_REG );
	eup->eu_stats.eni_st_oc3.oc3_path_bip8 +=
			READ_TWO ( SUNI_PATH_BIP_REG );
	eup->eu_stats.eni_st_oc3.oc3_line_bip24 +=
			READ_THREE ( SUNI_LINE_BIP_REG );
	eup->eu_stats.eni_st_oc3.oc3_line_febe +=
			READ_THREE ( SUNI_LINE_FEBE_REG );
	eup->eu_stats.eni_st_oc3.oc3_path_febe +=
			READ_TWO ( SUNI_PATH_FEBE_REG );
	eup->eu_stats.eni_st_oc3.oc3_hec_corr +=
			READ_ONE ( SUNI_HECS_REG );
	eup->eu_stats.eni_st_oc3.oc3_hec_uncorr +=
			READ_ONE ( SUNI_UHECS_REG );
}

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
 *	0		request processed successfully
 *	error		request failed - reason code
 *
 */
int
eni_atm_ioctl ( code, data, arg )
	int code;
	caddr_t	data;
	caddr_t	arg;
{
	struct atminfreq	*aip = (struct atminfreq *)data;
	struct atm_pif		*pip = (struct atm_pif *)arg;
	Eni_unit		*eup = (Eni_unit *)pip;
	caddr_t			buf = aip->air_buf_addr;
	struct air_vinfo_rsp	*avr;
	int			count, len, buf_len = aip->air_buf_len;
	int			err = 0;
	char			ifname[2*IFNAMSIZ];

	ATM_DEBUG2("eni_atm_ioctl: code=%d, opcode=%d\n",
		code, aip->air_opcode );

	switch ( aip->air_opcode ) {

	case AIOCS_INF_VST:
		/*
		 * Get vendor statistics
		 */
		if ( eup == NULL )
			return ( ENXIO );
		snprintf ( ifname, sizeof(ifname),
		    "%s%d", pip->pif_name, pip->pif_unit );

		/*
		 * Cast response structure onto user's buffer
		 */
		avr = (struct air_vinfo_rsp *)buf;

		/*
		 * How large is the response structure
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
		if ((err = copyout(ifname, avr->avsp_intf, IFNAMSIZ)) != 0)
			break;

		/*
		 * Advance the buffer address and decrement the size
		 */
		buf += len;
		buf_len -= len;

		/*
		 * Get the vendor stats (SUNI) from the hardware
		 */
		eni_get_stats ( eup );
		/*
		 * Stick as much of it as we have room for
		 * into the response
		 */
		count = MIN ( sizeof(Eni_stats), buf_len );

		/*
		 * Copy stats into user's buffer. Return value is
		 * amount of data copied.
		 */
		if ((err = copyout((void *)&eup->eu_stats, buf, count)) != 0)
				break;
		buf += count;
		buf_len -= count;
		if ( count < sizeof(Eni_stats) )
			err = ENOSPC;

		/*
		 * Record amount we're returning as vendor info...
		 */
		if ((err = copyout(&count, &avr->avsp_len, sizeof(int))) != 0)
			break;

		/*
		 * Update the reply pointers and length
		 */
		aip->air_buf_addr = buf;
		aip->air_buf_len = buf_len;
		break;

	default:
		err = ENOSYS;		/* Operation not supported */
		break;
	}

	return ( err );

}

