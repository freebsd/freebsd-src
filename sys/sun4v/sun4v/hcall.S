/*-
 * Copyright (c) 2006 Kip Macy <kmacy@FreeBSD.org>
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

#include <sys/cdefs.h>
	
#include <machine/asm.h>
#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/hypervisorvar.h>
#include <machine/pstate.h>

#include "assym.s"
/*
 * Section 9 API Versioning
 *
 */

/*
 * request and check for a version of the hypervisor apis
 * which may be compatible
 *
 * arg0 api_group         (%o0)
 * arg1 major_number      (%o1)
 * arg2 req_minor_number  (%o2)
 *	 				
 * ret0 status            (%o0)
 * ret1 act_minor_number  (%o1)
 * 
 */
ENTRY(api_set_version)
	mov     API_SET_VERSION, %o5
	ta      CORE_TRAP
	retl
	  stx   %o1, [%o3]
END(api_set_version)

/*
 * retrieve the major and minor number of the most recently
 * successfully negotiated API
 *
 * arg0 api_group         (%o0)
 *	 				
 * ret0 status            (%o0)
 * ret1 major_number      (%o1)
 * ret2 major_number      (%o2)
 * 
 */
ENTRY(api_get_version)
	mov	%o2, %o4
	mov	%o1, %o3
	mov     API_GET_VERSION, %o5
	ta      CORE_TRAP
	retl
	  stx   %o1, [%o4]
	
END(api_get_version)
		
/*
 * Section 10 Domain Services
 *
 */

/*
 * stop all CPUs in the virtual machine domain and place them
 * in the stopped state
 *
 * arg0 exit_code      (%o0)
 *	 				
 */
ENTRY(hv_mach_exit)
	mov	MACH_EXIT, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mach_exit)
	
/*
 * copy the most current machine description into buffer
 * upon success or EINVAL the service returns the actual
 * size of the machine description	
 *
 * arg0 buffer         (%o0)
 * arg1 length         (%o1)
 *	 				
 * ret0 status         (%o0)
 * ret1 length         (%o1)
 * 
 */
ENTRY(hv_mach_desc)
	mov     %o1, %o2
	ldx     [%o1], %o1
	mov     MACH_DESC, %o5
	ta      FAST_TRAP
	retl
	  stx   %o1, [%o2]
END(hv_mach_desc)

/*
 * execute a software initiated reset of a virtual machine domain
 * 
 */
ENTRY(hv_mach_sir)
	mov	MACH_SIR, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mach_sir)
	
/*
 * report the guests soft state to the hypervisor
 *
 * arg0 soft_state          (%o0)
 * arg1 soft_state_desc_ptr (%o1)
 *	 				
 * ret0 status              (%o0)
 * 
 */
ENTRY(hv_mach_set_soft_state)
	mov	MACH_SET_SOFT_STATE, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mach_set_soft_state)
	
/*
 * retrieve the current value of the guest's software state
 *
 * arg0 soft_desc_ptr       (%o0)
 *	 				
 * ret0 status              (%o0)
 * arg1 soft_state          (%o1)
 * 
 */
ENTRY(hv_mach_get_soft_state)
	mov	%o1, %o2
	mov	MACH_SET_SOFT_STATE, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_mach_get_soft_state)
	
/*
 * set a watchdog timer, 0 disables, upon success
 * time_remaining contains the time previously remaining
 *
 * arg0 timeout        (%o0)
 *	 				
 * ret0 status         (%o0)
 * ret1 time_remaining (%o1)
 * 
 */
ENTRY(hv_mach_watchdog)
	mov	%o1, %o2
	mov	MACH_WATCHDOG, %o5
	ta	FAST_TRAP
	brnz,pn %o0, 1f
	  nop
	stx	%o1, [%o2]
1:	retl
	  nop
END(hv_mach_watchdog)
	
/*
 * Section 11 CPU Services
 * 
 */

/*
 * start CPU with id cpuid with pc in %pc and real trap base address
 * of rtba
 *
 * arg0 cpuid        (%o0)
 * arg1 pc           (%o1)
 * arg2 rtba         (%o2)
 * arg3 target_arg0  (%o3)
 *	 				
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_cpu_start)
	mov	CPU_START, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_cpu_start)

/*
 * stop CPU with id cpuid 
 *
 * arg0 cpuid        (%o0)
 *	 				
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_cpu_stop)
	mov	CPU_STOP, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_cpu_stop)

/*
 * set the real trap base address of the local cpu to rtba
 * upon success the previous_rtba contains the address of the
 * old rtba
 *
 * arg0 rtba         (%o0)
 *	 				
 * ret0 status       (%o0)
 * ret1 previous_rtba(%o1)
 * 
 */
ENTRY(hv_cpu_set_rtba)
	mov	%o1, %o2
	mov	CPU_SET_RTBA, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
	
END(hv_cpu_set_rtba)

/*
 * return the current real trap base address
 *
 * ret0 status       (%o0)
 * ret1 rtba         (%o1)
 * 
 */
ENTRY(hv_cpu_get_rtba)
	mov	CPU_GET_RTBA, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_cpu_get_rtba)			
	
/*
 * suspend execution on current cpu
 *
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_cpu_yield)
	mov	CPU_YIELD, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_cpu_yield)

/*
 * configure queue of size nentries to be placed at base raddr 
 *
 * arg0 queue        (%o0)
 * arg1 base raddr   (%o1)
 * arg2 nentries     (%o2)
 *
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_cpu_qconf)
	mov	CPU_QCONF, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_cpu_qconf)
	
/*
 * return configuration of queue queue 
 *
 * arg0 queue        (%o0)
 *
 * ret0 status       (%o0)
 * ret1 base raddr   (%o1)
 * ret2 nentries     (%o2)
 * 
 */
ENTRY(hv_cpu_qinfo)
END(hv_cpu_qinfo)
	
/*
 * send cpu mondo interrupt to cpulist
 *
 * arg0 ncpus        (%o0)
 * arg1 cpu list ra  (%o1)
 * arg2 mondo data ra(%o2)
 *	 				
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_cpu_mondo_send)
	ldx	[PCPU(MONDO_DATA_RA)], %o2
	mov	CPU_MONDO_SEND, %o5
	ta	FAST_TRAP
	retl
	  membar	#Sync	
END(hv_cpu_mondo_send)

/*
 * return the hypervisor id for the current cpu
 *
 * ret0 status       (%o0)
 * ret1 cpuid        (%o1)
 * 
 */
ENTRY(hv_cpu_myid)
	mov	%o0, %o2
	mov	CPU_MYID, %o5
	ta	FAST_TRAP
	stx	%o1, [%o2]
	retl
	  nop
END(hv_cpu_myid)

/*
 * retrieve the current state of cpu cpuid
 *
 * arg0 cpuid        (%o0)
 *		
 * ret0 status       (%o0)
 * ret1 state        (%o1)
 * 
 */
ENTRY(hv_cpu_state)
	mov	%o1, %o2
	mov	CPU_STATE, %o5
	ta	FAST_TRAP
	stx	%o1, [%o2]
	retl
	  nop
END(hv_cpu_state)
	
/*
 * Section 12 MMU Services
 *
 */
	
/*
 * set the tsb(s) for the current cpu for context 0
 *
 * arg0 ntsb         (%o0)
 * arg1 tsbdptr      (%o1)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_tsb_ctx0)
	mov	MMU_TSB_CTX0, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_tsb_ctx0)

/*
 * set the tsb(s) for the current cpu for non-zero contexts 
 *
 * arg0 ntsb         (%o0)
 * arg1 tsbptr       (%o1)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_tsb_ctxnon0)
	mov	MMU_TSB_CTXNON0, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_tsb_ctxnon0)

/*
 * demap any page mapping of virtual address vaddr in context ctx
 *
 * arg0 reserved     (%o0)
 * arg1 reserved     (%o1)
 * arg2 vaddr        (%o2)
 * arg3 ctx          (%o3)
 * arg4 flags        (%o4)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_demap_page)
	mov	MMU_DEMAP_PAGE, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_demap_page)

/*
 * demap all non-permanent virtual address mappings in context ctx
 *
 * arg0 reserved     (%o0)
 * arg1 reserved     (%o1)
 * arg2 ctx          (%o2)
 * arg3 flags        (%o3)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_demap_ctx)
	mov	MMU_DEMAP_CTX, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_demap_ctx)

/*
 * demap all non-permanent virtual address mappings for the current
 * virtual cpu
 *
 * arg0 reserved     (%o0)
 * arg1 reserved     (%o1)
 * arg2 flags        (%o2)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_demap_all)
	mov	MMU_DEMAP_ALL, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_demap_all)

/*
 * create a non-permanent mapping for the calling virtual cpu
 *
 * arg0 vaddr        (%o0)
 * arg1 context      (%o1)
 * arg2 TTE          (%o2)
 * arg3 flags        (%o3)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_map_addr)
	mov	MMU_MAP_ADDR, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_map_addr)

/*
 * create a permanent mapping for the calling virtual cpu
 *
 * arg0 vaddr        (%o0)
 * arg1 reserved     (%o1)
 * arg2 TTE          (%o2)
 * arg3 flags        (%o3)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_map_perm_addr)
	mov	MMU_MAP_PERM_ADDR, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_map_perm_addr)

/*
 * demap virtual address vaddr in context ctx on current virtual cpu
 *
 * arg0 vaddr        (%o0)
 * arg1 ctx          (%o1)
 * arg2 flags        (%o2)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_unmap_addr)
	mov	MMU_UNMAP_ADDR, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_unmap_addr)

/*
 * demap any permanent mapping at virtual address vaddr on current virtual cpu
 *
 * arg0 vaddr        (%o0)
 * arg1 reserved     (%o1)
 * arg2 flags        (%o2)
 *		
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_mmu_unmap_perm_addr)
	mov	MMU_UNMAP_PERM_ADDR, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_unmap_perm_addr)

/*
 * configure the MMU fault status area for the current virtual cpu
 *
 * arg0 raddr        (%o0)
 *		
 * ret0 status       (%o0)
 * ret1 prev_raddr   (%o1)
 * 
 */
ENTRY(hv_mmu_fault_area_conf)
	mov	%o1, %o2
	mov	MMU_FAULT_AREA_CONF, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_mmu_fault_area_conf)

/*
 * enable or disable virtual address translation for the current virtual cpu
 *
 * arg0 enable_flag   (%o0)
 * arg1 return_target (%o1)
 *		
 * ret0 status        (%o0)
 * 
 */
ENTRY(hv_mmu_enable)
	mov	MMU_ENABLE, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mmu_enable)

/*
 * return the TSB configuration as previously defined by mmu_tsb_ctx0
 *
 * arg0 maxtsbs       (%o0)
 * arg1 buffer_ra     (%o1)
 *		
 * ret0 status        (%o0)
 * ret1 ntsbs         (%o1)
 * 
 */
ENTRY(hv_mmu_tsb_ctx0_info)
	mov	%o1, %o2
	mov	MMU_TSB_CTX0_INFO, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]	
END(hv_mmu_tsb_ctx0_info)

/*
 * return the TSB configuration as previously defined by mmu_tsb_ctxnon0
 *
 * arg0 maxtsbs       (%o0)
 * arg1 buffer_ra     (%o1)
 *		
 * ret0 status        (%o0)
 * ret1 ntsbs         (%o1)
 * 
 */
ENTRY(hv_mmu_tsb_ctxnon0_info)
	mov	%o1, %o2
	mov	MMU_TSB_CTXNON0_INFO, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]	
END(hv_mmu_tsb_ctxnon0_info)

/*
 * return the MMU fault status area defined for the current virtual cpu
 *
 * ret0 status        (%o0)
 * ret1 mmfsara       (%o1)
 * 
 */
ENTRY(hv_mmu_fault_area_info)
	mov	%o0, %o2
	mov	MMU_FAULT_AREA_INFO, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]	
	
END(hv_mmu_fault_area_info)

/*
 * Section 13 Cache and Memory Services
 *
 */

/*
 * zero from raddr to raddr+length-1
 *
 * arg0 raddr         (%o0)
 * arg1 length        (%o1)
 *		
 * ret0 status        (%o0)
 * ret1 length scrubbed (%o1)
 * 
 */
ENTRY(hv_mem_scrub)
	mov	MEM_SCRUB, %o5
	ta	FAST_TRAP
	stx	%o1, [%o2]
	retl
	  nop
END(hv_mem_scrub)

/*
 * for the memory address range from raddr to raddr+length-1
 * for the next access within that range from main system memory
 *
 * arg0 raddr         (%o0)
 * arg1 length        (%o1)
 *		
 * ret0 status        (%o0)
 * ret1 length synced (%o1)
 * 
 */
ENTRY(hv_mem_sync)
	mov	MEM_SYNC, %o5
	ta	FAST_TRAP
	stx	%o1, [%o2]
	retl
	  nop	
END(hv_mem_sync)

/*
 * Section 14 Device Interrupt Services
 *
 */

/*
 * converts a device specific interrupt number given by
 * devhandle and devino	to a system specific ino (sysino)
 *
 * arg0 devhandle     (%o0)
 * arg1 devino        (%o1)
 *		
 * ret0 status        (%o0)
 * ret1 sysino        (%o1)
 * 
 */
ENTRY(hv_intr_devino_to_sysino)
	mov	INTR_DEVINO2SYSINO, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]	
END(hv_intr_devino_to_sysino)

/*
 * return intr enabled state
 *
 * arg0 sysino        (%o0)
 *		
 * ret0 status        (%o0)
 * ret1 intr_enabled  (%o1)
 * 
 */
ENTRY(hv_intr_getenabled)
	mov	%o1, %o2
	mov	INTR_GETENABLED, %o5
	ta	FAST_TRAP
	retl
  	  st	%o1, [%o2]	  
END(hv_intr_getenabled)

/*
 * set intr enabled state
 *
 * arg0 sysino        (%o0)
 * arg1 intr_enabled  (%o1)
 *		
 * ret0 status        (%o0)
 * 
 */
ENTRY(hv_intr_setenabled)
	mov	INTR_SETENABLED, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_intr_setenabled)

/*
 * return current state of the interrupt given
 * by the sysino 	
 *
 * arg0 sysino        (%o0)
 *		
 * ret0 status        (%o0)
 * ret1 intr_state    (%o1)
 * 
 */
