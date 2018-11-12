/*-
 * Copyright (c) 2007-2012 Kai Wang
 * Copyright (c) 2003 David O'Brien.  All rights reserved.
 * Copyright (c) 2001 Jake Burkholder
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
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <ar.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <getopt.h>
#include <libelftc.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_LIBARCHIVE_AR
#include <archive.h>
#include <archive_entry.h>
#endif

#include "_elftc.h"

ELFTC_VCSID("$Id: elfdump.c 3198 2015-05-14 18:36:19Z emaste $");

#if defined(ELFTC_NEED_ELF_NOTE_DEFINITION)
#include "native-elf-format.h"
#if ELFTC_CLASS == ELFCLASS32
typedef Elf32_Nhdr	Elf_Note;
#else
typedef Elf64_Nhdr	Elf_Note;
#endif
#endif

/* elfdump(1) options. */
#define	ED_DYN		(1<<0)
#define	ED_EHDR		(1<<1)
#define	ED_GOT		(1<<2)
#define	ED_HASH		(1<<3)
#define	ED_INTERP	(1<<4)
#define	ED_NOTE		(1<<5)
#define	ED_PHDR		(1<<6)
#define	ED_REL		(1<<7)
#define	ED_SHDR		(1<<8)
#define	ED_SYMTAB	(1<<9)
#define	ED_SYMVER	(1<<10)
#define	ED_CHECKSUM	(1<<11)
#define	ED_ALL		((1<<12)-1)

/* elfdump(1) run control flags. */
#define	SOLARIS_FMT		(1<<0)
#define	PRINT_FILENAME		(1<<1)
#define	PRINT_ARSYM		(1<<2)
#define	ONLY_ARSYM		(1<<3)

/* Convenient print macro. */
#define	PRT(...)	fprintf(ed->out, __VA_ARGS__)

/* Internal data structure for sections. */
struct section {
	const char	*name;		/* section name */
	Elf_Scn		*scn;		/* section scn */
	uint64_t	 off;		/* section offset */
	uint64_t	 sz;		/* section size */
	uint64_t	 entsize;	/* section entsize */
	uint64_t	 align;		/* section alignment */
	uint64_t	 type;		/* section type */
	uint64_t	 flags;		/* section flags */
	uint64_t	 addr;		/* section virtual addr */
	uint32_t	 link;		/* section link ndx */
	uint32_t	 info;		/* section info ndx */
};

struct spec_name {
	const char	*name;
	STAILQ_ENTRY(spec_name)	sn_list;
};

/* Structure encapsulates the global data for readelf(1). */
struct elfdump {
	FILE		*out;		/* output redirection. */
	const char	*filename;	/* current processing file. */
	const char	*archive;	/* archive name */
	int		 options;	/* command line options. */
	int		 flags;		/* run control flags. */
	Elf		*elf;		/* underlying ELF descriptor. */
#ifndef USE_LIBARCHIVE_AR
	Elf		*ar;		/* ar(1) archive descriptor. */
#endif
	GElf_Ehdr	 ehdr;		/* ELF header. */
	int		 ec;		/* ELF class. */
	size_t		 shnum;		/* #sections. */
	struct section	*sl;		/* list of sections. */
	STAILQ_HEAD(, spec_name) snl;	/* list of names specified by -N. */
};

/* Relocation entry. */
struct rel_entry {
	union {
		GElf_Rel rel;
		GElf_Rela rela;
	} u_r;
	const char *symn;
	uint32_t type;
};

#if defined(ELFTC_NEED_BYTEORDER_EXTENSIONS)
static __inline uint32_t
be32dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static __inline uint32_t
le32dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}
#endif

/* http://www.sco.com/developers/gabi/latest/ch5.dynamic.html#tag_encodings */
static const char *
d_tags(uint64_t tag)
{
	switch (tag) {
	case 0: return "DT_NULL";
	case 1: return "DT_NEEDED";
	case 2: return "DT_PLTRELSZ";
	case 3: return "DT_PLTGOT";
	case 4: return "DT_HASH";
	case 5: return "DT_STRTAB";
	case 6: return "DT_SYMTAB";
	case 7: return "DT_RELA";
	case 8: return "DT_RELASZ";
	case 9: return "DT_RELAENT";
	case 10: return "DT_STRSZ";
	case 11: return "DT_SYMENT";
	case 12: return "DT_INIT";
	case 13: return "DT_FINI";
	case 14: return "DT_SONAME";
	case 15: return "DT_RPATH";
	case 16: return "DT_SYMBOLIC";
	case 17: return "DT_REL";
	case 18: return "DT_RELSZ";
	case 19: return "DT_RELENT";
	case 20: return "DT_PLTREL";
	case 21: return "DT_DEBUG";
	case 22: return "DT_TEXTREL";
	case 23: return "DT_JMPREL";
	case 24: return "DT_BIND_NOW";
	case 25: return "DT_INIT_ARRAY";
	case 26: return "DT_FINI_ARRAY";
	case 27: return "DT_INIT_ARRAYSZ";
	case 28: return "DT_FINI_ARRAYSZ";
	case 29: return "DT_RUNPATH";
	case 30: return "DT_FLAGS";
	case 32: return "DT_PREINIT_ARRAY"; /* XXX: DT_ENCODING */
	case 33: return "DT_PREINIT_ARRAYSZ";
	/* 0x6000000D - 0x6ffff000 operating system-specific semantics */
	case 0x6ffffdf5: return "DT_GNU_PRELINKED";
	case 0x6ffffdf6: return "DT_GNU_CONFLICTSZ";
	case 0x6ffffdf7: return "DT_GNU_LIBLISTSZ";
	case 0x6ffffdf8: return "DT_SUNW_CHECKSUM";
	case 0x6ffffdf9: return "DT_PLTPADSZ";
	case 0x6ffffdfa: return "DT_MOVEENT";
	case 0x6ffffdfb: return "DT_MOVESZ";
	case 0x6ffffdfc: return "DT_FEATURE";
	case 0x6ffffdfd: return "DT_POSFLAG_1";
	case 0x6ffffdfe: return "DT_SYMINSZ";
	case 0x6ffffdff: return "DT_SYMINENT (DT_VALRNGHI)";
	case 0x6ffffe00: return "DT_ADDRRNGLO";
	case 0x6ffffef5: return "DT_GNU_HASH";
	case 0x6ffffef8: return "DT_GNU_CONFLICT";
	case 0x6ffffef9: return "DT_GNU_LIBLIST";
	case 0x6ffffefa: return "DT_SUNW_CONFIG";
	case 0x6ffffefb: return "DT_SUNW_DEPAUDIT";
	case 0x6ffffefc: return "DT_SUNW_AUDIT";
	case 0x6ffffefd: return "DT_SUNW_PLTPAD";
	case 0x6ffffefe: return "DT_SUNW_MOVETAB";
	case 0x6ffffeff: return "DT_SYMINFO (DT_ADDRRNGHI)";
	case 0x6ffffff9: return "DT_RELACOUNT";
	case 0x6ffffffa: return "DT_RELCOUNT";
	case 0x6ffffffb: return "DT_FLAGS_1";
	case 0x6ffffffc: return "DT_VERDEF";
	case 0x6ffffffd: return "DT_VERDEFNUM";
	case 0x6ffffffe: return "DT_VERNEED";
	case 0x6fffffff: return "DT_VERNEEDNUM";
	case 0x6ffffff0: return "DT_GNU_VERSYM";
	/* 0x70000000 - 0x7fffffff processor-specific semantics */
	case 0x70000000: return "DT_IA_64_PLT_RESERVE";
	case 0x7ffffffd: return "DT_SUNW_AUXILIARY";
	case 0x7ffffffe: return "DT_SUNW_USED";
	case 0x7fffffff: return "DT_SUNW_FILTER";
	default: return "ERROR: TAG NOT DEFINED";
	}
}

static const char *
e_machines(unsigned int mach)
{
	static char machdesc[64];

	switch (mach) {
	case EM_NONE:	return "EM_NONE";
	case EM_M32:	return "EM_M32";
	case EM_SPARC:	return "EM_SPARC";
	case EM_386:	return "EM_386";
	case EM_68K:	return "EM_68K";
	case EM_88K:	return "EM_88K";
	case EM_IAMCU:	return "EM_IAMCU";
	case EM_860:	return "EM_860";
	case EM_MIPS:	return "EM_MIPS";
	case EM_PPC:	return "EM_PPC";
	case EM_ARM:	return "EM_ARM";
	case EM_ALPHA:	return "EM_ALPHA (legacy)";
	case EM_SPARCV9:return "EM_SPARCV9";
	case EM_IA_64:	return "EM_IA_64";
	case EM_X86_64:	return "EM_X86_64";
	}
	snprintf(machdesc, sizeof(machdesc),
	    "(unknown machine) -- type 0x%x", mach);
	return (machdesc);
}

static const char *e_types[] = {
	"ET_NONE", "ET_REL", "ET_EXEC", "ET_DYN", "ET_CORE"
};

static const char *ei_versions[] = {
	"EV_NONE", "EV_CURRENT"
};

static const char *ei_classes[] = {
	"ELFCLASSNONE", "ELFCLASS32", "ELFCLASS64"
};

static const char *ei_data[] = {
	"ELFDATANONE", "ELFDATA2LSB", "ELFDATA2MSB"
};

static const char *ei_abis[] = {
	"ELFOSABI_NONE", "ELFOSABI_HPUX", "ELFOSABI_NETBSD", "ELFOSABI_LINUX",
	"ELFOSABI_HURD", "ELFOSABI_86OPEN", "ELFOSABI_SOLARIS",
	"ELFOSABI_MONTEREY", "ELFOSABI_IRIX", "ELFOSABI_FREEBSD",
	"ELFOSABI_TRU64", "ELFOSABI_MODESTO", "ELFOSABI_OPENBSD"
};

static const char *p_types[] = {
	"PT_NULL", "PT_LOAD", "PT_DYNAMIC", "PT_INTERP", "PT_NOTE",
	"PT_SHLIB", "PT_PHDR", "PT_TLS"
};

static const char *p_flags[] = {
	"", "PF_X", "PF_W", "PF_X|PF_W", "PF_R", "PF_X|PF_R", "PF_W|PF_R",
	"PF_X|PF_W|PF_R"
};

static const char *
sh_name(struct elfdump *ed, int ndx)
{
	static char num[10];

	switch (ndx) {
	case SHN_UNDEF: return "UNDEF";
	case SHN_ABS: return "ABS";
	case SHN_COMMON: return "COMMON";
	default:
		if ((uint64_t)ndx < ed->shnum)
			return (ed->sl[ndx].name);
		else {
			snprintf(num, sizeof(num), "%d", ndx);
			return (num);
		}
	}
}

/* http://www.sco.com/developers/gabi/latest/ch4.sheader.html#sh_type */
static const char *
sh_types(u_int64_t sht) {
	switch (sht) {
	case 0:	return "SHT_NULL";
	case 1: return "SHT_PROGBITS";
	case 2: return "SHT_SYMTAB";
	case 3: return "SHT_STRTAB";
	case 4: return "SHT_RELA";
	case 5: return "SHT_HASH";
	case 6: return "SHT_DYNAMIC";
	case 7: return "SHT_NOTE";
	case 8: return "SHT_NOBITS";
	case 9: return "SHT_REL";
	case 10: return "SHT_SHLIB";
	case 11: return "SHT_DYNSYM";
	case 14: return "SHT_INIT_ARRAY";
	case 15: return "SHT_FINI_ARRAY";
	case 16: return "SHT_PREINIT_ARRAY";
	case 17: return "SHT_GROUP";
	case 18: return "SHT_SYMTAB_SHNDX";
	/* 0x60000000 - 0x6fffffff operating system-specific semantics */
	case 0x6ffffff0: return "XXX:VERSYM";
	case 0x6ffffff6: return "SHT_GNU_HASH";
	case 0x6ffffff7: return "SHT_GNU_LIBLIST";
	case 0x6ffffffc: return "XXX:VERDEF";
	case 0x6ffffffd: return "SHT_SUNW(GNU)_verdef";
	case 0x6ffffffe: return "SHT_SUNW(GNU)_verneed";
	case 0x6fffffff: return "SHT_SUNW(GNU)_versym";
	/* 0x70000000 - 0x7fffffff processor-specific semantics */
	case 0x70000000: return "SHT_IA_64_EXT";
	case 0x70000001: return "SHT_IA_64_UNWIND";
	case 0x7ffffffd: return "XXX:AUXILIARY";
	case 0x7fffffff: return "XXX:FILTER";
	/* 0x80000000 - 0xffffffff application programs */
	default: return "ERROR: SHT NOT DEFINED";
	}
}

/*
 * Define known section flags. These flags are defined in the order
 * they are to be printed out.
 */
#define	DEFINE_SHFLAGS()			\
	DEFINE_SHF(WRITE)			\
	DEFINE_SHF(ALLOC)			\
	DEFINE_SHF(EXECINSTR)			\
	DEFINE_SHF(MERGE)			\
	DEFINE_SHF(STRINGS)			\
	DEFINE_SHF(INFO_LINK)			\
	DEFINE_SHF(LINK_ORDER)			\
	DEFINE_SHF(OS_NONCONFORMING)		\
	DEFINE_SHF(GROUP)			\
	DEFINE_SHF(TLS)

#undef	DEFINE_SHF
#define	DEFINE_SHF(F) "SHF_" #F "|"
#define ALLSHFLAGS	DEFINE_SHFLAGS()

static const char *
sh_flags(uint64_t shf)
{
	static char	flg[sizeof(ALLSHFLAGS)+1];

	flg[0] = '\0';

#undef	DEFINE_SHF
#define	DEFINE_SHF(N)				\
	if (shf & SHF_##N)			\
		strcat(flg, "SHF_" #N "|");	\

	DEFINE_SHFLAGS()

	flg[strlen(flg) - 1] = '\0'; /* Remove the trailing "|". */

	return (flg);
}

static const char *st_types[] = {
	"STT_NOTYPE", "STT_OBJECT", "STT_FUNC", "STT_SECTION", "STT_FILE",
	"STT_COMMON", "STT_TLS"
};

static const char *st_types_S[] = {
	"NOTY", "OBJT", "FUNC", "SECT", "FILE"
};

static const char *st_bindings[] = {
	"STB_LOCAL", "STB_GLOBAL", "STB_WEAK"
};

static const char *st_bindings_S[] = {
	"LOCL", "GLOB", "WEAK"
};

static unsigned char st_others[] = {
	'D', 'I', 'H', 'P'
};

static const char *
r_type(unsigned int mach, unsigned int type)
{
	switch(mach) {
	case EM_NONE: return "";
	case EM_386:
	case EM_IAMCU:
		switch(type) {
		case 0: return "R_386_NONE";
		case 1: return "R_386_32";
		case 2: return "R_386_PC32";
		case 3: return "R_386_GOT32";
		case 4: return "R_386_PLT32";
		case 5: return "R_386_COPY";
		case 6: return "R_386_GLOB_DAT";
		case 7: return "R_386_JMP_SLOT";
		case 8: return "R_386_RELATIVE";
		case 9: return "R_386_GOTOFF";
		case 10: return "R_386_GOTPC";
		case 14: return "R_386_TLS_TPOFF";
		case 15: return "R_386_TLS_IE";
		case 16: return "R_386_TLS_GOTIE";
		case 17: return "R_386_TLS_LE";
		case 18: return "R_386_TLS_GD";
		case 19: return "R_386_TLS_LDM";
		case 24: return "R_386_TLS_GD_32";
		case 25: return "R_386_TLS_GD_PUSH";
		case 26: return "R_386_TLS_GD_CALL";
		case 27: return "R_386_TLS_GD_POP";
		case 28: return "R_386_TLS_LDM_32";
		case 29: return "R_386_TLS_LDM_PUSH";
		case 30: return "R_386_TLS_LDM_CALL";
		case 31: return "R_386_TLS_LDM_POP";
		case 32: return "R_386_TLS_LDO_32";
		case 33: return "R_386_TLS_IE_32";
		case 34: return "R_386_TLS_LE_32";
		case 35: return "R_386_TLS_DTPMOD32";
		case 36: return "R_386_TLS_DTPOFF32";
		case 37: return "R_386_TLS_TPOFF32";
		default: return "";
		}
	case EM_ARM:
		switch(type) {
		case 0: return "R_ARM_NONE";
		case 1: return "R_ARM_PC24";
		case 2: return "R_ARM_ABS32";
		case 3: return "R_ARM_REL32";
		case 4: return "R_ARM_PC13";
		case 5: return "R_ARM_ABS16";
		case 6: return "R_ARM_ABS12";
		case 7: return "R_ARM_THM_ABS5";
		case 8: return "R_ARM_ABS8";
		case 9: return "R_ARM_SBREL32";
		case 10: return "R_ARM_THM_PC22";
		case 11: return "R_ARM_THM_PC8";
		case 12: return "R_ARM_AMP_VCALL9";
		case 13: return "R_ARM_SWI24";
		case 14: return "R_ARM_THM_SWI8";
		case 15: return "R_ARM_XPC25";
		case 16: return "R_ARM_THM_XPC22";
		case 20: return "R_ARM_COPY";
		case 21: return "R_ARM_GLOB_DAT";
		case 22: return "R_ARM_JUMP_SLOT";
		case 23: return "R_ARM_RELATIVE";
		case 24: return "R_ARM_GOTOFF";
		case 25: return "R_ARM_GOTPC";
		case 26: return "R_ARM_GOT32";
		case 27: return "R_ARM_PLT32";
		case 100: return "R_ARM_GNU_VTENTRY";
		case 101: return "R_ARM_GNU_VTINHERIT";
		case 250: return "R_ARM_RSBREL32";
		case 251: return "R_ARM_THM_RPC22";
		case 252: return "R_ARM_RREL32";
		case 253: return "R_ARM_RABS32";
		case 254: return "R_ARM_RPC24";
		case 255: return "R_ARM_RBASE";
		default: return "";
		}
	case EM_IA_64:
		switch(type) {
		case 0: return "R_IA_64_NONE";
		case 33: return "R_IA_64_IMM14";
		case 34: return "R_IA_64_IMM22";
		case 35: return "R_IA_64_IMM64";
		case 36: return "R_IA_64_DIR32MSB";
		case 37: return "R_IA_64_DIR32LSB";
		case 38: return "R_IA_64_DIR64MSB";
		case 39: return "R_IA_64_DIR64LSB";
		case 42: return "R_IA_64_GPREL22";
		case 43: return "R_IA_64_GPREL64I";
		case 44: return "R_IA_64_GPREL32MSB";
		case 45: return "R_IA_64_GPREL32LSB";
		case 46: return "R_IA_64_GPREL64MSB";
		case 47: return "R_IA_64_GPREL64LSB";
		case 50: return "R_IA_64_LTOFF22";
		case 51: return "R_IA_64_LTOFF64I";
		case 58: return "R_IA_64_PLTOFF22";
		case 59: return "R_IA_64_PLTOFF64I";
		case 62: return "R_IA_64_PLTOFF64MSB";
		case 63: return "R_IA_64_PLTOFF64LSB";
		case 67: return "R_IA_64_FPTR64I";
		case 68: return "R_IA_64_FPTR32MSB";
		case 69: return "R_IA_64_FPTR32LSB";
		case 70: return "R_IA_64_FPTR64MSB";
		case 71: return "R_IA_64_FPTR64LSB";
		case 72: return "R_IA_64_PCREL60B";
		case 73: return "R_IA_64_PCREL21B";
		case 74: return "R_IA_64_PCREL21M";
		case 75: return "R_IA_64_PCREL21F";
		case 76: return "R_IA_64_PCREL32MSB";
		case 77: return "R_IA_64_PCREL32LSB";
		case 78: return "R_IA_64_PCREL64MSB";
		case 79: return "R_IA_64_PCREL64LSB";
		case 82: return "R_IA_64_LTOFF_FPTR22";
		case 83: return "R_IA_64_LTOFF_FPTR64I";
		case 84: return "R_IA_64_LTOFF_FPTR32MSB";
		case 85: return "R_IA_64_LTOFF_FPTR32LSB";
		case 86: return "R_IA_64_LTOFF_FPTR64MSB";
		case 87: return "R_IA_64_LTOFF_FPTR64LSB";
		case 92: return "R_IA_64_SEGREL32MSB";
		case 93: return "R_IA_64_SEGREL32LSB";
		case 94: return "R_IA_64_SEGREL64MSB";
		case 95: return "R_IA_64_SEGREL64LSB";
		case 100: return "R_IA_64_SECREL32MSB";
		case 101: return "R_IA_64_SECREL32LSB";
		case 102: return "R_IA_64_SECREL64MSB";
		case 103: return "R_IA_64_SECREL64LSB";
		case 108: return "R_IA_64_REL32MSB";
		case 109: return "R_IA_64_REL32LSB";
		case 110: return "R_IA_64_REL64MSB";
		case 111: return "R_IA_64_REL64LSB";
		case 116: return "R_IA_64_LTV32MSB";
		case 117: return "R_IA_64_LTV32LSB";
		case 118: return "R_IA_64_LTV64MSB";
		case 119: return "R_IA_64_LTV64LSB";
		case 121: return "R_IA_64_PCREL21BI";
		case 122: return "R_IA_64_PCREL22";
		case 123: return "R_IA_64_PCREL64I";
		case 128: return "R_IA_64_IPLTMSB";
		case 129: return "R_IA_64_IPLTLSB";
		case 133: return "R_IA_64_SUB";
		case 134: return "R_IA_64_LTOFF22X";
		case 135: return "R_IA_64_LDXMOV";
		case 145: return "R_IA_64_TPREL14";
		case 146: return "R_IA_64_TPREL22";
		case 147: return "R_IA_64_TPREL64I";
		case 150: return "R_IA_64_TPREL64MSB";
		case 151: return "R_IA_64_TPREL64LSB";
		case 154: return "R_IA_64_LTOFF_TPREL22";
		case 166: return "R_IA_64_DTPMOD64MSB";
		case 167: return "R_IA_64_DTPMOD64LSB";
		case 170: return "R_IA_64_LTOFF_DTPMOD22";
		case 177: return "R_IA_64_DTPREL14";
		case 178: return "R_IA_64_DTPREL22";
		case 179: return "R_IA_64_DTPREL64I";
		case 180: return "R_IA_64_DTPREL32MSB";
		case 181: return "R_IA_64_DTPREL32LSB";
		case 182: return "R_IA_64_DTPREL64MSB";
		case 183: return "R_IA_64_DTPREL64LSB";
		case 186: return "R_IA_64_LTOFF_DTPREL22";
		default: return "";
		}
	case EM_MIPS:
		switch(type) {
		case 0: return "R_MIPS_NONE";
		case 1: return "R_MIPS_16";
		case 2: return "R_MIPS_32";
		case 3: return "R_MIPS_REL32";
		case 4: return "R_MIPS_26";
		case 5: return "R_MIPS_HI16";
		case 6: return "R_MIPS_LO16";
		case 7: return "R_MIPS_GPREL16";
		case 8: return "R_MIPS_LITERAL";
		case 9: return "R_MIPS_GOT16";
		case 10: return "R_MIPS_PC16";
		case 11: return "R_MIPS_CALL16";
		case 12: return "R_MIPS_GPREL32";
		case 21: return "R_MIPS_GOTHI16";
		case 22: return "R_MIPS_GOTLO16";
		case 30: return "R_MIPS_CALLHI16";
		case 31: return "R_MIPS_CALLLO16";
		default: return "";
		}
	case EM_PPC:
		switch(type) {
		case 0: return "R_PPC_NONE";
		case 1: return "R_PPC_ADDR32";
		case 2: return "R_PPC_ADDR24";
		case 3: return "R_PPC_ADDR16";
		case 4: return "R_PPC_ADDR16_LO";
		case 5: return "R_PPC_ADDR16_HI";
		case 6: return "R_PPC_ADDR16_HA";
		case 7: return "R_PPC_ADDR14";
		case 8: return "R_PPC_ADDR14_BRTAKEN";
		case 9: return "R_PPC_ADDR14_BRNTAKEN";
		case 10: return "R_PPC_REL24";
		case 11: return "R_PPC_REL14";
		case 12: return "R_PPC_REL14_BRTAKEN";
		case 13: return "R_PPC_REL14_BRNTAKEN";
		case 14: return "R_PPC_GOT16";
		case 15: return "R_PPC_GOT16_LO";
		case 16: return "R_PPC_GOT16_HI";
		case 17: return "R_PPC_GOT16_HA";
		case 18: return "R_PPC_PLTREL24";
		case 19: return "R_PPC_COPY";
		case 20: return "R_PPC_GLOB_DAT";
		case 21: return "R_PPC_JMP_SLOT";
		case 22: return "R_PPC_RELATIVE";
		case 23: return "R_PPC_LOCAL24PC";
		case 24: return "R_PPC_UADDR32";
		case 25: return "R_PPC_UADDR16";
		case 26: return "R_PPC_REL32";
		case 27: return "R_PPC_PLT32";
		case 28: return "R_PPC_PLTREL32";
		case 29: return "R_PPC_PLT16_LO";
		case 30: return "R_PPC_PLT16_HI";
		case 31: return "R_PPC_PLT16_HA";
		case 32: return "R_PPC_SDAREL16";
		case 33: return "R_PPC_SECTOFF";
		case 34: return "R_PPC_SECTOFF_LO";
		case 35: return "R_PPC_SECTOFF_HI";
		case 36: return "R_PPC_SECTOFF_HA";
		case 67: return "R_PPC_TLS";
		case 68: return "R_PPC_DTPMOD32";
		case 69: return "R_PPC_TPREL16";
		case 70: return "R_PPC_TPREL16_LO";
		case 71: return "R_PPC_TPREL16_HI";
		case 72: return "R_PPC_TPREL16_HA";
		case 73: return "R_PPC_TPREL32";
		case 74: return "R_PPC_DTPREL16";
		case 75: return "R_PPC_DTPREL16_LO";
		case 76: return "R_PPC_DTPREL16_HI";
		case 77: return "R_PPC_DTPREL16_HA";
		case 78: return "R_PPC_DTPREL32";
		case 79: return "R_PPC_GOT_TLSGD16";
		case 80: return "R_PPC_GOT_TLSGD16_LO";
		case 81: return "R_PPC_GOT_TLSGD16_HI";
		case 82: return "R_PPC_GOT_TLSGD16_HA";
		case 83: return "R_PPC_GOT_TLSLD16";
		case 84: return "R_PPC_GOT_TLSLD16_LO";
		case 85: return "R_PPC_GOT_TLSLD16_HI";
		case 86: return "R_PPC_GOT_TLSLD16_HA";
		case 87: return "R_PPC_GOT_TPREL16";
		case 88: return "R_PPC_GOT_TPREL16_LO";
		case 89: return "R_PPC_GOT_TPREL16_HI";
		case 90: return "R_PPC_GOT_TPREL16_HA";
		case 101: return "R_PPC_EMB_NADDR32";
		case 102: return "R_PPC_EMB_NADDR16";
		case 103: return "R_PPC_EMB_NADDR16_LO";
		case 104: return "R_PPC_EMB_NADDR16_HI";
		case 105: return "R_PPC_EMB_NADDR16_HA";
		case 106: return "R_PPC_EMB_SDAI16";
		case 107: return "R_PPC_EMB_SDA2I16";
		case 108: return "R_PPC_EMB_SDA2REL";
		case 109: return "R_PPC_EMB_SDA21";
		case 110: return "R_PPC_EMB_MRKREF";
		case 111: return "R_PPC_EMB_RELSEC16";
		case 112: return "R_PPC_EMB_RELST_LO";
		case 113: return "R_PPC_EMB_RELST_HI";
		case 114: return "R_PPC_EMB_RELST_HA";
		case 115: return "R_PPC_EMB_BIT_FLD";
		case 116: return "R_PPC_EMB_RELSDA";
		default: return "";
		}
	case EM_SPARC:
	case EM_SPARCV9:
		switch(type) {
		case 0: return "R_SPARC_NONE";
		case 1: return "R_SPARC_8";
		case 2: return "R_SPARC_16";
		case 3: return "R_SPARC_32";
		case 4: return "R_SPARC_DISP8";
		case 5: return "R_SPARC_DISP16";
		case 6: return "R_SPARC_DISP32";
		case 7: return "R_SPARC_WDISP30";
		case 8: return "R_SPARC_WDISP22";
		case 9: return "R_SPARC_HI22";
		case 10: return "R_SPARC_22";
		case 11: return "R_SPARC_13";
		case 12: return "R_SPARC_LO10";
		case 13: return "R_SPARC_GOT10";
		case 14: return "R_SPARC_GOT13";
		case 15: return "R_SPARC_GOT22";
		case 16: return "R_SPARC_PC10";
		case 17: return "R_SPARC_PC22";
		case 18: return "R_SPARC_WPLT30";
		case 19: return "R_SPARC_COPY";
		case 20: return "R_SPARC_GLOB_DAT";
		case 21: return "R_SPARC_JMP_SLOT";
		case 22: return "R_SPARC_RELATIVE";
		case 23: return "R_SPARC_UA32";
		case 24: return "R_SPARC_PLT32";
		case 25: return "R_SPARC_HIPLT22";
		case 26: return "R_SPARC_LOPLT10";
		case 27: return "R_SPARC_PCPLT32";
		case 28: return "R_SPARC_PCPLT22";
		case 29: return "R_SPARC_PCPLT10";
		case 30: return "R_SPARC_10";
		case 31: return "R_SPARC_11";
		case 32: return "R_SPARC_64";
		case 33: return "R_SPARC_OLO10";
		case 34: return "R_SPARC_HH22";
		case 35: return "R_SPARC_HM10";
		case 36: return "R_SPARC_LM22";
		case 37: return "R_SPARC_PC_HH22";
		case 38: return "R_SPARC_PC_HM10";
		case 39: return "R_SPARC_PC_LM22";
		case 40: return "R_SPARC_WDISP16";
		case 41: return "R_SPARC_WDISP19";
		case 42: return "R_SPARC_GLOB_JMP";
		case 43: return "R_SPARC_7";
		case 44: return "R_SPARC_5";
		case 45: return "R_SPARC_6";
		case 46: return "R_SPARC_DISP64";
		case 47: return "R_SPARC_PLT64";
		case 48: return "R_SPARC_HIX22";
		case 49: return "R_SPARC_LOX10";
		case 50: return "R_SPARC_H44";
		case 51: return "R_SPARC_M44";
		case 52: return "R_SPARC_L44";
		case 53: return "R_SPARC_REGISTER";
		case 54: return "R_SPARC_UA64";
		case 55: return "R_SPARC_UA16";
		case 56: return "R_SPARC_TLS_GD_HI22";
		case 57: return "R_SPARC_TLS_GD_LO10";
		case 58: return "R_SPARC_TLS_GD_ADD";
		case 59: return "R_SPARC_TLS_GD_CALL";
		case 60: return "R_SPARC_TLS_LDM_HI22";
		case 61: return "R_SPARC_TLS_LDM_LO10";
		case 62: return "R_SPARC_TLS_LDM_ADD";
		case 63: return "R_SPARC_TLS_LDM_CALL";
		case 64: return "R_SPARC_TLS_LDO_HIX22";
		case 65: return "R_SPARC_TLS_LDO_LOX10";
		case 66: return "R_SPARC_TLS_LDO_ADD";
		case 67: return "R_SPARC_TLS_IE_HI22";
		case 68: return "R_SPARC_TLS_IE_LO10";
		case 69: return "R_SPARC_TLS_IE_LD";
		case 70: return "R_SPARC_TLS_IE_LDX";
		case 71: return "R_SPARC_TLS_IE_ADD";
		case 72: return "R_SPARC_TLS_LE_HIX22";
		case 73: return "R_SPARC_TLS_LE_LOX10";
		case 74: return "R_SPARC_TLS_DTPMOD32";
		case 75: return "R_SPARC_TLS_DTPMOD64";
		case 76: return "R_SPARC_TLS_DTPOFF32";
		case 77: return "R_SPARC_TLS_DTPOFF64";
		case 78: return "R_SPARC_TLS_TPOFF32";
		case 79: return "R_SPARC_TLS_TPOFF64";
		default: return "";
		}
	case EM_X86_64:
		switch(type) {
		case 0: return "R_X86_64_NONE";
		case 1: return "R_X86_64_64";
		case 2: return "R_X86_64_PC32";
		case 3: return "R_X86_64_GOT32";
		case 4: return "R_X86_64_PLT32";
		case 5: return "R_X86_64_COPY";
		case 6: return "R_X86_64_GLOB_DAT";
		case 7: return "R_X86_64_JMP_SLOT";
		case 8: return "R_X86_64_RELATIVE";
		case 9: return "R_X86_64_GOTPCREL";
		case 10: return "R_X86_64_32";
		case 11: return "R_X86_64_32S";
		case 12: return "R_X86_64_16";
		case 13: return "R_X86_64_PC16";
		case 14: return "R_X86_64_8";
		case 15: return "R_X86_64_PC8";
		case 16: return "R_X86_64_DTPMOD64";
		case 17: return "R_X86_64_DTPOFF64";
		case 18: return "R_X86_64_TPOFF64";
		case 19: return "R_X86_64_TLSGD";
		case 20: return "R_X86_64_TLSLD";
		case 21: return "R_X86_64_DTPOFF32";
		case 22: return "R_X86_64_GOTTPOFF";
		case 23: return "R_X86_64_TPOFF32";
		default: return "";
		}
	default: return "";
	}
}

static void	add_name(struct elfdump *ed, const char *name);
static void	elf_print_object(struct elfdump *ed);
static void	elf_print_elf(struct elfdump *ed);
static void	elf_print_ehdr(struct elfdump *ed);
static void	elf_print_phdr(struct elfdump *ed);
static void	elf_print_shdr(struct elfdump *ed);
static void	elf_print_symtab(struct elfdump *ed, int i);
static void	elf_print_symtabs(struct elfdump *ed);
static void	elf_print_symver(struct elfdump *ed);
static void	elf_print_verdef(struct elfdump *ed, struct section *s);
static void	elf_print_verneed(struct elfdump *ed, struct section *s);
static void	elf_print_interp(struct elfdump *ed);
static void	elf_print_dynamic(struct elfdump *ed);
static void	elf_print_rel_entry(struct elfdump *ed, struct section *s,
    int j, struct rel_entry *r);
static void	elf_print_rela(struct elfdump *ed, struct section *s,
    Elf_Data *data);
static void	elf_print_rel(struct elfdump *ed, struct section *s,
    Elf_Data *data);
static void	elf_print_reloc(struct elfdump *ed);
static void	elf_print_got(struct elfdump *ed);
static void	elf_print_got_section(struct elfdump *ed, struct section *s);
static void	elf_print_note(struct elfdump *ed);
static void	elf_print_svr4_hash(struct elfdump *ed, struct section *s);
static void	elf_print_svr4_hash64(struct elfdump *ed, struct section *s);
static void	elf_print_gnu_hash(struct elfdump *ed, struct section *s);
static void	elf_print_hash(struct elfdump *ed);
static void	elf_print_checksum(struct elfdump *ed);
static void	find_gotrel(struct elfdump *ed, struct section *gs,
    struct rel_entry *got);
static struct spec_name	*find_name(struct elfdump *ed, const char *name);
static const char *get_symbol_name(struct elfdump *ed, int symtab, int i);
static const char *get_string(struct elfdump *ed, int strtab, size_t off);
static void	get_versym(struct elfdump *ed, int i, uint16_t **vs, int *nvs);
static void	load_sections(struct elfdump *ed);
static void	unload_sections(struct elfdump *ed);
static void	usage(void);
#ifdef	USE_LIBARCHIVE_AR
static int	ac_detect_ar(int fd);
static void	ac_print_ar(struct elfdump *ed, int fd);
#else
static void	elf_print_ar(struct elfdump *ed, int fd);
#endif	/* USE_LIBARCHIVE_AR */

static struct option elfdump_longopts[] =
{
	{ "help",	no_argument,	NULL,	'H' },
	{ "version",	no_argument,	NULL,	'V' },
	{ NULL,		0,		NULL,	0   }
};

int
main(int ac, char **av)
{
	struct elfdump		*ed, ed_storage;
	struct spec_name	*sn;
	int			 ch, i;

	ed = &ed_storage;
	memset(ed, 0, sizeof(*ed));
	STAILQ_INIT(&ed->snl);
	ed->out = stdout;
	while ((ch = getopt_long(ac, av, "acdeiGHhknN:prsSvVw:",
		elfdump_longopts, NULL)) != -1)
		switch (ch) {
		case 'a':
			ed->options = ED_ALL;
			break;
		case 'c':
			ed->options |= ED_SHDR;
			break;
		case 'd':
			ed->options |= ED_DYN;
			break;
		case 'e':
			ed->options |= ED_EHDR;
			break;
		case 'i':
			ed->options |= ED_INTERP;
			break;
		case 'G':
			ed->options |= ED_GOT;
			break;
		case 'h':
			ed->options |= ED_HASH;
			break;
		case 'k':
			ed->options |= ED_CHECKSUM;
			break;
		case 'n':
			ed->options |= ED_NOTE;
			break;
		case 'N':
			add_name(ed, optarg);
			break;
		case 'p':
			ed->options |= ED_PHDR;
			break;
		case 'r':
			ed->options |= ED_REL;
			break;
		case 's':
			ed->options |= ED_SYMTAB;
			break;
		case 'S':
			ed->flags |= SOLARIS_FMT;
			break;
		case 'v':
			ed->options |= ED_SYMVER;
			break;
		case 'V':
			(void) printf("%s (%s)\n", ELFTC_GETPROGNAME(),
			    elftc_version());
			exit(EXIT_SUCCESS);
			break;
		case 'w':
			if ((ed->out = fopen(optarg, "w")) == NULL)
				err(EXIT_FAILURE, "%s", optarg);
			break;
		case '?':
		case 'H':
		default:
			usage();
		}

	ac -= optind;
	av += optind;

	if (ed->options == 0)
		ed->options = ED_ALL;
	sn = NULL;
	if (ed->options & ED_SYMTAB &&
	    (STAILQ_EMPTY(&ed->snl) || (sn = find_name(ed, "ARSYM")) != NULL)) {
		ed->flags |= PRINT_ARSYM;
		if (sn != NULL) {
			STAILQ_REMOVE(&ed->snl, sn, spec_name, sn_list);
			if (STAILQ_EMPTY(&ed->snl))
				ed->flags |= ONLY_ARSYM;
		}
	}
	if (ac == 0)
		usage();
	if (ac > 1)
		ed->flags |= PRINT_FILENAME;
	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(EXIT_FAILURE, "ELF library initialization failed: %s",
		    elf_errmsg(-1));

	for (i = 0; i < ac; i++) {
		ed->filename = av[i];
		ed->archive = NULL;
		elf_print_object(ed);
	}

	exit(EXIT_SUCCESS);
}

#ifdef USE_LIBARCHIVE_AR

/* Archive symbol table entry. */
struct arsym_entry {
	char *sym_name;
	size_t off;
};

/*
 * Convenient wrapper for general libarchive error handling.
 */
#define	AC(CALL) do {							\
	if ((CALL)) {							\
		warnx("%s", archive_error_string(a));			\
		return;							\
	}								\
} while (0)

/*
 * Detect an ar(1) archive using libarchive(3).
 */
static int
ac_detect_ar(int fd)
{
	struct archive		*a;
	struct archive_entry	*entry;
	int			 r;

	r = -1;
	if ((a = archive_read_new()) == NULL)
		return (0);
	archive_read_support_format_ar(a);
	if (archive_read_open_fd(a, fd, 10240) == ARCHIVE_OK)
		r = archive_read_next_header(a, &entry);
	archive_read_close(a);
	archive_read_free(a);

	return (r == ARCHIVE_OK);
}

/*
 * Dump an ar(1) archive using libarchive(3).
 */
static void
ac_print_ar(struct elfdump *ed, int fd)
{
	struct archive		*a;
	struct archive_entry	*entry;
	struct arsym_entry	*arsym;
	const char		*name;
	char			 idx[10], *b;
	void			*buff;
	size_t			 size;
	uint32_t		 cnt;
	int			 i, r;

	if (lseek(fd, 0, SEEK_SET) == -1)
		err(EXIT_FAILURE, "lseek failed");
	if ((a = archive_read_new()) == NULL)
		errx(EXIT_FAILURE, "%s", archive_error_string(a));
	archive_read_support_format_ar(a);
	AC(archive_read_open_fd(a, fd, 10240));
	for(;;) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_FATAL)
			errx(EXIT_FAILURE, "%s", archive_error_string(a));
		if (r == ARCHIVE_EOF)
			break;
		if (r == ARCHIVE_WARN || r == ARCHIVE_RETRY)
			warnx("%s", archive_error_string(a));
		if (r == ARCHIVE_RETRY)
			continue;
		name = archive_entry_pathname(entry);
		size = archive_entry_size(entry);
		if (size == 0)
			continue;
		if ((buff = malloc(size)) == NULL) {
			warn("malloc failed");
			continue;
		}
		if (archive_read_data(a, buff, size) != (ssize_t)size) {
			warnx("%s", archive_error_string(a));
			free(buff);
			continue;
		}

		/*
		 * Note that when processing arsym via libarchive, there is
		 * no way to tell which member a certain symbol belongs to,
		 * since we can not just "lseek" to a member offset and read
		 * the member header.
		 */
		if (!strcmp(name, "/") && ed->flags & PRINT_ARSYM) {
			b = buff;
			cnt = be32dec(b);
			if (cnt == 0) {
				free(buff);
				continue;
			}
			arsym = calloc(cnt, sizeof(*arsym));
			if (arsym == NULL)
				err(EXIT_FAILURE, "calloc failed");
			b += sizeof(uint32_t);
			for (i = 0; (size_t)i < cnt; i++) {
				arsym[i].off = be32dec(b);
				b += sizeof(uint32_t);
			}
			for (i = 0; (size_t)i < cnt; i++) {
				arsym[i].sym_name = b;
				b += strlen(b) + 1;
			}
			if (ed->flags & SOLARIS_FMT) {
				PRT("\nSymbol Table: (archive)\n");
				PRT("     index    offset    symbol\n");
			} else
				PRT("\nsymbol table (archive):\n");
			for (i = 0; (size_t)i < cnt; i++) {
				if (ed->flags & SOLARIS_FMT) {
					snprintf(idx, sizeof(idx), "[%d]", i);
					PRT("%10s  ", idx);
					PRT("0x%8.8jx  ",
					    (uintmax_t)arsym[i].off);
					PRT("%s\n", arsym[i].sym_name);
				} else {
					PRT("\nentry: %d\n", i);
					PRT("\toffset: %#jx\n",
					    (uintmax_t)arsym[i].off);
					PRT("\tsymbol: %s\n",
					    arsym[i].sym_name);
				}
			}
			free(arsym);
			free(buff);
			/* No need to continue if we only dump ARSYM. */
			if (ed->flags & ONLY_ARSYM) {
				AC(archive_read_close(a));
				AC(archive_read_free(a));
				return;
			}
			continue;
		}
		if ((ed->elf = elf_memory(buff, size)) == NULL) {
			warnx("elf_memroy() failed: %s",
			      elf_errmsg(-1));
			free(buff);
			continue;
		}
		/* Skip non-ELF member. */
		if (elf_kind(ed->elf) == ELF_K_ELF) {
			printf("\n%s(%s):\n", ed->archive, name);
			elf_print_elf(ed);
		}
		elf_end(ed->elf);
		free(buff);
	}
	AC(archive_read_close(a));
	AC(archive_read_free(a));
}

