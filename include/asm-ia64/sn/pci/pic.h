/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_SN_PCI_PIC_H
#define _ASM_SN_PCI_PIC_H


/*
 * The PIC ASIC is a follow-on to the Bridge and Xbridge ASICs.
 * It shares many of the same registers as those chips and therefore
 * the primary structure for the PIC will be bridge_s as defined
 * in irix/kern/sys/PCI/bridge.h.   This file is intended as a complement
 * to bridge.h, which includes this file.  
 */

/*
 * PIC AS DEVICE ZERO
 * ------------------
 *
 * PIC handles PCI/X busses.  PCI/X requires that the 'bridge' (i.e. PIC)
 * be designated as 'device 0'.   That is a departure from earlier SGI
 * PCI bridges.  Because of that we use config space 1 to access the
 * config space of the first actual PCI device on the bus. 
 * Here's what the PIC manual says:
 *
 *     The current PCI-X bus specification now defines that the parent
 *     hosts bus bridge (PIC for example) must be device 0 on bus 0. PIC
 *     reduced the total number of devices from 8 to 4 and removed the
 *     device registers and windows, now only supporting devices 0,1,2, and
 *     3. PIC did leave all 8 configuration space windows. The reason was
 *     there was nothing to gain by removing them. Here in lies the problem.
 *     The device numbering we do using 0 through 3 is unrelated to the device
 *     numbering which PCI-X requires in configuration space. In the past we
 *     correlated Configs pace and our device space 0 <-> 0, 1 <-> 1, etc.
 *     PCI-X requires we start a 1, not 0 and currently the PX brick
 *     does associate our:
 * 
 *         device 0 with configuration space window 1,
 *         device 1 with configuration space window 2, 
 *         device 2 with configuration space window 3,
 *         device 3 with configuration space window 4.
 *
 * The net effect is that all config space access are off-by-one with 
 * relation to other per-slot accesses on the PIC.   
 * Here is a table that shows some of that:
 *
 *                               Internal Slot#
 *           |
 *           |     0         1        2         3
 * ----------|---------------------------------------
 * config    |  0x21000   0x22000  0x23000   0x24000
 *           |
 * even rrb  |  0[0]      n/a      1[0]      n/a	[] == implied even/odd
 *           |
 * odd rrb   |  n/a       0[1]     n/a       1[1]
 *           |
 * int dev   |  00       01        10        11
 *           |
 * ext slot# |  1        2         3         4
 * ----------|---------------------------------------
 */


#ifndef __ASSEMBLY__

#ifdef __cplusplus
extern "C" {
#endif


/*********************************************************************
 *    bus provider function table
 *
 *	Normally, this table is only handed off explicitly
 *	during provider initialization, and the PCI generic
 *	layer will stash a pointer to it in the vertex; however,
 *	exporting it explicitly enables a performance hack in
 *	the generic PCI provider where if we know at compile
 *	time that the only possible PCI provider is a
 *	pcibr, we can go directly to this ops table.
 */

#ifdef __KERNEL__
#include <linux/config.h>
#include <asm/sn/pci/pciio.h>
extern pciio_provider_t pci_pic_provider;
#else
#include <linux/config.h>
#endif


/*********************************************************************
 * misc defines
 *
 */
#define PIC_WIDGET_PART_NUM_BUS0 0xd102
#define PIC_WIDGET_PART_NUM_BUS1 0xd112
#define PIC_WIDGET_MFGR_NUM 0x24
#define PIC_WIDGET_REV_A  0x1

#define IS_PIC_PART_REV_A(rev) \
	((rev == (PIC_WIDGET_PART_NUM_BUS0 << 4 | PIC_WIDGET_REV_A)) || \
	(rev == (PIC_WIDGET_PART_NUM_BUS1 << 4 | PIC_WIDGET_REV_A)))

/*********************************************************************
 * register offset defines
 *
 */
	/* Identification Register  -- read-only */
#define PIC_IDENTIFICATION 0x00000000

	/* Status Register  -- read-only */
#define PIC_STATUS 0x00000008

	/* Upper Address Holding Register Bus Side Errors  -- read-only */
#define PIC_UPPER_ADDR_REG_BUS_SIDE_ERRS 0x00000010

	/* Lower Address Holding Register Bus Side Errors  -- read-only */
#define PIC_LOWER_ADDR_REG_BUS_SIDE_ERRS 0x00000018

	/* Control Register  -- read/write */
#define PIC_CONTROL 0x00000020

	/* PCI Request Time-out Value Register  -- read/write */
#define PIC_PCI_REQ_TIME_OUT_VALUE 0x00000028

	/* Interrupt Destination Upper Address Register  -- read/write */
#define PIC_INTR_DEST_UPPER_ADDR 0x00000030

	/* Interrupt Destination Lower Address Register  -- read/write */
#define PIC_INTR_DEST_LOWER_ADDR 0x00000038

	/* Command Word Holding Register Bus Side  -- read-only */
#define PIC_CMD_WORD_REG_BUS_SIDE 0x00000040

	/* LLP Configuration Register (Bus 0 Only)  -- read/write */
#define PIC_LLP_CFG_REG_(BUS_0_ONLY) 0x00000048

	/* PCI Target Flush Register  -- read-only */
#define PIC_PCI_TARGET_FLUSH 0x00000050

	/* Command Word Holding Register Link Side  -- read-only */
#define PIC_CMD_WORD_REG_LINK_SIDE 0x00000058

	/* Response Buffer Error Upper Address Holding  -- read-only */
#define PIC_RESP_BUF_ERR_UPPER_ADDR_ 0x00000060

	/* Response Buffer Error Lower Address Holding  -- read-only */
#define PIC_RESP_BUF_ERR_LOWER_ADDR_ 0x00000068

	/* Test Pin Control Register  -- read/write */
#define PIC_TEST_PIN_CONTROL 0x00000070

	/* Address Holding Register Link Side Errors  -- read-only */
#define PIC_ADDR_REG_LINK_SIDE_ERRS 0x00000078

	/* Direct Map Register  -- read/write */
#define PIC_DIRECT_MAP 0x00000080

	/* PCI Map Fault Address Register  -- read-only */
#define PIC_PCI_MAP_FAULT_ADDR 0x00000090

	/* Arbitration Priority Register  -- read/write */
#define PIC_ARBITRATION_PRIORITY 0x000000A0

	/* Internal Ram Parity Error Register  -- read-only */
#define PIC_INTERNAL_RAM_PARITY_ERR 0x000000B0

	/* PCI Time-out Register  -- read/write */
#define PIC_PCI_TIME_OUT 0x000000C0

	/* PCI Type 1 Configuration Register  -- read/write */
#define PIC_PCI_TYPE_1_CFG 0x000000C8

	/* PCI Bus Error Upper Address Holding Register  -- read-only */
#define PIC_PCI_BUS_ERR_UPPER_ADDR_ 0x000000D0

	/* PCI Bus Error Lower Address Holding Register  -- read-only */
#define PIC_PCI_BUS_ERR_LOWER_ADDR_ 0x000000D8

	/* PCIX Error Address Register  -- read-only */
#define PIC_PCIX_ERR_ADDR 0x000000E0

	/* PCIX Error Attribute Register  -- read-only */
#define PIC_PCIX_ERR_ATTRIBUTE 0x000000E8

	/* PCIX Error Data Register  -- read-only */
#define PIC_PCIX_ERR_DATA 0x000000F0

	/* PCIX Read Request Timeout Error Register  -- read-only */
#define PIC_PCIX_READ_REQ_TIMEOUT_ERR 0x000000F8

	/* Interrupt Status Register  -- read-only */
#define PIC_INTR_STATUS 0x00000100

	/* Interrupt Enable Register  -- read/write */
#define PIC_INTR_ENABLE 0x00000108

	/* Reset Interrupt Status Register  -- write-only */
#define PIC_RESET_INTR_STATUS 0x00000110

	/* Interrupt Mode Register  -- read/write */
#define PIC_INTR_MODE 0x00000118

	/* Interrupt Device Register  -- read/write */
#define PIC_INTR_DEVICE 0x00000120

	/* Host Error Field Register  -- read/write */
#define PIC_HOST_ERR_FIELD 0x00000128

	/* Interrupt Pin 0 Host Address Register  -- read/write */
#define PIC_INTR_PIN_0_HOST_ADDR 0x00000130

	/* Interrupt Pin 1 Host Address Register  -- read/write */
#define PIC_INTR_PIN_1_HOST_ADDR 0x00000138

	/* Interrupt Pin 2 Host Address Register  -- read/write */
#define PIC_INTR_PIN_2_HOST_ADDR 0x00000140

	/* Interrupt Pin 3 Host Address Register  -- read/write */
#define PIC_INTR_PIN_3_HOST_ADDR 0x00000148

	/* Interrupt Pin 4 Host Address Register  -- read/write */
#define PIC_INTR_PIN_4_HOST_ADDR 0x00000150

	/* Interrupt Pin 5 Host Address Register  -- read/write */
#define PIC_INTR_PIN_5_HOST_ADDR 0x00000158

	/* Interrupt Pin 6 Host Address Register  -- read/write */
#define PIC_INTR_PIN_6_HOST_ADDR 0x00000160

	/* Interrupt Pin 7 Host Address Register  -- read/write */
#define PIC_INTR_PIN_7_HOST_ADDR 0x00000168

	/* Error Interrupt View Register  -- read-only */
#define PIC_ERR_INTR_VIEW 0x00000170

	/* Multiple Interrupt Register  -- read-only */
#define PIC_MULTIPLE_INTR 0x00000178

	/* Force Always Interrupt 0 Register  -- write-only */
#define PIC_FORCE_ALWAYS_INTR_0 0x00000180

	/* Force Always Interrupt 1 Register  -- write-only */
#define PIC_FORCE_ALWAYS_INTR_1 0x00000188

	/* Force Always Interrupt 2 Register  -- write-only */
#define PIC_FORCE_ALWAYS_INTR_2 0x00000190

	/* Force Always Interrupt 3 Register  -- write-only */
#define PIC_FORCE_ALWAYS_INTR_3 0x00000198

	/* Force Always Interrupt 4 Register  -- write-only */
#define PIC_FORCE_ALWAYS_INTR_4 0x000001A0

	/* Force Always Interrupt 5 Register  -- write-only */
#define PIC_FORCE_ALWAYS_INTR_5 0x000001A8

	/* Force Always Interrupt 6 Register  -- write-only */
#define PIC_FORCE_ALWAYS_INTR_6 0x000001B0

	/* Force Always Interrupt 7 Register  -- write-only */
#define PIC_FORCE_ALWAYS_INTR_7 0x000001B8

	/* Force w/Pin Interrupt 0 Register  -- write-only */
#define PIC_FORCE_PIN_INTR_0 0x000001C0

	/* Force w/Pin Interrupt 1 Register  -- write-only */
#define PIC_FORCE_PIN_INTR_1 0x000001C8

	/* Force w/Pin Interrupt 2 Register  -- write-only */
#define PIC_FORCE_PIN_INTR_2 0x000001D0

	/* Force w/Pin Interrupt 3 Register  -- write-only */
#define PIC_FORCE_PIN_INTR_3 0x000001D8

	/* Force w/Pin Interrupt 4 Register  -- write-only */
#define PIC_FORCE_PIN_INTR_4 0x000001E0

	/* Force w/Pin Interrupt 5 Register  -- write-only */
#define PIC_FORCE_PIN_INTR_5 0x000001E8

	/* Force w/Pin Interrupt 6 Register  -- write-only */
#define PIC_FORCE_PIN_INTR_6 0x000001F0

	/* Force w/Pin Interrupt 7 Register  -- write-only */
#define PIC_FORCE_PIN_INTR_7 0x000001F8

	/* Device 0 Register  -- read/write */
#define PIC_DEVICE_0 0x00000200

	/* Device 1 Register  -- read/write */
#define PIC_DEVICE_1 0x00000208

	/* Device 2 Register  -- read/write */
#define PIC_DEVICE_2 0x00000210

	/* Device 3 Register  -- read/write */
#define PIC_DEVICE_3 0x00000218

	/* Device 0 Write Request Buffer Register  -- read-only */
#define PIC_DEVICE_0_WRITE_REQ_BUF 0x00000240

	/* Device 1 Write Request Buffer Register  -- read-only */
#define PIC_DEVICE_1_WRITE_REQ_BUF 0x00000248

	/* Device 2 Write Request Buffer Register  -- read-only */
#define PIC_DEVICE_2_WRITE_REQ_BUF 0x00000250

	/* Device 3 Write Request Buffer Register  -- read-only */
#define PIC_DEVICE_3_WRITE_REQ_BUF 0x00000258

	/* Even Device Response Buffer Register  -- read/write */
#define PIC_EVEN_DEVICE_RESP_BUF 0x00000280

	/* Odd Device Response Buffer Register  -- read/write */
#define PIC_ODD_DEVICE_RESP_BUF 0x00000288

	/* Read Response Buffer Status Register  -- read-only */
#define PIC_READ_RESP_BUF_STATUS 0x00000290

	/* Read Response Buffer Clear Register  -- write-only */
#define PIC_READ_RESP_BUF_CLEAR 0x00000298

	/* PCI RR 0 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_0_UPPER_ADDR_MATCH 0x00000300

	/* PCI RR 0 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_0_LOWER_ADDR_MATCH 0x00000308

	/* PCI RR 1 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_1_UPPER_ADDR_MATCH 0x00000310

	/* PCI RR 1 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_1_LOWER_ADDR_MATCH 0x00000318

	/* PCI RR 2 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_2_UPPER_ADDR_MATCH 0x00000320

	/* PCI RR 2 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_2_LOWER_ADDR_MATCH 0x00000328

	/* PCI RR 3 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_3_UPPER_ADDR_MATCH 0x00000330

	/* PCI RR 3 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_3_LOWER_ADDR_MATCH 0x00000338

	/* PCI RR 4 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_4_UPPER_ADDR_MATCH 0x00000340

	/* PCI RR 4 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_4_LOWER_ADDR_MATCH 0x00000348

	/* PCI RR 5 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_5_UPPER_ADDR_MATCH 0x00000350

	/* PCI RR 5 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_5_LOWER_ADDR_MATCH 0x00000358

	/* PCI RR 6 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_6_UPPER_ADDR_MATCH 0x00000360

	/* PCI RR 6 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_6_LOWER_ADDR_MATCH 0x00000368

	/* PCI RR 7 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_7_UPPER_ADDR_MATCH 0x00000370

	/* PCI RR 7 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_7_LOWER_ADDR_MATCH 0x00000378

	/* PCI RR 8 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_8_UPPER_ADDR_MATCH 0x00000380

	/* PCI RR 8 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_8_LOWER_ADDR_MATCH 0x00000388

	/* PCI RR 9 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_9_UPPER_ADDR_MATCH 0x00000390

	/* PCI RR 9 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_9_LOWER_ADDR_MATCH 0x00000398

	/* PCI RR 10 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_10_UPPER_ADDR_MATCH 0x000003A0

	/* PCI RR 10 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_10_LOWER_ADDR_MATCH 0x000003A8

	/* PCI RR 11 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_11_UPPER_ADDR_MATCH 0x000003B0

	/* PCI RR 11 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_11_LOWER_ADDR_MATCH 0x000003B8

	/* PCI RR 12 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_12_UPPER_ADDR_MATCH 0x000003C0

	/* PCI RR 12 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_12_LOWER_ADDR_MATCH 0x000003C8

	/* PCI RR 13 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_13_UPPER_ADDR_MATCH 0x000003D0

	/* PCI RR 13 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_13_LOWER_ADDR_MATCH 0x000003D8

	/* PCI RR 14 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_14_UPPER_ADDR_MATCH 0x000003E0

	/* PCI RR 14 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_14_LOWER_ADDR_MATCH 0x000003E8

	/* PCI RR 15 Upper Address Match Register  -- read-only */
#define PIC_PCI_RR_15_UPPER_ADDR_MATCH 0x000003F0

	/* PCI RR 15 Lower Address Match Register  -- read-only */
#define PIC_PCI_RR_15_LOWER_ADDR_MATCH 0x000003F8

	/* Buffer 0 Flush Count with Data Touch Register  -- read/write */
#define PIC_BUF_0_FLUSH_CNT_WITH_DATA_TOUCH 0x00000400

	/* Buffer 0 Flush Count w/o Data Touch Register  -- read/write */
#define PIC_BUF_0_FLUSH_CNT_W_O_DATA_TOUCH 0x00000408

	/* Buffer 0 Request in Flight Count Register  -- read/write */
#define PIC_BUF_0_REQ_IN_FLIGHT_CNT 0x00000410

	/* Buffer 0 Prefetch Request Count Register  -- read/write */
#define PIC_BUF_0_PREFETCH_REQ_CNT 0x00000418

	/* Buffer 0 Total PCI Retry Count Register  -- read/write */
#define PIC_BUF_0_TOTAL_PCI_RETRY_CNT 0x00000420

	/* Buffer 0 Max PCI Retry Count Register  -- read/write */
#define PIC_BUF_0_MAX_PCI_RETRY_CNT 0x00000428

	/* Buffer 0 Max Latency Count Register  -- read/write */
#define PIC_BUF_0_MAX_LATENCY_CNT 0x00000430

	/* Buffer 0 Clear All Register  -- read/write */
#define PIC_BUF_0_CLEAR_ALL 0x00000438

	/* Buffer 2 Flush Count with Data Touch Register  -- read/write */
#define PIC_BUF_2_FLUSH_CNT_WITH_DATA_TOUCH 0x00000440

	/* Buffer 2 Flush Count w/o Data Touch Register  -- read/write */
#define PIC_BUF_2_FLUSH_CNT_W_O_DATA_TOUCH 0x00000448

	/* Buffer 2 Request in Flight Count Register  -- read/write */
#define PIC_BUF_2_REQ_IN_FLIGHT_CNT 0x00000450

	/* Buffer 2 Prefetch Request Count Register  -- read/write */
#define PIC_BUF_2_PREFETCH_REQ_CNT 0x00000458

	/* Buffer 2 Total PCI Retry Count Register  -- read/write */
#define PIC_BUF_2_TOTAL_PCI_RETRY_CNT 0x00000460

	/* Buffer 2 Max PCI Retry Count Register  -- read/write */
#define PIC_BUF_2_MAX_PCI_RETRY_CNT 0x00000468

	/* Buffer 2 Max Latency Count Register  -- read/write */
#define PIC_BUF_2_MAX_LATENCY_CNT 0x00000470

	/* Buffer 2 Clear All Register  -- read/write */
#define PIC_BUF_2_CLEAR_ALL 0x00000478

	/* Buffer 4 Flush Count with Data Touch Register  -- read/write */
#define PIC_BUF_4_FLUSH_CNT_WITH_DATA_TOUCH 0x00000480

	/* Buffer 4 Flush Count w/o Data Touch Register  -- read/write */
#define PIC_BUF_4_FLUSH_CNT_W_O_DATA_TOUCH 0x00000488

	/* Buffer 4 Request in Flight Count Register  -- read/write */
#define PIC_BUF_4_REQ_IN_FLIGHT_CNT 0x00000490

	/* Buffer 4 Prefetch Request Count Register  -- read/write */
#define PIC_BUF_4_PREFETCH_REQ_CNT 0x00000498

	/* Buffer 4 Total PCI Retry Count Register  -- read/write */
#define PIC_BUF_4_TOTAL_PCI_RETRY_CNT 0x000004A0

	/* Buffer 4 Max PCI Retry Count Register  -- read/write */
#define PIC_BUF_4_MAX_PCI_RETRY_CNT 0x000004A8

	/* Buffer 4 Max Latency Count Register  -- read/write */
#define PIC_BUF_4_MAX_LATENCY_CNT 0x000004B0

	/* Buffer 4 Clear All Register  -- read/write */
#define PIC_BUF_4_CLEAR_ALL 0x000004B8

	/* Buffer 6 Flush Count with Data Touch Register  -- read/write */
#define PIC_BUF_6_FLUSH_CNT_WITH_DATA_TOUCH 0x000004C0

	/* Buffer 6 Flush Count w/o Data Touch Register  -- read/write */
#define PIC_BUF_6_FLUSH_CNT_W_O_DATA_TOUCH 0x000004C8

	/* Buffer 6 Request in Flight Count Register  -- read/write */
#define PIC_BUF_6_REQ_IN_FLIGHT_CNT 0x000004D0

	/* Buffer 6 Prefetch Request Count Register  -- read/write */
#define PIC_BUF_6_PREFETCH_REQ_CNT 0x000004D8

	/* Buffer 6 Total PCI Retry Count Register  -- read/write */
#define PIC_BUF_6_TOTAL_PCI_RETRY_CNT 0x000004E0

	/* Buffer 6 Max PCI Retry Count Register  -- read/write */
#define PIC_BUF_6_MAX_PCI_RETRY_CNT 0x000004E8

	/* Buffer 6 Max Latency Count Register  -- read/write */
#define PIC_BUF_6_MAX_LATENCY_CNT 0x000004F0

	/* Buffer 6 Clear All Register  -- read/write */
#define PIC_BUF_6_CLEAR_ALL 0x000004F8

	/* Buffer 8 Flush Count with Data Touch Register  -- read/write */
#define PIC_BUF_8_FLUSH_CNT_WITH_DATA_TOUCH 0x00000500

	/* Buffer 8 Flush Count w/o Data Touch Register  -- read/write */
#define PIC_BUF_8_FLUSH_CNT_W_O_DATA_TOUCH 0x00000508

	/* Buffer 8 Request in Flight Count Register  -- read/write */
#define PIC_BUF_8_REQ_IN_FLIGHT_CNT 0x00000510

	/* Buffer 8 Prefetch Request Count Register  -- read/write */
#define PIC_BUF_8_PREFETCH_REQ_CNT 0x00000518

	/* Buffer 8 Total PCI Retry Count Register  -- read/write */
#define PIC_BUF_8_TOTAL_PCI_RETRY_CNT 0x00000520

	/* Buffer 8 Max PCI Retry Count Register  -- read/write */
#define PIC_BUF_8_MAX_PCI_RETRY_CNT 0x00000528

	/* Buffer 8 Max Latency Count Register  -- read/write */
#define PIC_BUF_8_MAX_LATENCY_CNT 0x00000530

	/* Buffer 8 Clear All Register  -- read/write */
#define PIC_BUF_8_CLEAR_ALL 0x00000538

	/* Buffer 10 Flush Count with Data Touch Register  -- read/write */
#define PIC_BUF_10_FLUSH_CNT_WITH_DATA_TOUCH 0x00000540

	/* Buffer 10 Flush Count w/o Data Touch Register  -- read/write */
#define PIC_BUF_10_FLUSH_CNT_W_O_DATA_TOUCH 0x00000548

	/* Buffer 10 Request in Flight Count Register  -- read/write */
#define PIC_BUF_10_REQ_IN_FLIGHT_CNT 0x00000550

	/* Buffer 10 Prefetch Request Count Register  -- read/write */
#define PIC_BUF_10_PREFETCH_REQ_CNT 0x00000558

	/* Buffer 10 Total PCI Retry Count Register  -- read/write */
#define PIC_BUF_10_TOTAL_PCI_RETRY_CNT 0x00000560

	/* Buffer 10 Max PCI Retry Count Register  -- read/write */
#define PIC_BUF_10_MAX_PCI_RETRY_CNT 0x00000568

	/* Buffer 10 Max Latency Count Register  -- read/write */
#define PIC_BUF_10_MAX_LATENCY_CNT 0x00000570

	/* Buffer 10 Clear All Register  -- read/write */
#define PIC_BUF_10_CLEAR_ALL 0x00000578

	/* Buffer 12 Flush Count with Data Touch Register  -- read/write */
#define PIC_BUF_12_FLUSH_CNT_WITH_DATA_TOUCH 0x00000580

	/* Buffer 12 Flush Count w/o Data Touch Register  -- read/write */
#define PIC_BUF_12_FLUSH_CNT_W_O_DATA_TOUCH 0x00000588

	/* Buffer 12 Request in Flight Count Register  -- read/write */
#define PIC_BUF_12_REQ_IN_FLIGHT_CNT 0x00000590

	/* Buffer 12 Prefetch Request Count Register  -- read/write */
#define PIC_BUF_12_PREFETCH_REQ_CNT 0x00000598

	/* Buffer 12 Total PCI Retry Count Register  -- read/write */
#define PIC_BUF_12_TOTAL_PCI_RETRY_CNT 0x000005A0

	/* Buffer 12 Max PCI Retry Count Register  -- read/write */
#define PIC_BUF_12_MAX_PCI_RETRY_CNT 0x000005A8

	/* Buffer 12 Max Latency Count Register  -- read/write */
#define PIC_BUF_12_MAX_LATENCY_CNT 0x000005B0

	/* Buffer 12 Clear All Register  -- read/write */
#define PIC_BUF_12_CLEAR_ALL 0x000005B8

	/* Buffer 14 Flush Count with Data Touch Register  -- read/write */
#define PIC_BUF_14_FLUSH_CNT_WITH_DATA_TOUCH 0x000005C0

	/* Buffer 14 Flush Count w/o Data Touch Register  -- read/write */
#define PIC_BUF_14_FLUSH_CNT_W_O_DATA_TOUCH 0x000005C8

	/* Buffer 14 Request in Flight Count Register  -- read/write */
#define PIC_BUF_14_REQ_IN_FLIGHT_CNT 0x000005D0

	/* Buffer 14 Prefetch Request Count Register  -- read/write */
#define PIC_BUF_14_PREFETCH_REQ_CNT 0x000005D8

	/* Buffer 14 Total PCI Retry Count Register  -- read/write */
#define PIC_BUF_14_TOTAL_PCI_RETRY_CNT 0x000005E0

	/* Buffer 14 Max PCI Retry Count Register  -- read/write */
#define PIC_BUF_14_MAX_PCI_RETRY_CNT 0x000005E8

	/* Buffer 14 Max Latency Count Register  -- read/write */
#define PIC_BUF_14_MAX_LATENCY_CNT 0x000005F0

	/* Buffer 14 Clear All Register  -- read/write */
#define PIC_BUF_14_CLEAR_ALL 0x000005F8

	/* PCIX Read Buffer 0 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_0_ADDR 0x00000A00

	/* PCIX Read Buffer 0 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_0_ATTRIBUTE 0x00000A08

	/* PCIX Read Buffer 1 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_1_ADDR 0x00000A10

	/* PCIX Read Buffer 1 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_1_ATTRIBUTE 0x00000A18

	/* PCIX Read Buffer 2 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_2_ADDR 0x00000A20

	/* PCIX Read Buffer 2 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_2_ATTRIBUTE 0x00000A28

	/* PCIX Read Buffer 3 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_3_ADDR 0x00000A30

	/* PCIX Read Buffer 3 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_3_ATTRIBUTE 0x00000A38

	/* PCIX Read Buffer 4 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_4_ADDR 0x00000A40

	/* PCIX Read Buffer 4 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_4_ATTRIBUTE 0x00000A48

	/* PCIX Read Buffer 5 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_5_ADDR 0x00000A50

	/* PCIX Read Buffer 5 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_5_ATTRIBUTE 0x00000A58

	/* PCIX Read Buffer 6 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_6_ADDR 0x00000A60

	/* PCIX Read Buffer 6 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_6_ATTRIBUTE 0x00000A68

	/* PCIX Read Buffer 7 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_7_ADDR 0x00000A70

	/* PCIX Read Buffer 7 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_7_ATTRIBUTE 0x00000A78

	/* PCIX Read Buffer 8 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_8_ADDR 0x00000A80

	/* PCIX Read Buffer 8 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_8_ATTRIBUTE 0x00000A88

	/* PCIX Read Buffer 9 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_9_ADDR 0x00000A90

	/* PCIX Read Buffer 9 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_9_ATTRIBUTE 0x00000A98

	/* PCIX Read Buffer 10 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_10_ADDR 0x00000AA0

	/* PCIX Read Buffer 10 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_10_ATTRIBUTE 0x00000AA8

	/* PCIX Read Buffer 11 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_11_ADDR 0x00000AB0

	/* PCIX Read Buffer 11 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_11_ATTRIBUTE 0x00000AB8

	/* PCIX Read Buffer 12 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_12_ADDR 0x00000AC0

	/* PCIX Read Buffer 12 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_12_ATTRIBUTE 0x00000AC8

	/* PCIX Read Buffer 13 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_13_ADDR 0x00000AD0

	/* PCIX Read Buffer 13 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_13_ATTRIBUTE 0x00000AD8

	/* PCIX Read Buffer 14 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_14_ADDR 0x00000AE0

	/* PCIX Read Buffer 14 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_14_ATTRIBUTE 0x00000AE8

	/* PCIX Read Buffer 15 Address Register  -- read-only */
#define PIC_PCIX_READ_BUF_15_ADDR 0x00000AF0

	/* PCIX Read Buffer 15 Attribute Register  -- read-only */
#define PIC_PCIX_READ_BUF_15_ATTRIBUTE 0x00000AF8

	/* PCIX Write Buffer 0 Address Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_0_ADDR 0x00000B00

	/* PCIX Write Buffer 0 Attribute Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_0_ATTRIBUTE 0x00000B08

	/* PCIX Write Buffer 0 Valid Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_0_VALID 0x00000B10

	/* PCIX Write Buffer 1 Address Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_1_ADDR 0x00000B20

	/* PCIX Write Buffer 1 Attribute Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_1_ATTRIBUTE 0x00000B28

	/* PCIX Write Buffer 1 Valid Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_1_VALID 0x00000B30

	/* PCIX Write Buffer 2 Address Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_2_ADDR 0x00000B40

	/* PCIX Write Buffer 2 Attribute Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_2_ATTRIBUTE 0x00000B48

	/* PCIX Write Buffer 2 Valid Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_2_VALID 0x00000B50

	/* PCIX Write Buffer 3 Address Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_3_ADDR 0x00000B60

	/* PCIX Write Buffer 3 Attribute Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_3_ATTRIBUTE 0x00000B68

	/* PCIX Write Buffer 3 Valid Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_3_VALID 0x00000B70

	/* PCIX Write Buffer 4 Address Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_4_ADDR 0x00000B80

	/* PCIX Write Buffer 4 Attribute Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_4_ATTRIBUTE 0x00000B88

	/* PCIX Write Buffer 4 Valid Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_4_VALID 0x00000B90

	/* PCIX Write Buffer 5 Address Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_5_ADDR 0x00000BA0

	/* PCIX Write Buffer 5 Attribute Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_5_ATTRIBUTE 0x00000BA8

	/* PCIX Write Buffer 5 Valid Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_5_VALID 0x00000BB0

	/* PCIX Write Buffer 6 Address Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_6_ADDR 0x00000BC0

	/* PCIX Write Buffer 6 Attribute Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_6_ATTRIBUTE 0x00000BC8

	/* PCIX Write Buffer 6 Valid Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_6_VALID 0x00000BD0

	/* PCIX Write Buffer 7 Address Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_7_ADDR 0x00000BE0

	/* PCIX Write Buffer 7 Attribute Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_7_ATTRIBUTE 0x00000BE8

	/* PCIX Write Buffer 7 Valid Register  -- read-only */
#define PIC_PCIX_WRITE_BUF_7_VALID 0x00000BF0

/*********************************************************************
 * misc typedefs
 *
 */
typedef uint64_t picreg_t;

/*********************************************************************
 * PIC register structures
 *
 */

/*
 * Identification Register
 *
 * The Identification register is a read only register used by the host CPU
 * during configuration to determine the type of the widget. The format is
 * the same as defined in IEEE 1149.1 JTAG Device Identification Register.
 */
	typedef union pic_id_reg_u {
		picreg_t	pic_id_reg_regval;
		struct {
			picreg_t          :	32; /* 63:32 */
			picreg_t rev_num  :	4; /* 31:28 */
			picreg_t part_num :	16; /* 27:12 */
			picreg_t mfg_num  :	11; /* 11:1 */
			picreg_t          :	1; /* 0:0 */
		} pic_id_reg_fld_s;
	} pic_id_reg_u_t;
/*
 * Status Register
 *
 * The status register is a read register which holds status information of the
 * Bus Subsection.
 */
	typedef union pic_stat_reg_u {
		picreg_t	pic_stat_reg_regval;
		struct {
			picreg_t                :	28; /* 63:36 */
			picreg_t pci_x_speed    :	2; /* 35:34 */
			picreg_t pci_x_active   :	1; /* 33:33 */
			picreg_t                :	1; /* 32:32 */
			picreg_t llp_rec_cnt    :	8; /* 31:24 */
			picreg_t llp_tx_cnt     :	8; /* 23:16 */
			picreg_t rx_credit_cnt  :	4; /* 15:12 */
			picreg_t tx_credit_cnt  :	4; /* 11:8 */
			picreg_t pci_misc_input :	8; /* 7:0 */
		} pic_stat_reg_fld_s;
	} pic_stat_reg_u_t;
/*
 * Upper Address Holding Register Bus Side Errors
 *
 * The upper address holding register is a read only register which contains
 * the upper 16-bits of the address when certain error occurs (see error cases
 * chapter). Subsequent errors are not logged until the error is cleared. The
 * last logged value is held until the group is cleared and enabled.
 */
	typedef union pic_upper_bus_err_u {
		picreg_t	pic_upper_bus_err_regval;
		struct {
			picreg_t          :	32; /* 63:32 */
			picreg_t          :	16; /* 31:16 */
			picreg_t upp_addr :	16; /* 15:0 */
		} pic_upper_bus_err_fld_s;
	} pic_upper_bus_err_u_t;
/*
 * Lower Address Holding Register Bus Side Errors
 *
 * The lower address holding register is a read only register which contains
 * the address which either can be accessed as a word or double word. Sub-
 * sequent errors are not logged until the error is cleared. The last logged
 * value is held until the group is cleared and enabled.
 */
	typedef union pic_lower_bus_err_u {
		picreg_t	pic_lower_bus_err_regval;
		struct {
			picreg_t          :	16; /* 63:48 */
			picreg_t upp_addr :	16; /* 47:32 */
			picreg_t low_addr :	32; /* 31:0 */
		} pic_lower_bus_err_fld_s;
	} pic_lower_bus_err_u_t;
/*
 * Control Register
 *
 * The control register is a read/write register which holds control informa-
 * tion for the bus subsection.
 */
	typedef union pic_control_reg_u {
		picreg_t	pic_control_reg_regval;
		struct {
			picreg_t                :	32; /* 63:32 */
			picreg_t                :	4; /* 31:28 */
			picreg_t rst_pin_n      :	4; /* 27:24 */
			picreg_t                :	1; /* 23:23 */
			picreg_t mem_swap       :	1; /* 22:22 */
			picreg_t page_size      :	1; /* 21:21 */
			picreg_t                :	4; /* 20:17 */
			picreg_t f_bad_pkt      :	1; /* 16:16 */
			picreg_t llp_xbar_crd   :	4; /* 15:12 */
			picreg_t clr_rllp_cnt   :	1; /* 11:11 */
			picreg_t clr_tllp_cnt   :	1; /* 10:10 */
			picreg_t sys_end        :	1; /* 9:9 */
			picreg_t                :	3; /* 8:6 */
			picreg_t pci_speed      :	2; /* 5:4 */
			picreg_t widget_id      :	4; /* 3:0 */
		} pic_control_reg_fld_s;
	} pic_control_reg_u_t;
/*
 * PCI/PCI-X Request Time-out Value Register
 *
 * This register contains the reload value for the response timer. The request
 * timer counts every 960 nS (32 PCI clocks)
 */
	typedef union pic_pci_req_to_u {
		picreg_t	pic_pci_req_to_regval;
		struct {
			picreg_t          :	32; /* 63:32 */
			picreg_t          :	12; /* 31:20 */
			picreg_t time_out :	20; /* 19:0 */
		} pic_pci_req_to_fld_s;
	} pic_pci_req_to_u_t;
/*
 * Interrupt Destination Upper Address Register
 *
 * The interrupt destination upper address register is a read/write register
 * containing the upper 16-bits of address of the host to which the interrupt
 * is targeted. In addition the target ID is also contained in this register for
 * use in Crosstalk mode.
 */
	typedef union pic_int_desc_upper_u {
		picreg_t	pic_int_desc_upper_regval;
		struct {
			picreg_t           :	32; /* 63:32 */
			picreg_t           :	12; /* 31:20 */
			picreg_t target_id :	4; /* 19:16 */
			picreg_t upp_addr  :	16; /* 15:0 */
		} pic_int_desc_upper_fld_s;
	} pic_int_desc_upper_u_t;
/*
 * Interrupt Destination Lower Address Register
 *
 * The interrupt destination lower address register is a read/write register
 * which contains the entire address of the host to which the interrupt is tar-
 * geted. In addition the target ID is also contained in this register for use in
 * Crosstalk mode.
 */
	typedef union pic_int_desc_lower_u {
		picreg_t	pic_int_desc_lower_regval;
		struct {
			picreg_t           :	12; /* 63:52 */
			picreg_t target_id :	4; /* 51:48 */
			picreg_t upp_addr  :	16; /* 47:32 */
			picreg_t low_addr  :	32; /* 31:0 */
		} pic_int_desc_lower_fld_s;
	} pic_int_desc_lower_u_t;
/*
 * Command Word Holding Register Bus Side Errors
 *
 * The command word holding is a read register that holds the command
 * word of a Crosstalk packet when errors occur on the link side (see error
 * chapter). Errors are indicated with error bits in the interrupt status regis-
 * ter. Subsequent errors are not logged until the interrupt is cleared..
 */
	typedef union pic_cmd_word_bus_err_u {
		picreg_t	pic_cmd_word_bus_err_regval;
		struct {
			picreg_t          :	32; /* 63:32 */
			picreg_t didn     :	4; /* 31:28 */
			picreg_t sidn     :	4; /* 27:24 */
			picreg_t pactyp   :	4; /* 23:20 */
			picreg_t tnum     :	5; /* 19:15 */
			picreg_t coherent :	1; /* 14:14 */
			picreg_t ds       :	2; /* 13:12 */
			picreg_t gbr      :	1; /* 11:11 */
			picreg_t vbpm     :	1; /* 10:10 */
			picreg_t error    :	1; /* 9:9 */
			picreg_t barrier  :	1; /* 8:8 */
			picreg_t          :	8; /* 7:0 */
		} pic_cmd_word_bus_err_fld_s;
	} pic_cmd_word_bus_err_u_t;
/*
 * LLP Configuration Register
 *
 * This register contains the configuration information for the LLP modules
 * and is only valid on bus 0 side.
 */
	typedef union pic_llp_cfg_u {
		picreg_t	pic_llp_cfg_regval;
		struct {
			picreg_t                 :	32; /* 63:32 */
			picreg_t                 :	6; /* 31:26 */
			picreg_t llp_maxretry    :	10; /* 25:16 */
			picreg_t llp_nulltimeout :	6; /* 15:10 */
			picreg_t llp_maxburst    :	10; /* 9:0 */
		} pic_llp_cfg_fld_s;
	} pic_llp_cfg_u_t;
/*
 * PCI/PCI-X Target Flush Register
 *
 * When read, this register will return a 0x00 after all previous transfers to
 * the PCI bus subsection have completed.
 */

/*
 * Command Word Holding Register Link Side Errors
 *
 * The command word holding is a read-only register that holds the com-
 * mand word of a Crosstalk packet when request fifo overflow or unexpect-
 * ed response errors occur. Errors are indicated with error bits in the
 * interrupt status register. Subsequent errors are not logged until this inter-
 * rupt is cleared.
 */
	typedef union pic_cmd_word_link_err_u {
		picreg_t	pic_cmd_word_link_err_regval;
		struct {
			picreg_t          :	32; /* 63:32 */
			picreg_t didn     :	4; /* 31:28 */
			picreg_t sidn     :	4; /* 27:24 */
			picreg_t pactyp   :	4; /* 23:20 */
			picreg_t tnum     :	5; /* 19:15 */
			picreg_t coherent :	1; /* 14:14 */
			picreg_t ds       :	2; /* 13:12 */
			picreg_t gbr      :	1; /* 11:11 */
			picreg_t vbpm     :	1; /* 10:10 */
			picreg_t error    :	1; /* 9:9 */
			picreg_t barrier  :	1; /* 8:8 */
			picreg_t          :	8; /* 7:0 */
		} pic_cmd_word_link_err_fld_s;
	} pic_cmd_word_link_err_u_t;
/*
 * PCI Response Buffer Error Upper Address Holding Reg
 *
 * The response buffer error upper address holding register is a read only
 * register which contains the upper 16-bits of the address when error asso-
 * ciated with response buffer entries occur. Subsequent errors are not
 * logged until the interrupt is cleared.
 */
	typedef union pic_pci_rbuf_err_upper_u {
		picreg_t	pic_pci_rbuf_err_upper_regval;
		struct {
			picreg_t          :	32; /* 63:32 */
			picreg_t          :	9; /* 31:23 */
			picreg_t dev_num  :	3; /* 22:20 */
			picreg_t buff_num :	4; /* 19:16 */
			picreg_t upp_addr :	16; /* 15:0 */
		} pic_pci_rbuf_err_upper_fld_s;
	} pic_pci_rbuf_err_upper_u_t;
/*
 * PCI Response Buffer Error Lower Address Holding Reg
 *
 * The response buffer error lower address holding register is a read only
 * register which contains the address of the error associated with response
 * buffer entries. Subsequent errors are not logged until the interrupt is
 * cleared.
 */
	typedef union pic_pci_rbuf_err_lower_u {
		picreg_t	pic_pci_rbuf_err_lower_regval;
		struct {
			picreg_t          :	9; /* 63:55 */
			picreg_t dev_num  :	3; /* 54:52 */
			picreg_t buff_num :	4; /* 51:48 */
			picreg_t upp_addr :	16; /* 47:32 */
			picreg_t low_addr :	32; /* 31:0 */
		} pic_pci_rbuf_err_lower_fld_s;
	} pic_pci_rbuf_err_lower_u_t;
/*
 * Test Pin Control Register
 *
 * This register selects the output function and value to the four test pins on
 * the PIC .
 */
	typedef union pic_test_pin_cntl_u {
		picreg_t	pic_test_pin_cntl_regval;
		struct {
			picreg_t            :	32; /* 63:32 */
			picreg_t            :	8; /* 31:24 */
			picreg_t tdata_out  :	8; /* 23:16 */
			picreg_t sel_tpin_7 :	2; /* 15:14 */
			picreg_t sel_tpin_6 :	2; /* 13:12 */
			picreg_t sel_tpin_5 :	2; /* 11:10 */
			picreg_t sel_tpin_4 :	2; /* 9:8 */
			picreg_t sel_tpin_3 :	2; /* 7:6 */
			picreg_t sel_tpin_2 :	2; /* 5:4 */
			picreg_t sel_tpin_1 :	2; /* 3:2 */
			picreg_t sel_tpin_0 :	2; /* 1:0 */
		} pic_test_pin_cntl_fld_s;
	} pic_test_pin_cntl_u_t;
/*
 * Address Holding Register Link Side Errors
 *
 * The address holding register is a read only register which contains the ad-
 * dress which either can be accessed as a word or double word. Subsequent
 * errors are not logged until the error is cleared. The last logged value is
 * held until the group is cleared and enabled.
 */
	typedef union pic_p_addr_lkerr_u {
		picreg_t	pic_p_addr_lkerr_regval;
		struct {
			picreg_t          :	16; /* 63:48 */
			picreg_t upp_addr :	16; /* 47:32 */
			picreg_t low_addr :	32; /* 31:0 */
		} pic_p_addr_lkerr_fld_s;
	} pic_p_addr_lkerr_u_t;
/*
 * PCI Direct Mapping Register
 *
 * This register is used to relocate a 2 GByte region for PCI to Crosstalk
 * transfers.
 */
	typedef union pic_p_dir_map_u {
		picreg_t	pic_p_dir_map_regval;
		struct {
			picreg_t            :	32; /* 63:32 */
			picreg_t            :	8; /* 31:24 */
			picreg_t dir_w_id   :	4; /* 23:20 */
			picreg_t            :	2; /* 19:18 */
			picreg_t dir_add512 :	1; /* 17:17 */
			picreg_t dir_off    :	17; /* 16:0 */
		} pic_p_dir_map_fld_s;
	} pic_p_dir_map_u_t;
/*
 * PCI Page Map Fault Address Register
 *
 * This register contains the address and device number when a page map
 * fault occurred.
 */
	typedef union pic_p_map_fault_u {
		picreg_t	pic_p_map_fault_regval;
		struct {
			picreg_t             :	32; /* 63:32 */
			picreg_t             :	10; /* 31:22 */
			picreg_t pci_addr    :	18; /* 21:4 */
			picreg_t             :	1; /* 3:3 */
			picreg_t pci_dev_num :	3; /* 2:0 */
		} pic_p_map_fault_fld_s;
	} pic_p_map_fault_u_t;
/*
 * Arbitration Register
 *
 * This register defines the priority and bus time out timing in PCI bus arbi-
 * tration.
 */
	typedef union pic_p_arb_u {
		picreg_t	pic_p_arb_regval;
		struct {
			picreg_t               :	32; /* 63:32 */
			picreg_t               :	8; /* 31:24 */
			picreg_t dev_broke     :	4; /* 23:20 */
			picreg_t               :	2; /* 19:18 */
			picreg_t req_wait_tick :	2; /* 17:16 */
			picreg_t               :	4; /* 15:12 */
			picreg_t req_wait_en   :	4; /* 11:8 */
			picreg_t disarb        :	1; /* 7:7 */
			picreg_t freeze_gnt    :	1; /* 6:6 */
			picreg_t               :	1; /* 5:5 */
			picreg_t en_bridge_hi  :	2; /* 4:3 */
			picreg_t               :	1; /* 2:2 */
			picreg_t en_bridge_lo  :	2; /* 1:0 */
		} pic_p_arb_fld_s;
	} pic_p_arb_u_t;
/*
 * Internal Ram Parity Error Register
 *
 * This register logs information about parity errors on internal ram access.
 */
	typedef union pic_p_ram_perr_u {
		picreg_t	pic_p_ram_perr_regval;
		struct {
			picreg_t         	     :	6; /* 63:58 */
			picreg_t ate_err_addr        :	10; /* 57:48 */
			picreg_t         	     :	7; /* 47:41 */
			picreg_t rd_resp_err_addr    :	9; /* 40:32 */
			picreg_t wrt_resp_err_addr   :	8; /* 31:24 */
			picreg_t         	     :	2; /* 23:22 */
			picreg_t ate_err 	     :	1; /* 21:21 */
			picreg_t rd_resp_err         :	1; /* 20:20 */
			picreg_t wrt_resp_err        :	1; /* 19:19 */
			picreg_t dbe_ate 	     :	3; /* 18:16 */
			picreg_t dbe_rd  	     :	8; /* 15:8 */
			picreg_t dbe_wrt 	     :	8; /* 7:0 */
		} pic_p_ram_perr_fld_s;
	} pic_p_ram_perr_u_t;
/*
 * Time-out Register
 *
 * This register determines retry hold off and max retries allowed for PIO
 * accesses to PCI/PCI-X.
 */
	typedef union pic_p_bus_timeout_u {
		picreg_t	pic_p_bus_timeout_regval;
		struct {
			picreg_t               :	32; /* 63:32 */
			picreg_t               :	11; /* 31:21 */
			picreg_t pci_retry_hld :	5; /* 20:16 */
			picreg_t               :	6; /* 15:10 */
			picreg_t pci_retry_cnt :	10; /* 9:0 */
		} pic_p_bus_timeout_fld_s;
	} pic_p_bus_timeout_u_t;
/*
 * PCI/PCI-X Type 1 Configuration Register
 *
 * This register is use during accesses to the PCI/PCI-X type 1 configuration
 * space. The bits in this register are used to supplement the address during
 * the configuration cycle to select the correct secondary bus and device.
 */
	typedef union pic_type1_cfg_u {
		picreg_t	pic_type1_cfg_regval;
		struct {
			picreg_t         :	32; /* 63:32 */
			picreg_t         :	8; /* 31:24 */
			picreg_t bus_num :	8; /* 23:16 */
			picreg_t dev_num :	5; /* 15:11 */
			picreg_t         :	11; /* 10:0 */
		} pic_type1_cfg_fld_s;
	} pic_type1_cfg_u_t;
/*
 * PCI Bus Error Upper Address Holding Register
 *
 * This register holds the value of the upper address on the PCI Bus when an
 * error occurs.
 */
	typedef union pic_p_pci_err_upper_u {
		picreg_t	pic_p_pci_err_upper_regval;
		struct {
			picreg_t                :	32; /* 63:32 */
			picreg_t                :	4; /* 31:28 */
			picreg_t pci_xtalk_did  :	4; /* 27:24 */
			picreg_t                :	2; /* 23:22 */
			picreg_t pci_dac        :	1; /* 21:21 */
			picreg_t pci_dev_master :	1; /* 20:20 */
			picreg_t pci_vdev       :	1; /* 19:19 */
			picreg_t pci_dev_num    :	3; /* 18:16 */
			picreg_t pci_uaddr_err  :	16; /* 15:0 */
		} pic_p_pci_err_upper_fld_s;
	} pic_p_pci_err_upper_u_t;
/*
 * PCI Bus Error Lower Address Holding Register
 *
 * This register holds the value of the lower address on the PCI Bus when an
 * error occurs.
 */
	typedef union pic_p_pci_err_lower_u {
		picreg_t	pic_p_pci_err_lower_regval;
		struct {
			picreg_t                :	4; /* 63:60 */
			picreg_t pci_xtalk_did  :	4; /* 59:56 */
			picreg_t                :	2; /* 55:54 */
			picreg_t pci_dac        :	1; /* 53:53 */
			picreg_t pci_dev_master :	1; /* 52:52 */
			picreg_t pci_vdev       :	1; /* 51:51 */
			picreg_t pci_dev_num    :	3; /* 50:48 */
			picreg_t pci_uaddr_err  :	16; /* 47:32 */
			picreg_t pci_laddr_err  :	32; /* 31:0 */
		} pic_p_pci_err_lower_fld_s;
	} pic_p_pci_err_lower_u_t;
/*
 * PCI-X Error Address Register
 *
 * This register contains the address on the PCI-X bus when an error oc-
 * curred.
 */
	typedef union pic_p_pcix_err_addr_u {
		picreg_t	pic_p_pcix_err_addr_regval;
		struct {
			picreg_t pcix_err_addr :	64; /* 63:0 */
		} pic_p_pcix_err_addr_fld_s;
	} pic_p_pcix_err_addr_u_t;
/*
 * PCI-X Error Attribute Register
 *
 * This register contains the attribute data on the PCI-X bus when an error
 * occurred.
 */
	typedef union pic_p_pcix_err_attr_u {
		picreg_t	pic_p_pcix_err_attr_regval;
		struct {
			picreg_t            :	16; /* 63:48 */
			picreg_t bus_cmd    :	4; /* 47:44 */
			picreg_t byte_cnt   :	12; /* 43:32 */
			picreg_t            :	1; /* 31:31 */
			picreg_t ns         :	1; /* 30:30 */
			picreg_t ro         :	1; /* 29:29 */
			picreg_t tag        :	5; /* 28:24 */
			picreg_t bus_num    :	8; /* 23:16 */
			picreg_t dev_num    :	5; /* 15:11 */
			picreg_t fun_num    :	3; /* 10:8 */
			picreg_t l_byte_cnt :	8; /* 7:0 */
		} pic_p_pcix_err_attr_fld_s;
	} pic_p_pcix_err_attr_u_t;
/*
 * PCI-X Error Data Register
 *
 * This register contains the Data on the PCI-X bus when an error occurred.
 */
	typedef union pic_p_pcix_err_data_u {
		picreg_t	pic_p_pcix_err_data_regval;
		struct {
			picreg_t pcix_err_data :	64; /* 63:0 */
		} pic_p_pcix_err_data_fld_s;
	} pic_p_pcix_err_data_u_t;
/*
 * PCI-X Read Request Timeout Error Register
 *
 * This register contains a pointer into the PCI-X read data structure.
 */
	typedef union pic_p_pcix_read_req_to_u {
		picreg_t	pic_p_pcix_read_req_to_regval;
		struct {
			picreg_t                :	55; /* 63:9 */
			picreg_t rd_buff_loc    :	5; /* 8:4 */
			picreg_t rd_buff_struct :	4; /* 3:0 */
		} pic_p_pcix_read_req_to_fld_s;
	} pic_p_pcix_read_req_to_u_t;
/*
 * INT_STATUS Register
 *
 * This is the current interrupt status register which maintains the current
 * status of all the interrupting devices which generated a n interrupt. This
 * register is read only and all the bits are active high. A high bit at
 * INT_STATE means the corresponding INT_N pin has been asserted
 * (low).
 */
	typedef union pic_p_int_status_u {
		picreg_t	pic_p_int_status_regval;
		struct {
			picreg_t                  :	22; /* 63:42 */
			picreg_t int_ram_perr     :	1; /* 41:41 */
			picreg_t bus_arb_broke    :	1; /* 40:40 */
			picreg_t pci_x_req_tout   :	1; /* 39:39 */
			picreg_t pci_x_tabort     :	1; /* 38:38 */
			picreg_t pci_x_perr       :	1; /* 37:37 */
			picreg_t pci_x_serr       :	1; /* 36:36 */
			picreg_t pci_x_mretry     :	1; /* 35:35 */
			picreg_t pci_x_mtout      :	1; /* 34:34 */
			picreg_t pci_x_da_parity  :	1; /* 33:33 */
			picreg_t pci_x_ad_parity  :	1; /* 32:32 */
			picreg_t                  :	1; /* 31:31 */
			picreg_t pmu_page_fault   :	1; /* 30:30 */
			picreg_t unexpected_resp  :	1; /* 29:29 */
			picreg_t bad_xresp_packet :	1; /* 28:28 */
			picreg_t bad_xreq_packet  :	1; /* 27:27 */
			picreg_t resp_xtalk_error :	1; /* 26:26 */
			picreg_t req_xtalk_error  :	1; /* 25:25 */
			picreg_t invalid_access   :	1; /* 24:24 */
			picreg_t unsupported_xop  :	1; /* 23:23 */
			picreg_t xreq_fifo_oflow  :	1; /* 22:22 */
			picreg_t llp_rec_snerror  :	1; /* 21:21 */
			picreg_t llp_rec_cberror  :	1; /* 20:20 */
			picreg_t llp_rcty         :	1; /* 19:19 */
			picreg_t llp_tx_retry     :	1; /* 18:18 */
			picreg_t llp_tcty         :	1; /* 17:17 */
			picreg_t                  :	1; /* 16:16 */
			picreg_t pci_abort        :	1; /* 15:15 */
			picreg_t pci_parity       :	1; /* 14:14 */
			picreg_t pci_serr         :	1; /* 13:13 */
			picreg_t pci_perr         :	1; /* 12:12 */
			picreg_t pci_master_tout  :	1; /* 11:11 */
			picreg_t pci_retry_cnt    :	1; /* 10:10 */
			picreg_t xread_req_tout   :	1; /* 9:9 */
			picreg_t                  :	1; /* 8:8 */
			picreg_t int_state        :	8; /* 7:0 */
		} pic_p_int_status_fld_s;
	} pic_p_int_status_u_t;
/*
 * Interrupt Enable Register
 *
 * This register enables the reporting of interrupt to the host. Each bit in this
 * register corresponds to the same bit in Interrupt Status register. All bits
 * are zero after reset.
 */
	typedef union pic_p_int_enable_u {
		picreg_t	pic_p_int_enable_regval;
		struct {
			picreg_t                     :	22; /* 63:42 */
			picreg_t en_int_ram_perr     :	1; /* 41:41 */
			picreg_t en_bus_arb_broke    :	1; /* 40:40 */
			picreg_t en_pci_x_req_tout   :	1; /* 39:39 */
			picreg_t en_pci_x_tabort     :	1; /* 38:38 */
			picreg_t en_pci_x_perr       :	1; /* 37:37 */
			picreg_t en_pci_x_serr       :	1; /* 36:36 */
			picreg_t en_pci_x_mretry     :	1; /* 35:35 */
			picreg_t en_pci_x_mtout      :	1; /* 34:34 */
			picreg_t en_pci_x_da_parity  :	1; /* 33:33 */
			picreg_t en_pci_x_ad_parity  :	1; /* 32:32 */
			picreg_t                     :	1; /* 31:31 */
			picreg_t en_pmu_page_fault   :	1; /* 30:30 */
			picreg_t en_unexpected_resp  :	1; /* 29:29 */
			picreg_t en_bad_xresp_packet :	1; /* 28:28 */
			picreg_t en_bad_xreq_packet  :	1; /* 27:27 */
			picreg_t en_resp_xtalk_error :	1; /* 26:26 */
			picreg_t en_req_xtalk_error  :	1; /* 25:25 */
			picreg_t en_invalid_access   :	1; /* 24:24 */
			picreg_t en_unsupported_xop  :	1; /* 23:23 */
			picreg_t en_xreq_fifo_oflow  :	1; /* 22:22 */
			picreg_t en_llp_rec_snerror  :	1; /* 21:21 */
			picreg_t en_llp_rec_cberror  :	1; /* 20:20 */
			picreg_t en_llp_rcty         :	1; /* 19:19 */
			picreg_t en_llp_tx_retry     :	1; /* 18:18 */
			picreg_t en_llp_tcty         :	1; /* 17:17 */
			picreg_t                     :	1; /* 16:16 */
			picreg_t en_pci_abort        :	1; /* 15:15 */
			picreg_t en_pci_parity       :	1; /* 14:14 */
			picreg_t en_pci_serr         :	1; /* 13:13 */
			picreg_t en_pci_perr         :	1; /* 12:12 */
			picreg_t en_pci_master_tout  :	1; /* 11:11 */
			picreg_t en_pci_retry_cnt    :	1; /* 10:10 */
			picreg_t en_xread_req_tout   :	1; /* 9:9 */
			picreg_t                     :	1; /* 8:8 */
			picreg_t en_int_state        :	8; /* 7:0 */
		} pic_p_int_enable_fld_s;
	} pic_p_int_enable_u_t;
/*
 * Reset Interrupt Register
 *
 * A write of a "1" clears the bit and rearms the error registers. Writes also
 * clear the error view register.
 */
	typedef union pic_p_int_rst_u {
		picreg_t	pic_p_int_rst_regval;
		struct {
			picreg_t                       :	22; /* 63:42 */
			picreg_t logv_int_ram_perr     :	1; /* 41:41 */
			picreg_t logv_bus_arb_broke    :	1; /* 40:40 */
			picreg_t logv_pci_x_req_tout   :	1; /* 39:39 */
			picreg_t logv_pci_x_tabort     :	1; /* 38:38 */
			picreg_t logv_pci_x_perr       :	1; /* 37:37 */
			picreg_t logv_pci_x_serr       :	1; /* 36:36 */
			picreg_t logv_pci_x_mretry     :	1; /* 35:35 */
			picreg_t logv_pci_x_mtout      :	1; /* 34:34 */
			picreg_t logv_pci_x_da_parity  :	1; /* 33:33 */
			picreg_t logv_pci_x_ad_parity  :	1; /* 32:32 */
			picreg_t                       :	1; /* 31:31 */
			picreg_t logv_pmu_page_fault   :	1; /* 30:30 */
			picreg_t logv_unexpected_resp  :	1; /* 29:29 */
			picreg_t logv_bad_xresp_packet :	1; /* 28:28 */
			picreg_t logv_bad_xreq_packet  :	1; /* 27:27 */
			picreg_t logv_resp_xtalk_error :	1; /* 26:26 */
			picreg_t logv_req_xtalk_error  :	1; /* 25:25 */
			picreg_t logv_invalid_access   :	1; /* 24:24 */
			picreg_t logv_unsupported_xop  :	1; /* 23:23 */
			picreg_t logv_xreq_fifo_oflow  :	1; /* 22:22 */
			picreg_t logv_llp_rec_snerror  :	1; /* 21:21 */
			picreg_t logv_llp_rec_cberror  :	1; /* 20:20 */
			picreg_t logv_llp_rcty         :	1; /* 19:19 */
			picreg_t logv_llp_tx_retry     :	1; /* 18:18 */
			picreg_t logv_llp_tcty         :	1; /* 17:17 */
			picreg_t                       :	1; /* 16:16 */
			picreg_t logv_pci_abort        :	1; /* 15:15 */
			picreg_t logv_pci_parity       :	1; /* 14:14 */
			picreg_t logv_pci_serr         :	1; /* 13:13 */
			picreg_t logv_pci_perr         :	1; /* 12:12 */
			picreg_t logv_pci_master_tout  :	1; /* 11:11 */
			picreg_t logv_pci_retry_cnt    :	1; /* 10:10 */
			picreg_t logv_xread_req_tout   :	1; /* 9:9 */
                        picreg_t                       :        2; /* 8:7 */
			picreg_t multi_clr             :	1; /* 6:6 */
			picreg_t                       :	6; /* 5:0 */
		} pic_p_int_rst_fld_s;
	} pic_p_int_rst_u_t;

/*
 * Interrupt Mode Register
 *
 * This register defines the interrupting mode of the INT_N pins.
 */
	typedef union pic_p_int_mode_u {
		picreg_t	pic_p_int_mode_regval;
		struct {
			picreg_t            :	32; /* 63:32 */
			picreg_t            :	24; /* 31:8 */
			picreg_t en_clr_pkt :	8; /* 7:0 */
		} pic_p_int_mode_fld_s;
	} pic_p_int_mode_u_t;
/*
 * Interrupt Device Select Register
 *
 * This register associates interrupt pins with devices thus allowing buffer
 * management (flushing) when a device interrupt occurs.
 */
	typedef union pic_p_int_device_u {
		picreg_t	pic_p_int_device_regval;
		struct {
			picreg_t          :	32; /* 63:32 */
			picreg_t          :	8; /* 31:24 */
			picreg_t int7_dev :	3; /* 23:21 */
			picreg_t int6_dev :	3; /* 20:18 */
			picreg_t int5_dev :	3; /* 17:15 */
			picreg_t int4_dev :	3; /* 14:12 */
			picreg_t int3_dev :	3; /* 11:9 */
			picreg_t int2_dev :	3; /* 8:6 */
			picreg_t int1_dev :	3; /* 5:3 */
			picreg_t int0_dev :	3; /* 2:0 */
		} pic_p_int_device_fld_s;
	} pic_p_int_device_u_t;
/*
 * Host Error Interrupt Field Register
 *
 * This register tells which bit location in the host's Interrupt Status register
 * to set or reset when any error condition happens.
 */
	typedef union pic_p_int_host_err_u {
		picreg_t	pic_p_int_host_err_regval;
		struct {
			picreg_t                :	32; /* 63:32 */
			picreg_t                :	24; /* 31:8 */
			picreg_t bridge_err_fld :	8; /* 7:0 */
		} pic_p_int_host_err_fld_s;
	} pic_p_int_host_err_u_t;
/*
 * Interrupt (x) Host Address Register
 *
 * This register allow different host address to be assigned to each interrupt
 * pin and the bit in the host.
 */
	typedef union pic_p_int_addr_u {
		picreg_t	pic_p_int_addr_regval;
		struct {
			picreg_t          :	8; /* 63:56 */
			picreg_t int_fld  :	8; /* 55:48 */
			picreg_t int_addr :	48; /* 47:0 */
		} pic_p_int_addr_fld_s;
	} pic_p_int_addr_u_t;
/*
 * Error Interrupt View Register
 *
 * This register contains the view of which interrupt occur even if they are
 * not currently enabled. The group clear is used to clear these bits just like
 * the interrupt status register bits.
 */
	typedef union pic_p_err_int_view_u {
		picreg_t	pic_p_err_int_view_regval;
		struct {
			picreg_t                  :	22; /* 63:42 */
			picreg_t int_ram_perr     :	1; /* 41:41 */
			picreg_t bus_arb_broke    :	1; /* 40:40 */
			picreg_t pci_x_req_tout   :	1; /* 39:39 */
			picreg_t pci_x_tabort     :	1; /* 38:38 */
			picreg_t pci_x_perr       :	1; /* 37:37 */
			picreg_t pci_x_serr       :	1; /* 36:36 */
			picreg_t pci_x_mretry     :	1; /* 35:35 */
			picreg_t pci_x_mtout      :	1; /* 34:34 */
			picreg_t pci_x_da_parity  :	1; /* 33:33 */
			picreg_t pci_x_ad_parity  :	1; /* 32:32 */
			picreg_t                  :	1; /* 31:31 */
			picreg_t pmu_page_fault   :	1; /* 30:30 */
			picreg_t unexpected_resp  :	1; /* 29:29 */
			picreg_t bad_xresp_packet :	1; /* 28:28 */
			picreg_t bad_xreq_packet  :	1; /* 27:27 */
			picreg_t resp_xtalk_error :	1; /* 26:26 */
			picreg_t req_xtalk_error  :	1; /* 25:25 */
			picreg_t invalid_access   :	1; /* 24:24 */
			picreg_t unsupported_xop  :	1; /* 23:23 */
			picreg_t xreq_fifo_oflow  :	1; /* 22:22 */
			picreg_t llp_rec_snerror  :	1; /* 21:21 */
			picreg_t llp_rec_cberror  :	1; /* 20:20 */
			picreg_t llp_rcty         :	1; /* 19:19 */
			picreg_t llp_tx_retry     :	1; /* 18:18 */
			picreg_t llp_tcty         :	1; /* 17:17 */
			picreg_t                  :	1; /* 16:16 */
			picreg_t pci_abort        :	1; /* 15:15 */
			picreg_t pci_parity       :	1; /* 14:14 */
			picreg_t pci_serr         :	1; /* 13:13 */
			picreg_t pci_perr         :	1; /* 12:12 */
			picreg_t pci_master_tout  :	1; /* 11:11 */
			picreg_t pci_retry_cnt    :	1; /* 10:10 */
			picreg_t xread_req_tout   :	1; /* 9:9 */
			picreg_t                  :	9; /* 8:0 */
		} pic_p_err_int_view_fld_s;
	} pic_p_err_int_view_u_t;


/*
 * Multiple Interrupt Register
 *
 * This register indicates if any interrupt occurs more than once without be-
 * ing cleared.
 */
	typedef union pic_p_mult_int_u {
		picreg_t	pic_p_mult_int_regval;
		struct {
			picreg_t                  :	22; /* 63:42 */
			picreg_t int_ram_perr     :	1; /* 41:41 */
			picreg_t bus_arb_broke    :	1; /* 40:40 */
			picreg_t pci_x_req_tout   :	1; /* 39:39 */
			picreg_t pci_x_tabort     :	1; /* 38:38 */
			picreg_t pci_x_perr       :	1; /* 37:37 */
			picreg_t pci_x_serr       :	1; /* 36:36 */
			picreg_t pci_x_mretry     :	1; /* 35:35 */
			picreg_t pci_x_mtout      :	1; /* 34:34 */
			picreg_t pci_x_da_parity  :	1; /* 33:33 */
			picreg_t pci_x_ad_parity  :	1; /* 32:32 */
			picreg_t                  :	1; /* 31:31 */
			picreg_t pmu_page_fault   :	1; /* 30:30 */
			picreg_t unexpected_resp  :	1; /* 29:29 */
			picreg_t bad_xresp_packet :	1; /* 28:28 */
			picreg_t bad_xreq_packet  :	1; /* 27:27 */
			picreg_t resp_xtalk_error :	1; /* 26:26 */
			picreg_t req_xtalk_error  :	1; /* 25:25 */
			picreg_t invalid_access   :	1; /* 24:24 */
			picreg_t unsupported_xop  :	1; /* 23:23 */
			picreg_t xreq_fifo_oflow  :	1; /* 22:22 */
			picreg_t llp_rec_snerror  :	1; /* 21:21 */
			picreg_t llp_rec_cberror  :	1; /* 20:20 */
			picreg_t llp_rcty         :	1; /* 19:19 */
			picreg_t llp_tx_retry     :	1; /* 18:18 */
			picreg_t llp_tcty         :	1; /* 17:17 */
			picreg_t                  :	1; /* 16:16 */
			picreg_t pci_abort        :	1; /* 15:15 */
			picreg_t pci_parity       :	1; /* 14:14 */
			picreg_t pci_serr         :	1; /* 13:13 */
			picreg_t pci_perr         :	1; /* 12:12 */
			picreg_t pci_master_tout  :	1; /* 11:11 */
			picreg_t pci_retry_cnt    :	1; /* 10:10 */
			picreg_t xread_req_tout   :	1; /* 9:9 */
			picreg_t                  :	1; /* 8:8 */
			picreg_t int_state        :	8; /* 7:0 */
		} pic_p_mult_int_fld_s;
	} pic_p_mult_int_u_t;
/*
 * Force Always Interrupt (x) Register
 *
 * A write to this data independent write only register will force a set inter-
 * rupt to occur as if the interrupt line had transitioned. If the interrupt line
 * is already active an addition set interrupt packet is set. All buffer flush op-
 * erations also occur on this operation.
 */


/*
 * Force Interrupt (x) Register
 *
 * A write to this data independent write only register in conjunction with
 * the assertion of the corresponding interrupt line will force a set interrupt
 * to occur as if the interrupt line had transitioned. The interrupt line must
 * be active for this operation to generate a set packet, otherwise the write
 * PIO is ignored. All buffer flush operations also occur when the set packet
 * is sent on this operation.
 */


/*
 * Device Registers
 *
 * The Device registers contain device specific and mapping information.
 */
	typedef union pic_device_reg_u {
		picreg_t	pic_device_reg_regval;
		struct {
			picreg_t               :	32; /* 63:32 */
			picreg_t               :	2; /* 31:30 */
			picreg_t en_virtual1   :	1; /* 29:29 */
			picreg_t en_error_lock :	1; /* 28:28 */
			picreg_t en_page_chk   :	1; /* 27:27 */
			picreg_t force_pci_par :	1; /* 26:26 */
			picreg_t en_virtual0   :	1; /* 25:25 */
			picreg_t               :	1; /* 24:24 */
			picreg_t dir_wrt_gen   :	1; /* 23:23 */
			picreg_t dev_size      :	1; /* 22:22 */
			picreg_t real_time     :	1; /* 21:21 */
			picreg_t               :	1; /* 20:20 */
			picreg_t swap_direct   :	1; /* 19:19 */
			picreg_t prefetch      :	1; /* 18:18 */
			picreg_t precise       :	1; /* 17:17 */
			picreg_t coherent      :	1; /* 16:16 */
			picreg_t barrier       :	1; /* 15:15 */
			picreg_t gbr           :	1; /* 14:14 */
			picreg_t dev_swap      :	1; /* 13:13 */
			picreg_t dev_io_mem    :	1; /* 12:12 */
			picreg_t dev_off       :	12; /* 11:0 */
		} pic_device_reg_fld_s;
	} pic_device_reg_u_t;
/*
 * Device (x) Write Request Buffer Flush
 *
 * When read, this register will return a 0x00 after the write buffer associat-
 * ed with the device has been flushed. (PCI Only)
 */


/*
 * Even Device Read Response Buffer Register (PCI Only)
 *
 * This register is use to allocate the read response buffers for the even num-
 * bered devices. (0,2)
 */
	typedef union pic_p_even_resp_u {
		picreg_t	pic_p_even_resp_regval;
		struct {
			picreg_t              :	32; /* 63:32 */
			picreg_t buff_14_en   :	1; /* 31:31 */
			picreg_t buff_14_vdev :	2; /* 30:29 */
			picreg_t buff_14_pdev :	1; /* 28:28 */
			picreg_t buff_12_en   :	1; /* 27:27 */
			picreg_t buff_12_vdev :	2; /* 26:25 */
			picreg_t buff_12_pdev :	1; /* 24:24 */
			picreg_t buff_10_en   :	1; /* 23:23 */
			picreg_t buff_10_vdev :	2; /* 22:21 */
			picreg_t buff_10_pdev :	1; /* 20:20 */
			picreg_t buff_8_en    :	1; /* 19:19 */
			picreg_t buff_8_vdev  :	2; /* 18:17 */
			picreg_t buff_8_pdev  :	1; /* 16:16 */
			picreg_t buff_6_en    :	1; /* 15:15 */
			picreg_t buff_6_vdev  :	2; /* 14:13 */
			picreg_t buff_6_pdev  :	1; /* 12:12 */
			picreg_t buff_4_en    :	1; /* 11:11 */
			picreg_t buff_4_vdev  :	2; /* 10:9 */
			picreg_t buff_4_pdev  :	1; /* 8:8 */
			picreg_t buff_2_en    :	1; /* 7:7 */
			picreg_t buff_2_vdev  :	2; /* 6:5 */
			picreg_t buff_2_pdev  :	1; /* 4:4 */
			picreg_t buff_0_en    :	1; /* 3:3 */
			picreg_t buff_0_vdev  :	2; /* 2:1 */
			picreg_t buff_0_pdev  :	1; /* 0:0 */
		} pic_p_even_resp_fld_s;
	} pic_p_even_resp_u_t;
/*
 * Odd Device Read Response Buffer Register (PCI Only)
 *
 * This register is use to allocate the read response buffers for the odd num-
 * bered devices. (1,3))
 */
	typedef union pic_p_odd_resp_u {
		picreg_t	pic_p_odd_resp_regval;
		struct {
			picreg_t              :	32; /* 63:32 */
			picreg_t buff_15_en   :	1; /* 31:31 */
			picreg_t buff_15_vdev :	2; /* 30:29 */
			picreg_t buff_15_pdev :	1; /* 28:28 */
			picreg_t buff_13_en   :	1; /* 27:27 */
			picreg_t buff_13_vdev :	2; /* 26:25 */
			picreg_t buff_13_pdev :	1; /* 24:24 */
			picreg_t buff_11_en   :	1; /* 23:23 */
			picreg_t buff_11_vdev :	2; /* 22:21 */
			picreg_t buff_11_pdev :	1; /* 20:20 */
			picreg_t buff_9_en    :	1; /* 19:19 */
			picreg_t buff_9_vdev  :	2; /* 18:17 */
			picreg_t buff_9_pdev  :	1; /* 16:16 */
			picreg_t buff_7_en    :	1; /* 15:15 */
			picreg_t buff_7_vdev  :	2; /* 14:13 */
			picreg_t buff_7_pdev  :	1; /* 12:12 */
			picreg_t buff_5_en    :	1; /* 11:11 */
			picreg_t buff_5_vdev  :	2; /* 10:9 */
			picreg_t buff_5_pdev  :	1; /* 8:8 */
			picreg_t buff_3_en    :	1; /* 7:7 */
			picreg_t buff_3_vdev  :	2; /* 6:5 */
			picreg_t buff_3_pdev  :	1; /* 4:4 */
			picreg_t buff_1_en    :	1; /* 3:3 */
			picreg_t buff_1_vdev  :	2; /* 2:1 */
			picreg_t buff_1_pdev  :	1; /* 0:0 */
		} pic_p_odd_resp_fld_s;
	} pic_p_odd_resp_u_t;
/*
 * Read Response Buffer Status Register (PCI Only)
 *
 * This read only register contains the current response buffer status.
 */
	typedef union pic_p_resp_status_u {
		picreg_t	pic_p_resp_status_regval;
		struct {
			picreg_t           :	32; /* 63:32 */
			picreg_t rrb_valid :	16; /* 31:16 */
			picreg_t rrb_inuse :	16; /* 15:0 */
		} pic_p_resp_status_fld_s;
	} pic_p_resp_status_u_t;
/*
 * Read Response Buffer Clear Register (PCI Only)
 *
 * A write to this register clears the current contents of the buffer.
 */
	typedef union pic_p_resp_clear_u {
		picreg_t	pic_p_resp_clear_regval;
		struct {
			picreg_t           :	32; /* 63:32 */
			picreg_t           :	16; /* 31:16 */
			picreg_t rrb_clear :	16; /* 15:0 */
		} pic_p_resp_clear_fld_s;
	} pic_p_resp_clear_u_t;
/*
 * PCI Read Response Buffer (x) Upper Address Match
 *
 * The PCI Bridge read response buffer upper address register is a read only
 * register which contains the upper 16-bits of the address and status used to
 * select the buffer for a PCI transaction.
 */
	typedef union pic_p_buf_upper_addr_match_u {
		picreg_t	pic_p_buf_upper_addr_match_regval;
		struct {
			picreg_t          :	32; /* 63:32 */
			picreg_t filled   :	1; /* 31:31 */
			picreg_t armed    :	1; /* 30:30 */
			picreg_t flush    :	1; /* 29:29 */
			picreg_t xerr     :	1; /* 28:28 */
			picreg_t pkterr   :	1; /* 27:27 */
			picreg_t timeout  :	1; /* 26:26 */
			picreg_t prefetch :	1; /* 25:25 */
			picreg_t precise  :	1; /* 24:24 */
			picreg_t dw_be    :	8; /* 23:16 */
			picreg_t upp_addr :	16; /* 15:0 */
		} pic_p_buf_upper_addr_match_fld_s;
	} pic_p_buf_upper_addr_match_u_t;
/*
 * PCI Read Response Buffer (x) Lower Address Match
 *
 * The PCI Bridge read response buffer lower address Match register is a
 * read only register which contains the address and status used to select the
 * buffer for a PCI transaction.
 */
	typedef union pic_p_buf_lower_addr_match_u {
		picreg_t	pic_p_buf_lower_addr_match_regval;
		struct {
			picreg_t filled   :	1; /* 63:63 */
			picreg_t armed    :	1; /* 62:62 */
			picreg_t flush    :	1; /* 61:61 */
			picreg_t xerr     :	1; /* 60:60 */
			picreg_t pkterr   :	1; /* 59:59 */
			picreg_t timeout  :	1; /* 58:58 */
			picreg_t prefetch :	1; /* 57:57 */
			picreg_t precise  :	1; /* 56:56 */
			picreg_t dw_be    :	8; /* 55:48 */
			picreg_t upp_addr :	16; /* 47:32 */
			picreg_t low_addr :	32; /* 31:0 */
		} pic_p_buf_lower_addr_match_fld_s;
	} pic_p_buf_lower_addr_match_u_t;
/*
 * PCI Buffer (x) Flush Count with Data Touch Register
 *
 * This counter is incremented each time the corresponding response buffer
 * is flushed after at least a single data element in the buffer is used. A word
 * write to this address clears the count.
 */
	typedef union pic_flush_w_touch_u {
		picreg_t	pic_flush_w_touch_regval;
		struct {
			picreg_t           :	32; /* 63:32 */
			picreg_t           :	16; /* 31:16 */
			picreg_t touch_cnt :	16; /* 15:0 */
		} pic_flush_w_touch_fld_s;
	} pic_flush_w_touch_u_t;
/*
 * PCI Buffer (x) Flush Count w/o Data Touch Register
 *
 * This counter is incremented each time the corresponding response buffer
 * is flushed without any data element in the buffer being used. A word
 * write to this address clears the count.
 */
	typedef union pic_flush_wo_touch_u {
		picreg_t	pic_flush_wo_touch_regval;
		struct {
			picreg_t             :	32; /* 63:32 */
			picreg_t             :	16; /* 31:16 */
			picreg_t notouch_cnt :	16; /* 15:0 */
		} pic_flush_wo_touch_fld_s;
	} pic_flush_wo_touch_u_t;
/*
 * PCI Buffer (x) Request in Flight Count Register
 *
 * This counter is incremented on each bus clock while the request is in-
 * flight. A word write to this address clears the count. ]
 */
	typedef union pic_inflight_u {
		picreg_t	pic_inflight_regval;
		struct {
			picreg_t              :	32; /* 63:32 */
			picreg_t              :	16; /* 31:16 */
			picreg_t inflight_cnt :	16; /* 15:0 */
		} pic_inflight_fld_s;
	} pic_inflight_u_t;
/*
 * PCI Buffer (x) Prefetch Request Count Register
 *
 * This counter is incremented each time the request using this buffer was
 * generated from the prefetcher. A word write to this address clears the
 * count.
 */
	typedef union pic_prefetch_u {
		picreg_t	pic_prefetch_regval;
		struct {
			picreg_t              :	32; /* 63:32 */
			picreg_t              :	16; /* 31:16 */
			picreg_t prefetch_cnt :	16; /* 15:0 */
		} pic_prefetch_fld_s;
	} pic_prefetch_u_t;
/*
 * PCI Buffer (x) Total PCI Retry Count Register
 *
 * This counter is incremented each time a PCI bus retry occurs and the ad-
 * dress matches the tag for the selected buffer. The buffer must also has this
 * request in-flight. A word write to this address clears the count.
 */
	typedef union pic_total_pci_retry_u {
		picreg_t	pic_total_pci_retry_regval;
		struct {
			picreg_t           :	32; /* 63:32 */
			picreg_t           :	16; /* 31:16 */
			picreg_t retry_cnt :	16; /* 15:0 */
		} pic_total_pci_retry_fld_s;
	} pic_total_pci_retry_u_t;
/*
 * PCI Buffer (x) Max PCI Retry Count Register
 *
 * This counter is contains the maximum retry count for a single request
 * which was in-flight for this buffer. A word write to this address clears the
 * count.
 */
	typedef union pic_max_pci_retry_u {
		picreg_t	pic_max_pci_retry_regval;
		struct {
			picreg_t               :	32; /* 63:32 */
			picreg_t               :	16; /* 31:16 */
			picreg_t max_retry_cnt :	16; /* 15:0 */
		} pic_max_pci_retry_fld_s;
	} pic_max_pci_retry_u_t;
/*
 * PCI Buffer (x) Max Latency Count Register
 *
 * This counter is contains the maximum count (in bus clocks) for a single
 * request which was in-flight for this buffer. A word write to this address
 * clears the count.
 */
	typedef union pic_max_latency_u {
		picreg_t	pic_max_latency_regval;
		struct {
			picreg_t                 :	32; /* 63:32 */
			picreg_t                 :	16; /* 31:16 */
			picreg_t max_latency_cnt :	16; /* 15:0 */
		} pic_max_latency_fld_s;
	} pic_max_latency_u_t;
/*
 * PCI Buffer (x) Clear All Register
 *
 * Any access to this register clears all the count values for the (x) registers.
 */


/*
 * PCI-X Registers
 *
 * This register contains the address in the read buffer structure. There are
 * 16 read buffer structures.
 */
	typedef union pic_rd_buf_addr_u {
		picreg_t	pic_rd_buf_addr_regval;
		struct {
			picreg_t pcix_err_addr :	64; /* 63:0 */
		} pic_rd_buf_addr_fld_s;
	} pic_rd_buf_addr_u_t;
/*
 * PCI-X Read Buffer (x) Attribute Register
 *
 * This register contains the attribute data in the read buffer structure. There
 * are  16 read buffer structures.
 */
	typedef union pic_px_read_buf_attr_u {
		picreg_t	pic_px_read_buf_attr_regval;
		struct {
			picreg_t                :	16; /* 63:48 */
			picreg_t bus_cmd        :	4; /* 47:44 */
			picreg_t byte_cnt       :	12; /* 43:32 */
			picreg_t entry_valid    :	1; /* 31:31 */
			picreg_t ns             :	1; /* 30:30 */
			picreg_t ro             :	1; /* 29:29 */
			picreg_t tag            :	5; /* 28:24 */
			picreg_t bus_num        :	8; /* 23:16 */
			picreg_t dev_num        :	5; /* 15:11 */
			picreg_t fun_num        :	3; /* 10:8 */
			picreg_t                :	2; /* 7:6 */
			picreg_t f_buffer_index :	6; /* 5:0 */
		} pic_px_read_buf_attr_fld_s;
	} pic_px_read_buf_attr_u_t;
/*
 * PCI-X Write Buffer (x) Address Register
 *
 * This register contains the address in the write buffer structure. There are
 * 8 write buffer structures.
 */
	typedef union pic_px_write_buf_addr_u {
		picreg_t	pic_px_write_buf_addr_regval;
		struct {
			picreg_t pcix_err_addr :	64; /* 63:0 */
		} pic_px_write_buf_addr_fld_s;
	} pic_px_write_buf_addr_u_t;
/*
 * PCI-X Write Buffer (x) Attribute Register
 *
 * This register contains the attribute data in the write buffer structure.
 * There are 8 write buffer structures.
 */
	typedef union pic_px_write_buf_attr_u {
		picreg_t	pic_px_write_buf_attr_regval;
		struct {
			picreg_t                :	16; /* 63:48 */
			picreg_t bus_cmd        :	4; /* 47:44 */
			picreg_t byte_cnt       :	12; /* 43:32 */
			picreg_t entry_valid    :	1; /* 31:31 */
			picreg_t ns             :	1; /* 30:30 */
			picreg_t ro             :	1; /* 29:29 */
			picreg_t tag            :	5; /* 28:24 */
			picreg_t bus_num        :	8; /* 23:16 */
			picreg_t dev_num        :	5; /* 15:11 */
			picreg_t fun_num        :	3; /* 10:8 */
			picreg_t                :	2; /* 7:6 */
			picreg_t f_buffer_index :	6; /* 5:0 */
		} pic_px_write_buf_attr_fld_s;
	} pic_px_write_buf_attr_u_t;
/*
 * PCI-X Write Buffer (x) Valid Register
 *
 * This register contains the valid or inuse cache lines for this buffer struc-
 * ture.
 */
	typedef union pic_px_write_buf_valid_u {
		picreg_t	pic_px_write_buf_valid_regval;
		struct {
			picreg_t                :	32; /* 63:32 */
			picreg_t wrt_valid_buff :	32; /* 31:0 */
		} pic_px_write_buf_valid_fld_s;
	} pic_px_write_buf_valid_u_t;

#endif				/* __ASSEMBLY__ */
#endif                          /* _ASM_SN_PCI_PIC_H */
