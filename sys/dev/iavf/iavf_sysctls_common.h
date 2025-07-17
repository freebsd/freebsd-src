/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file iavf_sysctls_common.h
 * @brief Sysctls common to the legacy and iflib drivers
 *
 * Contains global sysctl definitions which are shared between the legacy and
 * iflib driver implementations.
 */
#ifndef _IAVF_SYSCTLS_COMMON_H_
#define _IAVF_SYSCTLS_COMMON_H_

#include <sys/sysctl.h>

/* Root node for tunables */
static SYSCTL_NODE(_hw, OID_AUTO, iavf, CTLFLAG_RD, 0,
    "IAVF driver parameters");

/**
 * @var iavf_enable_head_writeback
 * @brief Sysctl to control Tx descriptor completion method
 *
 * Global sysctl value indicating whether to enable the head writeback method
 * of Tx descriptor completion notification.
 *
 * @remark Head writeback has been deprecated and will only work on 700-series
 * virtual functions only.
 */
static int iavf_enable_head_writeback = 0;
SYSCTL_INT(_hw_iavf, OID_AUTO, enable_head_writeback, CTLFLAG_RDTUN,
    &iavf_enable_head_writeback, 0,
    "For detecting last completed TX descriptor by hardware, use value written by HW instead of checking descriptors. For 700 series VFs only.");

/**
 * @var iavf_core_debug_mask
 * @brief Debug mask for driver messages
 *
 * Global sysctl value used to control what set of debug messages are printed.
 * Used by messages in core driver code.
 */
static int iavf_core_debug_mask = 0;
SYSCTL_INT(_hw_iavf, OID_AUTO, core_debug_mask, CTLFLAG_RDTUN,
    &iavf_core_debug_mask, 0,
    "Display debug statements that are printed in non-shared code");

/**
 * @var iavf_shared_debug_mask
 * @brief Debug mask for shared code messages
 *
 * Global sysctl value used to control what set of debug messages are printed.
 * Used by messages in shared device logic code.
 */
static int iavf_shared_debug_mask = 0;
SYSCTL_INT(_hw_iavf, OID_AUTO, shared_debug_mask, CTLFLAG_RDTUN,
    &iavf_shared_debug_mask, 0,
    "Display debug statements that are printed in shared code");

/**
 * @var iavf_rx_itr
 * @brief Rx interrupt throttling rate
 *
 * Controls the default interrupt throttling rate for receive interrupts.
 */
int iavf_rx_itr = IAVF_ITR_8K;
SYSCTL_INT(_hw_iavf, OID_AUTO, rx_itr, CTLFLAG_RDTUN,
    &iavf_rx_itr, 0, "RX Interrupt Rate");

/**
 * @var iavf_tx_itr
 * @brief Tx interrupt throttling rate
 *
 * Controls the default interrupt throttling rate for transmit interrupts.
 */
int iavf_tx_itr = IAVF_ITR_4K;
SYSCTL_INT(_hw_iavf, OID_AUTO, tx_itr, CTLFLAG_RDTUN,
    &iavf_tx_itr, 0, "TX Interrupt Rate");

/**
 * iavf_save_tunables - Sanity check and save off tunable values
 * @sc: device softc
 *
 * @pre "iavf_drv_info.h" is included before this file
 * @pre dev pointer in sc is valid
 */
static void
iavf_save_tunables(struct iavf_sc *sc)
{
	device_t dev = sc->dev;
	u16 pci_device_id = pci_get_device(dev);

	/* Save tunable information */
	sc->dbg_mask = (enum iavf_dbg_mask)iavf_core_debug_mask;
	sc->hw.debug_mask = iavf_shared_debug_mask;

	if (pci_device_id == IAVF_DEV_ID_VF ||
	    pci_device_id == IAVF_DEV_ID_X722_VF)
		sc->vsi.enable_head_writeback = !!(iavf_enable_head_writeback);
	else if (iavf_enable_head_writeback) {
		device_printf(dev, "Head writeback can only be enabled on 700 series Virtual Functions\n");
		device_printf(dev, "Using descriptor writeback instead...\n");
		sc->vsi.enable_head_writeback = 0;
	}

	if (iavf_tx_itr < 0 || iavf_tx_itr > IAVF_MAX_ITR) {
		device_printf(dev, "Invalid tx_itr value of %d set!\n",
		    iavf_tx_itr);
		device_printf(dev, "tx_itr must be between %d and %d, "
		    "inclusive\n",
		    0, IAVF_MAX_ITR);
		device_printf(dev, "Using default value of %d instead\n",
		    IAVF_ITR_4K);
		sc->tx_itr = IAVF_ITR_4K;
	} else
		sc->tx_itr = iavf_tx_itr;

	if (iavf_rx_itr < 0 || iavf_rx_itr > IAVF_MAX_ITR) {
		device_printf(dev, "Invalid rx_itr value of %d set!\n",
		    iavf_rx_itr);
		device_printf(dev, "rx_itr must be between %d and %d, "
		    "inclusive\n",
		    0, IAVF_MAX_ITR);
		device_printf(dev, "Using default value of %d instead\n",
		    IAVF_ITR_8K);
		sc->rx_itr = IAVF_ITR_8K;
	} else
		sc->rx_itr = iavf_rx_itr;
}
#endif /* _IAVF_SYSCTLS_COMMON_H_ */
