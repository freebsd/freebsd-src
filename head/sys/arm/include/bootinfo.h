/*-
 * Copyright (C) 2006-2008 Semihalf, Marian Balakowicz <m8@semihalf.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_BOOTINFO_H_
#define _MACHINE_BOOTINFO_H_

#if !defined(LOCORE)

/* Platform hardware spec, received from loader(8) */

#define BI_VERSION	1

struct bi_mem_region {
	vm_paddr_t	mem_base;
	vm_size_t	mem_size;
};

struct bi_eth_addr {
	u_int8_t	mac_addr[6];
	u_int8_t	padding[2];
};

struct bootinfo {
	u_int32_t	bi_version;
	vm_offset_t	bi_bar_base;
	u_int32_t	bi_cpu_clk;
	u_int32_t	bi_bus_clk;
	u_int8_t	bi_mem_reg_no;
	u_int8_t	bi_eth_addr_no;
	u_int8_t	padding[2];

	u_int8_t	bi_data[1];
	/*
	 * The bi_data container is allocated in run time and has the
	 * following layout:
	 *
	 * - bi_mem_reg_no elements of struct bi_mem_region
	 * - bi_eth_addr_no elements of struct bi_eth_addr
	 */
};

extern struct bootinfo *bootinfo;

struct bi_mem_region *bootinfo_mr(void);
struct bi_eth_addr *bootinfo_eth(void);
#endif

#endif /* _MACHINE_BOOTINFO_H_ */
