/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#ifndef	_DEV_XILINX_XLNX_PCIB_H_
#define	_DEV_XILINX_XLNX_PCIB_H_

#define	XLNX_PCIE_VSEC		0x12c
#define	XLNX_PCIE_BIR		0x130	/* Bridge Info Register */
#define	XLNX_PCIE_BSCR		0x134	/* Bridge Status and Control */
#define	XLNX_PCIE_IDR		0x138	/* Interrupt Decode Register */
#define	XLNX_PCIE_IMR		0x13C	/* Interrupt Mask Register */
#define	 IMR_LINK_DOWN		(1 << 0)
#define	 IMR_HOT_RESET		(1 << 3)
#define	 IMR_CFG_COMPL_STATUS_S	5
#define	 IMR_CFG_COMPL_STATUS_M	(0x7 << IMR_CFG_COMPL_STATUS_S)
#define	 IMR_CFG_TIMEOUT	(1 << 8)
#define	 IMR_CORRECTABLE	(1 << 9)
#define	 IMR_NON_FATAL		(1 << 10)
#define	 IMR_FATAL		(1 << 11)
#define	 IMR_INTX		(1 << 16) /* INTx Interrupt Received */
#define	 IMR_MSI		(1 << 17) /* MSI Interrupt Received */
#define	 IMR_SLAVE_UNSUPP_REQ	(1 << 20)
#define	 IMR_SLAVE_UNEXP_COMPL	(1 << 21)
#define	 IMR_SLAVE_COMPL_TIMOUT	(1 << 22)
#define	 IMR_SLAVE_ERROR_POISON	(1 << 23)
#define	 IMR_SLAVE_COMPL_ABORT	(1 << 24)
#define	 IMR_SLAVE_ILLEG_BURST	(1 << 25)
#define	 IMR_MASTER_DECERR	(1 << 26)
#define	 IMR_MASTER_SLVERR	(1 << 27)
#define	XLNX_PCIE_BLR		0x140	/* Bus Location Register */
#define	XLNX_PCIE_PHYSCR	0x144	/* PHY Status/Control Register */
#define	 PHYSCR_LINK_UP		(1 << 11)	/* Current PHY Link-up state */
#define	XLNX_PCIE_RPSCR		0x148	/* Root Port Status/Control Register */
#define	 RPSCR_BE		(1 << 0)	/* Bridge Enable */
#define	XLNX_PCIE_RPMSIBR1	0x14C	/* Root Port MSI Base Register 1 */
#define	XLNX_PCIE_RPMSIBR2	0x150	/* Root Port MSI Base Register 2 */
#define	XLNX_PCIE_RPERRFRR	0x154	/* Root Port Error FIFO Read */
#define	 RPERRFRR_VALID		(1 << 18) /* Indicates whether read succeeded.*/
#define	 RPERRFRR_REQ_ID_S	0	/* Requester of the error message. */
#define	 RPERRFRR_REQ_ID_M	(0xffff << RPERRFRR_REQ_ID_S)
#define	XLNX_PCIE_RPIFRR1	0x158	/* Root Port Interrupt FIFO Read 1 */
#define	XLNX_PCIE_RPIFRR2	0x15C	/* Root Port Interrupt FIFO Read 2 */
#define	XLNX_PCIE_RPID2		0x160	/* Root Port Interrupt Decode 2 */
#define	XLNX_PCIE_RPID2_MASK	0x164	/* Root Port Interrupt Decode 2 Mask */
#define	XLNX_PCIE_RPMSIID1	0x170	/* Root Port MSI Interrupt Decode 1 */
#define	XLNX_PCIE_RPMSIID2	0x174	/* Root Port MSI Interrupt Decode 2 */
#define	XLNX_PCIE_RPMSIID1_MASK	0x178	/* Root Port MSI Int. Decode 1 Mask */
#define	XLNX_PCIE_RPMSIID2_MASK	0x17C	/* Root Port MSI Int. Decode 2 Mask */
#define	XLNX_PCIE_CCR		0x168	/* Configuration Control Register */
#define	XLNX_PCIE_VSEC_CR	0x200	/* VSEC Capability Register 2 */
#define	XLNX_PCIE_VSEC_HR	0x204	/* VSEC Header Register 2 */

#endif /* !_DEV_XILINX_XLNX_PCIB_H_ */
