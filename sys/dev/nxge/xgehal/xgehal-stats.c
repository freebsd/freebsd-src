/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 *
 * $FreeBSD: src/sys/dev/nxge/xgehal/xgehal-stats.c,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <dev/nxge/include/xgehal-stats.h>
#include <dev/nxge/include/xgehal-device.h>

/*
 * __hal_stats_initialize
 * @stats: xge_hal_stats_t structure that contains, in particular,
 *         Xframe hw stat counters.
 * @devh: HAL device handle.
 *
 * Initialize per-device statistics object.
 * See also: xge_hal_stats_getinfo(), xge_hal_status_e{}.
 */
xge_hal_status_e
__hal_stats_initialize (xge_hal_stats_t *stats, xge_hal_device_h devh)
{
	int dma_flags;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

	xge_assert(!stats->is_initialized);

	dma_flags = XGE_OS_DMA_CACHELINE_ALIGNED;
#ifdef XGE_HAL_DMA_STATS_CONSISTENT
	dma_flags |= XGE_OS_DMA_CONSISTENT;
#else
	dma_flags |= XGE_OS_DMA_STREAMING;
#endif
	if (xge_hal_device_check_id(hldev) != XGE_HAL_CARD_TITAN) {
	    stats->hw_info =
	        (xge_hal_stats_hw_info_t *) xge_os_dma_malloc(
	                hldev->pdev,
	                sizeof(xge_hal_stats_hw_info_t),
	                dma_flags,
	                &stats->hw_info_dmah,
	                &stats->hw_info_dma_acch);

	    if (stats->hw_info == NULL) {
	        xge_debug_stats(XGE_ERR, "%s", "can not DMA alloc");
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }
	    xge_os_memzero(stats->hw_info,
	        sizeof(xge_hal_stats_hw_info_t));
	    xge_os_memzero(&stats->hw_info_saved,
	        sizeof(xge_hal_stats_hw_info_t));
	    xge_os_memzero(&stats->hw_info_latest,
	        sizeof(xge_hal_stats_hw_info_t));



	    stats->dma_addr = xge_os_dma_map(hldev->pdev,
	                               stats->hw_info_dmah,
	                   stats->hw_info,
	                   sizeof(xge_hal_stats_hw_info_t),
	                   XGE_OS_DMA_DIR_FROMDEVICE,
	                   XGE_OS_DMA_CACHELINE_ALIGNED |
#ifdef XGE_HAL_DMA_STATS_CONSISTENT
	                   XGE_OS_DMA_CONSISTENT
#else
	                       XGE_OS_DMA_STREAMING
#endif
	                                   );
	    if (stats->dma_addr == XGE_OS_INVALID_DMA_ADDR) {
	        xge_debug_stats(XGE_ERR,
	            "can not map vaddr 0x"XGE_OS_LLXFMT" to DMA",
	            (unsigned long long)(ulong_t)stats->hw_info);
	        xge_os_dma_free(hldev->pdev,
	              stats->hw_info,
	              sizeof(xge_hal_stats_hw_info_t),
	              &stats->hw_info_dma_acch,
	              &stats->hw_info_dmah);
	        return XGE_HAL_ERR_OUT_OF_MAPPING;
	    }
	}
	else {
	    stats->pcim_info_saved =
	        (xge_hal_stats_pcim_info_t *)xge_os_malloc(
	        hldev->pdev, sizeof(xge_hal_stats_pcim_info_t));
	    if (stats->pcim_info_saved == NULL) {
	        xge_debug_stats(XGE_ERR, "%s", "can not alloc");
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }

	    stats->pcim_info_latest =
	        (xge_hal_stats_pcim_info_t *)xge_os_malloc(
	        hldev->pdev, sizeof(xge_hal_stats_pcim_info_t));
	    if (stats->pcim_info_latest == NULL) {
	        xge_os_free(hldev->pdev, stats->pcim_info_saved,
	            sizeof(xge_hal_stats_pcim_info_t));
	        xge_debug_stats(XGE_ERR, "%s", "can not alloc");
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }

	    stats->pcim_info =
	        (xge_hal_stats_pcim_info_t *) xge_os_dma_malloc(
	                hldev->pdev,
	                sizeof(xge_hal_stats_pcim_info_t),
	                dma_flags,
	                &stats->hw_info_dmah,
	                &stats->hw_info_dma_acch);

	    if (stats->pcim_info == NULL) {
	        xge_os_free(hldev->pdev, stats->pcim_info_saved,
	            sizeof(xge_hal_stats_pcim_info_t));
	        xge_os_free(hldev->pdev, stats->pcim_info_latest,
	            sizeof(xge_hal_stats_pcim_info_t));
	        xge_debug_stats(XGE_ERR, "%s", "can not DMA alloc");
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }


	    xge_os_memzero(stats->pcim_info,
	        sizeof(xge_hal_stats_pcim_info_t));
	    xge_os_memzero(stats->pcim_info_saved,
	        sizeof(xge_hal_stats_pcim_info_t));
	    xge_os_memzero(stats->pcim_info_latest,
	        sizeof(xge_hal_stats_pcim_info_t));



	    stats->dma_addr = xge_os_dma_map(hldev->pdev,
	                               stats->hw_info_dmah,
	                   stats->pcim_info,
	                   sizeof(xge_hal_stats_pcim_info_t),
	                   XGE_OS_DMA_DIR_FROMDEVICE,
	                   XGE_OS_DMA_CACHELINE_ALIGNED |
#ifdef XGE_HAL_DMA_STATS_CONSISTENT
	                   XGE_OS_DMA_CONSISTENT
#else
	                       XGE_OS_DMA_STREAMING
#endif
	                                   );
	    if (stats->dma_addr == XGE_OS_INVALID_DMA_ADDR) {
	        xge_debug_stats(XGE_ERR,
	            "can not map vaddr 0x"XGE_OS_LLXFMT" to DMA",
	            (unsigned long long)(ulong_t)stats->hw_info);

	        xge_os_dma_free(hldev->pdev,
	              stats->pcim_info,
	              sizeof(xge_hal_stats_pcim_info_t),
	              &stats->hw_info_dma_acch,
	              &stats->hw_info_dmah);

	        xge_os_free(hldev->pdev, stats->pcim_info_saved,
	            sizeof(xge_hal_stats_pcim_info_t));

	        xge_os_free(hldev->pdev, stats->pcim_info_latest,
	            sizeof(xge_hal_stats_pcim_info_t));

	        return XGE_HAL_ERR_OUT_OF_MAPPING;
	    }
	}
	stats->devh = devh;
	xge_os_memzero(&stats->sw_dev_info_stats,
	         sizeof(xge_hal_stats_device_info_t));

	stats->is_initialized = 1;

	return XGE_HAL_OK;
}

