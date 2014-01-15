/*-
 * Copyright (c) 2011 Joseph Koshy
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
 * $Id: abi.m4 2077 2011-10-27 03:59:40Z jkoshy $
 */

/*
 * Tests for ABI values.
 *
 * See: http://www.sco.com/developers/gabi/latest/ch4.eheader.html
 */

#include <libelf.h>

#include "tet_api.h"

include(`elfts.m4')

undefine(`FN')
define(`FN',`
void
tcCheck$1(void)
{
	int result;
	const size_t nconst = sizeof($2) / sizeof ($2[0]);
	size_t n;

	TP_ANNOUNCE("Check " $3 " values");

	result = TET_FAIL;

	for (n = 0; n < nconst; n++)
		if ($2[n].symbol != $2[n].value)
			goto done;

	result = TET_PASS;

done:
	tet_result(result);
}
')

struct _sym {
	size_t	symbol;
	size_t	value;
};

/*
 * Check ELFOSABI_* values.
 */
struct _sym elf_osabi[] = {
       { ELFOSABI_NONE,		0},
       { ELFOSABI_SYSV,		0},
       { ELFOSABI_HPUX,		1},
       { ELFOSABI_NETBSD,	2},
       { ELFOSABI_GNU,        3},
       { ELFOSABI_HURD,       4},
       { ELFOSABI_86OPEN,     5},
       { ELFOSABI_SOLARIS,    6},
       { ELFOSABI_AIX,        7},
       { ELFOSABI_IRIX,       8},
       { ELFOSABI_FREEBSD,    9},
       { ELFOSABI_TRU64,      10},
       { ELFOSABI_MODESTO,    11},
       { ELFOSABI_OPENBSD,    12},
       { ELFOSABI_OPENVMS,    13},
       { ELFOSABI_NSK,        14},
       { ELFOSABI_AROS,       15},
       { ELFOSABI_FENIXOS,    16}
};

FN(OsAbi, elf_osabi, "ELF_OSABI")

/*
 * Check EM_* values.
 */
struct _sym elf_em[] = {
	{ EM_NONE, 0 },
	{ EM_M32, 1 },
	{ EM_SPARC, 2 },
	{ EM_386, 3 },
	{ EM_68K, 4 },
	{ EM_88K, 5 },
	{ EM_860, 7 },
	{ EM_MIPS, 8 },
	{ EM_S370, 9 },
	{ EM_MIPS_RS3_LE, 10 },
	{ EM_PARISC, 15 },
	{ EM_VPP500, 17 },
	{ EM_SPARC32PLUS, 18 },
	{ EM_960, 19 },
	{ EM_PPC, 20 },
	{ EM_PPC64, 21 },
	{ EM_S390, 22 },
	{ EM_SPU, 23 },
	{ EM_V800, 36 },
	{ EM_FR20, 37 },
	{ EM_RH32, 38 },
	{ EM_RCE, 39 },
	{ EM_ARM, 40 },
	{ EM_ALPHA, 41 },
	{ EM_SH, 42 },
	{ EM_SPARCV9, 43 },
	{ EM_TRICORE, 44 },
	{ EM_ARC, 45 },
	{ EM_H8_300, 46 },
	{ EM_H8_300H, 47 },
	{ EM_H8S, 48 },
	{ EM_H8_500, 49 },
	{ EM_IA_64, 50 },
	{ EM_MIPS_X, 51 },
	{ EM_COLDFIRE, 52 },
	{ EM_68HC12, 53 },
	{ EM_MMA, 54 },
	{ EM_PCP, 55 },
	{ EM_NCPU, 56 },
	{ EM_NDR1, 57 },
	{ EM_STARCORE, 58 },
	{ EM_ME16, 59 },
	{ EM_ST100, 60 },
	{ EM_TINYJ, 61 },
	{ EM_X86_64, 62 },
	{ EM_PDSP, 63 },
	{ EM_PDP10, 64 },
	{ EM_PDP11, 65 },
	{ EM_FX66, 66 },
	{ EM_ST9PLUS, 67 },
	{ EM_ST7, 68 },
	{ EM_68HC16, 69 },
	{ EM_68HC11, 70 },
	{ EM_68HC08, 71 },
	{ EM_68HC05, 72 },
	{ EM_SVX, 73 },
	{ EM_ST19, 74 },
	{ EM_VAX, 75 },
	{ EM_CRIS, 76 },
	{ EM_JAVELIN, 77 },
	{ EM_FIREPATH, 78 },
	{ EM_ZSP, 79 },
	{ EM_MMIX, 80 },
	{ EM_HUANY, 81 },
	{ EM_PRISM, 82 },
	{ EM_AVR, 83 },
	{ EM_FR30, 84 },
	{ EM_D10V, 85 },
	{ EM_D30V, 86 },
	{ EM_V850, 87 },
	{ EM_M32R, 88 },
	{ EM_MN10300, 89 },
	{ EM_MN10200, 90 },
	{ EM_PJ, 91 },
	{ EM_OPENRISC, 92 },
	{ EM_ARC_COMPACT, 93 },
	{ EM_XTENSA, 94 },
	{ EM_VIDEOCORE, 95 },
	{ EM_TMM_GPP, 96 },
	{ EM_NS32K, 97 },
	{ EM_TPC, 98 },
	{ EM_SNP1K, 99 },
	{ EM_ST200, 100 },
	{ EM_IP2K, 101 },
	{ EM_MAX, 102 },
	{ EM_CR, 103 },
	{ EM_F2MC16, 104 },
	{ EM_MSP430, 105 },
	{ EM_BLACKFIN, 106 },
	{ EM_SE_C33, 107 },
	{ EM_SEP, 108 },
	{ EM_ARCA, 109 },
	{ EM_UNICORE, 110 },
	{ EM_EXCESS, 111 },
	{ EM_DXP, 112 },
	{ EM_ALTERA_NIOS2, 113 },
	{ EM_CRX, 114 },
	{ EM_XGATE, 115 },
	{ EM_C166, 116 },
	{ EM_M16C, 117 },
	{ EM_DSPIC30F, 118 },
	{ EM_CE, 119 },
	{ EM_M32C, 120 },
	{ EM_TSK3000, 131 },
	{ EM_RS08, 132 },
	{ EM_SHARC, 133 },
	{ EM_ECOG2, 134 },
	{ EM_SCORE7, 135 },
	{ EM_DSP24, 136 },
	{ EM_VIDEOCORE3, 137 },
	{ EM_LATTICEMICO32, 138 },
	{ EM_SE_C17, 139 },
	{ EM_TI_C6000, 140 },
	{ EM_TI_C2000, 141 },
	{ EM_TI_C5500, 142 },
	{ EM_MMDSP_PLUS, 160 },
	{ EM_CYPRESS_M8C, 161 },
	{ EM_R32C, 162 },
	{ EM_TRIMEDIA, 163 },
	{ EM_QDSP6, 164 },
	{ EM_8051, 165 },
	{ EM_STXP7X, 166 },
	{ EM_NDS32, 167 },
	{ EM_ECOG1, 168 },
	{ EM_ECOG1X, 168 },
	{ EM_MAXQ30, 169 },
	{ EM_XIMO16, 170 },
	{ EM_MANIK, 171 },
	{ EM_CRAYNV2, 172 },
	{ EM_RX, 173 },
	{ EM_METAG, 174 },
	{ EM_MCST_ELBRUS, 175 },
	{ EM_ECOG16, 176 },
	{ EM_CR16, 177 },
	{ EM_ETPU, 178 },
	{ EM_SLE9X, 179 },
	{ EM_AVR32, 185 },
	{ EM_STM8, 186 },
	{ EM_TILE64, 187 },
	{ EM_TILEPRO, 188 },
	{ EM_MICROBLAZE, 189 },
	{ EM_CUDA, 190 },
	{ EM_TILEGX, 191 },
	{ EM_CLOUDSHIELD, 192 },
	{ EM_COREA_1ST, 193 },
	{ EM_COREA_2ND, 194 },
	{ EM_ARC_COMPACT2, 195 },
	{ EM_OPEN8, 196 },
	{ EM_RL78, 197 },
	{ EM_VIDEOCORE5, 198 },
	{ EM_78KOR, 199 },
};

FN(ElfMachine, elf_em, "EM_*")

/*
 * Check ET_* values.
 */
struct _sym elf_type[] = {
       { ET_NONE, 0 },
       { ET_REL, 1 },
       { ET_EXEC, 2 },
       { ET_DYN, 3 },
       { ET_CORE, 4 }
};

FN(ElfType, elf_type, "ET_*")

/*
 * Check values for miscellaneous ABI symbols.
 */
struct _sym elf_misc[] = {
       { EV_NONE, 0 },
       { EV_CURRENT, 1 },
       { EI_MAG0, 0 },
       { EI_MAG1, 1 },
       { EI_MAG2, 2 },
       { EI_MAG3, 3 },
       { EI_CLASS, 4 },
       { EI_DATA, 5 },
       { EI_VERSION, 6 },
       { EI_OSABI, 7 },
       { EI_ABIVERSION, 8 },
       { EI_NIDENT, 16 },
       { ELFMAG0, 0x7F },
       { ELFMAG1, 'E' },
       { ELFMAG2, 'L' },
       { ELFMAG3, 'F' },
       { ELFCLASSNONE, 0 },
       { ELFCLASS32, 1 },
       { ELFCLASS64, 2 },
       { ELFDATANONE, 0 },
       { ELFDATA2LSB, 1 },
       { ELFDATA2MSB, 2 },
};

FN(ElfMisc, elf_misc, "miscellaneous symbol");
