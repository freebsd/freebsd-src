/*
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_MCA_H_
#define _MACHINE_MCA_H_

struct mca_guid {
	uint32_t	data1;
	uint16_t	data2;
	uint16_t	data3;
	uint8_t		data4[8];
};

struct mca_record_header {
	uint64_t	rh_seqnr;		/* Record id. */
	uint8_t		rh_major;		/* BCD (=02). */
	uint8_t		rh_minor;		/* BCD (=00). */
	uint8_t		rh_error;		/* Error severity. */
#define	MCA_RH_ERROR_RECOVERABLE	0
#define	MCA_RH_ERROR_FATAL		1
#define	MCA_RH_ERROR_CORRECTED		2
	uint8_t		rh_flags;
#define	MCA_RH_FLAGS_PLATFORM_ID	0x01	/* Platform_id present. */
	uint32_t	rh_length;		/* Size including header. */
	uint8_t		rh_time[8];
#define	MCA_RH_TIME_SEC		0
#define	MCA_RH_TIME_MIN		1
#define	MCA_RH_TIME_HOUR	2
#define	MCA_RH_TIME_MDAY	4
#define	MCA_RH_TIME_MON		5
#define	MCA_RH_TIME_YEAR	6
#define	MCA_RH_TIME_CENT	7
	struct mca_guid	rh_platform;		/* XXX not really a GUID. */
};

struct mca_section_header {
	struct mca_guid	sh_guid;
	uint8_t		sh_major;		/* BCD (=02). */
	uint8_t		sh_minor;		/* BCD (=00). */
	uint8_t		sh_flags;
#define	MCA_SH_FLAGS_CORRECTED	0x01		/* Error has been corrected. */
#define	MCA_SH_FLAGS_PROPAGATE	0x02		/* Possible propagation. */
#define	MCA_SH_FLAGS_RESET	0x04		/* Reset device before use. */
#define	MCA_SH_FLAGS_VALID	0x80		/* Flags are valid. */
	uint8_t		__reserved;
	uint32_t	sh_length;		/* Size including header. */
};

struct mca_cpu_record {
	uint64_t	cpu_flags;
#define	MCA_CPU_FLAGS_ERRMAP		(1ULL << 0)
#define	MCA_CPU_FLAGS_STATE		(1ULL << 1)
#define	MCA_CPU_FLAGS_CR_LID		(1ULL << 2)
#define	MCA_CPU_FLAGS_PSI_STRUCT	(1ULL << 3)
#define	MCA_CPU_FLAGS_CACHE(x)		(((x) >> 4) & 15)
#define	MCA_CPU_FLAGS_TLB(x)		(((x) >> 8) & 15)
#define	MCA_CPU_FLAGS_BUS(x)		(((x) >> 12) & 15)
#define	MCA_CPU_FLAGS_REG(x)		(((x) >> 16) & 15)
#define	MCA_CPU_FLAGS_MS(x)		(((x) >> 20) & 15)
#define	MCA_CPU_FLAGS_CPUID		(1ULL << 24)
	uint64_t	cpu_errmap;
	uint64_t	cpu_state;
	uint64_t	cpu_cr_lid;
	/* Nx cpu_mod (cache) */
	/* Nx cpu_mod (TLB) */
	/* Nx cpu_mod (bus) */
	/* Nx cpu_mod (reg) */
	/* Nx cpu_mod (MS) */
	/* cpu_cpuid */
	/* cpu_psi */
};

struct mca_cpu_cpuid {
	uint64_t	cpuid[6];
};

struct mca_cpu_mod {
	uint64_t	cpu_mod_flags;
#define	MCA_CPU_MOD_FLAGS_INFO	(1ULL << 0)
#define	MCA_CPU_MOD_FLAGS_REQID	(1ULL << 1)
#define	MCA_CPU_MOD_FLAGS_RSPID	(1ULL << 2)
#define	MCA_CPU_MOD_FLAGS_TGTID	(1ULL << 3)
#define	MCA_CPU_MOD_FLAGS_IP	(1ULL << 4)
	uint64_t	cpu_mod_info;
	uint64_t	cpu_mod_reqid;
	uint64_t	cpu_mod_rspid;
	uint64_t	cpu_mod_tgtid;
	uint64_t	cpu_mod_ip;
};

struct mca_cpu_psi {
	uint64_t	cpu_psi_flags;
#define	MCA_CPU_PSI_FLAGS_STATE	(1ULL << 0)
#define	MCA_CPU_PSI_FLAGS_BR	(1ULL << 1)
#define	MCA_CPU_PSI_FLAGS_CR	(1ULL << 2)
#define	MCA_CPU_PSI_FLAGS_AR	(1ULL << 3)
#define	MCA_CPU_PSI_FLAGS_RR	(1ULL << 4)
#define	MCA_CPU_PSI_FLAGS_FR	(1ULL << 5)
	uint8_t		cpu_psi_state[1024];	/* XXX variable? */
	uint64_t	cpu_psi_br[8];
	uint64_t	cpu_psi_cr[128];	/* XXX variable? */
	uint64_t	cpu_psi_ar[128];	/* XXX variable? */
	uint64_t	cpu_psi_rr[8];
	uint64_t	cpu_psi_fr[256];	/* 16 bytes per register! */
};

#define	MCA_GUID_CPU		\
	{0xe429faf1,0x3cb7,0x11d4,{0xbc,0xa7,0x00,0x80,0xc7,0x3c,0x88,0x81}}
#define	MCA_GUID_MEMORY		\
	{0xe429faf2,0x3cb7,0x11d4,{0xbc,0xa7,0x00,0x80,0xc7,0x3c,0x88,0x81}}
#define	MCA_GUID_SEL		\
	{0xe429faf3,0x3cb7,0x11d4,{0xbc,0xa7,0x00,0x80,0xc7,0x3c,0x88,0x81}}
#define	MCA_GUID_PCI_BUS	\
	{0xe429faf4,0x3cb7,0x11d4,{0xbc,0xa7,0x00,0x80,0xc7,0x3c,0x88,0x81}}
#define	MCA_GUID_SMBIOS		\
	{0xe429faf5,0x3cb7,0x11d4,{0xbc,0xa7,0x00,0x80,0xc7,0x3c,0x88,0x81}}
#define	MCA_GUID_PCI_DEV	\
	{0xe429faf6,0x3cb7,0x11d4,{0xbc,0xa7,0x00,0x80,0xc7,0x3c,0x88,0x81}}
#define	MCA_GUID_GENERIC	\
	{0xe429faf7,0x3cb7,0x11d4,{0xbc,0xa7,0x00,0x80,0xc7,0x3c,0x88,0x81}}

#ifdef _KERNEL

void ia64_mca_init(void);
void ia64_mca_save_state(int);

#endif /* _KERNEL */

#endif /* _MACHINE_MCA_H_ */
