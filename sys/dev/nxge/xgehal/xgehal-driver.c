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
 * $FreeBSD: src/sys/dev/nxge/xgehal/xgehal-driver.c,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <dev/nxge/include/xgehal-driver.h>
#include <dev/nxge/include/xgehal-device.h>

static xge_hal_driver_t g_driver;
xge_hal_driver_t *g_xge_hal_driver = NULL;
char *g_xge_hal_log = NULL;

#ifdef XGE_OS_MEMORY_CHECK
xge_os_malloc_t g_malloc_arr[XGE_OS_MALLOC_CNT_MAX];
int g_malloc_cnt = 0;
#endif

/*
 * Runtime tracing support
 */
static unsigned long g_module_mask_default = 0;
unsigned long *g_module_mask = &g_module_mask_default;
static int g_level_default = 0;
int *g_level = &g_level_default;

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
static xge_os_tracebuf_t g_tracebuf;
char *dmesg, *dmesg_start;

/**
 * xge_hal_driver_tracebuf_dump - Dump the trace buffer.
 *
 * Dump the trace buffer contents.
 */
void
xge_hal_driver_tracebuf_dump(void)
{
	int i;
	int off = 0;

	if (g_xge_os_tracebuf == NULL) {
	    return;
	}

	xge_os_printf("################ Trace dump Begin ###############");
	if (g_xge_os_tracebuf->wrapped_once) {
	    for (i = 0; i < g_xge_os_tracebuf->size -
	            g_xge_os_tracebuf->offset; i += off) {
	        if (*(dmesg_start + i))
	            xge_os_printf(dmesg_start + i);
	        off = xge_os_strlen(dmesg_start + i) + 1;
	    }
	}
	for (i = 0; i < g_xge_os_tracebuf->offset; i += off) {
	    if (*(dmesg + i))
	        xge_os_printf(dmesg + i);
	    off = xge_os_strlen(dmesg + i) + 1;
	}
	xge_os_printf("################ Trace dump End ###############");
}

xge_hal_status_e
xge_hal_driver_tracebuf_read(int bufsize, char *retbuf, int *retsize)
{
	int i;
	int off = 0, retbuf_off = 0;

	*retsize = 0;
	*retbuf = 0;

	if (g_xge_os_tracebuf == NULL) {
	    return XGE_HAL_FAIL;
	}

	if (g_xge_os_tracebuf->wrapped_once) {
	    for (i = 0; i < g_xge_os_tracebuf->size -
	            g_xge_os_tracebuf->offset; i += off) {
	        if (*(dmesg_start + i)) {
	            xge_os_sprintf(retbuf + retbuf_off, "%s\n", dmesg_start + i);
	            retbuf_off += xge_os_strlen(dmesg_start + i) + 1;
	            if (retbuf_off > bufsize)
	                return XGE_HAL_ERR_OUT_OF_MEMORY;
	        }
	        off = xge_os_strlen(dmesg_start + i) + 1;
	    }
	}
	for (i = 0; i < g_xge_os_tracebuf->offset; i += off) {
	    if (*(dmesg + i)) {
	        xge_os_sprintf(retbuf + retbuf_off, "%s\n", dmesg + i);
	        retbuf_off += xge_os_strlen(dmesg + i) + 1;
	        if (retbuf_off > bufsize)
	            return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }
	    off = xge_os_strlen(dmesg + i) + 1;
	}

	*retsize = retbuf_off;
	*(retbuf + retbuf_off + 1) = 0;

	return XGE_HAL_OK;
}
#endif
xge_os_tracebuf_t *g_xge_os_tracebuf = NULL;

#ifdef XGE_HAL_DEBUG_BAR0_OFFSET
void
xge_hal_driver_bar0_offset_check(void)
{
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, adapter_status) ==
	       0x108);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, tx_traffic_int) ==
	       0x08E0);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, dtx_control) ==
	       0x09E8);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, tx_fifo_partition_0) ==
	       0x1108);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, pcc_enable) ==
	       0x1170);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, prc_rxd0_n[0]) ==
	       0x1930);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, rti_command_mem) ==
	       0x19B8);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, mac_cfg) ==
	       0x2100);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, rmac_addr_cmd_mem) ==
	       0x2128);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, mac_link_util) ==
	       0x2170);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, mc_pause_thresh_q0q3) ==
	       0x2918);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, pcc_err_reg) ==
	       0x1040);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, rxdma_int_status) ==
	       0x1800);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, mac_tmac_err_reg) ==
	       0x2010);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, mc_err_reg) ==
	       0x2810);
	xge_assert(xge_offsetof(xge_hal_pci_bar0_t, xgxs_int_status) ==
	       0x3000);
}
#endif

/**
 * xge_hal_driver_initialize - Initialize HAL.
 * @config: HAL configuration, see xge_hal_driver_config_t{}.
 * @uld_callbacks: Upper-layer driver callbacks, e.g. link-up.
 *
 * HAL initialization entry point. Not to confuse with device initialization
 * (note that HAL "contains" zero or more Xframe devices).
 *
 * Returns: XGE_HAL_OK - success;
 * XGE_HAL_ERR_BAD_DRIVER_CONFIG - Driver configuration params invalid.
 *
 * See also: xge_hal_device_initialize(), xge_hal_status_e{},
 * xge_hal_uld_cbs_t{}.
 */
xge_hal_status_e
xge_hal_driver_initialize(xge_hal_driver_config_t *config,
	        xge_hal_uld_cbs_t *uld_callbacks)
{
	xge_hal_status_e status;

	g_xge_hal_driver = &g_driver;

	xge_hal_driver_debug_module_mask_set(XGE_DEBUG_MODULE_MASK_DEF);
	xge_hal_driver_debug_level_set(XGE_DEBUG_LEVEL_DEF);

#ifdef XGE_HAL_DEBUG_BAR0_OFFSET
	xge_hal_driver_bar0_offset_check();
#endif

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
	if (config->tracebuf_size == 0)
	    /*
	     * Trace buffer implementation is not lock protected.
	     * The only harm to expect is memcpy() to go beyond of
	     * allowed boundaries. To make it safe (driver-wise),
	     * we pre-allocate needed number of extra bytes.
	     */
	    config->tracebuf_size = XGE_HAL_DEF_CIRCULAR_ARR +
	                XGE_OS_TRACE_MSGBUF_MAX;
#endif

	status = __hal_driver_config_check(config);
	if (status != XGE_HAL_OK)
	    return status;

	xge_os_memzero(g_xge_hal_driver,  sizeof(xge_hal_driver_t));

	/* apply config */
	xge_os_memcpy(&g_xge_hal_driver->config, config,
	            sizeof(xge_hal_driver_config_t));

	/* apply ULD callbacks */
	xge_os_memcpy(&g_xge_hal_driver->uld_callbacks, uld_callbacks,
	                sizeof(xge_hal_uld_cbs_t));

	g_xge_hal_driver->is_initialized = 1;

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
	g_tracebuf.size = config->tracebuf_size;
	g_tracebuf.data = (char *)xge_os_malloc(NULL, g_tracebuf.size);
	if (g_tracebuf.data == NULL) {
	    xge_os_printf("cannot allocate trace buffer!");
	    return XGE_HAL_ERR_OUT_OF_MEMORY;
	}
	/* timestamps disabled by default */
	g_tracebuf.timestamp = config->tracebuf_timestamp_en;
	if (g_tracebuf.timestamp) {
	    xge_os_timestamp(g_tracebuf.msg);
	    g_tracebuf.msgbuf_max = XGE_OS_TRACE_MSGBUF_MAX -
	                xge_os_strlen(g_tracebuf.msg);
	} else
	    g_tracebuf.msgbuf_max = XGE_OS_TRACE_MSGBUF_MAX;
	g_tracebuf.offset = 0;
	*g_tracebuf.msg = 0;
	xge_os_memzero(g_tracebuf.data, g_tracebuf.size);
	g_xge_os_tracebuf = &g_tracebuf;
	dmesg = g_tracebuf.data;
	*dmesg = 0;
#endif
	return XGE_HAL_OK;
}

/**
 * xge_hal_driver_terminate - Terminate HAL.
 *
 * HAL termination entry point.
 *
 * See also: xge_hal_device_terminate().
 */
void
xge_hal_driver_terminate(void)
{
	g_xge_hal_driver->is_initialized = 0;

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
	if (g_tracebuf.size) {
	    xge_os_free(NULL, g_tracebuf.data, g_tracebuf.size);
	}
#endif

	g_xge_hal_driver = NULL;

#ifdef XGE_OS_MEMORY_CHECK
	{
	    int i, leaks=0;
	    xge_os_printf("OSPAL: max g_malloc_cnt %d", g_malloc_cnt);
	    for (i=0; i<g_malloc_cnt; i++) {
	        if (g_malloc_arr[i].ptr != NULL) {
	            xge_os_printf("OSPAL: memory leak detected at "
	                "%s:%d:"XGE_OS_LLXFMT":%d",
	                g_malloc_arr[i].file,
	                g_malloc_arr[i].line,
	                (unsigned long long)(ulong_t)
	                    g_malloc_arr[i].ptr,
	                g_malloc_arr[i].size);
	            leaks++;
	        }
	    }
	    if (leaks) {
	        xge_os_printf("OSPAL: %d memory leaks detected", leaks);
	    } else {
	        xge_os_printf("OSPAL: no memory leaks detected");
	    }
	}
#endif
}
