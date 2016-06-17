/*
 * arch/ppc/kernel/pplus.h
 *
 * Definitions for Motorola MCG Falcon/Raven & HAWK North Bridge & Memory ctlr.
 *
 * Author: Mark A. Greer
 *         mgreer@mvista.com
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __ASMPPC_PPLUS_H
#define __ASMPPC_PPLUS_H

#include <asm/pci-bridge.h>

/*
 * The Falcon/Raven and HAWK have 4 sets of registers:
 *   1) PPC Registers which define the mappings from PPC bus to PCI bus,
 *      etc.
 *   2) PCI Registers which define the mappings from PCI bus to PPC bus and the
 *      MPIC base address.
 *   3) MPIC registers
 *   4) System Memory Controller (SMC) registers.
 */

#define	PPLUS_RAVEN_VEND_DEV_ID		0x48011057
#define	PPLUS_HAWK_VEND_DEV_ID		0x48031057

#define	PPLUS_PCI_CONFIG_ADDR_OFF	0x00000cf8
#define	PPLUS_PCI_CONFIG_DATA_OFF	0x00000cfc

#define PPLUS_MPIC_SIZE			0x00040000U
#define PPLUS_SMC_SIZE			0x00001000U

/*
 * Define PPC register offsets.
 */
#define PPLUS_PPC_XSADD0_OFF			0x40
#define PPLUS_PPC_XSOFF0_OFF			0x44
#define PPLUS_PPC_XSADD1_OFF			0x48
#define PPLUS_PPC_XSOFF1_OFF			0x4c
#define PPLUS_PPC_XSADD2_OFF			0x50
#define PPLUS_PPC_XSOFF2_OFF			0x54
#define PPLUS_PPC_XSADD3_OFF			0x58
#define PPLUS_PPC_XSOFF3_OFF			0x5c

/*
 * Define PCI register offsets.
 */
#define PPLUS_PCI_PSADD0_OFF			0x80
#define PPLUS_PCI_PSOFF0_OFF			0x84
#define PPLUS_PCI_PSADD1_OFF			0x88
#define PPLUS_PCI_PSOFF1_OFF			0x8c
#define PPLUS_PCI_PSADD2_OFF			0x90
#define PPLUS_PCI_PSOFF2_OFF			0x94
#define PPLUS_PCI_PSADD3_OFF			0x98
#define PPLUS_PCI_PSOFF3_OFF			0x9c

/*
 * Define the System Memory Controller (SMC) register offsets.
 */
#define PPLUS_SMC_RAM_A_SIZE_REG_OFF		0x10
#define PPLUS_SMC_RAM_B_SIZE_REG_OFF		0x11
#define PPLUS_SMC_RAM_C_SIZE_REG_OFF		0x12
#define PPLUS_SMC_RAM_D_SIZE_REG_OFF		0x13
#define PPLUS_SMC_RAM_E_SIZE_REG_OFF		0xc0	/* HAWK Only */
#define PPLUS_SMC_RAM_F_SIZE_REG_OFF		0xc1	/* HAWK Only */
#define PPLUS_SMC_RAM_G_SIZE_REG_OFF		0xc2	/* HAWK Only */
#define PPLUS_SMC_RAM_H_SIZE_REG_OFF		0xc3	/* HAWK Only */

#define	PPLUS_FALCON_SMC_REG_COUNT		4
#define	PPLUS_HAWK_SMC_REG_COUNT		8



int pplus_init(struct pci_controller *hose,
		 uint ppc_reg_base,
		 ulong processor_pci_mem_start,
		 ulong processor_pci_mem_end,
		 ulong processor_pci_io_start,
		 ulong processor_pci_io_end,
		 ulong processor_mpic_base);

unsigned long pplus_get_mem_size(uint smc_base);

int pplus_mpic_init(unsigned int pci_mem_offset);

#endif /* __ASMPPC_PPLUS_H */
