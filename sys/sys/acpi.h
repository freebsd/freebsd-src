/*-
 * Copyright (c) 1999 Takanori Watanabe <takawata@shidahara1.planet.sci.kobe-u.ac.jp>
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: acpi.h,v 1.9 2000/08/08 14:12:16 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef	_SYS_ACPI_H_
#define	_SYS_ACPI_H_

#include <sys/ioccom.h>

/* Root System Description Pointer */
struct ACPIrsdp {
	u_char		signature[8];
	u_char		sum;
	u_char		oem[6];
	u_char		res;
	u_int32_t	addr;
} __attribute__((packed));

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
} __attribute__((packed));

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
	u_char		reserved4[3];
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
} __attribute__((packed));

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
#define ACPI_GLOBAL_LOCK_OWNED 2
	u_int32_t	flags;		/* bit field */
#define ACPI_FACS_FLAG_S4BIOS_F	1	/* Supports S4BIOS_SEQ */
	char		reserved[40];
} __attribute__((packed));

/*
 * Bits for ACPI registers
 */

/* Power Management 1 Event Regisers (4.7.3.1 Table 4-9, 4-10) */
/* these bits are for status and enable regiser */
#define	ACPI_PM1_TMR_EN			0x0001
#define	ACPI_PM1_GBL_EN			0x0020
#define	ACPI_PM1_PWRBTN_EN		0x0100
#define	ACPI_PM1_SLPBTN_EN		0x0200
#define	ACPI_PM1_RTC_EN			0x0400
#define	ACPI_PM1_ALL_ENABLE_BITS	0x0721
/* these bits are for status regiser only */
#define	ACPI_PM1_BM_STS			0x0010
#define	ACPI_PM1_WAK_STS		0x8000

/* Power Management 1 Control Regisers (4.7.3.2 Table 4-11) */
#define	ACPI_CNT_SCI_EN			0x0001
#define	ACPI_CNT_BM_RLD			0x0002
#define	ACPI_CNT_GBL_RLS		0x0004
#define	ACPI_CNT_SLP_TYPX		0x1c00
#define	ACPI_CNT_SLP_EN			0x2000

#define	ACPI_CNT_SET_SLP_TYP(x)		(x << 10)

/* Power Management Timer (4.7.3.3 Table 4-12) */
/* Not yet */

/* Power Management 2 Control (4.7.3.4 Table 4-13) */
/* Not yet */

/* Processor Register Block (4.7.3.5 Table 4-14, 4-15, 4-16) */
/* Not yet */

#define ACPIIO_ENABLE		_IO('P', 1)
#define ACPIIO_DISABLE		_IO('P', 2)
#define ACPIIO_SETSLPSTATE	_IOW('P', 3, int)

#ifdef _KERNEL
/*
 * Structure for System State Package (7.5.2).
 */
struct acpi_system_state_package {
	struct {
		u_int8_t	slp_typ_a;	/* 3-bit only. */
		u_int8_t	slp_typ_b;	/* (4.7.3.2.1) */
	} mode[6];
};
#define ACPI_UNSUPPORTSLPTYP	0xff	/* unsupported sleeping type */

extern struct ACPIrsdp *acpi_rsdp;	/* ACPI Root System Description Table */

void		 acpi_init_addr_range(void);
void		 acpi_register_addr_range(u_int64_t, u_int64_t, u_int32_t);

/*
 * ACPICA compatibility
 */
#ifdef _IA64
typedef signed char			INT8;
typedef unsigned char			UINT8;
typedef unsigned char			UCHAR;
typedef short				INT16;
typedef unsigned short			UINT16;
typedef int				INT32;
typedef unsigned int			UINT32;
typedef long				INT64;
typedef unsigned long			UINT64;

typedef UINT64				NATIVE_UINT;
typedef INT64				NATIVE_INT;

typedef NATIVE_UINT			ACPI_TBLPTR;
#else	/* !_IA64 */
typedef signed char			INT8;
typedef unsigned char			UINT8;
typedef unsigned char			UCHAR;
typedef short				INT16;
typedef unsigned short			UINT16;
typedef int				INT32;
typedef unsigned int			UINT32;

typedef UINT32				NATIVE_UINT;
typedef INT32				NATIVE_INT;

typedef NATIVE_UINT			ACPI_TBLPTR;
#endif	/* _IA64 */

/* common types */
typedef NATIVE_UINT			ACPI_IO_ADDRESS;
typedef UINT32				ACPI_STATUS;

/* 
 * Exceptions returned by external ACPI interfaces
 */

#define ACPI_SUCCESS(a)			(!(a))
#define ACPI_FAILURE(a)			(a)

