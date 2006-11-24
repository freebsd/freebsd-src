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
 * Section 10 Domain Services
 *
 */

ENTRY(hv_mach_exit)
	mov	MACH_EXIT, %o5
	ta	FAST_TRAP
	retl
	  nop
END(hv_mach_exit)
	
ENTRY(hv_mach_desc)
	mov     %o1, %o2
	ldx     [%o1], %o1
	mov     MACH_DESC, %o5
	ta      FAST_TRAP
	retl
	  stx   %o1, [%o2]
END(hv_mach_desc)

ENTRY(hv_mach_watchdog)
	mov	%o1, %o2
	mov	MACH_SET_WATCHDOG, %o5
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

ENTRY(hv_cpu_start)
END(hv_cpu_start)

ENTRY(hv_cpu_stop)
END(hv_cpu_stop)

ENTRY(hv_cpu_set_rtba)
END(hv_cpu_set_rtba)

ENTRY(hv_cpu_get_rtba)
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
END(hv_cpu_mondo_send)

/*
 * return the hypervisor id for the current cpu
 *
 * ret0 status       (%o0)
 * ret1 cpuid        (%o1)
 * 
 */
ENTRY(hv_cpu_myid)
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
 * arg0 tsbptr       (%o1)
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

ENTRY(hv_mmu_demap_page)
END(hv_mmu_demap_page)

ENTRY(hv_mmu_demap_ctx)
END(hv_mmu_demap_ctx)

ENTRY(hv_mmu_demap_all)
END(hv_mmu_demap_all)

ENTRY(hv_mmu_map_addr)
END(hv_mmu_map_addr)

ENTRY(hv_mmu_map_perm_addr)
END(hv_mmu_map_perm_addr)

ENTRY(hv_mmu_unmap_addr)
END(hv_mmu_unmap_addr)

ENTRY(hv_mmu_unmap_perm_addr)
END(hv_mmu_unmap_perm_addr)

ENTRY(hv_mmu_fault_area_conf)
END(hv_mmu_fault_area_conf)

ENTRY(hv_mmu_enable)
END(hv_mmu_enable)

ENTRY(hv_mmu_tsb_ctx0_info)
END(hv_mmu_tsb_ctx0_info)

ENTRY(hv_mmu_tsb_ctxnon0_info)
END(hv_mmu_tsb_ctxnon0_info)

ENTRY(hv_mmu_fault_area_info)
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
 * ret1 character    (%o0)
 * 
 */
ENTRY(hv_cons_getchar)
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

ENTRY(hv_ldc_get_map_table)
END(hv_ldc_get_map_table)

ENTRY(hv_ldc_set_map_table)
END(hv_ldc_set_map_table)

ENTRY(hv_ldc_copy)
END(hv_ldc_copy)

ENTRY(hv_ldc_mapin)
END(hv_ldc_mapin)

ENTRY(hv_ldc_unmap)
END(hv_ldc_unmap)

ENTRY(hv_ldc_revoke)
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

ENTRY(hv_niagara_get_perfreg)
END(hv_niagara_get_perfreg)

ENTRY(hv_niagara_set_perfreg)
END(hv_niagara_set_perfreg)
	
/*
 * Section 23 UltraSPARC T1 MMU Statistics Counters
 *
 */

ENTRY(hv_niagara_mmustat_conf)
END(hv_niagara_mmustat_conf)

ENTRY(hv_niagara_mmustat_info)
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
	
		