ENTRY(hv_intr_getstate)
	mov	%o1, %o2
	mov	INTR_GETSTATE, %o5
	ta	FAST_TRAP
	retl
  	  st	%o1, [%o2]	  
END(hv_intr_getstate)

/*
 * set the current state of the interrupt given
 * by the sysino 	
 *
 * arg0 sysino        (%o0)
 * arg1 intr_state    (%o1)
 *		
 * ret0 status        (%o0)
 * 
 */
ENTRY(hv_intr_setstate)
	mov	INTR_SETSTATE, %o5
	ta	FAST_TRAP
	retl
	  nop	
END(hv_intr_setstate)

/*
 * return the cpuid that is the current target of the 
 * interrupt given by the sysino
 *
 * arg0 sysino        (%o0)
 *		
 * ret0 status        (%o0)
 * ret1 cpuid         (%o1)
 * 
 */
ENTRY(hv_intr_gettarget)
	mov	%o1, %o2
	mov	INTR_GETTARGET, %o5
	ta	FAST_TRAP
	retl
  	  st	%o1, [%o2]	  
END(hv_intr_gettarget)

/*
 * set the target to cpuid that for the
 * interrupt given by the sysino
 *
 * arg0 sysino        (%o0)
 * arg1 cpuid         (%o1)
 *		
 * ret0 status        (%o0)
 * 
 */
ENTRY(hv_intr_settarget)
	mov	INTR_SETTARGET, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_intr_settarget)

/*
 * Section 15 Time of Day Services
 *
 */

/*
 * get the current time of day
 *
 * ret0 status       (%o0)
 * ret1 time-of-day  (%o1)
 * 
 */
ENTRY(hv_tod_get)
	mov	%o0, %o2
	mov	TOD_GET, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_tod_get)

/*
 * set the current time-of-day
 * arg0 tod          (%o0)
 *
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_tod_set)
	mov	TOD_SET, %o5
	ta	FAST_TRAP
	retl
	  nop	
END(hv_tod_set)

/*
 * Section 16 Console Services
 *
 */
	
/*
 * return a character from the console device
 *
 * ret0 status       (%o0)
 * ret1 character    (%o1)
 * 
 */
ENTRY(hv_cons_getchar)
	mov	%o0, %o2
	mov	CONS_GETCHAR, %o5
	ta	FAST_TRAP
	retl
  	  stx	%o1, [%o2]	  
END(hv_cons_getchar)
	
/*
 * send a character to the console device
 * arg0 char         (%o0)
 *
 * ret0 status       (%o0)
 * 
 */
ENTRY(hv_cons_putchar)
	mov	CONS_PUTCHAR, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_cons_putchar)

/*
 * write characters in raddr to console
 * arg0 raddr        (%o0)
 * arg1 size         (%o1)
 *
 * ret0 status       (%o0)
 * ret1 char written (%o1)
 * 
 */
ENTRY(hv_cons_write)
	mov	%o2, %o3
	mov	CONS_WRITE, %o5
	ta	FAST_TRAP
	brnz,a	%o0, 1f		! failure, just return error
	  mov	1, %o0
	stx	%o1, [%o3]
1:	retl
	  nop
END(hv_cons_write)

/*
 * read up to size characters from console in to raddr 
 * arg0 raddr        (%o0)
 * arg1 size
 *
 * ret0 status       (%o0)
 * ret1 char written (%o1)
 * 
 */
ENTRY(hv_cons_read)
	mov	%o2, %o3
	mov	CONS_READ, %o5
	ta	FAST_TRAP
	brnz,a	%o0, 1f		! failure, just return error
	  mov	1, %o0

	cmp	%o1, H_BREAK
	be	1f
	  mov	%o1, %o0

	cmp	%o1, H_HUP
	be	1f
	  mov	%o1, %o0

	stx	%o1, [%o3]
1:	retl
	  nop
END(hv_cons_read)
	

/*
 * Section 17 Core Dump Services
 *
 */
	
/*
 * declare a domain dump buffer to the hypervisor
 * arg0 raddr        (%o0)
 * arg1 size         (%o1)
 *
 * ret0 status       (%o0)
 * ret1 required size of the dump buffer (%o1)
 * 
 */
ENTRY(hv_dump_buf_update)
	mov	DUMP_BUF_UPDATE, %o5
	ta	FAST_TRAP
	retl
  	  stx	%o1, [%o2]	
END(hv_dump_buf_update)

/*
 * return the currently configured dump buffer description 
 *
 * ret0 status                          (%o0)
 * ret1 ra of the current dump buffer   (%o1)
 * ret2 size of the current dump buffer (%o2)
 * 
 */
ENTRY(hv_dump_buf_info)
	mov	%o0, %o3
	mov	%o1, %o4
	mov	DUMP_BUF_INFO, %o5
	ta	FAST_TRAP
  	stx	%o1, [%o3]	
	retl
	  stx	%o2, [%o4]		
END(hv_dump_buf_info)

/*
 * Section 18 Trap Trace Services
 *
 */

/*
 * arg0 RA base of buffer          (%o0)
 * arg1 buf size in no. of entries (%o1)
 *
 * ret0 status (%o0)
 * ret1 minimum size in no. of entries on failure,
 * actual size in no. of entries on success (%o1)
 *	
 */
ENTRY(hv_ttrace_buf_conf)
	mov	TTRACE_BUF_CONF, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_ttrace_buf_conf)

/*
 * return the size and location of a previously declare
 * trap-trace buffer		
 *	
 * ret0 status                  (%o0)
 * ret1 RA base of buffer       (%o1)
 * ret2 size in no. of entries  (%o2)
 *	
 */
ENTRY(hv_ttrace_buf_info)
	mov	%o0, %o3
	mov	%o1, %o4
	mov	TTRACE_BUF_INFO, %o5
	ta	FAST_TRAP
	stx	%o1, [%o3]
	retl
	  stx	%o2, [%o4]
END(hv_ttrace_buf_info)

/*
 * enable / disable trap tracing
 *		
 * arg0 enable / disable      (%o0)
 *	
 * ret0 status                (%o0)
 * ret1 previous enable state (%o1)
 *	
 */
ENTRY(hv_ttrace_enable)
	mov	%o1, %o2
	mov	TTRACE_ENABLE, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_ttrace_enable)

/*
 * TTRACE_FREEZE
 * arg0 enable/ freeze        (%o0)
 *
 * ret0 status                (%o0)
 * ret1 previous freeze state (%o1)
 *	
 */
ENTRY(hv_ttrace_freeze)
	mov	%o1, %o2
	mov	TTRACE_FREEZE, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_ttrace_freeze)

/*
 * add an entry to the trap trace buffer
 *		
 * arg0 tag (16-bits) (%o0)
 * arg1 data word 0   (%o1)
 * arg2 data word 1   (%o2)
 * arg3 data word 2   (%o3)
 * arg4 data word 3   (%o4)
 *
 * ret0 status        (%o0)
 *
*/
ENTRY(hv_ttrace_addentry)
	ta	TTRACE_ADDENTRY
	retl
	  nop	
END(hv_ttrace_addentry)
			
/*
 * Section 19 Logical Domain Channel Services
 *
 */
	
/*
 * configure ldc tx queue
 *	
 * arg0 ldc_id     (%o0)
 * arg1 base_raddr (%o1)
 * arg2 nentries   (%o2)
 *
 * ret0 status     (%o0)
 *
 */
ENTRY(hv_ldc_tx_qconf)
	mov	LDC_TX_QCONF, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_ldc_tx_qconf)

/*
 * return configuration info for ldc tx queue
 *	
 * arg0 ldc_id     (%o0)
 *
 * ret0 status     (%o0)
 * ret1 base_raddr (%o1)
 * ret2 nentries   (%o2)
 *
 */
ENTRY(hv_ldc_tx_qinfo)
	mov	%o1, %o3
	mov	%o2, %o4
	mov	LDC_TX_QINFO, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  nop
	stx	%o1, [%o3]
	stx	%o2, [%o4]
1:	retl
	  nop
END(hv_ldc_tx_qinfo)

/*
 * get the state of the ldc tx queue
 *	
 * arg0 ldc_id        (%o0)
 *
 * ret0 status        (%o0)
 * ret1 head_offset   (%o1)
 * ret2 tail_offset   (%o2)
 * ret3 channel_state (%o3)
 * 
 */
ENTRY(hv_ldc_tx_get_state)
	mov	%o1, %o4
	mov	LDC_TX_GET_STATE, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  nop
	stx	%o1, [%o4]
	stx	%o2, [%o4 + 8]
	stx	%o3, [%o4 + 16]
1:	retl
	  nop		
END(hv_ldc_tx_get_state)

/*
 * update the tail pointer of the ldc tx queue
 *	
 * arg0 ldc_id      (%o0)
 * arg1 tail_offset (%o1)
 *
 * ret0 status      (%o0)
 *
 */
ENTRY(hv_ldc_tx_set_qtail)
	mov	LDC_TX_SET_QTAIL, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_ldc_tx_set_qtail)

/*
 * configure ldc rx queue
 *	
 * arg0 ldc_id     (%o0)
 * arg1 base_raddr (%o1)
 * arg2 nentries   (%o2)
 *
 * ret0 status     (%o0)
 *
 */
ENTRY(hv_ldc_rx_qconf)
	mov	LDC_RX_QCONF, %o5
	ta	FAST_TRAP
	retl
	  nop	
END(hv_ldc_rx_qconf)

/*
 * return configuration info for ldc rx queue
 *	
 * arg0 ldc_id     (%o0)
 *
 * ret0 status     (%o0)
 * ret1 base_raddr (%o1)
 * ret2 nentries   (%o2)
 *
 */
ENTRY(hv_ldc_rx_qinfo)
	mov	%o1, %o3
	mov	%o2, %o4
	mov	LDC_RX_QINFO, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  nop
	stx	%o1, [%o3]
	stx	%o2, [%o4]
1:	retl
	  nop
END(hv_ldc_rx_qinfo)

/*
 * get the state of the ldc rx queue
 *	
 * arg0 ldc_id        (%o0)
 *
 * ret0 status        (%o0)
 * ret1 head_offset   (%o1)
 * ret2 tail_offset   (%o2)
 * ret3 channel_state (%o3)
 * 
 */
ENTRY(hv_ldc_rx_get_state)
	mov	%o1, %o4
	mov	LDC_RX_GET_STATE, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  nop
	stx	%o1, [%o4]
	stx	%o2, [%o4 + 8]
	stx	%o3, [%o4 + 16]
1:	retl
	  nop		
END(hv_ldc_rx_get_state)

/*
 * update the head pointer of the ldc rx queue
 *	
 * arg0 ldc_id      (%o0)
 * arg1 head_offset (%o1)
 *
 * ret0 status      (%o0)
 *
 */
ENTRY(hv_ldc_rx_set_qhead)
	mov	LDC_RX_SET_QHEAD, %o5
	ta	FAST_TRAP
	retl
	  nop	
END(hv_ldc_rx_set_qhead)

/*
 * declare an export map table
 *	
 * arg0 channel      (%o0)
 * arg1 base_ra      (%o1)
 * arg2 nentries     (%o2)
 *
 * ret0 status      (%o0)
 *
 */
ENTRY(hv_ldc_set_map_table)
	mov	LDC_SET_MAPTABLE, %o5
	ta	FAST_TRAP
	retl
	  nop	
END(hv_ldc_set_map_table)

/*
 * retrieve the current map table configuration associated
 * with the given domain channel
 *	
 * arg0 channel      (%o0)
 *
 * ret0 status       (%o0)
 * ret1 base_ra      (%o1)
 * ret2 nentries     (%o2)
 *
 */
ENTRY(hv_ldc_get_map_table)
	mov	%o1, %o3
	mov	%o2, %o4
	mov	LDC_GET_MAPTABLE, %o5
	ta	FAST_TRAP
	stx	%o1, [%o3]
	retl
          stx	%o2, [%o4]	
END(hv_ldc_get_map_table)

/*
 * copy data into or out of a local memory region form or to
 * the logical domain at the other end of the specified domain
 * channel	
 *	
 * arg0 channel      (%o0)
 * arg1 flags        (%o1)
 * arg2 cookie       (%o2)
 * arg3 raddr        (%o3)
 * arg4 length       (%o4)
 *
 * ret0 status       (%o0)
 * ret1 ret_length   (%o1)
 *
 */
ENTRY(hv_ldc_copy)
END(hv_ldc_copy)

/*
 * attempt to map into the local guest's real address space the
 * page identified by the shared memory cookie
 *	
 * arg0 channel      (%o0)
 * arg1 cookie       (%o1)
 *
 * ret0 status       (%o0)
 * ret1 raddr        (%o1)
 * ret2 perms        (%o2)
 *
 */
ENTRY(hv_ldc_mapin)
	mov	%o3, %o4
	mov	%o2, %o3
	mov	LDC_MAPIN, %o5
	ta	FAST_TRAP
	stx	%o1, [%o3]
	retl
          stx	%o2, [%o4]	
END(hv_ldc_mapin)

/*
 * attempt unmap from the local guest's real address space the imported
 * page mapped at the real address raddr
 *	
 * arg0 raddr       (%o0)
 *
 * ret0 status      (%o0)
 *
 */
ENTRY(hv_ldc_unmap)
	mov	LDC_UNMAP, %o5
	ta	FAST_TRAP
	retl
	  nop	
END(hv_ldc_unmap)

/*
 * forcibly unmap from a remote guest's real address space a page 
 * previously exported by the local guest
 *	
 * arg0 channel       (%o0)
 * arg1 cookie        (%o1)
 * arg2 revoke_cookie (%o2)
 *
 * ret0 status        (%o0)
 *
 */
ENTRY(hv_ldc_revoke)
	mov	LDC_REVOKE, %o5
	ta	FAST_TRAP
	retl
	  nop	
END(hv_ldc_revoke)
		
/*
 * Section 20 PCI I/O Services
 *
 */
	
