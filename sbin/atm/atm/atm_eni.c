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
 *	@(#) $FreeBSD: src/sbin/atm/atm/atm_eni.c,v 1.3.2.1 2000/07/01 06:02:14 ps Exp $
 *
 */

/*
 * User configuration and display program
 * --------------------------------------
 *
 * Routines for Efficient-specific subcommands
 *
 */

#include <sys/param.h>  
#include <sys/socket.h> 
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h> 
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
#include <dev/hea/eni_stats.h>

#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sbin/atm/atm/atm_eni.c,v 1.3.2.1 2000/07/01 06:02:14 ps Exp $");
#endif


/*
 * Local constants
 */
#define	SHOW_PHY	1
#define	SHOW_ATM	2
#define	SHOW_AAL0	4
#define	SHOW_AAL5	8
#define	SHOW_DRIVER	64


/*
 * Headers for statistics
 */
#define ATM_STATS_HDR \
"%s ATM Layer Statistics\n\
  Cells In   Cells Out\n"

#define AAL0_STATS_HDR \
"%s AAL 0 Statistics\n\
  Cells In   Cells Out  Cell Drops\n"

#define AAL5_STATS_HDR \
"%s AAL 5 Statistics\n\
                        CRC/Len                              CRC   Proto  PDU\n\
  Cells In   Cells Out  Errs   Drops    PDUs In   PDUs Out   Errs  Errs   Drops\n"

#define DRIVER_STATS_HDR_1 \
"%s Device Driver Statistics\n\
  Buf    Buf    Buf    Buf  Can't    VCC    VCC     No     No  No RX     RX\n\
  Req     No     No  Alrdy   Find    PDU  Range  Resrc     RX    DMA  Queue\n\
 Size  Descr    Mem   Free  Descr   Size  Error     In   Bufs   Room   Full\n"

#define DRIVER_STATS_HDR_2 \
"%s Device Driver Statistics\n\
   No    ATM  No RX  No TX    Seg           Max       No     No    No TX\n\
   RX  IntrQ    DMA    DMA    Not    Seg    Seg       TX  Resrc      DMA\n\
  VCC   Full   Room   Addr  Align    Pad    Out      Buf    Out     Room\n"

#define OC3_STATS_HDR \
"%s OC-3c Statistics\n\
Section     Path    Line      Line     Path     Corr   Uncorr\n\
BIP8        BIP8    BIP24     FEBE     FEBE     HCS    HCS\n\
Errs        Errs    Errs      Errs     Errs     Errs   Errs\n"


/*
 * Process show ENI statistics command
 *
 * The statistics printed are vendor-specific, depending on the brand of
 * the interface card.
 * 
 * Command format: 
 *	atm show stats interface [<interface-name> [phy | dev | atm |
		aal0 | aal5 | driver ]]
 *
 * Arguments:
 *	intf	interface to print statistics for
 *	argc	number of remaining arguments to command
 *	argv	pointer to remaining argument strings
 *
 * Returns:
 *	none
 *
 */
void
show_eni_stats(intf, argc, argv)
	char		*intf;
	int		argc;
	char		**argv;
{
	int	buf_len, stats_type;
	struct atminfreq	air;
	struct air_vinfo_rsp	*stats;

	/*
	 * Get statistics type qualifier
	 */
	if (!strcasecmp("phy", argv[0])) {
		stats_type = SHOW_PHY;
	} else if (!strcasecmp("atm", argv[0])) {
		stats_type = SHOW_ATM;
	} else if (!strcasecmp("aal0", argv[0])) {
		stats_type = SHOW_AAL0;
	} else if (!strcasecmp("aal5", argv[0])) {
		stats_type = SHOW_AAL5;
	} else if (!strcasecmp("driver", argv[0])) {
		stats_type = SHOW_DRIVER;
	} else {
		fprintf(stderr, "%s: Illegal or unsupported statistics type\n", prog);
		exit(1);
	}
	argc--; argv++;

	/*
	 * Get vendor-specific statistics from the kernel
	 */
	UM_ZERO(&air, sizeof(air));
	air.air_opcode = AIOCS_INF_VST;
	strcpy(air.air_vinfo_intf, intf);
	buf_len = do_info_ioctl(&air, sizeof(struct air_vinfo_rsp) + 1024);
	if (buf_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					intf);
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}
	stats = (struct air_vinfo_rsp *) air.air_buf_addr;

	/*
	 * Print the statistics
	 */
	if (buf_len < sizeof(struct air_vinfo_rsp) +
			sizeof(Eni_stats)) {
		UM_FREE(stats);
		return;
	}

	switch (stats_type) {
	case SHOW_PHY:
		print_eni_oc3(stats);
		break;
	case SHOW_ATM:
		print_eni_atm(stats);
		break;
	case SHOW_AAL0:
		print_eni_aal0(stats);
		break;
	case SHOW_AAL5:
		print_eni_aal5(stats);
		break;
	case SHOW_DRIVER:
		print_eni_driver(stats);
		break;
	}

	UM_FREE(stats);
}


