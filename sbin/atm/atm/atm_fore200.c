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
 * User configuration and display program
 * --------------------------------------
 *
 * Routines for Fore SBA-200-specific subcommands
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
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>

#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "atm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local constants
 */
#define	SHOW_PHY	1
#define	SHOW_DEV	2
#define	SHOW_ATM	4
#define	SHOW_AAL0	8
#define	SHOW_AAL4	16
#define	SHOW_AAL5	32
#define	SHOW_DRIVER	64


/*
 * Headers for statistics
 */
#define TAXI_STATS_HDR \
"%s TAXI Statistics\n\
  CRC Errs  Framing Errs\n"

#define DEV_STATS_HDR \
"%s Device Statistics\n\
Type 1      Type 1      Type 2      Type 2\n\
Small Buff  Large Buff  Small Buff  Large Buff  Receive     Receive\n\
Alloc Fail  Alloc Fail  Alloc Fail  Alloc Fail  Queue Full  Carrier\n"

#define ATM_STATS_HDR \
"%s ATM Layer Statistics\n\
  Cells In   Cells Out   VPI Range  VPI NoConn   VCI Range  VCI NoConn\n"

#define AAL0_STATS_HDR \
"%s AAL 0 Statistics\n\
  Cells In   Cells Out  Cell Drops\n"

#define AAL4_STATS_HDR \
"%s AAL 4 Statistics\n\
                         CRC   Proto  Cell                          PDU   PDU\n\
  Cells In   Cells Out   Errs  Errs   Drops    PDUs In   PDUs Out   Errs  Drops\n"

#define AAL5_STATS_HDR \
"%s AAL 5 Statistics\n\
                        CRC/Len                              CRC   Proto  PDU\n\
  Cells In   Cells Out  Errs   Drops    PDUs In   PDUs Out   Errs  Errs   Drops\n"

#define DRIVER_STATS_HDR \
"%s Device Driver Statistics\n\
  No  Xmit   Max   Seg          No    No          No    IQ    No   Cmd    No\n\
 VCC Queue   Seg   Not   Seg   DMA   VCC    No  Mbuf  Full   DMA Queue   DMA\n\
 Out  Full  Size Align   Pad   Out    In  Buff    In    In   Sup  Full   Cmd\n"

#define OC3_STATS_HDR \
"%s OC-3c Statistics\n\
Section     Path    Line      Line     Path     Corr   Uncorr\n\
BIP8        BIP8    BIP24     FEBE     FEBE     HCS    HCS\n\
Errs        Errs    Errs      Errs     Errs     Errs   Errs\n"

static void	print_fore200_taxi(struct air_vinfo_rsp *);
static void	print_fore200_oc3(struct air_vinfo_rsp *);
static void	print_fore200_dev(struct air_vinfo_rsp *);
static void	print_fore200_atm(struct air_vinfo_rsp *);
static void	print_fore200_aal0(struct air_vinfo_rsp *);
static void	print_fore200_aal4(struct air_vinfo_rsp *);
static void	print_fore200_aal5(struct air_vinfo_rsp *);
static void	print_fore200_driver(struct air_vinfo_rsp *);

/*
 * Process show Fore SBA-200 statistics command
 *
 * The statistics printed are vendor-specific, depending on the brand of
 * the interface card.
 * 
 * Command format: 
 *	atm show stats interface [<interface-name> [phy | dev | atm |
 *		aal0 | aal4 | aal5 | driver]]
 *
 * Arguments:
 *	intf	interface statistics are for
 *	argc	number of remaining arguments to command
 *	argv	pointer to remaining argument strings
 *
 * Returns:
 *	none
 *
 */