static void
__hal_stats_save (xge_hal_stats_t *stats)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)stats->devh;

	if (xge_hal_device_check_id(hldev) != XGE_HAL_CARD_TITAN) {
	    xge_hal_stats_hw_info_t *latest;

	    (void) xge_hal_stats_hw(stats->devh, &latest);

	    xge_os_memcpy(&stats->hw_info_saved, stats->hw_info,
	          sizeof(xge_hal_stats_hw_info_t));
	} else {
	    xge_hal_stats_pcim_info_t   *latest;

	    (void) xge_hal_stats_pcim(stats->devh, &latest);

	    xge_os_memcpy(stats->pcim_info_saved, stats->pcim_info,
	          sizeof(xge_hal_stats_pcim_info_t));
	}
}

/*
 * __hal_stats_disable
 * @stats: xge_hal_stats_t structure that contains, in particular,
 *         Xframe hw stat counters.
 *
 * Ask device to stop collecting stats.
 * See also: xge_hal_stats_getinfo().
 */
void
__hal_stats_disable (xge_hal_stats_t *stats)
{
	xge_hal_device_t *hldev;
	xge_hal_pci_bar0_t *bar0;
	u64 val64;

	xge_assert(stats->hw_info);

	hldev = (xge_hal_device_t*)stats->devh;
	xge_assert(hldev);
	bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	    &bar0->stat_cfg);
	val64 &= ~XGE_HAL_STAT_CFG_STAT_EN;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	    &bar0->stat_cfg);
	/* flush the write */
	(void)xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	    &bar0->stat_cfg);

	xge_debug_stats(XGE_TRACE, "stats disabled at 0x"XGE_OS_LLXFMT,
	     (unsigned long long)stats->dma_addr);

	stats->is_enabled = 0;
}

/*
 * __hal_stats_terminate
 * @stats: xge_hal_stats_t structure that contains, in particular,
 *         Xframe hw stat counters.
 * Terminate per-device statistics object.
 */
void
__hal_stats_terminate (xge_hal_stats_t *stats)
{
	xge_hal_device_t *hldev;

	xge_assert(stats->hw_info);

	hldev = (xge_hal_device_t*)stats->devh;
	xge_assert(hldev);
	xge_assert(stats->is_initialized);
	if (xge_hal_device_check_id(hldev) != XGE_HAL_CARD_TITAN) {
	    xge_os_dma_unmap(hldev->pdev,
	               stats->hw_info_dmah,
	           stats->dma_addr,
	           sizeof(xge_hal_stats_hw_info_t),
	           XGE_OS_DMA_DIR_FROMDEVICE);

	    xge_os_dma_free(hldev->pdev,
	          stats->hw_info,
	          sizeof(xge_hal_stats_hw_info_t),
	          &stats->hw_info_dma_acch,
	          &stats->hw_info_dmah);
	} else {
	    xge_os_dma_unmap(hldev->pdev,
	               stats->hw_info_dmah,
	           stats->dma_addr,
	           sizeof(xge_hal_stats_pcim_info_t),
	           XGE_OS_DMA_DIR_FROMDEVICE);

	    xge_os_dma_free(hldev->pdev,
	          stats->pcim_info,
	          sizeof(xge_hal_stats_pcim_info_t),
	          &stats->hw_info_dma_acch,
	          &stats->hw_info_dmah);

	    xge_os_free(hldev->pdev, stats->pcim_info_saved,
	        sizeof(xge_hal_stats_pcim_info_t));

	    xge_os_free(hldev->pdev, stats->pcim_info_latest,
	            sizeof(xge_hal_stats_pcim_info_t));

	}

	stats->is_initialized = 0;
	stats->is_enabled = 0;
}



/*
 * __hal_stats_enable
 * @stats: xge_hal_stats_t structure that contains, in particular,
 *         Xframe hw stat counters.
 *
 * Ask device to start collecting stats.
 * See also: xge_hal_stats_getinfo().
 */
void
__hal_stats_enable (xge_hal_stats_t *stats)
{
	xge_hal_device_t *hldev;
	xge_hal_pci_bar0_t *bar0;
	u64 val64;
	unsigned int refresh_time_pci_clocks;

	xge_assert(stats->hw_info);

	hldev = (xge_hal_device_t*)stats->devh;
	xge_assert(hldev);

	bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	/* enable statistics
	 * For Titan stat_addr offset == 0x09d8, and stat_cfg offset == 0x09d0
	*/
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	    stats->dma_addr, &bar0->stat_addr);

	refresh_time_pci_clocks = XGE_HAL_XENA_PER_SEC *
	    hldev->config.stats_refresh_time_sec;
	refresh_time_pci_clocks =
	    __hal_fix_time_ival_herc(hldev,
	        refresh_time_pci_clocks);

#ifdef XGE_HAL_HERC_EMULATION
	/*
	 *  The clocks in the emulator are running ~1000 times slower
	 *  than real world, so the stats transfer will occur ~1000
	 *  times less frequent. STAT_CFG.STAT_TRSF_PERIOD should be
	 *  set to 0x20C for Hercules emulation (stats transferred
	 *  every 0.5 sec).
	*/

	val64 = (0x20C | XGE_HAL_STAT_CFG_STAT_RO |
	    XGE_HAL_STAT_CFG_STAT_EN);
#else
	val64 = XGE_HAL_SET_UPDT_PERIOD(refresh_time_pci_clocks) |
	                    XGE_HAL_STAT_CFG_STAT_RO |
	            XGE_HAL_STAT_CFG_STAT_EN;
#endif

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	    val64, &bar0->stat_cfg);

	xge_debug_stats(XGE_TRACE, "stats enabled at 0x"XGE_OS_LLXFMT,
	     (unsigned long long)stats->dma_addr);

	stats->is_enabled = 1;
}

/*
 * __hal_stats_pcim_update_latest - Update hw ER stats counters, based on the
 * real hardware maintained counters and the stored "reset" values.
 */
