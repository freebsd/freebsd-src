/*
 * Copyright (c) 2003 Vincent Jardin 6WIND
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * User configuration and display program
 * --------------------------------------
 *
 * Routines for PROATM-155 and 25 specific subcommands
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#include <dev/proatm/proatm.h>

#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atm.h"

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

static void print_proatm_utp25(struct air_vinfo_rsp *vi);
static void print_proatm_oc3(struct air_vinfo_rsp *vi);
static void print_proatm_atm(struct air_vinfo_rsp *vi);
static void print_proatm_aal0(struct air_vinfo_rsp *vi);
static void print_proatm_aal4(struct air_vinfo_rsp *vi);
static void print_proatm_aal5(struct air_vinfo_rsp *vi);
static void print_proatm_driver(struct air_vinfo_rsp *vi);
static void print_proatm_dev(struct air_vinfo_rsp *vi);

/*
 * Process show PROATM statistics command
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
show_proatm_stats(intf, argc, argv)
	char		*intf;
	int		argc;
	char		**argv;
{
	int			buf_len, stats_type;
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
		fprintf(stderr, "%s: Illegal statistics type\n", prog);
		exit(1);
	}
	argc--; argv++;

	/*
	 * Get adapter configuration from the kernel
	 */
	bzero(&air, sizeof(air));
	air.air_opcode = AIOCS_INF_CFG;
	strcpy(air.air_cfg_intf, intf);
	buf_len = do_info_ioctl(&air, sizeof(struct air_cfg_rsp));
	if (buf_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n", intf);
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}
	cfg = (struct air_cfg_rsp *) air.air_buf_addr;

	/*
	 * Get vendor-specific statistics from the kernel
	 */
	bzero(&air, sizeof(air));
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
			fprintf(stderr, "%s is not an ATM device\n", intf);
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
	if (buf_len < sizeof(struct air_vinfo_rsp) + sizeof(Proatm_stats)) {
		free(stats);
		free(cfg);
		return;
	}

	switch (stats_type) {
	case SHOW_PHY:
		switch (cfg->acp_media) {
		case MEDIA_UTP25:
			print_proatm_utp25(stats);
			break;
		case MEDIA_OC3C:
			print_proatm_oc3(stats);
			break;
		default:
			break;
		}
		break;
	case SHOW_DEV:
		print_proatm_dev(stats);
		break;
	case SHOW_ATM:
		print_proatm_atm(stats);
		break;
	case SHOW_AAL0:
		print_proatm_aal0(stats);
		break;
	case SHOW_AAL4:
		print_proatm_aal4(stats);
		break;
	case SHOW_AAL5:
		print_proatm_aal5(stats);
		break;
	case SHOW_DRIVER:
		print_proatm_driver(stats);
		break;
	}

	free(stats);
	free(cfg);
}

/*
 * Print PROATM-155 UTP 25.6 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
static void
print_proatm_utp25(struct air_vinfo_rsp *vi)
{
#define UTP25_STATS_HDR \
"%s UTP 25.6 Statistics\n\
Symbol        TX          RX       RX HEC\n\
 Errs         Cells       Cells      Errs\n"

	Proatm_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Proatm_stats *)
			((uintptr_t) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(UTP25_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the UTP 25.6 info
	 */
	printf("%7ld  %10ld  %10ld  %7ld\n",
			stats->proatm_st_utp25.utp25_symbol_errors,
			stats->proatm_st_utp25.utp25_tx_cells,
			stats->proatm_st_utp25.utp25_rx_cells,
			stats->proatm_st_utp25.utp25_rx_hec_errors);
}

