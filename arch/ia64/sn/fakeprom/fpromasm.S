/* 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 *   (Code copied from or=ther files)
 * Copyright (C) 1998-2000 Hewlett-Packard Co
 * Copyright (C) 1998-2000 David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */



#define __ASSEMBLY__ 1
#include <linux/config.h>
#include <asm/processor.h>
#include <asm/sn/addrs.h>
#include <asm/sn/sn2/shub_mmr.h>

/*
 * This file contains additional set up code that is needed to get going on
 * Medusa.  This code should disappear once real hw is available.
 *
 * On entry to this routine, the following register values are assumed:
 *
 *	gr[8]	- BSP cpu
 *	pr[9]	- kernel entry address
 *	pr[10]	- cpu number on the node
 *
 * NOTE:
 *   This FPROM may be loaded/executed at an address different from the
 *   address that it was linked at. The FPROM is linked to run on node 0
 *   at address 0x100000. If the code in loaded into another node, it
 *   must be loaded at offset 0x100000 of the node. In addition, the
 *   FPROM does the following things:
 *		- determine the base address of the node it is loaded on
 *		- add the node base to _gp.
 *		- add the node base to all addresses derived from "movl" 
 *		  instructions. (I couldnt get GPREL addressing to work)
 *		  (maybe newer versions of the tools will support this)
 *		- scan the .got section and add the node base to all
 *		  pointers in this section.
 *		- add the node base to all physical addresses in the
 *		  SAL/PAL/EFI table built by the C code. (This is done
 *		  in the C code - not here)
 *		- add the node base to the TLB entries for vmlinux
 */

#define KERNEL_BASE	0xe000000000000000
#define BOOT_PARAM_ADDR 0x40000


/* 
 * ar.k0 gets set to IOPB_PA value, on 460gx chipset it should 
 * be 0x00000ffffc000000, but on snia we use the (inverse swizzled)
 * IOSPEC_BASE value
 */
#ifdef SGI_SN2
#define IOPB_PA		0xc000000fcc000000
#endif

#define RR_RID		8



// ====================================================================================	
        .text
        .align 16
	.global _start
	.proc _start
_start:

// Setup psr and rse for system init
	mov		psr.l = r0;;
	srlz.d;;
	invala
	mov		ar.rsc = r0;;
	loadrs
	;;

// Isolate node number we are running on.
	mov		r6 = ip;;
#ifdef SGI_SN2
	shr		r5 = r6,38			// r5 = node number
	dep		r6 = 0,r6,0,36			// r6 = base memory address of node

#endif


// Set & relocate gp.
	movl		r1= __gp;;			// Add base memory address
	or 		r1 = r1,r6			// Relocate to boot node

// Lets figure out who we are & put it in the LID register.
#ifdef SGI_SN2
// On SN2, we (currently) pass the cpu number in r10 at boot
	and		r25=3,r10;;
	movl		r16=0x8000008110000400		// Allow IPIs
	mov		r17=-1;;
	st8		[r16]=r17
	movl		r16=0x8000008110060580;;	// SHUB_ID
	ld8		r27=[r16];;
	extr.u		r27=r27,32,11;;
	shl 		r26=r25,28;;			// Align local cpu# to lid.eid
	shl 		r27=r27,16;;			// Align NASID to lid.id
	or  		r26=r26,r27;;			// build the LID
#else
// The BR_PI_SELF_CPU_NUM register gives us a value of 0-3.
// This identifies the cpu on the node. 
// Merge the cpu number with the NASID to generate the LID.
	movl		r24=0x80000a0001000020;;	// BR_PI_SELF_CPU_NUM
	ld8 		r25=[r24]			// Fetch PI_SELF
	movl		r27=0x80000a0001600000;;	// Fetch REVID to get local NASID
	ld8 		r27=[r27];;
	extr.u		r27=r27,32,8;;
	shl 		r26=r25,16;;			// Align local cpu# to lid.eid
	shl 		r27=r27,24;;			// Align NASID to lid.id
	or  		r26=r26,r27;;			// build the LID
#endif
	mov 		cr.lid=r26			// Now put in in the LID register

	movl		r2=FPSR_DEFAULT;;
	mov 		ar.fpsr=r2
	movl		sp = bootstacke-16;;
	or 		sp = sp,r6			// Relocate to boot node			

// Save the NASID that we are loaded on.
	movl		r2=base_nasid;;			// Save base_nasid for C code
	or 		r2 = r2,r6;;			// Relocate to boot node
  	st8 		[r2]=r5				// Uncond st8 - same on all cpus

