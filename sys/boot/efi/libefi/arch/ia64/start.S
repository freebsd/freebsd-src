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
 *	$FreeBSD$
 */

	.text

#include <sys/cdefs.h>
#include <machine/asm.h>
		
#define EFI_SUCCESS		0
#define EFI_LOAD_ERROR		1
#define EFI_BUFFER_TOO_SMALL	5
	
#define DT_NULL		0	/* Terminating entry. */
#define DT_NEEDED	1	/* String table offset of a needed shared
				   library. */
#define DT_PLTRELSZ	2	/* Total size in bytes of PLT relocations. */
#define DT_PLTGOT	3	/* Processor-dependent address. */
#define DT_HASH		4	/* Address of symbol hash table. */
#define DT_STRTAB	5	/* Address of string table. */
#define DT_SYMTAB	6	/* Address of symbol table. */
#define DT_RELA		7	/* Address of ElfNN_Rela relocations. */
#define DT_RELASZ	8	/* Total size of ElfNN_Rela relocations. */
#define DT_RELAENT	9	/* Size of each ElfNN_Rela relocation entry. */
#define DT_STRSZ	10	/* Size of string table. */
#define DT_SYMENT	11	/* Size of each symbol table entry. */
#define DT_INIT		12	/* Address of initialization function. */
#define DT_FINI		13	/* Address of finalization function. */
#define DT_SONAME	14	/* String table offset of shared object
				   name. */
#define DT_RPATH	15	/* String table offset of library path. */
#define DT_SYMBOLIC	16	/* Indicates "symbolic" linking. */
#define DT_REL		17	/* Address of ElfNN_Rel relocations. */
#define DT_RELSZ	18	/* Total size of ElfNN_Rel relocations. */
#define DT_RELENT	19	/* Size of each ElfNN_Rel relocation. */
#define DT_PLTREL	20	/* Type of relocation used for PLT. */
#define DT_DEBUG	21	/* Reserved (not used). */
#define DT_TEXTREL	22	/* Indicates there may be relocations in
				   non-writable segments. */
#define DT_JMPREL	23	/* Address of PLT relocations. */

#define DT_COUNT	24	/* Number of defined d_tag values. */
	
#define R_IA64_NONE		0	/* None */
#define R_IA64_DIR64MSB		0x26	/* word64 MSB	S + A */
#define R_IA64_DIR64LSB		0x27	/* word64 LSB	S + A */
#define R_IA64_FPTR64MSB	0x46	/* word64 MSB	@fptr(S + A) */
#define R_IA64_FPTR64LSB	0x47	/* word64 LSB	@fptr(S + A) */
#define R_IA64_REL64MSB		0x6e	/* word64 MSB	BD + A */
#define R_IA64_REL64LSB		0x6f	/* word64 LSB	BD + A */

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
	.global _start_plabel
	.section .plabel, "a"
	.align	16
_start_plabel:	
	.quad	_start
	.quad	__gp
	.previous

	// A PE32+ relocation entry for the plabel

	.section .reloc, "a"
	.long	_start_plabel
	.long	12
	.short	(10 << 12) + 0
	.short	(10 << 12) + 8
	.previous

	// in0:	image base
	// in1:	system table
STATIC_ENTRY(_reloc, 2)
	alloc	loc0=ar.pfs,2,2,2,0
	mov	loc1=rp
	;; 
	movl	r15=@gprel(_DYNAMIC)	// find _DYNAMIC etc.
	movl	r2=@gprel(fptr_storage)
	movl	r3=@gprel(fptr_storage_end)
	;;
	add	r15=r15,gp		// relocate _DYNAMIC etc.
	add	r2=r2,gp
	add	r3=r3,gp
	;;
1:	ld8	r16=[r15],8		// read r15->d_tag
	;;
	ld8	r17=[r15],8		// and r15->d_val
	;;
	cmp.eq	p6,p0=DT_NULL,r16	// done?
(p6)	br.cond.dpnt.few 2f
	;; 
	cmp.eq	p6,p0=DT_RELA,r16
	;; 
(p6)	add	r18=r17,in0		// found rela section
	;; 
	cmp.eq	p6,p0=DT_RELASZ,r16
	;; 
(p6)	mov	r19=r17			// found rela size
	;; 
	cmp.eq	p6,p0=DT_SYMTAB,r16
	;; 
(p6)	add	r20=r17,in0		// found symbol table
	;; 
(p6)	setf.sig f8=r20
	;; 
	cmp.eq	p6,p0=DT_SYMENT,r16
	;; 
(p6)	setf.sig f9=r17			// found symbol entry size
	;; 
	cmp.eq	p6,p0=DT_RELAENT,r16
	;; 
(p6)	mov	r22=r17			// found rela entry size
	;;
	br.sptk.few 1b
	
2:	
	ld8	r15=[r18],8		// read r_offset
	;; 
	ld8	r16=[r18],8		// read r_info
	add	r15=r15,in0		// relocate r_offset
	;;
	ld8	r17=[r18],8		// read r_addend
	sub	r19=r19,r22		// update relasz

	extr.u	r23=r16,0,32		// ELF64_R_TYPE(r16)
	;;
	cmp.eq	p6,p0=R_IA64_NONE,r23
(p6)	br.cond.dpnt.few 3f
	;;
	cmp.eq	p6,p0=R_IA64_DIR64LSB,r23
	;;
(p6)	br.cond.dptk.few 4f
	;;
	cmp.eq	p6,p0=R_IA64_FPTR64LSB,r23
	;;
(p6)	br.cond.dptk.few 5f
	;;
	cmp.eq	p6,p0=R_IA64_REL64LSB,r23
	;;
(p6)	br.cond.dptk.few 4f
	;;

3:	cmp.ltu	p6,p0=0,r19		// more?
(p6)	br.cond.dptk.few 2b		// loop
	
	mov	r8=EFI_SUCCESS		// success return value
	;;
	br.cond.sptk.few 9f		// done

4:
	ld8	r16=[r15]		// read value
	;;
	add	r16=r16,in0		// relocate it
	;;
	st8	[r15]=r16		// and store it back
	br.cond.sptk.few 3b

5:
	extr.u	r23=r16,32,32		// ELF64_R_SYM(r16)
	;; 
	setf.sig f10=r23		// so we can multiply
	;;
	xma.lu	f10=f10,f9,f8		// f10=symtab + r_sym*syment
	;;
	getf.sig r16=f10
	mov	r8=EFI_BUFFER_TOO_SMALL	// failure return value
	;;
	cmp.geu	p6,p0=r2,r3		// space left?
(p6)	br.cond.dpnt.few 9f		// bail out

	st8	[r15]=r2		// install fptr
	add	r16=8,r16		// address of st_value
	;;
	ld8	r16=[r16]		// read symbol value
	;;
	add	r16=r16,in0		// relocate symbol value
	;;
	st8	[r2]=r16,8		// write fptr address
	;;
	st8	[r2]=gp,8		// write fptr gp
	br.cond.sptk.few 3b

9:
	mov	ar.pfs=loc0
	mov	rp=loc1
	;;
	br.ret.sptk.few rp

END(_reloc)

	// in0:	system table
	// in1:	character
ENTRY(_putchar, 2)
	alloc	loc0=ar.pfs,2,3,2,0
	mov	loc1=rp
	mov	loc2=gp
	add	sp=-32,sp
	;; 
	add	r14=64,in0		// r14 = &in1->ConOut
	;;
	ld8	r14=[r14]		// r14 = in1->ConOut
	;;
	add	r15=8,r14		// r15 = &r14->OutputString
	mov	out0=r14
	mov	out1=sp
	mov	r16=sp
	;;
	ld8	r15=[r15]		// r15 = r14->OutputString
	st2	[r16]=in1,2		// write character
	;;
	st2	[r16]=r0		// terminate
	ld8	r17=[r15],8		// function address
	;;
	ld8	gp=[r15]		// function gp
	mov	b6=r17			// transfer to branch register
	;;
	br.call.sptk.few rp=b6		// call function
	;;
	mov	gp=loc2			// restore gp
	mov	ar.pfs=loc0
	mov	rp=loc1
	add	sp=32,sp
	;;
	br.ret.sptk.few rp
	
END(_putchar)

	// in0:	system table
	// in1:	string
ENTRY(_puts, 2)
	alloc	loc0=ar.pfs,3,2,2,0
	mov	loc1=rp
	;;
	mov	out0=in0
	;;
1:	ld1	out1=[in1],1
	;; 
	cmp.eq	p6,p0=r0,out1
(p6)	br.cond.dpnt.few 9f
	;;
	br.call.sptk.few rp=_putchar
	;;
	br.cond.sptk.few 1b
9:
	mov	ar.pfs=loc0
	mov	rp=loc1
	;;
	br.ret.sptk.few rp
END(_puts)
	
	// in0:	system table
	// in1:	number
ENTRY(_puthex, 2)
	alloc	loc0=ar.pfs,2,3,2,0
	mov	loc1=rp
	mov	loc2=ar.lc
	;;
	mov	out0=in0
	mov	ar.lc=15
	;;
1:	extr.u	out1=in1,60,4
	;;
	cmp.leu	p6,p7=10,out1
	;;
(p6)	add	out1='a'-10,out1
(p7)	add	out1='0',out1
	dep.z	in1=in1,4,60
	;; 
	br.call.sptk.few rp=_putchar
	;;
	br.cloop.sptk.few 1b
	;; 
	mov	out1='\r'
	;;
	br.call.sptk.few rp=_putchar
	;;
	mov	out1='\n'
	;;
	br.call.sptk.few rp=_putchar
	;; 
9:
	mov	ar.pfs=loc0
	mov	rp=loc1
	mov	ar.lc=loc2
	;;
	br.ret.sptk.few rp
END(_puthex)

	.data
	.align	16
	
fptr_storage:	
	.space	1024*16			// XXX
fptr_storage_end:	