/*
 * Print PROATM-155 OC-3c statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
static void
print_proatm_oc3(struct air_vinfo_rsp *vi)
{
#define OC3_STATS_HDR_1 \
"%s OC-3c Statistics\n\
Section     Path    Line      Line     Path     Corr   Uncorr\n\
BIP8        BIP8    BIP24     FEBE     FEBE     HCS    HCS      RX     TX\n\
Errs        Errs    Errs      Errs     Errs     Errs   Errs     Cells  Cells\n"

#define OC3_STATS_HDR_2 \
"%s OC-3c Statistics\n\
     RX          TX\n\
     Cells       Cells\n"

	Proatm_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Proatm_stats *)
			((uintptr_t) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(OC3_STATS_HDR_1, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the OC-3c info
	 */
	printf("%7ld  %7ld  %7ld  %7ld  %7ld  %7ld  %7ld  %7ld  %7ld\n",
			stats->proatm_st_oc3.oc3_sect_bip8,
			stats->proatm_st_oc3.oc3_path_bip8,
			stats->proatm_st_oc3.oc3_line_bip24,
			stats->proatm_st_oc3.oc3_line_febe,
			stats->proatm_st_oc3.oc3_path_febe,
			stats->proatm_st_oc3.oc3_hec_corr,
			stats->proatm_st_oc3.oc3_hec_uncorr,
			stats->proatm_st_oc3.oc3_rx_cells,
			stats->proatm_st_oc3.oc3_tx_cells);

	/*
	 * Print a header
	 */
	printf(OC3_STATS_HDR_2, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the OC-3c info
	 */
	printf("%10ld  %10ld\n",
			stats->proatm_st_oc3.oc3_rx_cells,
			stats->proatm_st_oc3.oc3_tx_cells);
}

/*
 * Print PROATM statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
static void
print_proatm_atm(struct air_vinfo_rsp *vi)
{
#define ATM_STATS_HDR \
"%s ATM Layer Statistics\n\
 Cells In    Cells Out\n"

	Proatm_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Proatm_stats *)
			((uintptr_t) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(ATM_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the ATM layer info
	 */
	printf("%10ld  %10ld\n",
			stats->proatm_st_atm.atm_rcvd,
			stats->proatm_st_atm.atm_xmit);
}

/*
 * Print PROATM AAL 0 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
static void
print_proatm_aal0(struct air_vinfo_rsp *vi)
{
#define AAL0_STATS_HDR \
"%s AAL 0 Statistics\n\
 Cells In    Cells Out  Cell Drops\n"

	Proatm_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Proatm_stats *)
			((uintptr_t) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(AAL0_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the AAL 0 info
	 */
	printf("%10ld  %10ld  %10ld\n",
			stats->proatm_st_aal0.aal0_rcvd,
			stats->proatm_st_aal0.aal0_xmit,
			stats->proatm_st_aal0.aal0_drops);
}

/*
 * Print PROATM AAL 4 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
static void
print_proatm_aal4(struct air_vinfo_rsp *vi)
{
#define AAL4_STATS_HDR \
"%s AAL 3/4 Statistics\n\
                         Cells CS\n\
                         CRC   Proto  Cell                          PDU   PDU\n\
 Cells In    Cells Out   Errs  Errs   Drops    PDUs In   PDUs Out   Errs  Drops\n"

	Proatm_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Proatm_stats *)
			((uintptr_t) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(AAL4_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the AAL 4 info
	 */
	printf("%10ld  %10ld  %5ld  %5ld  %5ld  %9ld  %9ld  %5ld  %5ld\n",
			stats->proatm_st_aal4.aal4_rcvd,
			stats->proatm_st_aal4.aal4_xmit,
			stats->proatm_st_aal4.aal4_crc,
			stats->proatm_st_aal4.aal4_sar_cs,
			stats->proatm_st_aal4.aal4_drops,
			stats->proatm_st_aal4.aal4_pdu_rcvd,
			stats->proatm_st_aal4.aal4_pdu_xmit,
			stats->proatm_st_aal4.aal4_pdu_errs,
			stats->proatm_st_aal4.aal4_pdu_drops);
}

/*
 * Print PROATM AAL 5 statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
static void
print_proatm_aal5(struct air_vinfo_rsp *vi)
{
#define AAL5_STATS_HDR \
"%s AAL 5 Statistics\n\
                        Cells\n\
                       CRC/Len Cell                         PDU CRC  PDU\n\
 Cells In    Cells Out  Errs   Drops    PDUs In   PDUs Out   Errs    Drops\n"

	Proatm_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Proatm_stats *)
			((uintptr_t) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(AAL5_STATS_HDR, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the AAL 5 info
	 */
	printf("%10ld  %10ld  %5ld  %5ld  %9ld  %9ld  %5ld  %9ld\n",
			stats->proatm_st_aal5.aal5_rcvd,
			stats->proatm_st_aal5.aal5_xmit,
			stats->proatm_st_aal5.aal5_crc_len,
			stats->proatm_st_aal5.aal5_drops,
			stats->proatm_st_aal5.aal5_pdu_rcvd,
			stats->proatm_st_aal5.aal5_pdu_xmit,
			stats->proatm_st_aal5.aal5_pdu_crc,
			stats->proatm_st_aal5.aal5_pdu_drops);
}

