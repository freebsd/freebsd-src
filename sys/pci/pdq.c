/*-
 * Copyright (c) 1995 Matt Thomas (matt@lkg.dec.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: pdq.c,v 1.8 1996/01/24 21:12:23 phk Exp $
 *
 * Revision 1.8  1995/03/14  01:52:52  thomas
 * Update for new FreeBSD PCI Interrupt interface
 *
 * Revision 1.7  1995/03/07  23:03:16  thomas
 * Fix SMT queue processing
 *
 * Revision 1.6  1995/03/06  18:03:47  thomas
 * restart trasmitter once link is available
 *
 * Revision 1.5  1995/03/06  17:07:56  thomas
 * Add copyright/disclaimer
 * Add error recovery code.
 * Add BPF SMT support
 *
 * Revision 1.3  1995/03/03  13:48:35  thomas
 * more fixes
 *
 *
 */

/*
 * DEC PDQ FDDI Controller O/S independent code
 *
 * Written by Matt Thomas <matt@lkg.dec.com>
 *
 * This module should work any PDQ based board.  Note that changes for
 * MIPS and Alpha architectures (or any other architecture which requires
 * a flushing of memory or write buffers and/or has incoherent caches)
 * have yet to be made.
 */

#include <pci/pdqreg.h>
#include <pci/pdq_os.h>

#define	PDQ_ROUNDUP(n, x)	(((n) + ((x) - 1)) & ~((x) - 1))
#define	PDQ_CMD_RX_ALIGNMENT	16

#if defined(PDQTEST) && !defined(PDQ_NOPRINTF)
#define	PDQ_PRINTF(x)	printf x
#else
#define	PDQ_PRINTF(x)
#endif

static void pdq_process_transmitted_data(pdq_t *pdq);
static void pdq_flush_transmitter(pdq_t *pdq);
static void pdq_print_fddi_chars(pdq_t *pdq, const pdq_response_status_chars_get_t *rsp);

static void pdq_init_csrs(pdq_csrs_t *csrs, void *csrs_va, size_t csr_size);
static void pdq_init_pci_csrs(pdq_pci_csrs_t *csrs, void *csrs_va, size_t csr_size);

static void pdq_flush_databuf_queue(pdq_databuf_queue_t *q);

static pdq_boolean_t pdq_do_port_control(const pdq_csrs_t * const csrs, pdq_uint32_t cmd);
static void pdq_read_mla(const pdq_csrs_t * const csrs, pdq_lanaddr_t *hwaddr);
static void pdq_read_fwrev(const pdq_csrs_t * const csrs, pdq_fwrev_t *fwrev);
static pdq_boolean_t pdq_read_error_log(pdq_t *pdq, pdq_response_error_log_get_t *log_entry);
static pdq_chip_rev_t pdq_read_chiprev(const pdq_csrs_t * const csrs);

static void pdq_queue_commands(pdq_t *pdq);
static void pdq_process_command_responses(pdq_t *pdq);
static void pdq_process_unsolicited_events(pdq_t *pdq);

static void pdq_process_received_data(pdq_t *pdq, pdq_rx_info_t *rx,
				      pdq_rxdesc_t *receives,
				      pdq_uint32_t completion_goal,
				      pdq_uint32_t ring_mask);

static const char * const pdq_halt_codes[] = {
    "Selftest Timeout", "Host Bus Parity Error", "Host Directed Fault",
    "Software Fault", "Hardware Fault", "PC Trace Path Test",
    "DMA Error", "Image CRC Error", "Adapter Processer Error"
};

static const char * const pdq_adapter_states[] = {
    "Reset", "Upgrade", "DMA Unavailable", "DMA Available",
    "Link Available", "Link Unavailable", "Halted", "Ring Member"
};

/*
 * The following are used in conjunction with
 * unsolicited events
 */
static const char * const pdq_entities[] = {
    "Station", "Link", "Phy Port"
};

static const char * const pdq_station_events[] = {
    "Trace Received"
};

static const char * const pdq_station_arguments[] = {
    "Reason"
};

static const char * const pdq_link_events[] = {
    "Transmit Underrun",
    "Transmit Failed",
    "Block Check Error (CRC)",
    "Frame Status Error",
    "PDU Length Error",
    NULL,
    NULL,
    "Receive Data Overrun",
    NULL,
    "No User Buffer",
    "Ring Initialization Initiated",
    "Ring Initialization Received",
    "Ring Beacon Initiated",
    "Duplicate Address Failure",
    "Duplicate Token Detected",
    "Ring Purger Error",
    "FCI Strip Error",
    "Trace Initiated",
    "Directed Beacon Received",
};

static const char * const pdq_link_arguments[] = {
    "Reason",
    "Data Link Header",
    "Source",
    "Upstream Neighbor"
};

static const char * const pdq_phy_events[] = {
    "LEM Error Monitor Reject",
    "Elasticy Buffer Error",
    "Link Confidence Test Reject"
};

static const char * const pdq_phy_arguments[] = {
    "Direction"
};

static const char * const * const pdq_event_arguments[] = {
    pdq_station_arguments,
    pdq_link_arguments,
    pdq_phy_arguments
};

static const char * const * const pdq_event_codes[] = {
    pdq_station_events,
    pdq_link_events,
    pdq_phy_events
};

static const char * const pdq_station_types[] = {
    "SAS", "DAC", "SAC", "NAC", "DAS"
};

static const char * const pdq_smt_versions[] = { "", "V6.2", "V7.2" };

static const char pdq_phy_types[] = "ABSM";

static const char * const pdq_pmd_types0[] = {
    "ANSI Multi-Mode", "ANSI Single-Mode Type 1", "ANSI Single-Mode Type 2",
    "ANSI Sonet"
};

static const char * const pdq_pmd_types100[] = {
    "Low Power", "Thin Wire", "Shielded Twisted Pair",
    "Unshielded Twisted Pair"
};

static const char * const * const pdq_pmd_types[] = {
    pdq_pmd_types0, pdq_pmd_types100
};

static const char * const pdq_descriptions[] = {
    "DEFPA PCI",
    "DEFEA EISA",
};

static void
pdq_print_fddi_chars(
    pdq_t *pdq,
    const pdq_response_status_chars_get_t *rsp)
{

    printf(PDQ_OS_PREFIX "DEC %s FDDI %s Controller\n",
	   PDQ_OS_PREFIX_ARGS, pdq_descriptions[pdq->pdq_type],
	   pdq_station_types[rsp->status_chars_get.station_type]);

    printf(PDQ_OS_PREFIX "FDDI address %6D, FW=%4.4s, HW=%c",
	   PDQ_OS_PREFIX_ARGS, pdq->pdq_hwaddr.lanaddr_bytes, ":",
	   pdq->pdq_fwrev.fwrev_bytes,
	   rsp->status_chars_get.module_rev.fwrev_bytes[0]);

    if (rsp->status_chars_get.smt_version_id < PDQ_ARRAY_SIZE(pdq_smt_versions)) {
	printf(", SMT %s\n", pdq_smt_versions[rsp->status_chars_get.smt_version_id]);
    }

    printf(PDQ_OS_PREFIX "FDDI Port%s = %c (PMD = %s)",
	   PDQ_OS_PREFIX_ARGS,
	   rsp->status_chars_get.station_type == PDQ_STATION_TYPE_DAS ? "[A]" : "",
	   pdq_phy_types[rsp->status_chars_get.phy_type[0]],
	   pdq_pmd_types[rsp->status_chars_get.pmd_type[0] / 100][rsp->status_chars_get.pmd_type[0] % 100]);

    if (rsp->status_chars_get.station_type == PDQ_STATION_TYPE_DAS)
	printf(", FDDI Port[B] = %c (PMD = %s)",
	       pdq_phy_types[rsp->status_chars_get.phy_type[1]],
	       pdq_pmd_types[rsp->status_chars_get.pmd_type[1] / 100][rsp->status_chars_get.pmd_type[1] % 100]);

    printf("\n");
}

static void
pdq_init_csrs(
    pdq_csrs_t *csrs,
    void *csr_va,
    size_t csrsize)
{
    volatile pdq_uint32_t *csr_base = (volatile pdq_uint32_t *) csr_va;

    csrs->csr_port_reset		= &csr_base[0 * csrsize];
    csrs->csr_host_data			= &csr_base[1 * csrsize];
    csrs->csr_port_control		= &csr_base[2 * csrsize];
    csrs->csr_port_data_a		= &csr_base[3 * csrsize];
    csrs->csr_port_data_b		= &csr_base[4 * csrsize];
    csrs->csr_port_status		= &csr_base[5 * csrsize];
    csrs->csr_host_int_type_0		= &csr_base[6 * csrsize];
    csrs->csr_host_int_enable		= &csr_base[7 * csrsize];
    csrs->csr_type_2_producer		= &csr_base[8 * csrsize];
    csrs->csr_cmd_response_producer	= &csr_base[10 * csrsize];
    csrs->csr_cmd_request_producer	= &csr_base[11 * csrsize];
    csrs->csr_host_smt_producer		= &csr_base[12 * csrsize];
    csrs->csr_unsolicited_producer	= &csr_base[13 * csrsize];
}