#else  /* USE_LIBARCHIVE_AR */

/*
 * Dump an ar(1) archive.
 */
static void
elf_print_ar(struct elfdump *ed, int fd)
{
	Elf		*e;
	Elf_Arhdr	*arh;
	Elf_Arsym	*arsym;
	Elf_Cmd		 cmd;
	char		 idx[10];
	size_t		 cnt;
	int		 i;

	ed->ar = ed->elf;

	if (ed->flags & PRINT_ARSYM) {
		cnt = 0;
		if ((arsym = elf_getarsym(ed->ar, &cnt)) == NULL) {
			warnx("elf_getarsym failed: %s", elf_errmsg(-1));
			goto print_members;
		}
		if (cnt == 0)
			goto print_members;
		if (ed->flags & SOLARIS_FMT) {
			PRT("\nSymbol Table: (archive)\n");
			PRT("     index    offset    member name and symbol\n");
		} else
			PRT("\nsymbol table (archive):\n");
		for (i = 0; (size_t)i < cnt - 1; i++) {
			if (elf_rand(ed->ar, arsym[i].as_off) !=
			    arsym[i].as_off) {
				warnx("elf_rand failed: %s", elf_errmsg(-1));
				break;
			}
			if ((e = elf_begin(fd, ELF_C_READ, ed->ar)) == NULL) {
				warnx("elf_begin failed: %s", elf_errmsg(-1));
				break;
			}
			if ((arh = elf_getarhdr(e)) == NULL) {
				warnx("elf_getarhdr failed: %s",
				    elf_errmsg(-1));
				break;
			}
			if (ed->flags & SOLARIS_FMT) {
				snprintf(idx, sizeof(idx), "[%d]", i);
				PRT("%10s  ", idx);
				PRT("0x%8.8jx  ",
				    (uintmax_t)arsym[i].as_off);
				PRT("(%s):%s\n", arh->ar_name,
				    arsym[i].as_name);
			} else {
				PRT("\nentry: %d\n", i);
				PRT("\toffset: %#jx\n",
				    (uintmax_t)arsym[i].as_off);
				PRT("\tmember: %s\n", arh->ar_name);
				PRT("\tsymbol: %s\n", arsym[i].as_name);
			}
			elf_end(e);
		}

		/* No need to continue if we only dump ARSYM. */
		if (ed->flags & ONLY_ARSYM)
			return;
	}

print_members:

	/* Rewind the archive. */
	if (elf_rand(ed->ar, SARMAG) != SARMAG) {
		warnx("elf_rand failed: %s", elf_errmsg(-1));
		return;
	}

	/* Dump each member of the archive. */
	cmd = ELF_C_READ;
	while ((ed->elf = elf_begin(fd, cmd, ed->ar)) != NULL) {
		/* Skip non-ELF member. */
		if (elf_kind(ed->elf) == ELF_K_ELF) {
			if ((arh = elf_getarhdr(ed->elf)) == NULL) {
				warnx("elf_getarhdr failed: %s",
				    elf_errmsg(-1));
				break;
			}
			printf("\n%s(%s):\n", ed->archive, arh->ar_name);
			elf_print_elf(ed);
		}
		cmd = elf_next(ed->elf);
		elf_end(ed->elf);
	}
}