/*
 * Print Proatm device driver statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
static void
print_proatm_driver(struct air_vinfo_rsp *vi)
{
#define DRIVER_STATS_HDR_1 \
"%s Device Driver Statistics\n\
 VCC   VCC  VCC    RX   RX    RX    RX  RX err   RX   RX   RX    RX   RX Raw\n\
 max   bad  out    No   No    Err   Err  Range   No   Null Full  Raw    No\n\
 PDU   rng of Bps  CX  Pkhdr  buf   len   In    Bufs  PDU  Queue NoRdy Bufs\n"

#define DRIVER_STATS_HDR_2 \
"%s Device Driver Statistics\n\
 TX               TX     TX       TX    TX     TX\n\
part         TX   Idle Closing   Free   CBR  BestEff\n\
 PDU         TBD  VBR    VCC    Queues  Bps   Queue\n"

	Proatm_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Proatm_stats *)
			((uintptr_t) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print the RX header
	 */
	printf(DRIVER_STATS_HDR_1, get_adapter_name(vi->avsp_intf));
	
	/*
	 * Print the RX driver info
	 */
	printf("%4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld  %4ld\n",
			stats->proatm_st_drv.drv_vc_maxpdu,
			stats->proatm_st_drv.drv_vc_badrng,
			stats->proatm_st_drv.drv_vc_outofbw,

			stats->proatm_st_drv.drv_rv_nocx,
			stats->proatm_st_drv.drv_rv_nopkthdr,
			stats->proatm_st_drv.drv_rv_invchain,
			stats->proatm_st_drv.drv_rv_toobigpdu,
			stats->proatm_st_drv.drv_rv_novcc,
			stats->proatm_st_drv.drv_rv_nobufs,
			stats->proatm_st_drv.drv_rv_null,
			stats->proatm_st_drv.drv_rv_intrq,
			stats->proatm_st_drv.drv_rv_rnotrdy,
			stats->proatm_st_drv.drv_rv_rnobufs);

	/*
	 * Print the TX header
	 */
	printf(DRIVER_STATS_HDR_2, get_adapter_name(vi->avsp_intf));

	/*
	 * Print the TX driver info
	 */
	printf("%4ld  %10ld  %4ld  %6ld  %5ld  %4ld  %4ld\n",
			stats->proatm_st_drv.drv_xm_txicp,
			stats->proatm_st_drv.drv_xm_ntbd,
			stats->proatm_st_drv.drv_xm_idlevbr,
			stats->proatm_st_drv.drv_xm_closing,
			stats->proatm_st_drv.drv_xm_qufree,
			stats->proatm_st_drv.drv_xm_cbrbw,
			stats->proatm_st_drv.drv_xm_ubr0free);
}

/*
 * Print Proatm controler statistics
 * 
 * Arguments:
 *	vi	pointer to vendor-specific statistics to print
 *
 * Returns:
 *	none
 *
 */
static void
print_proatm_dev(struct air_vinfo_rsp *vi)
{
#if 0

#define DEVICE_STATS_HDR \
"%s Controler Statistics\n\
 Phys\n\
 Sig\n\
 PDU\n"
/*
          1         2         3         4         5         6         7
01234567890123456789012345678901234567890123456789012345678901234567890123456789
*/

	Proatm_stats	*stats;

	/*
	 * Bump stats pointer past header info
	 */
	stats = (Proatm_stats *)
			((uintptr_t) vi + sizeof(struct air_vinfo_rsp));

	/*
	 * Print a header
	 */
	printf(DEVICE_STATS_HDR, get_adapter_name(vi->avsp_intf));

	/*
	 * Print the driver info
	 */
	printf("%s\n",
			stats->proatm_st_drv.drv_dv_phys ? "on" : "off");
#else
	printf("The controler statistics are not supported yet\n");
#endif
}
