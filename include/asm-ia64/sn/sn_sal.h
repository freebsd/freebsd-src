#ifndef _ASM_IA64_SN_SN_SAL_H
#define _ASM_IA64_SN_SN_SAL_H

/*
 * System Abstraction Layer definitions for IA64
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All rights reserved.
 */


#include <linux/config.h>
#include <asm/sal.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/arch.h>
#include <asm/sn/nodepda.h>


// SGI Specific Calls
#define  SN_SAL_POD_MODE                           0x02000001
#define  SN_SAL_SYSTEM_RESET                       0x02000002
#define  SN_SAL_PROBE                              0x02000003
#define  SN_SAL_GET_MASTER_NASID                   0x02000004
#define	 SN_SAL_GET_KLCONFIG_ADDR		   0x02000005
#define  SN_SAL_LOG_CE				   0x02000006
#define  SN_SAL_REGISTER_CE			   0x02000007
#define  SN_SAL_GET_PARTITION_ADDR		   0x02000009
#define  SN_SAL_XP_ADDR_REGION			   0x0200000f
#define  SN_SAL_NO_FAULT_ZONE_VIRTUAL		   0x02000010
#define  SN_SAL_NO_FAULT_ZONE_PHYSICAL		   0x02000011
#define  SN_SAL_PRINT_ERROR			   0x02000012
#define  SN_SAL_CONSOLE_PUTC                       0x02000021
#define  SN_SAL_CONSOLE_GETC                       0x02000022
#define  SN_SAL_CONSOLE_PUTS                       0x02000023
#define  SN_SAL_CONSOLE_GETS                       0x02000024
#define  SN_SAL_CONSOLE_GETS_TIMEOUT               0x02000025
#define  SN_SAL_CONSOLE_POLL                       0x02000026
#define  SN_SAL_CONSOLE_INTR                       0x02000027
#define  SN_SAL_CONSOLE_PUTB			   0x02000028
#define  SN_SAL_CONSOLE_XMIT_CHARS		   0x0200002a
#define  SN_SAL_CONSOLE_READC			   0x0200002b
#define  SN_SAL_SYSCTL_MODID_GET	           0x02000031
#define  SN_SAL_SYSCTL_GET                         0x02000032
#define  SN_SAL_SYSCTL_IOBRICK_MODULE_GET          0x02000033
#define  SN_SAL_SYSCTL_IO_PORTSPEED_GET            0x02000035
#define  SN_SAL_SYSCTL_SLAB_GET                    0x02000036
#define  SN_SAL_BUS_CONFIG		   	   0x02000037
#define  SN_SAL_SYS_SERIAL_GET			   0x02000038
#define  SN_SAL_PARTITION_SERIAL_GET		   0x02000039
#define  SN_SAL_SYSCTL_PARTITION_GET		   0x0200003a
#define  SN_SAL_SYSTEM_POWER_DOWN		   0x0200003b
#define  SN_SAL_GET_MASTER_BASEIO_NASID		   0x0200003c
#define  SN_SAL_COHERENCE                          0x0200003d
#define  SN_SAL_MEMPROTECT                         0x0200003e
#define  SN_SAL_SYSCTL_FRU_CAPTURE		   0x0200003f


/*
 * Service-specific constants
 */

/* Console interrupt manipulation */
	/* action codes */
#define SAL_CONSOLE_INTR_OFF    0       /* turn the interrupt off */
#define SAL_CONSOLE_INTR_ON     1       /* turn the interrupt on */
#define SAL_CONSOLE_INTR_STATUS 2	/* retrieve the interrupt status */
	/* interrupt specification & status return codes */
#define SAL_CONSOLE_INTR_XMIT	1	/* output interrupt */
#define SAL_CONSOLE_INTR_RECV	2	/* input interrupt */


/*
 * SN_SAL_GET_PARTITION_ADDR return constants
 */
#define SALRET_MORE_PASSES	1
#define SALRET_OK		0
#define SALRET_INVALID_ARG	-2
#define SALRET_ERROR		-3


/**
 * sn_sal_rev_major - get the major SGI SAL revision number
 *
 * The SGI PROM stores its version in sal_[ab]_rev_(major|minor).
 * This routine simply extracts the major value from the
 * @ia64_sal_systab structure constructed by ia64_sal_init().
 */
static inline int
sn_sal_rev_major(void)
{
	struct ia64_sal_systab *systab = efi.sal_systab;

	return (int)systab->sal_b_rev_major;
}