#endif	/* USE_LIBARCHIVE_AR */

/*
 * Dump an object. (ELF object or ar(1) archive)
 */
static void
elf_print_object(struct elfdump *ed)
{
	int fd;

	if ((fd = open(ed->filename, O_RDONLY)) == -1) {
		warn("open %s failed", ed->filename);
		return;
	}

#ifdef	USE_LIBARCHIVE_AR
	if (ac_detect_ar(fd)) {
		ed->archive = ed->filename;
		ac_print_ar(ed, fd);
		return;
	}
#endif	/* USE_LIBARCHIVE_AR */

	if ((ed->elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		warnx("elf_begin() failed: %s", elf_errmsg(-1));
		return;
	}

	switch (elf_kind(ed->elf)) {
	case ELF_K_NONE:
		warnx("Not an ELF file.");
		return;
	case ELF_K_ELF:
		if (ed->flags & PRINT_FILENAME)
			printf("\n%s:\n", ed->filename);
		elf_print_elf(ed);
		break;
	case ELF_K_AR:
#ifndef	USE_LIBARCHIVE_AR
		ed->archive = ed->filename;
		elf_print_ar(ed, fd);
#endif
		break;
	default:
		warnx("Internal: libelf returned unknown elf kind.");
		return;
	}

	elf_end(ed->elf);
}

/*
 * Dump an ELF object.
 */
static void
elf_print_elf(struct elfdump *ed)
{

	if (gelf_getehdr(ed->elf, &ed->ehdr) == NULL) {
		warnx("gelf_getehdr failed: %s", elf_errmsg(-1));
		return;
	}
	if ((ed->ec = gelf_getclass(ed->elf)) == ELFCLASSNONE) {
		warnx("gelf_getclass failed: %s", elf_errmsg(-1));
		return;
	}

	if (ed->options & (ED_SHDR | ED_DYN | ED_REL | ED_GOT | ED_SYMTAB |
	    ED_SYMVER | ED_NOTE | ED_HASH))
		load_sections(ed);

	if (ed->options & ED_EHDR)
		elf_print_ehdr(ed);
	if (ed->options & ED_PHDR)
		elf_print_phdr(ed);
	if (ed->options & ED_INTERP)
		elf_print_interp(ed);
	if (ed->options & ED_SHDR)
		elf_print_shdr(ed);
	if (ed->options & ED_DYN)
		elf_print_dynamic(ed);
	if (ed->options & ED_REL)
		elf_print_reloc(ed);
	if (ed->options & ED_GOT)
		elf_print_got(ed);
	if (ed->options & ED_SYMTAB)
		elf_print_symtabs(ed);
	if (ed->options & ED_SYMVER)
		elf_print_symver(ed);
	if (ed->options & ED_NOTE)
		elf_print_note(ed);
	if (ed->options & ED_HASH)
		elf_print_hash(ed);
	if (ed->options & ED_CHECKSUM)
		elf_print_checksum(ed);

	unload_sections(ed);
}

/*
 * Read the section headers from ELF object and store them in the
 * internal cache.
 */
static void
load_sections(struct elfdump *ed)
{
	struct section	*s;
	const char	*name;
	Elf_Scn		*scn;
	GElf_Shdr	 sh;
	size_t		 shstrndx, ndx;
	int		 elferr;

	assert(ed->sl == NULL);

	if (!elf_getshnum(ed->elf, &ed->shnum)) {
		warnx("elf_getshnum failed: %s", elf_errmsg(-1));
		return;
	}
	if (ed->shnum == 0)
		return;
	if ((ed->sl = calloc(ed->shnum, sizeof(*ed->sl))) == NULL)
		err(EXIT_FAILURE, "calloc failed");
	if (!elf_getshstrndx(ed->elf, &shstrndx)) {
		warnx("elf_getshstrndx failed: %s", elf_errmsg(-1));
		return;
	}
	if ((scn = elf_getscn(ed->elf, 0)) == NULL) {
		warnx("elf_getscn failed: %s", elf_errmsg(-1));
		return;
	}
	(void) elf_errno();
	do {
		if (gelf_getshdr(scn, &sh) == NULL) {
			warnx("gelf_getshdr failed: %s", elf_errmsg(-1));
			(void) elf_errno();
			continue;
		}
		if ((name = elf_strptr(ed->elf, shstrndx, sh.sh_name)) == NULL) {
			(void) elf_errno();
			name = "ERROR";
		}
		if ((ndx = elf_ndxscn(scn)) == SHN_UNDEF)
			if ((elferr = elf_errno()) != 0) {
				warnx("elf_ndxscn failed: %s",
				    elf_errmsg(elferr));
				continue;
			}
		if (ndx >= ed->shnum) {
			warnx("section index of '%s' out of range", name);
			continue;
		}
		s = &ed->sl[ndx];
		s->name = name;
		s->scn = scn;
		s->off = sh.sh_offset;
		s->sz = sh.sh_size;
		s->entsize = sh.sh_entsize;
		s->align = sh.sh_addralign;
		s->type = sh.sh_type;
		s->flags = sh.sh_flags;
		s->addr = sh.sh_addr;
		s->link = sh.sh_link;
		s->info = sh.sh_info;
	} while ((scn = elf_nextscn(ed->elf, scn)) != NULL);
	elferr = elf_errno();
	if (elferr != 0)
		warnx("elf_nextscn failed: %s", elf_errmsg(elferr));
}

/*
 * Release section related resources.
 */
static void
unload_sections(struct elfdump *ed)
{
	if (ed->sl != NULL) {
		free(ed->sl);
		ed->sl = NULL;
	}
}

/*
 * Add a name to the '-N' name list.
 */
static void
add_name(struct elfdump *ed, const char *name)
{
	struct spec_name *sn;

	if (find_name(ed, name))
		return;
	if ((sn = malloc(sizeof(*sn))) == NULL) {
		warn("malloc failed");
		return;
	}
	sn->name = name;
	STAILQ_INSERT_TAIL(&ed->snl, sn, sn_list);
}

/*
 * Lookup a name in the '-N' name list.
 */
static struct spec_name *
find_name(struct elfdump *ed, const char *name)
{
	struct spec_name *sn;

	STAILQ_FOREACH(sn, &ed->snl, sn_list) {
		if (!strcmp(sn->name, name))
			return (sn);
	}

	return (NULL);
}

/*
 * Retrieve the name of a symbol using the section index of the symbol
 * table and the index of the symbol within that table.
 */
static const char *
get_symbol_name(struct elfdump *ed, int symtab, int i)
{
	static char	 sname[64];
	struct section	*s;
	const char	*name;
	GElf_Sym	 sym;
	Elf_Data	*data;
	int		 elferr;

	s = &ed->sl[symtab];
	if (s->type != SHT_SYMTAB && s->type != SHT_DYNSYM)
		return ("");
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s", elf_errmsg(elferr));
		return ("");
	}
	if (gelf_getsym(data, i, &sym) != &sym)
		return ("");
	if (GELF_ST_TYPE(sym.st_info) == STT_SECTION) {
		if (sym.st_shndx < ed->shnum) {
			snprintf(sname, sizeof(sname), "%s (section)",
			    ed->sl[sym.st_shndx].name);
			return (sname);
		} else
			return ("");
	}
	if ((name = elf_strptr(ed->elf, s->link, sym.st_name)) == NULL)
		return ("");

	return (name);
}

