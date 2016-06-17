/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * This is the interface to the remote debugger stub.
 *
 */
#include <linux/serialP.h>
#include <linux/serial_reg.h>

#include <asm/serial.h>
#include <asm/io.h>
#include <asm/hp-lj/asic.h>


int putDebugChar(char c);
char getDebugChar(void);


///////////////////////  andros values ///////////////////////////////////////////////////////
#define SERIAL_REG(offset) (*((volatile unsigned int*)(HPSR_BASE_ADDR|offset)))

// Register set base address
#define HPSR_BASE_ADDR   0xbfe00000UL

// Transmit / Receive Data
#define HPSR_DATA_OFFSET    0x00020010UL
// Transmit control / status
#define HPSR_TX_STAT_OFFSET 0x0002000CUL
// Receive status
#define HPSR_RX_STAT_OFFSET 0x00020008UL

#define HPSR_TX_STAT_READY  0x8UL
#define HPSR_RX_DATA_AVAIL  0x4UL


///////////////////////  harmony values ///////////////////////////////////////////////////////
// Transmit / Receive Data
#define H_HPSR_DATA_TX       *((volatile unsigned int*)0xbff65014)
// Transmit / Receive Data
#define H_HPSR_DATA_RX       *((volatile unsigned int*)0xbff65018)
// Status
#define H_HPSR_STAT          *((volatile unsigned int*)0xbff65004)

// harmony serial status bits
#define H_SER_STAT_TX_EMPTY       0x04
#define H_SER_STAT_RX_EMPTY       0x10




int putDebugChar(char c)
{
	if (GetAsicId() == HarmonyAsic) {
		while (!( ( (H_HPSR_STAT) & H_SER_STAT_TX_EMPTY) != 0));

		H_HPSR_DATA_TX = (unsigned int) c;

	} else if (GetAsicId() == AndrosAsic) {
        	while (((SERIAL_REG(HPSR_TX_STAT_OFFSET) & HPSR_TX_STAT_READY) == 0))
             		;
        	SERIAL_REG(HPSR_DATA_OFFSET) = (unsigned int) c;
        }
	return 1;
}

char getDebugChar(void)
{
	if (GetAsicId() == HarmonyAsic) {
		while (!(((H_HPSR_STAT) & H_SER_STAT_RX_EMPTY) == 0));

	        return H_HPSR_DATA_RX;

	} else if (GetAsicId() == AndrosAsic) {
        	while ((SERIAL_REG(HPSR_RX_STAT_OFFSET) & HPSR_RX_DATA_AVAIL) == 0)
              		;

        	return (SERIAL_REG(HPSR_DATA_OFFSET));

	}
}