void
show_fore200_stats(intf, argc, argv)
	char		*intf;
	int		argc;
	char		**argv;
{
	int stats_type;
	ssize_t buf_len;
	struct air_cfg_rsp	*cfg;
	struct air_vinfo_rsp	*stats;
	struct atminfreq	air;

	/*
	 * Get statistics type qualifier
	 */
	if (!strcasecmp("phy", argv[0])) {
		stats_type = SHOW_PHY;
	} else if (!strcasecmp("dev", argv[0])) {
		stats_type = SHOW_DEV;
	} else if (!strcasecmp("atm", argv[0])) {
		stats_type = SHOW_ATM;
	} else if (!strcasecmp("aal0", argv[0])) {
		stats_type = SHOW_AAL0;
	} else if (!strcasecmp("aal4", argv[0])) {
		stats_type = SHOW_AAL4;
	} else if (!strcasecmp("aal5", argv[0])) {
		stats_type = SHOW_AAL5;
	} else if (!strcasecmp("driver", argv[0])) {
		stats_type = SHOW_DRIVER;
	} else {
		errx(1, "Illegal statistics type");
	}
	argc--; argv++;

	/*
	 * Get adapter configuration from the kernel
	 */
	bzero(&air, sizeof(air));
	air.air_opcode = AIOCS_INF_CFG;
	strcpy(air.air_cfg_intf, intf);
	buf_len = do_info_ioctl(&air, sizeof(struct air_cfg_rsp));
	if (buf_len == -1) {
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			err(1, "Internal error");
		case ENXIO:
			errx(1, "%s is not an ATM device", intf);
		default:
			err(1, "ioctl (AIOCINFO)");
		}
	}
	cfg = (struct air_cfg_rsp *)(void *)air.air_buf_addr;

	/*
	 * Get vendor-specific statistics from the kernel
	 */
	bzero(&air, sizeof(air));
	air.air_opcode = AIOCS_INF_VST;
	strcpy(air.air_vinfo_intf, intf);
	buf_len = do_info_ioctl(&air, sizeof(struct air_vinfo_rsp) + 1024);
	if (buf_len == -1) {
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			err(1, "Internal error");
		case ENXIO:
			errx(1, "%s is not an ATM device", intf);
		default:
			err(1, "ioctl (AIOCINFO)");
		}
	}
	stats = (struct air_vinfo_rsp *)(void *)air.air_buf_addr;

	/*
	 * Print the statistics
	 */
	if ((size_t)buf_len < sizeof(struct air_vinfo_rsp) +
			sizeof(Fore_stats)) {
		free(stats);
		free(cfg);
		return;
	}

	switch (stats_type) {
	case SHOW_PHY:
		switch (cfg->acp_media) {
		case MEDIA_TAXI_100:
		case MEDIA_TAXI_140:
			print_fore200_taxi(stats);
			break;
		case MEDIA_OC3C:
			print_fore200_oc3(stats);
			break;
		case MEDIA_OC12C:
			break;
		default:
			break;
		}
		break;
	case SHOW_DEV:
		print_fore200_dev(stats);
		break;
	case SHOW_ATM:
		print_fore200_atm(stats);
		break;
	case SHOW_AAL0:
		print_fore200_aal0(stats);
		break;
	case SHOW_AAL4:
		print_fore200_aal4(stats);
		break;
	case SHOW_AAL5:
		print_fore200_aal5(stats);
		break;
	case SHOW_DRIVER:
		print_fore200_driver(stats);
		break;
	}

	free(stats);
	free(cfg);
}


/*
 * Print Fore ASX-200 TAXI statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_fore200_taxi(vi)
	struct air_vinfo_rsp	*vi;
{
	Fore_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Fore_stats *) 
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(TAXI_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the physical layer info
	 */
	printf("%10ld  %12ld\n",
			stats->st_taxi.taxi_bad_crc,
			stats->st_taxi.taxi_framing);
}


/*
 * Print Fore ASX-200 OC-3c statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_fore200_oc3(vi)
	struct air_vinfo_rsp	*vi;
{
	Fore_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Fore_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(OC3_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the OC-3c info
	 */
	printf("%7ld  %7ld  %7ld  %7ld  %7ld  %7ld  %7ld\n",
			stats->st_oc3.oc3_sect_bip8,
			stats->st_oc3.oc3_path_bip8,
			stats->st_oc3.oc3_line_bip24,
			stats->st_oc3.oc3_line_febe,
			stats->st_oc3.oc3_path_febe,
			stats->st_oc3.oc3_hec_corr,
			stats->st_oc3.oc3_hec_uncorr);
}