/*
 * Retrieve a string using string table section index and the string offset.
 */
static const char*
get_string(struct elfdump *ed, int strtab, size_t off)
{
	const char *name;

	if ((name = elf_strptr(ed->elf, strtab, off)) == NULL)
		return ("");

	return (name);
}

/*
 * Dump the ELF Executable Header.
 */
static void
elf_print_ehdr(struct elfdump *ed)
{

	if (!STAILQ_EMPTY(&ed->snl))
		return;

	if (ed->flags & SOLARIS_FMT) {
		PRT("\nELF Header\n");
		PRT("  ei_magic:   { %#x, %c, %c, %c }\n",
		    ed->ehdr.e_ident[0], ed->ehdr.e_ident[1],
		    ed->ehdr.e_ident[2], ed->ehdr.e_ident[3]);
		PRT("  ei_class:   %-18s",
		    ei_classes[ed->ehdr.e_ident[EI_CLASS]]);
		PRT("  ei_data:      %s\n", ei_data[ed->ehdr.e_ident[EI_DATA]]);
		PRT("  e_machine:  %-18s", e_machines(ed->ehdr.e_machine));
		PRT("  e_version:    %s\n", ei_versions[ed->ehdr.e_version]);
		PRT("  e_type:     %s\n", e_types[ed->ehdr.e_type]);
		PRT("  e_flags:    %18d\n", ed->ehdr.e_flags);
		PRT("  e_entry:    %#18jx", (uintmax_t)ed->ehdr.e_entry);
		PRT("  e_ehsize: %6d", ed->ehdr.e_ehsize);
		PRT("  e_shstrndx:%5d\n", ed->ehdr.e_shstrndx);
		PRT("  e_shoff:    %#18jx", (uintmax_t)ed->ehdr.e_shoff);
		PRT("  e_shentsize: %3d", ed->ehdr.e_shentsize);
		PRT("  e_shnum:   %5d\n", ed->ehdr.e_shnum);
		PRT("  e_phoff:    %#18jx", (uintmax_t)ed->ehdr.e_phoff);
		PRT("  e_phentsize: %3d", ed->ehdr.e_phentsize);
		PRT("  e_phnum:   %5d\n", ed->ehdr.e_phnum);
	} else {
		PRT("\nelf header:\n");
		PRT("\n");
		PRT("\te_ident: %s %s %s\n",
		    ei_classes[ed->ehdr.e_ident[EI_CLASS]],
		    ei_data[ed->ehdr.e_ident[EI_DATA]],
		    ei_abis[ed->ehdr.e_ident[EI_OSABI]]);
		PRT("\te_type: %s\n", e_types[ed->ehdr.e_type]);
		PRT("\te_machine: %s\n", e_machines(ed->ehdr.e_machine));
		PRT("\te_version: %s\n", ei_versions[ed->ehdr.e_version]);
		PRT("\te_entry: %#jx\n", (uintmax_t)ed->ehdr.e_entry);
		PRT("\te_phoff: %ju\n", (uintmax_t)ed->ehdr.e_phoff);
		PRT("\te_shoff: %ju\n", (uintmax_t) ed->ehdr.e_shoff);
		PRT("\te_flags: %u\n", ed->ehdr.e_flags);
		PRT("\te_ehsize: %u\n", ed->ehdr.e_ehsize);
		PRT("\te_phentsize: %u\n", ed->ehdr.e_phentsize);
		PRT("\te_phnum: %u\n", ed->ehdr.e_phnum);
		PRT("\te_shentsize: %u\n", ed->ehdr.e_shentsize);
		PRT("\te_shnum: %u\n", ed->ehdr.e_shnum);
		PRT("\te_shstrndx: %u\n", ed->ehdr.e_shstrndx);
	}
}

/*
 * Dump the ELF Program Header Table.
 */
static void
elf_print_phdr(struct elfdump *ed)
{
	GElf_Phdr	 ph;
	size_t		 phnum;
	int		 header, i;

	if (elf_getphnum(ed->elf, &phnum) == 0) {
		warnx("elf_getphnum failed: %s", elf_errmsg(-1));
		return;
	}
	header = 0;
	for (i = 0; (u_int64_t) i < phnum; i++) {
		if (gelf_getphdr(ed->elf, i, &ph) != &ph) {
			warnx("elf_getphdr failed: %s", elf_errmsg(-1));
			continue;
		}
		if (!STAILQ_EMPTY(&ed->snl) &&
		    find_name(ed, p_types[ph.p_type & 0x7]) == NULL)
			continue;
		if (ed->flags & SOLARIS_FMT) {
			PRT("\nProgram Header[%d]:\n", i);
			PRT("    p_vaddr:      %#-14jx", (uintmax_t)ph.p_vaddr);
			PRT("  p_flags:    [ %s ]\n", p_flags[ph.p_flags]);
			PRT("    p_paddr:      %#-14jx", (uintmax_t)ph.p_paddr);
			PRT("  p_type:     [ %s ]\n", p_types[ph.p_type & 0x7]);
			PRT("    p_filesz:     %#-14jx",
			    (uintmax_t)ph.p_filesz);
			PRT("  p_memsz:    %#jx\n", (uintmax_t)ph.p_memsz);
			PRT("    p_offset:     %#-14jx",
			    (uintmax_t)ph.p_offset);
			PRT("  p_align:    %#jx\n", (uintmax_t)ph.p_align);
		} else {
			if (!header) {
				PRT("\nprogram header:\n");
				header = 1;
			}
			PRT("\n");
			PRT("entry: %d\n", i);
			PRT("\tp_type: %s\n", p_types[ph.p_type & 0x7]);
			PRT("\tp_offset: %ju\n", (uintmax_t)ph.p_offset);
			PRT("\tp_vaddr: %#jx\n", (uintmax_t)ph.p_vaddr);
			PRT("\tp_paddr: %#jx\n", (uintmax_t)ph.p_paddr);
			PRT("\tp_filesz: %ju\n", (uintmax_t)ph.p_filesz);
			PRT("\tp_memsz: %ju\n", (uintmax_t)ph.p_memsz);
			PRT("\tp_flags: %s\n", p_flags[ph.p_flags]);
			PRT("\tp_align: %ju\n", (uintmax_t)ph.p_align);
		}
	}
}

/*
 * Dump the ELF Section Header Table.
 */
static void
elf_print_shdr(struct elfdump *ed)
{
	struct section *s;
	int i;

	if (!STAILQ_EMPTY(&ed->snl))
		return;

	if ((ed->flags & SOLARIS_FMT) == 0)
		PRT("\nsection header:\n");
	for (i = 0; (size_t)i < ed->shnum; i++) {
		s = &ed->sl[i];
		if (ed->flags & SOLARIS_FMT) {
			if (i == 0)
				continue;
			PRT("\nSection Header[%d]:", i);
			PRT("  sh_name: %s\n", s->name);
			PRT("    sh_addr:      %#-14jx", (uintmax_t)s->addr);
			if (s->flags != 0)
				PRT("  sh_flags:   [ %s ]\n", sh_flags(s->flags));
			else
				PRT("  sh_flags:   0\n");
			PRT("    sh_size:      %#-14jx", (uintmax_t)s->sz);
			PRT("  sh_type:    [ %s ]\n", sh_types(s->type));
			PRT("    sh_offset:    %#-14jx", (uintmax_t)s->off);
			PRT("  sh_entsize: %#jx\n", (uintmax_t)s->entsize);
			PRT("    sh_link:      %-14u", s->link);
			PRT("  sh_info:    %u\n", s->info);
			PRT("    sh_addralign: %#jx\n", (uintmax_t)s->align);
		} else {
			PRT("\n");
			PRT("entry: %ju\n", (uintmax_t)i);
			PRT("\tsh_name: %s\n", s->name);
			PRT("\tsh_type: %s\n", sh_types(s->type));
			PRT("\tsh_flags: %s\n", sh_flags(s->flags));
			PRT("\tsh_addr: %#jx\n", (uintmax_t)s->addr);
			PRT("\tsh_offset: %ju\n", (uintmax_t)s->off);
			PRT("\tsh_size: %ju\n", (uintmax_t)s->sz);
			PRT("\tsh_link: %u\n", s->link);
			PRT("\tsh_info: %u\n", s->info);
			PRT("\tsh_addralign: %ju\n", (uintmax_t)s->align);
			PRT("\tsh_entsize: %ju\n", (uintmax_t)s->entsize);
		}
	}
}

/*
 * Retrieve the content of the corresponding SHT_SUNW_versym section for
 * a symbol table section.
 */
static void
get_versym(struct elfdump *ed, int i, uint16_t **vs, int *nvs)
{
	struct section	*s;
	Elf_Data	*data;
	int		 j, elferr;

	s = NULL;
	for (j = 0; (size_t)j < ed->shnum; j++) {
		s = &ed->sl[j];
		if (s->type == SHT_SUNW_versym && s->link == (uint32_t)i)
			break;
	}
	if ((size_t)j >= ed->shnum) {
		*vs = NULL;
		return;
	}
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s", elf_errmsg(elferr));
		*vs = NULL;
		return;
	}

	*vs = data->d_buf;
	*nvs = data->d_size / s->entsize;
}

/*
 * Dump the symbol table section.
 */
