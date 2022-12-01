#include "assym.inc"
#include <machine/asm.h>
#include <machine/armreg.h>
#include <machine/zone_manager_asm.h>

/*
PLAN:

* If we can build a safe exception handler, we don't actually need to set breakpoints
  on the watchpoint control registers. Rather, we just need a breakpoint at the end
  of the reconfiguration sequence that we know we'll hit since an exception will always
  rocket us back into the exception vector, which we consider to be part of the trusted
  zone manager. On a breakpoint exception (which indicates that we took an 
  exception due to hitting a privileged instruction) or if the PC is in some
  special "privileged routines" code section, we know something nasty is
  happening and that we should bail out.  
  While a similar approach can be used for TTBR0, ERET must be protected
  directly as it can lead to a complete loss of control that we can't claw back.
* We cannot sensibly support preemption for threads executing inside a zone
  as the only way to realistically achieve such a thing is to have a stack per
  zone per thread, aka up to five stacks per kthread which is utterly insane. As
  such, only spin locks, etc. 

Breakpoints list
* ERET
* TTBR0 write
* Safe DAIF write (for performance)
* Guarded return

TODO:
* Mark MMIO watchpoints/breakpoints RO.
* Rip out MDSCR manipulation
* Fix up exception handling
	* Security tests
	* Safe spilling
	* Exit for urgent IRQs
	* Consider supporting thread suspend?
* Allow zone re-entrancy 
	* This may have complications relating to stack overflows but doing so would
	  allow us to 
* Fix up SMH
	* Currently we call out to an enormous attack surface, much of it
	  unprotected. We need exit the zone to grab more pages. This is easier for
	  the pmap since we can return to the pmap with the pages but other zones
	  will need to cross-call.
*/

.macro ZM_ENTER_NO_SAVE
	msr DAIFSet, DAIF_ALL
.endm
.macro ZM_ENTER save_register
	mrs \save_register, DAIF
	ZM_ENTER_NO_SAVE
.endm

/*
Exit the zone manager.
If save_register is x15, taints only lr, x15
Otherwise, taints lr, x15, x0
 */
.macro ZM_EXIT save_register
.ifc \save_register, x15
	bl safe_restore_interrupts_x15
.else
	mov x0, \save_register
	bl zm_safe_restore_interrupts
.endif
.endm

/*
Exit the zone manager and tail-returns.
If save_register is x15, taints only lr, x15
Otherwise, taints lr, x15, x0
 */
.macro ZM_EXIT_TAIL save_register
.ifc \save_register, x15
	b safe_restore_interrupts_x15
.else
	mov x0, \save_register
	b zm_safe_restore_interrupts
.endif
.endm

/*
 * Fetches the secure per-cpu data pointer.
 * Assumes that we're already in the zone manager.
 * Triggers a panic if authentication fails.
 * NOTE: This function VIOLATES the ARM ABI.
 * This is done intentionally to avoid having to spill registers here.
 * This function returns on x9 and additionally taints x10, x11
 */
LENTRY(outlined_get_secure_pcpu_ptr)
	/* 
	The thread pointer is stored in x18, the platform register. This data
	structure holds a software friendly CPU index. We'll use this as an 
	accelerator to locate the correct secure PCPU. We do, however, need to
	ensure that this information is accurate using the secure MPIDR so that we
	know we were pointed to the right place.

	Note: We are loading a potentially malicious pointer. This is okay because
	we do not support no-access zones and because we have a mechanism to safely
	handle exceptions taken in ZM.
	*/
	ldr w10, [x18, #PC_CPUID]

	/* Bounds check the CPU ID */
	adrp x9, zm_pcpu_count
	ldr w9, [x9, :lo12:zm_pcpu_count]
	cmp w10, w9
	b.cs Lzm_get_pcpu_ptr_cpu_oob

	/* Get the PCPU ptr */	
	mov x11, #ZM_PCPU_SIZE
	umull x10, w10, w11 
	adrp x9, zm_pcpus
	ldr x9, [x9, :lo12:zm_pcpus]
	add x9, x9, x10

	/* Did the insecure PCPU lead us to the right place? */
	ldr w10, [x9, #ZM_PCPU_MPIDR]
	mrs x11, MPIDR_EL1
	/* We want to mask 0xff00ffffff to get Aff3-0 */
	and x11, x11, #0xffffffffff
	bic x11, x11, #0x00ff000000
	cmp w10, w11
	b.ne Lzm_get_cpu_ptr_identity_fail

	/* Secure PCPU ptr okay! */
	ret

Lzm_get_pcpu_ptr_cpu_oob:
	adrp x0, Lget_pcpu_ptr_oob_panic_str
	add x0, x0, :lo12:Lget_pcpu_ptr_oob_panic_str
	b panic_entered

Lzm_get_cpu_ptr_identity_fail:
	adrp x0, Lzm_get_cpu_ptr_identity_fail_str
	add x0, x0, :lo12:Lzm_get_cpu_ptr_identity_fail_str
	b panic_entered
LEND(outlined_get_secure_pcpu_ptr)

.macro get_secure_pcpu_ptr dst
	mrs \dst, DBGWVR0_EL1
.endm

/*
If we attempted to transit through the zone manager while in a panicked state,
we will hang here.
*/
LENTRY(zm_hang_panic)
	b .
LEND(zm_hang_panic)

#if 0
/*
Restore all interrupt state except for debug state from x0
Callable in any context.
*/
ENTRY(zm_safe_restore_interrupts)
	/* This isn't a real zone manager function, so don't bother entering */
	msr DAIFClr, DAIF_D
0:
	tbz x0, DAIF_A, 0f
	msr DAIFSet, DAIF_A
	b 1f
0:
	msr DAIFClr, DAIF_A
1:

	tbz x0, DAIF_I, 0f
	msr DAIFSet, DAIF_I
	b 1f
0:
	msr DAIFClr, DAIF_I
1:

	tbz x0, DAIF_F, 0f
	msr DAIFSet, DAIF_F
	b 1f
0:
	msr DAIFClr, DAIF_F
1:
	ret
END(zm_safe_restore_interrupts)
#endif

ENTRY(zm_safe_restore_interrupts)
	ZM_ENTER_NO_SAVE
	mov x15, x0
safe_restore_interrupts_x15:
	/* 
	Allow jumping here, i.e. skipping x0 and exception disable, if we're on the
	ZM_EXIT fast path where we are already in ZM and have the restore data in 
	x15.
	*/
	bic x15, x15, #DAIF_D
safe_restore_interrupts_breakpoint:
	msr DAIF, x15
	ret
END(zm_safe_restore_interrupts)

/*
 * Enter a zone ID in x0 with auxiliary argument in x1
 */
ENTRY(zm_zone_enter)
	/* 
	Enter the zone manager, saving DAIF into x15.
	Calling this forces all code below to be executed, lest we panic on the 
	watchpoint disable routine
	*/
	ZM_ENTER x15

	/* Save off LR temporarily */
	mov x14, lr

	/* Is the zone ID valid? Note that we compare x0, which enforces [63:32]=0*/
	cmp x0, #ZONE_STATE_POS_MAX
	b.cs Lzm_zone_enter_oob_zone_id

	/* 
	Load the dispatch function -- we do this as early as possible for
	load scheduling reasons
	*/
	adrp x13, zm_dispatch_functions
	add x13, x13, :lo12:zm_dispatch_functions
	ldr x13, [x13, x0, LSL #3 /* 8 byte pointers */]

/* 
Disable panic lockout for now; it's a nice security bonus but not strictly
necessary. 
*/
#if 0
	/*
	If we've hit a panic in any zone, we want to prevent both re-entry and any
	further harm. Hang if we're panicked.
	*/
	adrp x9, zm_globals
	ldr x9, [x9, :lo12:zm_globals]
	ldr w10, [x9, #ZM_GLOBALS_IS_PANICKED]
	cbnz w10, zm_hang_panic
#endif

#if 0
	/* ABI violating function: returns in x9, taints x10, x11 */	
	bl outlined_get_secure_pcpu_ptr
#endif
	get_secure_pcpu_ptr x9

	/* 
	Load the stack pointer for this zone 
	Done early for scheduling
	*/
	add x12, x9, #ZM_PCPU_ZONE_STACKS
	ldr x12, [x12, x0, LSL #3 /* 8 byte pointers */]

	/* 
	Can we enter the zone on this CPU at this time? 
	Only allow entry into the zone if the CPU is coming from kernel mode.
	*/
	ldr w10, [x9, #ZM_PCPU_STATE]
	cmp w10, #ZONE_STATE_NONE
	b.ne Lzm_zone_enter_invalid_state

	/* All tests passed! Okay to enter zone! */
	/* Mark as having entered the zone */
	str w0, [x9, #ZM_PCPU_STATE]
	
	/* Disable watchpoint w0 to activate the zone. Taints x10. */
	bl outlined_watchpoint_disable_one

	/* 
	Spill necessary data on to the private stack.
	This technically gives the target zone control over the host kernel as they
	can manipulate the return address. This is not something we care about,
	however, because RW in a zone is a superset of kernel RW as zones can 
	access all of kernel memory.
	*/
	/* SP and LR, skip FP because it's not strictly necessary */
	mov x11, sp
	stp x11, x14, [x12, #-0x10]!
	/* Pivot */
	mov sp, x12

	/* Dispatch! */
	/* 
	Exit and re-enable the exceptions from our caller
	Taints only x15, lr if argument is x15 
	*/
	ZM_EXIT x15
	
	/* Move argument into place */
	mov x0, x1
	/* Yeet :) */
	blr x13

	/*
							.~* ZONE RETURN *~.
	*/
	/* 
	While we're returning from a zone, we have to do much of the same 
	verification since we exited the zone manager and don't necessarily 
	trust the zone to not be maliciously invoking us.
	*/
	ZM_ENTER x15

#if 0
	/* ABI violating function: returns in x9, taints x10, x11 */	
	bl outlined_get_secure_pcpu_ptr
#endif
	get_secure_pcpu_ptr x9

	/* Check: are we exiting after having actually entered a zone? */
	ldr w12, [x9, #ZM_PCPU_STATE]
	/* States where we're in a zone are zero or positive */
	cmp w12, #0
	b.lt Lzone_return_invalid_state
	mov w10, #ZONE_STATE_NONE
	str w10, [x9, #ZM_PCPU_STATE]

	/* State okay! Go for exit */

	/* Re-enable the watchpoint from the zone we just exited, x12. Taint x10. */
	bl outlined_watchpoint_enable_one

	/* 
	Restore interrupts and exit ZM. 	
	Taints only x15, lr if argument is x15  
	*/
	ZM_EXIT x15

	/*
	Stack pivot back
	*/
	ldp x10, lr, [sp], #0x10
	mov sp, x10
	ret 

Lzm_zone_enter_oob_zone_id:
	adrp x0, Lzm_zone_enter_oob_zone_id_str
	add x0, x0, :lo12:Lzm_zone_enter_oob_zone_id_str
	b panic_entered

Lzm_zone_enter_invalid_state:
	adrp x0, Lzm_zone_enter_invalid_state_str
	add x0, x0, :lo12:Lzm_zone_enter_invalid_state_str
	b panic_entered

Lzone_return_invalid_state:
	adrp x0, Lzone_return_invalid_state_str
	add x0, x0, :lo12:Lzone_return_invalid_state_str
	b panic_entered
END(zm_zone_enter)

LENTRY(guarded_return)
guarded_return_breakpoint:
	ret
LEND(guarded_return)

/*
 * Disables a single watchpoint, indexed by w0.
 * Precondition: 0 <= w0 < 4
 * This is an outlined function which violates the ABI: taints x10.
 * Requires the zone manager have already been entered.
 */
LENTRY(outlined_watchpoint_disable_one)
	/*
	 * Calculate the switch target.
	 * Note: This is actually safe despite the indirect branch since w0 is
	 * bounded is by a precondition.
	 */
	adr x10, 0f
	add x10, x10, x0, LSL #4 /* skip 16 bytes/4 instructions per index */
	br x10

0:
	/* WCR0 */
	mrs x10, DBGWCR0_EL1
	bic x10, x10, #DBGWCR_EN
	msr DBGWCR0_EL1, x10
	b 0f

	/* WCR1 */
	mrs x10, DBGWCR1_EL1
	bic x10, x10, #DBGWCR_EN
	msr DBGWCR1_EL1, x10
	b 0f

	/* WCR2 */
	mrs x10, DBGWCR2_EL1
	bic x10, x10, #DBGWCR_EN
	msr DBGWCR2_EL1, x10
	b 0f

	/* WCR3 */
	mrs x10, DBGWCR2_EL1
	bic x10, x10, #DBGWCR_EN
	msr DBGWCR2_EL1, x10
	b 0f
0:
	/* Wait for configuration to apply */
	isb
	/* Force an exception if we were ROP'd */
	b guarded_return
LEND(outlined_watchpoint_disable_one)

/*
 * Enables a single watchpoint, indexed by x12.
 * Taints x10.
 */
LENTRY(outlined_watchpoint_enable_one)
	/*
	 * Calculate the switch target.
	 */
	adr x10, 0f
	add x10, x10, x12, LSL #4 /* skip 16 bytes/4 instructions per index */
	br x10

0:
	/* WCR0 */
	mrs x10, DBGWCR0_EL1
	orr x10, x10, #DBGWCR_EN
	msr DBGWCR0_EL1, x10
	b 0f

	/* WCR1 */
	mrs x10, DBGWCR1_EL1
	orr x10, x10, #DBGWCR_EN
	msr DBGWCR1_EL1, x10
	b 0f

	/* WCR2 */
	mrs x10, DBGWCR2_EL1
	orr x10, x10, #DBGWCR_EN
	msr DBGWCR2_EL1, x10
	b 0f

	/* WCR3 */
	mrs x10, DBGWCR2_EL1
	orr x10, x10, #DBGWCR_EN
	msr DBGWCR2_EL1, x10
	b 0f
0:
	/* Wait for configuration to apply */
	isb
	/* Force an exception if we were ROP'd */
	b guarded_return
LEND(outlined_watchpoint_enable_one)


ENTRY(zone_manager_set_ttbr0)
	msr TTBR0_EL1, x0
	isb
	b guarded_return
END(zone_manager_set_ttbr0)

/*
 Called when we took a panic while inside the zone manager.
 Sets panic and exits.
 */
LENTRY(panic_entered)
	adrp x9, zm_globals
	add x9, x9, :lo12:zm_globals
	mov x10, #1
	str w10, [x9, ZM_GLOBALS_IS_PANICKED]
	mov x19, x0
	mov x0, DAIF_ALL
	ZM_EXIT x0
	mov x0, x19
	b panic
LEND(panic_entered)

ENTRY(dispatch_test_nop)
	ret
END(dispatch_test_nop)

/*
Boot time configuration of the debug registers. Expects an array of four
zm_init_watchpoint_config_s items describing watchpoint configuration.
This function must be called on each CPU.
This function is unmapped after boot is complete.
*/
ZM_NX_ENTRY(zm_init_debug)
	/*
	Since this is a late NX function, we can do not need to guard here. 
	Configure the registers directly.
	*/
	ldp x9, x10, [x0], #0x10
	msr DBGWCR0_EL1, x9
	msr DBGWVR0_EL1, x10

	ldp x9, x10, [x0], #0x10
	msr DBGWCR1_EL1, x9
	msr DBGWVR1_EL1, x10

	ldp x9, x10, [x0], #0x10
	msr DBGWCR2_EL1, x9
	msr DBGWVR2_EL1, x10

	ldp x9, x10, [x0], #0x10
	msr DBGWCR3_EL1, x9
	msr DBGWVR3_EL1, x10

	/* Setup breakpoints */
	mov x9, #(DBGBCR_BAS | DBGBCR_EN | DBGBCR_PMC_EL1)

	ldr x10, =safe_restore_interrupts_breakpoint
	msr DBGBCR0_EL1, x9
	msr DBGBVR0_EL1, x10

	ldr x10, =guarded_return_breakpoint
	msr DBGBCR1_EL1, x9
	msr DBGBVR1_EL1, x10
	isb
	ret
END(zm_init_debug)

.section .rodata
Lsafe_restore_debug_exception_panic_str:
.string "Masking debug exceptions is not permitted in safe handlers!"
Lget_pcpu_ptr_oob_panic_str:
.string "Invalid CPU ID (out of bounds?)"
Lzm_zone_enter_oob_zone_id_str:
.string "Invalid zone ID (out of bounds?)"
Lzm_zone_enter_invalid_state_str:
.string "CPU in an invalid state; refusing to enter zone!"
Lzone_return_invalid_state_str:
.string "CPU in an invalid state; refusing to exit zone!"
Lzm_get_cpu_ptr_identity_fail_str:
.string "Insecure PCPU pointing to wrong secure PCPU?"