static void
pdq_init_pci_csrs(
    pdq_pci_csrs_t *csrs,
    void *csr_va,
    size_t csrsize)
{
    volatile pdq_uint32_t *csr_base = (volatile pdq_uint32_t *) csr_va;

    csrs->csr_pfi_mode_control	= &csr_base[16 * csrsize];
    csrs->csr_pfi_status	= &csr_base[17 * csrsize];
    csrs->csr_fifo_write	= &csr_base[18 * csrsize];
    csrs->csr_fifo_read		= &csr_base[19 * csrsize];
}

static void
pdq_flush_databuf_queue(
    pdq_databuf_queue_t *q)
{
    PDQ_OS_DATABUF_T *pdu;
    for (;;) {
	PDQ_OS_DATABUF_DEQUEUE(q, pdu);
	if (pdu == NULL)
	    return;
	PDQ_OS_DATABUF_FREE(pdu);
    }
}

static pdq_boolean_t
pdq_do_port_control(
    const pdq_csrs_t * const csrs,
    pdq_uint32_t cmd)
{
    int cnt = 0;
    *csrs->csr_host_int_type_0 = PDQ_HOST_INT_CSR_CMD_DONE;
    *csrs->csr_port_control = PDQ_PCTL_CMD_ERROR | cmd;
    while ((*csrs->csr_host_int_type_0 & PDQ_HOST_INT_CSR_CMD_DONE) == 0 && cnt < 33000000)
	cnt++;
    PDQ_PRINTF(("CSR cmd spun %d times\n", cnt));
    if (*csrs->csr_host_int_type_0 & PDQ_HOST_INT_CSR_CMD_DONE) {
	*csrs->csr_host_int_type_0 = PDQ_HOST_INT_CSR_CMD_DONE;
	return (*csrs->csr_port_control & PDQ_PCTL_CMD_ERROR) ? PDQ_FALSE : PDQ_TRUE;
    }
    /* adapter failure */
    PDQ_ASSERT(0);
    return PDQ_FALSE;
}

static void
pdq_read_mla(
    const pdq_csrs_t * const csrs,
    pdq_lanaddr_t *hwaddr)
{
    pdq_uint32_t data;

    *csrs->csr_port_data_a = 0;
    pdq_do_port_control(csrs, PDQ_PCTL_MLA_READ);
    data = *csrs->csr_host_data;

    hwaddr->lanaddr_bytes[0] = (data >> 0) & 0xFF;
    hwaddr->lanaddr_bytes[1] = (data >> 8) & 0xFF;
    hwaddr->lanaddr_bytes[2] = (data >> 16) & 0xFF;
    hwaddr->lanaddr_bytes[3] = (data >> 24) & 0xFF;

    *csrs->csr_port_data_a = 1;
    pdq_do_port_control(csrs, PDQ_PCTL_MLA_READ);
    data = *csrs->csr_host_data;

    hwaddr->lanaddr_bytes[4] = (data >> 0) & 0xFF;
    hwaddr->lanaddr_bytes[5] = (data >> 8) & 0xFF;
}

static void
pdq_read_fwrev(
    const pdq_csrs_t * const csrs,
    pdq_fwrev_t *fwrev)
{
    pdq_uint32_t data;

    pdq_do_port_control(csrs, PDQ_PCTL_FW_REV_READ);
    data = *csrs->csr_host_data;

    fwrev->fwrev_bytes[3] = (data >> 0) & 0xFF;
    fwrev->fwrev_bytes[2] = (data >> 8) & 0xFF;
    fwrev->fwrev_bytes[1] = (data >> 16) & 0xFF;
    fwrev->fwrev_bytes[0] = (data >> 24) & 0xFF;
}

static pdq_boolean_t
pdq_read_error_log(
    pdq_t *pdq,
    pdq_response_error_log_get_t *log_entry)
{
    const pdq_csrs_t * const csrs = &pdq->pdq_csrs;
    pdq_uint32_t *ptr = (pdq_uint32_t *) log_entry;

    pdq_do_port_control(csrs, PDQ_PCTL_ERROR_LOG_START);

    while (pdq_do_port_control(csrs, PDQ_PCTL_FW_REV_READ) == PDQ_TRUE) {
	*ptr++ = *csrs->csr_host_data;
	if ((pdq_uint8_t *) ptr - (pdq_uint8_t *) log_entry == sizeof(*log_entry))
	    break;
    }
    return (ptr == (pdq_uint32_t *) log_entry) ? PDQ_FALSE : PDQ_TRUE;
}

static pdq_chip_rev_t
pdq_read_chiprev(
    const pdq_csrs_t * const csrs)
{
    pdq_uint32_t data;

    *csrs->csr_port_data_a = PDQ_SUB_CMD_PDQ_REV_GET;
    pdq_do_port_control(csrs, PDQ_PCTL_SUB_CMD);
    data = *csrs->csr_host_data;

    return (pdq_chip_rev_t) data;
}

static const struct {
    size_t cmd_len;
    size_t rsp_len;
    const char *cmd_name;
} pdq_cmd_info[] = {
    { sizeof(pdq_cmd_generic_t),		/* 0 - PDQC_START */
      sizeof(pdq_response_generic_t),
      "Start"
    },
    { sizeof(pdq_cmd_filter_set_t),		/* 1 - PDQC_FILTER_SET */
      sizeof(pdq_response_generic_t),
      "Filter Set"
    },
    { sizeof(pdq_cmd_generic_t),		/* 2 - PDQC_FILTER_GET */
      sizeof(pdq_response_filter_get_t),
      "Filter Get"
    },
    { sizeof(pdq_cmd_chars_set_t),		/* 3 - PDQC_CHARS_SET */
      sizeof(pdq_response_generic_t),
      "Chars Set"
    },
    { sizeof(pdq_cmd_generic_t),		/* 4 - PDQC_STATUS_CHARS_GET */
      sizeof(pdq_response_status_chars_get_t),
      "Status Chars Get"
    },
#if 0
    { sizeof(pdq_cmd_generic_t),		/* 5 - PDQC_COUNTERS_GET */
      sizeof(pdq_response_counters_get_t),
      "Counters Get"
    },
    { sizeof(pdq_cmd_counters_set_t),		/* 6 - PDQC_COUNTERS_SET */
      sizeof(pdq_response_generic_t),
      "Counters Set"
    },
#else
    { 0, 0, "Counters Get" },
    { 0, 0, "Counters Set" },
#endif
    { sizeof(pdq_cmd_addr_filter_set_t),	/* 7 - PDQC_ADDR_FILTER_SET */
      sizeof(pdq_response_generic_t),
      "Addr Filter Set"
    },
    { sizeof(pdq_cmd_generic_t),		/* 8 - PDQC_ADDR_FILTER_GET */
      sizeof(pdq_response_addr_filter_get_t),
      "Addr Filter Get"
    },
#if 0
    { sizeof(pdq_cmd_generic_t),		/* 9 - PDQC_ERROR_LOG_CLEAR */
      sizeof(pdq_response_generic_t),
      "Error Log Clear"
    },
    { sizeof(pdq_cmd_generic_t),		/* 10 - PDQC_ERROR_LOG_SET */
      sizeof(pdq_response_generic_t),
      "Error Log Set"
    },
    { sizeof(pdq_cmd_generic_t),		/* 11 - PDQC_FDDI_MIB_GET */
      sizeof(pdq_response_generic_t),
      "FDDI MIB Get"
    },
    { sizeof(pdq_cmd_generic_t),		/* 12 - PDQC_DEC_EXT_MIB_GET */
      sizeof(pdq_response_generic_t),
      "DEC Ext MIB Get"
    },
    { sizeof(pdq_cmd_generic_t),		/* 13 - PDQC_DEC_SPECIFIC_GET */
      sizeof(pdq_response_generic_t),
      "DEC Specific Get"
    },
    { sizeof(pdq_cmd_generic_t),		/* 14 - PDQC_SNMP_SET */
      sizeof(pdq_response_generic_t),
      "SNMP Set"
    },
    { 0, 0, "N/A" },
    { sizeof(pdq_cmd_generic_t),		/* 16 - PDQC_SMT_MIB_GET */
      sizeof(pdq_response_generic_t),
      "SMT MIB Get"
    },
    { sizeof(pdq_cmd_generic_t),		/* 17 - PDQC_SMT_MIB_SET */
      sizeof(pdq_response_generic_t),
      "SMT MIB Set",
    },
#endif
};