static void
elf_print_symtab(struct elfdump *ed, int i)
{
	struct section	*s;
	const char	*name;
	uint16_t	*vs;
	char		 idx[10];
	Elf_Data	*data;
	GElf_Sym	 sym;
	int		 len, j, elferr, nvs;

	s = &ed->sl[i];
	if (ed->flags & SOLARIS_FMT)
		PRT("\nSymbol Table Section:  %s\n", s->name);
	else
		PRT("\nsymbol table (%s):\n", s->name);
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s", elf_errmsg(elferr));
		return;
	}
	vs = NULL;
	nvs = 0;
	len = data->d_size / s->entsize;
	if (ed->flags & SOLARIS_FMT) {
		if (ed->ec == ELFCLASS32)
			PRT("     index    value       ");
		else
			PRT("     index        value           ");
		PRT("size     type bind oth ver shndx       name\n");
		get_versym(ed, i, &vs, &nvs);
		if (vs != NULL && nvs != len) {
			warnx("#symbol not equal to #versym");
			vs = NULL;
		}
	}
	for (j = 0; j < len; j++) {
		if (gelf_getsym(data, j, &sym) != &sym) {
			warnx("gelf_getsym failed: %s", elf_errmsg(-1));
			continue;
		}
		name = get_string(ed, s->link, sym.st_name);
		if (ed->flags & SOLARIS_FMT) {
			snprintf(idx, sizeof(idx), "[%d]", j);
			if (ed->ec == ELFCLASS32)
				PRT("%10s  ", idx);
			else
				PRT("%10s      ", idx);
			PRT("0x%8.8jx ", (uintmax_t)sym.st_value);
			if (ed->ec == ELFCLASS32)
				PRT("0x%8.8jx  ", (uintmax_t)sym.st_size);
			else
				PRT("0x%12.12jx  ", (uintmax_t)sym.st_size);
			PRT("%s ", st_types_S[GELF_ST_TYPE(sym.st_info)]);
			PRT("%s  ", st_bindings_S[GELF_ST_BIND(sym.st_info)]);
			PRT("%c  ", st_others[sym.st_other]);
			PRT("%3u ", (vs == NULL ? 0 : vs[j]));
			PRT("%-11.11s ", sh_name(ed, sym.st_shndx));
			PRT("%s\n", name);
		} else {
			PRT("\nentry: %d\n", j);
			PRT("\tst_name: %s\n", name);
			PRT("\tst_value: %#jx\n", (uintmax_t)sym.st_value);
			PRT("\tst_size: %ju\n", (uintmax_t)sym.st_size);
			PRT("\tst_info: %s %s\n",
			    st_types[GELF_ST_TYPE(sym.st_info)],
			    st_bindings[GELF_ST_BIND(sym.st_info)]);
			PRT("\tst_shndx: %ju\n", (uintmax_t)sym.st_shndx);
		}
	}
}

/*
 * Dump the symbol tables. (.dynsym and .symtab)
 */
static void
elf_print_symtabs(struct elfdump *ed)
{
	int i;

	for (i = 0; (size_t)i < ed->shnum; i++)
		if ((ed->sl[i].type == SHT_SYMTAB ||
		    ed->sl[i].type == SHT_DYNSYM) &&
		    (STAILQ_EMPTY(&ed->snl) || find_name(ed, ed->sl[i].name)))
			elf_print_symtab(ed, i);
}

/*
 * Dump the content of .dynamic section.
 */
static void
elf_print_dynamic(struct elfdump *ed)
{
	struct section	*s;
	const char	*name;
	char		 idx[10];
	Elf_Data	*data;
	GElf_Dyn	 dyn;
	int		 elferr, i, len;

	s = NULL;
	for (i = 0; (size_t)i < ed->shnum; i++) {
		s = &ed->sl[i];
		if (s->type == SHT_DYNAMIC &&
		    (STAILQ_EMPTY(&ed->snl) || find_name(ed, s->name)))
			break;
	}
	if ((size_t)i >= ed->shnum)
		return;

	if (ed->flags & SOLARIS_FMT) {
		PRT("Dynamic Section:  %s\n", s->name);
		PRT("     index  tag               value\n");
	} else
		PRT("\ndynamic:\n");
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s", elf_errmsg(elferr));
		return;
	}
	len = data->d_size / s->entsize;
	for (i = 0; i < len; i++) {
		if (gelf_getdyn(data, i, &dyn) != &dyn) {
			warnx("gelf_getdyn failed: %s", elf_errmsg(-1));
			continue;
		}

		if (ed->flags & SOLARIS_FMT) {
			snprintf(idx, sizeof(idx), "[%d]", i);
			PRT("%10s  %-16s ", idx, d_tags(dyn.d_tag));
		} else {
			PRT("\n");
			PRT("entry: %d\n", i);
			PRT("\td_tag: %s\n", d_tags(dyn.d_tag));
		}
		switch(dyn.d_tag) {
		case DT_NEEDED:
		case DT_SONAME:
		case DT_RPATH:
			if ((name = elf_strptr(ed->elf, s->link,
				    dyn.d_un.d_val)) == NULL)
				name = "";
			if (ed->flags & SOLARIS_FMT)
				PRT("%#-16jx %s\n", (uintmax_t)dyn.d_un.d_val,
				    name);
			else
				PRT("\td_val: %s\n", name);
			break;
		case DT_PLTRELSZ:
		case DT_RELA:
		case DT_RELASZ:
		case DT_RELAENT:
		case DT_RELACOUNT:
		case DT_STRSZ:
		case DT_SYMENT:
		case DT_RELSZ:
		case DT_RELENT:
		case DT_PLTREL:
		case DT_VERDEF:
		case DT_VERDEFNUM:
		case DT_VERNEED:
		case DT_VERNEEDNUM:
		case DT_VERSYM:
			if (ed->flags & SOLARIS_FMT)
				PRT("%#jx\n", (uintmax_t)dyn.d_un.d_val);
			else
				PRT("\td_val: %ju\n",
				    (uintmax_t)dyn.d_un.d_val);
			break;
		case DT_PLTGOT:
		case DT_HASH:
		case DT_GNU_HASH:
		case DT_STRTAB:
		case DT_SYMTAB:
		case DT_INIT:
		case DT_FINI:
		case DT_REL:
		case DT_JMPREL:
		case DT_DEBUG:
			if (ed->flags & SOLARIS_FMT)
				PRT("%#jx\n", (uintmax_t)dyn.d_un.d_ptr);
			else
				PRT("\td_ptr: %#jx\n",
				    (uintmax_t)dyn.d_un.d_ptr);
			break;
		case DT_NULL:
		case DT_SYMBOLIC:
		case DT_TEXTREL:
		default:
			if (ed->flags & SOLARIS_FMT)
				PRT("\n");
			break;
		}
	}
}

/*
 * Dump a .rel/.rela section entry.
 */
static void
elf_print_rel_entry(struct elfdump *ed, struct section *s, int j,
    struct rel_entry *r)
{

	if (ed->flags & SOLARIS_FMT) {
		PRT("        %-23s ", r_type(ed->ehdr.e_machine,
			GELF_R_TYPE(r->u_r.rel.r_info)));
		PRT("%#12jx ", (uintmax_t)r->u_r.rel.r_offset);
		if (r->type == SHT_RELA)
			PRT("%10jd  ", (intmax_t)r->u_r.rela.r_addend);
		else
			PRT("    ");
		PRT("%-14s ", s->name);
		PRT("%s\n", r->symn);
	} else {
		PRT("\n");
		PRT("entry: %d\n", j);
		PRT("\tr_offset: %#jx\n", (uintmax_t)r->u_r.rel.r_offset);
		if (ed->ec == ELFCLASS32)
			PRT("\tr_info: %#jx\n", (uintmax_t)
			    ELF32_R_INFO(ELF64_R_SYM(r->u_r.rel.r_info),
			    ELF64_R_TYPE(r->u_r.rel.r_info)));
		else
			PRT("\tr_info: %#jx\n", (uintmax_t)r->u_r.rel.r_info);
		if (r->type == SHT_RELA)
			PRT("\tr_addend: %jd\n",
			    (intmax_t)r->u_r.rela.r_addend);
	}
}

/*
 * Dump a relocation section of type SHT_RELA.
 */
static void
elf_print_rela(struct elfdump *ed, struct section *s, Elf_Data *data)
{
	struct rel_entry	r;
	int			j, len;

	if (ed->flags & SOLARIS_FMT) {
		PRT("\nRelocation Section:  %s\n", s->name);
		PRT("        type                          offset     "
		    "addend  section        with respect to\n");
	} else
		PRT("\nrelocation with addend (%s):\n", s->name);
	r.type = SHT_RELA;
	len = data->d_size / s->entsize;
	for (j = 0; j < len; j++) {
		if (gelf_getrela(data, j, &r.u_r.rela) != &r.u_r.rela) {
			warnx("gelf_getrela failed: %s",
			    elf_errmsg(-1));
			continue;
		}
		r.symn = get_symbol_name(ed, s->link,
		    GELF_R_SYM(r.u_r.rela.r_info));
		elf_print_rel_entry(ed, s, j, &r);
	}
}

/*
 * Dump a relocation section of type SHT_REL.
 */
static void
elf_print_rel(struct elfdump *ed, struct section *s, Elf_Data *data)
{
	struct rel_entry	r;
	int			j, len;

	if (ed->flags & SOLARIS_FMT) {
		PRT("\nRelocation Section:  %s\n", s->name);
		PRT("        type                          offset     "
		    "section        with respect to\n");
	} else
		PRT("\nrelocation (%s):\n", s->name);
	r.type = SHT_REL;
	len = data->d_size / s->entsize;
	for (j = 0; j < len; j++) {
		if (gelf_getrel(data, j, &r.u_r.rel) != &r.u_r.rel) {
			warnx("gelf_getrel failed: %s", elf_errmsg(-1));
			continue;
		}
		r.symn = get_symbol_name(ed, s->link,
		    GELF_R_SYM(r.u_r.rel.r_info));
		elf_print_rel_entry(ed, s, j, &r);
	}
}

/*
 * Dump relocation sections.
 */
static void
elf_print_reloc(struct elfdump *ed)
{
	struct section	*s;
	Elf_Data	*data;
	int		 i, elferr;

	for (i = 0; (size_t)i < ed->shnum; i++) {
		s = &ed->sl[i];
		if ((s->type == SHT_REL || s->type == SHT_RELA) &&
		    (STAILQ_EMPTY(&ed->snl) || find_name(ed, s->name))) {
			(void) elf_errno();
			if ((data = elf_getdata(s->scn, NULL)) == NULL) {
				elferr = elf_errno();
				if (elferr != 0)
					warnx("elf_getdata failed: %s",
					    elf_errmsg(elferr));
				continue;
			}
			if (s->type == SHT_REL)
				elf_print_rel(ed, s, data);
			else
				elf_print_rela(ed, s, data);
		}
	}
}

/*
 * Dump the content of PT_INTERP segment.
 */
static void
elf_print_interp(struct elfdump *ed)
{
	const char *s;
	GElf_Phdr phdr;
	size_t phnum;
	int i;

	if (!STAILQ_EMPTY(&ed->snl) && find_name(ed, "PT_INTERP") == NULL)
		return;

	if ((s = elf_rawfile(ed->elf, NULL)) == NULL) {
		warnx("elf_rawfile failed: %s", elf_errmsg(-1));
		return;
	}
	if (!elf_getphnum(ed->elf, &phnum)) {
		warnx("elf_getphnum failed: %s", elf_errmsg(-1));
		return;
	}
	for (i = 0; (size_t)i < phnum; i++) {
		if (gelf_getphdr(ed->elf, i, &phdr) != &phdr) {
			warnx("elf_getphdr failed: %s", elf_errmsg(-1));
			continue;
		}
		if (phdr.p_type == PT_INTERP) {
			PRT("\ninterp:\n");
			PRT("\t%s\n", s + phdr.p_offset);
		}
	}
}

/*
 * Search the relocation sections for entries refering to the .got section.
 */
static void
find_gotrel(struct elfdump *ed, struct section *gs, struct rel_entry *got)
{
	struct section		*s;
	struct rel_entry	 r;
	Elf_Data		*data;
	int			 elferr, i, j, k, len;

	for(i = 0; (size_t)i < ed->shnum; i++) {
		s = &ed->sl[i];
		if (s->type != SHT_REL && s->type != SHT_RELA)
			continue;
		(void) elf_errno();
		if ((data = elf_getdata(s->scn, NULL)) == NULL) {
			elferr = elf_errno();
			if (elferr != 0)
				warnx("elf_getdata failed: %s",
				    elf_errmsg(elferr));
			return;
		}
		memset(&r, 0, sizeof(struct rel_entry));
		r.type = s->type;
		len = data->d_size / s->entsize;
		for (j = 0; j < len; j++) {
			if (s->type == SHT_REL) {
				if (gelf_getrel(data, j, &r.u_r.rel) !=
				    &r.u_r.rel) {
					warnx("gelf_getrel failed: %s",
					    elf_errmsg(-1));
					continue;
				}
			} else {
				if (gelf_getrela(data, j, &r.u_r.rela) !=
				    &r.u_r.rela) {
					warnx("gelf_getrel failed: %s",
					    elf_errmsg(-1));
					continue;
				}
			}
			if (r.u_r.rel.r_offset >= gs->addr &&
			    r.u_r.rel.r_offset < gs->addr + gs->sz) {
				r.symn = get_symbol_name(ed, s->link,
				    GELF_R_SYM(r.u_r.rel.r_info));
				k = (r.u_r.rel.r_offset - gs->addr) /
				    gs->entsize;
				memcpy(&got[k], &r, sizeof(struct rel_entry));
			}
		}
	}
}