static void
__hal_stats_pcim_update_latest(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	int i;

#define set_latest_stat_link_cnt(_link, _p)                   \
	    hldev->stats.pcim_info_latest->link_info[_link]._p =              \
	((hldev->stats.pcim_info->link_info[_link]._p >=                  \
	    hldev->stats.pcim_info_saved->link_info[_link]._p) ?          \
	    hldev->stats.pcim_info->link_info[_link]._p -             \
	        hldev->stats.pcim_info_saved->link_info[_link]._p :   \
	    ((-1) - hldev->stats.pcim_info_saved->link_info[_link]._p) +  \
	        hldev->stats.pcim_info->link_info[_link]._p)


#define set_latest_stat_aggr_cnt(_aggr, _p)                   \
	    hldev->stats.pcim_info_latest->aggr_info[_aggr]._p =              \
	((hldev->stats.pcim_info->aggr_info[_aggr]._p >=              \
	    hldev->stats.pcim_info_saved->aggr_info[_aggr]._p) ?          \
	    hldev->stats.pcim_info->aggr_info[_aggr]._p -             \
	        hldev->stats.pcim_info_saved->aggr_info[_aggr]._p :   \
	    ((-1) - hldev->stats.pcim_info_saved->aggr_info[_aggr]._p) +  \
	        hldev->stats.pcim_info->aggr_info[_aggr]._p)


	for (i = 0; i < XGE_HAL_MAC_LINKS; i++) {
	    set_latest_stat_link_cnt(i, tx_frms);
	    set_latest_stat_link_cnt(i, tx_ttl_eth_octets);
	    set_latest_stat_link_cnt(i, tx_data_octets);
	    set_latest_stat_link_cnt(i, tx_mcst_frms);
	    set_latest_stat_link_cnt(i, tx_bcst_frms);
	    set_latest_stat_link_cnt(i, tx_ucst_frms);
	    set_latest_stat_link_cnt(i, tx_tagged_frms);
	    set_latest_stat_link_cnt(i, tx_vld_ip);
	    set_latest_stat_link_cnt(i, tx_vld_ip_octets);
	    set_latest_stat_link_cnt(i, tx_icmp);
	    set_latest_stat_link_cnt(i, tx_tcp);
	    set_latest_stat_link_cnt(i, tx_rst_tcp);
	    set_latest_stat_link_cnt(i, tx_udp);
	    set_latest_stat_link_cnt(i, tx_unknown_protocol);
	    set_latest_stat_link_cnt(i, tx_parse_error);
	    set_latest_stat_link_cnt(i, tx_pause_ctrl_frms);
	    set_latest_stat_link_cnt(i, tx_lacpdu_frms);
	    set_latest_stat_link_cnt(i, tx_marker_pdu_frms);
	    set_latest_stat_link_cnt(i, tx_marker_resp_pdu_frms);
	    set_latest_stat_link_cnt(i, tx_drop_ip);
	    set_latest_stat_link_cnt(i, tx_xgmii_char1_match);
	    set_latest_stat_link_cnt(i, tx_xgmii_char2_match);
	    set_latest_stat_link_cnt(i, tx_xgmii_column1_match);
	    set_latest_stat_link_cnt(i, tx_xgmii_column2_match);
	    set_latest_stat_link_cnt(i, tx_drop_frms);
	    set_latest_stat_link_cnt(i, tx_any_err_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_frms);
	    set_latest_stat_link_cnt(i, rx_vld_frms);
	    set_latest_stat_link_cnt(i, rx_offld_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_eth_octets);
	    set_latest_stat_link_cnt(i, rx_data_octets);
	    set_latest_stat_link_cnt(i, rx_offld_octets);
	    set_latest_stat_link_cnt(i, rx_vld_mcst_frms);
	    set_latest_stat_link_cnt(i, rx_vld_bcst_frms);
	    set_latest_stat_link_cnt(i, rx_accepted_ucst_frms);
	    set_latest_stat_link_cnt(i, rx_accepted_nucst_frms);
	    set_latest_stat_link_cnt(i, rx_tagged_frms);
	    set_latest_stat_link_cnt(i, rx_long_frms);
	    set_latest_stat_link_cnt(i, rx_usized_frms);
	    set_latest_stat_link_cnt(i, rx_osized_frms);
	    set_latest_stat_link_cnt(i, rx_frag_frms);
	    set_latest_stat_link_cnt(i, rx_jabber_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_64_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_65_127_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_128_255_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_256_511_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_512_1023_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_1024_1518_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_1519_4095_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_40956_8191_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_8192_max_frms);
	    set_latest_stat_link_cnt(i, rx_ttl_gt_max_frms);
	    set_latest_stat_link_cnt(i, rx_ip);
	    set_latest_stat_link_cnt(i, rx_ip_octets);
	    set_latest_stat_link_cnt(i, rx_hdr_err_ip);
	    set_latest_stat_link_cnt(i, rx_icmp);
	    set_latest_stat_link_cnt(i, rx_tcp);
	    set_latest_stat_link_cnt(i, rx_udp);
	    set_latest_stat_link_cnt(i, rx_err_tcp);
	    set_latest_stat_link_cnt(i, rx_pause_cnt);
	    set_latest_stat_link_cnt(i, rx_pause_ctrl_frms);
	    set_latest_stat_link_cnt(i, rx_unsup_ctrl_frms);
	    set_latest_stat_link_cnt(i, rx_in_rng_len_err_frms);
	    set_latest_stat_link_cnt(i, rx_out_rng_len_err_frms);
	    set_latest_stat_link_cnt(i, rx_drop_frms);
	    set_latest_stat_link_cnt(i, rx_discarded_frms);
	    set_latest_stat_link_cnt(i, rx_drop_ip);
	    set_latest_stat_link_cnt(i, rx_err_drp_udp);
	    set_latest_stat_link_cnt(i, rx_lacpdu_frms);
	    set_latest_stat_link_cnt(i, rx_marker_pdu_frms);
	    set_latest_stat_link_cnt(i, rx_marker_resp_pdu_frms);
	    set_latest_stat_link_cnt(i, rx_unknown_pdu_frms);
	    set_latest_stat_link_cnt(i, rx_illegal_pdu_frms);
	    set_latest_stat_link_cnt(i, rx_fcs_discard);
	    set_latest_stat_link_cnt(i, rx_len_discard);
	    set_latest_stat_link_cnt(i, rx_pf_discard);
	    set_latest_stat_link_cnt(i, rx_trash_discard);
	    set_latest_stat_link_cnt(i, rx_rts_discard);
	    set_latest_stat_link_cnt(i, rx_wol_discard);
	    set_latest_stat_link_cnt(i, rx_red_discard);
	    set_latest_stat_link_cnt(i, rx_ingm_full_discard);
	    set_latest_stat_link_cnt(i, rx_xgmii_data_err_cnt);
	    set_latest_stat_link_cnt(i, rx_xgmii_ctrl_err_cnt);
	    set_latest_stat_link_cnt(i, rx_xgmii_err_sym);
	    set_latest_stat_link_cnt(i, rx_xgmii_char1_match);
	    set_latest_stat_link_cnt(i, rx_xgmii_char2_match);
	    set_latest_stat_link_cnt(i, rx_xgmii_column1_match);
	    set_latest_stat_link_cnt(i, rx_xgmii_column2_match);
	    set_latest_stat_link_cnt(i, rx_local_fault);
	    set_latest_stat_link_cnt(i, rx_remote_fault);
	    set_latest_stat_link_cnt(i, rx_queue_full);
	}

	for (i = 0; i < XGE_HAL_MAC_AGGREGATORS; i++) {
	    set_latest_stat_aggr_cnt(i, tx_frms);
	    set_latest_stat_aggr_cnt(i, tx_mcst_frms);
	    set_latest_stat_aggr_cnt(i, tx_bcst_frms);
	    set_latest_stat_aggr_cnt(i, tx_discarded_frms);
	    set_latest_stat_aggr_cnt(i, tx_errored_frms);
	    set_latest_stat_aggr_cnt(i, rx_frms);
	    set_latest_stat_aggr_cnt(i, rx_data_octets);
	    set_latest_stat_aggr_cnt(i, rx_mcst_frms);
	    set_latest_stat_aggr_cnt(i, rx_bcst_frms);
	    set_latest_stat_aggr_cnt(i, rx_discarded_frms);
	    set_latest_stat_aggr_cnt(i, rx_errored_frms);
	    set_latest_stat_aggr_cnt(i, rx_unknown_protocol_frms);
	}
	return;
}

/*
 * __hal_stats_update_latest - Update hw stats counters, based on the real
 * hardware maintained counters and the stored "reset" values.
 */
