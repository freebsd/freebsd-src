/*
 * Switch back to real mode and call the BIOS reboot vector.
 * This is a trampoline copied around in process.c 
 * Written 2003 by Andi Kleen, SuSE Labs.	 	
 */			
	
#include <asm/msr.h>

#define R(x) x-warm_reboot(%ebx)
#define R64(x) x-warm_reboot(%rbx)
		
	/* running in identity mapping and in the first 64k of memory
	   and in compatibility mode. This must be position independent */

	/* Follows 14.7 "Leaving Long Mode" in the AMD x86-64 manual, volume 2
	   and 8.9.2 "Switching Back to Real-Address Mode" in the Intel IA32
	   manual, volume 2 */

	/* ebx:	self pointer to warm_reboot */

	.globl warm_reboot
warm_reboot:
	addl  %ebx,  R64(real_mode_desc)	/* relocate tables */
	addl  %ebx,2+R64(warm_gdt_desc)		

	movq  %cr0,%rax
	btr  $31,%rax		
	movq %rax,%cr0		/* disable paging */
	jmp  1f			/* flush prefetch queue */
	
	.code32	
1:	movl $MSR_EFER,%ecx
	rdmsr
	andl $~((1<<_EFER_LME)|(1<<_EFER_SCE)|(1<<_EFER_NX)),%eax
	wrmsr			/* disable long mode in EFER */
		
	xorl %eax,%eax
	movl %eax,%cr3		/* flush tlb */

	/* Running protected mode without paging now */
	
	wbinvd			/* flush caches. Needed? */

	lidt R(warm_idt_desc)
	lgdt R(warm_gdt_desc)
	
	movl $0x10,%ecx		/* load segment registers with real mode settings */
	movl %ecx,%ds
	movl %ecx,%es
	movl %ecx,%fs
	movl %ecx,%gs
	movl %ecx,%ss
	
	lea  R(real_mode_desc),%eax		
	ljmp *(%eax)

	.code16:
real_mode:
	xorl %eax,%eax
	movl %eax,%cr0
	
	/* some people claim $0xf000,0xfff0 is better. Use what 32bit linux uses. */
	/* code as bytes because gas has problems with it */
	.byte 	0xea,0xf0,0xff,0x00,0xf0	/* ljmp  0xf000:0xfff0 */

real_mode_desc:
	.long  real_mode - warm_reboot
	.short 8	
warm_gdt_desc:
	.short 8*3
	.long warm_gdt - warm_reboot
warm_gdt:	
	.quad   0
	.quad 	0x00009a000000ffff	/* 16-bit real-mode 64k code at 0x00000000 */
	.quad   0x000092000100ffff	/* 16-bit real-mode 64k data at 0x00000100 */
	
warm_idt_desc:
	.short 0x3ff
	.long  0
				
	.globl warm_reboot_end
warm_reboot_end:

