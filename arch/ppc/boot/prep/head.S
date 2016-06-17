#include <asm/ppc_asm.h>
#include <asm/processor.h>
#include <asm/cache.h>

	.text

/*
 * Boot loader philosophy:
 *      ROM loads us to some arbitrary location
 *      Move the boot code to the link address (8M)
 *      Call decompress_kernel()
 *         Relocate the initrd, zimage and residual data to 8M
 *         Decompress the kernel to 0
 *      Jump to the kernel entry
 *            -- Cort
 */
	.globl	start
start:
	bl	start_
start_:

        /* Enable, invalidate, Disable L1 icache/dcache */
	li	r8, 0
	ori	r8, r8, (HID0_ICE|HID0_DCE|HID0_ICFI|HID0_DCI)
	mfspr	r11,HID0
	or	r11,r11,r8
	andc	r10,r11,r8
	isync
	mtspr	HID0,r8
	sync
	isync
	mtspr	HID0,r10
	sync
	isync

	mr	r11,r3		/* Save pointer to residual/board data */

/*
 * Save the OF pointer to r25, but only if the entry point is in a sane
 * location; if not we store 0.  If there is no entry point, or it is
 * invalid, we establish the default MSR value immediately.  Otherwise,
 * we defer doing that, to allow OF functions to be called, until we
 * begin uncompressing the kernel.
 */
	lis	r3,0x0fff		/* r3 = 0x0fffffff */
	ori	r3,r3,0xffff

	subc	r3,r3,r5		/* r3 = (r5 <= r3) ? ~0 : 0 */
	subfe	r3,r3,r3
	nand	r3,r3,r3
  
	and.	r5,r5,r3		/* r5 will be cleared if (r5 > r3) */
	bne+	haveOF

	li	r3,MSR_IP|MSR_FP	/* Not OF: set MSR immediately */
  	mtmsr	r3
	isync
haveOF:
	mr	r25,r5

	/* compute the size of the whole image in words. */
	lis	r4,start@h
	ori	r4,r4,start@l
	lis	r5,end@h
	ori	r5,r5,end@l
	addi	r5,r5,3		/* round up */
	sub	r5,r5,r4	/* end - start */
	srwi	r5,r5,2
	mr	r7,r5		/* Save for later use. */

	/* check if we need to relocate ourselves to the link addr or were
	 * we loaded there to begin with -- Cort */
	mflr	r3
	subi	r3,r3,4		/* we get the nip, not the ip of the branch */
	mr	r8,r3
	cmp	0,r3,r4
	beq	start_ldr	/* If 0, we don't need to relocate */
/*
 * no matter where we're loaded, move ourselves to -Ttext address
 */
relocate:
	mflr	r3		/* Compute code bias */
	subi	r3,r3,4
	mr	r8,r3
	lis	r4,start@h
	ori	r4,r4,start@l
	mr	r5,r7		/* Get the # of longwords again */
	mtctr	r5		/* Setup for loop */
	li	r6,0
	subi	r3,r3,4
	subi	r4,r4,4
00:	lwzu	r5,4(r3)
	stwu	r5,4(r4)
	xor	r6,r6,r5
	bdnz	00b
  	lis	r3,start_ldr@h
	ori	r3,r3,start_ldr@l
	mtlr	r3		/* Easiest way to do an absolute jump */
	blr

start_ldr:
/* Some boards don't boot up with the I-cache enabled.  Do that
 * now because the decompress runs much faster that way.
 * As a side effect, we have to ensure the data cache is not enabled
 * so we can access the serial I/O without trouble.
 */
	bl	flush_instruction_cache

/* Clear all of BSS */
	lis	r3,edata@h
	ori	r3,r3,edata@l
	lis	r4,end@h
	ori	r4,r4,end@l
	subi	r3,r3,4
	subi	r4,r4,4
	li	r0,0
50:	stwu	r0,4(r3)
	cmp	0,r3,r4
	bne	50b
90:	mr	r9,r1			/* Save old stack pointer (in case it matters) */
	lis	r1,.stack@h
	ori	r1,r1,.stack@l
	addi	r1,r1,4096*2
	subi	r1,r1,256
	li	r2,0x000F		/* Mask pointer to 16-byte boundary */
	andc	r1,r1,r2

/* Run loader */
	mr	r3,r8			/* Load point */
	mr	r4,r7			/* Program length */
	mr	r5,r6			/* Checksum */
	mr	r6,r11			/* Residual data */
	mr      r7,r25                  /* Validated OFW interface */
	bl	decompress_kernel

	/*
	 * We have to do this after decompress_kernel, just to make
	 * sure we don't wipe out things mapped in BATs which we need.
	 * -- Tom
	 */
	li      r6,0
	/* Test for a 601 */
	mfspr	r9,PVR
	srwi	r9,r9,16
	cmpi	0,r9,1          /* 601 ? */
	beq	.clearbats_601

	/* Clear BATS */
	mtspr   DBAT0U,r6
	mtspr	DBAT0L,r6
	mtspr   DBAT1U,r6
	mtspr	DBAT1L,r6
	mtspr   DBAT2U,r6
	mtspr	DBAT2L,r6
	mtspr   DBAT3U,r6
	mtspr	DBAT3L,r6
.clearbats_601:
	mtspr   IBAT0U,r6
	mtspr	IBAT0L,r6
	mtspr   IBAT1U,r6
	mtspr	IBAT1L,r6
	mtspr	IBAT2U,r6
	mtspr   IBAT2L,r6
	mtspr   IBAT3U,r6
	mtspr	IBAT3L,r6
	isync
	sync
	sync

	/* Set segment registers */
	li	r6,16		/* load up segment register values */
	mtctr	r6		/* for context 0 */
	lis	r6,0x2000	/* Ku = 1, VSID = 0 */
	li	r10,0
3:	mtsrin	r6,r10
	addi	r6,r6,0x111	/* increment VSID */
	addis	r10,r10,0x1000	/* address of next segment */
	bdnz	3b

	/* tell kernel we're prep, by putting 0xdeadc0de at KERNELLOAD,
	 * and tell the kernel to start on the 4th instruction since we
	 * overwrite the first 3 sometimes (which are 'nop').
	 */
	li	r9,0xc
	mtlr	r9
	lis	r10,0xdeadc0de@h
	ori	r10,r10,0xdeadc0de@l
	li	r9,0
	stw	r10,0(r9)
	blr

	.comm	.stack,4096*2,4