static void
__hal_stats_update_latest(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

#define set_latest_stat_cnt(_dev, _p)                                   \
	    hldev->stats.hw_info_latest._p =                                \
	((hldev->stats.hw_info->_p >= hldev->stats.hw_info_saved._p) ?  \
	      hldev->stats.hw_info->_p - hldev->stats.hw_info_saved._p :    \
	  ((-1) - hldev->stats.hw_info_saved._p) + hldev->stats.hw_info->_p)

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_TITAN) {
	    __hal_stats_pcim_update_latest(devh);
	    return;
	}

	/* Tx MAC statistics counters. */
	set_latest_stat_cnt(hldev, tmac_frms);
	set_latest_stat_cnt(hldev, tmac_data_octets);
	set_latest_stat_cnt(hldev, tmac_drop_frms);
	set_latest_stat_cnt(hldev, tmac_mcst_frms);
	set_latest_stat_cnt(hldev, tmac_bcst_frms);
	set_latest_stat_cnt(hldev, tmac_pause_ctrl_frms);
	set_latest_stat_cnt(hldev, tmac_ttl_octets);
	set_latest_stat_cnt(hldev, tmac_ucst_frms);
	set_latest_stat_cnt(hldev, tmac_nucst_frms);
	set_latest_stat_cnt(hldev, tmac_any_err_frms);
	set_latest_stat_cnt(hldev, tmac_ttl_less_fb_octets);
	set_latest_stat_cnt(hldev, tmac_vld_ip_octets);
	set_latest_stat_cnt(hldev, tmac_vld_ip);
	set_latest_stat_cnt(hldev, tmac_drop_ip);
	set_latest_stat_cnt(hldev, tmac_icmp);
	set_latest_stat_cnt(hldev, tmac_rst_tcp);
	set_latest_stat_cnt(hldev, tmac_tcp);
	set_latest_stat_cnt(hldev, tmac_udp);
	set_latest_stat_cnt(hldev, reserved_0);

	/* Rx MAC Statistics counters. */
	set_latest_stat_cnt(hldev, rmac_vld_frms);
	set_latest_stat_cnt(hldev, rmac_data_octets);
	set_latest_stat_cnt(hldev, rmac_fcs_err_frms);
	set_latest_stat_cnt(hldev, rmac_drop_frms);
	set_latest_stat_cnt(hldev, rmac_vld_mcst_frms);
	set_latest_stat_cnt(hldev, rmac_vld_bcst_frms);
	set_latest_stat_cnt(hldev, rmac_in_rng_len_err_frms);
	set_latest_stat_cnt(hldev, rmac_out_rng_len_err_frms);
	set_latest_stat_cnt(hldev, rmac_long_frms);
	set_latest_stat_cnt(hldev, rmac_pause_ctrl_frms);
	set_latest_stat_cnt(hldev, rmac_unsup_ctrl_frms);
	set_latest_stat_cnt(hldev, rmac_ttl_octets);
	set_latest_stat_cnt(hldev, rmac_accepted_ucst_frms);
	set_latest_stat_cnt(hldev, rmac_accepted_nucst_frms);
	set_latest_stat_cnt(hldev, rmac_discarded_frms);
	set_latest_stat_cnt(hldev, rmac_drop_events);
	set_latest_stat_cnt(hldev, reserved_1);
	set_latest_stat_cnt(hldev, rmac_ttl_less_fb_octets);
	set_latest_stat_cnt(hldev, rmac_ttl_frms);
	set_latest_stat_cnt(hldev, reserved_2);
	set_latest_stat_cnt(hldev, reserved_3);
	set_latest_stat_cnt(hldev, rmac_usized_frms);
	set_latest_stat_cnt(hldev, rmac_osized_frms);
	set_latest_stat_cnt(hldev, rmac_frag_frms);
	set_latest_stat_cnt(hldev, rmac_jabber_frms);
	set_latest_stat_cnt(hldev, reserved_4);
	set_latest_stat_cnt(hldev, rmac_ttl_64_frms);
	set_latest_stat_cnt(hldev, rmac_ttl_65_127_frms);
	set_latest_stat_cnt(hldev, reserved_5);
	set_latest_stat_cnt(hldev, rmac_ttl_128_255_frms);
	set_latest_stat_cnt(hldev, rmac_ttl_256_511_frms);
	set_latest_stat_cnt(hldev, reserved_6);
	set_latest_stat_cnt(hldev, rmac_ttl_512_1023_frms);
	set_latest_stat_cnt(hldev, rmac_ttl_1024_1518_frms);
	set_latest_stat_cnt(hldev, reserved_7);
	set_latest_stat_cnt(hldev, rmac_ip);
	set_latest_stat_cnt(hldev, rmac_ip_octets);
	set_latest_stat_cnt(hldev, rmac_hdr_err_ip);
	set_latest_stat_cnt(hldev, rmac_drop_ip);
	set_latest_stat_cnt(hldev, rmac_icmp);
	set_latest_stat_cnt(hldev, reserved_8);
	set_latest_stat_cnt(hldev, rmac_tcp);
	set_latest_stat_cnt(hldev, rmac_udp);
	set_latest_stat_cnt(hldev, rmac_err_drp_udp);
	set_latest_stat_cnt(hldev, rmac_xgmii_err_sym);
	set_latest_stat_cnt(hldev, rmac_frms_q0);
	set_latest_stat_cnt(hldev, rmac_frms_q1);
	set_latest_stat_cnt(hldev, rmac_frms_q2);
	set_latest_stat_cnt(hldev, rmac_frms_q3);
	set_latest_stat_cnt(hldev, rmac_frms_q4);
	set_latest_stat_cnt(hldev, rmac_frms_q5);
	set_latest_stat_cnt(hldev, rmac_frms_q6);
	set_latest_stat_cnt(hldev, rmac_frms_q7);
	set_latest_stat_cnt(hldev, rmac_full_q0);
	set_latest_stat_cnt(hldev, rmac_full_q1);
	set_latest_stat_cnt(hldev, rmac_full_q2);
	set_latest_stat_cnt(hldev, rmac_full_q3);
	set_latest_stat_cnt(hldev, rmac_full_q4);
	set_latest_stat_cnt(hldev, rmac_full_q5);
	set_latest_stat_cnt(hldev, rmac_full_q6);
	set_latest_stat_cnt(hldev, rmac_full_q7);
	set_latest_stat_cnt(hldev, rmac_pause_cnt);
	set_latest_stat_cnt(hldev, reserved_9);
	set_latest_stat_cnt(hldev, rmac_xgmii_data_err_cnt);
	set_latest_stat_cnt(hldev, rmac_xgmii_ctrl_err_cnt);
	set_latest_stat_cnt(hldev, rmac_accepted_ip);
	set_latest_stat_cnt(hldev, rmac_err_tcp);

	/* PCI/PCI-X Read transaction statistics. */
	set_latest_stat_cnt(hldev, rd_req_cnt);
	set_latest_stat_cnt(hldev, new_rd_req_cnt);
	set_latest_stat_cnt(hldev, new_rd_req_rtry_cnt);
	set_latest_stat_cnt(hldev, rd_rtry_cnt);
	set_latest_stat_cnt(hldev, wr_rtry_rd_ack_cnt);

	/* PCI/PCI-X write transaction statistics. */
	set_latest_stat_cnt(hldev, wr_req_cnt);
	set_latest_stat_cnt(hldev, new_wr_req_cnt);
	set_latest_stat_cnt(hldev, new_wr_req_rtry_cnt);
	set_latest_stat_cnt(hldev, wr_rtry_cnt);
	set_latest_stat_cnt(hldev, wr_disc_cnt);
	set_latest_stat_cnt(hldev, rd_rtry_wr_ack_cnt);

	/* DMA Transaction statistics. */
	set_latest_stat_cnt(hldev, txp_wr_cnt);
	set_latest_stat_cnt(hldev, txd_rd_cnt);
	set_latest_stat_cnt(hldev, txd_wr_cnt);
	set_latest_stat_cnt(hldev, rxd_rd_cnt);
	set_latest_stat_cnt(hldev, rxd_wr_cnt);
	set_latest_stat_cnt(hldev, txf_rd_cnt);
	set_latest_stat_cnt(hldev, rxf_wr_cnt);

	/* Enhanced Herc statistics */
	set_latest_stat_cnt(hldev, tmac_frms_oflow);
	set_latest_stat_cnt(hldev, tmac_data_octets_oflow);
	set_latest_stat_cnt(hldev, tmac_mcst_frms_oflow);
	set_latest_stat_cnt(hldev, tmac_bcst_frms_oflow);
	set_latest_stat_cnt(hldev, tmac_ttl_octets_oflow);
	set_latest_stat_cnt(hldev, tmac_ucst_frms_oflow);
	set_latest_stat_cnt(hldev, tmac_nucst_frms_oflow);
	set_latest_stat_cnt(hldev, tmac_any_err_frms_oflow);
	set_latest_stat_cnt(hldev, tmac_vlan_frms);
	set_latest_stat_cnt(hldev, tmac_vld_ip_oflow);
	set_latest_stat_cnt(hldev, tmac_drop_ip_oflow);
	set_latest_stat_cnt(hldev, tmac_icmp_oflow);
	set_latest_stat_cnt(hldev, tmac_rst_tcp_oflow);
	set_latest_stat_cnt(hldev, tmac_udp_oflow);
	set_latest_stat_cnt(hldev, tpa_unknown_protocol);
	set_latest_stat_cnt(hldev, tpa_parse_failure);
	set_latest_stat_cnt(hldev, rmac_vld_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_data_octets_oflow);
	set_latest_stat_cnt(hldev, rmac_vld_mcst_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_vld_bcst_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_ttl_octets_oflow);
	set_latest_stat_cnt(hldev, rmac_accepted_ucst_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_accepted_nucst_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_discarded_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_drop_events_oflow);
	set_latest_stat_cnt(hldev, rmac_usized_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_osized_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_frag_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_jabber_frms_oflow);
	set_latest_stat_cnt(hldev, rmac_ip_oflow);
	set_latest_stat_cnt(hldev, rmac_drop_ip_oflow);
	set_latest_stat_cnt(hldev, rmac_icmp_oflow);
	set_latest_stat_cnt(hldev, rmac_udp_oflow);
	set_latest_stat_cnt(hldev, rmac_err_drp_udp_oflow);
	set_latest_stat_cnt(hldev, rmac_pause_cnt_oflow);
	set_latest_stat_cnt(hldev, rmac_ttl_1519_4095_frms);
	set_latest_stat_cnt(hldev, rmac_ttl_4096_8191_frms);
	set_latest_stat_cnt(hldev, rmac_ttl_8192_max_frms);
	set_latest_stat_cnt(hldev, rmac_ttl_gt_max_frms);
	set_latest_stat_cnt(hldev, rmac_osized_alt_frms);
	set_latest_stat_cnt(hldev, rmac_jabber_alt_frms);
	set_latest_stat_cnt(hldev, rmac_gt_max_alt_frms);
	set_latest_stat_cnt(hldev, rmac_vlan_frms);
	set_latest_stat_cnt(hldev, rmac_fcs_discard);
	set_latest_stat_cnt(hldev, rmac_len_discard);
	set_latest_stat_cnt(hldev, rmac_da_discard);
	set_latest_stat_cnt(hldev, rmac_pf_discard);
	set_latest_stat_cnt(hldev, rmac_rts_discard);
	set_latest_stat_cnt(hldev, rmac_red_discard);
	set_latest_stat_cnt(hldev, rmac_ingm_full_discard);
	set_latest_stat_cnt(hldev, rmac_accepted_ip_oflow);
	set_latest_stat_cnt(hldev, link_fault_cnt);
}