static void
pdq_queue_commands(
    pdq_t *pdq)
{
    const pdq_csrs_t * const csrs = &pdq->pdq_csrs;
    pdq_command_info_t *ci = &pdq->pdq_command_info;
    pdq_descriptor_block_t *dbp = pdq->pdq_dbp;
    pdq_txdesc_t *txd;
    pdq_rxdesc_t *rxd;
    pdq_cmd_code_t op;
    pdq_uint32_t cmdlen, rsplen;
    pdq_uint8_t *bufptr;
    unsigned cmds;

    for (cmds = 0; ci->ci_pending_commands != 0; cmds++) {
	pdq_uint32_t mask;
	/*
	 * If there is no free space in the queues, stop queuing
	 * commands.
	 */
	if (ci->ci_request_free == 0 || ci->ci_response_free == 0)
	    break;
	/*
	 * If there are no commands nor responses pending, then reset
	 * the command/response buffer back to the start.
	 */
	if (ci->ci_request_free == ci->ci_request_max
	    && ci->ci_response_free == ci->ci_response_max) {
	    ci->ci_bufptr = ci->ci_bufstart;
	    ci->ci_buffree = sizeof(pdq->pdq_dbp->pdqdb_command_pool);
	}
	/*
	 * Determine which commands need to be queued.
	 */
	op = PDQC_SMT_MIB_SET;
	for (mask = 1 << ((int) op); (mask & ci->ci_pending_commands) == 0; mask >>= 1)
	    op = (pdq_cmd_code_t) ((int) op - 1);
	/*
	 * Obtain the sizes needed for the command and response.
	 * Round up to PDQ_CMD_RX_ALIGNMENT so the receive buffer is
	 * always properly aligned.
	 */
	cmdlen = PDQ_ROUNDUP(pdq_cmd_info[op].cmd_len, PDQ_CMD_RX_ALIGNMENT);
	rsplen = PDQ_ROUNDUP(pdq_cmd_info[op].rsp_len, PDQ_CMD_RX_ALIGNMENT);
	if (cmdlen < rsplen)
	    cmdlen = rsplen;
	/*
	 * If there's not enough space or if there isn't enough continguous
	 * space, then wait for some to be freed.
	 */
	if (cmdlen > ci->ci_buffree)
	    break;
	if (cmdlen > ci->ci_bufend - ci->ci_bufptr)
	    break;

	/*
	 * Allocate the space for the command and the repsonse.
	 */
	bufptr = ci->ci_bufptr;
	ci->ci_bufptr += cmdlen;
	ci->ci_buffree -= cmdlen;
	if (ci->ci_bufptr == ci->ci_bufend)
	    ci->ci_bufptr = ci->ci_bufstart;

	/*
	 * Obtain and fill in the descriptor for the command
	 */
	txd = &dbp->pdqdb_command_requests[ci->ci_request_producer];
	PDQ_ADVANCE(ci->ci_request_producer, 1, PDQ_RING_MASK(dbp->pdqdb_command_requests));
	ci->ci_request_free--;

	txd->txd_pa_lo = ci->ci_pa_bufstart + (bufptr - ci->ci_bufstart);
	txd->txd_eop = txd->txd_sop = 1;
	txd->txd_seg_len = cmdlen;
	txd->txd_pa_hi = 0;

	/*
	 * Obtain and fill in the descriptor for the response
	 */
	rxd = &dbp->pdqdb_command_responses[ci->ci_response_producer];
	PDQ_ADVANCE(ci->ci_response_producer, 1, PDQ_RING_MASK(dbp->pdqdb_command_responses));
	ci->ci_response_free--;

	rxd->rxd_pa_lo = ci->ci_pa_bufstart + (bufptr - ci->ci_bufstart);
	rxd->rxd_sop = 1;
	rxd->rxd_seg_cnt = 0;
	rxd->rxd_seg_len_lo = 0;
	rxd->rxd_seg_len_hi = cmdlen / 16;
	rxd->rxd_pa_hi = 0;

	/*
	 * Clear the command area, set the opcode, and the command from the pending
	 * mask.
	 */

	PDQ_OS_MEMZERO(bufptr, cmdlen);
	*(pdq_cmd_code_t *) bufptr = op;
	ci->ci_pending_commands &= ~mask;

	/*
	 * Fill in the command area, if needed.
	 */
	switch (op) {
	    case PDQC_FILTER_SET: {
		pdq_cmd_filter_set_t *filter_set = (pdq_cmd_filter_set_t *) bufptr;
		unsigned idx = 0;
		filter_set->filter_set_items[idx].item_code = PDQI_IND_GROUP_PROM;
		filter_set->filter_set_items[idx].filter_state = (pdq->pdq_flags & PDQ_PROMISC ? PDQ_FILTER_PASS : PDQ_FILTER_BLOCK);
		idx++;
		filter_set->filter_set_items[idx].item_code = PDQI_GROUP_PROM;
		filter_set->filter_set_items[idx].filter_state = (pdq->pdq_flags & PDQ_ALLMULTI ? PDQ_FILTER_PASS : PDQ_FILTER_BLOCK);
		idx++;
		filter_set->filter_set_items[idx].item_code = PDQI_SMT_PROM;
		filter_set->filter_set_items[idx].filter_state = ((pdq->pdq_flags & (PDQ_PROMISC|PDQ_PASS_SMT)) == (PDQ_PROMISC|PDQ_PASS_SMT) ? PDQ_FILTER_PASS : PDQ_FILTER_BLOCK);
		idx++;
		filter_set->filter_set_items[idx].item_code = PDQI_SMT_USER;
		filter_set->filter_set_items[idx].filter_state = (pdq->pdq_flags & PDQ_PASS_SMT ? PDQ_FILTER_PASS : PDQ_FILTER_BLOCK);
		idx++;
		filter_set->filter_set_items[idx].item_code = PDQI_EOL;
		break;
	    }
	    case PDQC_ADDR_FILTER_SET: {
		pdq_cmd_addr_filter_set_t *addr_filter_set = (pdq_cmd_addr_filter_set_t *) bufptr;
		pdq_lanaddr_t *addr = addr_filter_set->addr_filter_set_addresses;
		addr->lanaddr_bytes[0] = 0xFF;
		addr->lanaddr_bytes[1] = 0xFF;
		addr->lanaddr_bytes[2] = 0xFF;
		addr->lanaddr_bytes[3] = 0xFF;
		addr->lanaddr_bytes[4] = 0xFF;
		addr->lanaddr_bytes[5] = 0xFF;
		addr++;
		pdq_os_addr_fill(pdq, addr, 61);
		break;
	    }
	    default:
		break;
	}
	/*
	 * At this point the command is done.  All that needs to be done is to
	 * produce it to the PDQ.  That will be done after the loop exits.
	 */
	PDQ_PRINTF(("PDQ Queue Command Request: %s queued\n",
	       pdq_cmd_info[op].cmd_name));
    }
    if (cmds > 0) {
	*csrs->csr_cmd_response_producer = ci->ci_response_producer | (ci->ci_response_completion << 8);
	*csrs->csr_cmd_request_producer = ci->ci_request_producer | (ci->ci_request_completion << 8);
    }
}

static void
pdq_process_command_responses(
    pdq_t *pdq)
{
    const pdq_csrs_t * const csrs = &pdq->pdq_csrs;
    pdq_command_info_t *ci = &pdq->pdq_command_info;
    volatile const pdq_consumer_block_t *cbp = pdq->pdq_cbp;
    pdq_descriptor_block_t *dbp = pdq->pdq_dbp;
    pdq_txdesc_t *txd;
    pdq_rxdesc_t *rxd;
    const pdq_response_generic_t *rspgen;
    unsigned cmds;

    /*
     * We have to process the command and response in tandem so
     * just wait for the response to be consumed.  If it has been
     * consumed then the command must have been as well.
     */

    for (cmds = 0; cbp->pdqcb_command_response != ci->ci_response_completion; cmds = 1) {
	PDQ_ASSERT (cbp->pdqcb_command_request != ci->ci_request_completion);

	txd = &dbp->pdqdb_command_requests[ci->ci_request_completion];
	rxd = &dbp->pdqdb_command_responses[ci->ci_response_completion];

	rspgen = (const pdq_response_generic_t *) (ci->ci_bufstart + rxd->rxd_pa_lo - ci->ci_pa_bufstart);
	PDQ_ASSERT(rspgen->generic_status == PDQR_SUCCESS);
	PDQ_PRINTF(("PDQ Process Command Response: %s completed\n",
	       pdq_cmd_info[rspgen->generic_op].cmd_name));

	if (rspgen->generic_op == PDQC_STATUS_CHARS_GET && (pdq->pdq_flags & PDQ_PRINTCHARS)) {
	    pdq->pdq_flags &= ~PDQ_PRINTCHARS;
	    pdq_print_fddi_chars(pdq, (const pdq_response_status_chars_get_t *) rspgen);
	}

	PDQ_ADVANCE(ci->ci_request_completion, 1, PDQ_RING_MASK(dbp->pdqdb_command_requests));
	PDQ_ADVANCE(ci->ci_response_completion, 1, PDQ_RING_MASK(dbp->pdqdb_command_responses));
	ci->ci_response_free++;
	ci->ci_request_free++;
	ci->ci_buffree += txd->txd_seg_len;
    }

    if (cmds > 0) {
	*csrs->csr_cmd_response_producer = ci->ci_response_producer | (ci->ci_response_completion << 8);
	*csrs->csr_cmd_request_producer = ci->ci_request_producer | (ci->ci_request_completion << 8);
	if (ci->ci_pending_commands != 0)
	    pdq_queue_commands(pdq);
    }
}

/*
 * This following routine processes unsolicited events.
 * In addition, it also fills the unsolicited queue with
 * event buffers so it can be used to initialize the queue
 * as well.
 */
static void
pdq_process_unsolicited_events(
    pdq_t *pdq)
{
    const pdq_csrs_t * const csrs = &pdq->pdq_csrs;
    pdq_unsolicited_info_t *ui = &pdq->pdq_unsolicited_info;
    volatile const pdq_consumer_block_t *cbp = pdq->pdq_cbp;
    pdq_descriptor_block_t *dbp = pdq->pdq_dbp;
    const pdq_unsolicited_event_t *event;
    pdq_rxdesc_t *rxd;

    /*
     * Process each unsolicited event (if any).
     */

    while (cbp->pdqcb_unsolicited_event != ui->ui_completion) {
	rxd = &dbp->pdqdb_unsolicited_events[ui->ui_completion];
	event = &ui->ui_events[ui->ui_completion & (PDQ_NUM_UNSOLICITED_EVENTS-1)];

	switch (event->event_type) {
	    case PDQ_UNSOLICITED_EVENT: {
		printf(PDQ_OS_PREFIX "Unsolicited Event: %s: %s",
		       PDQ_OS_PREFIX_ARGS,
		       pdq_entities[event->event_entity],
		       pdq_event_codes[event->event_entity][event->event_code.value]);
		if (event->event_type == PDQ_ENTITY_PHY_PORT)
		    printf("[%d]", event->event_index);
		printf("\n");
		break;
	    }
	    case PDQ_UNSOLICITED_COUNTERS: {
		break;
	    }
	}
	PDQ_ADVANCE(ui->ui_completion, 1, PDQ_RING_MASK(dbp->pdqdb_unsolicited_events));
	ui->ui_free++;
    }

    /*
     * Now give back the event buffers back to the PDQ.
     */
    PDQ_ADVANCE(ui->ui_producer, ui->ui_free, PDQ_RING_MASK(dbp->pdqdb_unsolicited_events));
    ui->ui_free = 0;

    *csrs->csr_unsolicited_producer = ui->ui_producer | (ui->ui_completion << 8);
}

static void
pdq_process_received_data(
    pdq_t *pdq,
    pdq_rx_info_t *rx,
    pdq_rxdesc_t *receives,
    pdq_uint32_t completion_goal,
    pdq_uint32_t ring_mask)
{
    pdq_uint32_t completion = rx->rx_completion;
    pdq_uint32_t producer = rx->rx_producer;
    PDQ_OS_DATABUF_T **buffers = (PDQ_OS_DATABUF_T **) rx->rx_buffers;
    pdq_rxdesc_t *rxd;
    pdq_uint32_t idx;

    while (completion != completion_goal) {
	PDQ_OS_DATABUF_T *fpdu, *lpdu, *npdu;
	pdq_uint8_t *dataptr;
	pdq_uint32_t fc, datalen, pdulen, segcnt;
	pdq_rxstatus_t status;

	fpdu = lpdu = buffers[completion];
	PDQ_ASSERT(fpdu != NULL);

	dataptr = PDQ_OS_DATABUF_PTR(fpdu);
	status = *(pdq_rxstatus_t *) dataptr;
	if ((status.rxs_status & 0x200000) == 0) {
	    datalen = status.rxs_status & 0x1FFF;
	    fc = dataptr[PDQ_RX_FC_OFFSET];
	    switch (fc & (PDQ_FDDIFC_C|PDQ_FDDIFC_L|PDQ_FDDIFC_F)) {
		case PDQ_FDDI_LLC_ASYNC:
		case PDQ_FDDI_LLC_SYNC:
		case PDQ_FDDI_IMP_ASYNC:
		case PDQ_FDDI_IMP_SYNC: {
		    if (datalen > PDQ_FDDI_MAX || datalen < PDQ_FDDI_LLC_MIN) {
			printf("discard: bad length %d\n", datalen);
			goto discard_frame;
		    }
		    break;
		}
		case PDQ_FDDI_SMT: {
		    if (datalen > PDQ_FDDI_MAX || datalen < PDQ_FDDI_SMT_MIN)
			goto discard_frame;
		    break;
		}
		default: {
		    printf("discard: bad fc 0x%x\n", fc);
		    goto discard_frame;
		}
	    }
	    /*
	     * Update the lengths of the data buffers now that we know
	     * the real length.
	     */
	    pdulen = datalen - 4 /* CRC */ + 1 /* FC */;
	    segcnt = (pdulen + PDQ_RX_FC_OFFSET + PDQ_OS_DATABUF_SIZE - 1) / PDQ_OS_DATABUF_SIZE;
	    PDQ_OS_DATABUF_ALLOC(npdu);
	    if (npdu == NULL) {
		printf("discard: no databuf #0\n");
		goto discard_frame;
	    }
	    buffers[completion] = npdu;
	    for (idx = 1; idx < segcnt; idx++) {
		PDQ_OS_DATABUF_ALLOC(npdu);
		if (npdu == NULL) {
		    PDQ_OS_DATABUF_NEXT_SET(lpdu, NULL);
		    PDQ_OS_DATABUF_FREE(fpdu);
		    goto discard_frame;
		}
		PDQ_OS_DATABUF_NEXT_SET(lpdu, buffers[(completion + idx) & ring_mask]);
		lpdu = PDQ_OS_DATABUF_NEXT(lpdu);
		buffers[(completion + idx) & ring_mask] = npdu;
	    }
	    PDQ_OS_DATABUF_NEXT_SET(lpdu, NULL);
	    for (idx = 0; idx < PDQ_RX_SEGCNT; idx++) {
		buffers[(producer + idx) & ring_mask] =
		    buffers[(completion + idx) & ring_mask];
		buffers[(completion + idx) & ring_mask] = NULL;
	    }
	    PDQ_OS_DATABUF_PTR_ADJ(fpdu, PDQ_RX_FC_OFFSET);
	    if (segcnt == 1) {
		PDQ_OS_DATABUF_LEN_SET(fpdu, pdulen);
	    } else {
		PDQ_OS_DATABUF_LEN_SET(lpdu, pdulen + PDQ_RX_FC_OFFSET - (segcnt - 1) * PDQ_OS_DATABUF_SIZE);
		PDQ_OS_DATABUF_LEN_ADJ(fpdu, -PDQ_RX_FC_OFFSET);
	    }
	    pdq_os_receive_pdu(pdq, fpdu, pdulen);
	    rx->rx_free += PDQ_RX_SEGCNT;
	    PDQ_ADVANCE(producer, PDQ_RX_SEGCNT, ring_mask);
	    PDQ_ADVANCE(completion, PDQ_RX_SEGCNT, ring_mask);
	    continue;
	} else {
	    printf("discard: bad pdu 0x%x(%d.%d.%d.%d.%d)\n", status.rxs_status,
			status.rxs_rcc_badpdu, status.rxs_rcc_badcrc,
			status.rxs_rcc_reason, status.rxs_fsc, status.rxs_fsb_e);
	    if (status.rxs_rcc_reason == 7)
		goto discard_frame;
	    if (status.rxs_rcc_reason != 0)
		/* hardware fault */
	    if (status.rxs_rcc_badcrc) {
		/* rx->rx_badcrc++; */
	    } else if (status.rxs_fsc == 0 | status.rxs_fsb_e == 1) {
		/* rx->rx_frame_status_errors++; */
	    } else {
		/* hardware fault */
	    }
	}
      discard_frame:
	/*
	 * Discarded frames go right back on the queue; therefore
	 * ring entries were freed.
	 */
	for (idx = 0; idx < PDQ_RX_SEGCNT; idx++) {
	    buffers[producer] = buffers[completion];
	    buffers[completion] = NULL;
	    rxd = &receives[rx->rx_producer];
	    if (idx == 0) {
		rxd->rxd_sop = 1; rxd->rxd_seg_cnt = PDQ_RX_SEGCNT - 1;
	    } else {
		rxd->rxd_sop = 0; rxd->rxd_seg_cnt = 0;
	    }
	    rxd->rxd_pa_hi = 0;
	    rxd->rxd_seg_len_hi = PDQ_OS_DATABUF_SIZE / 16;
	    rxd->rxd_pa_lo = PDQ_OS_VA_TO_PA(PDQ_OS_DATABUF_PTR(buffers[rx->rx_producer]));
	    PDQ_ADVANCE(rx->rx_producer, 1, ring_mask);
	    PDQ_ADVANCE(producer, 1, ring_mask);
	    PDQ_ADVANCE(completion, 1, ring_mask);
	}
    }
    rx->rx_completion = completion;

    while (rx->rx_free > PDQ_RX_SEGCNT && rx->rx_free > rx->rx_target) {
	PDQ_OS_DATABUF_T *pdu;
	/*
	 * Allocate the needed number of data buffers.
	 * Try to obtain them from our free queue before
	 * asking the system for more.
	 */
	for (idx = 0; idx < PDQ_RX_SEGCNT; idx++) {
	    if ((pdu = buffers[(rx->rx_producer + idx) & ring_mask]) == NULL) {
		PDQ_OS_DATABUF_ALLOC(pdu);
		if (pdu == NULL)
		    break;
		buffers[(rx->rx_producer + idx) & ring_mask] = pdu;
	    }
	    rxd = &receives[(rx->rx_producer + idx) & ring_mask];
	    if (idx == 0) {
		rxd->rxd_sop = 1; rxd->rxd_seg_cnt = PDQ_RX_SEGCNT - 1;
	    } else {
		rxd->rxd_sop = 0; rxd->rxd_seg_cnt = 0;
	    }
	    rxd->rxd_pa_hi = 0;
	    rxd->rxd_seg_len_hi = PDQ_OS_DATABUF_SIZE / 16;
	    rxd->rxd_pa_lo = PDQ_OS_VA_TO_PA(PDQ_OS_DATABUF_PTR(pdu));
	}
	if (idx < PDQ_RX_SEGCNT) {
	    /*
	     * We didn't get all databufs required to complete a new
	     * receive buffer.  Keep the ones we got and retry a bit
	     * later for the rest.
	     */
	    break;
	}
	PDQ_ADVANCE(rx->rx_producer, PDQ_RX_SEGCNT, ring_mask);
	rx->rx_free -= PDQ_RX_SEGCNT;
    }
}

pdq_boolean_t
pdq_queue_transmit_data(
    pdq_t *pdq,
    PDQ_OS_DATABUF_T *pdu)
{
    pdq_tx_info_t *tx = &pdq->pdq_tx_info;
    pdq_descriptor_block_t *dbp = pdq->pdq_dbp;
    pdq_uint32_t producer = tx->tx_producer;
    pdq_txdesc_t *eop = NULL;
    PDQ_OS_DATABUF_T *pdu0;
    pdq_uint32_t freecnt;

    dbp->pdqdb_transmits[producer] = tx->tx_hdrdesc;
    PDQ_ADVANCE(producer, 1, PDQ_RING_MASK(dbp->pdqdb_transmits));

    for (freecnt = tx->tx_free - 1, pdu0 = pdu; pdu0 != NULL && freecnt > 0;) {
	pdq_uint32_t fraglen, datalen = PDQ_OS_DATABUF_LEN(pdu0);
	const pdq_uint8_t *dataptr = PDQ_OS_DATABUF_PTR(pdu0);

	/*
	 * The first segment is limited to the space remaining in
	 * page.  All segments after that can be up to a full page
	 * in size.
	 */
	fraglen = PDQ_OS_PAGESIZE - ((dataptr - (pdq_uint8_t *) NULL) & (PDQ_OS_PAGESIZE-1));
	while (datalen > 0 && freecnt > 0) {
	    pdq_uint32_t seglen = (fraglen < datalen ? fraglen : datalen);

	    /*
	     * Initialize the transmit descriptor
	     */
	    eop = &dbp->pdqdb_transmits[producer];
	    eop->txd_seg_len = seglen;
	    eop->txd_pa_lo = PDQ_OS_VA_TO_PA(dataptr);
	    eop->txd_sop = eop->txd_eop = eop->txd_pa_hi = 0;

	    datalen -= seglen;
	    dataptr += seglen;
	    fraglen = PDQ_OS_PAGESIZE;
	    freecnt--;
	    PDQ_ADVANCE(producer, 1, PDQ_RING_MASK(dbp->pdqdb_transmits));
	}
	pdu0 = PDQ_OS_DATABUF_NEXT(pdu0);
    }
    if (pdu0 != NULL) {
	PDQ_ASSERT(free == 0);
	/*
	 * If we still have data to process then the ring was too full
	 * to store the PDU.  Return FALSE so the caller will requeue
	 * the PDU for later.
	 */
	return PDQ_FALSE;
    }
    /*
     * Everything went fine.  Finish it up.
     */
    tx->tx_descriptor_count[tx->tx_producer] = tx->tx_free - freecnt;
    eop->txd_eop = 1;
    PDQ_OS_DATABUF_ENQUEUE(&tx->tx_txq, pdu);
    tx->tx_producer = producer;
    tx->tx_free = freecnt;
    PDQ_DO_TYPE2_PRODUCER(pdq);
    return PDQ_TRUE;
}

static void
pdq_process_transmitted_data(
    pdq_t *pdq)
{
    pdq_tx_info_t *tx = &pdq->pdq_tx_info;
    volatile const pdq_consumer_block_t *cbp = pdq->pdq_cbp;
    pdq_descriptor_block_t *dbp = pdq->pdq_dbp;
    pdq_uint32_t completion = tx->tx_completion;

    while (completion != cbp->pdqcb_transmits) {
	PDQ_OS_DATABUF_T *pdu;
	pdq_uint32_t descriptor_count = tx->tx_descriptor_count[completion];
	PDQ_ASSERT(dbp->pdqdb_transmits[tx->tx_completion].txd_sop == 1);
	PDQ_ASSERT(dbp->pdqdb_transmits[(completion + descriptor_count - 1) & PDQ_RING_MASK(dbp->pdqdb_transmits)].txd_eop == 1);
	PDQ_OS_DATABUF_DEQUEUE(&tx->tx_txq, pdu);
	pdq_os_transmit_done(pdq, pdu);
	tx->tx_free += descriptor_count;

	PDQ_ADVANCE(completion, descriptor_count, PDQ_RING_MASK(dbp->pdqdb_transmits));
    }
    if (tx->tx_completion != completion) {
	tx->tx_completion = completion;
	pdq_os_restart_transmitter(pdq);
    }
    PDQ_DO_TYPE2_PRODUCER(pdq);
}

static void
pdq_flush_transmitter(
    pdq_t *pdq)
{
    volatile pdq_consumer_block_t *cbp = pdq->pdq_cbp;
    pdq_tx_info_t *tx = &pdq->pdq_tx_info;

    for (;;) {
	PDQ_OS_DATABUF_T *pdu;
	PDQ_OS_DATABUF_DEQUEUE(&tx->tx_txq, pdu);
	if (pdu == NULL)
	    break;
	/*
	 * Don't call transmit done since the packet never made it
	 * out on the wire.
	 */
	PDQ_OS_DATABUF_FREE(pdu);
    }

    tx->tx_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_transmits);
    tx->tx_completion = cbp->pdqcb_transmits = tx->tx_producer;

    PDQ_DO_TYPE2_PRODUCER(pdq);
}

/*
 * The following routine brings the PDQ from whatever state it is
 * in to DMA_UNAVAILABLE (ie. like a RESET but without doing a RESET).
 */
pdq_state_t
pdq_stop(
    pdq_t *pdq)
{
    pdq_state_t state;
    const pdq_csrs_t * const csrs = &pdq->pdq_csrs;
    int cnt, pass = 0, idx;
    PDQ_OS_DATABUF_T **buffers;

  restart:
    state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
    if (state != PDQS_DMA_UNAVAILABLE) {
	*csrs->csr_port_data_a = (state == PDQS_HALTED) ? 0 : PDQ_PRESET_SKIP_SELFTEST;
	*csrs->csr_port_reset = 1;
	PDQ_OS_USEC_DELAY(100);
	*csrs->csr_port_reset = 0;
	for (cnt = 45000;;cnt--) {
	    PDQ_OS_USEC_DELAY(1000);
	    state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
	    if (state == PDQS_DMA_UNAVAILABLE || cnt == 0)
		break;
	}
	PDQ_PRINTF(("PDQ Reset spun %d cycles\n", 45000 - cnt));
	PDQ_OS_USEC_DELAY(10000);
	state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
	PDQ_ASSERT(state == PDQS_DMA_UNAVAILABLE);
	PDQ_ASSERT(cnt > 0);
    }
#if 0
    switch (state) {
	case PDQS_RING_MEMBER:
	case PDQS_LINK_UNAVAILABLE:
	case PDQS_LINK_AVAILABLE: {
	    *csrs->csr_port_data_a = PDQ_SUB_CMD_LINK_UNINIT;
	    *csrs->csr_port_data_b = 0;
	    pdq_do_port_control(csrs, PDQ_PCTL_SUB_CMD);
	    state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
	    PDQ_ASSERT(state == PDQS_DMA_AVAILABLE);
	    /* FALL THROUGH */
	}
	case PDQS_DMA_AVAILABLE: {
	    *csrs->csr_port_data_a = 0;
	    *csrs->csr_port_data_b = 0;
	    pdq_do_port_control(csrs, PDQ_PCTL_DMA_UNINIT);
	    state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
	    PDQ_ASSERT(state == PDQS_DMA_UNAVAILABLE);
	    /* FALL THROUGH */
	}
	case PDQS_DMA_UNAVAILABLE: {
	    break;
	}
    }
#endif
    /*
     * Now we should be in DMA_UNAVAILABLE.  So bring the PDQ into
     * DMA_AVAILABLE.
     */

    /*
     * Obtain the hardware address and firmware revisions
     * (MLA = my long address which is FDDI speak for hardware address)
     */
    pdq_read_mla(&pdq->pdq_csrs, &pdq->pdq_hwaddr);
    pdq_read_fwrev(&pdq->pdq_csrs, &pdq->pdq_fwrev);
    pdq->pdq_chip_rev = pdq_read_chiprev(&pdq->pdq_csrs);

    if (pdq->pdq_type == PDQ_DEFPA) {
	/*
	 * Disable interrupts and DMA.
	 */
	*pdq->pdq_pci_csrs.csr_pfi_mode_control = 0;
	*pdq->pdq_pci_csrs.csr_pfi_status = 0x10;
    }

    /*
     * Flush all the databuf queues.
     */
    pdq_flush_databuf_queue(&pdq->pdq_tx_info.tx_txq);
    buffers = (PDQ_OS_DATABUF_T **) pdq->pdq_rx_info.rx_buffers;
    for (idx = 0; idx < PDQ_RING_SIZE(pdq->pdq_dbp->pdqdb_receives); idx++) {
	if (buffers[idx] != NULL) {
	    PDQ_OS_DATABUF_FREE(buffers[idx]);
	    buffers[idx] = NULL;
	}
    }
    pdq->pdq_rx_info.rx_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_receives);
    buffers = (PDQ_OS_DATABUF_T **) pdq->pdq_host_smt_info.rx_buffers;
    for (idx = 0; idx < PDQ_RING_SIZE(pdq->pdq_dbp->pdqdb_host_smt); idx++) {
	if (buffers[idx] != NULL) {
	    PDQ_OS_DATABUF_FREE(buffers[idx]);
	    buffers[idx] = NULL;
	}
    }
    pdq->pdq_host_smt_info.rx_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_host_smt);

    /*
     * Reset the consumer indexes to 0.
     */
    pdq->pdq_cbp->pdqcb_receives = 0;
    pdq->pdq_cbp->pdqcb_transmits = 0;
    pdq->pdq_cbp->pdqcb_host_smt = 0;
    pdq->pdq_cbp->pdqcb_unsolicited_event = 0;
    pdq->pdq_cbp->pdqcb_command_response = 0;
    pdq->pdq_cbp->pdqcb_command_request = 0;

    /*
     * Reset the producer and completion indexes to 0.
     */
    pdq->pdq_command_info.ci_request_producer = 0;
    pdq->pdq_command_info.ci_response_producer = 0;
    pdq->pdq_command_info.ci_request_completion = 0;
    pdq->pdq_command_info.ci_response_completion = 0;
    pdq->pdq_unsolicited_info.ui_producer = 0;
    pdq->pdq_unsolicited_info.ui_completion = 0;
    pdq->pdq_rx_info.rx_producer = 0;
    pdq->pdq_rx_info.rx_completion = 0;
    pdq->pdq_tx_info.tx_producer = 0;
    pdq->pdq_tx_info.tx_completion = 0;
    pdq->pdq_host_smt_info.rx_producer = 0;
    pdq->pdq_host_smt_info.rx_completion = 0;

    pdq->pdq_command_info.ci_request_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_command_requests);
    pdq->pdq_command_info.ci_response_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_command_responses);
    pdq->pdq_unsolicited_info.ui_free = PDQ_NUM_UNSOLICITED_EVENTS;
    pdq->pdq_tx_info.tx_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_transmits);

    /*
     * Allow the DEFPA to do DMA.  Then program the physical
     * addresses of the consumer and descriptor blocks.
     */
    if (pdq->pdq_type == PDQ_DEFPA) {
#ifdef PDQTEST
	*pdq->pdq_pci_csrs.csr_pfi_mode_control = PDQ_PFI_MODE_DMA_ENABLE;
#else
	*pdq->pdq_pci_csrs.csr_pfi_mode_control = PDQ_PFI_MODE_DMA_ENABLE
	    |PDQ_PFI_MODE_PFI_PCI_INTR|PDQ_PFI_MODE_PDQ_PCI_INTR;
#endif
    }

    /*
     * Make the unsolicited queue has events ...
     */
    pdq_process_unsolicited_events(pdq);

    *csrs->csr_port_data_b = PDQ_DMA_BURST_8LW;
    *csrs->csr_port_data_a = PDQ_SUB_CMD_DMA_BURST_SIZE_SET;
    pdq_do_port_control(csrs, PDQ_PCTL_SUB_CMD);

    *csrs->csr_port_data_b = 0;
    *csrs->csr_port_data_a = PDQ_OS_VA_TO_PA(pdq->pdq_cbp);
    pdq_do_port_control(csrs, PDQ_PCTL_CONSUMER_BLOCK);

    *csrs->csr_port_data_b = 0;
    *csrs->csr_port_data_a = PDQ_OS_VA_TO_PA(pdq->pdq_dbp) | PDQ_DMA_INIT_LW_BSWAP_DATA;
    pdq_do_port_control(csrs, PDQ_PCTL_DMA_INIT);

    for (cnt = 0; cnt < 1000; cnt++) {
	state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
	if (state == PDQS_HALTED) {
	    if (pass > 0)
		return PDQS_HALTED;
	    pass = 1;
	    goto restart;
	}
	if (state == PDQS_DMA_AVAILABLE) {
	    PDQ_PRINTF(("Transition to DMA Available took %d spins\n", cnt));
	    break;
	}
	PDQ_OS_USEC_DELAY(1000);
    }
    PDQ_ASSERT(state == PDQS_DMA_AVAILABLE);

    *csrs->csr_host_int_type_0 = 0xFF;
    *csrs->csr_host_int_enable = 0 /* PDQ_HOST_INT_STATE_CHANGE
	|PDQ_HOST_INT_FATAL_ERROR|PDQ_HOST_INT_CMD_RSP_ENABLE
	|PDQ_HOST_INT_UNSOL_ENABLE */;

    /*
     * Any other command but START should be valid.
     */
    pdq->pdq_command_info.ci_pending_commands &= ~(PDQ_BITMASK(PDQC_START));
    if (pdq->pdq_flags & PDQ_PRINTCHARS)
	pdq->pdq_command_info.ci_pending_commands |= PDQ_BITMASK(PDQC_STATUS_CHARS_GET);
    pdq_queue_commands(pdq);

    if (pdq->pdq_flags & PDQ_PRINTCHARS) {
	/*
	 * Now wait (up to 100ms) for the command(s) to finish.
	 */
	for (cnt = 0; cnt < 1000; cnt++) {
	    pdq_process_command_responses(pdq);
	    if (pdq->pdq_command_info.ci_response_producer == pdq->pdq_command_info.ci_response_completion)
		break;
	    PDQ_OS_USEC_DELAY(1000);
	}
	state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
    }

    return state;
}

void
pdq_run(
    pdq_t *pdq)
{
    const pdq_csrs_t * const csrs = &pdq->pdq_csrs;
    pdq_state_t state;

    state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
    PDQ_ASSERT(state != PDQS_DMA_UNAVAILABLE);
    PDQ_ASSERT(state != PDQS_RESET);
    PDQ_ASSERT(state != PDQS_HALTED);
    PDQ_ASSERT(state != PDQS_UPGRADE);
    PDQ_ASSERT(state != PDQS_RING_MEMBER);
    switch (state) {
	case PDQS_DMA_AVAILABLE: {
	    /*
	     * The PDQ after being reset screws up some of its state.
	     * So we need to clear all the errors/interrupts so the real
	     * ones will get through.
	     */
	    *csrs->csr_host_int_type_0 = 0xFF;
	    *csrs->csr_host_int_enable = PDQ_HOST_INT_STATE_CHANGE|PDQ_HOST_INT_XMT_DATA_FLUSH
		|PDQ_HOST_INT_FATAL_ERROR|PDQ_HOST_INT_CMD_RSP_ENABLE|PDQ_HOST_INT_UNSOL_ENABLE
		|PDQ_HOST_INT_RX_ENABLE|PDQ_HOST_INT_TX_ENABLE|PDQ_HOST_INT_HOST_SMT_ENABLE;
	    /*
	     * Set the MAC and address filters and start up the PDQ.
	     */
	    pdq_process_unsolicited_events(pdq);
	    pdq_process_received_data(pdq, &pdq->pdq_rx_info,
				      pdq->pdq_dbp->pdqdb_receives,
				      pdq->pdq_cbp->pdqcb_receives,
				      PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_receives));
	    PDQ_DO_TYPE2_PRODUCER(pdq);
	    if (pdq->pdq_flags & PDQ_PASS_SMT) {
		pdq_process_received_data(pdq, &pdq->pdq_host_smt_info,
					  pdq->pdq_dbp->pdqdb_host_smt,
					  pdq->pdq_cbp->pdqcb_host_smt,
					  PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_host_smt));
		*csrs->csr_host_smt_producer = pdq->pdq_host_smt_info.rx_producer | (pdq->pdq_host_smt_info.rx_completion << 8);
	    }
	    pdq->pdq_command_info.ci_pending_commands = PDQ_BITMASK(PDQC_FILTER_SET)
		| PDQ_BITMASK(PDQC_ADDR_FILTER_SET) | PDQ_BITMASK(PDQC_START);
	    if (pdq->pdq_flags & PDQ_PRINTCHARS)
		pdq->pdq_command_info.ci_pending_commands |= PDQ_BITMASK(PDQC_STATUS_CHARS_GET);
	    pdq_queue_commands(pdq);
	    break;
	}
	case PDQS_LINK_UNAVAILABLE:
	case PDQS_LINK_AVAILABLE: {
	    pdq->pdq_command_info.ci_pending_commands = PDQ_BITMASK(PDQC_FILTER_SET)
		| PDQ_BITMASK(PDQC_ADDR_FILTER_SET);
	    if (pdq->pdq_flags & PDQ_PRINTCHARS)
		pdq->pdq_command_info.ci_pending_commands |= PDQ_BITMASK(PDQC_STATUS_CHARS_GET);
	    if (pdq->pdq_flags & PDQ_PASS_SMT) {
		pdq_process_received_data(pdq, &pdq->pdq_host_smt_info,
					  pdq->pdq_dbp->pdqdb_host_smt,
					  pdq->pdq_cbp->pdqcb_host_smt,
					  PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_host_smt));
		*csrs->csr_host_smt_producer = pdq->pdq_host_smt_info.rx_producer | (pdq->pdq_host_smt_info.rx_completion << 8);
	    }
	    pdq_process_unsolicited_events(pdq);
	    pdq_queue_commands(pdq);
	    break;
	}
	case PDQS_RING_MEMBER: {
	}
	    break;
	default:
	    break;
    }
}

