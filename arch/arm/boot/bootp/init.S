/*
 *  linux/arch/arm/boot/bootp/init.S
 *
 *  Copyright (C) 2000-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Header file for splitting kernel + initrd.  Note that we pass
 *  r0 through to r3 straight through.
 */
		.section .start,#alloc,#execinstr
		.type	_start, #function
		.globl	_start
_start:		adr	r10, initdata
		ldr	r11, initdata
		sub	r11, r10, r11		@ work out exec offset
		b	splitify
		.size	_entry,. - _entry

		.type	initdata, #object
initdata:	.word	initdata		@ compiled address of this
		.size	initdata,. - initdata

splitify:	adr	r13, data
		ldmia	r13!, {r4-r6}		@ move the initrd
		add	r4, r4, r11		@ correction
		bl	move

/*
 * Setup the initrd parameters to pass to the kernel.  This can either be
 * passed in via a param_struct or a tag list.  We spot the param_struct
 * method by looking at the first word; this should either indicate a page
 * size of 4K, 16K or 32K.
 */
		ldmia	r13, {r4-r8}		@ get size and addr of initrd
						@ r5 = ATAG_INITRD
						@ r6 = initrd start
						@ r7 = initrd end
						@ r8 = param_struct address
		ldr	r9, [r8, #0]		@ no param struct?
		teq	r9, #0x1000		@ 4K?
		teqne	r9, #0x4000		@ 16K?
		teqne	r9, #0x8000		@ 32K?
		beq	param_struct

		ldr	r9, [r8, #4]		@ get first tag
		teq	r9, r4
		beq	taglist			@ ok, we have a tag list

/*
 * We didn't find a valid tag list - create one.
 */
		str	r4, [r8, #4]
		mov	r4, #(8 >> 2)
		str	r4, [r8, #0]
		mov	r4, #0
		str	r4, [r8, #8]
/*
 * find the end of the tag list, and then add an INITRD tag on the end.
 * If there is already an INITRD tag, then we ignore it; the last INITRD
 * tag takes precidence.
 */
taglist:	ldr	r9, [r8, #0]		@ tag length
		teq	r9, #0			@ last tag?
		addne	r8, r8, r9, lsl #2
		bne	taglist
		mov	r4, #(16 >> 2)		@ length of initrd tag
		mov	r9, #0			@ end of tag list terminator
		stmia	r8, {r4, r5, r6, r7, r9}
		adr	r12, kernel_start
		mov	pc, r12			@ call kernel

/*
 * We found a param struct.  Modify the param struct for the initrd
 */
param_struct:	add	r8, r8, #16*4
		stmia	r8, {r6,r7}		@ save in param_struct
		adr	r12, kernel_start
		mov	pc, r12			@ call kernel

move:		ldmia	r4!, {r7 - r10}		@ move 32-bytes at a time
		stmia	r5!, {r7 - r10}
		ldmia	r4!, {r7 - r10}
		stmia	r5!, {r7 - r10}
		subs	r6, r6, #8 * 4
		bcs	move
		mov	pc, lr

data:		.word	initrd_start
		.word	initrd_addr
		.word	initrd_len

		.word	0x54410001		@ r4 = ATAG_CORE
		.word	0x54420005		@ r5 = ATAG_INITRD
		.word	initrd_addr		@ r6
		.word	initrd_len		@ r7
		.word	params			@ r8

		.type	initrd_start,#object

kernel_start:
