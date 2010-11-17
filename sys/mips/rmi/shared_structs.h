/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * RMI_BSD */
#ifndef _SHARED_STRUCTS_H
#define _SHARED_STRUCTS_H

/* If you make any changes to the below structs, shared_structs_offsets.h
 * should be regenerated
 */
#define BOOT1_INFO_VERSION 0x0001

struct boot1_info {
	uint64_t boot_level;
	uint64_t io_base;
	uint64_t output_device;
	uint64_t uart_print;
	uint64_t led_output;
	uint64_t init;
	uint64_t exit;
	uint64_t warm_reset;
	uint64_t wakeup;
	uint64_t cpu_online_map;
	uint64_t master_reentry_sp;
	uint64_t master_reentry_gp;
	uint64_t master_reentry_fn;
	uint64_t slave_reentry_fn;
	uint64_t magic_dword;
	uint64_t uart_putchar;
	uint64_t size;
	uint64_t uart_getchar;
	uint64_t nmi_handler;
	uint64_t psb_version;
	uint64_t mac_addr;
	uint64_t cpu_frequency;
	uint64_t board_version;
	uint64_t malloc;
	uint64_t free;
	uint64_t alloc_pbuf;
	uint64_t free_pbuf;
	uint64_t psb_os_cpu_map;
	uint64_t userapp_cpu_map;
	uint64_t wakeup_os;
	uint64_t psb_mem_map;
	uint64_t board_major_version;
	uint64_t board_minor_version;
	uint64_t board_manf_revision;
	uint64_t board_serial_number;
	uint64_t psb_physaddr_map;
};

extern struct boot1_info xlr_boot1_info;


/* This structure is passed to all applications launched from the linux
   loader through K0 register
 */
#define XLR_LOADER_INFO_MAGIC 0x600ddeed
struct xlr_loader_info {
	uint32_t magic;
	/* xlr_loader_shared_struct_t for CPU 0 will start here */
	unsigned long sh_mem_start;
	/* Size of the shared memory b/w linux apps and rmios apps  */
	uint32_t app_sh_mem_size;
};

/* Boot loader uses the linux mips convention */
#define BOOT1_MEMMAP_MAX	32

enum xlr_phys_memmap_t {
	BOOT1_MEM_RAM = 1, BOOT1_MEM_ROM_DATA, BOOT1_MEM_RESERVED
};

struct xlr_boot1_mem_map {
	uint32_t num_entries;
	struct {
		uint64_t addr;
		uint64_t size;
		uint32_t type;
		uint32_t pad;
	}      physmem_map[BOOT1_MEMMAP_MAX];
};


#endif
