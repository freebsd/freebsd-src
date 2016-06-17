/*
 *  linux/include/asm-arm/arch-integrator/irqs.h
 *
 *  Copyright (C) 1999 ARM Limited
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

/* Use the integrator definitions */
#include <asm/arch/platform.h>

/* 
 *  IRQ interrupts definitions are the same the INT definitions
 *  held within platform.h
 */
#define IRQ_SOFTINT                     INT_SOFTINT
#define IRQ_UARTINT0                    INT_UARTINT0
#define IRQ_UARTINT1                    INT_UARTINT1
#define IRQ_KMIINT0                     INT_KMIINT0
#define IRQ_KMIINT1                     INT_KMIINT1
#define IRQ_TIMERINT0                   INT_TIMERINT0
#define IRQ_TIMERINT1                   INT_TIMERINT1
#define IRQ_TIMERINT2                   INT_TIMERINT2
#define IRQ_RTCINT                      INT_RTCINT
#define IRQ_EXPINT0                     INT_EXPINT0
#define IRQ_EXPINT1                     INT_EXPINT1
#define IRQ_EXPINT2                     INT_EXPINT2
#define IRQ_EXPINT3                     INT_EXPINT3
#define IRQ_PCIINT0                     INT_PCIINT0
#define IRQ_PCIINT1                     INT_PCIINT1
#define IRQ_PCIINT2                     INT_PCIINT2
#define IRQ_PCIINT3                     INT_PCIINT3
#define IRQ_V3INT                       INT_V3INT
#define IRQ_CPINT0                      INT_CPINT0
#define IRQ_CPINT1                      INT_CPINT1
#define IRQ_LBUSTIMEOUT                 INT_LBUSTIMEOUT
#define IRQ_APCINT                      INT_APCINT

#define IRQMASK_SOFTINT                 INTMASK_SOFTINT
#define IRQMASK_UARTINT0                INTMASK_UARTINT0
#define IRQMASK_UARTINT1                INTMASK_UARTINT1
#define IRQMASK_KMIINT0                 INTMASK_KMIINT0
#define IRQMASK_KMIINT1                 INTMASK_KMIINT1
#define IRQMASK_TIMERINT0               INTMASK_TIMERINT0
#define IRQMASK_TIMERINT1               INTMASK_TIMERINT1
#define IRQMASK_TIMERINT2               INTMASK_TIMERINT2
#define IRQMASK_RTCINT                  INTMASK_RTCINT
#define IRQMASK_EXPINT0                 INTMASK_EXPINT0
#define IRQMASK_EXPINT1                 INTMASK_EXPINT1
#define IRQMASK_EXPINT2                 INTMASK_EXPINT2
#define IRQMASK_EXPINT3                 INTMASK_EXPINT3
#define IRQMASK_PCIINT0                 INTMASK_PCIINT0
#define IRQMASK_PCIINT1                 INTMASK_PCIINT1
#define IRQMASK_PCIINT2                 INTMASK_PCIINT2
#define IRQMASK_PCIINT3                 INTMASK_PCIINT3
#define IRQMASK_V3INT                   INTMASK_V3INT
#define IRQMASK_CPINT0                  INTMASK_CPINT0
#define IRQMASK_CPINT1                  INTMASK_CPINT1
#define IRQMASK_LBUSTIMEOUT             INTMASK_LBUSTIMEOUT
#define IRQMASK_APCINT                  INTMASK_APCINT

/* 
 *  FIQ interrupts definitions are the same the INT definitions.
 */
#define FIQ_SOFTINT                     INT_SOFTINT
#define FIQ_UARTINT0                    INT_UARTINT0
#define FIQ_UARTINT1                    INT_UARTINT1
#define FIQ_KMIINT0                     INT_KMIINT0
#define FIQ_KMIINT1                     INT_KMIINT1
#define FIQ_TIMERINT0                   INT_TIMERINT0
#define FIQ_TIMERINT1                   INT_TIMERINT1
#define FIQ_TIMERINT2                   INT_TIMERINT2
#define FIQ_RTCINT                      INT_RTCINT
#define FIQ_EXPINT0                     INT_EXPINT0
#define FIQ_EXPINT1                     INT_EXPINT1
#define FIQ_EXPINT2                     INT_EXPINT2
#define FIQ_EXPINT3                     INT_EXPINT3
#define FIQ_PCIINT0                     INT_PCIINT0
#define FIQ_PCIINT1                     INT_PCIINT1
#define FIQ_PCIINT2                     INT_PCIINT2
#define FIQ_PCIINT3                     INT_PCIINT3
#define FIQ_V3INT                       INT_V3INT
#define FIQ_CPINT0                      INT_CPINT0
#define FIQ_CPINT1                      INT_CPINT1
#define FIQ_LBUSTIMEOUT                 INT_LBUSTIMEOUT
#define FIQ_APCINT                      INT_APCINT

#define FIQMASK_SOFTINT                 INTMASK_SOFTINT
#define FIQMASK_UARTINT0                INTMASK_UARTINT0
#define FIQMASK_UARTINT1                INTMASK_UARTINT1
#define FIQMASK_KMIINT0                 INTMASK_KMIINT0
#define FIQMASK_KMIINT1                 INTMASK_KMIINT1
#define FIQMASK_TIMERINT0               INTMASK_TIMERINT0
#define FIQMASK_TIMERINT1               INTMASK_TIMERINT1
#define FIQMASK_TIMERINT2               INTMASK_TIMERINT2
#define FIQMASK_RTCINT                  INTMASK_RTCINT
#define FIQMASK_EXPINT0                 INTMASK_EXPINT0
#define FIQMASK_EXPINT1                 INTMASK_EXPINT1
#define FIQMASK_EXPINT2                 INTMASK_EXPINT2
#define FIQMASK_EXPINT3                 INTMASK_EXPINT3
#define FIQMASK_PCIINT0                 INTMASK_PCIINT0
#define FIQMASK_PCIINT1                 INTMASK_PCIINT1
#define FIQMASK_PCIINT2                 INTMASK_PCIINT2
#define FIQMASK_PCIINT3                 INTMASK_PCIINT3
#define FIQMASK_V3INT                   INTMASK_V3INT
#define FIQMASK_CPINT0                  INTMASK_CPINT0
#define FIQMASK_CPINT1                  INTMASK_CPINT1
#define FIQMASK_LBUSTIMEOUT             INTMASK_LBUSTIMEOUT
#define FIQMASK_APCINT                  INTMASK_APCINT

/* 
 *  Misc. interrupt definitions
 */
#define IRQ_KEYBDINT                    INT_KMIINT0
#define IRQ_MOUSEINT                    INT_KMIINT1

#define IRQMASK_KEYBDINT                INTMASK_KMIINT0
#define IRQMASK_MOUSEINT                INTMASK_KMIINT1

#define NR_IRQS                         (MAXIRQNUM + 1)