/**
 * sn_sal_rev_minor - get the minor SGI SAL revision number
 *
 * The SGI PROM stores its version in sal_[ab]_rev_(major|minor).
 * This routine simply extracts the minor value from the
 * @ia64_sal_systab structure constructed by ia64_sal_init().
 */
static inline int
sn_sal_rev_minor(void)
{
	struct ia64_sal_systab *systab = efi.sal_systab;
	
	return (int)systab->sal_b_rev_minor;
}

/*
 * Specify the minimum PROM revsion required for this kernel.
 * Note that they're stored in hex format...
 */
#define SN_SAL_MIN_MAJOR	0x1  /* SN2 kernels need at least PROM 1.0 */
#define SN_SAL_MIN_MINOR	0x0

u64 ia64_sn_probe_io_slot(long paddr, long size, void *data_ptr);

/*
 * Returns the master console nasid, if the call fails, return an illegal
 * value.
 */
static inline u64
ia64_sn_get_console_nasid(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL(ret_stuff, SN_SAL_GET_MASTER_NASID, 0, 0, 0, 0, 0, 0, 0);

	if (ret_stuff.status < 0)
		return ret_stuff.status;

	/* Master console nasid is in 'v0' */
	return ret_stuff.v0;
}

/*
 * Returns the master baseio nasid, if the call fails, return an illegal
 * value.
 */
static inline u64
ia64_sn_get_master_baseio_nasid(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL(ret_stuff, SN_SAL_GET_MASTER_BASEIO_NASID, 0, 0, 0, 0, 0, 0, 0);

	if (ret_stuff.status < 0)
		return ret_stuff.status;

	/* Master baseio nasid is in 'v0' */
	return ret_stuff.v0;
}

static inline u64
ia64_sn_get_klconfig_addr(nasid_t nasid)
{
	struct ia64_sal_retval ret_stuff;
	extern u64 klgraph_addr[];
	int cnodeid;

	cnodeid = nasid_to_cnodeid(nasid);
	if (klgraph_addr[cnodeid] == 0) {
		ret_stuff.status = 0;
		ret_stuff.v0 = 0;
		ret_stuff.v1 = 0;
		ret_stuff.v2 = 0;
		SAL_CALL(ret_stuff, SN_SAL_GET_KLCONFIG_ADDR, (u64)nasid, 0, 0, 0, 0, 0, 0);

		/*
	 	* We should panic if a valid cnode nasid does not produce
	 	* a klconfig address.
	 	*/
		if (ret_stuff.status != 0) {
			panic("ia64_sn_get_klconfig_addr: Returned error %lx\n", ret_stuff.status);
		}

		klgraph_addr[cnodeid] = ret_stuff.v0;
	}
	return(klgraph_addr[cnodeid]);
}

/*
 * Returns the next console character.
 */
static inline u64
ia64_sn_console_getc(int *ch)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_GETC, 0, 0, 0, 0, 0, 0, 0);

	/* character is in 'v0' */
	*ch = (int)ret_stuff.v0;

	return ret_stuff.status;
}

/*
 * Read a character from the SAL console device, after a previous interrupt
 * or poll operation has given us to know that a character is available
 * to be read.
 */
static inline u64
ia64_sn_console_readc(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_READC, 0, 0, 0, 0, 0, 0, 0);

	/* character is in 'v0' */
	return ret_stuff.v0;
}

/*
 * Sends the given character to the console.
 */
static inline u64
ia64_sn_console_putc(char ch)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_PUTC, (uint64_t)ch, 0, 0, 0, 0, 0, 0);

	return ret_stuff.status;
}

/*
 * Sends the given buffer to the console.
 */
static inline u64
ia64_sn_console_putb(const char *buf, int len)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0; 
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_PUTB, (uint64_t)buf, (uint64_t)len, 0, 0, 0, 0, 0);

	if ( ret_stuff.status == 0 ) {
		return ret_stuff.v0;
	}
	return (u64)0;
}

/*
 * Print a platform error record
 */
static inline u64
ia64_sn_plat_specific_err_print(int (*hook)(const char*, ...), char *rec)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_PRINT_ERROR, (uint64_t)hook, (uint64_t)rec, 0, 0, 0, 0, 0);

	return ret_stuff.status;
}

/*
 * Check for Platform errors
 */
static inline u64
ia64_sn_plat_cpei_handler(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_LOG_CE, 0, 0, 0, 0, 0, 0, 0);

	return ret_stuff.status;
}

/*
 * Checks for console input.
 */
static inline u64
ia64_sn_console_check(int *result)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_POLL, 0, 0, 0, 0, 0, 0, 0);

	/* result is in 'v0' */
	*result = (int)ret_stuff.v0;

	return ret_stuff.status;
}

/*
 * Checks console interrupt status
 */
static inline u64
ia64_sn_console_intr_status(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_INTR, 
		 0, SAL_CONSOLE_INTR_STATUS,
		 0, 0, 0, 0, 0);

	if (ret_stuff.status == 0) {
	    return ret_stuff.v0;
	}
	
	return 0;
}

/*
 * Enable an interrupt on the SAL console device.
 */
static inline void
ia64_sn_console_intr_enable(uint64_t intr)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_INTR, 
		 intr, SAL_CONSOLE_INTR_ON,
		 0, 0, 0, 0, 0);
}

/*
 * Disable an interrupt on the SAL console device.
 */
static inline void
ia64_sn_console_intr_disable(uint64_t intr)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_INTR, 
		 intr, SAL_CONSOLE_INTR_OFF,
		 0, 0, 0, 0, 0);
}

/*
 * Sends a character buffer to the console asynchronously.
 */
static inline u64
ia64_sn_console_xmit_chars(char *buf, int len)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_XMIT_CHARS,
		 (uint64_t)buf, (uint64_t)len,
		 0, 0, 0, 0, 0);

	if (ret_stuff.status == 0) {
	    return ret_stuff.v0;
	}

	return 0;
}

/*
 * Returns the iobrick module Id
 */
static inline u64
ia64_sn_sysctl_iobrick_module_get(nasid_t nasid, int *result)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_SYSCTL_IOBRICK_MODULE_GET, nasid, 0, 0, 0, 0, 0, 0);

	/* result is in 'v0' */
	*result = (int)ret_stuff.v0;

	return ret_stuff.status;
}

/**
 * ia64_sn_pod_mode - call the SN_SAL_POD_MODE function
 *
 * SN_SAL_POD_MODE actually takes an argument, but it's always
 * 0 when we call it from the kernel, so we don't have to expose
 * it to the caller.
 */
static inline u64
ia64_sn_pod_mode(void)
{
	struct ia64_sal_retval isrv;
	SAL_CALL(isrv, SN_SAL_POD_MODE, 0, 0, 0, 0, 0, 0, 0);
	if (isrv.status)
		return 0;
	return isrv.v0;
}

/*
 * Retrieve the system serial number as an ASCII string.
 */
static inline u64
ia64_sn_sys_serial_get(char *buf)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_SYS_SERIAL_GET, buf, 0, 0, 0, 0, 0, 0);
	return ret_stuff.status;
}

extern char sn_system_serial_number_string[];
extern u64 sn_partition_serial_number;

static inline char *
sn_system_serial_number(void) {
	if (sn_system_serial_number_string[0]) {
		return(sn_system_serial_number_string);
	} else {
		ia64_sn_sys_serial_get(sn_system_serial_number_string);
		return(sn_system_serial_number_string);
	}
}
	

/*
 * Returns a unique id number for this system and partition (suitable for
 * use with license managers), based in part on the system serial number.
 */
static inline u64
ia64_sn_partition_serial_get(void)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL(ret_stuff, SN_SAL_PARTITION_SERIAL_GET, 0, 0, 0, 0, 0, 0, 0);
	if (ret_stuff.status != 0)
	    return 0;
	return ret_stuff.v0;
}

static inline u64
sn_partition_serial_number_val(void) {
	if (sn_partition_serial_number) {
		return(sn_partition_serial_number);
	} else {
		return(sn_partition_serial_number = ia64_sn_partition_serial_get());
	}
}

/*
 * Returns the partition id of the nasid passed in as an argument,
 * or INVALID_PARTID if the partition id cannot be retrieved.
 */
static inline partid_t
ia64_sn_sysctl_partition_get(nasid_t nasid)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL(ret_stuff, SN_SAL_SYSCTL_PARTITION_GET, nasid,
		 0, 0, 0, 0, 0, 0);
	if (ret_stuff.status != 0)
	    return INVALID_PARTID;
	return ((partid_t)ret_stuff.v0);
}

/*
 * Returns the partition id of the current processor.
 */

extern partid_t sn_partid;

static inline partid_t
sn_local_partid(void) {
	if (sn_partid < 0) {
		return (sn_partid = ia64_sn_sysctl_partition_get(cpuid_to_nasid(smp_processor_id())));
	} else {
		return sn_partid;
	}
}

/*
 * Register or unregister a physical address range being referenced across
 * a partition boundary for which certain SAL errors should be scanned for,
 * cleaned up and ignored.  This is of value for kernel partitioning code only.
 * Values for the operation argument:
 *	1 = register this address range with SAL
 *	0 = unregister this address range with SAL
 * 
 * SAL maintains a reference count on an address range in case it is registered
 * multiple times.
 * 
 * On success, returns the reference count of the address range after the SAL
 * call has performed the current registration/unregistration.  Returns a
 * negative value if an error occurred.
 */
static inline int
sn_register_xp_addr_region(u64 paddr, u64 len, int operation)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL(ret_stuff, SN_SAL_XP_ADDR_REGION, paddr, len, (u64)operation,
		 0, 0, 0, 0);
	return ret_stuff.status;
}

/*
 * Register or unregister an instruction range for which SAL errors should
 * be ignored.  If an error occurs while in the registered range, SAL jumps
 * to return_addr after ignoring the error.  Values for the operation argument:
 *	1 = register this instruction range with SAL
 *	0 = unregister this instruction range with SAL
 *
 * Returns 0 on success, or a negative value if an error occurred.
 */
static inline int
sn_register_nofault_code(u64 start_addr, u64 end_addr, u64 return_addr,
			 int virtual, int operation)
{
	struct ia64_sal_retval ret_stuff;
	u64 call;
	if (virtual) {
		call = SN_SAL_NO_FAULT_ZONE_VIRTUAL;
	} else {
		call = SN_SAL_NO_FAULT_ZONE_PHYSICAL;
	}
	SAL_CALL(ret_stuff, call, start_addr, end_addr, return_addr, (u64)1,
		 0, 0, 0);
	return ret_stuff.status;
}

/*
 * Change or query the coherence domain for this partition. Each cpu-based
 * nasid is represented by a bit in an array of 64-bit words:
 *      0 = not in this partition's coherency domain
 *      1 = in this partition's coherency domain
 *
 * It is not possible for the local system's nasids to be removed from
 * the coherency domain.  Purpose of the domain arguments:
 *      new_domain = set the coherence domain to the given nasids
 *      old_domain = return the current coherence domain
 *
 * Returns 0 on success, or a negative value if an error occurred.
 */
static inline int
sn_change_coherence(u64 *new_domain, u64 *old_domain)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL(ret_stuff, SN_SAL_COHERENCE, new_domain, old_domain, 0, 0,
		 0, 0, 0);
	return ret_stuff.status;
}

/*
 * Change memory access protections for a physical address range.
 * nasid_array is not used on Altix, but may be in future architectures.
 * Available memory protection access classes are defined after the function.
 */
static inline int
sn_change_memprotect(u64 paddr, u64 len, u64 perms, u64 *nasid_array)
{
	struct ia64_sal_retval ret_stuff;
	int cnodeid;
	unsigned long irq_flags;

	cnodeid = nasid_to_cnodeid(get_node_number(paddr));
	spin_lock(&NODEPDA(cnodeid)->bist_lock);
	local_irq_save(irq_flags);
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_MEMPROTECT, paddr, len, nasid_array,
		 perms, 0, 0, 0);
	local_irq_restore(irq_flags);
	spin_unlock(&NODEPDA(cnodeid)->bist_lock);
	return ret_stuff.status;
}
#define SN_MEMPROT_ACCESS_CLASS_0		0x14a080
#define SN_MEMPROT_ACCESS_CLASS_1		0x2520c2
#define SN_MEMPROT_ACCESS_CLASS_2		0x14a1ca
#define SN_MEMPROT_ACCESS_CLASS_3		0x14a290
#define SN_MEMPROT_ACCESS_CLASS_6		0x084080
#define SN_MEMPROT_ACCESS_CLASS_7		0x021080

/*
 * Turns off system power.
 */
static inline void
ia64_sn_power_down(void)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL(ret_stuff, SN_SAL_SYSTEM_POWER_DOWN, 0, 0, 0, 0, 0, 0, 0);
	while(1);
	/* never returns */
}

/**
 * ia64_sn_fru_capture - tell the system controller to capture hw state
 *
 * This routine will call the SAL which will tell the system controller(s)
 * to capture hw mmr information from each SHub in the system.
 */
static inline u64
ia64_sn_fru_capture(void)
{
        struct ia64_sal_retval isrv;
        SAL_CALL(isrv, SN_SAL_SYSCTL_FRU_CAPTURE, 0, 0, 0, 0, 0, 0, 0);
        if (isrv.status)
                return 0;
        return isrv.v0;
}

#endif /* _ASM_IA64_SN_SN_SAL_H */