static void
elf_print_got_section(struct elfdump *ed, struct section *s)
{
	struct rel_entry	*got;
	Elf_Data		*data, dst;
	int			 elferr, i, len;

	if (s->entsize == 0) {
		/* XXX IA64 GOT section generated by gcc has entry size 0. */
		if (s->align != 0)
			s->entsize = s->align;
		else
			return;
	}

	if (ed->flags & SOLARIS_FMT)
		PRT("\nGlobal Offset Table Section:  %s  (%jd entries)\n",
		    s->name, s->sz / s->entsize);
	else
		PRT("\nglobal offset table: %s\n", s->name);
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s", elf_errmsg(elferr));
		return;
	}

	/*
	 * GOT section has section type SHT_PROGBITS, thus libelf treats it as
	 * byte stream and will not perfrom any translation on it. As a result,
	 * an exlicit call to gelf_xlatetom is needed here. Depends on arch,
	 * GOT section should be translated to either WORD or XWORD.
	 */
	if (ed->ec == ELFCLASS32)
		data->d_type = ELF_T_WORD;
	else
		data->d_type = ELF_T_XWORD;
	memcpy(&dst, data, sizeof(Elf_Data));
	if (gelf_xlatetom(ed->elf, &dst, data, ed->ehdr.e_ident[EI_DATA]) !=
	    &dst) {
		warnx("gelf_xlatetom failed: %s", elf_errmsg(-1));
		return;
	}
	len = dst.d_size / s->entsize;
	if (ed->flags & SOLARIS_FMT) {
		/*
		 * In verbose/Solaris mode, we search the relocation sections
		 * and try to find the corresponding reloc entry for each GOT
		 * section entry.
		 */
		if ((got = calloc(len, sizeof(struct rel_entry))) == NULL)
			err(EXIT_FAILURE, "calloc failed");
		find_gotrel(ed, s, got);
		if (ed->ec == ELFCLASS32) {
			PRT(" ndx     addr      value    reloc              ");
			PRT("addend   symbol\n");
		} else {
			PRT(" ndx     addr              value             ");
			PRT("reloc              addend       symbol\n");
		}
		for(i = 0; i < len; i++) {
			PRT("[%5.5d]  ", i);
			if (ed->ec == ELFCLASS32) {
				PRT("%-8.8jx  ", s->addr + i * s->entsize);
				PRT("%-8.8x ", *((uint32_t *)dst.d_buf + i));
			} else {
				PRT("%-16.16jx  ", s->addr + i * s->entsize);
				PRT("%-16.16jx  ", *((uint64_t *)dst.d_buf + i));
			}
			PRT("%-18s ", r_type(ed->ehdr.e_machine,
				GELF_R_TYPE(got[i].u_r.rel.r_info)));
			if (ed->ec == ELFCLASS32)
				PRT("%-8.8jd ",
				    (intmax_t)got[i].u_r.rela.r_addend);
			else
				PRT("%-12.12jd ",
				    (intmax_t)got[i].u_r.rela.r_addend);
			if (got[i].symn == NULL)
				got[i].symn = "";
			PRT("%s\n", got[i].symn);
		}
		free(got);
	} else {
		for(i = 0; i < len; i++) {
			PRT("\nentry: %d\n", i);
			if (ed->ec == ELFCLASS32)
				PRT("\t%#x\n", *((uint32_t *)dst.d_buf + i));
			else
				PRT("\t%#jx\n", *((uint64_t *)dst.d_buf + i));
		}
	}
}

/*
 * Dump the content of Global Offset Table section.
 */
static void
elf_print_got(struct elfdump *ed)
{
	struct section	*s;
	int		 i;

	if (!STAILQ_EMPTY(&ed->snl))
		return;

	s = NULL;
	for (i = 0; (size_t)i < ed->shnum; i++) {
		s = &ed->sl[i];
		if (s->name && !strncmp(s->name, ".got", 4) &&
		    (STAILQ_EMPTY(&ed->snl) || find_name(ed, s->name)))
			elf_print_got_section(ed, s);
	}
}

/*
 * Dump the content of .note.ABI-tag section.
 */
static void
elf_print_note(struct elfdump *ed)
{
	struct section	*s;
	Elf_Data        *data;
	Elf_Note	*en;
	uint32_t	 namesz;
	uint32_t	 descsz;
	uint32_t	 desc;
	size_t		 count;
	int		 elferr, i;
	char		*src, idx[10];

	s = NULL;
	for (i = 0; (size_t)i < ed->shnum; i++) {
		s = &ed->sl[i];
		if (s->type == SHT_NOTE && s->name &&
		    !strcmp(s->name, ".note.ABI-tag") &&
		    (STAILQ_EMPTY(&ed->snl) || find_name(ed, s->name)))
			break;
	}
	if ((size_t)i >= ed->shnum)
		return;
	if (ed->flags & SOLARIS_FMT)
		PRT("\nNote Section:  %s\n", s->name);
	else
		PRT("\nnote (%s):\n", s->name);
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s", elf_errmsg(elferr));
		return;
	}
	src = data->d_buf;
	count = data->d_size;
	while (count > sizeof(Elf_Note)) {
		en = (Elf_Note *) (uintptr_t) src;
		namesz = en->n_namesz;
		descsz = en->n_descsz;
		src += sizeof(Elf_Note);
		count -= sizeof(Elf_Note);
		if (ed->flags & SOLARIS_FMT) {
			PRT("\n    type   %#x\n", en->n_type);
			PRT("    namesz %#x:\n", en->n_namesz);
			PRT("%s\n", src);
		} else
			PRT("\t%s ", src);
		src += roundup2(namesz, 4);
		count -= roundup2(namesz, 4);

		/*
		 * Note that we dump the whole desc part if we're in
		 * "Solaris mode", while in the normal mode, we only look
		 * at the first 4 bytes (a 32bit word) of the desc, i.e,
		 * we assume that it's always a FreeBSD version number.
		 */
		if (ed->flags & SOLARIS_FMT) {
			PRT("    descsz %#x:", en->n_descsz);
			for (i = 0; (uint32_t)i < descsz; i++) {
				if ((i & 0xF) == 0) {
					snprintf(idx, sizeof(idx), "desc[%d]",
					    i);
					PRT("\n      %-9s", idx);
				} else if ((i & 0x3) == 0)
					PRT("  ");
				PRT(" %2.2x", src[i]);
			}
			PRT("\n");
		} else {
			if (ed->ehdr.e_ident[EI_DATA] == ELFDATA2MSB)
				desc = be32dec(src);
			else
				desc = le32dec(src);
			PRT("%d\n", desc);
		}
		src += roundup2(descsz, 4);
		count -= roundup2(descsz, 4);
	}
}

/*
 * Dump a hash table.
 */
static void
elf_print_svr4_hash(struct elfdump *ed, struct section *s)
{
	Elf_Data	*data;
	uint32_t	*buf;
	uint32_t	*bucket, *chain;
	uint32_t	 nbucket, nchain;
	uint32_t	*bl, *c, maxl, total;
	int		 i, j, first, elferr;
	char		 idx[10];

	if (ed->flags & SOLARIS_FMT)
		PRT("\nHash Section:  %s\n", s->name);
	else
		PRT("\nhash table (%s):\n", s->name);
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s",
			    elf_errmsg(elferr));
		return;
	}
	if (data->d_size < 2 * sizeof(uint32_t)) {
		warnx(".hash section too small");
		return;
	}
	buf = data->d_buf;
	nbucket = buf[0];
	nchain = buf[1];
	if (nbucket <= 0 || nchain <= 0) {
		warnx("Malformed .hash section");
		return;
	}
	if (data->d_size != (nbucket + nchain + 2) * sizeof(uint32_t)) {
		warnx("Malformed .hash section");
		return;
	}
	bucket = &buf[2];
	chain = &buf[2 + nbucket];

	if (ed->flags & SOLARIS_FMT) {
		maxl = 0;
		if ((bl = calloc(nbucket, sizeof(*bl))) == NULL)
			err(EXIT_FAILURE, "calloc failed");
		for (i = 0; (uint32_t)i < nbucket; i++)
			for (j = bucket[i]; j > 0 && (uint32_t)j < nchain;
			     j = chain[j])
				if (++bl[i] > maxl)
					maxl = bl[i];
		if ((c = calloc(maxl + 1, sizeof(*c))) == NULL)
			err(EXIT_FAILURE, "calloc failed");
		for (i = 0; (uint32_t)i < nbucket; i++)
			c[bl[i]]++;
		PRT("    bucket    symndx    name\n");
		for (i = 0; (uint32_t)i < nbucket; i++) {
			first = 1;
			for (j = bucket[i]; j > 0 && (uint32_t)j < nchain;
			     j = chain[j]) {
				if (first) {
					PRT("%10d  ", i);
					first = 0;
				} else
					PRT("            ");
				snprintf(idx, sizeof(idx), "[%d]", j);
				PRT("%-10s  ", idx);
				PRT("%s\n", get_symbol_name(ed, s->link, j));
			}
		}
		PRT("\n");
		total = 0;
		for (i = 0; (uint32_t)i <= maxl; i++) {
			total += c[i] * i;
			PRT("%10u  buckets contain %8d symbols\n", c[i], i);
		}
		PRT("%10u  buckets         %8u symbols (globals)\n", nbucket,
		    total);
	} else {
		PRT("\nnbucket: %u\n", nbucket);
		PRT("nchain: %u\n\n", nchain);
		for (i = 0; (uint32_t)i < nbucket; i++)
			PRT("bucket[%d]:\n\t%u\n\n", i, bucket[i]);
		for (i = 0; (uint32_t)i < nchain; i++)
			PRT("chain[%d]:\n\t%u\n\n", i, chain[i]);
	}
}

/*
 * Dump a 64bit hash table.
 */
static void
elf_print_svr4_hash64(struct elfdump *ed, struct section *s)
{
	Elf_Data	*data, dst;
	uint64_t	*buf;
	uint64_t	*bucket, *chain;
	uint64_t	 nbucket, nchain;
	uint64_t	*bl, *c, maxl, total;
	int		 i, j, elferr, first;
	char		 idx[10];

	if (ed->flags & SOLARIS_FMT)
		PRT("\nHash Section:  %s\n", s->name);
	else
		PRT("\nhash table (%s):\n", s->name);

	/*
	 * ALPHA uses 64-bit hash entries. Since libelf assumes that
	 * .hash section contains only 32-bit entry, an explicit
	 * gelf_xlatetom is needed here.
	 */
	(void) elf_errno();
	if ((data = elf_rawdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_rawdata failed: %s",
			    elf_errmsg(elferr));
		return;
	}
	data->d_type = ELF_T_XWORD;
	memcpy(&dst, data, sizeof(Elf_Data));
	if (gelf_xlatetom(ed->elf, &dst, data,
		ed->ehdr.e_ident[EI_DATA]) != &dst) {
		warnx("gelf_xlatetom failed: %s", elf_errmsg(-1));
		return;
	}
	if (dst.d_size < 2 * sizeof(uint64_t)) {
		warnx(".hash section too small");
		return;
	}
	buf = dst.d_buf;
	nbucket = buf[0];
	nchain = buf[1];
	if (nbucket <= 0 || nchain <= 0) {
		warnx("Malformed .hash section");
		return;
	}
	if (dst.d_size != (nbucket + nchain + 2) * sizeof(uint64_t)) {
		warnx("Malformed .hash section");
		return;
	}
	bucket = &buf[2];
	chain = &buf[2 + nbucket];

	if (ed->flags & SOLARIS_FMT) {
		maxl = 0;
		if ((bl = calloc(nbucket, sizeof(*bl))) == NULL)
			err(EXIT_FAILURE, "calloc failed");
		for (i = 0; (uint64_t)i < nbucket; i++)
			for (j = bucket[i]; j > 0 && (uint64_t)j < nchain;
			     j = chain[j])
				if (++bl[i] > maxl)
					maxl = bl[i];
		if ((c = calloc(maxl + 1, sizeof(*c))) == NULL)
			err(EXIT_FAILURE, "calloc failed");
		for (i = 0; (uint64_t)i < nbucket; i++)
			c[bl[i]]++;
		PRT("    bucket    symndx    name\n");
		for (i = 0; (uint64_t)i < nbucket; i++) {
			first = 1;
			for (j = bucket[i]; j > 0 && (uint64_t)j < nchain;
			     j = chain[j]) {
				if (first) {
					PRT("%10d  ", i);
					first = 0;
				} else
					PRT("            ");
				snprintf(idx, sizeof(idx), "[%d]", j);
				PRT("%-10s  ", idx);
				PRT("%s\n", get_symbol_name(ed, s->link, j));
			}
		}
		PRT("\n");
		total = 0;
		for (i = 0; (uint64_t)i <= maxl; i++) {
			total += c[i] * i;
			PRT("%10ju  buckets contain %8d symbols\n",
			    (uintmax_t)c[i], i);
		}
		PRT("%10ju  buckets         %8ju symbols (globals)\n",
		    (uintmax_t)nbucket, (uintmax_t)total);
	} else {
		PRT("\nnbucket: %ju\n", (uintmax_t)nbucket);
		PRT("nchain: %ju\n\n", (uintmax_t)nchain);
		for (i = 0; (uint64_t)i < nbucket; i++)
			PRT("bucket[%d]:\n\t%ju\n\n", i, (uintmax_t)bucket[i]);
		for (i = 0; (uint64_t)i < nchain; i++)
			PRT("chain[%d]:\n\t%ju\n\n", i, (uintmax_t)chain[i]);
	}

}