/**
 * xge_hal_stats_hw - Get HW device statistics.
 * @devh: HAL device handle.
 * @hw_info: Xframe statistic counters. See xge_hal_stats_hw_info_t.
 *           Returned by HAL.
 *
 * Get device and HAL statistics. The latter is part of the in-host statistics
 * that HAL maintains for _that_ device.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_STATS_IS_NOT_READY - Statistics information is not
 * currently available.
 *
 * See also: xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_stats_hw(xge_hal_device_h devh, xge_hal_stats_hw_info_t **hw_info)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	xge_assert(xge_hal_device_check_id(hldev) != XGE_HAL_CARD_TITAN)

	if (!hldev->stats.is_initialized ||
	    !hldev->stats.is_enabled) {
	    *hw_info = NULL;
	    return XGE_HAL_INF_STATS_IS_NOT_READY;
	}

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_STATS_STREAMING)
	xge_os_dma_sync(hldev->pdev,
	              hldev->stats.hw_info_dmah,
	          hldev->stats.dma_addr,
	          0,
	          sizeof(xge_hal_stats_hw_info_t),
	          XGE_OS_DMA_DIR_FROMDEVICE);
#endif

	    /*
	 * update hw counters, taking into account
	 * the "reset" or "saved"
	 * values
	 */
	__hal_stats_update_latest(devh);

	/*
	 * statistics HW bug fixups for Xena and Herc
	 */
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA ||
	    xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    u64 mcst, bcst;
	    xge_hal_stats_hw_info_t *hwsta = &hldev->stats.hw_info_latest;

	    mcst = ((u64)hwsta->rmac_vld_mcst_frms_oflow << 32) |
	        hwsta->rmac_vld_mcst_frms;

	    bcst = ((u64)hwsta->rmac_vld_bcst_frms_oflow << 32) |
	        hwsta->rmac_vld_bcst_frms;

	    mcst -= bcst;

	    hwsta->rmac_vld_mcst_frms_oflow = (u32)(mcst >> 32);
	    hwsta->rmac_vld_mcst_frms = (u32)mcst;
	}

	*hw_info = &hldev->stats.hw_info_latest;

	return XGE_HAL_OK;
}