/*
 * create iommu mappings in the device defined by devhandle
 *	
 * arg0 devhandle        (%o0)
 * arg1 tsbid            (%o1)
 * arg2 nttes            (%o2)
 * arg3 io_attributes    (%o3)
 * arg4 io_page_list_p   (%o4)
 *
 * ret0 status           (%o0)
 * ret1 nttes_mapped     (%o1)
 *
 */
ENTRY(hv_pci_iommu_map)
	save	%sp, -CCFSZ, %sp
	mov	%i0, %o0
	mov	%i1, %o1
	mov	%i2, %o2
	mov	%i3, %o3
	mov	%i4, %o4
	mov	PCI_IOMMU_MAP, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  mov	%o0, %i0
	stuw	%o1, [%i5]
1:
	ret
	  restore
END(hv_pci_iommu_map)

/*
 * demap and flush iommu mappings in the device defined by devhandle
 *	
 * arg0 devhandle        (%o0)
 * arg1 tsbid            (%o1)
 * arg2 nttes            (%o2)
 *
 * ret0 status           (%o0)
 * ret1 nttes_demapped   (%o1)
 *
 */
ENTRY(hv_pci_iommu_demap)
	mov	PCI_IOMMU_DEMAP, %o5
	ta	FAST_TRAP
	brz,a	%o0, 1f
	  stuw	%o1, [%o3]
1:	retl
	  nop	
END(hv_pci_iommu_demap)

/*
 * read and return the mapping in the device defined by devhandle
 *	
 * arg0 devhandle        (%o0)
 * arg1 tsbid            (%o1)
 *
 * ret0 status           (%o0)
 * ret1 io_attributes    (%o1)
 * ret2 ra               (%o2)
 *
 */
ENTRY(hv_pci_iommu_getmap)
	mov	%o3, %o4
	mov	%o2, %o3
	mov	PCI_IOMMU_GETMAP, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  nop
	st	%o1, [%o3]
	stx	%o2, [%o4]
1:	retl
	  nop	
END(hv_pci_iommu_getmap)

/*
 * create a "special" mapping in the device given by devhandle
 *	
 * arg0 devhandle        (%o0)
 * arg1 r_addr           (%o1)
 * arg2 io_attributes    (%o2)
 *
 * ret0 status           (%o0)
 * ret1 io_addr          (%o1)
 *
 */
ENTRY(hv_pci_iommu_getbypass)
	mov	PCI_IOMMU_GETBYPASS, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  nop
	stx	%o1, [%o3]
1:	retl
	  nop	
END(hv_pci_iommu_getbypass)
	
/*
 * read PCI configuration space for adapter specified by devhandle
 *	
 * arg0 devhandle        (%o0)
 * arg1 pci_device       (%o1)
 * arg2 pci_config_offset(%o2)
 * arg3 size             (%o3)
 *
 * ret0 status           (%o0)
 * ret1 error_flag       (%o1)
 * ret2 data             (%o2)
 *
 */
ENTRY(hv_pci_config_get)
	mov	PCI_CONFIG_GET, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  movrnz %o1, -1, %o2
	brz,a	%o1, 1f
	  stuw	%o2, [%o4]
1:	retl
	  nop	
END(hv_pci_config_get)

/*
 * write PCI config space for the PCI adapter
 * specified by devhandle	
 *	
 * arg0 devhandle        (%o0)
 * arg1 pci_device       (%o1)
 * arg2 pci_config_offset(%o2)
 * arg3 size             (%o3)
 * arg4 data             (%o4)
 *
 * ret0 status           (%o0)
 * ret1 error_flag       (%o1)
 *
 */	
ENTRY(hv_pci_config_put)
	mov	PCI_CONFIG_PUT, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_pci_config_put)

/*
 * read the io-address given by devhandle, raddr, and size
 *	
 * arg0 devhandle        (%o0)
 * arg1 raddr            (%o1)
 * arg2 size             (%o2)
 *
 * ret0 status           (%o0)
 * ret1 error_flag       (%o1)
 * ret2 data             (%o2)
 *
 */	
ENTRY(hv_pci_peek)
	mov	PCI_PEEK, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  nop
	st	%o1, [%o3]
	stx	%o2, [%o4]
1:	retl
	  nop	
END(hv_pci_peek)

/*
 * attempt to write data to the io-address
 * specified by devhandle, raddr, and size	
 *	
 * arg0 devhandle        (%o0)
 * arg1 raddr            (%o1)
 * arg2 size             (%o2)
 * arg3 data             (%o3)
 * arg4 pci_device       (%o4)
 *
 * ret0 status           (%o0)
 * ret1 error_flag       (%o1)
 *
 */	
ENTRY(hv_pci_poke)
	save	%sp, -CCFSZ, %sp
	mov	%i0, %o0
	mov	%i1, %o1
	mov	%i2, %o2
	mov	%i3, %o3
	mov	%i4, %o4
	mov	PCI_POKE, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  mov	%o0, %i0
	stuw	%o1, [%i5]
1:
	ret
	  restore
