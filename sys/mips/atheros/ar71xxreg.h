/*-
 * Copyright (c) 2009 Oleksandr Tymoshenko
 * All rights reserved.
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
#ifndef _AR71XX_REG_H_
#define _AR71XX_REG_H_

#define ATH_READ_REG(reg) \
    *((volatile uint32_t *)MIPS_PHYS_TO_KSEG1((reg)))

#define ATH_WRITE_REG(reg, val) \
    *((volatile uint32_t *)MIPS_PHYS_TO_KSEG1((reg))) = (val)

#define	AR71XX_UART_ADDR	0x18020000

/* APB registers */
/* 
 * APB interrupt status and mask register and interrupt bit numbers for 
 */
#define AR71XX_MISC_INTR_STATUS	0x18060010
#define AR71XX_MISC_INTR_MASK	0x18060014
#define		MISC_INTR_TIMER		0
#define		MISC_INTR_ERROR		1
#define		MISC_INTR_GPIO		2
#define		MISC_INTR_UART		3
#define		MISC_INTR_WATCHDOG	4
#define		MISC_INTR_PERF		5
#define		MISC_INTR_OHCI		6
#define		MISC_INTR_DMA		7

#define AR71XX_RST_RESET	0x18060024
#define		RST_RESET_CPU_COLD_RESET	(1 << 20) /* Cold reset */
#define		RST_RESET_FULL_CHIP_RESET	(1 << 24) /* Same as pulling
							     the reset pin */
#endif /* _AR71XX_REG_H_ */
