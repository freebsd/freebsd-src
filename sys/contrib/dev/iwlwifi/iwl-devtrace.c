/*-
 * Copyright (c) 2021 The FreeBSD Foundation
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "iwl-devtrace.h"

#include <sys/sdt.h>

SDT_PROVIDER_DEFINE(iwlwifi);

SDT_PROBE_DEFINE4(iwlwifi, trace, dev_hcmd, ,
    "const struct device *dev",
    "struct iwl_host_cmd *", "uint16_t",
    "struct iwl_cmd_header_wide *");

void
trace_iwlwifi_dev_hcmd(const struct device *dev,
    struct iwl_host_cmd *cmd, uint16_t cmd_size,
    struct iwl_cmd_header_wide *hdr_wide)
{

	SDT_PROBE4(iwlwifi, trace, dev_hcmd, ,
	    dev, cmd, cmd_size, hdr_wide);
}

SDT_PROBE_DEFINE4(iwlwifi, trace, dev_rx, ,
    "const struct device *dev",
    "const struct iwl_trans *",
    "struct iwl_rx_packet *", "size_t");

void
trace_iwlwifi_dev_rx(const struct device *dev,
    const struct iwl_trans *trans,
    struct iwl_rx_packet *pkt, size_t len)
{

	SDT_PROBE4(iwlwifi, trace, dev_rx, ,
	    dev, trans, pkt, len);
}

SDT_PROBE_DEFINE4(iwlwifi, trace, dev_rx_data, ,
    "const struct device *dev",
    "const struct iwl_trans *",
    "struct iwl_rx_packet *", "size_t");

void
trace_iwlwifi_dev_rx_data(const struct device *dev,
    const struct iwl_trans *trans,
    struct iwl_rx_packet *pkt, size_t len)
{

	SDT_PROBE4(iwlwifi, trace, dev_rx_data, ,
	    dev, trans, pkt, len);
}

