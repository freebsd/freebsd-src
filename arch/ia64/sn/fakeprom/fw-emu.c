/*
 * PAL & SAL emulation.
 *
 * Copyright (C) 1998-2000 Hewlett-Packard Co
 * Copyright (C) 1998-2000 David Mosberger-Tang <davidm@hpl.hp.com>
 *
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc.  All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */
#include <linux/config.h>
#include <linux/efi.h>
#include <asm/pal.h>
#include <asm/sal.h>
#include <asm/sn/sn_sal.h>
#include <asm/processor.h>
#include <asm/sn/sn_cpuid.h>
#ifdef SGI_SN2
#include <asm/sn/sn2/addrs.h>
#include <asm/sn/sn2/shub_mmr.h>
#endif
#include <linux/acpi.h>
#include <acpi/acpi.h>
#include <acpi/actbl.h>
#include "fpmem.h"

#define RSDP_NAME               "RSDP"
#define RSDP_SIG                "RSD PTR "  /* RSDT Pointer signature */
#define APIC_SIG                "APIC"      /* Multiple APIC Description Table */
#define DSDT_SIG                "DSDT"      /* Differentiated System Description Table */
#define FADT_SIG                "FACP"      /* Fixed ACPI Description Table */
#define FACS_SIG                "FACS"      /* Firmware ACPI Control Structure */
#define PSDT_SIG                "PSDT"      /* Persistent System Description Table */
#define RSDT_SIG                "RSDT"      /* Root System Description Table */
#define XSDT_SIG                "XSDT"      /* Extended  System Description Table */
#define SSDT_SIG                "SSDT"      /* Secondary System Description Table */
#define SBST_SIG                "SBST"      /* Smart Battery Specification Table */
#define SPIC_SIG                "SPIC"      /* IOSAPIC table */
#define SRAT_SIG                "SRAT"      /* SRAT table */
#define SLIT_SIG                "SLIT"      /* SLIT table */
#define BOOT_SIG                "BOOT"      /* Boot table */
#define ACPI_SRAT_REVISION 1
#define ACPI_SLIT_REVISION 1
#define ACPI_FADT_REVISION 3
#define ACPI_DSDT_REVISION 1

#define OEMID			"SGI"
#ifdef SGI_SN2
#define PRODUCT			"SN2"
#endif

#define MB	(1024*1024UL)
#define GB	(MB*1024UL)
#define BOOT_PARAM_ADDR 0x40000
#define MAX(i,j)		((i) > (j) ? (i) : (j))
#define MIN(i,j)		((i) < (j) ? (i) : (j))
#define ABS(i)			((i) > 0   ? (i) : -(i))
#define ALIGN8(p)		(((long)(p) +7) & ~7)

#define NUM_EFI_DESCS		2

#define RSDP_CHECKSUM_LENGTH	20

typedef union ia64_nasid_va {
        struct {
#if defined(SGI_SN2)
                unsigned long off   : 36;       /* intra-region offset */
		unsigned long attr  :  2;
		unsigned long nasid : 11;	/* NASID */
		unsigned long off2  : 12;	/* fill */
                unsigned long reg   :  3;       /* region number */
#endif
        } f;
        unsigned long l;
        void *p;
} ia64_nasid_va;

typedef struct {
	unsigned long	pc;
	unsigned long	gp;
} func_ptr_t;
 
#define IS_VIRTUAL_MODE() 	 ({struct ia64_psr psr; asm("mov %0=psr" : "=r"(psr)); psr.dt;})
#define ADDR_OF(p)		(IS_VIRTUAL_MODE() ? ((void*)((long)(p)+PAGE_OFFSET)) : ((void*) (p)))

#if defined(SGI_SN2)
#define __fwtab_pa(n,x)		({ia64_nasid_va _v; _v.l = (long) (x); _v.f.nasid = (x) ? (n) : 0; _v.f.reg = 0; _v.f.attr = 3; _v.l;})
#endif

/*
 * The following variables are passed thru registersfrom the configuration file and
 * are set via the _start function.
 */
long		base_nasid;
long		num_cpus;
long		bsp_entry_pc;
long		num_nodes;
func_ptr_t	ap_entry;
char		nasid_present[MAX_NASID];


extern void pal_emulator(void);
static efi_runtime_services_t    *efi_runtime_p;
static char fw_mem[(  sizeof(efi_system_table_t)
		    + sizeof(efi_runtime_services_t)
		    + NUM_EFI_DESCS*sizeof(efi_config_table_t)
		    + sizeof(struct ia64_sal_systab)
		    + sizeof(struct ia64_sal_desc_entry_point)
		    + sizeof(struct ia64_sal_desc_ap_wakeup)
		    + sizeof(struct ia64_sal_desc_platform_feature)
		    + sizeof(struct acpi20_table_rsdp)
		    + sizeof(struct acpi_table_xsdt)
		    + sizeof(struct acpi_table_header) /* dummy DSDT */
		    + sizeof(struct fadt_descriptor_rev2)
		    + sizeof(struct acpi_table_slit)
		    +   MAX_SN_NODES*MAX_SN_NODES+8
		    + sizeof(struct acpi_table_madt)
		    +   16*MAX_CPUS
		    + (1+8*MAX_SN_NODES)*(sizeof(efi_memory_desc_t))
		    + sizeof(struct acpi_table_srat)
		    +   MAX_CPUS*sizeof(struct acpi_table_processor_affinity)
		    +   MAX_SN_NODES*sizeof(struct acpi_table_memory_affinity)
		    + sizeof(ia64_sal_desc_ptc_t) +
		    + MAX_SN_NODES*sizeof(ia64_sal_ptc_domain_info_t) +
		    + MAX_CPUS*sizeof(ia64_sal_ptc_domain_proc_entry_t) +
		    + 1024)] __attribute__ ((aligned (8)));


static efi_status_t
efi_get_time (efi_time_t *tm, efi_time_cap_t *tc)
{
	if (tm) {
		memset(tm, 0, sizeof(*tm));
		tm->year = 2000;
		tm->month = 2;
		tm->day = 13;
		tm->hour = 10;
		tm->minute = 11;
		tm->second = 12;
	}

	if (tc) {
		tc->resolution = 10;
		tc->accuracy = 12;
		tc->sets_to_zero = 1;
	}

	return EFI_SUCCESS;
}

static void
efi_reset_system (int reset_type, efi_status_t status, unsigned long data_size, efi_char16_t *data)
{
	while(1);	/* Is there a pseudo-op to stop medusa */
}

static efi_status_t
efi_success (void)
{
	return EFI_SUCCESS;
}

static efi_status_t
efi_unimplemented (void)
{
	return EFI_UNSUPPORTED;
}

#ifdef SGI_SN2

#undef cpu_physical_id
#define cpu_physical_id(cpuid)                  ((ia64_get_lid() >> 16) & 0xffff)

void
fprom_send_cpei(void) {
        long            *p, val;
        long            physid;
        long            nasid, slice;

        physid = cpu_physical_id(0);
        nasid = cpu_physical_id_to_nasid(physid);
        slice = cpu_physical_id_to_slice(physid);

        p = (long*)GLOBAL_MMR_ADDR(nasid, SH_IPI_INT);
        val =   (1UL<<SH_IPI_INT_SEND_SHFT) |
                (physid<<SH_IPI_INT_PID_SHFT) |
                ((long)0<<SH_IPI_INT_TYPE_SHFT) |
                ((long)0x1e<<SH_IPI_INT_IDX_SHFT) |
                (0x000feeUL<<SH_IPI_INT_BASE_SHFT);
        *p = val;

}
#endif


static long
sal_emulator (long index, unsigned long in1, unsigned long in2,
	      unsigned long in3, unsigned long in4, unsigned long in5,
	      unsigned long in6, unsigned long in7)
{
	register long r9 asm ("r9") = 0;
	register long r10 asm ("r10") = 0;
	register long r11 asm ("r11") = 0;
	long status;

	/*
	 * Don't do a "switch" here since that gives us code that
	 * isn't self-relocatable.
	 */
	status = 0;
	if (index == SAL_FREQ_BASE) {
		switch (in1) {
		      case SAL_FREQ_BASE_PLATFORM:
			     /* slow down the clock on large systems to reduce the interrupt rate */
			if (num_cpus < 32)
				r9 = 500000000UL;
			else
				r9 = 5000000000UL;
			break;

		      case SAL_FREQ_BASE_INTERVAL_TIMER:
			/*
			 * Is this supposed to be the cr.itc frequency
			 * or something platform specific?  The SAL
			 * doc ain't exactly clear on this...
			 */
			r9 = 700000000;
			break;

		      case SAL_FREQ_BASE_REALTIME_CLOCK:
			r9 = 50000000;
			break;

		      default:
			status = -1;
			break;
		}
	} else if (index == SAL_SET_VECTORS) {
		if (in1 == SAL_VECTOR_OS_BOOT_RENDEZ) {
			func_ptr_t	*fp;
			fp = ADDR_OF(&ap_entry);
			fp->pc = in2;
			fp->gp = in3;
		} else if (in1 == SAL_VECTOR_OS_MCA || in1 == SAL_VECTOR_OS_INIT) {
		} else {
			status = -1;
		}
		;
	} else if (index == SAL_GET_STATE_INFO) {
		;
	} else if (index == SAL_GET_STATE_INFO_SIZE) {
		r9 = 10000;
		;
	} else if (index == SAL_CLEAR_STATE_INFO) {
		;
	} else if (index == SAL_MC_RENDEZ) {
		;
	} else if (index == SAL_MC_SET_PARAMS) {
		;
	} else if (index == SAL_CACHE_FLUSH) {
		;
	} else if (index == SAL_CACHE_INIT) {
		;
	} else if (index == SAL_UPDATE_PAL) {
		;
#ifdef SGI_SN2
	} else if (index == SN_SAL_LOG_CE) {
#ifdef ajmtestcpei
		fprom_send_cpei();
#else /* ajmtestcpei */
		;
#endif /* ajmtestcpei */
#endif
	} else if (index == SN_SAL_PROBE) {
		r9 = 0UL;
		if (in2 == 4) {
			r9 = *(unsigned *)in1;
			if (r9 == -1) {
				status = 1;
			}
		} else if (in2 == 2) {
			r9 = *(unsigned short *)in1;
			if (r9 == -1) {
				status = 1;
			}
		} else if (in2 == 1) {
			r9 = *(unsigned char *)in1;
			if (r9 == -1) {
				status = 1;
			}
		} else if (in2 == 8) {
			r9 = *(unsigned long *)in1;
			if (r9 == -1) {
				status = 1;
			}
		} else {
			status = 2;
		}
	} else if (index == SN_SAL_GET_KLCONFIG_ADDR) {
		r9 = 0x30000;
	} else if (index == SN_SAL_CONSOLE_PUTC) {
		status = -1;
	} else if (index == SN_SAL_CONSOLE_GETC) {
		status = -1;
	} else if (index == SN_SAL_CONSOLE_POLL) {
		status = -1;
	} else if (index == SN_SAL_SYSCTL_IOBRICK_MODULE_GET) {
		status = -1;
	} else {
		status = -1;
	}

	asm volatile ("" :: "r"(r9), "r"(r10), "r"(r11));
	return status;
}


/*
 * This is here to work around a bug in egcs-1.1.1b that causes the
 * compiler to crash (seems like a bug in the new alias analysis code.
 */
void *
id (long addr)
{
	return (void *) addr;
}


/*
 * Fix the addresses in a function pointer by adding base node address
 * to pc & gp.
 */
void
fix_function_pointer(void *fp)
{
	func_ptr_t	*_fp;

	_fp = fp;
	_fp->pc = __fwtab_pa(base_nasid, _fp->pc);
	_fp->gp = __fwtab_pa(base_nasid, _fp->gp);
}

void
fix_virt_function_pointer(void **fptr)
{
        func_ptr_t      *fp;
	long		*p;

	p = (long*)fptr;
        fp = *fptr;
        fp->pc = fp->pc | PAGE_OFFSET;
        fp->gp = fp->gp | PAGE_OFFSET;
	*p |= PAGE_OFFSET;
}


int
efi_set_virtual_address_map(void)
{
        efi_runtime_services_t            *runtime;

        runtime = efi_runtime_p;
        fix_virt_function_pointer((void**)&runtime->get_time);
        fix_virt_function_pointer((void**)&runtime->set_time);
        fix_virt_function_pointer((void**)&runtime->get_wakeup_time);
        fix_virt_function_pointer((void**)&runtime->set_wakeup_time);
        fix_virt_function_pointer((void**)&runtime->set_virtual_address_map);
        fix_virt_function_pointer((void**)&runtime->get_variable);
        fix_virt_function_pointer((void**)&runtime->get_next_variable);
        fix_virt_function_pointer((void**)&runtime->set_variable);
        fix_virt_function_pointer((void**)&runtime->get_next_high_mono_count);
        fix_virt_function_pointer((void**)&runtime->reset_system);
        return EFI_SUCCESS;;
}

void
acpi_table_initx(struct acpi_table_header *p, char *sig, int siglen, int revision, int oem_revision)
{
	memcpy(p->signature, sig, siglen);
	memcpy(p->oem_id, OEMID, 6);
	memcpy(p->oem_table_id, sig, 4);
	memcpy(p->oem_table_id+4, PRODUCT, 4);
	p->revision = revision;
	p->oem_revision = (revision<<16) + oem_revision;
	memcpy(p->asl_compiler_id, "FPRM", 4);
	p->asl_compiler_revision = 1;
}

void
acpi_checksum(struct acpi_table_header *p, int length)
{
	u8	*cp, *cpe, checksum;

	p->checksum = 0;
	p->length = length;
	checksum = 0;
	for (cp=(u8*)p, cpe=cp+p->length; cp<cpe; cp++)
		checksum += *cp;
	p->checksum = -checksum;
}

void
acpi_checksum_rsdp20(struct acpi20_table_rsdp *p, int length)
{
	u8	*cp, *cpe, checksum;

	p->checksum = 0;
	p->ext_checksum = 0;
	p->length = length;
	checksum = 0;
	for (cp=(u8*)p, cpe=cp+20; cp<cpe; cp++)
		checksum += *cp;
	p->checksum = -checksum;

	checksum = 0;
	for (cp=(u8*)p, cpe=cp+length; cp<cpe; cp++)
		checksum += *cp;
	p->ext_checksum = -checksum;
}

int
is_nasid_present(int nasid)
{
	int	cnode;
	for (cnode=0; cnode<num_nodes; cnode++)
		if (GetNasid(cnode) == nasid)
			return 1;
	return 0;
}

void
sys_fw_init (const char *args, int arglen)
{
	/*
	 * Use static variables to keep from overflowing the RSE stack
	 */
	static efi_system_table_t *efi_systab;
	static efi_runtime_services_t *efi_runtime;
	static efi_config_table_t *efi_tables;
	static ia64_sal_desc_ptc_t *sal_ptc;
	static ia64_sal_ptc_domain_info_t *sal_ptcdi;
	static ia64_sal_ptc_domain_proc_entry_t *sal_ptclid;
	static struct acpi20_table_rsdp *acpi20_rsdp;
	static struct acpi_table_xsdt *acpi_xsdt;
	static struct acpi_table_slit *acpi_slit;
	static struct acpi_table_madt *acpi_madt;
	static struct acpi_table_lsapic *lsapic20;
	static struct acpi_table_header *acpi_dsdt;
	static struct fadt_descriptor_rev2 *acpi_fadt;
	static struct ia64_sal_systab *sal_systab;
	static struct acpi_table_srat *acpi_srat;
	static struct acpi_table_processor_affinity *srat_cpu_affinity;
	static efi_memory_desc_t *efi_memmap, *md;
	static unsigned long *pal_desc, *sal_desc;
	static struct ia64_sal_desc_entry_point *sal_ed;
	static struct ia64_boot_param *bp;
	static struct ia64_sal_desc_ap_wakeup *sal_apwake;
	static struct ia64_sal_desc_platform_feature *sal_feature;
	static unsigned char checksum;
	static char *cp, *cmd_line, *vendor;
	static void *ptr;
	static int mdsize, domain, last_domain ;
	static int i, j, cnode, max_nasid, nasid, cpu, num_memmd, cpus_found;

	num_nodes = GetNumNodes();
	num_cpus = GetNumCpus();
	for (max_nasid=0, cnode=0; cnode<num_nodes; cnode++)
		max_nasid = MAX(max_nasid, GetNasid(cnode));

	//max_nasid = 0;
	//num_nodes = 1;

	memset(fw_mem, 0, sizeof(fw_mem));

	pal_desc = (unsigned long *) &pal_emulator;
	sal_desc = (unsigned long *) &sal_emulator;
	fix_function_pointer(&pal_emulator);
	fix_function_pointer(&sal_emulator);

	/* Align this to 16 bytes, probably EFI does this  */
	mdsize = (sizeof(efi_memory_desc_t) + 15) & ~15 ;

	cp = fw_mem;
	efi_systab  = (void *) cp; cp += ALIGN8(sizeof(*efi_systab));
	efi_runtime_p = efi_runtime = (void *) cp; cp += ALIGN8(sizeof(*efi_runtime));
	efi_tables  = (void *) cp; cp += ALIGN8(NUM_EFI_DESCS*sizeof(*efi_tables));
	sal_systab  = (void *) cp; cp += ALIGN8(sizeof(*sal_systab));
	sal_ed      = (void *) cp; cp += ALIGN8(sizeof(*sal_ed));
	sal_ptc     = (void *) cp; cp += ALIGN8(sizeof(*sal_ptc));
	sal_apwake  = (void *) cp; cp += ALIGN8(sizeof(*sal_apwake));
	sal_feature = (void *) cp; cp += ALIGN8(sizeof(*sal_feature));
	acpi20_rsdp = (void *) cp; cp += ALIGN8(sizeof(*acpi20_rsdp));
	acpi_xsdt   = (void *) cp; cp += ALIGN8(sizeof(*acpi_xsdt) + 64);
			/* save space for more OS defined table pointers. */

	acpi_dsdt   = (void *) cp; cp += ALIGN8(sizeof(*acpi_dsdt));
	acpi_fadt   = (void *) cp; cp += ALIGN8(sizeof(*acpi_fadt));
	acpi_slit   = (void *) cp; cp += ALIGN8(sizeof(*acpi_slit) + 8 + (max_nasid+1)*(max_nasid+1));
	acpi_madt   = (void *) cp; cp += ALIGN8(sizeof(*acpi_madt) + sizeof(struct acpi_table_lsapic) * (num_cpus+1));
	acpi_srat   = (void *) cp; cp += ALIGN8(sizeof(struct acpi_table_srat));
	cp         += sizeof(struct acpi_table_processor_affinity)*num_cpus + sizeof(struct acpi_table_memory_affinity)*num_nodes;
	vendor 	    = (char *) cp; cp += ALIGN8(40);
	efi_memmap  = (void *) cp; cp += ALIGN8(8*32*sizeof(*efi_memmap));
	sal_ptcdi   = (void *) cp; cp += ALIGN8(CPUS_PER_FSB*(1+num_nodes)*sizeof(*sal_ptcdi));
	sal_ptclid  = (void *) cp; cp += ALIGN8(((3+num_cpus)*sizeof(*sal_ptclid)+7)/8*8);
	cmd_line    = (void *) cp;

	if (args) {
		if (arglen >= 1024)
			arglen = 1023;
		memcpy(cmd_line, args, arglen);
	} else {
		arglen = 0;
	}
	cmd_line[arglen] = '\0';
	/* 
	 * For now, just bring up bash.
	 * If you want to execute all the startup scripts, delete the "init=..".
	 * You can also edit this line to pass other arguments to the kernel.
	 *    Note: disable kernel text replication.
	 */
	strcpy(cmd_line, "init=/bin/bash console=ttyS0");

	memset(efi_systab, 0, sizeof(efi_systab));
	efi_systab->hdr.signature = EFI_SYSTEM_TABLE_SIGNATURE;
	efi_systab->hdr.revision  = EFI_SYSTEM_TABLE_REVISION;
	efi_systab->hdr.headersize = sizeof(efi_systab->hdr);
	efi_systab->fw_vendor = __fwtab_pa(base_nasid, vendor);
	efi_systab->fw_revision = 1;
	efi_systab->runtime = __fwtab_pa(base_nasid, efi_runtime);
	efi_systab->nr_tables = 2;
	efi_systab->tables = __fwtab_pa(base_nasid, efi_tables);
	memcpy(vendor, "S\0i\0l\0i\0c\0o\0n\0-\0G\0r\0a\0p\0h\0i\0c\0s\0\0", 40);

	efi_runtime->hdr.signature = EFI_RUNTIME_SERVICES_SIGNATURE;
	efi_runtime->hdr.revision = EFI_RUNTIME_SERVICES_REVISION;
	efi_runtime->hdr.headersize = sizeof(efi_runtime->hdr);
	efi_runtime->get_time = __fwtab_pa(base_nasid, &efi_get_time);
	efi_runtime->set_time = __fwtab_pa(base_nasid, &efi_unimplemented);
	efi_runtime->get_wakeup_time = __fwtab_pa(base_nasid, &efi_unimplemented);
	efi_runtime->set_wakeup_time = __fwtab_pa(base_nasid, &efi_unimplemented);
	efi_runtime->set_virtual_address_map = __fwtab_pa(base_nasid, &efi_set_virtual_address_map);
	efi_runtime->get_variable = __fwtab_pa(base_nasid, &efi_unimplemented);
	efi_runtime->get_next_variable = __fwtab_pa(base_nasid, &efi_unimplemented);
	efi_runtime->set_variable = __fwtab_pa(base_nasid, &efi_unimplemented);
	efi_runtime->get_next_high_mono_count = __fwtab_pa(base_nasid, &efi_unimplemented);
	efi_runtime->reset_system = __fwtab_pa(base_nasid, &efi_reset_system);

	efi_tables->guid = SAL_SYSTEM_TABLE_GUID;
	efi_tables->table = __fwtab_pa(base_nasid, sal_systab);
	efi_tables++;
	efi_tables->guid = ACPI_20_TABLE_GUID;
	efi_tables->table = __fwtab_pa(base_nasid, acpi20_rsdp);
	efi_tables++;

	fix_function_pointer(&efi_unimplemented);
	fix_function_pointer(&efi_get_time);
	fix_function_pointer(&efi_success);
	fix_function_pointer(&efi_reset_system);
	fix_function_pointer(&efi_set_virtual_address_map);


	/* fill in the ACPI20 system table - has a pointer to the ACPI table header */
	memcpy(acpi20_rsdp->signature, "RSD PTR ", 8);
	acpi20_rsdp->xsdt_address = (u64)__fwtab_pa(base_nasid, acpi_xsdt);
	acpi20_rsdp->revision = 2;
	acpi_checksum_rsdp20(acpi20_rsdp, sizeof(struct acpi20_table_rsdp));

	/* Set up the XSDT table  - contains pointers to the other ACPI tables */
	acpi_table_initx(&acpi_xsdt->header, XSDT_SIG, 4, 1, 1);
	acpi_xsdt->entry[0] = __fwtab_pa(base_nasid, acpi_madt);
	acpi_xsdt->entry[1] = __fwtab_pa(base_nasid, acpi_slit);
	acpi_xsdt->entry[2] = __fwtab_pa(base_nasid, acpi_srat);
	acpi_xsdt->entry[3] = __fwtab_pa(base_nasid, acpi_fadt);
	acpi_xsdt->entry[4] = __fwtab_pa(base_nasid, acpi_dsdt);
	acpi_checksum(&acpi_xsdt->header, sizeof(struct acpi_table_xsdt) + 32);

	/* Set up the APIC table */
	acpi_table_initx(&acpi_madt->header, APIC_SIG, 4, 1, 1);
	lsapic20 = (struct acpi_table_lsapic*) (acpi_madt + 1);
	for (cnode=0; cnode<num_nodes; cnode++) {
		nasid = GetNasid(cnode);
		for(cpu=0; cpu<CPUS_PER_NODE; cpu++) {
			if (!IsCpuPresent(cnode, cpu))
				continue;
			lsapic20->header.type = ACPI_MADT_LSAPIC;
			lsapic20->header.length = sizeof(struct acpi_table_lsapic);
			lsapic20->acpi_id = cnode*4+cpu;
			lsapic20->flags.enabled = 1;
#if defined(SGI_SN2)
			lsapic20->eid = nasid&0xffff;
			lsapic20->id = (cpu<<4) | (nasid>>8);
#endif
			lsapic20 = (struct acpi_table_lsapic*) ((long)lsapic20+sizeof(struct acpi_table_lsapic));
		}
	}
	acpi_checksum(&acpi_madt->header, (char*)lsapic20 - (char*)acpi_madt);

	/* Set up the SRAT table */
	acpi_table_initx(&acpi_srat->header, SRAT_SIG, 4, ACPI_SRAT_REVISION, 1);
	ptr = acpi_srat+1;

	ptr = build_memory_srat(ptr);

	/* Build processor SRAT */
	for (cnode=0; cnode<num_nodes; cnode++) {
		nasid = GetNasid(cnode);
		for(cpu=0; cpu<CPUS_PER_NODE; cpu++) {
			if (!IsCpuPresent(cnode, cpu))
				continue;
			srat_cpu_affinity = ptr;
			ptr = srat_cpu_affinity + 1;
			srat_cpu_affinity->header.type = ACPI_SRAT_PROCESSOR_AFFINITY;
			srat_cpu_affinity->header.length = sizeof(struct acpi_table_processor_affinity);
			srat_cpu_affinity->proximity_domain = PROXIMITY_DOMAIN(nasid);
			srat_cpu_affinity->flags.enabled = 1;
#if defined(SGI_SN2)
			srat_cpu_affinity->lsapic_eid = nasid&0xffff;
			srat_cpu_affinity->apic_id = (cpu<<4) | (nasid>>8);
#endif
		}
	}
	acpi_checksum(&acpi_srat->header, (char*)ptr - (char*)acpi_srat);

	acpi_table_initx(acpi_dsdt, DSDT_SIG, 4, ACPI_DSDT_REVISION, 1);
	acpi_checksum(acpi_dsdt, sizeof(*acpi_dsdt));

	/* Set up the FADT table */
	acpi_table_initx((struct acpi_table_header *)&acpi_fadt->header, FADT_SIG, 4, ACPI_FADT_REVISION, 1);
	/*
	 * We don't have legacy PC keyboard support etc.
	 */
	acpi_fadt->iapc_boot_arch = 0;
	acpi_fadt->Xdsdt = (u64)__fwtab_pa(base_nasid, acpi_dsdt);
	acpi_checksum((struct acpi_table_header *)&acpi_fadt->header,
		      sizeof(*acpi_fadt));

	/* Set up the SLIT table */
	acpi_table_initx(&acpi_slit->header, SLIT_SIG, 4, ACPI_SLIT_REVISION, 1);
	acpi_slit->localities = PROXIMITY_DOMAIN(max_nasid)+1;
	cp=acpi_slit->entry;
	memset(cp, 255, acpi_slit->localities*acpi_slit->localities);

	for (i=0; i<=max_nasid; i++)
		nasid_present[i] = is_nasid_present(i);
	for (i=0; i<=max_nasid; i++)
		for (j=0; j<=max_nasid; j++)
			if (nasid_present[i] && nasid_present[j])
				*(cp+PROXIMITY_DOMAIN(i)*acpi_slit->localities+PROXIMITY_DOMAIN(j)) = 10 + MIN(254, 5*ABS(i-j));

	cp = acpi_slit->entry + acpi_slit->localities*acpi_slit->localities;
	acpi_checksum(&acpi_slit->header, cp - (char*)acpi_slit);


	/* fill in the SAL system table: */
	memcpy(sal_systab->signature, "SST_", 4);
	sal_systab->size = sizeof(*sal_systab);
	sal_systab->sal_rev_minor = 1;
	sal_systab->sal_rev_major = 0;
	sal_systab->entry_count = 4;
	sal_systab->sal_b_rev_major = 0x1; /* set the SN SAL rev to */
	sal_systab->sal_b_rev_minor = 0x0; /* 1.00 */

	strcpy(sal_systab->oem_id, "SGI");
	strcpy(sal_systab->product_id, "SN2");

	/* fill in an entry point: */	
	sal_ed->type = SAL_DESC_ENTRY_POINT;
	sal_ed->pal_proc = __fwtab_pa(base_nasid, pal_desc[0]);
	sal_ed->sal_proc = __fwtab_pa(base_nasid, sal_desc[0]);
	sal_ed->gp = __fwtab_pa(base_nasid, sal_desc[1]);

	/* kludge the PTC domain info */
	sal_ptc->type = SAL_DESC_PTC;
	sal_ptc->num_domains = 0;
	sal_ptc->domain_info = __fwtab_pa(base_nasid, sal_ptcdi);
	cpus_found = 0;
	last_domain = -1;
	sal_ptcdi--;
	for (cnode=0; cnode<num_nodes; cnode++) {
		nasid = GetNasid(cnode);
		for(cpu=0; cpu<CPUS_PER_NODE; cpu++) {
			if (IsCpuPresent(cnode, cpu)) {
				domain = cnode*CPUS_PER_NODE + cpu/CPUS_PER_FSB;
				if (domain != last_domain) {
					sal_ptc->num_domains++;
					sal_ptcdi++;
					sal_ptcdi->proc_count = 0;
					sal_ptcdi->proc_list = __fwtab_pa(base_nasid, sal_ptclid);
					last_domain = domain;
				}
				sal_ptcdi->proc_count++;
				sal_ptclid->id = nasid;
				sal_ptclid->eid = cpu;
				sal_ptclid++;
				cpus_found++;
			}
		}
	}

	if (cpus_found != num_cpus)
		FPROM_BUG();

	/* Make the AP WAKEUP entry */
	sal_apwake->type = SAL_DESC_AP_WAKEUP;
	sal_apwake->mechanism = IA64_SAL_AP_EXTERNAL_INT;
	sal_apwake->vector = 18;

	/* Setup the SAL feature descriptor */
	sal_feature->type = SAL_DESC_PLATFORM_FEATURE;
	sal_feature->feature_mask = IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT;

	for (checksum=0, cp=(char*)sal_systab; cp < (char *)efi_memmap; ++cp)
		checksum += *cp;
	sal_systab->checksum = -checksum;

	/* If the checksum is correct, the kernel tries to use the
	 * table. We dont build enough table & the kernel aborts.
	 * Note that the PROM hasd thhhe same problem!!
	 */

	md = &efi_memmap[0];
	num_memmd = build_efi_memmap((void *)md, mdsize) ;

	bp = (struct ia64_boot_param*) __fwtab_pa(base_nasid, BOOT_PARAM_ADDR);
	bp->efi_systab = __fwtab_pa(base_nasid, &fw_mem);
	bp->efi_memmap = __fwtab_pa(base_nasid, efi_memmap);
	bp->efi_memmap_size = num_memmd*mdsize;
	bp->efi_memdesc_size = mdsize;
	bp->efi_memdesc_version = 0x101;
	bp->command_line = __fwtab_pa(base_nasid, cmd_line);
	bp->console_info.num_cols = 80;
	bp->console_info.num_rows = 25;
	bp->console_info.orig_x = 0;
	bp->console_info.orig_y = 24;
	bp->fpswa = 0;
}
