/*-
 * Copyright (C) 2010 Nathan Whitehorn
 * Copyright (C) 2011 glevand (geoffrey.levand@mail.ru)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* Hypercall stubs. Note: this is all a hack and should die. */

#define	hc	.long   0x44000022

#define LD64_IM(r, highest, higher, high, low) \
	lis	r,highest; \
	addi	r,r,higher; \
	sldi	r,r,32; \
	addis	r,r,high; \
	addi	r,r,low;

#define SIMPLE_HVCALL(x, c) \
.global x;			\
x:				\
	mflr	%r0;		\
	stw	%r0,4(%r1);	\
	clrldi	%r3,%r3,32;	\
	clrldi	%r4,%r4,32;	\
	clrldi	%r5,%r5,32;	\
	clrldi	%r6,%r6,32;	\
	clrldi	%r7,%r7,32;	\
	clrldi	%r8,%r8,32;	\
	clrldi	%r9,%r9,32;	\
	clrldi	%r10,%r10,32;	\
	li	%r11,c;		\
	hc;			\
	extsw	%r3,%r3;	\
	lwz	%r0,4(%r1);	\
	mtlr	%r0;		\
	blr

SIMPLE_HVCALL(lv1_open_device, 170)
SIMPLE_HVCALL(lv1_close_device, 171)
SIMPLE_HVCALL(lv1_gpu_open, 210)
SIMPLE_HVCALL(lv1_gpu_context_attribute, 225)
SIMPLE_HVCALL(lv1_panic, 255)
SIMPLE_HVCALL(lv1_net_start_tx_dma, 187)
SIMPLE_HVCALL(lv1_net_stop_tx_dma, 188)
SIMPLE_HVCALL(lv1_net_start_rx_dma, 189)
SIMPLE_HVCALL(lv1_net_stop_rx_dma, 190)

.global lv1_get_physmem
lv1_get_physmem:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r3,-8(%r1)	/* Address for maxmem */

	li	%r11,69		/* Get PU ID */
	hc
	std	%r4,-16(%r1)

	li	%r11,74		/* Get LPAR ID */
	hc
	std	%r4,-24(%r1)

	ld	%r3,-24(%r1)
	LD64_IM(%r4,0x0000,0x0000,0x6269,0x0000 /* "bi" */)
	LD64_IM(%r5,0x7075,0x0000,0x0000,0x0000 /* "pu" */)
	ld	%r6,-16(%r1)
	LD64_IM(%r7,0x726d,0x5f73,0x697a,0x6500 /* "rm_size" */)
	li	%r11,91
	hc
	extsw	%r3,%r3

	lwz	%r5,-8(%r1)
	std	%r4,0(%r5)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_setup_address_space
lv1_setup_address_space:
	mflr	%r0
	stw	%r0,4(%r1)

	stw	%r3,-4(%r1)
	stw	%r4,-8(%r1)

	li	%r3,18		/* PT size: log2(256 KB) */
	li	%r4,2		/* Two page sizes */
	li	%r5,24		/* Page sizes: (24 << 56) | (16 << 48) */
	sldi	%r5,%r5,24
	li	%r6,16
	sldi	%r6,%r6,16
	or	%r5,%r5,%r6
	sldi	%r5,%r5,32

	li	%r11,2		/* lv1_construct_virtual_address_space */
	hc

	lwz	%r6,-4(%r1)
	lwz	%r7,-8(%r1)
	std	%r4,0(%r6)
	std	%r5,0(%r7)

	/* AS_ID in r4 */
	mr	%r3,%r4
	li	%r11,7		/* lv1_select_virtual_address_space */
	hc
	extsw	%r3,%r3

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_insert_pte
lv1_insert_pte:
	mflr	%r0
	stw	%r0,4(%r1)

	mr	%r11,%r4	/* Save R4 */

	clrldi	%r3,%r3,32
	clrldi	%r7,%r5,32

	sldi	%r4,%r3,3	/* Convert ptegidx into base PTE slot */
	li	%r3,0		/* Current address space */
	ld	%r5,0(%r11)
	ld	%r6,8(%r11)
	li	%r8,0		/* No other flags */
	
	li	%r11,158
	hc
	extsw	%r3,%r3

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_gpu_context_allocate
lv1_gpu_context_allocate:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r7,-4(%r1)

	sldi	%r3,%r3,32
	clrldi	%r4,%r4,32
	or	%r3,%r3,%r4
	clrldi	%r4,%r5,32
	clrldi	%r5,%r6,32

	li	%r11,217
	hc
	extsw	%r3,%r3

	lwz	%r7,-4(%r1)
	std	%r4,0(%r7)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr
	