int
pdq_interrupt(
    pdq_t *pdq)
{
    const pdq_csrs_t * const csrs = &pdq->pdq_csrs;
    pdq_uint32_t data;
    int progress = 0;

    if (pdq->pdq_type == PDQ_DEFPA)
	if (*pdq->pdq_pci_csrs.csr_pfi_status & 0x10)
	    *pdq->pdq_pci_csrs.csr_pfi_status = 0x10;

    while ((data = *csrs->csr_port_status) & PDQ_PSTS_INTR_PENDING) {
	progress = 1;
	PDQ_PRINTF(("PDQ Interrupt: Status = 0x%08x\n", data));
	if (data & PDQ_PSTS_RCV_DATA_PENDING) {
	    pdq_process_received_data(pdq, &pdq->pdq_rx_info,
				      pdq->pdq_dbp->pdqdb_receives,
				      pdq->pdq_cbp->pdqcb_receives,
				      PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_receives));
	    PDQ_DO_TYPE2_PRODUCER(pdq);
	}
	if (data & PDQ_PSTS_HOST_SMT_PENDING) {
	    pdq_process_received_data(pdq, &pdq->pdq_host_smt_info,
				      pdq->pdq_dbp->pdqdb_host_smt,
				      pdq->pdq_cbp->pdqcb_host_smt,
				      PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_host_smt));
	    *csrs->csr_host_smt_producer = pdq->pdq_host_smt_info.rx_producer | (pdq->pdq_host_smt_info.rx_completion << 8);
	}
	if (data & PDQ_PSTS_XMT_DATA_PENDING)
	    pdq_process_transmitted_data(pdq);
	if (data & PDQ_PSTS_UNSOL_PENDING)
	    pdq_process_unsolicited_events(pdq);
	if (data & PDQ_PSTS_CMD_RSP_PENDING)
	    pdq_process_command_responses(pdq);
	if (data & PDQ_PSTS_TYPE_0_PENDING) {
	    data = *csrs->csr_host_int_type_0;
	    if (data & PDQ_HOST_INT_STATE_CHANGE) {
		pdq_state_t state = PDQ_PSTS_ADAPTER_STATE(*csrs->csr_port_status);
		printf(PDQ_OS_PREFIX "%s", PDQ_OS_PREFIX_ARGS, pdq_adapter_states[state]);
		if (state == PDQS_LINK_UNAVAILABLE) {
		    pdq->pdq_flags &= ~PDQ_TXOK;
		} else if (state == PDQS_LINK_AVAILABLE) {
		    pdq->pdq_flags |= PDQ_TXOK;
		    pdq_os_restart_transmitter(pdq);
		} else if (state == PDQS_HALTED) {
		    pdq_response_error_log_get_t log_entry;
		    pdq_halt_code_t halt_code = PDQ_PSTS_HALT_ID(*csrs->csr_port_status);
		    printf(": halt code = %d (%s)\n",
			   halt_code, pdq_halt_codes[halt_code]);
		    if (halt_code == PDQH_DMA_ERROR) {
			PDQ_PRINTF(("\tPFI status = 0x%x, Host 0 Fatal Interrupt = 0x%x\n",
			       *pdq->pdq_pci_csrs.csr_pfi_status,
			       data & PDQ_HOST_INT_FATAL_ERROR));
		    }
		    pdq_read_error_log(pdq, &log_entry);
		    pdq_stop(pdq);
		    if (pdq->pdq_flags & PDQ_RUNNING)
			pdq_run(pdq);
		    return 1;
		}
		printf("\n");
		*csrs->csr_host_int_type_0 = PDQ_HOST_INT_STATE_CHANGE;
	    }
	    if (data & PDQ_HOST_INT_FATAL_ERROR) {
		pdq_stop(pdq);
		if (pdq->pdq_flags & PDQ_RUNNING)
		    pdq_run(pdq);
		return 1;
	    }
	    if (data & PDQ_HOST_INT_XMT_DATA_FLUSH) {
		printf(PDQ_OS_PREFIX "Flushing transmit queue\n", PDQ_OS_PREFIX_ARGS);
		pdq->pdq_flags &= ~PDQ_TXOK;
		pdq_flush_transmitter(pdq);
		pdq_do_port_control(csrs, PDQ_PCTL_XMT_DATA_FLUSH_DONE);
		*csrs->csr_host_int_type_0 = PDQ_HOST_INT_XMT_DATA_FLUSH;
	    }
	}
	if (pdq->pdq_type == PDQ_DEFPA)
	    if (*pdq->pdq_pci_csrs.csr_pfi_status & 0x10)
		*pdq->pdq_pci_csrs.csr_pfi_status = 0x10;
    }
    return progress;
}

