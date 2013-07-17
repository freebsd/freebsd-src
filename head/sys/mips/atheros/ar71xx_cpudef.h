/*-
 * Copyright (c) 2010 Adrian Chadd
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

/* $FreeBSD$ */

#ifndef	__AR71XX_CPUDEF_H__
#define	__AR71XX_CPUDEF_H__

struct ar71xx_cpu_def {
	void (* detect_mem_size) (void);
	void (* detect_sys_frequency) (void);
	void (* ar71xx_chip_device_stop) (uint32_t);
	void (* ar71xx_chip_device_start) (uint32_t);
	int (* ar71xx_chip_device_stopped) (uint32_t);
	void (* ar71xx_chip_set_pll_ge) (int, int, uint32_t);
	void (* ar71xx_chip_set_mii_speed) (uint32_t, uint32_t);
	void (* ar71xx_chip_set_mii_if) (uint32_t, ar71xx_mii_mode);
	void (* ar71xx_chip_ddr_flush_ge) (int);
	uint32_t (* ar71xx_chip_get_eth_pll) (unsigned int, int);

	/*
	 * From Linux - Handling this IRQ is a bit special.
	 * AR71xx - AR71XX_DDR_REG_FLUSH_PCI
	 * AR724x - AR724X_DDR_REG_FLUSH_PCIE
	 * AR91xx - AR91XX_DDR_REG_FLUSH_WMAC
	 *
	 * These are set when STATUSF_IP2 is set in regiser c0.
	 * This flush is done before the IRQ is handled to make
	 * sure the driver correctly sees any memory updates.
	 */
	void (* ar71xx_chip_ddr_flush_ip2) (void);
	/*
	 * The USB peripheral init code is subtly different for
	 * each chip.
	 */
	void (* ar71xx_chip_init_usb_peripheral) (void);
};

extern struct ar71xx_cpu_def * ar71xx_cpu_ops;

static inline void ar71xx_detect_sys_frequency(void)
{
	ar71xx_cpu_ops->detect_sys_frequency();
}

static inline void ar71xx_device_stop(uint32_t mask)
{
	ar71xx_cpu_ops->ar71xx_chip_device_stop(mask);
}

static inline void ar71xx_device_start(uint32_t mask)
{
	ar71xx_cpu_ops->ar71xx_chip_device_start(mask);
}

static inline int ar71xx_device_stopped(uint32_t mask)
{
	return ar71xx_cpu_ops->ar71xx_chip_device_stopped(mask);
}

static inline void ar71xx_device_set_pll_ge(int unit, int speed, uint32_t pll)
{
	ar71xx_cpu_ops->ar71xx_chip_set_pll_ge(unit, speed, pll);
}

static inline void ar71xx_device_set_mii_speed(int unit, int speed)
{
	ar71xx_cpu_ops->ar71xx_chip_set_mii_speed(unit, speed);
}

static inline void ar71xx_device_set_mii_if(int unit, ar71xx_mii_mode mii_cfg)
{
	ar71xx_cpu_ops->ar71xx_chip_set_mii_if(unit, mii_cfg);
}

static inline void ar71xx_device_flush_ddr_ge(int unit)
{
	ar71xx_cpu_ops->ar71xx_chip_ddr_flush_ge(unit);
}

static inline uint32_t ar71xx_device_get_eth_pll(unsigned int unit, int speed)
{
	return (ar71xx_cpu_ops->ar71xx_chip_get_eth_pll(unit, speed));
}

static inline void ar71xx_init_usb_peripheral(void)
{
	ar71xx_cpu_ops->ar71xx_chip_init_usb_peripheral();
}

static inline void ar71xx_device_ddr_flush_ip2(void)
{
	ar71xx_cpu_ops->ar71xx_chip_ddr_flush_ip2();
}

/* XXX shouldn't be here! */
extern uint32_t u_ar71xx_refclk;
extern uint32_t u_ar71xx_cpu_freq;
extern uint32_t u_ar71xx_ahb_freq;
extern uint32_t u_ar71xx_ddr_freq;

static inline uint64_t ar71xx_refclk(void) { return u_ar71xx_refclk; }
static inline uint64_t ar71xx_cpu_freq(void) { return u_ar71xx_cpu_freq; }
static inline uint64_t ar71xx_ahb_freq(void) { return u_ar71xx_ahb_freq; }
static inline uint64_t ar71xx_ddr_freq(void) { return u_ar71xx_ddr_freq; }
 
#endif