/**
 * xge_hal_stats_pcim - Get HW device statistics.
 * @devh: HAL device handle.
 * @hw_info: Xframe statistic counters. See xge_hal_stats_pcim_info_t.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_STATS_IS_NOT_READY - Statistics information is not
 * currently available.
 *
 * See also: xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_stats_pcim(xge_hal_device_h devh, xge_hal_stats_pcim_info_t **hw_info)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	xge_assert(xge_hal_device_check_id(hldev) == XGE_HAL_CARD_TITAN)

	if (!hldev->stats.is_initialized ||
	    !hldev->stats.is_enabled) {
	    *hw_info = NULL;
	    return XGE_HAL_INF_STATS_IS_NOT_READY;
	}

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_STATS_STREAMING)
	xge_os_dma_sync(hldev->pdev,
	              hldev->stats.hw_info_dmah,
	          hldev->stats.dma_addr,
	          0,
	          sizeof(xge_hal_stats_pcim_info_t),
	          XGE_OS_DMA_DIR_FROMDEVICE);
#endif

	    /*
	 * update hw counters, taking into account
	 * the "reset" or "saved"
	 * values
	 */
	__hal_stats_pcim_update_latest(devh);

	*hw_info = hldev->stats.pcim_info_latest;

	return XGE_HAL_OK;
}