// Save the kernel entry address. It is passed in r9 on one of
// the cpus.
	movl		r2=bsp_entry_pc
	cmp.ne		p6,p0=r9,r0;;
	or 		r2 = r2,r6;;			// Relocate to boot node
(p6)  	st8 		[r2]=r9				// Uncond st8 - same on all cpus


// The following can ONLY be done by 1 cpu. Lets set a lock - the
// cpu that gets it does the initilization. The rest just spin waiting
// til initilization is complete.
	movl		r22 = initlock;;
	or		r22 = r22,r6			// Relocate to boot node
	mov		r23 = 1;;
	xchg8		r23 = [r22],r23;;
	cmp.eq 		p6,p0 = 0,r23
(p6)	br.cond.spnt.few init
1:	ld4		r23 = [r22];;
	cmp.eq		p6,p0 = 1,r23
(p6)	br.cond.sptk	1b
	br		initx

// Add base address of node memory to each pointer in the .got section.
init:	movl		r16 = _GLOBAL_OFFSET_TABLE_;;
	or		r16 = r16,r6;;			// Relocate to boot node
1: 	ld8		r17 = [r16];;
	cmp.eq		p6,p7=0,r17
(p6)	br.cond.sptk.few.clr 2f;;
	or		r17 = r17,r6;;			// Relocate to boot node
	st8		[r16] = r17,8
	br		1b
2:
	mov		r23 = 2;;			// All done, release the spinning cpus
	st4		[r22] = r23
initx:

//
//	I/O-port space base address:
//
	movl		r2 = IOPB_PA;;
	mov		ar.k0 = r2


// Now call main & pass it the current LID value.
	alloc 		r2=ar.pfs,0,0,2,0
	mov    		r32=r26
	mov   		r33=r8;;
	br.call.sptk.few rp=fmain
	
// Initialize Region Registers
//
        mov             r10 = r0
        mov             r2 = (13<<2)
        mov             r3 = r0;;
1:      cmp4.gtu        p6,p7 = 7, r3
        dep             r10 = r3, r10, 61, 3
        dep             r2 = r3, r2, RR_RID, 4;;
(p7)    dep             r2 = 0, r2, 0, 1;;
(p6)    dep             r2 = -1, r2, 0, 1;;
        mov             rr[r10] = r2
        add             r3 = 1, r3;;
        srlz.d;;
        cmp4.gtu        p6,p0 = 8, r3
(p6)    br.cond.sptk.few.clr 1b

//
// Return value indicates if we are the BSP or AP.
// 	   1 = BSP, 0 = AP
	mov             cr.tpr=r0;;
	cmp.eq		p6,p0=r8,r0
(p6)	br.cond.spnt	slave

//
// Go to kernel C startup routines
//	Need to do a "rfi" in order set "it" and "ed" bits in the PSR.
//	This is the only way to set them.

	movl		r28=BOOT_PARAM_ADDR
	movl		r2=bsp_entry_pc;;
	or 		r28 = r28,r6;;			// Relocate to boot node
	or 		r2 = r2,r6;;			// Relocate to boot node
	ld8		r2=[r2];;
	or		r2=r2,r6;;
	dep		r2=0,r2,61,3;;			// convert to phys mode

//
// Turn on address translation, interrupt collection, psr.ed, protection key.
// Interrupts (PSR.i) are still off here.
//

        movl            r3 = (  IA64_PSR_BN | \
                                IA64_PSR_AC | \
                                IA64_PSR_DB | \
                                IA64_PSR_DA | \
                                IA64_PSR_IC   \
                             )
        ;;
        mov             cr.ipsr = r3

//
// Go to kernel C startup routines
//      Need to do a "rfi" in order set "it" and "ed" bits in the PSR.
//      This is the only way to set them.

	mov		r8=r28;;
	bsw.1		;;
	mov		r28=r8;;
	bsw.0		;;
        mov             cr.iip = r2
        srlz.d;;
        rfi;;

	.endp		_start



// Slave processors come here to spin til they get an interrupt. Then they launch themselves to
// the place ap_entry points. No initialization is necessary - the kernel makes no
// assumptions about state on this entry.
//	Note: should verify that the interrupt we got was really the ap_wakeup
//	      interrupt but this should not be an issue on medusa
slave:
	mov		r8=cr.irr0;;			// Check for interrupt pending.
	cmp.eq		p6,p0=r8,r0
	;;
(p6)	nop.i		0x8beef				// Medusa - put cpu to sleep til interrupt occurs
(p6)	br.cond.sptk	slave;;

	mov		r8=cr.ivr;;			// Got one. Must read ivr to accept it
	srlz.d;;
	mov		cr.eoi=r0;;			// must write eoi to clear
	movl		r8=ap_entry;;			// now jump to kernel entry
	or 		r8 = r8,r6;;			// Relocate to boot node
	ld8		r9=[r8],8;;
	ld8		r1=[r8]
	mov		b0=r9;;
	br		b0