/*
 * Print Fore ASX-200 device statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_fore200_dev(vi)
	struct air_vinfo_rsp	*vi;
{
	Fore_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Fore_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(DEV_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the device info
	 */
	printf("%10ld  %10ld  %10ld  %10ld  %10ld  %s\n",
			stats->st_misc.buf1_sm_fail,
			stats->st_misc.buf1_lg_fail,
			stats->st_misc.buf2_sm_fail,
			stats->st_misc.buf2_lg_fail,
			stats->st_misc.rcvd_pdu_fail,
			(stats->st_misc.carrier_status ? "On" : "Off"));
}


/*
 * Print Fore ASX-200 ATM statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_fore200_atm(vi)
	struct air_vinfo_rsp	*vi;
{
	Fore_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Fore_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(ATM_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the ATM layer info
	 */
	printf("%10ld  %10ld  %10ld  %10ld  %10ld  %10ld\n",
			stats->st_atm.atm_rcvd,
			stats->st_atm.atm_xmit,
			stats->st_atm.atm_vpi_range,
			stats->st_atm.atm_vpi_noconn,
			stats->st_atm.atm_vci_range,
			stats->st_atm.atm_vci_noconn);
}


/*
 * Print Fore ASX-200 AAL 0 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_fore200_aal0(vi)
	struct air_vinfo_rsp	*vi;
{
	Fore_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Fore_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(AAL0_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the AAL 0 info
	 */
	printf("%10ld  %10ld  %10ld\n",
			stats->st_aal0.aal0_rcvd,
			stats->st_aal0.aal0_xmit,
			stats->st_aal0.aal0_drops);
}


/*
 * Print Fore ASX-200 AAL 4 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_fore200_aal4(vi)
	struct air_vinfo_rsp	*vi;
{
	Fore_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Fore_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(AAL4_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the AAL 4 info
	 */
	printf("%10ld  %10ld  %5ld  %5ld  %5ld  %9ld  %9ld  %5ld  %5ld\n",
			stats->st_aal4.aal4_rcvd,
			stats->st_aal4.aal4_xmit,
			stats->st_aal4.aal4_crc,
			stats->st_aal4.aal4_sar_cs,
			stats->st_aal4.aal4_drops,
			stats->st_aal4.aal4_pdu_rcvd,
			stats->st_aal4.aal4_pdu_xmit,
			stats->st_aal4.aal4_pdu_errs,
			stats->st_aal4.aal4_pdu_drops);
}


/*
 * Print Fore ASX-200 AAL 5 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_fore200_aal5(vi)
	struct air_vinfo_rsp	*vi;
{
	Fore_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Fore_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(AAL5_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the AAL 5 info
	 */
	printf("%10ld  %10ld  %5ld  %5ld  %9ld  %9ld  %5ld  %5ld  %5ld\n",
			stats->st_aal5.aal5_rcvd,
			stats->st_aal5.aal5_xmit,
			stats->st_aal5.aal5_crc_len,
			stats->st_aal5.aal5_drops,
			stats->st_aal5.aal5_pdu_rcvd,
			stats->st_aal5.aal5_pdu_xmit,
			stats->st_aal5.aal5_pdu_crc,
			stats->st_aal5.aal5_pdu_errs,
			stats->st_aal5.aal5_pdu_drops);
}


/*
 * Print Fore ASX-200 device driver statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_fore200_driver(vi)
	struct air_vinfo_rsp	*vi;
{
	Fore_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Fore_stats *)
			((u_long) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(DRIVER_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the driver info
	 */
	printf("%4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld\n",
			stats->st_drv.drv_xm_notact,
			stats->st_drv.drv_xm_full,
			stats->st_drv.drv_xm_maxpdu,
			stats->st_drv.drv_xm_segnoal,
			stats->st_drv.drv_xm_seglen,
			stats->st_drv.drv_xm_segdma,
			stats->st_drv.drv_rv_novcc,
			stats->st_drv.drv_rv_nosbf,
			stats->st_drv.drv_rv_nomb,
			stats->st_drv.drv_rv_ifull,
			stats->st_drv.drv_bf_segdma,
			stats->st_drv.drv_cm_full,
			stats->st_drv.drv_cm_nodma);

}