#define AE_OK				(ACPI_STATUS) 0x0000
#define AE_CTRL_RETURN_VALUE		(ACPI_STATUS) 0x0001
#define AE_CTRL_PENDING			(ACPI_STATUS) 0x0002
#define AE_CTRL_TERMINATE		(ACPI_STATUS) 0x0003
#define AE_CTRL_TRUE			(ACPI_STATUS) 0x0004
#define AE_CTRL_FALSE			(ACPI_STATUS) 0x0005
#define AE_CTRL_DEPTH			(ACPI_STATUS) 0x0006
#define AE_CTRL_RESERVED		(ACPI_STATUS) 0x0007
#define AE_AML_ERROR			(ACPI_STATUS) 0x0008
#define AE_AML_PARSE			(ACPI_STATUS) 0x0009
#define AE_AML_BAD_OPCODE		(ACPI_STATUS) 0x000A
#define AE_AML_NO_OPERAND		(ACPI_STATUS) 0x000B
#define AE_AML_OPERAND_TYPE		(ACPI_STATUS) 0x000C
#define AE_AML_OPERAND_VALUE		(ACPI_STATUS) 0x000D
#define AE_AML_UNINITIALIZED_LOCAL	(ACPI_STATUS) 0x000E
#define AE_AML_UNINITIALIZED_ARG	(ACPI_STATUS) 0x000F
#define AE_AML_UNINITIALIZED_ELEMENT	(ACPI_STATUS) 0x0010
#define AE_AML_NUMERIC_OVERFLOW		(ACPI_STATUS) 0x0011
#define AE_AML_REGION_LIMIT		(ACPI_STATUS) 0x0012
#define AE_AML_BUFFER_LIMIT		(ACPI_STATUS) 0x0013
#define AE_AML_PACKAGE_LIMIT		(ACPI_STATUS) 0x0014
#define AE_AML_DIVIDE_BY_ZERO		(ACPI_STATUS) 0x0015
#define AE_AML_BAD_NAME			(ACPI_STATUS) 0x0016
#define AE_AML_NAME_NOT_FOUND		(ACPI_STATUS) 0x0017
#define AE_AML_INTERNAL			(ACPI_STATUS) 0x0018
#define AE_AML_RESERVED			(ACPI_STATUS) 0x0019
#define AE_ERROR			(ACPI_STATUS) 0x001A
#define AE_NO_ACPI_TABLES		(ACPI_STATUS) 0x001B
#define AE_NO_NAMESPACE			(ACPI_STATUS) 0x001C
#define AE_NO_MEMORY			(ACPI_STATUS) 0x001D
#define AE_BAD_SIGNATURE		(ACPI_STATUS) 0x001E
#define AE_BAD_HEADER			(ACPI_STATUS) 0x001F
#define AE_BAD_CHECKSUM			(ACPI_STATUS) 0x0020
#define AE_BAD_PARAMETER		(ACPI_STATUS) 0x0021
#define AE_BAD_CHARACTER		(ACPI_STATUS) 0x0022
#define AE_BAD_PATHNAME			(ACPI_STATUS) 0x0023
#define AE_BAD_DATA			(ACPI_STATUS) 0x0024
#define AE_BAD_ADDRESS			(ACPI_STATUS) 0x0025
#define AE_NOT_FOUND			(ACPI_STATUS) 0x0026
#define AE_NOT_EXIST			(ACPI_STATUS) 0x0027
#define AE_EXIST			(ACPI_STATUS) 0x0028
#define AE_TYPE				(ACPI_STATUS) 0x0029
#define AE_NULL_OBJECT			(ACPI_STATUS) 0x002A
#define AE_NULL_ENTRY			(ACPI_STATUS) 0x002B
#define AE_BUFFER_OVERFLOW		(ACPI_STATUS) 0x002C
#define AE_STACK_OVERFLOW		(ACPI_STATUS) 0x002D
#define AE_STACK_UNDERFLOW		(ACPI_STATUS) 0x002E
#define AE_NOT_IMPLEMENTED		(ACPI_STATUS) 0x002F
#define AE_VERSION_MISMATCH		(ACPI_STATUS) 0x0030
#define AE_SUPPORT			(ACPI_STATUS) 0x0031
#define AE_SHARE			(ACPI_STATUS) 0x0032
#define AE_LIMIT			(ACPI_STATUS) 0x0033
#define AE_TIME				(ACPI_STATUS) 0x0034
#define AE_UNKNOWN_STATUS		(ACPI_STATUS) 0x0035
#define ACPI_MAX_STATUS			(ACPI_STATUS) 0x0035
#define ACPI_NUM_STATUS			(ACPI_STATUS) 0x0036

/*
 * ACPICA Osd family functions
 */

#ifdef ACPI_NO_OSDFUNC_INLINE
ACPI_STATUS	 OsdMapMemory(void *, UINT32, void **);
void		 OsdUnMapMemory(void *, UINT32);

UINT8		 OsdIn8(ACPI_IO_ADDRESS);
UINT16		 OsdIn16(ACPI_IO_ADDRESS);
UINT32		 OsdIn32(ACPI_IO_ADDRESS);
void		 OsdOut8(ACPI_IO_ADDRESS, UINT8);
void		 OsdOut16(ACPI_IO_ADDRESS, UINT16);
void		 OsdOut32(ACPI_IO_ADDRESS, UINT32);

ACPI_STATUS	 OsdReadPciCfgByte(UINT32, UINT32 , UINT32 , UINT8 *);
ACPI_STATUS	 OsdReadPciCfgWord(UINT32, UINT32 , UINT32 , UINT16 *);
ACPI_STATUS	 OsdReadPciCfgDword(UINT32, UINT32 , UINT32 , UINT32 *);
ACPI_STATUS	 OsdWritePciCfgByte(UINT32, UINT32 , UINT32 , UINT8);
ACPI_STATUS	 OsdWritePciCfgWord(UINT32, UINT32 , UINT32 , UINT16);
ACPI_STATUS	 OsdWritePciCfgDword(UINT32, UINT32 , UINT32 , UINT32);
#endif	/* ACPI_NO_OSDFUNC_INLINE */

#else	/* !_KERNEL */

void		*acpi_map_physical(vm_offset_t, size_t);
struct ACPIrsdp	*acpi_find_rsd_ptr(void);
int		 acpi_checksum(void *, size_t);
struct ACPIsdt	*acpi_map_sdt(vm_offset_t);
void		 acpi_print_rsd_ptr(struct ACPIrsdp *);
void		 acpi_print_sdt(struct ACPIsdt *);
void		 acpi_print_rsdt(struct ACPIsdt *);
void		 acpi_print_facp(struct FACPbody *);
void		 acpi_print_dsdt(struct ACPIsdt *);

#endif	/* _KERNEL */

#endif	/* _SYS_ACPI_H_ */