/**
 * xge_hal_stats_device - Get HAL statistics.
 * @devh: HAL device handle.
 * @hw_info: Xframe statistic counters. See xge_hal_stats_hw_info_t.
 *           Returned by HAL.
 * @device_info: HAL statistics. See xge_hal_stats_device_info_t.
 *               Returned by HAL.
 *
 * Get device and HAL statistics. The latter is part of the in-host statistics
 * that HAL maintains for _that_ device.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_STATS_IS_NOT_READY - Statistics information is not
 * currently available.
 *
 * See also: xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_stats_device(xge_hal_device_h devh,
	    xge_hal_stats_device_info_t **device_info)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	if (!hldev->stats.is_initialized ||
	    !hldev->stats.is_enabled) {
	    *device_info = NULL;
	    return XGE_HAL_INF_STATS_IS_NOT_READY;
	}

	hldev->stats.sw_dev_info_stats.traffic_intr_cnt =
	    hldev->stats.sw_dev_info_stats.total_intr_cnt -
	        hldev->stats.sw_dev_info_stats.not_traffic_intr_cnt;

	*device_info = &hldev->stats.sw_dev_info_stats;

	return XGE_HAL_OK;
}

/**
 * xge_hal_stats_channel - Get channel statistics.
 * @channelh: Channel handle.
 * @channel_info: HAL channel statistic counters.
 *                See xge_hal_stats_channel_info_t{}. Returned by HAL.
 *
 * Retrieve statistics of a particular HAL channel. This includes, for instance,
 * number of completions per interrupt, number of traffic interrupts, etc.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_STATS_IS_NOT_READY - Statistics information is not
 * currently available.
 *
 * See also: xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_stats_channel(xge_hal_channel_h channelh,
	    xge_hal_stats_channel_info_t **channel_info)
{
	xge_hal_stats_hw_info_t *latest;
	xge_hal_channel_t *channel;
	xge_hal_device_t *hldev;

	channel = (xge_hal_channel_t *)channelh;
	if ((channel == NULL) || (channel->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}
	hldev = (xge_hal_device_t *)channel->devh;
	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (!hldev->stats.is_initialized ||
	    !hldev->stats.is_enabled ||
	    !channel->is_open) {
	    *channel_info = NULL;
	    return XGE_HAL_INF_STATS_IS_NOT_READY;
	}

	hldev->stats.sw_dev_info_stats.traffic_intr_cnt =
	    hldev->stats.sw_dev_info_stats.total_intr_cnt -
	        hldev->stats.sw_dev_info_stats.not_traffic_intr_cnt;

	if (hldev->stats.sw_dev_info_stats.traffic_intr_cnt) {
	    int rxcnt = hldev->stats.sw_dev_info_stats.rx_traffic_intr_cnt;
	    int txcnt = hldev->stats.sw_dev_info_stats.tx_traffic_intr_cnt;
	    if (channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) {
	        if (!txcnt)
	            txcnt = 1;
	        channel->stats.avg_compl_per_intr_cnt =
	            channel->stats.total_compl_cnt / txcnt;
	    } else if (channel->type == XGE_HAL_CHANNEL_TYPE_RING &&
	           !hldev->config.bimodal_interrupts) {
	        if (!rxcnt)
	            rxcnt = 1;
	        channel->stats.avg_compl_per_intr_cnt =
	            channel->stats.total_compl_cnt / rxcnt;
	    }
	    if (channel->stats.avg_compl_per_intr_cnt == 0) {
	        /* to not confuse user */
	        channel->stats.avg_compl_per_intr_cnt = 1;
	    }
	}

	(void) xge_hal_stats_hw(hldev, &latest);

	if (channel->stats.total_posts) {
	    channel->stats.avg_buffers_per_post =
	        channel->stats.total_buffers /
	            channel->stats.total_posts;
#ifdef XGE_OS_PLATFORM_64BIT
	        if (channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) {
	            channel->stats.avg_post_size =
	        (u32)(latest->tmac_ttl_less_fb_octets /
	            channel->stats.total_posts);
	        }
#endif
	}

#ifdef XGE_OS_PLATFORM_64BIT
	if (channel->stats.total_buffers &&
	    channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) {
	    channel->stats.avg_buffer_size =
	        (u32)(latest->tmac_ttl_less_fb_octets /
	            channel->stats.total_buffers);
	}
#endif

	*channel_info = &channel->stats;
	return XGE_HAL_OK;
}

/**
 * xge_hal_stats_reset - Reset (zero-out) device statistics
 * @devh: HAL device handle.
 *
 * Reset all device statistics.
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_STATS_IS_NOT_READY - Statistics information is not
 * currently available.
 *
 * See also: xge_hal_status_e{}, xge_hal_stats_channel_info_t{},
 * xge_hal_stats_sw_err_t{}, xge_hal_stats_device_info_t{}.
 */
xge_hal_status_e
xge_hal_stats_reset(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	if (!hldev->stats.is_initialized ||
	    !hldev->stats.is_enabled) {
	    return XGE_HAL_INF_STATS_IS_NOT_READY;
	}

	/* save hw stats to calculate the after-reset values */
	__hal_stats_save(&hldev->stats);

	/* zero-out driver-maintained stats, don't reset the saved */
	    __hal_stats_soft_reset(hldev, 0);

	return XGE_HAL_OK;
}

/*
 * __hal_stats_soft_reset - Reset software-maintained statistics.
 */
void
__hal_stats_soft_reset (xge_hal_device_h devh, int reset_all)
{
	xge_list_t *item;
	xge_hal_channel_t *channel;
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	    if (reset_all)  {
	    if (xge_hal_device_check_id(hldev) != XGE_HAL_CARD_TITAN) {
	        xge_os_memzero(&hldev->stats.hw_info_saved,
	                   sizeof(xge_hal_stats_hw_info_t));
	        xge_os_memzero(&hldev->stats.hw_info_latest,
	                   sizeof(xge_hal_stats_hw_info_t));
	    } else {
	        xge_os_memzero(&hldev->stats.pcim_info_saved,
	                   sizeof(xge_hal_stats_pcim_info_t));
	        xge_os_memzero(&hldev->stats.pcim_info_latest,
	                   sizeof(xge_hal_stats_pcim_info_t));
	    }
	    }

	/* Reset the "soft" error and informational statistics */
	xge_os_memzero(&hldev->stats.sw_dev_err_stats,
	             sizeof(xge_hal_stats_sw_err_t));
	xge_os_memzero(&hldev->stats.sw_dev_info_stats,
	             sizeof(xge_hal_stats_device_info_t));

	/* for each Rx channel */
	xge_list_for_each(item, &hldev->ring_channels) {
	    channel = xge_container_of(item, xge_hal_channel_t, item);
	    xge_os_memzero(&channel->stats,
	                 sizeof(xge_hal_stats_channel_info_t));
	}

	/* for each Tx channel */
	xge_list_for_each(item, &hldev->fifo_channels) {
	    channel = xge_container_of(item, xge_hal_channel_t, item);
	    xge_os_memzero(&channel->stats,
	                 sizeof(xge_hal_stats_channel_info_t));
	}
}