/*
 * Print ENI OC-3c statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_eni_oc3(vi)
	struct air_vinfo_rsp	*vi;
{
	Eni_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Eni_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(OC3_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the OC-3c info
	 */
	printf("%7ld  %7ld  %7ld  %7ld  %7ld  %7ld  %7ld\n",
			stats->eni_st_oc3.oc3_sect_bip8,
			stats->eni_st_oc3.oc3_path_bip8,
			stats->eni_st_oc3.oc3_line_bip24,
			stats->eni_st_oc3.oc3_line_febe,
			stats->eni_st_oc3.oc3_path_febe,
			stats->eni_st_oc3.oc3_hec_corr,
			stats->eni_st_oc3.oc3_hec_uncorr);
}


/*
 * Print ENI ATM statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_eni_atm(vi)
	struct air_vinfo_rsp	*vi;
{
	Eni_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Eni_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(ATM_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the ATM layer info
	 */
	printf("%10ld  %10ld\n",
			stats->eni_st_atm.atm_rcvd,
			stats->eni_st_atm.atm_xmit);
}


/*
 * Print ENI AAL 0 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_eni_aal0(vi)
	struct air_vinfo_rsp	*vi;
{
	Eni_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Eni_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(AAL0_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the AAL 0 info
	 */
	printf("%10ld  %10ld  %10ld\n",
			stats->eni_st_aal0.aal0_rcvd,
			stats->eni_st_aal0.aal0_xmit,
			stats->eni_st_aal0.aal0_drops);
}


/*
 * Print ENI AAL 5 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_eni_aal5(vi)
	struct air_vinfo_rsp	*vi;
{
	Eni_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Eni_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(AAL5_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the AAL 5 info
	 */
	printf("%10ld  %10ld  %5ld  %5ld  %9ld  %9ld  %5ld  %5ld  %5ld\n",
			stats->eni_st_aal5.aal5_rcvd,
			stats->eni_st_aal5.aal5_xmit,
			stats->eni_st_aal5.aal5_crc_len,
			stats->eni_st_aal5.aal5_drops,
			stats->eni_st_aal5.aal5_pdu_rcvd,
			stats->eni_st_aal5.aal5_pdu_xmit,
			stats->eni_st_aal5.aal5_pdu_crc,
			stats->eni_st_aal5.aal5_pdu_errs,
			stats->eni_st_aal5.aal5_pdu_drops);
}

/*
 * Print Efficient device driver statistics
 *
 * Arguments:
 *      vi      pointer to vendor-specific statistics to print
 *
 * Returns:
 *      none
 *
 */
void
print_eni_driver(vi)
        struct air_vinfo_rsp    *vi;
{
        Eni_stats       *stats;

        /*
         * Bump stats pointer past header info
         */
        stats = (Eni_stats *)
                        ((u_long) vi + sizeof(struct air_vinfo_rsp));

        /*
         * Print 1st header
         */
        printf(DRIVER_STATS_HDR_1, get_adapter_name(vi->avsp_intf));

        /*
         * Print the driver info
         */
        printf ( "%5ld  %5ld  %5ld  %5ld  %5ld  %5ld  %5ld  %5ld  %5ld  %5ld  %5ld\n",
                stats->eni_st_drv.drv_mm_toobig,
                stats->eni_st_drv.drv_mm_nodesc,
                stats->eni_st_drv.drv_mm_nobuf,
                stats->eni_st_drv.drv_mm_notuse,
                stats->eni_st_drv.drv_mm_notfnd,
                stats->eni_st_drv.drv_vc_maxpdu,
                stats->eni_st_drv.drv_vc_badrng,
                stats->eni_st_drv.drv_rv_norsc,
                stats->eni_st_drv.drv_rv_nobufs,
                stats->eni_st_drv.drv_rv_nodma,
                stats->eni_st_drv.drv_rv_rxq
        );

        /*
         * Print 2nd header
         */
        printf(DRIVER_STATS_HDR_2, get_adapter_name(vi->avsp_intf));

        /*
         * Print the driver info
         */
        printf ( "%5ld  %5ld  %5ld  %5ld  %5ld  %5ld  %5ld  %7ld  %5ld  %7ld\n",
                stats->eni_st_drv.drv_rv_novcc,
                stats->eni_st_drv.drv_rv_intrq,
                stats->eni_st_drv.drv_rv_segdma,
                stats->eni_st_drv.drv_xm_segdma,
                stats->eni_st_drv.drv_xm_segnoal,
                stats->eni_st_drv.drv_xm_seglen,
                stats->eni_st_drv.drv_xm_maxpdu,
                stats->eni_st_drv.drv_xm_nobuf,
                stats->eni_st_drv.drv_xm_norsc,
                stats->eni_st_drv.drv_xm_nodma
        );


}