/*
 * Dump a GNU hash table.
 */
static void
elf_print_gnu_hash(struct elfdump *ed, struct section *s)
{
	struct section	*ds;
	Elf_Data	*data;
	uint32_t	*buf;
	uint32_t	*bucket, *chain;
	uint32_t	 nbucket, nchain, symndx, maskwords, shift2;
	uint32_t	*bl, *c, maxl, total;
	int		 i, j, first, elferr, dynsymcount;
	char		 idx[10];

	if (ed->flags & SOLARIS_FMT)
		PRT("\nGNU Hash Section:  %s\n", s->name);
	else
		PRT("\ngnu hash table (%s):\n", s->name);
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s",
			    elf_errmsg(elferr));
		return;
	}
	if (data->d_size < 4 * sizeof(uint32_t)) {
		warnx(".gnu.hash section too small");
		return;
	}
	buf = data->d_buf;
	nbucket = buf[0];
	symndx = buf[1];
	maskwords = buf[2];
	shift2 = buf[3];
	buf += 4;
	ds = &ed->sl[s->link];
	dynsymcount = ds->sz / ds->entsize;
	nchain = dynsymcount - symndx;
	if (data->d_size != 4 * sizeof(uint32_t) + maskwords *
	    (ed->ec == ELFCLASS32 ? sizeof(uint32_t) : sizeof(uint64_t)) +
	    (nbucket + nchain) * sizeof(uint32_t)) {
		warnx("Malformed .gnu.hash section");
		return;
	}
	bucket = buf + (ed->ec == ELFCLASS32 ? maskwords : maskwords * 2);
	chain = bucket + nbucket;

	if (ed->flags & SOLARIS_FMT) {
		maxl = 0;
		if ((bl = calloc(nbucket, sizeof(*bl))) == NULL)
			err(EXIT_FAILURE, "calloc failed");
		for (i = 0; (uint32_t)i < nbucket; i++)
			for (j = bucket[i];
			     j > 0 && (uint32_t)j - symndx < nchain;
			     j++) {
				if (++bl[i] > maxl)
					maxl = bl[i];
				if (chain[j - symndx] & 1)
					break;
			}
		if ((c = calloc(maxl + 1, sizeof(*c))) == NULL)
			err(EXIT_FAILURE, "calloc failed");
		for (i = 0; (uint32_t)i < nbucket; i++)
			c[bl[i]]++;
		PRT("    bucket    symndx    name\n");
		for (i = 0; (uint32_t)i < nbucket; i++) {
			first = 1;
			for (j = bucket[i];
			     j > 0 && (uint32_t)j - symndx < nchain;
			     j++) {
				if (first) {
					PRT("%10d  ", i);
					first = 0;
				} else
					PRT("            ");
				snprintf(idx, sizeof(idx), "[%d]", j );
				PRT("%-10s  ", idx);
				PRT("%s\n", get_symbol_name(ed, s->link, j));
				if (chain[j - symndx] & 1)
					break;
			}
		}
		PRT("\n");
		total = 0;
		for (i = 0; (uint32_t)i <= maxl; i++) {
			total += c[i] * i;
			PRT("%10u  buckets contain %8d symbols\n", c[i], i);
		}
		PRT("%10u  buckets         %8u symbols (globals)\n", nbucket,
		    total);
	} else {
		PRT("\nnbucket: %u\n", nbucket);
		PRT("symndx: %u\n", symndx);
		PRT("maskwords: %u\n", maskwords);
		PRT("shift2: %u\n", shift2);
		PRT("nchain: %u\n\n", nchain);
		for (i = 0; (uint32_t)i < nbucket; i++)
			PRT("bucket[%d]:\n\t%u\n\n", i, bucket[i]);
		for (i = 0; (uint32_t)i < nchain; i++)
			PRT("chain[%d]:\n\t%u\n\n", i, chain[i]);
	}
}

/*
 * Dump hash tables.
 */
static void
elf_print_hash(struct elfdump *ed)
{
	struct section	*s;
	int		 i;

	for (i = 0; (size_t)i < ed->shnum; i++) {
		s = &ed->sl[i];
		if ((s->type == SHT_HASH || s->type == SHT_GNU_HASH) &&
		    (STAILQ_EMPTY(&ed->snl) || find_name(ed, s->name))) {
			if (s->type == SHT_GNU_HASH)
				elf_print_gnu_hash(ed, s);
			else if (ed->ehdr.e_machine == EM_ALPHA &&
			    s->entsize == 8)
				elf_print_svr4_hash64(ed, s);
			else
				elf_print_svr4_hash(ed, s);
		}
	}
}

/*
 * Dump the content of a Version Definition(SHT_SUNW_Verdef) Section.
 */
static void
elf_print_verdef(struct elfdump *ed, struct section *s)
{
	Elf_Data	*data;
	Elf32_Verdef	*vd;
	Elf32_Verdaux	*vda;
	const char 	*str;
	char		 idx[10];
	uint8_t		*buf, *end, *buf2;
	int		 i, j, elferr, count;

	if (ed->flags & SOLARIS_FMT)
		PRT("Version Definition Section:  %s\n", s->name);
	else
		PRT("\nversion definition section (%s):\n", s->name);
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s",
			    elf_errmsg(elferr));
		return;
	}
	buf = data->d_buf;
	end = buf + data->d_size;
	i = 0;
	if (ed->flags & SOLARIS_FMT)
		PRT("     index  version                     dependency\n");
	while (buf + sizeof(Elf32_Verdef) <= end) {
		vd = (Elf32_Verdef *) (uintptr_t) buf;
		if (ed->flags & SOLARIS_FMT) {
			snprintf(idx, sizeof(idx), "[%d]", vd->vd_ndx);
			PRT("%10s  ", idx);
		} else {
			PRT("\nentry: %d\n", i++);
			PRT("\tvd_version: %u\n", vd->vd_version);
			PRT("\tvd_flags: %u\n", vd->vd_flags);
			PRT("\tvd_ndx: %u\n", vd->vd_ndx);
			PRT("\tvd_cnt: %u\n", vd->vd_cnt);
			PRT("\tvd_hash: %u\n", vd->vd_hash);
			PRT("\tvd_aux: %u\n", vd->vd_aux);
			PRT("\tvd_next: %u\n\n", vd->vd_next);
		}
		buf2 = buf + vd->vd_aux;
		j = 0;
		count = 0;
		while (buf2 + sizeof(Elf32_Verdaux) <= end && j < vd->vd_cnt) {
			vda = (Elf32_Verdaux *) (uintptr_t) buf2;
			str = get_string(ed, s->link, vda->vda_name);
			if (ed->flags & SOLARIS_FMT) {
				if (count == 0)
					PRT("%-26.26s", str);
				else if (count == 1)
					PRT("  %-20.20s", str);
				else {
					PRT("\n%40.40s", "");
					PRT("%s", str);
				}
			} else {
				PRT("\t\tvda: %d\n", j++);
				PRT("\t\t\tvda_name: %s\n", str);
				PRT("\t\t\tvda_next: %u\n", vda->vda_next);
			}
			if (vda->vda_next == 0) {
				if (ed->flags & SOLARIS_FMT) {
					if (vd->vd_flags & VER_FLG_BASE) {
						if (count == 0)
							PRT("%-20.20s", "");
						PRT("%s", "[ BASE ]");
					}
					PRT("\n");
				}
				break;
			}
			if (ed->flags & SOLARIS_FMT)
				count++;
			buf2 += vda->vda_next;
		}
		if (vd->vd_next == 0)
			break;
		buf += vd->vd_next;
	}
}

/*
 * Dump the content of a Version Needed(SHT_SUNW_Verneed) Section.
 */
static void
elf_print_verneed(struct elfdump *ed, struct section *s)
{
	Elf_Data	*data;
	Elf32_Verneed	*vn;
	Elf32_Vernaux	*vna;
	uint8_t		*buf, *end, *buf2;
	int		 i, j, elferr, first;

	if (ed->flags & SOLARIS_FMT)
		PRT("\nVersion Needed Section:  %s\n", s->name);
	else
		PRT("\nversion need section (%s):\n", s->name);
	(void) elf_errno();
	if ((data = elf_getdata(s->scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			warnx("elf_getdata failed: %s",
			    elf_errmsg(elferr));
		return;
	}
	buf = data->d_buf;
	end = buf + data->d_size;
	if (ed->flags & SOLARIS_FMT)
		PRT("            file                        version\n");
	i = 0;
	while (buf + sizeof(Elf32_Verneed) <= end) {
		vn = (Elf32_Verneed *) (uintptr_t) buf;
		if (ed->flags & SOLARIS_FMT)
			PRT("            %-26.26s  ",
			    get_string(ed, s->link, vn->vn_file));
		else {
			PRT("\nentry: %d\n", i++);
			PRT("\tvn_version: %u\n", vn->vn_version);
			PRT("\tvn_cnt: %u\n", vn->vn_cnt);
			PRT("\tvn_file: %s\n",
			    get_string(ed, s->link, vn->vn_file));
			PRT("\tvn_aux: %u\n", vn->vn_aux);
			PRT("\tvn_next: %u\n\n", vn->vn_next);
		}
		buf2 = buf + vn->vn_aux;
		j = 0;
		first = 1;
		while (buf2 + sizeof(Elf32_Vernaux) <= end && j < vn->vn_cnt) {
			vna = (Elf32_Vernaux *) (uintptr_t) buf2;
			if (ed->flags & SOLARIS_FMT) {
				if (!first)
					PRT("%40.40s", "");
				else
					first = 0;
				PRT("%s\n", get_string(ed, s->link,
				    vna->vna_name));
			} else {
				PRT("\t\tvna: %d\n", j++);
				PRT("\t\t\tvna_hash: %u\n", vna->vna_hash);
				PRT("\t\t\tvna_flags: %u\n", vna->vna_flags);
				PRT("\t\t\tvna_other: %u\n", vna->vna_other);
				PRT("\t\t\tvna_name: %s\n",
				    get_string(ed, s->link, vna->vna_name));
				PRT("\t\t\tvna_next: %u\n", vna->vna_next);
			}
			if (vna->vna_next == 0)
				break;
			buf2 += vna->vna_next;
		}
		if (vn->vn_next == 0)
			break;
		buf += vn->vn_next;
	}
}

/*
 * Dump the symbol-versioning sections.
 */
static void
elf_print_symver(struct elfdump *ed)
{
	struct section	*s;
	int		 i;

	for (i = 0; (size_t)i < ed->shnum; i++) {
		s = &ed->sl[i];
		if (!STAILQ_EMPTY(&ed->snl) && !find_name(ed, s->name))
			continue;
		if (s->type == SHT_SUNW_verdef)
			elf_print_verdef(ed, s);
		if (s->type == SHT_SUNW_verneed)
			elf_print_verneed(ed, s);
	}
}

/*
 * Dump the ELF checksum. See gelf_checksum(3) for details.
 */
static void
elf_print_checksum(struct elfdump *ed)
{

	if (!STAILQ_EMPTY(&ed->snl))
		return;

	PRT("\nelf checksum: %#lx\n", gelf_checksum(ed->elf));
}

#define	USAGE_MESSAGE	"\
Usage: %s [options] file...\n\
  Display information about ELF objects and ar(1) archives.\n\n\
  Options:\n\
  -a                        Show all information.\n\
  -c                        Show shared headers.\n\
  -d                        Show dynamic symbols.\n\
  -e                        Show the ELF header.\n\
  -G                        Show the GOT.\n\
  -H | --help               Show a usage message and exit.\n\
  -h                        Show hash values.\n\
  -i                        Show the dynamic interpreter.\n\
  -k                        Show the ELF checksum.\n\
  -n                        Show the contents of note sections.\n\
  -N NAME                   Show the section named \"NAME\".\n\
  -p                        Show the program header.\n\
  -r                        Show relocations.\n\
  -s                        Show the symbol table.\n\
  -S                        Use the Solaris elfdump format.\n\
  -v                        Show symbol-versioning information.\n\
  -V | --version            Print a version identifier and exit.\n\
  -w FILE                   Write output to \"FILE\".\n"

static void
usage(void)
{
	fprintf(stderr, USAGE_MESSAGE, ELFTC_GETPROGNAME());
	exit(EXIT_FAILURE);
}
