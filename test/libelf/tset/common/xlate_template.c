/*-
 * Copyright (c) 2006,2010-2011 Joseph Koshy
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
 * $Id: xlate_template.c 3174 2015-03-27 17:13:41Z emaste $
 */

/*
 * Boilerplate for testing the *_xlate() functions.
 *
 * Usage:
 *
 * #define	TS_XLATESZ	32 (or 64)
 * #include	"xlate_template.c"
 */

#include <sys/param.h>

#define	__XCONCAT(x,y)	__CONCAT(x,y)
#ifndef	__XSTRING
#define __XSTRING(x)	__STRING(x)
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define	TS_XLATETOF	__XCONCAT(elf,__XCONCAT(TS_XLATESZ,_xlatetof))
#define	TS_XLATETOM	__XCONCAT(elf,__XCONCAT(TS_XLATESZ,_xlatetom))

#define	BYTE_VAL	0xFF
#define	BYTE_SEQ_LSB	0xFF,
#define	BYTE_SEQ_MSB	0xFF,

#define	HALF_VAL	0xFEDC
#define	HALF_SEQ_LSB	0xDC,0xFE,
#define	HALF_SEQ_MSB	0xFE,0xDC,

#define	WORD_VAL	0xFEDCBA98UL
#define	WORD_SEQ_LSB	0x98,0xBA,0xDC,0xFE,
#define	WORD_SEQ_MSB	0xFE,0xDC,0xBA,0x98,

#define	QUAD_VAL	0xFEDCBA9876543210ULL
#define	QUAD_SEQ_LSB	0x10,0x32,0x54,0x76,\
	0x98,0xBA,0xDC,0xFE,
#define	QUAD_SEQ_MSB	0xFE,0xDC,0xBA,0x98,\
	0x76,0x54,0x32,0x10,

#define	IDENT_BYTES	46,33,46,64,46,35,46,36,46,37,46,94,46,38,46,42
#define	IDENT_VAL	{ IDENT_BYTES }
#define	IDENT_SEQ_LSB	IDENT_BYTES,
#define	IDENT_SEQ_MSB	IDENT_BYTES,


#define	TYPEDEFNAME(E,N)	__XCONCAT(__XCONCAT(td_,		\
	__XCONCAT(E,TS_XLATESZ)), __XCONCAT(_,N))
#define	TYPEDEFINITION(E,N) __XCONCAT(ELF_TYPE_E,__XCONCAT(TS_XLATESZ,	\
	__XCONCAT(_, N)))
#define	CHKFNNAME(N)	__XCONCAT(__XCONCAT(td_chk_,TS_XLATESZ),	\
	__XCONCAT(_,N))
#define	MEMSIZENAME(N)	__XCONCAT(N,__XCONCAT(TS_XLATESZ,_SIZE))
#define	MEMSTRUCTNAME(N)	__XCONCAT(N,__XCONCAT(TS_XLATESZ,_mem))

/*
 * Definitions of 32 bit ELF file structures.
 */

#define	ELF_TYPE_E32_CAP()			\
	MEMBER(c_tag,		WORD)		\
	MEMBER(c_un.c_val,	WORD)

#define	ELF_TYPE_E32_DYN()			\
	MEMBER(d_tag,		WORD)		\
	MEMBER(d_un.d_val,	WORD)

#define	ELF_TYPE_E32_EHDR()			\
	MEMBER(e_ident,		IDENT)		\
	MEMBER(e_type,		HALF)		\
	MEMBER(e_machine,	HALF)		\
	MEMBER(e_version,	WORD)		\
	MEMBER(e_entry,		WORD)		\
	MEMBER(e_phoff,		WORD)		\
	MEMBER(e_shoff,		WORD)		\
	MEMBER(e_flags,		WORD)		\
	MEMBER(e_ehsize,	HALF)		\
	MEMBER(e_phentsize,	HALF)		\
	MEMBER(e_phnum,		HALF)		\
	MEMBER(e_shentsize,	HALF)		\
	MEMBER(e_shnum,		HALF)		\
	MEMBER(e_shstrndx,	HALF)

#define	ELF_TYPE_E32_MOVE()			\
	MEMBER(m_value,		QUAD)		\
	MEMBER(m_info,		WORD)		\
	MEMBER(m_poffset,	WORD)		\
	MEMBER(m_repeat,	HALF)		\
	MEMBER(m_stride,	HALF)

#define	ELF_TYPE_E32_PHDR()			\
	MEMBER(p_type,		WORD)		\
	MEMBER(p_offset,	WORD)		\
	MEMBER(p_vaddr,		WORD)		\
	MEMBER(p_paddr,		WORD)		\
	MEMBER(p_filesz,	WORD)		\
	MEMBER(p_memsz,		WORD)		\
	MEMBER(p_flags,		WORD)		\
	MEMBER(p_align,		WORD)

#define	ELF_TYPE_E32_REL()			\
	MEMBER(r_offset,	WORD)		\
	MEMBER(r_info,		WORD)

#define	ELF_TYPE_E32_RELA()			\
	MEMBER(r_offset,	WORD)		\
	MEMBER(r_info,		WORD)		\
	MEMBER(r_addend,	WORD)

#define	ELF_TYPE_E32_SHDR()			\
	MEMBER(sh_name,		WORD)		\
	MEMBER(sh_type,		WORD)		\
	MEMBER(sh_flags,	WORD)		\
	MEMBER(sh_addr,		WORD)		\
	MEMBER(sh_offset,	WORD)		\
	MEMBER(sh_size,		WORD)		\
	MEMBER(sh_link,		WORD)		\
	MEMBER(sh_info,		WORD)		\
	MEMBER(sh_addralign,	WORD)		\
	MEMBER(sh_entsize,	WORD)

#define	ELF_TYPE_E32_SYM()			\
	MEMBER(st_name,		WORD)		\
	MEMBER(st_value,	WORD)		\
	MEMBER(st_size,		WORD)		\
	MEMBER(st_info,		BYTE)		\
	MEMBER(st_other,	BYTE)		\
	MEMBER(st_shndx,	HALF)

#define	ELF_TYPE_E32_SYMINFO()			\
	MEMBER(si_boundto,	HALF)		\
	MEMBER(si_flags,	HALF)

/*
 * Definitions of 64 bit ELF file structures.
 */

#define	ELF_TYPE_E64_CAP()			\
	MEMBER(c_tag,		QUAD)		\
	MEMBER(c_un.c_val,	QUAD)

#define	ELF_TYPE_E64_DYN()			\
	MEMBER(d_tag,		QUAD)		\
	MEMBER(d_un.d_val,	QUAD)

#define	ELF_TYPE_E64_EHDR()			\
	MEMBER(e_ident,		IDENT)		\
	MEMBER(e_type,		HALF)		\
	MEMBER(e_machine,	HALF)		\
	MEMBER(e_version,	WORD)		\
	MEMBER(e_entry,		QUAD)		\
	MEMBER(e_phoff,		QUAD)		\
	MEMBER(e_shoff,		QUAD)		\
	MEMBER(e_flags,		WORD)		\
	MEMBER(e_ehsize,	HALF)		\
	MEMBER(e_phentsize,	HALF)		\
	MEMBER(e_phnum,		HALF)		\
	MEMBER(e_shentsize,	HALF)		\
	MEMBER(e_shnum,		HALF)		\
	MEMBER(e_shstrndx,	HALF)

#define	ELF_TYPE_E64_MOVE()			\
	MEMBER(m_value,		QUAD)		\
	MEMBER(m_info,		QUAD)		\
	MEMBER(m_poffset,	QUAD)		\
	MEMBER(m_repeat,	HALF)		\
	MEMBER(m_stride,	HALF)

#define	ELF_TYPE_E64_PHDR()			\
	MEMBER(p_type,		WORD)		\
	MEMBER(p_flags,		WORD)		\
	MEMBER(p_offset,	QUAD)		\
	MEMBER(p_vaddr,		QUAD)		\
	MEMBER(p_paddr,		QUAD)		\
	MEMBER(p_filesz,	QUAD)		\
	MEMBER(p_memsz,		QUAD)		\
	MEMBER(p_align,		QUAD)

#define	ELF_TYPE_E64_REL()			\
	MEMBER(r_offset,	QUAD)		\
	MEMBER(r_info,		QUAD)

#define	ELF_TYPE_E64_RELA()			\
	MEMBER(r_offset,	QUAD)		\
	MEMBER(r_info,		QUAD)		\
	MEMBER(r_addend,	QUAD)

#define	ELF_TYPE_E64_SHDR()			\
	MEMBER(sh_name,		WORD)		\
	MEMBER(sh_type,		WORD)		\
	MEMBER(sh_flags,	QUAD)		\
	MEMBER(sh_addr,		QUAD)		\
	MEMBER(sh_offset,	QUAD)		\
	MEMBER(sh_size,		QUAD)		\
	MEMBER(sh_link,		WORD)		\
	MEMBER(sh_info,		WORD)		\
	MEMBER(sh_addralign,	QUAD)		\
	MEMBER(sh_entsize,	QUAD)

#define	ELF_TYPE_E64_SYM()			\
	MEMBER(st_name,		WORD)		\
	MEMBER(st_info,		BYTE)		\
	MEMBER(st_other,	BYTE)		\
	MEMBER(st_shndx,	HALF)		\
	MEMBER(st_value,	QUAD)		\
	MEMBER(st_size,		QUAD)		\

#define	ELF_TYPE_E64_SYMINFO()			\
	MEMBER(si_boundto,	HALF)		\
	MEMBER(si_flags,	HALF)

static unsigned char TYPEDEFNAME(L,CAP)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,CAP)()
};
static unsigned char TYPEDEFNAME(M,CAP)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,CAP)()
};

static unsigned char TYPEDEFNAME(L,DYN)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,DYN)()
};
static unsigned char TYPEDEFNAME(M,DYN)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,DYN)()
};

static unsigned char TYPEDEFNAME(L,EHDR)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,EHDR)()
};
static unsigned char TYPEDEFNAME(M,EHDR)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,EHDR)()
};

static unsigned char TYPEDEFNAME(L,HALF)[] = {
	HALF_SEQ_LSB
};
static unsigned char TYPEDEFNAME(M,HALF)[] = {
	HALF_SEQ_MSB
};

static unsigned char TYPEDEFNAME(L,MOVE)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,MOVE)()
};
static unsigned char TYPEDEFNAME(M,MOVE)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,MOVE)()
};

static unsigned char TYPEDEFNAME(L,PHDR)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,PHDR)()
};
static unsigned char TYPEDEFNAME(M,PHDR)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,PHDR)()
};

static unsigned char TYPEDEFNAME(L,REL)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,REL)()
};
static unsigned char TYPEDEFNAME(M,REL)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,REL)()
};

static unsigned char TYPEDEFNAME(L,RELA)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,RELA)()
};
static unsigned char TYPEDEFNAME(M,RELA)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,RELA)()
};

static unsigned char TYPEDEFNAME(L,SHDR)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,SHDR)()
};
static unsigned char TYPEDEFNAME(M,SHDR)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,SHDR)()
};

static unsigned char TYPEDEFNAME(L,SYM)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,SYM)()
};
static unsigned char TYPEDEFNAME(M,SYM)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,SYM)()
};

static unsigned char TYPEDEFNAME(L,SYMINFO)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_LSB
	TYPEDEFINITION(L,SYMINFO)()
};
static unsigned char TYPEDEFNAME(M,SYMINFO)[] = {
#undef	MEMBER
#define	MEMBER(N,K)	K##_SEQ_MSB
	TYPEDEFINITION(M,SYMINFO)()
};

static unsigned char TYPEDEFNAME(L,QUAD)[] = {
	QUAD_SEQ_LSB
};

static unsigned char TYPEDEFNAME(M,QUAD)[] = {
	QUAD_SEQ_MSB
};

static unsigned char TYPEDEFNAME(L,WORD)[] = {
	WORD_SEQ_LSB
};
static unsigned char TYPEDEFNAME(M,WORD)[] = {
	WORD_SEQ_MSB
};

#if	TS_XLATESZ == 32
/*
 * 32 bit reference structures.
 */

#define	td_L32_ADDR	td_L32_WORD
#define	td_M32_ADDR	td_M32_WORD
#define	td_L32_SWORD	td_L32_WORD
#define	td_M32_SWORD	td_M32_WORD
#define	td_L32_OFF	td_L32_WORD
#define	td_M32_OFF	td_M32_WORD

static Elf32_Addr MEMSTRUCTNAME(ADDR) = WORD_VAL;
#define	ADDR32_SIZE	sizeof(Elf32_Addr)

static Elf32_Cap MEMSTRUCTNAME(CAP) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_CAP()
};
#define	CAP32_SIZE	sizeof(Elf32_Cap)

static Elf32_Dyn MEMSTRUCTNAME(DYN) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_DYN()
};
#define	DYN32_SIZE	sizeof(Elf32_Dyn)

static Elf32_Ehdr MEMSTRUCTNAME(EHDR) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_EHDR()
};
#define	EHDR32_SIZE	sizeof(Elf32_Ehdr)

static Elf32_Half MEMSTRUCTNAME(HALF) = HALF_VAL;
#define	HALF32_SIZE	sizeof(Elf32_Half)

static Elf32_Move MEMSTRUCTNAME(MOVE) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_MOVE()
};
#define	MOVE32_SIZE	sizeof(Elf32_Move)

static Elf32_Off MEMSTRUCTNAME(OFF) = WORD_VAL;
#define	OFF32_SIZE	sizeof(Elf32_Off)

static Elf32_Phdr MEMSTRUCTNAME(PHDR) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_PHDR()
};
#define	PHDR32_SIZE	sizeof(Elf32_Phdr)

static Elf32_Rel MEMSTRUCTNAME(REL) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_REL()
};
#define	REL32_SIZE	sizeof(Elf32_Rel)

static Elf32_Rela MEMSTRUCTNAME(RELA) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_RELA()
};
#define	RELA32_SIZE	sizeof(Elf32_Rela)

static Elf32_Shdr MEMSTRUCTNAME(SHDR) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_SHDR()
};
#define	SHDR32_SIZE	sizeof(Elf32_Shdr)

static Elf32_Sword MEMSTRUCTNAME(SWORD) = WORD_VAL;
#define	SWORD32_SIZE	sizeof(Elf32_Sword)

static Elf32_Sym MEMSTRUCTNAME(SYM) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_SYM()
};
#define	SYM32_SIZE	sizeof(Elf32_Sym)

static Elf32_Syminfo MEMSTRUCTNAME(SYMINFO) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E32_SYMINFO()
};
#define	SYMINFO32_SIZE	sizeof(Elf32_Syminfo)

static Elf32_Word MEMSTRUCTNAME(WORD) = WORD_VAL;
#define	WORD32_SIZE	sizeof(Elf32_Word)

#else
/*
 * 64 bit reference structures.
 */

#define	td_L64_ADDR	td_L64_QUAD
#define	td_M64_ADDR	td_M64_QUAD
#define	td_L64_OFF	td_L64_QUAD
#define	td_M64_OFF	td_M64_QUAD
#define	td_L64_SWORD	td_L64_WORD
#define	td_M64_SWORD	td_M64_WORD
#define	td_L64_SXWORD	td_L64_QUAD
#define	td_M64_SXWORD	td_M64_QUAD
#define	td_L64_XWORD	td_L64_QUAD
#define	td_M64_XWORD	td_M64_QUAD

static Elf64_Addr MEMSTRUCTNAME(ADDR) = QUAD_VAL;
#define	ADDR64_SIZE	sizeof(Elf64_Addr)

static Elf64_Cap MEMSTRUCTNAME(CAP) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_CAP()
};
#define	CAP64_SIZE	sizeof(Elf64_Cap)

static Elf64_Dyn MEMSTRUCTNAME(DYN) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_DYN()
};
#define	DYN64_SIZE	sizeof(Elf64_Dyn)

static Elf64_Ehdr MEMSTRUCTNAME(EHDR) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_EHDR()
};
#define	EHDR64_SIZE	sizeof(Elf64_Ehdr)

static Elf64_Half MEMSTRUCTNAME(HALF) = HALF_VAL;
#define	HALF64_SIZE	sizeof(Elf64_Half)

static Elf64_Move MEMSTRUCTNAME(MOVE) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_MOVE()
};
#define	MOVE64_SIZE	sizeof(Elf64_Move)

static Elf64_Phdr MEMSTRUCTNAME(PHDR) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_PHDR()
};
#define	PHDR64_SIZE	sizeof(Elf64_Phdr)

static Elf64_Off MEMSTRUCTNAME(OFF) = QUAD_VAL;
#define	OFF64_SIZE	sizeof(Elf64_Off)

static Elf64_Rel MEMSTRUCTNAME(REL) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_REL()
};
#define	REL64_SIZE	sizeof(Elf64_Rel)

static Elf64_Rela MEMSTRUCTNAME(RELA) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_RELA()
};
#define	RELA64_SIZE	sizeof(Elf64_Rela)

static Elf64_Shdr MEMSTRUCTNAME(SHDR) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_SHDR()
};
#define	SHDR64_SIZE	sizeof(Elf64_Shdr)

static Elf64_Sword MEMSTRUCTNAME(SWORD) = WORD_VAL;
#define	SWORD64_SIZE	sizeof(Elf64_Sword)

static Elf64_Sxword MEMSTRUCTNAME(SXWORD) = QUAD_VAL;
#define	SXWORD64_SIZE	sizeof(Elf64_Sxword)

static Elf64_Sym MEMSTRUCTNAME(SYM) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_SYM()
};
#define	SYM64_SIZE	sizeof(Elf64_Sym)

static Elf64_Syminfo MEMSTRUCTNAME(SYMINFO) = {
#undef	MEMBER
#define	MEMBER(N,K)	.N = K##_VAL ,
	ELF_TYPE_E64_SYMINFO()
};
#define	SYMINFO64_SIZE	sizeof(Elf64_Syminfo)

static Elf64_Word MEMSTRUCTNAME(WORD) = WORD_VAL;
#define	WORD64_SIZE	sizeof(Elf64_Word)

static Elf64_Xword MEMSTRUCTNAME(XWORD) = QUAD_VAL;
#define	XWORD64_SIZE	sizeof(Elf64_Xword)

#endif	/* TS_XLATESZ == 32 */


#ifndef	_TESTDATA_STRUCT_
#define	_TESTDATA_STRUCT_ 1
struct testdata {
	char			*tsd_name;
	Elf_Type		tsd_type;
	void			*tsd_mem;
	size_t			tsd_fsz;
	size_t			tsd_msz;
	const unsigned char	*tsd_lsb;
	const unsigned char	*tsd_msb;
};
#endif	/*_TESTDATA_STRUCT_*/

#define	TESTDATASET	__XCONCAT(tests,TS_XLATESZ)
static struct testdata 	TESTDATASET [] = {
#undef	DEFINE_TEST_DATA
#define	DEFINE_TEST_DATA(N)  {				\
		.tsd_name = #N,				\
		.tsd_type = ELF_T_##N,			\
		.tsd_fsz = sizeof(TYPEDEFNAME(L,N)),	\
		.tsd_msz = MEMSIZENAME(N),		\
		.tsd_mem = (void *) &MEMSTRUCTNAME(N),	\
		.tsd_lsb = TYPEDEFNAME(L,N),		\
		.tsd_msb = TYPEDEFNAME(M,N),		\
	}
#if	TS_XLATESZ == 32
	DEFINE_TEST_DATA(ADDR),
	DEFINE_TEST_DATA(CAP),
	DEFINE_TEST_DATA(DYN),
	DEFINE_TEST_DATA(EHDR),
	DEFINE_TEST_DATA(HALF),
	DEFINE_TEST_DATA(MOVE),
	DEFINE_TEST_DATA(OFF),
	DEFINE_TEST_DATA(PHDR),
	DEFINE_TEST_DATA(REL),
	DEFINE_TEST_DATA(RELA),
	DEFINE_TEST_DATA(SHDR),
	DEFINE_TEST_DATA(SWORD),
	DEFINE_TEST_DATA(SYM),
	DEFINE_TEST_DATA(SYMINFO),
	DEFINE_TEST_DATA(WORD),
#else
	DEFINE_TEST_DATA(ADDR),
	DEFINE_TEST_DATA(CAP),
	DEFINE_TEST_DATA(DYN),
	DEFINE_TEST_DATA(EHDR),
	DEFINE_TEST_DATA(HALF),
	DEFINE_TEST_DATA(MOVE),
	DEFINE_TEST_DATA(OFF),
	DEFINE_TEST_DATA(PHDR),
	DEFINE_TEST_DATA(REL),
	DEFINE_TEST_DATA(RELA),
	DEFINE_TEST_DATA(SHDR),
	DEFINE_TEST_DATA(SWORD),
	DEFINE_TEST_DATA(SXWORD),
	DEFINE_TEST_DATA(SYM),
	DEFINE_TEST_DATA(SYMINFO),
	DEFINE_TEST_DATA(WORD),
	DEFINE_TEST_DATA(XWORD),
#endif	/* TS_XLATESZ == 32 */
	{ .tsd_name = NULL }
};


#define NCOPIES		3
#define	NOFFSET		8	/* check every alignment in a quad word */

#ifndef	NO_TESTCASE_FUNCTIONS

static int
check_xlate(Elf_Data *xlator(Elf_Data *d, const Elf_Data *s, unsigned int enc),
    int ed, Elf_Data *dst, Elf_Data *src, struct testdata *td, int ncopies)
{
	Elf_Data *dstret;
	size_t msz;

	msz = td->tsd_msz;

	/* Invoke translator */
	if ((dstret = xlator(dst, src, ed)) != dst) {
		tet_printf("fail: \"%s\" " __XSTRING(TC_XLATETOM)
		    ": %s", td->tsd_name, elf_errmsg(-1));
		tet_result(TET_FAIL);
		return (0);
	}

	/* Check return parameters. */
	if (dst->d_type != td->tsd_type || dst->d_size != msz*ncopies) {
		tet_printf("fail: \"%s\" type(ret=%d,expected=%d) "
		    "size (ret=%d,expected=%d).", td->tsd_name,
		    dst->d_type,  td->tsd_type, dst->d_size, msz*ncopies);
		tet_result(TET_FAIL);
		return (0);
	}

	return (1);
}

/*
 * Check byte conversions:
 */

void
__XCONCAT(tcXlate_tpByte,TS_XLATESZ)(void)
{
	Elf_Data dst, src;
	int i, offset, sz;
	char *filebuf, *membuf, *t, *ref;

	ref = TYPEDEFNAME(L,QUAD);
	sz = sizeof(TYPEDEFNAME(L,QUAD));

	if ((membuf = malloc(sz*NCOPIES)) == NULL ||
	    (filebuf = malloc(sz*NCOPIES+NOFFSET)) == NULL) {
		if (membuf)
			free(membuf);
		tet_infoline("unresolved: malloc() failed.");
		tet_result(TET_UNRESOLVED);
		return;
	}

	/*
	 * Check memory to file conversions.
	 */
	t = membuf;
	for (i = 0; i < NCOPIES; i++)
		t = memcpy(t, ref, sz) + sz;

	src.d_buf     = membuf;
	src.d_size    = sz*NCOPIES;
	src.d_type    = ELF_T_BYTE;
	src.d_version = EV_CURRENT;

	tet_infoline("assertion: Byte TOF() succeeds.");

	for (offset = 0; offset < NOFFSET; offset++) {
		/*
		 * LSB
		 */
		dst.d_buf     = filebuf + offset;
		dst.d_size    = sz*NCOPIES;
		dst.d_version = EV_CURRENT;

		if (TS_XLATETOF(&dst,&src,ELFDATA2LSB) != &dst ||
		    dst.d_size != sz*NCOPIES) {
			tet_infoline("fail: LSB TOF() conversion.");
			tet_result(TET_FAIL);
			goto done;
		}

		if (memcmp(membuf, filebuf+offset, sz*NCOPIES)) {
			tet_infoline("fail: LSB TOF() memcmp().");
			tet_result(TET_FAIL);
			goto done;
		}

		/*
		 * MSB
		 */
		dst.d_buf     = filebuf + offset;
		dst.d_size    = sz*NCOPIES;
		dst.d_version = EV_CURRENT;

		if (TS_XLATETOF(&dst,&src,ELFDATA2MSB) != &dst ||
		    dst.d_size != sz*NCOPIES) {
			tet_infoline("fail: MSB TOF() conversion.");
			tet_result(TET_FAIL);
			goto done;
		}

		if (memcmp(membuf, filebuf+offset, sz*NCOPIES)) {
			tet_infoline("fail: MSB TOF() memcmp().");
			tet_result(TET_FAIL);
			goto done;
		}
	}

	/*
	 * Check file to memory conversions.
	 */

	tet_infoline("assertion: Byte TOM() succeeds.");

	ref = TYPEDEFNAME(M,QUAD);
	sz = sizeof(TYPEDEFNAME(M,QUAD));

	for (offset = 0; offset < NOFFSET; offset++) {

		src.d_buf = t = filebuf + offset;
		for (i = 0; i < NCOPIES; i++)
			t = memcpy(t,ref,sz);

		src.d_size    = sz*NCOPIES;
		src.d_type    = ELF_T_BYTE;
		src.d_version = EV_CURRENT;

		/*
		 * LSB
		 */
		dst.d_buf     = membuf;
		dst.d_size    = sz*NCOPIES;
		dst.d_version = EV_CURRENT;

		if (TS_XLATETOM(&dst,&src,ELFDATA2LSB) != &dst ||
		    dst.d_size != sz * NCOPIES) {
			tet_infoline("fail: LSB TOM() conversion.");
			tet_result(TET_FAIL);
			goto done;
		}

		if (memcmp(membuf, filebuf+offset, sz*NCOPIES)) {
			tet_infoline("fail: LSB TOM() memcmp().");
			tet_result(TET_FAIL);
			goto done;
		}

		/*
		 * MSB
		 */
		dst.d_buf     = membuf;
		dst.d_size    = sz*NCOPIES;
		dst.d_version = EV_CURRENT;

		if (TS_XLATETOM(&dst,&src,ELFDATA2MSB) != &dst ||
		    dst.d_size != sz * NCOPIES) {
			tet_infoline("fail: MSB TOM() conversion.");
			tet_result(TET_FAIL);
			goto done;
		}

		if (memcmp(membuf, filebuf+offset, sz*NCOPIES)) {
			tet_infoline("fail: MSB TOM() memcmp().");
			tet_result(TET_FAIL);
			goto done;
		}
	}

	tet_result(TET_PASS);

 done:
	if (membuf)
		free(membuf);
	if (filebuf)
		free(filebuf);
}

/*
 * Check a byte conversion on a shared buffer.
 */

void
__XCONCAT(tcXlate_tpByteShared,TS_XLATESZ)(void)
{
	int i;
	size_t sz;
	Elf_Data dst, src;
	char *membuf, *t, *ref;

#define	PREPARE_SHARED(T,SZ)	do {					\
		src.d_buf     = dst.d_buf     = membuf;			\
		src.d_size    = dst.d_size    = (SZ) * NCOPIES;		\
		src.d_type    = dst.d_type    = (T);			\
		src.d_version = dst.d_version = EV_CURRENT;		\
	} while (0)

#define	VERIFY(R,SZ)	do {						\
		t = dst.d_buf;						\
		for (i = 0; i < NCOPIES; i++, t += (SZ))		\
			if (memcmp((R), t, (SZ))) {			\
				tet_infoline("fail: LSB TOF() "		\
				    "memcmp().");			\
				tet_result(TET_FAIL);			\
				goto done;				\
			}						\
	} while (0)

	membuf = NULL;
	ref    = TYPEDEFNAME(L,QUAD);
	sz     = sizeof(TYPEDEFNAME(L,QUAD));

	if ((membuf = malloc(sz * NCOPIES)) == NULL) {
		tet_infoline("unresolved: malloc() failed.");
		tet_result(TET_UNRESOLVED);
		return;
	}

	t = membuf;
	for (i = 0; i < NCOPIES; i++)
		t = memcpy(t, ref, sz) + sz;

	tet_infoline("assertion: byte TOF() on a shared dst/src arena "
	    "succeeds.");

	PREPARE_SHARED(ELF_T_BYTE, sz);
	if (TS_XLATETOF(&dst, &src, ELFDATA2LSB) != &dst ||
	    dst.d_size != sz * NCOPIES ||
	    dst.d_buf != src.d_buf) {
		tet_printf("fail: LSB TOF() conversion: %s.",
		    elf_errmsg(-1));
		tet_result(TET_FAIL);
		goto done;
	}
	VERIFY(ref,sz);

	PREPARE_SHARED(ELF_T_BYTE, sz);
	if (TS_XLATETOF(&dst, &src, ELFDATA2MSB) != &dst ||
	    dst.d_size != sz * NCOPIES ||
	    dst.d_buf != src.d_buf) {
		tet_printf("fail: MSB TOF() conversion: %s.",
		    elf_errmsg(-1));
		tet_result(TET_FAIL);
		goto done;
	}
	VERIFY(ref,sz);

	tet_infoline("assertion: byte TOM() on a shared dst/src arena "
	    "succeeds.");

	PREPARE_SHARED(ELF_T_BYTE, sz);
	if (TS_XLATETOM(&dst, &src, ELFDATA2LSB) != &dst ||
	    dst.d_size != sz * NCOPIES ||
	    dst.d_buf != src.d_buf) {
		tet_printf("fail: LSB TOM() conversion: %s.",
		    elf_errmsg(-1));
		tet_result(TET_FAIL);
		goto done;
	}
	VERIFY(ref,sz);

	PREPARE_SHARED(ELF_T_BYTE, sz);
	if (TS_XLATETOM(&dst, &src, ELFDATA2MSB) != &dst ||
	    dst.d_size != sz * NCOPIES ||
	    dst.d_buf != src.d_buf) {
		tet_printf("fail: MSB TOM() conversion: %s.",
		    elf_errmsg(-1));
		tet_result(TET_FAIL);
		goto done;
	}
	VERIFY(ref,sz);

	tet_result(TET_PASS);

 done:
	free(membuf);
}

/*
 * Check non-byte conversions from file representations to memory.
 */
void
__XCONCAT(tcXlate_tpToM,TS_XLATESZ)(void)
{
	Elf_Data dst, src;
	struct testdata *td;
	size_t fsz, msz;
	int i, offset;
	char *srcbuf, *membuf, *t;

	srcbuf = NULL;	/* file data (bytes) */
	membuf = NULL;	/* memory data (struct) */

	/* Loop over all types */
	for (td = TESTDATASET; td->tsd_name; td++) {

		fsz = __XCONCAT(__XCONCAT(elf,TS_XLATESZ),_fsize)(td->tsd_type,
		    1, EV_CURRENT);

		msz = td->tsd_msz;

		if (msz == 0 ||
		    fsz != td->tsd_fsz) {
			tet_printf("? %s: msz=%d fsz=%d td->fsz=%d.",
			    td->tsd_name, msz, fsz, td->tsd_fsz);
		}

		assert(fsz == td->tsd_fsz);

		/*
		 * allocate space for NCOPIES of data + offset for file data and
		 * NCOPIES of memory data.
		 */
		if ((srcbuf = malloc(NCOPIES*fsz+NOFFSET)) == NULL ||
		    ((membuf = malloc(NCOPIES*msz))) == NULL) {
			if (srcbuf)
				free(srcbuf);
			tet_infoline("unresolved: malloc() failed.");
			tet_result(TET_UNRESOLVED);
			return;
		}


		tet_printf("assertion: "__XSTRING(TS_XLATETOM)"(%s) succeeds.",
		    td->tsd_name);

		for (offset = 0; offset < NOFFSET; offset++) {

			src.d_buf     = t = srcbuf + offset;
			src.d_size    = fsz * NCOPIES;
			src.d_type    = td->tsd_type;
			src.d_version = EV_CURRENT;

			dst.d_buf     = membuf;
			dst.d_size    = msz * NCOPIES;
			dst.d_version = EV_CURRENT;


			/*
			 * Check conversion of LSB encoded data.
			 */

			/* copy `NCOPIES*fsz' bytes in `srcbuf+offset' */
			for (i = 0; i < NCOPIES; i++) {
				(void) memcpy(t, td->tsd_lsb, fsz);
				t += fsz;
			}
			(void) memset(membuf, 0, NCOPIES*msz);

			if (check_xlate(TS_XLATETOM, ELFDATA2LSB, &dst, &src,
				td, NCOPIES) == 0)
				goto done;

			/* compare the retrieved data with the canonical value */
			t = dst.d_buf;
			for (i = 0; i < NCOPIES; i++) {
				if (memcmp(t, td->tsd_mem, msz)) {
					tet_printf("fail: \"%s\" LSB memory "
					    "compare failed.", td->tsd_name);
					tet_result(TET_FAIL);
					goto done;
				}
				t += msz;
			}

			/*
			 * Check conversion of MSB encoded data.
			 */

			t = srcbuf + offset;
			for (i = 0; i < NCOPIES; i++) {
				(void) memcpy(t, td->tsd_msb, fsz);
				t += fsz;
			}
			(void) memset(membuf, 0, NCOPIES*msz);
			if (check_xlate(TS_XLATETOM, ELFDATA2MSB, &dst, &src,
				td, NCOPIES) == 0)
				goto done;

			/* compare the retrieved data with the canonical value */
			t = dst.d_buf;
			for (i = 0; i < NCOPIES; i++) {
				if (memcmp(t, td->tsd_mem, msz)) {
					tet_printf("fail: \"%s\" MSB memory "
					    "compare failed.", td->tsd_name);
					tet_result(TET_FAIL);
					goto done;
				}
				t += msz;
			}
		}

		free(srcbuf); srcbuf = NULL;
		free(membuf); membuf = NULL;
	}

	tet_result(TET_PASS);

 done:
	if (srcbuf)
		free(srcbuf);
	if (membuf)
		free(membuf);
}

void
__XCONCAT(tcXlate_tpToMShared,TS_XLATESZ)(void)
{
	Elf_Data dst, src;
	struct testdata *td;
	size_t fsz, msz;
	int i, result;
	char *membuf, *t;

	membuf = NULL;

	for (td = TESTDATASET; td->tsd_name; td++) {

		tet_printf("assertion: in-place "__XSTRING(TS_XLATETOM)"(\"%s\").",
		    td->tsd_name);

		fsz = __XCONCAT(__XCONCAT(elf,TS_XLATESZ),_fsize)(td->tsd_type,
		    1, EV_CURRENT);
		msz = td->tsd_msz;

		assert(msz >= fsz);

		if ((membuf = malloc(fsz * NCOPIES)) == NULL) {
			tet_printf("unresolved: \"%s\" malloc() failed.",
			    td->tsd_name);
			tet_result(TET_UNRESOLVED);
			goto done;
		}

		/*
		 * In-place conversion of LSB data.
		 */

		t = membuf;
		for (i = 0; i < NCOPIES; i++)
			t = memcpy(t, td->tsd_lsb, fsz) + fsz;

		PREPARE_SHARED(td->tsd_type, fsz);
		result = TS_XLATETOM(&dst, &src, ELFDATA2LSB) == &dst;

		if (fsz < msz) {
			/* conversion should fail with ELF_E_DATA */
			if (result || elf_errno() != ELF_E_DATA) {
				tet_printf("fail: \"%s\" LSB TOM() succeeded "
				    "with fsz < msz", td->tsd_name);
				tet_result(TET_FAIL);
				goto done;
			}
			free(membuf); membuf = NULL;
			continue;
		}

		/* conversion should have succeeded. */
		if (!result) {
			tet_printf("fail: \"%s\" LSB TOM() failed.",
			    td->tsd_name);
			tet_result(TET_FAIL);
			goto done;
		}

		VERIFY(td->tsd_mem,msz);

		/*
		 * In-place conversion of MSB data.
		 */

		t = membuf;
		for (i = 0; i < NCOPIES; i++)
			t = memcpy(t, td->tsd_msb, fsz) + fsz;

		PREPARE_SHARED(td->tsd_type, fsz);
		result = TS_XLATETOM(&dst, &src, ELFDATA2MSB) == &dst;

		if (fsz < msz) {
			/* conversion should fail with ELF_E_DATA */
			if (result || elf_errno() != ELF_E_DATA) {
				tet_printf("fail: \"%s\" MSB TOM() succeeded "
				    "with fsz < msz", td->tsd_name);
				tet_result(TET_FAIL);
				goto done;
			}
			free(membuf); membuf = NULL;
			continue;
		}

		/* conversion should have succeeded. */
		if (!result) {
			tet_printf("fail: \"%s\" MSB TOM() failed.",
			    td->tsd_name);
			tet_result(TET_FAIL);
			goto done;
		}

		VERIFY(td->tsd_mem,msz);

	}

	tet_result(TET_PASS);

 done:
	if (membuf)
		free(membuf);
}


/*
 * Check non-byte conversions from memory to file.
 */
void
__XCONCAT(tcXlate_tpToF,TS_XLATESZ)(void)
{
	Elf_Data dst, src;
	struct testdata *td;
	size_t fsz, msz;
	int i, offset;
	char *filebuf, *membuf, *t;

	filebuf = NULL;	/* file data (bytes) */
	membuf = NULL;	/* memory data (struct) */

	/* Loop over all types */
	for (td = TESTDATASET; td->tsd_name; td++) {

		fsz = __XCONCAT(__XCONCAT(elf,TS_XLATESZ),_fsize)(td->tsd_type,
		    1, EV_CURRENT);

		msz = td->tsd_msz;

		if (msz == 0 ||
		    fsz != td->tsd_fsz) {
			tet_printf("? %s: msz=%d fsz=%d td->fsz=%d.",
			    td->tsd_name, msz, fsz, td->tsd_fsz);
		}

		assert(msz > 0);
		assert(fsz == td->tsd_fsz);

		/*
		 * allocate space for NCOPIES of data + offset for file data and
		 * NCOPIES of memory data.
		 */
		if ((filebuf = malloc(NCOPIES*fsz+NOFFSET)) == NULL ||
		    ((membuf = malloc(NCOPIES*msz))) == NULL) {
			if (filebuf)
				free(filebuf);
			tet_infoline("unresolved: malloc() failed.");
			tet_result(TET_UNRESOLVED);
			return;
		}


		tet_printf("assertion: "__XSTRING(TS_XLATETOF)"(%s) succeeds.",
		    td->tsd_name);

		for (offset = 0; offset < NOFFSET; offset++) {

			src.d_buf     = membuf;
			src.d_size    = msz * NCOPIES;
			src.d_type    = td->tsd_type;
			src.d_version = EV_CURRENT;

			/*
			 * Check LSB conversion.
			 */

			/* copy `NCOPIES' of canonical memory data to the src buffer */
			t = membuf;
			for (i = 0; i < NCOPIES; i++) {
				(void) memcpy(t, td->tsd_mem, msz);
				t += msz;
			}
			(void) memset(filebuf, 0, NCOPIES*fsz+NOFFSET);

			dst.d_buf     = filebuf + offset;
			dst.d_size    = fsz * NCOPIES;
			dst.d_version = EV_CURRENT;

			if (check_xlate(TS_XLATETOF, ELFDATA2LSB, &dst, &src,
				td, NCOPIES) == 0)
				goto done;

			/* compare converted data to canonical form */
			t = filebuf + offset;
			for (i = 0; i < NCOPIES; i++) {
				if (memcmp(t, td->tsd_lsb, fsz)) {
					tet_printf("fail: \"%s\" LSB memory "
					    "compare.", td->tsd_name);
					tet_result(TET_FAIL);
					goto done;
				}
				t += fsz;
			}

			/*
			 * Check MSB conversion.
			 */
			t = membuf;
			for (i = 0; i < NCOPIES; i++) {
				(void) memcpy(t, td->tsd_mem, msz);
				t += msz;
			}
			(void) memset(filebuf, 0, NCOPIES*fsz+NOFFSET);

			dst.d_buf     = filebuf + offset;
			dst.d_size    = fsz * NCOPIES;
			dst.d_version = EV_CURRENT;

			if (check_xlate(TS_XLATETOF, ELFDATA2MSB, &dst, &src,
				td, NCOPIES) == 0)
				goto done;

			/* compare converted data to canonical form */
			t = filebuf + offset;
			for (i = 0; i < NCOPIES; i++) {
				if (memcmp(t, td->tsd_msb, fsz)) {
					tet_printf("fail: \"%s\" MSB memory "
					    "compare.", td->tsd_name);
					tet_result(TET_FAIL);
					goto done;
				}
				t += fsz;
			}
		}

		free(filebuf); filebuf = NULL;
		free(membuf); membuf = NULL;
	}

	tet_result(TET_PASS);

 done:
	if (filebuf)
		free(filebuf);
	if (membuf)
		free(membuf);
}

void
__XCONCAT(tcXlate_tpToFShared,TS_XLATESZ)(void)
{
	Elf_Data dst, src;
	struct testdata *td;
	size_t fsz, msz;
	int i;
	char *membuf, *t;

	membuf = NULL;

	for (td = TESTDATASET; td->tsd_name; td++) {

		tet_printf("assertion: in-place "__XSTRING(TS_XLATETOF)"(\"%s\").",
		    td->tsd_name);

		fsz = __XCONCAT(__XCONCAT(elf,TS_XLATESZ),_fsize)(td->tsd_type,
		    1, EV_CURRENT);
		msz = td->tsd_msz;

		assert(msz >= fsz);

		if ((membuf = malloc(msz * NCOPIES)) == NULL) {
			tet_printf("unresolved: \"%s\" malloc() failed.",
			    td->tsd_name);
			tet_result(TET_UNRESOLVED);
			goto done;
		}

		/*
		 * In-place conversion to LSB data.
		 */

		t = membuf;
		for (i = 0; i < NCOPIES; i++)
			t = memcpy(t, td->tsd_mem, msz) + msz;

		PREPARE_SHARED(td->tsd_type, msz);
		if (TS_XLATETOF(&dst, &src, ELFDATA2LSB) != &dst) {
			tet_printf("fail: \"%s\" LSB TOF() failed: %s.",
			    td->tsd_name, elf_errmsg(-1));
			tet_result(TET_FAIL);
			goto done;
		}
		VERIFY(td->tsd_lsb,fsz);

		/*
		 * In-place conversion to MSB data.
		 */

		t = membuf;
		for (i = 0; i < NCOPIES; i++)
			t = memcpy(t, td->tsd_mem, msz) + msz;

		PREPARE_SHARED(td->tsd_type, msz);
		if (TS_XLATETOF(&dst, &src, ELFDATA2MSB) != &dst) {
			tet_printf("fail: \"%s\" MSB TOF() failed: %s.",
			    td->tsd_name, elf_errmsg(-1));
			tet_result(TET_FAIL);
			goto done;
		}
		VERIFY(td->tsd_msb,fsz);

	}

	tet_result(TET_PASS);

 done:
	if (membuf)
		free(membuf);
}



/*
 * Various checks for invalid arguments.
 */

void
__XCONCAT(tcArgs_tpNullArgs,TS_XLATESZ)(void)
{
	Elf_Data ed;
	int result;

	tet_infoline("assertion: "__XSTRING(TS_XLATETOF) "/"
	    __XSTRING(TS_XLATETOM) " with NULL arguments fail "
	    "with ELF_E_ARGUMENT.");

	result = TET_PASS;

	if (TS_XLATETOF(NULL,&ed,ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;

	if (TS_XLATETOF(&ed,NULL,ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;

	if (TS_XLATETOM(NULL,&ed,ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;

	if (TS_XLATETOM(&ed,NULL,ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;

	tet_result(result);
}

void
__XCONCAT(tcArgs_tpBadType,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int result;
	char buf[1024];

	tet_infoline("assertion: "__XSTRING(TS_XLATETOF) "/"
	    __XSTRING(TS_XLATETOM) " with an out of range type "
	    "fails with ELF_E_DATA.");

	result = TET_PASS;

	es.d_version = ed.d_version = EV_CURRENT;
	es.d_buf     = ed.d_buf = buf;
	es.d_size    = ed.d_size = sizeof(buf);

	es.d_type = (Elf_Type) -1;

	if (TS_XLATETOF(&ed,&es,ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA)
		result = TET_FAIL;

	if (TS_XLATETOM(&ed,&es,ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA)
		result = TET_FAIL;

	es.d_type = ELF_T_NUM;

	if (TS_XLATETOF(&ed,&es,ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA)
		result = TET_FAIL;

	if (TS_XLATETOM(&ed,&es,ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA)
		result = TET_FAIL;

	tet_result(result);
}

void
__XCONCAT(tcArgs_tpBadEncoding,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int result;

	tet_infoline("assertion: "__XSTRING(TS_XLATETOF) "/"
	    __XSTRING(TS_XLATETOM) " (*,*,BADENCODING) "
	    "fails with ELF_E_ARGUMENT.");

	result = TET_PASS;

	if (TS_XLATETOF(&ed,&es,ELFDATANONE-1) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;
	else if (TS_XLATETOF(&ed,&es,ELFDATA2MSB+1) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;

	if (TS_XLATETOM(&ed,&es,ELFDATANONE-1) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;
	else if (TS_XLATETOM(&ed,&es,ELFDATA2MSB+1) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;

	tet_result(result);
}

void
__XCONCAT(tcArg_tpDstSrcVersionToF,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int result;
	char buf[sizeof(int)];

	tet_infoline("assertion: "__XSTRING(TS_XLATETOF)"() / "
	    __XSTRING(TS_XLATETOM) "() with unequal "
	    "src,dst versions fails with ELF_E_UNIMPL.");

	es.d_buf     = ed.d_buf = buf;
	es.d_type    = ELF_T_BYTE;
	es.d_size    = ed.d_size = sizeof(buf);
	es.d_version = EV_CURRENT;
	ed.d_version = EV_NONE;

	result = TET_PASS;

	if (TS_XLATETOF(&ed, &es, ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_UNIMPL)
		result = TET_FAIL;

	if (TS_XLATETOM(&ed, &es, ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_UNIMPL)
		result = TET_FAIL;

	tet_result(result);
}

/*
 * Check for an unimplemented type.
 */
void
__XCONCAT(tcArg_tpUnimplemented,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int i, result;
	char *buf;

	tet_infoline("assertion: "__XSTRING(TS_XLATETOF)"() on "
	    "unimplemented types will with ELF_E_UNIMPL.");

	/*
	 * allocate a buffer that is large enough for any potential
	 * ELF data structure.
	 */
	if ((buf = malloc(1024)) == NULL) {
		tet_infoline("unresolved: malloc() failed.");
		tet_result(TET_UNRESOLVED);
		return;
	}

	ed.d_buf = es.d_buf = buf;
	ed.d_size = es.d_size = 1024;
	ed.d_version = es.d_version = EV_CURRENT;

	result = TET_PASS;

	for (i = 0; i < ELF_T_NUM; i++) {
		switch (i) {
		case ELF_T_MOVEP:
#if	TS_XLATESZ == 32
		case ELF_T_SXWORD:
		case ELF_T_XWORD:
#endif
			break;
		default:
			continue;
		}

		es.d_type = i;

		if (TS_XLATETOF(&ed,&es,ELFDATA2LSB) != NULL ||
		    elf_errno() != ELF_E_UNIMPL) {
			tet_printf("fail: TOF/LSB/type=%d.", i);
			result = TET_FAIL;
		}

		if (TS_XLATETOF(&ed,&es,ELFDATA2MSB) != NULL ||
		    elf_errno() != ELF_E_UNIMPL) {
			tet_printf("fail: TOF/MSB/type=%d.", i);
			result = TET_FAIL;
		}

		if (TS_XLATETOM(&ed,&es,ELFDATA2LSB) != NULL ||
		    elf_errno() != ELF_E_UNIMPL) {
			tet_printf("fail: TOM/LSB/type=%d.", i);
			result = TET_FAIL;
		}

		if (TS_XLATETOM(&ed,&es,ELFDATA2MSB) != NULL ||
		    elf_errno() != ELF_E_UNIMPL) {
			tet_printf("fail: TOM/MSB/type=%d.", i);
			result = TET_FAIL;
		}
	}

	tet_result(result);
	free(buf);
}

/*
 * Check for null buffer pointers.
 */
void
__XCONCAT(tcBuffer_tpNullDataPtr,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int result;
	char buf[sizeof(int)];

	tet_infoline("assertion: "__XSTRING(TS_XLATETOF)"() / "
	    __XSTRING(TS_XLATETOM) "() with a null "
	    "src,dst buffer pointer fails with ELF_E_DATA.");

	result = TET_PASS;

	es.d_type    = ELF_T_BYTE;
	es.d_size    = ed.d_size = sizeof(buf);
	es.d_version = EV_CURRENT;
	ed.d_version = EV_CURRENT;

	es.d_buf     = NULL;
	ed.d_buf     = buf;
	if (TS_XLATETOF(&ed, &es, ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA)
		result = TET_FAIL;

	if (TS_XLATETOM(&ed, &es, ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA)
		result = TET_FAIL;

	es.d_buf     = buf;
	ed.d_buf     = NULL;
	if (TS_XLATETOF(&ed, &es, ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA)
		result = TET_FAIL;

	if (TS_XLATETOM(&ed, &es, ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA)
		result = TET_FAIL;

	tet_result(result);
}

/*
 * Misaligned data.
 */

void
__XCONCAT(tcBuffer_tpMisaligned,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int result;
	size_t fsz, msz;
	char *sb, *db;
	struct testdata *td;

	tet_infoline("assertion: misaligned buffers are rejected with "
	    "ELF_E_DATA.");

	sb = db = NULL;
	if ((sb = malloc(1024)) == NULL ||
	    (db = malloc(1024)) == NULL) {
		tet_infoline("unresolved: malloc() failed.");
		tet_result(TET_UNRESOLVED);
		if (sb)
			free(sb);
		return;
	}

	result = TET_PASS;

	for (td = TESTDATASET; td->tsd_name; td++) {
		fsz = td->tsd_fsz;
		msz = td->tsd_msz;

		es.d_type = td->tsd_type;
		es.d_version = EV_CURRENT;

		/* Misalign the destination for to-memory xfers */
		es.d_size = (1024 / fsz) * fsz;	/* round down */
		es.d_buf  = sb;

		ed.d_buf = db + 1;	/* guaranteed to be misaliged */
		ed.d_version = EV_CURRENT;
		ed.d_size = 1024;

		if (TS_XLATETOM(&ed, &es, ELFDATANONE) != NULL ||
		    elf_errno() != ELF_E_DATA) {
			tet_printf("fail: \"%s\" TOM alignment.",
			    td->tsd_name);
			result = TET_FAIL;
		}

		/* Misalign the source for to-file xfers */
		es.d_buf = sb + 1;
		es.d_size = (1024/msz) * msz;	/* round down */
		ed.d_buf = db;

		if (TS_XLATETOF(&ed, &es, ELFDATANONE) != NULL ||
		    elf_errno() != ELF_E_DATA) {
			tet_printf("fail: \"%s\" TOF alignment.",
			    td->tsd_name);
			result = TET_FAIL;
		}
	}

	tet_result(result);
	free(sb);
	free(db);
}


/*
 * Overlapping buffers.
 */
void
__XCONCAT(tcBuffer_tpOverlap,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int result;
	char buf[sizeof(int)];

	tet_infoline("assertion: overlapping buffers are rejected with "
	    "ELF_E_DATA.");

	es.d_buf = buf; 	ed.d_buf = buf+1;
	es.d_version = ed.d_version = EV_CURRENT;
	es.d_size = ed.d_size = sizeof(buf);
	es.d_type = ELF_T_BYTE;

	result = TET_PASS;

	if (TS_XLATETOF(&ed, &es, ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA) {
		tet_infoline("fail: "__XSTRING(TS_XLATETOF));
		result = TET_FAIL;
	}

	if (TS_XLATETOM(&ed, &es, ELFDATANONE) != NULL ||
	    elf_errno() != ELF_E_DATA) {
		tet_infoline("fail: "__XSTRING(TS_XLATETOM));
		result = TET_FAIL;
	}

	tet_result(result);
}

/*
 * Non-integral number of src elements.
 */
void
__XCONCAT(tcBuffer_tpSrcExtra,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int result;
	size_t fsz, msz;
	char *sb, *db;
	struct testdata *td;

	tet_infoline("assertion: mis-sized buffers are rejected with "
	    "ELF_E_DATA.");

	sb = db = NULL;
	if ((sb = malloc(1024)) == NULL ||
	    (db = malloc(1024)) == NULL) {
		tet_infoline("unresolved: malloc() failed.");
		tet_result(TET_UNRESOLVED);
		if (sb)
			free(sb);
		return;
	}

	result = TET_PASS;

	for (td = TESTDATASET; td->tsd_name; td++) {
		fsz = td->tsd_fsz;
		msz = td->tsd_msz;

		es.d_type    = td->tsd_type;
		es.d_version = EV_CURRENT;
		ed.d_version = EV_CURRENT;
		ed.d_buf     = db;
		es.d_buf     = sb;
		ed.d_size    = 1024;

		/* Pad src bytes with extra bytes for to memor */
		es.d_size = fsz+1;

		if (TS_XLATETOM(&ed, &es, ELFDATANONE) != NULL ||
		    elf_errno() != ELF_E_DATA) {
			tet_printf("fail: \"%s\" TOM buffer size.",
			    td->tsd_name);
			result = TET_FAIL;
		}

		es.d_size = msz+1;
		if (TS_XLATETOF(&ed, &es, ELFDATANONE) != NULL ||
		    elf_errno() != ELF_E_DATA) {
			tet_printf("fail: \"%s\" TOF buffer size.",
			    td->tsd_name);
			result = TET_FAIL;
		}
	}

	tet_result(result);
	free(sb);
	free(db);
}

void
__XCONCAT(tcBuffer_tpDstTooSmall,TS_XLATESZ)(void)
{
	Elf_Data ed, es;
	int result;
	struct testdata *td;
	size_t fsz, msz;
	char buf[1024];

	result = TET_PASS;

	tet_infoline("assertion: too small destination buffers are rejected "
	    "with ELF_E_DATA.");

	for (td = TESTDATASET; td->tsd_name; td++) {
		msz = td->tsd_msz;
		fsz = td->tsd_fsz;

		es.d_type    = td->tsd_type;
		es.d_version = ed.d_version = EV_CURRENT;
		es.d_buf     = ed.d_buf = buf;

		es.d_size = (sizeof(buf) / msz) * msz;
		ed.d_size  = 1;	/* too small a size */

		if (TS_XLATETOF(&ed, &es, ELFDATANONE) != NULL ||
		    elf_errno() != ELF_E_DATA) {
			tet_printf("fail: \"%s\" TOF dst size.",
			    td->tsd_name);
			result = TET_FAIL;
		}

		es.d_size = (sizeof(buf) / fsz) * fsz;
		if (TS_XLATETOM(&ed,&es,ELFDATANONE) != NULL ||
		    elf_errno() != ELF_E_DATA) {
			tet_printf("fail: \"%s\" TOF dst size.",
			    td->tsd_name);
			result = TET_FAIL;
		}
	}

	tet_result(result);
}

#endif	/* NO_TESTCASE_FUNCTIONS */
