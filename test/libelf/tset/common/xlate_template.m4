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
 * $Id: xlate_template.m4 2053 2011-10-26 11:50:18Z jkoshy $
 */

/*
 * Boilerplate for testing the *_xlate() functions.
 *
 * This M4-based macro set attempts to generate test functions for
 * testing the elf{32,54}_xlateto{f,m}() and gelf_xlateto{f,m}()
 * functions.
 *
 * The following needs to be kept in mind:
 *
 * - 32 bit ELF code uses a subset of the primitive types used by
 *   64 bit code.  In particular, the Sxword and Xword types do not
 *   exist in the 32 bit ELF definition.
 * - Elf type identifiers `FOO' usually map to a C type name `Foo',
 *   except in the case of a few types.
 * - Elf types `ADDR' and `OFF' use ELF class dependent sizes for initializers.
 * - Each Elf type needs to be associated with a FreeBSD version where
 *   it first appeared, so that the generated code can be made compilable
 *   on older systems.
 */

divert(-1)

ifdef(`TPFNNAME',`',`errprint(`Macro TPFNNAME must be defined.')m4exit(1)')
ifelse(index(TPFNNAME,`32'),-1,`define(`ISELF64',`Y')')
ifelse(index(TPFNNAME,`64'),-1,`define(`ISELF32',`Y')')
ifelse(index(TPFNNAME,`gelf'),0,`define(`ISGELF',`Y')')

/*
 * TO_M_OR_F(M_TEXT,F_TEXT)
 *
 * Selectively expand one of `M_TEXT' or `F_TEXT' depending on whether
 * the function being tested is a *_tom() or *_tof() function.
 */

define(`TO_M_OR_F',`ifelse(eval(index(TPFNNAME,`tom') > 0),1,`$1',`$2')')
define(`__N__',TOUPPER(substr(TPFNNAME,regexp(TPFNNAME,`to[fm]'))))

/*
 * DO(SIZE,TEXT)
 *
 * Invoke `TEXT' in an environment that defines `__SZ__' to SIZE.
 */
define(`DO',`pushdef(`__SZ__',$1)$2`'popdef(`__SZ__')')

/*
 * ELF_TYPE_LIST((TYPE, VERSION)...)
 *
 * Lists all ELF types for which macro expansion is desired and associates
 * each such type with its `C Name'.
 *
 * Note that the following ELF types with variable sized `file'/memory
 * representations need to be handled specially and are not part of
 * this list:
 * - ELF_T_BYTE
 * - ELF_T_GNUHASH
 * - ELF_T_NOTE
 * - ELF_T_VDEF
 * - ELF_T_VNEED
 */
define(`ELF_COMMON_TYPES',
  ``ADDR,	Addr',
   `CAP,	Cap',
   `DYN,	Dyn',
   `EHDR,	Ehdr',
   `HALF,	Half',
   `LWORD,	Lword',
   `MOVE,	Move',
   `OFF,	Off',
   `PHDR,	Phdr',
   `REL,	Rel',
   `RELA,	Rela',
   `SHDR,	Shdr',
   `SWORD,	Sword',
   `SYM,	Sym',
   `SYMINFO,	Syminfo',
   `WORD,	Word'')

define(`ELF32_TYPES',
  `ELF_COMMON_TYPES,
   `_,		_'')

define(`ELF64_TYPES',
  `ELF_COMMON_TYPES,
   `SXWORD,	Sxword',
   `XWORD,	Xword',
   `_,		_'')

/*
 * Tests that need to be written manually: include those for
 * types: BYTE, NOTE and perhaps VDEF and VNEED.
 */

/*
 * _DOTYPE(TYPE)
 *
 * Process one type.  This invokes `__F__' with args: 1=TYPE, 2=C-Name and the
 * the additional arguments specified to `DOELFTYPES' below.
 */
define(`_DOTYPE',`
indir(`__F__',$1,$2,__ARGS__)
')

/*
 * _DOELFTYPES: iterate over an ELF type list.
 */
define(`_DOELFTYPES',
  `ifelse($#,1,`',
    `_DOTYPE($1)
_DOELFTYPES(shift($@))')')

/*
 * DOELFTYPES(MACRO,ARGS...)
 *
 * Invoke `MACRO'(TYPE,C-NAME,ARGS...) for each type in the ELF type list
 * for the current size in macro `__SZ__'.
 */
define(`DOELFTYPES',
  `pushdef(`__F__',defn(`$1'))pushdef(`__ARGS__',`shift($@)')dnl
ifelse(__SZ__,32,`_DOELFTYPES(ELF32_TYPES)',`_DOELFTYPES(ELF64_TYPES)')dnl
popdef(`__ARGS__')popdef(`__F__')')

/*
 * ELFTYPEDEFINITION(TYPE,SZ,ENDIANNESS)
 *
 * Generate the `C' name of the char[] array holding the `raw' bits
 * for an ELF type.
 */
define(`ELFTYPEDEFINITION',`td_$1_$2_$3')

/*
 * ELFMEMSTRUCT(TYPE,SZ)
 *
 * Generate the name for a `C' struct containing the memory
 * representation of an ELF type.
 */
define(`ELFMEMSTRUCT',`$1_$2_mem')

/*
 * ELFTESTS(SZ)
 */
define(`ELFTESTS',`tests$1')

divert(0)

#define	TPBUFSIZE	1024

#define	BYTE_VAL	0xFF
#define	BYTE_SEQ_LSB	0xFF,
#define	BYTE_SEQ_MSB	0xFF,

#define	HALF_SEQ_LSB	0xDC,0xFE,
#define	HALF_SEQ_LSB32	HALF_SEQ_LSB
#define	HALF_SEQ_LSB64	HALF_SEQ_LSB
#define	HALF_SEQ_MSB	0xFE,0xDC,
#define	HALF_SEQ_MSB32	HALF_SEQ_MSB
#define	HALF_SEQ_MSB64	HALF_SEQ_MSB
#define	HALF_VAL	0xFEDC
#define	HALF_VAL32	HALF_VAL
#define	HALF_VAL64	HALF_VAL

#define	WORD_SEQ_LSB	0x98,0xBA,0xDC,0xFE,
#define	WORD_SEQ_MSB	0xFE,0xDC,0xBA,0x98,
#define	WORD_SEQ_LSB32	WORD_SEQ_LSB
#define	WORD_SEQ_MSB32	WORD_SEQ_MSB
#define	WORD_SEQ_LSB64	WORD_SEQ_LSB
#define	WORD_SEQ_MSB64	WORD_SEQ_MSB
#define	WORD_VAL	0xFEDCBA98UL
#define	WORD_VAL32	WORD_VAL
#define	WORD_VAL64	WORD_VAL

#define	QUAD_SEQ_LSB	0x10,0x32,0x54,0x76,\
	0x98,0xBA,0xDC,0xFE,
#define	QUAD_SEQ_MSB	0xFE,0xDC,0xBA,0x98,\
	0x76,0x54,0x32,0x10,
#define	QUAD_VAL	0xFEDCBA9876543210ULL
#define	QUAD_VAL32	QUAD_VAL
#define	QUAD_VAL64	QUAD_VAL

#define	IDENT_BYTES	46,33,46,64,46,35,46,36,46,37,46,94,46,38,46,42
#define	IDENT_VAL	{ IDENT_BYTES }
#define	IDENT_SEQ_LSB	IDENT_BYTES,
#define	IDENT_SEQ_MSB	IDENT_BYTES,

#define	LWORD_SEQ_LSB	QUAD_SEQ_LSB
#define	LWORD_SEQ_LSB32	QUAD_SEQ_LSB
#define	LWORD_SEQ_LSB64	QUAD_SEQ_LSB
#define	LWORD_SEQ_MSB	QUAD_SEQ_MSB
#define	LWORD_SEQ_MSB32	QUAD_SEQ_MSB
#define	LWORD_SEQ_MSB64	QUAD_SEQ_MSB
#define	LWORD_VAL32	QUAD_VAL32
#define	LWORD_VAL64	QUAD_VAL64

#define	SWORD_SEQ_LSB	WORD_SEQ_LSB
#define	SWORD_SEQ_LSB32	WORD_SEQ_LSB
#define	SWORD_SEQ_LSB64	WORD_SEQ_LSB
#define	SWORD_SEQ_MSB	WORD_SEQ_MSB
#define	SWORD_SEQ_MSB32	WORD_SEQ_MSB
#define	SWORD_SEQ_MSB64	WORD_SEQ_MSB
#define	SWORD_VAL32	WORD_VAL32
#define	SWORD_VAL64	WORD_VAL64

#define	SXWORD_SEQ_LSB	QUAD_SEQ_LSB
#define	SXWORD_SEQ_LSB64 QUAD_SEQ_LSB
#define	SXWORD_SEQ_MSB	QUAD_SEQ_MSB
#define	SXWORD_SEQ_MSB64 QUAD_SEQ_MSB
#define	SXWORD_VAL64	QUAD_VAL64

#define	XWORD_SEQ_LSB	QUAD_SEQ_LSB
#define	XWORD_SEQ_LSB64	QUAD_SEQ_LSB
#define	XWORD_SEQ_MSB	QUAD_SEQ_MSB
#define	XWORD_SEQ_MSB64	QUAD_SEQ_MSB
#define	XWORD_VAL64	QUAD_VAL64

/*
 * ELF class dependent types.
 */
#define	ADDR_SEQ_LSB32	WORD_SEQ_LSB
#define	ADDR_SEQ_MSB32	WORD_SEQ_MSB
#define	ADDR_VAL32	WORD_VAL32
#define	OFF_SEQ_LSB32	WORD_SEQ_LSB
#define	OFF_SEQ_MSB32	WORD_SEQ_MSB
#define	OFF_VAL32	WORD_VAL32

#define	ADDR_SEQ_LSB64	QUAD_SEQ_LSB
#define	ADDR_SEQ_MSB64	QUAD_SEQ_MSB
#define	ADDR_VAL64	QUAD_VAL64
#define	OFF_SEQ_LSB64	QUAD_SEQ_LSB
#define	OFF_SEQ_MSB64	QUAD_SEQ_MSB
#define	OFF_VAL64	QUAD_VAL64

#define	NCOPIES		3
#define	NOFFSET		8	/* Every alignment in a quad word. */

divert(-1)
/*
 * Definitions of 32 bit ELF file structures.
 */

define(`ELF_TYPE_E32_CAP',
  `MEMBER(c_tag,	WORD)
   MEMBER(c_un.c_val,	WORD)')

define(`ELF_TYPE_E32_DYN',
  `MEMBER(d_tag,	WORD)
   MEMBER(d_un.d_val,	WORD)')

define(`ELF_TYPE_E32_EHDR',
  `MEMBER(e_ident,	IDENT)
   MEMBER(e_type,	HALF)
   MEMBER(e_machine,	HALF)
   MEMBER(e_version,	WORD)
   MEMBER(e_entry,	WORD)
   MEMBER(e_phoff,	WORD)
   MEMBER(e_shoff,	WORD)
   MEMBER(e_flags,	WORD)
   MEMBER(e_ehsize,	HALF)
   MEMBER(e_phentsize,	HALF)
   MEMBER(e_phnum,	HALF)
   MEMBER(e_shentsize,	HALF)
   MEMBER(e_shnum,	HALF)
   MEMBER(e_shstrndx,	HALF)')

define(`ELF_TYPE_E32_MOVE',
  `MEMBER(m_value,	QUAD)
   MEMBER(m_info,	WORD)
   MEMBER(m_poffset,	WORD)
   MEMBER(m_repeat,	HALF)
   MEMBER(m_stride,	HALF)')

define(`ELF_TYPE_E32_PHDR',
  `MEMBER(p_type,	WORD)
   MEMBER(p_offset,	WORD)
   MEMBER(p_vaddr,	WORD)
   MEMBER(p_paddr,	WORD)
   MEMBER(p_filesz,	WORD)
   MEMBER(p_memsz,	WORD)
   MEMBER(p_flags,	WORD)
   MEMBER(p_align,	WORD)')

define(`ELF_TYPE_E32_REL',
  `MEMBER(r_offset,	WORD)
   MEMBER(r_info,	WORD)')

define(`ELF_TYPE_E32_RELA',
  `MEMBER(r_offset,	WORD)
   MEMBER(r_info,	WORD)
   MEMBER(r_addend,	WORD)')

define(`ELF_TYPE_E32_SHDR',
  `MEMBER(sh_name,	WORD)
   MEMBER(sh_type,	WORD)
   MEMBER(sh_flags,	WORD)
   MEMBER(sh_addr,	WORD)
   MEMBER(sh_offset,	WORD)
   MEMBER(sh_size,	WORD)
   MEMBER(sh_link,	WORD)
   MEMBER(sh_info,	WORD)
   MEMBER(sh_addralign,	WORD)
   MEMBER(sh_entsize,	WORD)')

define(`ELF_TYPE_E32_SYM',
  `MEMBER(st_name,		WORD)
   MEMBER(st_value,	WORD)
   MEMBER(st_size,		WORD)
   MEMBER(st_info,		BYTE)
   MEMBER(st_other,	BYTE)
   MEMBER(st_shndx,	HALF)')

define(`ELF_TYPE_E32_SYMINFO',
  `MEMBER(si_boundto,	HALF)
   MEMBER(si_flags,	HALF)')

define(`ELF_TYPE_E32_VDEF',
  `MEMBER(vd_version,	HALF)
   MEMBER(vd_flags,	HALF)
   MEMBER(vd_ndx,	HALF)
   MEMBER(vd_cnt,	HALF)
   MEMBER(vd_hash,	WORD)
   MEMBER(vd_aux,	WORD)
   MEMBER(vd_next,	WORD)')

define(`ELF_TYPE_E32_VNEED',
  `MEMBER(vn_version,	HALF)
   MEMBER(vn_cnt,	HALF)
   MEMBER(vn_file,	WORD)
   MEMBER(vn_aux,	WORD)
   MEMBER(vn_next,	WORD)')


/*
 * Definitions of 64 bit ELF file structures.
 */

define(`ELF_TYPE_E64_CAP',
  `MEMBER(c_tag,	QUAD)
   MEMBER(c_un.c_val,	QUAD)')

define(`ELF_TYPE_E64_DYN',
  `MEMBER(d_tag,	QUAD)
   MEMBER(d_un.d_val,	QUAD)')

define(`ELF_TYPE_E64_EHDR',
  `MEMBER(e_ident,	IDENT)
   MEMBER(e_type,	HALF)
   MEMBER(e_machine,	HALF)
   MEMBER(e_version,	WORD)
   MEMBER(e_entry,	QUAD)
   MEMBER(e_phoff,	QUAD)
   MEMBER(e_shoff,	QUAD)
   MEMBER(e_flags,	WORD)
   MEMBER(e_ehsize,	HALF)
   MEMBER(e_phentsize,	HALF)
   MEMBER(e_phnum,	HALF)
   MEMBER(e_shentsize,	HALF)
   MEMBER(e_shnum,	HALF)
   MEMBER(e_shstrndx,	HALF)')

define(`ELF_TYPE_E64_MOVE',
  `MEMBER(m_value,	QUAD)
   MEMBER(m_info,	QUAD)
   MEMBER(m_poffset,	QUAD)
   MEMBER(m_repeat,	HALF)
   MEMBER(m_stride,	HALF)')

define(`ELF_TYPE_E64_PHDR',
  `MEMBER(p_type,	WORD)
   MEMBER(p_flags,	WORD)
   MEMBER(p_offset,	QUAD)
   MEMBER(p_vaddr,	QUAD)
   MEMBER(p_paddr,	QUAD)
   MEMBER(p_filesz,	QUAD)
   MEMBER(p_memsz,	QUAD)
   MEMBER(p_align,	QUAD)')

define(`ELF_TYPE_E64_REL',
  `MEMBER(r_offset,	QUAD)
   MEMBER(r_info,	QUAD)')

define(`ELF_TYPE_E64_RELA',
  `MEMBER(r_offset,	QUAD)
   MEMBER(r_info,	QUAD)
   MEMBER(r_addend,	QUAD)')

define(`ELF_TYPE_E64_SHDR',
  `MEMBER(sh_name,	WORD)
   MEMBER(sh_type,	WORD)
   MEMBER(sh_flags,	QUAD)
   MEMBER(sh_addr,	QUAD)
   MEMBER(sh_offset,	QUAD)
   MEMBER(sh_size,	QUAD)
   MEMBER(sh_link,	WORD)
   MEMBER(sh_info,	WORD)
   MEMBER(sh_addralign,	QUAD)
   MEMBER(sh_entsize,	QUAD)')

define(`ELF_TYPE_E64_SYM',
  `MEMBER(st_name,	WORD)
   MEMBER(st_info,	BYTE)
   MEMBER(st_other,	BYTE)
   MEMBER(st_shndx,	HALF)
   MEMBER(st_value,	QUAD)
   MEMBER(st_size,	QUAD)')

define(`ELF_TYPE_E64_SYMINFO',
  `MEMBER(si_boundto,	HALF)
   MEMBER(si_flags,	HALF)')

define(`ELF_TYPE_E64_VDEF',
  `MEMBER(vd_version,	HALF)
   MEMBER(vd_flags,	HALF)
   MEMBER(vd_ndx,	HALF)
   MEMBER(vd_cnt,	HALF)
   MEMBER(vd_hash,	WORD)
   MEMBER(vd_aux,	WORD)
   MEMBER(vd_next,	WORD)')

define(`ELF_TYPE_E64_VNEED',
  `MEMBER(vn_version,	HALF)
   MEMBER(vn_cnt,	HALF)
   MEMBER(vn_file,	WORD)
   MEMBER(vn_aux,	WORD)
   MEMBER(vn_next,	WORD)')

/*
 * MKRAWBITS(TYPE,CNAME,ENDIANNESS,SIZE)
 *
 * Create a char[] array that holds the type's file representation.
 */
define(`MKRAWBITS',
  `static unsigned char ELFTYPEDEFINITION($1,`'__SZ__`',$3)[] = {
ifdef(`ELF_TYPE_E'__SZ__`_$1',
  `pushdef(`MEMBER',`$'2`_SEQ_$3')ELF_TYPE_E'__SZ__`_$1`'popdef(`MEMBER')',
  `$1_SEQ_$3`'__SZ__') };')

divert(0)

ifdef(`ISELF32',
   DO(32,`DOELFTYPES(`MKRAWBITS',LSB)')
   DO(32,`DOELFTYPES(`MKRAWBITS',MSB)'))

ifdef(`ISELF64',
  `DO(64,`DOELFTYPES(`MKRAWBITS',LSB)')
   DO(64,`DOELFTYPES(`MKRAWBITS',MSB)')')

divert(-1)

/*
 * MKMEMSTRUCT(TYPE,CNAME)
 *
 * Define a C-structure with test data for TYPE.
 */
define(`MKMEMSTRUCT',
  `static Elf`'__SZ__`'_$2 ELFMEMSTRUCT($1,__SZ__) =
ifdef(`ELF_TYPE_E'__SZ__`_$1',
  `pushdef(`MEMBER',.`$'1 = `$'2_VAL `,'){
ELF_TYPE_E'__SZ__`_$1
	}popdef(`MEMBER')',
  `$1_VAL`'__SZ__');')

/*
 * MKMEMCHECK(TYPE,CNAME)
 *
 * Generate code to check a memory structure against reference data.
 */
define(`MKMEMCHECK',
  `ifdef(`ELF_TYPE_E'__SZ__`_$1',dnl Structure type
    `pushdef(`_T_',defn(`ELF_TYPE_E'__SZ__`_$1'))dnl
     pushdef(`MEMBER',`
			'if (`ifelse'($`'2,IDENT,memcmp(dt->$`'1,ref->$`'1,sizeof(ref->$`'1)),
			 dt->$`'1 != ref->$`'1)) {
				TP_FAIL("$1: unequal `$'1.");
				goto done;
			})
			_T_
			dt += 1;
     popdef(`MEMBER')popdef(`_T_')',`dnl Primitive type.
			if (memcmp(t, td->tsd_mem, msz) != 0) {
				TP_FAIL("$1 compare failed.");
				goto done;
			}
			t += msz;')')

divert(0)

ifdef(`ISELF32',`DO(32,`DOELFTYPES(`MKMEMSTRUCT')')')
ifdef(`ISELF64',`DO(64,`DOELFTYPES(`MKMEMSTRUCT')')')

struct testdata {
	char			*tsd_name;
	Elf_Type		tsd_type;

	size_t			tsd_fsz;
	const unsigned char	*tsd_lsb;
	const unsigned char	*tsd_msb;
	void			*tsd_mem;
	size_t			tsd_msz;
};

define(`DEFINE_TEST_DATA',
  `[ELF_T_$1] = {
		.tsd_name = "$1",
		.tsd_type = ELF_T_$1,

		.tsd_fsz = sizeof(ELFTYPEDEFINITION($1,__SZ__,LSB)),
		.tsd_lsb = ELFTYPEDEFINITION($1,__SZ__,LSB),
		.tsd_msb = ELFTYPEDEFINITION($1,__SZ__,MSB),

		.tsd_mem = (void *) &ELFMEMSTRUCT($1,__SZ__),
		.tsd_msz = sizeof(ELFMEMSTRUCT($1,__SZ__))
}')

dnl Tests for variable length Elf types.
define(`DEFINE_TEST_DATA_VARIABLE_LENGTH',`
[ELF_T_BYTE] = {
	/* For byte compares, the LSB/MSB and memory data are identical. */
	.tsd_name = "BYTE",
	.tsd_type = ELF_T_BYTE,
	.tsd_fsz = sizeof(ELFTYPEDEFINITION(WORD,__SZ__,LSB)),
	.tsd_lsb = (void *) &ELFMEMSTRUCT(WORD,__SZ__),
	.tsd_msb = (void *) &ELFMEMSTRUCT(WORD,__SZ__),
	.tsd_mem = (void *) &ELFMEMSTRUCT(WORD,__SZ__),
	.tsd_msz = sizeof(ELFMEMSTRUCT(WORD,__SZ__))
}')
define(`MKTD',`DEFINE_TEST_DATA($1) `,'')

ifdef(`ISELF32',`static struct testdata ELFTESTS(32)[] = {
DO(32,`DEFINE_TEST_DATA_VARIABLE_LENGTH'),
DO(32,`DOELFTYPES(`MKTD')')
{ }
};')

ifdef(`ISELF64',`static struct testdata ELFTESTS(64)[] = {
DO(64,`DEFINE_TEST_DATA_VARIABLE_LENGTH'),
DO(64,`DOELFTYPES(`MKTD')')
{ }
};')

divert(-1)

/*
 * CallXlator(ARGS)
 *
 * Munge the call sequence depending on whether a gelf_* function is
 * being tested or not.
 */
define(`CallXlator',`ifdef(`USEGELF',`TPFNNAME (e, $*)',`TPFNNAME ($*)')')

/*
 * CheckXlateResult(SZ)
 */
define(`CheckXlateResult',`
	if (dst->d_type != td->tsd_type || dst->d_size != $1 * ncopies) {
		TP_FAIL("type: ret=%d/expected=%d size: ret=%d/expected=%d",
			dst->d_type, td->tsd_type, dst->d_size, $1*ncopies);
		goto done;
	}')
define(`CheckXlateResultM',`CheckXlateResult(msz)')
define(`CheckXlateResultF',`CheckXlateResult(fsz)')

/*
 * For all xlate tests we need to do the following:
 *
 * 1. Declare variables.
 * 2. Allocate memory.
 * 3. Locate reference data.
 * 4. For each offset:
 *    4a. if doing a ToF: initialize the source buffer (N copies)
 *    4b. if doing a ToM: initialize the source (N copies) at the offset
 *    4c. Invoke the xlator.
 *    4d. Check by memcmp() against the reference.
 *
 * XlatePrelude(TYPE,ENDIANNESS,C-NAME)
 */
define(`XlatePrelude',`
	Elf_Data dst, src, *r;
	struct testdata *td;
	size_t expected_size, fsz, msz;
	int i, offset, result;
	char *srcbuf, *dstbuf, *t;
	TO_M_OR_F(`ifdef(`ELF_TYPE_E'__SZ__`_$1',`
	Elf`'__SZ__`'_$3 *dt, *ref;')',`')

	TP_ANNOUNCE("TPFNNAME""($1,$2) conversion.");

	(void) memset(&dst, 0, sizeof(dst));
	(void) memset(&src, 0, sizeof(src));

	td = &tests`'__SZ__[ELF_T_$1];

	fsz = elf`'__SZ__`'_fsize(td->tsd_type, 1, EV_CURRENT);
	msz = td->tsd_msz;

	result = TET_PASS;

	assert(msz > 0);
	assert(fsz == td->tsd_fsz);	/* Sanity check. */

	srcbuf = dstbuf = NULL;

	TO_M_OR_F(`
	/* Copy to memory. */
	if ((srcbuf = malloc(NCOPIES*fsz + NOFFSET)) == NULL) {
		TP_UNRESOLVED("TPFNNAME"" malloc() failed.");
		goto done;
	}

	if ((dstbuf = calloc(1,NCOPIES*msz)) == NULL) {
		TP_UNRESOLVED("TPFNNAME"" malloc() failed.");
		goto done;
	}',`
	/* Copy to file. */
	if ((srcbuf = malloc(NCOPIES*msz)) == NULL) {
		TP_UNRESOLVED("TPFNNAME"" malloc() failed.");
		goto done;
	}

	if ((dstbuf = calloc(1,NCOPIES*fsz + NOFFSET)) == NULL) {
		TP_UNRESOLVED("TPFNNAME"" malloc() failed.");
		goto done;
	}')
')

/*
 * XlateCopySrcData(TYPE,ENDIANNESS)
 *
 * Copy bytes of src data, and set the src and dst Elf_Data structures.
 */
define(`XlateCopySrcData',`
TO_M_OR_F(`
		t = srcbuf + offset;
		for (i = 0; i < NCOPIES; i++) {
			(void) memcpy(t, td->tsd_`'TOLOWER($2), fsz);
			t += fsz;
		}

		src.d_buf = srcbuf + offset;
		src.d_size = fsz * NCOPIES;
		src.d_type = td->tsd_type;
		src.d_version = EV_CURRENT;

		dst.d_buf = dstbuf;
		dst.d_size = msz * NCOPIES;
		dst.d_version = EV_CURRENT;
		',`
		t = srcbuf;
		for (i = 0; i < NCOPIES; i++) {
			(void) memcpy(t, td->tsd_mem, msz);
			t += msz;
		}

		src.d_buf = srcbuf;
		src.d_size = msz * NCOPIES;
		src.d_type = td->tsd_type;
		src.d_version = EV_CURRENT;

		dst.d_buf = dstbuf + offset;
		dst.d_size = fsz * NCOPIES;
		dst.d_version = EV_CURRENT;')')

/*
 * XlateConvertAndCheck(TYPE,ENDIANNESS,C-NAME)
 *
 * Invoke TPFNNAME () and check the returned buffer type and size.
 */
define(`XlateConvertAndCheck',`
		if ((r = CallXlator(&dst, &src, ELFDATA2$2)) != &dst) {
			TP_FAIL("TPFNNAME""($1:$2) failed: \"%s\".",
			   elf_errmsg(-1));
			goto done;
		}

		expected_size =	NCOPIES * TO_M_OR_F(`msz',`fsz');

		if (dst.d_type != td->tsd_type ||
		    dst.d_size != expected_size) {
			TP_FAIL("TPFNNAME""($1:$2) type(%d != %d expected), "
			    "size(%d != %d expected).", dst.d_type, td->tsd_type,
			    dst.d_size, expected_size);
			goto done;
		}
		TO_M_OR_F(`
		/* Check returned memory data. */
ifdef(`ELF_TYPE_E'__SZ__`_$1',`
		dt = (Elf`'__SZ__`'_$3 *) (uintptr_t) dst.d_buf;
		ref = (Elf`'__SZ__`'_$3 *) td->tsd_mem;',`
		t = dst.d_buf;')

		for (i = 0; i < NCOPIES; i++) {
			MKMEMCHECK($1)
		}',`
		/* Check returned file data. */
		t = dst.d_buf;
		for (i = 0; i < NCOPIES; i++) {
			if (memcmp(t, td->tsd_`'TOLOWER($2), fsz) != 0) {
				TP_FAIL("$1 compare failed.");
				goto done;
			}
			t += fsz;
		}')
')

/*
 * MKCONVERSIONTP(TYPE,C-Name,ENDIANNESS)
 *
 * Generate a test purpose that tests conversions for Elf type TYPE.
 */
define(`MKCONVERSIONTP',`
void
tcXlate_tp$1_$3`'__SZ__ (void)
{
	XlatePrelude($1,$3,$2)

	result = TET_PASS;

	for (offset = 0; offset < NOFFSET; offset++) {
		XlateCopySrcData($1,$3)
		XlateConvertAndCheck($1,$3,$2)
	}

 done:
	if (srcbuf)
		free(srcbuf);
	if (dstbuf)
		free(dstbuf);
	tet_result(result);
}')

/*
 * Xlate_TestConversions_Byte()
 *
 * Test byte conversions.
 */
define(`Xlate_TestConversions_Byte',`
void
tcXlate_tpByte`'__SZ__ (void)
{
	Elf_Data dst, src, *r;
	int i, offset, result;
	size_t expected_size, fsz, msz;
	struct testdata *td;
	char srcbuf[NCOPIES*sizeof(ELFTYPEDEFINITION(WORD,__SZ__,LSB)) + NOFFSET];
	char dstbuf[sizeof(srcbuf)];
	char *t;

	TP_ANNOUNCE("TPFNNAME""(BYTE) conversion.");

	(void) memset(&dst, 0, sizeof(dst));
	(void) memset(&src, 0, sizeof(src));

	td = &tests`'__SZ__[ELF_T_BYTE];

	fsz = msz = sizeof(ELFTYPEDEFINITION(WORD,__SZ__,LSB));
	expected_size = NCOPIES * msz;
	result = TET_PASS;

	for (offset = 0; offset < NOFFSET; offset++) {

		XlateCopySrcData(BYTE,LSB);
		XlateConvertAndCheck(BYTE,LSB);
		XlateConvertAndCheck(BYTE,MSB,Word);
	}

 done:
	tet_result(result);
}')

define(`Xlate_TestConversions_Note')

/*
 * Xlate_TestConversions
 *
 * Make test cases th non-byte conversions from file representations
 * to memory.
 */
define(`Xlate_TestConversions',`
ifdef(`ISELF32',dnl
`DO(32,`Xlate_TestConversions_Byte
Xlate_TestConversions_Note')
DO(32,`DOELFTYPES(`MKCONVERSIONTP',LSB)')
DO(32,`DOELFTYPES(`MKCONVERSIONTP',MSB)')')
ifdef(`ISELF64',dnl
`DO(64,`Xlate_TestConversions_Byte
Xlate_TestConversions_Note')
DO(64,`DOELFTYPES(`MKCONVERSIONTP',LSB)')
DO(64,`DOELFTYPES(`MKCONVERSIONTP',MSB)')')')

define(`Xlate_TestSharedConversions_Byte',`
void
tcXlate_tpByteShared`'__SZ__ (void)
{
	Elf_Data dst, src, *r;
	int i, offset, result;
	size_t expected_size, fsz, msz;
	struct testdata *td;
	char srcbuf[NCOPIES*sizeof(ELFTYPEDEFINITION(WORD,__SZ__,LSB))];
	char *dstbuf;
	char *t;

	TP_ANNOUNCE("Test TPFNNAME""(BYTE) shared-buffer conversion.");

	(void) memset(&dst, 0, sizeof(dst));
	(void) memset(&src, 0, sizeof(src));

	td = &tests`'__SZ__[ELF_T_BYTE];

	fsz = msz = sizeof(ELFTYPEDEFINITION(WORD,__SZ__,LSB));
	expected_size = NCOPIES * msz;
	result = TET_PASS;
	dstbuf = srcbuf;
	offset = 0;

	XlateCopySrcData(BYTE,LSB);
	XlateConvertAndCheck(BYTE,LSB,Word);
	XlateConvertAndCheck(BYTE,MSB,Word);

 done:
	tet_result(result);
}')

define(`Xlate_TestSharedConversions_Note')

define(`MKSHAREDCONVERSIONTP',`
void
tcXlate_tpShared$1_$3`'__SZ__ (void)
{
	Elf_Data dst, src, *r;
	struct testdata *td;
	size_t expected_size, fsz, msz;
	int i, result;
	char *srcbuf, *t;
	TO_M_OR_F(`ifdef(`ELF_TYPE_E'__SZ__`_$1',`
	Elf`'__SZ__`'_$2 *dt, *ref;')',`')

	TP_ANNOUNCE("TPFNNAME""($1,$3) conversion.");

	(void) memset(&dst, 0, sizeof(dst));
	(void) memset(&src, 0, sizeof(src));

	td = &tests`'__SZ__[ELF_T_$1];

	fsz = elf`'__SZ__`'_fsize(td->tsd_type, 1, EV_CURRENT);
	msz = td->tsd_msz;

	result = TET_PASS;

	assert(msz > 0);
	assert(fsz == td->tsd_fsz);	/* Sanity check. */

	srcbuf = t = NULL;
	if ((srcbuf = malloc(NCOPIES*msz)) == NULL) {
		TP_UNRESOLVED("TPFNNAME"" malloc() failed.");
		goto done;
	}

	src.d_buf = dst.d_buf = srcbuf;
	src.d_version = dst.d_version = EV_CURRENT;
	TO_M_OR_F(`src.d_size = fsz * NCOPIES;
	dst.d_size = msz * NCOPIES;',`dnl
	src.d_size = msz * NCOPIES;
	dst.d_size = fsz * NCOPIES;')
	src.d_type = dst.d_type = ELF_T_$1;
	t = srcbuf;
	for (i = 0; i < NCOPIES; i++) {
	TO_M_OR_F(`
		(void) memcpy(t, td->tsd_`'TOLOWER($3), fsz);
		t += fsz;',`
		(void) memcpy(t, td->tsd_mem, msz);
		t += msz;')
	}

	result = TET_PASS;

	XlateConvertAndCheck($1,$3,$2)

 done:
	if (srcbuf)
		free(srcbuf);
	tet_result(result);
}')

define(`Xlate_TestConversionsSharedBuffer',`
ifdef(`ISELF32',dnl
`DO(32,`Xlate_TestSharedConversions_Byte
Xlate_TestSharedConversions_Note')
DO(32,`DOELFTYPES(`MKSHAREDCONVERSIONTP',LSB)')
DO(32,`DOELFTYPES(`MKSHAREDCONVERSIONTP',MSB)')')
ifdef(`ISELF64',dnl
`DO(64,`Xlate_TestSharedConversions_Byte
Xlate_TestSharedConversions_Note')
DO(64,`DOELFTYPES(`MKSHAREDCONVERSIONTP',LSB)')
DO(64,`DOELFTYPES(`MKSHAREDCONVERSIONTP',MSB)')')')

define(`Xlate_TestBadArguments',`
void
tcArgs_tpNullArgs(void)
{
	Elf_Data ed, *r;
	int error, result;

	TP_ANNOUNCE("TPFNNAME () with NULL arguments fails with "
	    "ELF_E_ARGUMENT");

	memset(&ed, 0, sizeof(ed));

	result = TET_PASS;

	if ((r = CallXlator(NULL, &ed, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("TPFNNAME(NULL, *, LSB) failed: r=%p error=\"%s\".",
		    (void *) r, elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(NULL, &ed, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("TPFNNAME(NULL, *, MSB) failed: r=%p error=\"%s\".",
		    (void *) r, elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, NULL, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("TPFNNAME(*, NULL, LSB) failed: r=%p error=\"%s\".",
		    (void *) r, elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, NULL, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("TPFNNAME(*, NULL, MSB) failed: r=%p error=\"%s\".",
		    (void *) r, elf_errmsg(error));

 done:
	tet_result(result);
}

void
tcArgs_tpBadType(void)
{

	Elf_Data ed, es, *r;
	int error, result;
	char buf[1024];

	TP_ANNOUNCE("TPFNNAME () with an out of range type fails with "
	    "ELF_E_DATA.");

	result = TET_PASS;

	(void) memset(&es, 0, sizeof(es));
	(void) memset(&ed, 0, sizeof(ed));

	es.d_version = ed.d_version = EV_CURRENT;
	es.d_buf     = ed.d_buf = buf;
	es.d_size    = ed.d_size = sizeof(buf);

	es.d_type = (Elf_Type) -1;

	if ((r = CallXlator(&ed, &es, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA) {
		TP_FAIL("TPFNNAME (*, *, LSB) (%d): r=%p error=\"%s\".",
		    es.d_type, (void *) r, elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, &es, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA) {
		TP_FAIL("TPFNNAME (*, *, MSB) (%d): r=%p error=\"%s\".",
		    es.d_type, (void *) r, elf_errmsg(error));
		goto done;
	}

	es.d_type = ELF_T_NUM;

	if ((r = CallXlator(&ed, &es, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA) {
		TP_FAIL("TPFNNAME (*, *, LSB) (%d): r=%p error=%\"%s\".",
		    es.d_type, (void *) r, elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, &es, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA)
		TP_FAIL("TPFNNAME (*, *, MSB) (%d): r=%p error=\"%s\".",
		    es.d_type, (void *) r, elf_errmsg(error));


 done:
	tet_result(result);
}

void
tcArgs_tpBadEncoding(void)
{
	Elf_Data ed, es, *r;
	int error, result;

	TP_ANNOUNCE("TPFNNAME (*,*,BADENCODING) fails with "
	    "ELF_E_ARGUMENT.");

	(void) memset(&ed, 0, sizeof(ed));
	(void) memset(&es, 0, sizeof(es));

	result = TET_PASS;

	if ((r = CallXlator(&ed, &es, ELFDATANONE-1)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("TPFNNAME (*, *, %d): r=%p error=\"%s\".",
		    ELFDATANONE-1, r, elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, &es, ELFDATA2MSB+1)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("TPFNNAME (*, *, %d): r=%p error=\"%s\".",
		    ELFDATA2MSB+1, r, elf_errmsg(error));

 done:
	tet_result(result);
}

void
tcArgs_tpDstVersion(void)
{
	Elf_Data ed, es, *r;
	int error, result;
	char buf[sizeof(int)];

	TP_ANNOUNCE("TPFNNAME (*,*,*) with an illegal dst version "
	    "fails with ELF_E_UNIMPL.");

	(void) memset(&ed, 0, sizeof(ed));
	(void) memset(&es, 0, sizeof(es));

	es.d_buf     = ed.d_buf = buf;
	es.d_type    = ELF_T_BYTE;
	es.d_size    = ed.d_size = sizeof(buf);
	es.d_version = EV_CURRENT;
	ed.d_version = EV_NONE;

	result = TET_PASS;

	if ((r = CallXlator(&ed, &es, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_UNIMPL) {
		TP_FAIL("TPFNNAME (*,*,LSB) ver=%d r=%p error=\"%s\".",
		    ed.d_version, r, elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, &es, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_UNIMPL)
		TP_FAIL("TPFNNAME (*,*,MSB) ver=%d r=%p error=\"%s\".",
		    ed.d_version, r, elf_errmsg(error));

 done:
	tet_result(result);
}

void
tcArgs_tpSrcVersion(void)
{
	Elf_Data ed, es, *r;
	int error, result;
	char buf[sizeof(int)];

	TP_ANNOUNCE("TPFNNAME (*,*,*) with an illegal src version fails "
	    "with ELF_E_UNIMPL.");

	(void) memset(&ed, 0, sizeof(ed));
	(void) memset(&es, 0, sizeof(es));

	es.d_buf     = ed.d_buf = buf;
	es.d_type    = ELF_T_BYTE;
	es.d_size    = ed.d_size = sizeof(buf);
	es.d_version = EV_CURRENT+1;
	ed.d_version = EV_CURRENT;

	result = TET_PASS;

	if ((r = CallXlator(&ed, &es, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_UNIMPL) {
		TP_FAIL("TPFNNAME (*,*,LSB) ver=%d r=%p error=\"%s\".",
		    es.d_version, r, elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, &es, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_UNIMPL)
		TP_FAIL("TPFNNAME (*,*,MSB) ver=%d r=%p error=\"%s\".",
		    es.d_version, r, elf_errmsg(error));

 done:
	tet_result(result);
}

/*
 * Check for an unimplemented type.
 */
void
tcArgs_tpUnimplemented(void)
{
	Elf_Data ed, es, *r;
	int error, i, result;
	char sbuf[TPBUFSIZE]; /* large enough for any ELF type */
	char dbuf[TPBUFSIZE];

	TP_ANNOUNCE("TPFNNAME""() on unimplemented types fails with "
	    "ELF_E_UNIMPL.");

	(void) memset(&ed, 0, sizeof(ed));
	(void) memset(&es, 0, sizeof(es));

	ed.d_buf = dbuf; ed.d_size = sizeof(dbuf);
	es.d_buf = sbuf; es.d_size = sizeof(sbuf);
	es.d_version = ed.d_version = EV_CURRENT;

	result = TET_PASS;

	for (i = 0; i < ELF_T_NUM; i++) {
		/* Skip over supported types. */
		switch (i) {
		case ELF_T_MOVEP:
		ifelse(ISELF64,`Y',`',`
		case ELF_T_SXWORD:
		case ELF_T_XWORD:
')
			break;
		default:
			continue;
		}

		es.d_type = i;

		if ((r = CallXlator(&ed, &es, ELFDATA2LSB)) != NULL ||
		    (error = elf_errno()) != ELF_E_UNIMPL) {
			TP_FAIL("TPFNNAME (*,*,LSB): type=%d r=%p "
			    "error=\"%s\".", i, r, elf_errmsg(error));
			goto done;
		}

		if ((r = CallXlator(&ed, &es, ELFDATA2MSB)) != NULL ||
		    (error = elf_errno()) != ELF_E_UNIMPL) {
			TP_FAIL("TPFNNAME (*,*,LSB): type=%d r=%p "
			    "error=\"%s\".", i, r, elf_errmsg(error));
			goto done;
		}
	}

  done:
	tet_result(result);
}
')

/*
 * MKMISALIGNEDTP(TYPE,C-NAME)
 *
 * Generate a test case for checking misaligned buffers.
 */

define(`MKMISALIGNEDTP',`
void
tcBuffer_tpMisaligned_$1_`'__SZ__`'(void)
{
	Elf_Data ed, es, *r;
	int count, error, result;
	size_t fsz, msz;
	char sb[TPBUFSIZE], db[TPBUFSIZE];
	struct testdata *td;

	TP_ANNOUNCE("TPFNNAME""($1) misaligned buffers with "
	    "ELF_E_DATA.");

	result = TET_PASS;

	td = &tests`'__SZ__[ELF_T_$1];
	fsz = td->tsd_fsz;
	msz = td->tsd_msz;

	(void) memset(&ed, 0, sizeof(ed));
	(void) memset(&es, 0, sizeof(es));

	es.d_type = es.d_type = td->tsd_type;
	es.d_version = ed.d_version = EV_CURRENT;

	count = sizeof(sb) / msz; /* Note: msz >= fsz always. */

	TO_M_OR_F(`
	/* Misalign the destination for to-memory xfers. */
	es.d_size = count * fsz;
	ed.d_size = count * msz;

	es.d_buf  = sb;
	ed.d_buf  = db + 1;	/* Guaranteed to be misaliged. */
	',`
	/* Misalign the source for to-file xfers. */

	es.d_size = count * msz;
	ed.d_size = count * fsz;

	es.d_buf = sb + 1;	/* Guaranteed to be misaliged. */
	ed.d_buf = db;')

	if ((r = CallXlator(&ed, &es, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA) {
		TP_FAIL("TPFNNAME""(LSB) r=%p error=\"%s\".", r,
		    elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, &es, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA)
		TP_FAIL("TPFNNAME""(MSB) r=%p error=\"%s\".", r,
		    elf_errmsg(error));

 done:
	tet_result(result);
}')

define(`MKNONINTEGRALSRC',`
void
tcBuffer_tpSrcExtra_$1_`'__SZ__`'(void)
{
	Elf_Data ed, es, *r;
	int count, error, result;
	size_t fsz, msz;
	char sb[TPBUFSIZE], db[TPBUFSIZE];
	struct testdata *td;

	TP_ANNOUNCE("TPFNNAME""($1) mis-sized source buffer is rejected with "
	    "ELF_E_DATA.");

	result = TET_PASS;

	td = &tests`'__SZ__[ELF_T_$1];
	fsz = td->tsd_fsz;
	msz = td->tsd_msz;

	(void) memset(&ed, 0, sizeof(ed));
	(void) memset(&es, 0, sizeof(es));

	ed.d_type = es.d_type = td->tsd_type;
	ed.d_version = es.d_version = EV_CURRENT;
	es.d_buf = sb; ed.d_buf = db;

	count = (sizeof(db) / msz) - 1;	/* Note: msz >= fsz always. */

	/* Add an extra byte to the source buffer size. */
	TO_M_OR_F(`
	es.d_size = (count * fsz) + 1;
	ed.d_size = count * msz;',`
	es.d_size = (count * msz) + 1;
	ed.d_size = count * fsz;')

	if ((r = CallXlator(&ed, &es, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA) {
		TP_FAIL("TPFNNAME""(LSB) r=%p error=\"%s\".", r,
		    elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, &es, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA)
		TP_FAIL("TPFNNAME""(LSB) r=%p error=\"%s\".", r,
		    elf_errmsg(error));

 done:
	tet_result(result);

}')

define(`MKDSTTOOSMALL',`
void
tcBuffer_tpDstTooSmall_$1_`'__SZ__`'(void)
{
 	Elf_Data ed, es, *r;
 	int count, error, result;
 	struct testdata *td;
 	size_t fsz, msz;
 	char sb[TPBUFSIZE], db[TPBUFSIZE];

	TP_ANNOUNCE("TPFNNAME""($1) small destination buffers are rejected "
	    "with ELF_E_DATA.");

 	result = TET_PASS;

	td = &tests`'__SZ__[ELF_T_$1];
	fsz = td->tsd_fsz;
	msz = td->tsd_msz;

	(void) memset(&ed, 0, sizeof(ed));
	(void) memset(&es, 0, sizeof(es));

	count = sizeof(sb) / msz; /* Note: msz >= fsz always. */

	ed.d_type    = es.d_type    = td->tsd_type;
	ed.d_version = es.d_version = EV_CURRENT;
	es.d_buf     = sb; ed.d_buf = db;
	ed.d_size    = 1;

	TO_M_OR_F(`es.d_size = sizeof(sb) / fsz;',
	    `es.d_size = sizeof(sb) / msz;')

	if ((r = CallXlator(&ed, &es, ELFDATA2LSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA) {
		TP_FAIL("TPFNNAME""(LSB) r=%p error=\"%s\".", r,
		    elf_errmsg(error));
		goto done;
	}

	if ((r = CallXlator(&ed, &es, ELFDATA2MSB)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA)
		TP_FAIL("TPFNNAME""(LSB) r=%p error=\"%s\".", r,
		    elf_errmsg(error));

 done:
	tet_result(result);
}')

define(`Xlate_TestBadBuffers',`
void
tcBuffer_tpNullDataPtr(void)
{
	Elf_Data ed, es, *r;
	int error, result;
	char buf[sizeof(int)];

	TP_ANNOUNCE("TPFNNAME" "(...) with null d_buf pointers fails with "
	    "ELF_E_DATA.");

	(void) memset(&ed, 0, sizeof(ed));
	(void) memset(&es, 0, sizeof(es));

	result = TET_PASS;

	es.d_type    = ELF_T_BYTE;
	es.d_size    = ed.d_size = sizeof(buf);
	es.d_version = EV_CURRENT;
	ed.d_version = EV_CURRENT;

	es.d_buf     = NULL;
	ed.d_buf     = buf;
	if ((r = CallXlator(&ed, &es, ELFDATANONE)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA) {
		TP_FAIL("TPFNNAME""(...) src.d_buf=NULL r=%d error=\"%s\".",
		    r, elf_errmsg(error));
		goto done;
	}

	es.d_buf     = buf;
	ed.d_buf     = NULL;

	if ((r = CallXlator(&ed, &es, ELFDATANONE)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA)
		TP_FAIL("TPFNNAME""(...) dst.d_buf=NULL r=%d error=\"%s\".",
		    r, elf_errmsg(error));

 done:
	tet_result(result);
}

/*
 * Misaligned data.
 */

ifdef(`ISELF32',`DO(32,`DOELFTYPES(`MKMISALIGNEDTP')')')
ifdef(`ISELF64',`DO(64,`DOELFTYPES(`MKMISALIGNEDTP')')')

/*
 * Overlapping buffers.
 */
void
tcBuffer_tpOverlap(void)
{
	Elf_Data ed, es, *r;
	int error, result;
	char buf[sizeof(int)];

	TP_ANNOUNCE("TPFNNAME""(...) overlapping buffers are rejected with "
	    "ELF_E_DATA.");

	es.d_buf = buf; 	ed.d_buf = buf+1;
	es.d_version = ed.d_version = EV_CURRENT;
	es.d_size = ed.d_size = sizeof(buf);
	es.d_type = ELF_T_BYTE;

	result = TET_PASS;

	if ((r = CallXlator(&ed, &es, ELFDATANONE)) != NULL ||
	    (error = elf_errno()) != ELF_E_DATA)
		TP_FAIL("r=%p error=\"%s\".", r, elf_errmsg(error));

	tet_result(result);
}

/*
 * Non-integral number of src elements.
 */
ifdef(`ISELF32',`DO(32,`DOELFTYPES(`MKNONINTEGRALSRC')')')
ifdef(`ISELF64',`DO(64,`DOELFTYPES(`MKNONINTEGRALSRC')')')

/*
 * Destination too small.
 */
ifdef(`ISELF32',`DO(32,`DOELFTYPES(`MKDSTTOOSMALL')')')
ifdef(`ISELF64',`DO(64,`DOELFTYPES(`MKDSTTOOSMALL')')')

')
divert(0)