END(hv_pci_poke)

/*
 * attempt to write data to the io-address
 * specified by devhandle, raddr, and size	
 *	
 * arg0 devhandle        (%o0)
 * arg1 raddr            (%o1)
 * arg2 size             (%o2)
 * arg3 io_sync_direction(%o3)
 *
 * ret0 status           (%o0)
 * ret1 nsynced          (%o1)
 *
 */	
ENTRY(hv_pci_dma_sync)
	mov	PCI_DMA_SYNC, %o5
	ta	FAST_TRAP
	brnz	%o0, 1f
	  nop
	stx	%o1, [%o4]
1:	retl
	  nop	
END(hv_pci_dma_sync)

/*
 * Section 21 MSI Services
 *
 */

ENTRY(hv_pci_msiq_conf)
END(hv_pci_msiq_conf)

ENTRY(hv_pci_msiq_info)
END(hv_pci_msiq_info)

ENTRY(hv_pci_msiq_getvalid)
END(hv_pci_msiq_getvalid)

ENTRY(hv_pci_msiq_setvalid)
END(hv_pci_msiq_setvalid)

ENTRY(hv_pci_msiq_getstate)
END(hv_pci_msiq_getstate)

ENTRY(hv_pci_msiq_setstate)
END(hv_pci_msiq_setstate)

ENTRY(hv_pci_msiq_gethead)
END(hv_pci_msiq_gethead)

ENTRY(hv_pci_msiq_sethead)
END(hv_pci_msiq_sethead)

ENTRY(hv_pci_msiq_gettail)
END(hv_pci_msiq_gettail)

ENTRY(hv_pci_msi_getvalid)
END(hv_pci_msi_getvalid)

ENTRY(hv_pci_msi_setvalid)
END(hv_pci_msi_setvalid)

ENTRY(hv_pci_msi_getmsiq)
END(hv_pci_msi_getmsiq)

ENTRY(hv_pci_msi_setmsiq)
END(hv_pci_msi_setmsiq)

ENTRY(hv_pci_msi_getstate)
END(hv_pci_msi_getstate)

ENTRY(hv_pci_msi_setstate)
END(hv_pci_msi_setstate)

ENTRY(hv_pci_msg_getmsiq)
END(hv_pci_msg_getmsiq)

ENTRY(hv_pci_msg_setmsiq)
END(hv_pci_msg_setmsiq)

ENTRY(hv_pci_msg_getvalid)
END(hv_pci_msg_getvalid)

ENTRY(hv_pci_msg_setvalid)
END(hv_pci_msg_setvalid)

/*
 * Section 22 UltraSPARC T1 Performance Counters
 *
 */

	
/*
 * read the value of the DRAM/JBus performance register as selected by
 * the perfreg argument
 *	
 * arg0 perfreg          (%o0)
 *
 * ret0 status           (%o0)
 * ret1 value            (%o1)
 *
 */	
ENTRY(hv_niagara_get_perfreg)
	mov	%o1, %o2
	mov	NIAGARA_GET_PERFREG, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_niagara_get_perfreg)

/*
 * set the value of the DRAM/JBus performance register as selected by
 * the perfreg argument
 *	
 * arg0 perfreg          (%o0)
 * arg1 value            (%o1)
 *
 * ret0 status           (%o0)
 *
 */	
ENTRY(hv_niagara_set_perfreg)
	mov	NIAGARA_SET_PERFREG, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_niagara_set_perfreg)
	
/*
 * Section 23 UltraSPARC T1 MMU Statistics Counters
 *
 */

/*
 * enable MMU statistics collection and supply the buffer to deposit the
 * results for the current virtual cpu
 *	
 * arg0 raddr            (%o0)
 *
 * ret0 status           (%o0)
 * ret1 prev_raddr       (%o1)
 *
 */	
ENTRY(hv_niagara_mmustat_conf)
	mov	%o1, %o2
	mov	NIAGARA_MMUSTAT_CONF, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_niagara_mmustat_conf)

/*
 * query the status and the real address for the currently configured buffer
 *	
 * ret0 status           (%o0)
 * ret1 raddr            (%o1)
 *
 */	
ENTRY(hv_niagara_mmustat_info)
	mov	%o0, %o2
	mov	NIAGARA_MMUSTAT_INFO, %o5
	ta	FAST_TRAP
	retl
	  stx	%o1, [%o2]
END(hv_niagara_mmustat_info)

/*
 * Simulator Services
 *
 */

ENTRY(hv_magic_trap_on)
	ta	0x77
	retl
	  nop
END(hv_magic_trap_on)

ENTRY(hv_magic_trap_off)
	ta	0x78
	retl
	  nop
END(hv_magic_trap_off)


ENTRY(hv_sim_read)
	mov	SIM_READ, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_read)

ENTRY(hv_sim_write)
	mov	SIM_WRITE, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_write)
	
		
