/*-
 * Copyright (c) 1999 Doug Rabson
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$FreeBSD$
 */

#ifndef _ACPIDUMP_H_
#define _ACPIDUMP_H_

/* Generic Address structure */
struct ACPIgas {
	u_int8_t	address_space_id;
#define ACPI_GAS_MEMORY		0
#define ACPI_GAS_IO		1
#define ACPI_GAS_PCI		2
#define ACPI_GAS_EMBEDDED	3
#define ACPI_GAS_SMBUS		4
#define ACPI_GAS_FIXED		0x7f
	u_int8_t	register_bit_width;
	u_int8_t	register_bit_offset;
	u_int8_t	res;
	u_int64_t	address;
} __packed;

/* Root System Description Pointer */
struct ACPIrsdp {
	u_char		signature[8];
	u_char		sum;
	u_char		oem[6];
	u_char		revision;
	u_int32_t	rsdt_addr;
	u_int32_t	length;
	u_int64_t	xsdt_addr;
	u_char		xsum;
	u_char		_reserved_[3];
} __packed;

/* System Description Table */
struct ACPIsdt {
	u_char		signature[4];
	u_int32_t	len;
	u_char		rev;
	u_char		check;
	u_char		oemid[6];
	u_char		oemtblid[8];
	u_int32_t	oemrev;
	u_char		creator[4];
	u_int32_t	crerev;
#define SIZEOF_SDT_HDR 36	/* struct size except body */
	u_int32_t	body[1];/* This member should be casted */
} __packed;

/* Fixed ACPI Description Table (body) */
struct FACPbody {
	u_int32_t	facs_ptr;
	u_int32_t	dsdt_ptr;
	u_int8_t	int_model;
#define ACPI_FACP_INTMODEL_PIC	0	/* Standard PC-AT PIC */
#define ACPI_FACP_INTMODEL_APIC	1	/* Multiple APIC */
	u_char		reserved1;
	u_int16_t	sci_int;
	u_int32_t	smi_cmd;
	u_int8_t	acpi_enable;
	u_int8_t	acpi_disable;
	u_int8_t	s4biosreq;
	u_int8_t	reserved2;
	u_int32_t	pm1a_evt_blk;
	u_int32_t	pm1b_evt_blk;
	u_int32_t	pm1a_cnt_blk;
	u_int32_t	pm1b_cnt_blk;
	u_int32_t	pm2_cnt_blk;
	u_int32_t	pm_tmr_blk;
	u_int32_t	gpe0_blk;
	u_int32_t	gpe1_blk;
	u_int8_t	pm1_evt_len;
	u_int8_t	pm1_cnt_len;
	u_int8_t	pm2_cnt_len;
	u_int8_t	pm_tmr_len;
	u_int8_t	gpe0_len;
	u_int8_t	gpe1_len;
	u_int8_t	gpe1_base;
	u_int8_t	reserved3;
	u_int16_t	p_lvl2_lat;
	u_int16_t	p_lvl3_lat;
	u_int16_t	flush_size;
	u_int16_t	flush_stride;
	u_int8_t	duty_off;
	u_int8_t	duty_width;
	u_int8_t	day_alrm;
	u_int8_t	mon_alrm;
	u_int8_t	century;
	u_int16_t	iapc_boot_arch;
	u_char		reserved4[1];
	u_int32_t	flags;
#define ACPI_FACP_FLAG_WBINVD	1	/* WBINVD is correctly supported */
#define ACPI_FACP_FLAG_WBINVD_FLUSH 2	/* WBINVD flushes caches */
#define ACPI_FACP_FLAG_PROC_C1	4	/* C1 power state supported */
#define ACPI_FACP_FLAG_P_LVL2_UP 8	/* C2 power state works on SMP */
#define ACPI_FACP_FLAG_PWR_BUTTON 16	/* Power button uses control method */
#define ACPI_FACP_FLAG_SLP_BUTTON 32	/* Sleep button uses control method */
#define ACPI_FACP_FLAG_FIX_RTC	64	/* RTC wakeup not supported */
#define ACPI_FACP_FLAG_RTC_S4	128	/* RTC can wakeup from S4 state */
#define ACPI_FACP_FLAG_TMR_VAL_EXT 256	/* TMR_VAL is 32bit */
#define ACPI_FACP_FLAG_DCK_CAP	512	/* Can support docking */
	struct ACPIgas	reset_reg;
	u_int8_t	reset_value;
	u_int8_t	reserved5[3];
	u_int64_t	x_firmware_ctrl;
	u_int64_t	x_dsdt;
	struct ACPIgas	x_pm1a_evt_blk;
	struct ACPIgas	x_pm1b_evt_blk;
	struct ACPIgas	x_pm1a_cnt_blk;
	struct ACPIgas	x_pm1b_cnt_blk;
	struct ACPIgas	x_pm2_cnt_blk;
	struct ACPIgas	x_pm_tmr_blk;
	struct ACPIgas	x_gpe0_blk;
	struct ACPIgas	x_gpe1_blk;
} __packed;

/* Firmware ACPI Control Structure */
struct FACS {
	u_char		signature[4];
	u_int32_t	len;
	u_char		hard_sig[4];
	/*
	 * NOTE This should be filled with physical address below 1MB!!
	 * sigh....
	 */
	u_int32_t	firm_wake_vec;
	u_int32_t	g_lock;		/* bit field */
	/* 5.2.6.1 Global Lock */
#define ACPI_GLOBAL_LOCK_PENDING 1
#define ACPI_GLOBAL_LOCK_OWNED   2
	u_int32_t	flags;		/* bit field */
#define ACPI_FACS_FLAG_S4BIOS_F  1	/* Supports S4BIOS_SEQ */
	char		reserved[40];
} __packed;

struct MADT_local_apic {
	u_char		cpu_id;
	u_char		apic_id;
	u_int32_t	flags;
#define	ACPI_MADT_APIC_LOCAL_FLAG_ENABLED	1
} __packed;

struct MADT_io_apic {
	u_char		apic_id;
	u_char		reserved;
	u_int32_t	apic_addr;
	u_int32_t	int_base;
} __packed;

struct MADT_int_override {
	u_char		bus;
	u_char		source;
	u_int32_t	intr;
	u_int16_t	mps_flags;
#define	MPS_INT_FLAG_POLARITY_MASK	0x3
#define	MPS_INT_FLAG_POLARITY_CONFORM	0x0
#define	MPS_INT_FLAG_POLARITY_HIGH	0x1
#define	MPS_INT_FLAG_POLARITY_LOW	0x3
#define	MPS_INT_FLAG_TRIGGER_MASK	0xc
#define	MPS_INT_FLAG_TRIGGER_CONFORM	0x0
#define	MPS_INT_FLAG_TRIGGER_EDGE	0x4
#define	MPS_INT_FLAG_TRIGGER_LEVEL	0xc
} __packed;

struct MADT_nmi {
	u_int16_t	mps_flags;
	u_int32_t	intr;
} __packed;

struct MADT_local_nmi {
	u_char		cpu_id;
	u_int16_t	mps_flags;
	u_char		lintpin;
} __packed;

struct MADT_local_apic_override {
	u_char		reserved[2];
	u_int64_t	apic_addr;
} __packed;

struct MADT_io_sapic {
	u_char		apic_id;
	u_char		reserved;
	u_int32_t	int_base;
	u_int64_t	apic_addr;
} __packed;

struct MADT_local_sapic {
	u_char		cpu_id;
	u_char		apic_id;
	u_char		apic_eid;
	u_char		reserved[3];
	u_int32_t	flags;
} __packed;

struct MADT_int_src {
	u_int16_t	mps_flags;
	u_char		type;
#define	ACPI_MADT_APIC_INT_SOURCE_PMI	1
#define	ACPI_MADT_APIC_INT_SOURCE_INIT	2
#define	ACPI_MADT_APIC_INT_SOURCE_CPEI	3	/* Corrected Platform Error */
	u_char		cpu_id;
	u_char		cpu_eid;
	u_char		sapic_vector;
	u_int32_t	intr;
	u_char		reserved[4];
} __packed;

struct MADT_APIC {
	u_char		type;
#define	ACPI_MADT_APIC_TYPE_LOCAL_APIC	0
#define	ACPI_MADT_APIC_TYPE_IO_APIC	1
#define	ACPI_MADT_APIC_TYPE_INT_OVERRIDE 2
#define	ACPI_MADT_APIC_TYPE_NMI		3
#define	ACPI_MADT_APIC_TYPE_LOCAL_NMI	4
#define	ACPI_MADT_APIC_TYPE_LOCAL_OVERRIDE 5
#define	ACPI_MADT_APIC_TYPE_IO_SAPIC	6
#define	ACPI_MADT_APIC_TYPE_LOCAL_SAPIC	7
#define	ACPI_MADT_APIC_TYPE_INT_SRC	8
	u_char		len;
	union {
		struct MADT_local_apic local_apic;
		struct MADT_io_apic io_apic;
		struct MADT_int_override int_override;
		struct MADT_nmi nmi;
		struct MADT_local_nmi local_nmi;
		struct MADT_local_apic_override local_apic_override;
		struct MADT_io_sapic io_sapic;
		struct MADT_local_sapic local_sapic;
		struct MADT_int_src int_src;
	} body;
} __packed;

struct MADTbody {
	u_int32_t	lapic_addr;
	u_int32_t	flags;
#define	ACPI_APIC_FLAG_PCAT_COMPAT 1	/* System has dual-8259 setup. */
	u_char		body[1];
} __packed;

struct HPETbody {
	u_int32_t	block_hwrev:8,
			block_comparitors:5,
			block_counter_size:1,
			:1,
			block_legacy_capable:1,
			block_pcivendor:16;
	u_int32_t	base_addr;
	u_int64_t	reserved1;
	u_int8_t	hpet_number;
	u_int16_t	clock_tick __packed;
} __packed;

#if 0
void		*acpi_map_physical(vm_offset_t, size_t);
struct ACPIrsdp	*acpi_find_rsd_ptr(void);
int		 acpi_checksum(void *, size_t);
struct ACPIsdt	*acpi_map_sdt(vm_offset_t);
void		 acpi_print_rsd_ptr(struct ACPIrsdp *);
void		 acpi_print_sdt(struct ACPIsdt *);
void		 acpi_print_rsdt(struct ACPIsdt *);
void		 acpi_print_facp(struct FACPbody *);
void		 acpi_print_dsdt(struct ACPIsdt *);
void    	 acpi_handle_rsdt(struct ACPIsdt *);
void		 acpi_load_dsdt(char *, u_int8_t **, u_int8_t **);
void		 acpi_dump_dsdt(u_int8_t *, u_int8_t *);
#endif

/* Find and map the RSD PTR structure and return it for parsing */
struct ACPIsdt  *sdt_load_devmem(void);

/*
 * Load the DSDT from a previous save file.  Note that other tables are
 * not saved (i.e. FADT)
 */
struct ACPIsdt  *dsdt_load_file(char *);

/* Save the DSDT to a file */
void		 dsdt_save_file(char *, struct ACPIsdt *);

/* Print out as many fixed tables as possible, given the RSD PTR */
void    	 sdt_print_all(struct ACPIsdt *);

/* Disassemble the AML in the DSDT */
void    	 aml_disassemble(struct ACPIsdt *);

/* Routines for accessing tables in physical memory */
struct ACPIrsdp	*acpi_find_rsd_ptr(void);
void *		 acpi_map_physical(vm_offset_t, size_t);
struct ACPIsdt	*sdt_from_rsdt(struct ACPIsdt *, const char *);
struct ACPIsdt	*dsdt_from_facp(struct FACPbody *);
int		 acpi_checksum(void *, size_t);

/* Command line flags */
extern int	dflag;
extern int	tflag;
extern int	vflag;

#endif	/* !_ACPIDUMP_H_ */
