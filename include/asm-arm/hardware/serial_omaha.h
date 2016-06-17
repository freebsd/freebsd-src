/*
 *  linux/include/asm-arm/hardware/serial_omaha.h
 *
 *  Internal header file for Omaha serial ports
 *
 *  Copyright (C) 2002 ARM Limited
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
#ifndef ASM_ARM_HARDWARE_SERIAL_OMAHA_H
#define ASM_ARM_HARDWARE_SERIAL_OMAHA_H

#define OMAHA_UART0_BASE                0x15000000	 /*  Uart 0 base */
#define OMAHA_UART1_BASE                0x15004000	 /*  Uart 1 base */

/* 
 *  UART Register Offsets.
 */
#define OMAHA_ULCON                     0x00		 /*  Line control */
#define OMAHA_UCON                      0x04		 /*  Control */
#define OMAHA_UFCON                     0x08		 /*  FIFO control */
#define OMAHA_UMCON                     0x0C		 /*  Modem control */
#define OMAHA_UTRSTAT                   0x10		 /*  Rx/Tx status */
#define OMAHA_UERSTAT                   0x14		 /*  Rx Error Status */
#define OMAHA_UFSTAT                    0x18		 /*  FIFO status */
#define OMAHA_UMSTAT                    0x1C		 /*  Modem status */
#define OMAHA_UTXH                      0x20		 /*  Transmission Hold (byte wide) */
#define OMAHA_URXH                      0x24		 /*  Receive buffer (byte wide) */
#define OMAHA_UBRDIV                    0x28		 /*  Baud rate divisor */

/*  UART status flags in OMAHA_UTRSTAT
 */
#define OMAHA_URX_FULL                  0x1		 /*  Receive buffer has valid data */
#define OMAHA_UTX_EMPTY                 0x2		 /*  Transmitter has finished */

/* UART status flags in UFSTAT */
#define OMAHA_TXFF			0x200		/* TX FIFO Full. */
#define OMAHA_RXFF			0x100		/* RX FIFO Full. */
#define OMAHA_TXFF_CNT			0xF0		/* Tx FIFO count */
#define OMAHA_RXFF_CNT			0x0F		/* Rx FIFO count */

/* UART status flags in UERSTAT
*/
#define OMAHA_UART_OVERRUN  		0x0
#define OMAHA_UART_PARITY		0x1
#define OMAHA_UART_FRAME		0x2
#define OMAHA_UART_BREAK		0x4

#endif
