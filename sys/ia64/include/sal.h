/*-
 * Copyright (c) 2001 Doug Rabson
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
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_SAL_H_
#define _MACHINE_SAL_H_

struct sal_system_table {
	char		sal_signature[4];
#define	SAL_SIGNATURE	"SST_"
	u_int32_t	sal_length;
	u_int8_t	sal_rev[2];
	u_int16_t	sal_entry_count;
	u_int8_t	sal_checksum;
	u_int8_t	sal_reserved1[7];
	u_int8_t	sal_a_version[2];
	u_int8_t	sal_b_version[2];
	char		sal_oem_id[32];
	char		sal_product_id[32];
	u_int8_t	sal_reserved2[8];
};

struct sal_entrypoint_descriptor {
	u_int8_t	sale_type;	/* == 0 */
	u_int8_t	sale_reserved1[7];
	u_int64_t	sale_pal_proc;
	u_int64_t	sale_sal_proc;
	u_int64_t	sale_sal_gp;
	u_int8_t	sale_reserved2[16];
};

struct sal_memory_descriptor {
	u_int8_t	sale_type;	/* == 1 */
	u_int8_t	sale_need_virtual;
	u_int8_t	sale_current_attribute;
	u_int8_t	sale_access_rights;
	u_int8_t	sale_supported_attributes;
	u_int8_t	sale_reserved1;
	u_int8_t	sale_memory_type[2];
	u_int64_t	sale_physical_address;
	u_int32_t	sale_length;
	u_int8_t	sale_reserved2[12];
};

struct sal_platform_descriptor {
	u_int8_t	sale_type;	/* == 2 */
	u_int8_t	sale_features;
	u_int8_t	sale_reserved[14];
};

struct sal_tr_descriptor {
	u_int8_t	sale_type;	/* == 3 */
	u_int8_t	sale_register_type;
	u_int8_t	sale_register_number;
	u_int8_t	sale_reserved1[5];
	u_int64_t	sale_virtual_address;
	u_int64_t	sale_page_size;
	u_int8_t	sale_reserved2[8];
};

struct sal_ptc_cache_descriptor {
	u_int8_t	sale_type;	/* == 4 */
	u_int8_t	sale_reserved[3];
	u_int32_t	sale_domains;
	u_int64_t	sale_address;
};

struct sal_ap_wakeup_descriptor {
	u_int8_t	sale_type;	/* == 5 */
	u_int8_t	sale_mechanism;
	u_int8_t	sale_reserved[6];
	u_int64_t	sale_vector;
};

/*
 * SAL Procedure numbers.
 */

#define SAL_SET_VECTORS		0x01000000
#define SAL_GET_STATE_INFO	0x01000001
#define SAL_GET_STATE_INFO_SIZE	0x01000002
#define SAL_CLEAR_STATE_INFO	0x01000003
#define SAL_MC_RENDEZ		0x01000004
#define SAL_MC_SET_PARAMS	0x01000005
#define SAL_REGISTER_PHYSICAL_ADDR 0x01000006
#define SAL_CACHE_FLUSH		0x01000008
#define SAL_CACHE_INIT		0x01000009
#define SAL_PCI_CONFIG_READ	0x01000010
#define SAL_PCI_CONFIG_WRITE	0x01000011
#define SAL_FREQ_BASE		0x01000012
#define SAL_UPDATE_PAL		0x01000020

/* SAL_SET_VECTORS event handler types */
#define	SAL_OS_MCA		0
#define	SAL_OS_INIT		1
#define	SAL_OS_BOOT_RENDEZ	2

/* SAL_GET_STATE_INFO, SAL_GET_STATE_INFO_SIZE types */
#define	SAL_INFO_MCA		0
#define	SAL_INFO_INIT		1
#define	SAL_INFO_CMC		2
#define	SAL_INFO_CPE		3
#define	SAL_INFO_TYPES		4	/* number of types we know about */

struct ia64_sal_result {
	int64_t		sal_status;
	u_int64_t	sal_result[3];
};

typedef struct ia64_sal_result sal_entry_t
	(u_int64_t, u_int64_t, u_int64_t, u_int64_t,
	 u_int64_t, u_int64_t, u_int64_t, u_int64_t);

extern sal_entry_t *ia64_sal_entry;

extern void ia64_sal_init(struct sal_system_table *saltab);

#endif /* _MACHINE_SAL_H_ */
