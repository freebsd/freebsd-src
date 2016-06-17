/* -*- linux-c -*- */
#ifndef __PCIREGS_H
#define __PCIREGS_H

#include <linux/pci.h>

/*******************************************************************************
 * Copyright (c) 1997 - 1999 PLX Technology, Inc.
 * 
 * PLX Technology Inc. licenses this software under specific terms and
 * conditions.  Use of any of the software or derviatives thereof in any
 * product without a PLX Technology chip is strictly prohibited. 
 * 
 * PLX Technology, Inc. provides this software AS IS, WITHOUT ANY WARRANTY,
 * EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION, ANY WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  PLX makes no guarantee
 * or representations regarding the use of, or the results of the use of,
 * the software and documentation in terms of correctness, accuracy,
 * reliability, currentness, or otherwise; and you rely on the software,
 * documentation and results solely at your own risk.
 * 
 * IN NO EVENT SHALL PLX BE LIABLE FOR ANY LOSS OF USE, LOSS OF BUSINESS,
 * LOSS OF PROFITS, INDIRECT, INCIDENTAL, SPECIAL OR CONSEQUENTIAL DAMAGES
 * OF ANY KIND.  IN NO EVENT SHALL PLX'S TOTAL LIABILITY EXCEED THE SUM
 * PAID TO PLX FOR THE PRODUCT LICENSED HEREUNDER.
 * 
 ******************************************************************************/

/* Modifications and extensions
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 **/

/******************************************************************************
 *
 * File Name:   PciRegs.h
 *
 * Module Name: IOP API and PCI API
 *
 * Description: This file defines the generic PCI Configuration Registers
 *
 * Revision:
 *     09-03-99 : PCI SDK v3.00 Release
 *
 ******************************************************************************/

#define CFG_VENDOR_ID           PCI_VENDOR_ID
#define CFG_COMMAND             PCI_COMMAND
#define CFG_REV_ID              PCI_REVISION_ID
#define CFG_CACHE_SIZE          PCI_CACHE_LINE_SIZE
#define CFG_BAR0                PCI_BASE_ADDRESS_0
#define CFG_BAR1                PCI_BASE_ADDRESS_1
#define CFG_BAR2                PCI_BASE_ADDRESS_2
#define CFG_BAR3                PCI_BASE_ADDRESS_3
#define CFG_BAR4                PCI_BASE_ADDRESS_4
#define CFG_BAR5                PCI_BASE_ADDRESS_5
#define CFG_CIS_PTR             PCI_CARDBUS_CIS
#define CFG_SUB_VENDOR_ID       PCI_SUBSYSTEM_VENDOR_ID
#define CFG_EXP_ROM_BASE        PCI_ROM_ADDRESS
#define CFG_RESERVED1           PCI_CAPABILITY_LIST
#define CFG_RESERVED2           (PCI_CAPABILITY_LIST + 4)
#define CFG_INT_LINE            PCI_INTERRUPT_LINE

#endif  /* __PCIREGS_H */