.global lv1_gpu_memory_allocate
lv1_gpu_memory_allocate:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r8,-4(%r1)
	stw	%r9,-8(%r1)

	li	%r11,214
	hc
	extsw	%r3,%r3

	lwz	%r8,-4(%r1)
	lwz	%r9,-8(%r1)
	std	%r4,0(%r8)
	std	%r5,0(%r9)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_net_control
lv1_net_control:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r9,-4(%r1)

	li	%r11,194
	hc
	extsw	%r3,%r3

	lwz	%r8,-4(%r1)
	std	%r4,0(%r8)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_setup_dma
lv1_setup_dma:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r3,-4(%r1)
	stw	%r4,-8(%r1)
	stw	%r5,-12(%r1)

	lwz	%r3,-4(%r1)
	lwz	%r4,-8(%r1)
	lis	%r5,0x0800	/* 128 MB */
	li	%r6,24		/* log2(IO_PAGESIZE) */
	li	%r7,0		/* flags */
	li	%r11,174	/* lv1_allocate_device_dma_region */
	hc
	extsw	%r3,%r3
	cmpdi	%r3,0
	bne	1f
	std	%r4,-24(%r1)

	lwz	%r3,-4(%r1)
	lwz	%r4,-8(%r1)
	li	%r5,0
	ld	%r6,-24(%r1)
	lis	%r7,0x0800	/* 128 MB */
	lis	%r8,0xf800	/* flags */
	sldi	%r8,%r8,32
	li	%r11,176	/* lv1_map_device_dma_region */
	hc
	extsw	%r3,%r3

	lwz	%r9,-12(%r1)
	ld	%r6,-24(%r1)
	std	%r6,0(%r9)

1:	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_get_repository_node_value
lv1_get_repository_node_value:
	mflr	%r0
	stw	%r0,4(%r1)

	sldi	%r3,%r3,32
	clrldi	%r4,%r4,32
	or	%r3,%r3,%r4
	sldi	%r4,%r5,32
	clrldi	%r5,%r6,32
	or	%r4,%r4,%r5
	sldi	%r5,%r7,32
	clrldi	%r6,%r8,32
	or	%r5,%r5,%r6
	sldi	%r6,%r9,32
	clrldi	%r7,%r10,32
	or	%r6,%r6,%r7
	lwz     %r7,8(%r1)
	lwz     %r8,12(%r1)
	sldi	%r7,%r7,32
	or	%r7,%r7,%r8

	li	%r11,91
	hc
	extsw	%r3,%r3

	lwz	%r6,16(%r1)
	std	%r4,0(%r6)
	lwz	%r6,20(%r1)
	std	%r5,0(%r6)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_storage_read
lv1_storage_read:
	mflr	%r0
	stw	%r0,4(%r1)

	sldi	%r3,%r3,32
	clrldi	%r4,%r4,32
	or	%r3,%r3,%r4
	sldi	%r4,%r5,32
	clrldi	%r5,%r6,32
	or	%r4,%r4,%r5
	sldi	%r5,%r7,32
	clrldi	%r6,%r8,32
	or	%r5,%r5,%r6
	sldi	%r6,%r9,32
	clrldi	%r7,%r10,32
	or	%r6,%r6,%r7
	lwz     %r7,8(%r1)
	lwz     %r8,12(%r1)
	sldi	%r7,%r7,32
	or	%r7,%r7,%r8
	lwz     %r8,16(%r1)
	lwz     %r9,20(%r1)
	sldi	%r8,%r8,32
	or	%r8,%r8,%r9

	li	%r11,245
	hc
	extsw	%r3,%r3

	lwz	%r5,24(%r1)
	std	%r4,0(%r5)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_storage_check_async_status
lv1_storage_check_async_status:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r7,-4(%r1)

	sldi	%r3,%r3,32
	clrldi	%r4,%r4,32
	or	%r3,%r3,%r4
	sldi	%r4,%r5,32
	clrldi	%r5,%r6,32
	or	%r4,%r4,%r5

	li	%r11,254
	hc
	extsw	%r3,%r3

	lwz	%r5,-4(%r1)
	std	%r4,0(%r5)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr
