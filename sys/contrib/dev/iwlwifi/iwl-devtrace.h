/*-
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 * $FreeBSD$
 */

#ifndef _IWL_DEVTRACE_H
#define	_IWL_DEVTRACE_H

#include <linux/types.h>
#include <linux/device.h>

#include "iwl-trans.h"

void trace_iwlwifi_dev_hcmd(const struct device *,
    struct iwl_host_cmd *, uint16_t,
    struct iwl_cmd_header_wide *);

void trace_iwlwifi_dev_rx(const struct device *,
    const struct iwl_trans *,
    struct iwl_rx_packet *, size_t);

void trace_iwlwifi_dev_rx_data(const struct device *,
    const struct iwl_trans *,
    struct iwl_rx_packet *, size_t);

#define	trace_iwlwifi_dev_ict_read(...)
#define	trace_iwlwifi_dev_ioread32(...)
#define	trace_iwlwifi_dev_ioread_prph32(...)
#define	trace_iwlwifi_dev_iowrite32(...)
#define	trace_iwlwifi_dev_iowrite64(...)
#define	trace_iwlwifi_dev_iowrite8(...)
#define	trace_iwlwifi_dev_iowrite_prph32(...)
#define	trace_iwlwifi_dev_iowrite_prph64(...)
#define	trace_iwlwifi_dev_irq(...)
#define	trace_iwlwifi_dev_irq_msix(...)
#define	trace_iwlwifi_dev_tx(...)
#define	trace_iwlwifi_dev_tx_tb(...)

#define	trace_iwlwifi_crit(...)
#define	trace_iwlwifi_dbg(...)
#define	trace_iwlwifi_err(...)
#define	trace_iwlwifi_info(...)
#define	trace_iwlwifi_warn(...)

#endif /* _IWL_DEVTRACE_H */