// Here is the kernel stack used for the fake PROM
	.bss
	.align		16384
bootstack:
	.skip		16384
bootstacke:
initlock:
	data4



//////////////////////////////////////////////////////////////////////////////////////////////////////////
// This code emulates the PAL. Only essential interfaces are emulated.


	.text
	.global	pal_emulator
	.proc	pal_emulator
pal_emulator:
	mov	r8=-1

	mov	r9=256
	;;
	cmp.gtu p6,p7=r9,r28		/* r28 <= 255? */
(p6)	br.cond.sptk.few static
	;;
	mov	r9=512
	;;
	cmp.gtu p6,p7=r9,r28
(p6)	br.cond.sptk.few stacked
	;;

static:	cmp.eq	p6,p7=6,r28		/* PAL_PTCE_INFO */
(p7)	br.cond.sptk.few 1f
	movl	r8=0				/* status = 0 */
	movl	r9=0x100000000			/* tc.base */
	movl	r10=0x0000000200000003		/* count[0], count[1] */
	movl	r11=0x1000000000002000		/* stride[0], stride[1] */
	;;

1:	cmp.eq	p6,p7=14,r28		/* PAL_FREQ_RATIOS */
(p7)	br.cond.sptk.few 1f
	movl	r8=0				/* status = 0 */
	movl	r9 =0x100000064			/* proc_ratio (1/100) */
	movl	r10=0x100000100			/* bus_ratio<<32 (1/256) */
	movl	r11=0x10000000a			/* itc_ratio<<32 (1/100) */
	;;

1:	cmp.eq	p6,p7=8,r28		/* PAL_VM_SUMMARY */
(p7)	br.cond.sptk.few 1f
	movl	r8=0
#ifdef SGI_SN2
	movl	r9=0x0203083001151065
	movl	r10=0x183f
#endif
	movl	r11=0
	;;

1:	cmp.eq	p6,p7=19,r28		/* PAL_RSE_INFO */
(p7)	br.cond.sptk.few 1f
	movl	r8=0
	movl	r9=0x60
	movl	r10=0x0
	movl	r11=0
	;;

1:	cmp.eq	p6,p7=15,r28		/* PAL_PERF_MON_INFO */
(p7)	br.cond.sptk.few 1f
	movl	r8=0
	movl	r9=0x08122004
	movl	r10=0x0
	movl	r11=0
	mov	r2=ar.lc
	mov	r3=16;;
	mov	ar.lc=r3
	mov	r3=r29;;
5:	st8	[r3]=r0,8
	br.cloop.sptk.few 5b;;
	mov	ar.lc=r2
	mov	r3=r29
	movl	r2=0x1fff;;			/* PMC regs */
	st8	[r3]=r2
	add	r3=32,r3
	movl	r2=0x3ffff;;			/* PMD regs */
	st8	[r3]=r2
	add	r3=32,r3
	movl	r2=0xf0;;			/* cycle regs */
	st8	[r3]=r2
	add	r3=32,r3
	movl	r2=0x10;;			/* retired regs */
	st8	[r3]=r2
	;;

1:	cmp.eq	p6,p7=34,r28		/* PAL_VM_PAGE_SIZE */
(p7)	br.cond.sptk.few 1f
	movl	r8=0				/* status = 0 */
	movl	r9=0x015557000			/* insertable page sizes */
	movl	r10=0x115557000			/* purgeable page sizes */
	movl	r11=0

1:	cmp.eq	p6,p7=19,r28		/* PAL_RSE_INFO */
(p7)	br.cond.sptk.few 1f
	movl	r8=0				/* status = 0 */
	movl	r9=96				/* num phys stacked */
	movl	r10=0				/* hints */
	movl	r11=0
	;;

1:	cmp.eq	p6,p7=1,r28		/* PAL_CACHE_FLUSH */
(p7)	br.cond.sptk.few 1f
	mov	r9=ar.lc
	movl	r8=524288				/* flush 512k million cache lines (16MB) */
	;;
	mov	ar.lc=r8
	movl	r8=0xe000000000000000
	;;
.loop:	fc	r8
	add	r8=32,r8
	br.cloop.sptk.few .loop
	sync.i
	;;
	srlz.i
	;;
	mov	ar.lc=r9
	mov	r8=r0
1:	br.cond.sptk.few rp

stacked:
	br.ret.sptk.few rp

	.endp pal_emulator