pdq_t *
pdq_initialize(
    void *csr_va,
    const char *name,
    int unit,
    void *ctx,
    pdq_type_t type)
{
    pdq_t *pdq;
    pdq_state_t state;
    const pdq_uint32_t contig_bytes = (sizeof(pdq_descriptor_block_t) * 2) - PDQ_OS_PAGESIZE;
    pdq_uint8_t *p;
    int idx;

    PDQ_ASSERT(sizeof(pdq_descriptor_block_t) == 8192);
    PDQ_ASSERT(sizeof(pdq_consumer_block_t) == 64);
    PDQ_ASSERT(sizeof(pdq_response_filter_get_t) == PDQ_SIZE_RESPONSE_FILTER_GET);
    PDQ_ASSERT(sizeof(pdq_cmd_addr_filter_set_t) == PDQ_SIZE_CMD_ADDR_FILTER_SET);
    PDQ_ASSERT(sizeof(pdq_response_addr_filter_get_t) == PDQ_SIZE_RESPONSE_ADDR_FILTER_GET);
    PDQ_ASSERT(sizeof(pdq_response_status_chars_get_t) == PDQ_SIZE_RESPONSE_STATUS_CHARS_GET);
    PDQ_ASSERT(sizeof(pdq_response_fddi_mib_get_t) == PDQ_SIZE_RESPONSE_FDDI_MIB_GET);
    PDQ_ASSERT(sizeof(pdq_response_dec_ext_mib_get_t) == PDQ_SIZE_RESPONSE_DEC_EXT_MIB_GET);
    PDQ_ASSERT(sizeof(pdq_unsolicited_event_t) == 512);

    pdq = (pdq_t *) PDQ_OS_MEMALLOC(sizeof(pdq_t));
    if (pdq == NULL) {
	PDQ_PRINTF(("malloc(%d) failed\n", sizeof(*pdq)));
	return NULL;
    }
    PDQ_OS_MEMZERO(pdq, sizeof(pdq_t));
    pdq->pdq_type = type;
    pdq->pdq_unit = unit;
    pdq->pdq_os_ctx = (void *) ctx;
    pdq->pdq_os_name = name;
    pdq->pdq_flags = PDQ_PRINTCHARS;
    /*
     * Allocate the additional data structures required by
     * the PDQ driver.  Allocate a contiguous region of memory
     * for the descriptor block.  We need to allocated enough
     * to guarantee that we will a get 8KB block of memory aligned
     * on a 8KB boundary.  This turns to require that we allocate
     * (N*2 - 1 page) pages of memory.  On machine with less than
     * a 8KB page size, it mean we will allocate more memory than
     * we need.  The extra will be used for the unsolicited event
     * buffers (though on machines with 8KB pages we will to allocate
     * them separately since there will be nothing left overs.)
     */
    p = (pdq_uint8_t *) PDQ_OS_MEMALLOC_CONTIG(contig_bytes);
    if (p != NULL) {
	pdq_physaddr_t physaddr = PDQ_OS_VA_TO_PA(p) & 0x1FFF;
	if (physaddr) {
	    pdq->pdq_unsolicited_info.ui_events = (pdq_unsolicited_event_t *) p;
	    pdq->pdq_dbp = (pdq_descriptor_block_t *) &p[0x2000 - physaddr];
	} else {
	    pdq->pdq_dbp = (pdq_descriptor_block_t *) p;
	    pdq->pdq_unsolicited_info.ui_events = (pdq_unsolicited_event_t *) &p[0x2000];
	}
    }
    if (contig_bytes == sizeof(pdq_descriptor_block_t)) {
	pdq->pdq_unsolicited_info.ui_events =
	    (pdq_unsolicited_event_t *) PDQ_OS_MEMALLOC(
		PDQ_NUM_UNSOLICITED_EVENTS * sizeof(pdq_unsolicited_event_t));
    }

    /*
     * Make sure everything got allocated.  If not, free what did
     * get allocated and return.
     */
    if (pdq->pdq_dbp == NULL || pdq->pdq_unsolicited_info.ui_events == NULL) {
      cleanup_and_return:
	if (pdq->pdq_dbp != NULL)
	    PDQ_OS_MEMFREE_CONTIG(pdq->pdq_dbp, contig_bytes);
	if (contig_bytes == sizeof(pdq_descriptor_block_t) && pdq->pdq_unsolicited_info.ui_events != NULL)
	    PDQ_OS_MEMFREE(pdq->pdq_unsolicited_info.ui_events,
			   PDQ_NUM_UNSOLICITED_EVENTS * sizeof(pdq_unsolicited_event_t));
	PDQ_OS_MEMFREE(pdq, sizeof(pdq_t));
	return NULL;
    }

    pdq->pdq_cbp = (volatile pdq_consumer_block_t *) &pdq->pdq_dbp->pdqdb_consumer;
    pdq->pdq_command_info.ci_bufstart = (pdq_uint8_t *) pdq->pdq_dbp->pdqdb_command_pool;
    pdq->pdq_rx_info.rx_buffers = (void *) pdq->pdq_dbp->pdqdb_receive_buffers;

    pdq->pdq_host_smt_info.rx_buffers = (void *) pdq->pdq_dbp->pdqdb_host_smt_buffers;

    PDQ_PRINTF(("PDQ Descriptor Block = %p\n", pdq->pdq_dbp));
    PDQ_PRINTF(("    Recieve Queue          = %p\n", pdq->pdq_dbp->pdqdb_receives));
    PDQ_PRINTF(("    Transmit Queue         = %p\n", pdq->pdq_dbp->pdqdb_transmits));
    PDQ_PRINTF(("    Host SMT Queue         = %p\n", pdq->pdq_dbp->pdqdb_host_smt));
    PDQ_PRINTF(("    Command Response Queue = %p\n", pdq->pdq_dbp->pdqdb_command_responses));
    PDQ_PRINTF(("    Command Request Queue  = %p\n", pdq->pdq_dbp->pdqdb_command_requests));
    PDQ_PRINTF(("PDQ Consumer Block = %p\n", pdq->pdq_cbp));

    /*
     * Zero out the descriptor block.  Not really required but
     * it pays to be neat.  This will also zero out the consumer
     * block, command pool, and buffer pointers for the receive
     * host_smt rings.
     */
    PDQ_OS_MEMZERO(pdq->pdq_dbp, sizeof(*pdq->pdq_dbp));

    /*
     * Initialize the CSR references.
     */
    pdq_init_csrs(&pdq->pdq_csrs, csr_va, 1);
    switch (pdq->pdq_type) {
	case PDQ_DEFPA: pdq_init_pci_csrs(&pdq->pdq_pci_csrs, csr_va, 1); break;
#ifdef PDQ_DO_EISA
	case PDQ_DEFEA: pdq_init_esia_csrs(&pdq->pdq_eisa_csrs, csr_va, 1); break;
#endif
	default:
	    break;
    }

    PDQ_PRINTF(("PDQ CSRs:\n"));
    PDQ_PRINTF(("    Port Reset                = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_port_reset, *pdq->pdq_csrs.csr_port_reset));
    PDQ_PRINTF(("    Host Data                 = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_host_data, *pdq->pdq_csrs.csr_host_data));
    PDQ_PRINTF(("    Port Control              = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_port_control, *pdq->pdq_csrs.csr_port_control));
    PDQ_PRINTF(("    Port Data A               = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_port_data_a, *pdq->pdq_csrs.csr_port_data_a));
    PDQ_PRINTF(("    Port Data B               = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_port_data_b, *pdq->pdq_csrs.csr_port_data_b));
    PDQ_PRINTF(("    Port Status               = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_port_status, *pdq->pdq_csrs.csr_port_status));
    PDQ_PRINTF(("    Host Int Type 0           = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_host_int_type_0, *pdq->pdq_csrs.csr_host_int_type_0));
    PDQ_PRINTF(("    Host Int Enable           = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_host_int_enable, *pdq->pdq_csrs.csr_host_int_enable));
    PDQ_PRINTF(("    Type 2 Producer           = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_type_2_producer, *pdq->pdq_csrs.csr_type_2_producer));
    PDQ_PRINTF(("    Command Response Producer = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_cmd_response_producer, *pdq->pdq_csrs.csr_cmd_response_producer));
    PDQ_PRINTF(("    Command Request Producer  = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_cmd_request_producer, *pdq->pdq_csrs.csr_cmd_request_producer));
    PDQ_PRINTF(("    Host SMT Producer         = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_host_smt_producer, *pdq->pdq_csrs.csr_host_smt_producer));
    PDQ_PRINTF(("    Unsolicited Producer      = %p [0x%08x]\n",
	   pdq->pdq_csrs.csr_unsolicited_producer, *pdq->pdq_csrs.csr_unsolicited_producer));

    /*
     * Initialize the command information block
     */
    pdq->pdq_command_info.ci_buffree = sizeof(pdq->pdq_dbp->pdqdb_command_pool);
    pdq->pdq_command_info.ci_bufend = pdq->pdq_command_info.ci_bufstart + pdq->pdq_command_info.ci_buffree;
    pdq->pdq_command_info.ci_bufptr = pdq->pdq_command_info.ci_bufstart;
    pdq->pdq_command_info.ci_pa_bufstart = PDQ_OS_VA_TO_PA(pdq->pdq_command_info.ci_bufstart);
    pdq->pdq_command_info.ci_request_max = pdq->pdq_command_info.ci_request_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_command_requests);
    pdq->pdq_command_info.ci_response_max = pdq->pdq_command_info.ci_response_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_command_responses);

    /*
     * Initialize the unsolicited event information block
     */
    pdq->pdq_unsolicited_info.ui_free = PDQ_NUM_UNSOLICITED_EVENTS;
    pdq->pdq_unsolicited_info.ui_pa_bufstart = PDQ_OS_VA_TO_PA(pdq->pdq_unsolicited_info.ui_events);
    for (idx = 0; idx < sizeof(pdq->pdq_dbp->pdqdb_unsolicited_events)/sizeof(pdq->pdq_dbp->pdqdb_unsolicited_events[0]); idx++) {
	pdq_rxdesc_t *rxd = &pdq->pdq_dbp->pdqdb_unsolicited_events[idx];
	pdq_unsolicited_event_t *event = &pdq->pdq_unsolicited_info.ui_events[idx & (PDQ_NUM_UNSOLICITED_EVENTS-1)];

	rxd->rxd_sop = 1;
	rxd->rxd_seg_cnt = 0;
	rxd->rxd_seg_len_hi = sizeof(pdq_unsolicited_event_t) / 16;
	rxd->rxd_pa_lo = pdq->pdq_unsolicited_info.ui_pa_bufstart + (const pdq_uint8_t *) event
	    - (const pdq_uint8_t *) pdq->pdq_unsolicited_info.ui_events;
	rxd->rxd_pa_hi = 0;
    }
    /*
     * Initialize the receive information blocks (normal and SMT).
     */
    pdq->pdq_rx_info.rx_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_receives);
    pdq->pdq_rx_info.rx_target = pdq->pdq_rx_info.rx_free - PDQ_RX_SEGCNT * 8;

    pdq->pdq_host_smt_info.rx_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_host_smt);
    pdq->pdq_host_smt_info.rx_target = pdq->pdq_host_smt_info.rx_free - PDQ_RX_SEGCNT * 3;

    /*
     * Initialize the transmit information block.
     */
    pdq->pdq_tx_hdr[0] = PDQ_FDDI_PH0;
    pdq->pdq_tx_hdr[1] = PDQ_FDDI_PH1;
    pdq->pdq_tx_hdr[2] = PDQ_FDDI_PH2;
    pdq->pdq_tx_info.tx_free = PDQ_RING_MASK(pdq->pdq_dbp->pdqdb_transmits);
    pdq->pdq_tx_info.tx_hdrdesc.txd_seg_len = sizeof(pdq->pdq_tx_hdr);
    pdq->pdq_tx_info.tx_hdrdesc.txd_sop = 1;
    pdq->pdq_tx_info.tx_hdrdesc.txd_pa_lo = PDQ_OS_VA_TO_PA(pdq->pdq_tx_hdr);

    state = PDQ_PSTS_ADAPTER_STATE(*pdq->pdq_csrs.csr_port_status);
    PDQ_PRINTF(("PDQ Adapter State = %s\n", pdq_adapter_states[state]));

    /*
     * Stop the PDQ if it is running and put it into a known state.
     */
    state = pdq_stop(pdq);

    /* state = PDQ_PSTS_ADAPTER_STATE(*pdq->pdq_csrs.csr_port_status); */
    PDQ_PRINTF(("PDQ Adapter State = %s\n", pdq_adapter_states[state]));
    PDQ_ASSERT(state == PDQS_DMA_AVAILABLE);
    /*
     * If the adapter is not the state we expect, then the initialization
     * failed.  Cleanup and exit.
     */
    if (state == PDQS_RESET || state == PDQS_HALTED || state == PDQS_UPGRADE)
	goto cleanup_and_return;

    PDQ_PRINTF(("PDQ Hardware Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
	   pdq->pdq_hwaddr.lanaddr_bytes[0], pdq->pdq_hwaddr.lanaddr_bytes[1],
	   pdq->pdq_hwaddr.lanaddr_bytes[2], pdq->pdq_hwaddr.lanaddr_bytes[3],
	   pdq->pdq_hwaddr.lanaddr_bytes[4], pdq->pdq_hwaddr.lanaddr_bytes[5]));
    PDQ_PRINTF(("PDQ Firmware Revision = %c%c%c%c\n",
	   pdq->pdq_fwrev.fwrev_bytes[0], pdq->pdq_fwrev.fwrev_bytes[1],
	   pdq->pdq_fwrev.fwrev_bytes[2], pdq->pdq_fwrev.fwrev_bytes[3]));
    PDQ_PRINTF(("PDQ Chip Revision = "));
    switch (pdq->pdq_chip_rev) {
	case PDQ_CHIP_REV_A_B_OR_C: PDQ_PRINTF(("Rev C or below")); break;
	case PDQ_CHIP_REV_D: PDQ_PRINTF(("Rev D")); break;
	case PDQ_CHIP_REV_E: PDQ_PRINTF(("Rev E")); break;
	default: PDQ_PRINTF(("Unknown Rev %d", (int) pdq->pdq_chip_rev));
    }
    PDQ_PRINTF(("\n"));

    return pdq;
}
