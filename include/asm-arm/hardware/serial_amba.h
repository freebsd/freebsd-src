/*
 *  linux/include/asm-arm/hardware/serial_amba.h
 *
 *  Internal header file for AMBA serial ports
 *
 *  Copyright (C) ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef ASM_ARM_HARDWARE_SERIAL_AMBA_H
#define ASM_ARM_HARDWARE_SERIAL_AMBA_H

/* -------------------------------------------------------------------------------
 *  From AMBA UART (PL010) Block Specification (ARM-0001-CUST-DSPC-A03)
 * -------------------------------------------------------------------------------
 *  UART Register Offsets.
 */
#define AMBA_UARTDR                     0x00	 /*  Data read or written from the interface. */
#define AMBA_UARTRSR                    0x04	 /*  Receive status register (Read). */
#define AMBA_UARTECR                    0x04	 /*  Error clear register (Write). */
#define AMBA_UARTLCR_H                  0x08	 /*  Line control register, high byte. */
#define AMBA_UARTLCR_M                  0x0C	 /*  Line control register, middle byte. */
#define AMBA_UARTLCR_L                  0x10	 /*  Line control register, low byte. */
#define AMBA_UARTCR                     0x14	 /*  Control register. */
#define AMBA_UARTFR                     0x18	 /*  Flag register (Read only). */
#define AMBA_UARTIIR                    0x1C	 /*  Interrupt indentification register (Read). */
#define AMBA_UARTICR                    0x1C	 /*  Interrupt clear register (Write). */
#define AMBA_UARTILPR                   0x20	 /*  IrDA low power counter register. */

#define AMBA_UARTRSR_OE                 0x08
#define AMBA_UARTRSR_BE                 0x04
#define AMBA_UARTRSR_PE                 0x02
#define AMBA_UARTRSR_FE                 0x01

#define AMBA_UARTFR_TXFF                0x20
#define AMBA_UARTFR_RXFE                0x10
#define AMBA_UARTFR_BUSY                0x08
#define AMBA_UARTFR_DCD			0x04
#define AMBA_UARTFR_DSR			0x02
#define AMBA_UARTFR_CTS			0x01
#define AMBA_UARTFR_TMSK                (AMBA_UARTFR_TXFF + AMBA_UARTFR_BUSY)
 
#define AMBA_UARTCR_RTIE                0x40
#define AMBA_UARTCR_TIE                 0x20
#define AMBA_UARTCR_RIE                 0x10
#define AMBA_UARTCR_MSIE                0x08
#define AMBA_UARTCR_IIRLP               0x04
#define AMBA_UARTCR_SIREN               0x02
#define AMBA_UARTCR_UARTEN              0x01
 
#define AMBA_UARTLCR_H_WLEN_8           0x60
#define AMBA_UARTLCR_H_WLEN_7           0x40
#define AMBA_UARTLCR_H_WLEN_6           0x20
#define AMBA_UARTLCR_H_WLEN_5           0x00
#define AMBA_UARTLCR_H_FEN              0x10
#define AMBA_UARTLCR_H_STP2             0x08
#define AMBA_UARTLCR_H_EPS              0x04
#define AMBA_UARTLCR_H_PEN              0x02
#define AMBA_UARTLCR_H_BRK              0x01

#define AMBA_UARTIIR_RTIS               0x08
#define AMBA_UARTIIR_TIS                0x04
#define AMBA_UARTIIR_RIS                0x02
#define AMBA_UARTIIR_MIS                0x01

#define ARM_BAUD_460800                 1
#define ARM_BAUD_230400                 3
#define ARM_BAUD_115200                 7
#define ARM_BAUD_57600                  15
#define ARM_BAUD_38400                  23
#define ARM_BAUD_19200                  47
#define ARM_BAUD_14400                  63
#define ARM_BAUD_9600                   95
#define ARM_BAUD_4800                   191
#define ARM_BAUD_2400                   383
#define ARM_BAUD_1200                   767

#define AMBA_UARTRSR_ANY	(AMBA_UARTRSR_OE|AMBA_UARTRSR_BE|AMBA_UARTRSR_PE|AMBA_UARTRSR_FE)
#define AMBA_UARTFR_MODEM_ANY	(AMBA_UARTFR_DCD|AMBA_UARTFR_DSR|AMBA_UARTFR_CTS)

#endif
