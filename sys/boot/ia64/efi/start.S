/*-
 * Copyright (c) 2001 Doug Rabson
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
 *	$FreeBSD: src/sys/boot/ia64/efi/start.S,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

	.text

#include <machine/asm.h>

#define EFI_SUCCESS		0
#define EFI_LOAD_ERROR		1
#define EFI_BUFFER_TOO_SMALL	5

#define DT_NULL		0	/* Terminating entry. */
#define DT_PLTRELSZ	2	/* Total size in bytes of PLT relocations. */
#define DT_SYMTAB	6	/* Address of symbol table. */
#define DT_RELA		7	/* Address of ElfNN_Rela relocations. */
#define DT_RELASZ	8	/* Total size of ElfNN_Rela relocations. */
#define DT_RELAENT	9	/* Size of each ElfNN_Rela relocation entry. */
#define DT_SYMENT	11	/* Size of each symbol table entry. */
#define DT_JMPREL	23	/* Address of PLT relocations. */

#define R_IA_64_NONE		0	/* None */
#define R_IA_64_DIR64LSB	0x27	/* word64 LSB	S + A */
#define R_IA_64_FPTR64LSB	0x47	/* word64 LSB	@fptr(S + A) */
#define R_IA_64_REL32LSB	0x6d	/* word32 LSB	BD + A */
#define R_IA_64_REL64LSB	0x6f	/* word64 LSB	BD + A */
#define	R_IA_64_IPLTLSB		0x81	/* function descriptor LSB speciaal */

ENTRY(_start, 2)
	alloc	loc0=ar.pfs,2,3,3,0
	mov	loc1=rp
	movl	loc2=@gprel(ImageBase)
	;;
	add	loc2=gp,loc2
	;;
	mov	out0=loc2
	mov	out1=in1
	;;
	br.call.sptk.few rp=_reloc	// relocate image
	
	cmp.ne	p6,p0=EFI_SUCCESS,r8	// did it work?
(p6)	br.cond.dpnt.few 9f

	mov	out0=in0		// image_handle
	mov	out1=in1		// system_table
	br.call.sptk.few rp=efi_main
9:
	mov	ar.pfs=loc0
	mov	rp=loc1
	;;
	br.ret.sptk.few rp
END(_start)

	// PLABEL for PE32+
	.section .plabel, "a"
	.align	16
	.global _start_plabel
_start_plabel:
	data16	@iplt(_start)
	.previous

	// A PE32+ relocation entry for the plabel
	.section .reloc, "a"
	data4	_start_plabel
	data4	12
	data2	(10 << 12) + 0
	data2	(10 << 12) + 8
	.previous

// in0:	image base
// in1:	system table
//
// XXX Assumes PLT relocations are of type Elf_Rela
//
// r2 = address of fptr_storage
// r3 = address of fptr_storage_end
// r4 = address of first free fptr
//
// r15 = r_offset
// r16 = r_info		-OR-	d_tag
// r17 = r_addend	-OR-	d_val (=d_ptr)
// r18 = address of .rela dynamic section
// r19 = size of .rela section
// r20 = size of .rela element (Elf_Rela)
// r21 = address of first PLT relocation
// r22 = size of PLT relocations
// r23 = relocation type
// r24 = address of symbol
// r28 = R_IA_64_IPLTLSB
// f8 = address of symbol table
// f9 = size of symtab element

STATIC_ENTRY(_reloc, 2)
	alloc	loc0=ar.pfs,2,2,0,0
	;;
	mov	loc1=rp
	movl	r29=@gprel(_DYNAMIC)	// find _DYNAMIC etc.
	;;
	add	r15=r29,gp
	movl	r29=@gprel(fptr_storage)
	;;
	add	r2=r29,gp
	movl	r29=@gprel(fptr_storage_end)
	;;
	add	r3=r29,gp
	mov	r4=r2
	mov	r19=0
	mov	r22=0
	mov	r20=24
	mov	r28=R_IA_64_IPLTLSB
	;;
1:
	ld8	r16=[r15],8		// read r15->d_tag
	;;
	ld8	r17=[r15],8		// and r15->d_val
	;;
	cmp.eq	p6,p0=DT_NULL,r16	// done?
(p6)	br.cond.dpnt.few 2f
	;;
	cmp.eq	p6,p0=DT_RELA,r16	// rela section?
	;;
(p6)	add	r18=r17,in0
	cmp.eq	p6,p0=DT_RELASZ,r16	// rela section size?
	;;
(p6)	mov	r19=r17
	cmp.eq	p6,p0=DT_RELAENT,r16	// rela entry size?
	;;
(p6)	mov	r20=r17
	cmp.eq	p6,p0=DT_JMPREL,r16	// PLT relocs?
	;;
(p6)	add	r21=r17,in0
	cmp.eq	p6,p0=DT_PLTRELSZ,r16	// PLT relocs size?
	;;
(p6)	mov	r22=r17
	cmp.eq	p6,p0=DT_SYMTAB,r16	// symbol table?
	;;
(p6)	add	r29=r17,in0
	;;
(p6)	setf.sig f8=r29
	cmp.eq	p6,p0=DT_SYMENT,r16	// symbol entry size?
	;;
(p6)	setf.sig f9=r17
	br.dptk	1b

2:
	cmp.lt	p6,p0=0,r19
(p6)	br.cond.dptk	3f
	;;
	mov	r19=r22
	mov	r18=r21
	mov	r21=0
	mov	r22=0
	;;
	cmp.lt	p6,p0=0,r19
(p6)	br.cond.dptk	3f
	;;
	mov	r8=EFI_SUCCESS
	br.dptk	9f
3:
	ld8	r29=[r18],8		// read r_offset
	;;
	ld8	r16=[r18],8		// read r_info
	add	r15=r29,in0		// relocate r_offset
	;;
	ld8	r17=[r18],8		// read r_addend
	sub	r19=r19,r20		// update relasz
	extr.u	r23=r16,0,32		// ELF64_R_TYPE(r16)
	;;
	cmp.eq	p6,p0=R_IA_64_NONE,r23
(p6)	br.cond.dpnt.few 2b
	;;
	cmp.eq	p6,p0=R_IA_64_REL32LSB,r23
(p6)	br.cond.dptk.few 3f
	;;
	cmp.eq	p6,p0=R_IA_64_REL64LSB,r23
(p6)	br.cond.dptk.few 4f
	;;
	extr.u	r29=r16,32,32		// ELF64_R_SYM(r16)
	;;
	setf.sig f10=r29		// so we can multiply
	;;
	xma.lu	f10=f10,f9,f8		// f10=symtab + r_sym*syment
	;;
	getf.sig r29=f10
	;;
	add	r29=8,r29		// address of st_value
	;;
	ld8	r29=[r29]		// read symbol value
	;;
	add	r24=r29,in0		// relocate symbol value
	;;
	cmp.eq	p6,p0=R_IA_64_DIR64LSB,r23
(p6)	br.cond.dptk.few 5f
	;;
	cmp.eq	p6,p0=R_IA_64_FPTR64LSB,r23
(p6)	br.cond.dptk.few 6f
	;;
	cmp.ne	p6,p0=r28,r23		// IPLTLSB
(p6)	br.cond.dptk.few 2b

	// IPLTLSB
	add	r29=r24,r17		// S + A
	;;
	st8	[r15]=r29,8		// fdesc:FP
	;;
	st8	[r15]=gp		// fdesc:GP
	br.cond.sptk.few 2b

	// REL32LSB
3:
	add	r29=in0,r17
	;;
	st4	[r15]=r29
	br.cond.sptk.few 2b

	// REL64LSB
4:
	add	r29=in0,r17		// BD + A
	;;
	st8	[r15]=r29		// word64
	br.cond.sptk.few 2b

	// DIR64LSB
5:
	add	r29=r24,r17		// S + A
	;;
	st8	[r15]=r29		// word64
	br.cond.sptk.few 2b

6:
	mov	r29=r2			// FPTR64LSB
	;;
7:
	cmp.geu	p6,p0=r29,r4		// end of fptrs?
(p6)	br.cond.dpnt.few 8f		// can't find existing fptr
	ld8	r17=[r29]		// read function from fptr
	;;
	cmp.eq	p6,p0=r24,r17		// same function?
	;;
(p6)	st8	[r15]=r29		// reuse fptr
(p6)	br.cond.sptk.few 2b		// done
	add	r29=16,r29		// next fptr
	br.sptk.few 7b
8:
	mov	r8=EFI_BUFFER_TOO_SMALL	// failure return value
	cmp.geu	p6,p0=r4,r3		// space left?
(p6)	br.cond.dpnt.few 9f		// bail out
	st8	[r15]=r4		// install fptr
	;;
	st8	[r4]=r24,8		// write fptr address
	;;
	st8	[r4]=gp,8		// write fptr gp
	br.cond.sptk.few 2b

9:
	mov	ar.pfs=loc0
	mov	rp=loc1
	;;
	br.ret.sptk.few rp
END(_reloc)

	.data
	.align	16
fptr_storage:
	.space	1024*16			// XXX
fptr_storage_end:
