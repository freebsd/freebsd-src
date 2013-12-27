/*-
 * Copyright (c) 2007 John Birrell (jb@freebsd.org)
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
 * $FreeBSD$
 */

#ifndef	__LIBDWARF_H_
#define	__LIBDWARF_H_

#include <sys/param.h>
#include <sys/queue.h>
#include <stdio.h>
#include <gelf.h>
#include "dwarf.h"
#include "libdwarf.h"

#define DWARF_debug_abbrev		0
#define DWARF_debug_aranges		1
#define DWARF_debug_frame		2
#define DWARF_debug_info		3
#define DWARF_debug_line		4
#define DWARF_debug_pubnames		5
#define DWARF_eh_frame			6
#define DWARF_debug_macinfo		7
#define DWARF_debug_str			8
#define DWARF_debug_loc			9
#define DWARF_debug_pubtypes		10
#define DWARF_debug_ranges		11
#define DWARF_debug_static_func		12
#define DWARF_debug_static_vars		13
#define DWARF_debug_types		14
#define DWARF_debug_weaknames		15
#define DWARF_symtab			16
#define DWARF_strtab			17
#define DWARF_DEBUG_SNAMES		18

#define DWARF_DIE_HASH_SIZE		8191

#define	DWARF_SET_ERROR(_e, _err)	do { 		\
	_e->err_error = _err;				\
	_e->elf_error = 0;				\
	_e->err_func  = __func__;			\
	_e->err_line  = __LINE__;			\
	_e->err_msg[0] = '\0';				\
	} while (0)

#define	DWARF_SET_ELF_ERROR(_e, _err)	do { 		\
	_e->err_error = DWARF_E_ELF;			\
	_e->elf_error = _err;				\
	_e->err_func  = __func__;			\
	_e->err_line  = __LINE__;			\
	_e->err_msg[0] = '\0';				\
	} while (0)

struct _Dwarf_AttrValue {
	uint64_t	av_attrib;	/* DW_AT_ */
	uint64_t	av_form;	/* DW_FORM_ */
	union {
		uint64_t	u64;
		int64_t		s64;
		const char	*s;
		uint8_t		*u8p;
	} u[2];				/* Value. */
	STAILQ_ENTRY(_Dwarf_AttrValue)
			av_next;	/* Next attribute value. */
};

struct _Dwarf_Die {
	int		die_level;	/* Parent-child level. */
	uint64_t	die_offset;	/* DIE offset in section. */
	uint64_t	die_abnum;	/* Abbrev number. */
	Dwarf_Abbrev	die_a;		/* Abbrev pointer. */
	Dwarf_CU	die_cu;		/* Compilation unit pointer. */
	const char	*die_name;	/* Ptr to the name string. */
	STAILQ_HEAD(, _Dwarf_AttrValue)
			die_attrval;	/* List of attribute values. */
	STAILQ_ENTRY(_Dwarf_Die)
			die_next;	/* Next die in list. */
	STAILQ_ENTRY(_Dwarf_Die)
			die_hash;	/* Next die in hash table. */
};

struct _Dwarf_Attribute {
	uint64_t	at_attrib;	/* DW_AT_ */
	uint64_t	at_form;	/* DW_FORM_ */
	STAILQ_ENTRY(_Dwarf_Attribute)
			at_next;	/* Next attribute. */
};

struct _Dwarf_Abbrev {
	uint64_t	a_entry;	/* Abbrev entry. */
	uint64_t	a_tag;		/* Tag: DW_TAG_ */
	uint8_t		a_children;	/* DW_CHILDREN_no or DW_CHILDREN_yes */
	STAILQ_HEAD(, _Dwarf_Attribute)
			a_attrib;	/* List of attributes. */
	STAILQ_ENTRY(_Dwarf_Abbrev)
			a_next;		/* Next abbrev. */
};

struct _Dwarf_CU {
	uint64_t	cu_offset;	/* Offset to the this compilation unit. */
	uint32_t	cu_length;	/* Length of CU data. */
	uint32_t	cu_header_length;
					/* Length of the CU header. */
	uint16_t	cu_version;	/* DWARF version. */
	uint64_t	cu_abbrev_offset;
					/* Offset into .debug_abbrev. */
	uint8_t		cu_pointer_size;
					/* Number of bytes in pointer. */
	uint64_t	cu_next_offset;
					/* Offset to the next compilation unit. */
	STAILQ_HEAD(, _Dwarf_Abbrev)
			cu_abbrev;	/* List of abbrevs. */
	STAILQ_HEAD(, _Dwarf_Die)
			cu_die;		/* List of dies. */
	STAILQ_HEAD(, _Dwarf_Die)
			cu_die_hash[DWARF_DIE_HASH_SIZE];
					/* Hash of dies. */
	STAILQ_ENTRY(_Dwarf_CU)
			cu_next;	/* Next compilation unit. */
};

typedef struct _Dwarf_section {
	Elf_Scn		*s_scn;		/* Section pointer. */
	GElf_Shdr	s_shdr;		/* Copy of the section header. */
	char		*s_sname;	/* Ptr to the section name. */
	uint32_t	s_shnum;	/* Section number. */
	Elf_Data	*s_data;	/* Section data. */
} Dwarf_section;

struct _Dwarf_Debug {
	Elf		*dbg_elf;	/* Ptr to the ELF handle. */
	GElf_Ehdr	dbg_ehdr;	/* Copy of the ELF header. */
	int		dbg_elf_close;	/* True if elf_end() required. */
	int		dbg_mode;	/* Access mode. */
	size_t		dbg_stnum;	/* Section header string table section number. */
	int		dbg_offsize;	/* DWARF offset size. */
	Dwarf_section	dbg_s[DWARF_DEBUG_SNAMES];
					/* Array of section information. */
	STAILQ_HEAD(, _Dwarf_CU)
			dbg_cu;		/* List of compilation units. */
	Dwarf_CU	dbg_cu_current;
					/* Ptr to the current compilation unit. */

	STAILQ_HEAD(, _Dwarf_Func) dbg_func; /* List of functions */
};

struct _Dwarf_Func {
	Dwarf_Die	func_die;
	const char	*func_name;
	Dwarf_Addr	func_low_pc;
	Dwarf_Addr	func_high_pc;
	int		func_is_inlined;
	/* inlined instance */
	STAILQ_HEAD(, _Dwarf_Inlined_Func) func_inlined_instances;
	STAILQ_ENTRY(_Dwarf_Func) func_next;
};

struct _Dwarf_Inlined_Func {
	struct _Dwarf_Func *ifunc_origin;
	Dwarf_Die	ifunc_abstract;
	Dwarf_Die	ifunc_concrete;
	Dwarf_Addr	ifunc_low_pc;
	Dwarf_Addr	ifunc_high_pc;
	STAILQ_ENTRY(_Dwarf_Inlined_Func) ifunc_next;
};

void	dwarf_build_function_table(Dwarf_Debug dbg);

#ifdef DWARF_DEBUG
#include <assert.h>
#define DWARF_ASSERT(x)	assert(x)
#else
#define DWARF_ASSERT(x)
#endif

#endif /* !__LIBDWARF_H_ */
