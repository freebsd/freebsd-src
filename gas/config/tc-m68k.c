/* tc-m68k.c -- Assemble for the m68k family
   Copyright 1987, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "as.h"
#include "safe-ctype.h"
#include "obstack.h"
#include "subsegs.h"
#include "dwarf2dbg.h"
#include "dw2gencfi.h"

#include "opcode/m68k.h"
#include "m68k-parse.h"

#if defined (OBJ_ELF)
#include "elf/m68k.h"
#endif

#ifdef M68KCOFF
#include "obj-coff.h"
#endif

/* This string holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.  The macro
   tc_comment_chars points to this.  We use this, rather than the
   usual comment_chars, so that the --bitwise-or option will work.  */
#if defined (TE_SVR4) || defined (TE_DELTA)
const char *m68k_comment_chars = "|#";
#else
const char *m68k_comment_chars = "|";
#endif

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  */
/* Also note that comments like this one will always work.  */
const char line_comment_chars[] = "#*";

const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant from exp in floating point nums.  */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant, as
   in "0f12.456" or "0d1.2345e12".  */

const char FLT_CHARS[] = "rRsSfFdDxXeEpP";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c .  Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.  */

/* Are we trying to generate PIC code?  If so, absolute references
   ought to be made into linkage table references or pc-relative
   references.  Not implemented.  For ELF there are other means
   to denote pic relocations.  */
int flag_want_pic;

static int flag_short_refs;	/* -l option.  */
static int flag_long_jumps;	/* -S option.  */
static int flag_keep_pcrel;	/* --pcrel option.  */

#ifdef REGISTER_PREFIX_OPTIONAL
int flag_reg_prefix_optional = REGISTER_PREFIX_OPTIONAL;
#else
int flag_reg_prefix_optional;
#endif

/* Whether --register-prefix-optional was used on the command line.  */
static int reg_prefix_optional_seen;

/* The floating point coprocessor to use by default.  */
static enum m68k_register m68k_float_copnum = COP1;

/* If this is non-zero, then references to number(%pc) will be taken
   to refer to number, rather than to %pc + number.  */
static int m68k_abspcadd;

/* If this is non-zero, then the quick forms of the move, add, and sub
   instructions are used when possible.  */
static int m68k_quick = 1;

/* If this is non-zero, then if the size is not specified for a base
   or outer displacement, the assembler assumes that the size should
   be 32 bits.  */
static int m68k_rel32 = 1;

/* This is non-zero if m68k_rel32 was set from the command line.  */
static int m68k_rel32_from_cmdline;

/* The default width to use for an index register when using a base
   displacement.  */
static enum m68k_size m68k_index_width_default = SIZE_LONG;

/* We want to warn if any text labels are misaligned.  In order to get
   the right line number, we need to record the line number for each
   label.  */
struct label_line
{
  struct label_line *next;
  symbolS *label;
  char *file;
  unsigned int line;
  int text;
};

/* The list of labels.  */

static struct label_line *labels;

/* The current label.  */

static struct label_line *current_label;

/* Pointer to list holding the opcodes sorted by name.  */
static struct m68k_opcode const ** m68k_sorted_opcodes;

/* Its an arbitrary name:  This means I don't approve of it.
   See flames below.  */
static struct obstack robyn;

struct m68k_incant
  {
    const char *m_operands;
    unsigned long m_opcode;
    short m_opnum;
    short m_codenum;
    int m_arch;
    struct m68k_incant *m_next;
  };

#define getone(x)	((((x)->m_opcode)>>16)&0xffff)
#define gettwo(x)	(((x)->m_opcode)&0xffff)

static const enum m68k_register m68000_ctrl[] = { 0 };
static const enum m68k_register m68010_ctrl[] = {
  SFC, DFC, USP, VBR,
  0
};
static const enum m68k_register m68020_ctrl[] = {
  SFC, DFC, USP, VBR, CACR, CAAR, MSP, ISP,
  0
};
static const enum m68k_register m68040_ctrl[] = {
  SFC, DFC, CACR, TC, ITT0, ITT1, DTT0, DTT1,
  USP, VBR, MSP, ISP, MMUSR, URP, SRP,
  0
};
static const enum m68k_register m68060_ctrl[] = {
  SFC, DFC, CACR, TC, ITT0, ITT1, DTT0, DTT1, BUSCR,
  USP, VBR, URP, SRP, PCR,
  0
};
static const enum m68k_register mcf_ctrl[] = {
  CACR, TC, ACR0, ACR1, ACR2, ACR3, VBR, ROMBAR,
  RAMBAR0, RAMBAR1, MBAR,
  0
};
static const enum m68k_register mcf5208_ctrl[] = {
  CACR, ACR0, ACR1, VBR, RAMBAR1,
  0
};
static const enum m68k_register mcf5213_ctrl[] = {
  VBR, RAMBAR, FLASHBAR,
  0
};
static const enum m68k_register mcf5216_ctrl[] = {
  VBR, CACR, ACR0, ACR1, FLASHBAR, RAMBAR,
  0
};
static const enum m68k_register mcf5235_ctrl[] = {
  VBR, CACR, ACR0, ACR1, RAMBAR,
  0
};
static const enum m68k_register mcf5249_ctrl[] = {
  VBR, CACR, ACR0, ACR1, RAMBAR0, RAMBAR1, MBAR, MBAR2,
  0
};
static const enum m68k_register mcf5250_ctrl[] = {
  VBR,
  0
};
static const enum m68k_register mcf5271_ctrl[] = {
  VBR, CACR, ACR0, ACR1, RAMBAR,
  0
};
static const enum m68k_register mcf5272_ctrl[] = {
  VBR, CACR, ACR0, ACR1, ROMBAR, RAMBAR, MBAR,
  0
};
static const enum m68k_register mcf5275_ctrl[] = {
  VBR, CACR, ACR0, ACR1, RAMBAR,
  0
};
static const enum m68k_register mcf5282_ctrl[] = {
  VBR, CACR, ACR0, ACR1, FLASHBAR, RAMBAR,
  0
};
static const enum m68k_register mcf5329_ctrl[] = {
  VBR, CACR, ACR0, ACR1, RAMBAR,
  0
};
static const enum m68k_register mcf5373_ctrl[] = {
  VBR, CACR, ACR0, ACR1, RAMBAR,
  0
};
static const enum m68k_register mcfv4e_ctrl[] = {
  CACR, TC, ITT0, ITT1, DTT0, DTT1, BUSCR, VBR, PC, ROMBAR,
  ROMBAR1, RAMBAR0, RAMBAR1, MPCR, EDRAMBAR, SECMBAR, MBAR, MBAR0, MBAR1,
  PCR1U0, PCR1L0, PCR1U1, PCR1L1, PCR2U0, PCR2L0, PCR2U1, PCR2L1,
  PCR3U0, PCR3L0, PCR3U1, PCR3L1,
  0
};
#define cpu32_ctrl m68010_ctrl

static const enum m68k_register *control_regs;

/* Internal form of a 68020 instruction.  */
struct m68k_it
{
  const char *error;
  const char *args;		/* List of opcode info.  */
  int numargs;

  int numo;			/* Number of shorts in opcode.  */
  short opcode[11];

  struct m68k_op operands[6];

  int nexp;			/* Number of exprs in use.  */
  struct m68k_exp exprs[4];

  int nfrag;			/* Number of frags we have to produce.  */
  struct
    {
      int fragoff;		/* Where in the current opcode the frag ends.  */
      symbolS *fadd;
      offsetT foff;
      int fragty;
    }
  fragb[4];

  int nrel;			/* Num of reloc strucs in use.  */
  struct
    {
      int n;
      expressionS exp;
      char wid;
      char pcrel;
      /* In a pc relative address the difference between the address
	 of the offset and the address that the offset is relative
	 to.  This depends on the addressing mode.  Basically this
	 is the value to put in the offset field to address the
	 first byte of the offset, without regarding the special
	 significance of some values (in the branch instruction, for
	 example).  */
      int pcrel_fix;
#ifdef OBJ_ELF
      /* Whether this expression needs special pic relocation, and if
	 so, which.  */
      enum pic_relocation pic_reloc;
#endif
    }
  reloc[5];			/* Five is enough???  */
};

#define cpu_of_arch(x)		((x) & (m68000up | mcfisa_a))
#define float_of_arch(x)	((x) & mfloat)
#define mmu_of_arch(x)		((x) & mmmu)
#define arch_coldfire_p(x)	((x) & mcfisa_a)
#define arch_coldfire_fpu(x)	((x) & cfloat)

/* Macros for determining if cpu supports a specific addressing mode.  */
#define HAVE_LONG_BRANCH(x)     ((x) & (m68020|m68030|m68040|m68060|cpu32|mcfisa_b))

static struct m68k_it the_ins;	/* The instruction being assembled.  */

#define op(ex)		((ex)->exp.X_op)
#define adds(ex)	((ex)->exp.X_add_symbol)
#define subs(ex)	((ex)->exp.X_op_symbol)
#define offs(ex)	((ex)->exp.X_add_number)

/* Macros for adding things to the m68k_it struct.  */
#define addword(w)	(the_ins.opcode[the_ins.numo++] = (w))

/* Like addword, but goes BEFORE general operands.  */

static void
insop (int w, const struct m68k_incant *opcode)
{
  int z;
  for (z = the_ins.numo; z > opcode->m_codenum; --z)
    the_ins.opcode[z] = the_ins.opcode[z - 1];
  for (z = 0; z < the_ins.nrel; z++)
    the_ins.reloc[z].n += 2;
  for (z = 0; z < the_ins.nfrag; z++)
    the_ins.fragb[z].fragoff++;
  the_ins.opcode[opcode->m_codenum] = w;
  the_ins.numo++;
}

/* The numo+1 kludge is so we can hit the low order byte of the prev word.
   Blecch.  */
static void
add_fix (int width, struct m68k_exp *exp, int pc_rel, int pc_fix)
{
  the_ins.reloc[the_ins.nrel].n = (width == 'B' || width == '3'
				   ? the_ins.numo * 2 - 1
				   : (width == 'b'
				      ? the_ins.numo * 2 + 1
				      : the_ins.numo * 2));
  the_ins.reloc[the_ins.nrel].exp = exp->exp;
  the_ins.reloc[the_ins.nrel].wid = width;
  the_ins.reloc[the_ins.nrel].pcrel_fix = pc_fix;
#ifdef OBJ_ELF
  the_ins.reloc[the_ins.nrel].pic_reloc = exp->pic_reloc;
#endif
  the_ins.reloc[the_ins.nrel++].pcrel = pc_rel;
}

/* Cause an extra frag to be generated here, inserting up to 10 bytes
   (that value is chosen in the frag_var call in md_assemble).  TYPE
   is the subtype of the frag to be generated; its primary type is
   rs_machine_dependent.

   The TYPE parameter is also used by md_convert_frag_1 and
   md_estimate_size_before_relax.  The appropriate type of fixup will
   be emitted by md_convert_frag_1.

   ADD becomes the FR_SYMBOL field of the frag, and OFF the FR_OFFSET.  */
static void
add_frag (symbolS *add, offsetT off, int type)
{
  the_ins.fragb[the_ins.nfrag].fragoff = the_ins.numo;
  the_ins.fragb[the_ins.nfrag].fadd = add;
  the_ins.fragb[the_ins.nfrag].foff = off;
  the_ins.fragb[the_ins.nfrag++].fragty = type;
}

#define isvar(ex) \
  (op (ex) != O_constant && op (ex) != O_big)

static char *crack_operand (char *str, struct m68k_op *opP);
static int get_num (struct m68k_exp *exp, int ok);
static int reverse_16_bits (int in);
static int reverse_8_bits (int in);
static void install_gen_operand (int mode, int val);
static void install_operand (int mode, int val);
static void s_bss (int);
static void s_data1 (int);
static void s_data2 (int);
static void s_even (int);
static void s_proc (int);
static void s_chip (int);
static void s_fopt (int);
static void s_opt (int);
static void s_reg (int);
static void s_restore (int);
static void s_save (int);
static void s_mri_if (int);
static void s_mri_else (int);
static void s_mri_endi (int);
static void s_mri_break (int);
static void s_mri_next (int);
static void s_mri_for (int);
static void s_mri_endf (int);
static void s_mri_repeat (int);
static void s_mri_until (int);
static void s_mri_while (int);
static void s_mri_endw (int);
static void s_m68k_cpu (int);
static void s_m68k_arch (int);

struct m68k_cpu
{
  unsigned long arch;	/* Architecture features.  */
  const enum m68k_register *control_regs;	/* Control regs on chip */
  const char *name;	/* Name */
  int alias;       	/* Alias for a cannonical name.  If 1, then
			   succeeds canonical name, if -1 then
			   succeeds canonical name, if <-1 ||>1 this is a
			   deprecated name, and the next/previous name
			   should be used. */
};

/* We hold flags for features explicitly enabled and explicitly
   disabled.  */
static int current_architecture;
static int not_current_architecture;
static const struct m68k_cpu *selected_arch;
static const struct m68k_cpu *selected_cpu;
static int initialized;

/* Architecture models.  */
static const struct m68k_cpu m68k_archs[] =
{
  {m68000,					m68000_ctrl, "68000", 0},
  {m68010,					m68010_ctrl, "68010", 0},
  {m68020|m68881|m68851,			m68020_ctrl, "68020", 0},
  {m68030|m68881|m68851,			m68020_ctrl, "68030", 0},
  {m68040,					m68040_ctrl, "68040", 0},
  {m68060,					m68060_ctrl, "68060", 0},
  {cpu32|m68881,				cpu32_ctrl, "cpu32", 0},
  {mcfisa_a|mcfhwdiv,				NULL, "isaa", 0},
  {mcfisa_a|mcfhwdiv|mcfisa_aa|mcfusp,		NULL, "isaaplus", 0},
  {mcfisa_a|mcfhwdiv|mcfisa_b|mcfusp,		NULL, "isab", 0},
  {mcfisa_a|mcfhwdiv|mcfisa_b|mcfmac|mcfusp,	mcf_ctrl, "cfv4", 0},
  {mcfisa_a|mcfhwdiv|mcfisa_b|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "cfv4e", 0},
  {0,0,NULL, 0}
};

/* Architecture extensions, here 'alias' -1 for m68k, +1 for cf and 0
   for either.  */
static const struct m68k_cpu m68k_extensions[] =
{
  {m68851,					NULL, "68851", -1},
  {m68881,					NULL, "68881", -1},
  {m68881,					NULL, "68882", -1},
  
  {cfloat|m68881,				NULL, "float", 0},
  
  {mcfhwdiv,					NULL, "div", 1},
  {mcfusp,					NULL, "usp", 1},
  {mcfmac,					NULL, "mac", 1},
  {mcfemac,					NULL, "emac", 1},
   
  {0,NULL,NULL, 0}
};

/* Processor list */
static const struct m68k_cpu m68k_cpus[] =
{
  {m68000,					m68000_ctrl, "68000", 0},
  {m68000,					m68000_ctrl, "68ec000", 1},
  {m68000,					m68000_ctrl, "68hc000", 1},
  {m68000,					m68000_ctrl, "68hc001", 1},
  {m68000,					m68000_ctrl, "68008", 1},
  {m68000,					m68000_ctrl, "68302", 1},
  {m68000,					m68000_ctrl, "68306", 1},
  {m68000,					m68000_ctrl, "68307", 1},
  {m68000,					m68000_ctrl, "68322", 1},
  {m68000,					m68000_ctrl, "68356", 1},
  {m68010,					m68010_ctrl, "68010", 0},
  {m68020|m68881|m68851,			m68020_ctrl, "68020", 0},
  {m68020|m68881|m68851,			m68020_ctrl, "68k", 1},
  {m68020|m68881|m68851,			m68020_ctrl, "68ec020", 1},
  {m68030|m68881|m68851,			m68020_ctrl, "68030", 0},
  {m68030|m68881|m68851,			m68020_ctrl, "68ec030", 1},
  {m68040,					m68040_ctrl, "68040", 0},
  {m68040,					m68040_ctrl, "68ec040", 1},
  {m68060,					m68060_ctrl, "68060", 0},
  {m68060,					m68060_ctrl, "68ec060", 1},
  
  {cpu32|m68881,				cpu32_ctrl, "cpu32",  0},
  {cpu32|m68881,				cpu32_ctrl, "68330", 1},
  {cpu32|m68881,				cpu32_ctrl, "68331", 1},
  {cpu32|m68881,				cpu32_ctrl, "68332", 1},
  {cpu32|m68881,				cpu32_ctrl, "68333", 1},
  {cpu32|m68881,				cpu32_ctrl, "68334", 1},
  {cpu32|m68881,				cpu32_ctrl, "68336", 1},
  {cpu32|m68881,				cpu32_ctrl, "68340", 1},
  {cpu32|m68881,				cpu32_ctrl, "68341", 1},
  {cpu32|m68881,				cpu32_ctrl, "68349", 1},
  {cpu32|m68881,				cpu32_ctrl, "68360", 1},
  
  {mcfisa_a,					mcf_ctrl, "5200", 0},
  {mcfisa_a,					mcf_ctrl, "5202", 1},
  {mcfisa_a,					mcf_ctrl, "5204", 1},
  {mcfisa_a,					mcf_ctrl, "5206", 1},
  
  {mcfisa_a|mcfhwdiv|mcfmac,			mcf_ctrl, "5206e", 0},
  
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5208_ctrl, "5207", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5208_ctrl, "5208", 0},
  
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfmac|mcfusp,	mcf5213_ctrl, "5211", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfmac|mcfusp,	mcf5213_ctrl, "5212", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfmac|mcfusp,	mcf5213_ctrl, "5213", 0},
  
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5216_ctrl, "5214", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5216_ctrl, "5216", 0},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5216_ctrl, "521x", 2},

  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5235_ctrl, "5232", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5235_ctrl, "5233", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5235_ctrl, "5234", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5235_ctrl, "5235", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5235_ctrl, "523x", 0},
  
  {mcfisa_a|mcfhwdiv|mcfemac,			mcf5249_ctrl, "5249", 0},
  {mcfisa_a|mcfhwdiv|mcfemac,			mcf5250_ctrl, "5250", 0},
  
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5271_ctrl, "5270", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5271_ctrl, "5271", 0},
  
  {mcfisa_a|mcfhwdiv|mcfmac,			mcf5272_ctrl, "5272", 0},
  
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5275_ctrl, "5274", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5275_ctrl, "5275", 0},
  
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5282_ctrl, "5280", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5282_ctrl, "5281", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5282_ctrl, "5282", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5282_ctrl, "528x", 0},
  
  {mcfisa_a|mcfhwdiv|mcfmac,			mcf_ctrl, "5307", 0},
  
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5329_ctrl, "5327", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5329_ctrl, "5328", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5329_ctrl, "5329", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5329_ctrl, "532x", 0},
  
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5373_ctrl, "5372", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5373_ctrl, "5373", -1},
  {mcfisa_a|mcfisa_aa|mcfhwdiv|mcfemac|mcfusp,	mcf5373_ctrl, "537x", 0},
  
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfmac,		mcf_ctrl, "5407",0},
  
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5470", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5471", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5472", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5473", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5474", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5475", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "547x", 0},
  
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5480", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5481", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5482", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5483", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5484", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "5485", -1},
  {mcfisa_a|mcfisa_b|mcfhwdiv|mcfemac|mcfusp|cfloat, mcfv4e_ctrl, "548x", 0},
  
  {0,NULL,NULL, 0}
  };

static const struct m68k_cpu *m68k_lookup_cpu
(const char *, const struct m68k_cpu *, int, int *);
static int m68k_set_arch (const char *, int, int);
static int m68k_set_cpu (const char *, int, int);
static int m68k_set_extension (const char *, int, int);
static void m68k_init_arch (void);

/* This is the assembler relaxation table for m68k. m68k is a rich CISC
   architecture and we have a lot of relaxation modes.  */

/* Macros used in the relaxation code.  */
#define TAB(x,y)	(((x) << 2) + (y))
#define TABTYPE(x)      ((x) >> 2)

/* Relaxation states.  */
#define BYTE		0
#define SHORT		1
#define LONG		2
#define SZ_UNDEF	3

/* Here are all the relaxation modes we support.  First we can relax ordinary
   branches.  On 68020 and higher and on CPU32 all branch instructions take
   three forms, so on these CPUs all branches always remain as such.  When we
   have to expand to the LONG form on a 68000, though, we substitute an
   absolute jump instead.  This is a direct replacement for unconditional
   branches and a branch over a jump for conditional branches.  However, if the
   user requires PIC and disables this with --pcrel, we can only relax between
   BYTE and SHORT forms, punting if that isn't enough.  This gives us four
   different relaxation modes for branches:  */

#define BRANCHBWL	0	/* Branch byte, word, or long.  */
#define BRABSJUNC	1	/* Absolute jump for LONG, unconditional.  */
#define BRABSJCOND	2	/* Absolute jump for LONG, conditional.  */
#define BRANCHBW	3	/* Branch byte or word.  */

/* We also relax coprocessor branches and DBcc's.  All CPUs that support
   coprocessor branches support them in word and long forms, so we have only
   one relaxation mode for them.  DBcc's are word only on all CPUs.  We can
   relax them to the LONG form with a branch-around sequence.  This sequence
   can use a long branch (if available) or an absolute jump (if acceptable).
   This gives us two relaxation modes.  If long branches are not available and
   absolute jumps are not acceptable, we don't relax DBcc's.  */

#define FBRANCH		4	/* Coprocessor branch.  */
#define DBCCLBR		5	/* DBcc relaxable with a long branch.  */
#define DBCCABSJ	6	/* DBcc relaxable with an absolute jump.  */

/* That's all for instruction relaxation.  However, we also relax PC-relative
   operands.  Specifically, we have three operand relaxation modes.  On the
   68000 PC-relative operands can only be 16-bit, but on 68020 and higher and
   on CPU32 they may be 16-bit or 32-bit.  For the latter we relax between the
   two.  Also PC+displacement+index operands in their simple form (with a non-
   suppressed index without memory indirection) are supported on all CPUs, but
   on the 68000 the displacement can be 8-bit only, whereas on 68020 and higher
   and on CPU32 we relax it to SHORT and LONG forms as well using the extended
   form of the PC+displacement+index operand.  Finally, some absolute operands
   can be relaxed down to 16-bit PC-relative.  */

#define PCREL1632	7	/* 16-bit or 32-bit PC-relative.  */
#define PCINDEX		8	/* PC + displacement + index. */
#define ABSTOPCREL	9	/* Absolute relax down to 16-bit PC-relative.  */

/* Note that calls to frag_var need to specify the maximum expansion
   needed; this is currently 10 bytes for DBCC.  */

/* The fields are:
   How far Forward this mode will reach:
   How far Backward this mode will reach:
   How many bytes this mode will add to the size of the frag
   Which mode to go to if the offset won't fit in this one

   Please check tc-m68k.h:md_prepare_relax_scan if changing this table.  */
relax_typeS md_relax_table[] =
{
  {   127,   -128,  0, TAB (BRANCHBWL, SHORT) },
  { 32767, -32768,  2, TAB (BRANCHBWL, LONG) },
  {     0,	0,  4, 0 },
  {     1,	1,  0, 0 },

  {   127,   -128,  0, TAB (BRABSJUNC, SHORT) },
  { 32767, -32768,  2, TAB (BRABSJUNC, LONG) },
  {	0,	0,  4, 0 },
  {	1,	1,  0, 0 },

  {   127,   -128,  0, TAB (BRABSJCOND, SHORT) },
  { 32767, -32768,  2, TAB (BRABSJCOND, LONG) },
  {	0,	0,  6, 0 },
  {	1,	1,  0, 0 },

  {   127,   -128,  0, TAB (BRANCHBW, SHORT) },
  {	0,	0,  2, 0 },
  {	1,	1,  0, 0 },
  {	1,	1,  0, 0 },

  {	1, 	1,  0, 0 },		/* FBRANCH doesn't come BYTE.  */
  { 32767, -32768,  2, TAB (FBRANCH, LONG) },
  {	0,	0,  4, 0 },
  {	1, 	1,  0, 0 },

  {	1,	1,  0, 0 },		/* DBCC doesn't come BYTE.  */
  { 32767, -32768,  2, TAB (DBCCLBR, LONG) },
  {	0,	0, 10, 0 },
  {	1,	1,  0, 0 },

  {	1,	1,  0, 0 },		/* DBCC doesn't come BYTE.  */
  { 32767, -32768,  2, TAB (DBCCABSJ, LONG) },
  {	0,	0, 10, 0 },
  {	1,	1,  0, 0 },

  {	1, 	1,  0, 0 },		/* PCREL1632 doesn't come BYTE.  */
  { 32767, -32768,  2, TAB (PCREL1632, LONG) },
  {	0,	0,  6, 0 },
  {	1,	1,  0, 0 },

  {   125,   -130,  0, TAB (PCINDEX, SHORT) },
  { 32765, -32770,  2, TAB (PCINDEX, LONG) },
  {	0,	0,  4, 0 },
  {	1,	1,  0, 0 },

  {	1,	1,  0, 0 },		/* ABSTOPCREL doesn't come BYTE.  */
  { 32767, -32768,  2, TAB (ABSTOPCREL, LONG) },
  {	0,	0,  4, 0 },
  {	1,	1,  0, 0 },
};

/* These are the machine dependent pseudo-ops.  These are included so
   the assembler can work on the output from the SUN C compiler, which
   generates these.  */

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function.  */
const pseudo_typeS md_pseudo_table[] =
{
  {"data1", s_data1, 0},
  {"data2", s_data2, 0},
  {"bss", s_bss, 0},
  {"even", s_even, 0},
  {"skip", s_space, 0},
  {"proc", s_proc, 0},
#if defined (TE_SUN3) || defined (OBJ_ELF)
  {"align", s_align_bytes, 0},
#endif
#ifdef OBJ_ELF
  {"swbeg", s_ignore, 0},
#endif
  {"extend", float_cons, 'x'},
  {"ldouble", float_cons, 'x'},

  {"arch", s_m68k_arch, 0},
  {"cpu", s_m68k_cpu, 0},

  /* The following pseudo-ops are supported for MRI compatibility.  */
  {"chip", s_chip, 0},
  {"comline", s_space, 1},
  {"fopt", s_fopt, 0},
  {"mask2", s_ignore, 0},
  {"opt", s_opt, 0},
  {"reg", s_reg, 0},
  {"restore", s_restore, 0},
  {"save", s_save, 0},

  {"if", s_mri_if, 0},
  {"if.b", s_mri_if, 'b'},
  {"if.w", s_mri_if, 'w'},
  {"if.l", s_mri_if, 'l'},
  {"else", s_mri_else, 0},
  {"else.s", s_mri_else, 's'},
  {"else.l", s_mri_else, 'l'},
  {"endi", s_mri_endi, 0},
  {"break", s_mri_break, 0},
  {"break.s", s_mri_break, 's'},
  {"break.l", s_mri_break, 'l'},
  {"next", s_mri_next, 0},
  {"next.s", s_mri_next, 's'},
  {"next.l", s_mri_next, 'l'},
  {"for", s_mri_for, 0},
  {"for.b", s_mri_for, 'b'},
  {"for.w", s_mri_for, 'w'},
  {"for.l", s_mri_for, 'l'},
  {"endf", s_mri_endf, 0},
  {"repeat", s_mri_repeat, 0},
  {"until", s_mri_until, 0},
  {"until.b", s_mri_until, 'b'},
  {"until.w", s_mri_until, 'w'},
  {"until.l", s_mri_until, 'l'},
  {"while", s_mri_while, 0},
  {"while.b", s_mri_while, 'b'},
  {"while.w", s_mri_while, 'w'},
  {"while.l", s_mri_while, 'l'},
  {"endw", s_mri_endw, 0},

  {0, 0, 0}
};

/* The mote pseudo ops are put into the opcode table, since they
   don't start with a . they look like opcodes to gas.  */

const pseudo_typeS mote_pseudo_table[] =
{

  {"dcl", cons, 4},
  {"dc", cons, 2},
  {"dcw", cons, 2},
  {"dcb", cons, 1},

  {"dsl", s_space, 4},
  {"ds", s_space, 2},
  {"dsw", s_space, 2},
  {"dsb", s_space, 1},

  {"xdef", s_globl, 0},
#ifdef OBJ_ELF
  {"align", s_align_bytes, 0},
#else
  {"align", s_align_ptwo, 0},
#endif
#ifdef M68KCOFF
  {"sect", obj_coff_section, 0},
  {"section", obj_coff_section, 0},
#endif
  {0, 0, 0}
};

/* Truncate and sign-extend at 32 bits, so that building on a 64-bit host
   gives identical results to a 32-bit host.  */
#define TRUNC(X)	((valueT) (X) & 0xffffffff)
#define SEXT(X)		((TRUNC (X) ^ 0x80000000) - 0x80000000)

#define issbyte(x)	((valueT) SEXT (x) + 0x80 < 0x100)
#define isubyte(x)	((valueT) TRUNC (x) < 0x100)
#define issword(x)	((valueT) SEXT (x) + 0x8000 < 0x10000)
#define isuword(x)	((valueT) TRUNC (x) < 0x10000)

#define isbyte(x)	((valueT) SEXT (x) + 0xff < 0x1ff)
#define isword(x)	((valueT) SEXT (x) + 0xffff < 0x1ffff)
#define islong(x)	(1)

static char notend_table[256];
static char alt_notend_table[256];
#define notend(s)						\
  (! (notend_table[(unsigned char) *s]				\
      || (*s == ':'						\
	  && alt_notend_table[(unsigned char) s[1]])))

#ifdef OBJ_ELF

/* Return zero if the reference to SYMBOL from within the same segment may
   be relaxed.  */

/* On an ELF system, we can't relax an externally visible symbol,
   because it may be overridden by a shared library.  However, if
   TARGET_OS is "elf", then we presume that we are assembling for an
   embedded system, in which case we don't have to worry about shared
   libraries, and we can relax any external sym.  */

#define relaxable_symbol(symbol) \
  (!((S_IS_EXTERNAL (symbol) && EXTERN_FORCE_RELOC) \
     || S_IS_WEAK (symbol)))

/* Compute the relocation code for a fixup of SIZE bytes, using pc
   relative relocation if PCREL is non-zero.  PIC says whether a special
   pic relocation was requested.  */

static bfd_reloc_code_real_type
get_reloc_code (int size, int pcrel, enum pic_relocation pic)
{
  switch (pic)
    {
    case pic_got_pcrel:
      switch (size)
	{
	case 1:
	  return BFD_RELOC_8_GOT_PCREL;
	case 2:
	  return BFD_RELOC_16_GOT_PCREL;
	case 4:
	  return BFD_RELOC_32_GOT_PCREL;
	}
      break;

    case pic_got_off:
      switch (size)
	{
	case 1:
	  return BFD_RELOC_8_GOTOFF;
	case 2:
	  return BFD_RELOC_16_GOTOFF;
	case 4:
	  return BFD_RELOC_32_GOTOFF;
	}
      break;

    case pic_plt_pcrel:
      switch (size)
	{
	case 1:
	  return BFD_RELOC_8_PLT_PCREL;
	case 2:
	  return BFD_RELOC_16_PLT_PCREL;
	case 4:
	  return BFD_RELOC_32_PLT_PCREL;
	}
      break;

    case pic_plt_off:
      switch (size)
	{
	case 1:
	  return BFD_RELOC_8_PLTOFF;
	case 2:
	  return BFD_RELOC_16_PLTOFF;
	case 4:
	  return BFD_RELOC_32_PLTOFF;
	}
      break;

    case pic_none:
      if (pcrel)
	{
	  switch (size)
	    {
	    case 1:
	      return BFD_RELOC_8_PCREL;
	    case 2:
	      return BFD_RELOC_16_PCREL;
	    case 4:
	      return BFD_RELOC_32_PCREL;
	    }
	}
      else
	{
	  switch (size)
	    {
	    case 1:
	      return BFD_RELOC_8;
	    case 2:
	      return BFD_RELOC_16;
	    case 4:
	      return BFD_RELOC_32;
	    }
	}
    }

  if (pcrel)
    {
      if (pic == pic_none)
	as_bad (_("Can not do %d byte pc-relative relocation"), size);
      else
	as_bad (_("Can not do %d byte pc-relative pic relocation"), size);
    }
  else
    {
      if (pic == pic_none)
	as_bad (_("Can not do %d byte relocation"), size);
      else
	as_bad (_("Can not do %d byte pic relocation"), size);
    }

  return BFD_RELOC_NONE;
}

/* Here we decide which fixups can be adjusted to make them relative
   to the beginning of the section instead of the symbol.  Basically
   we need to make sure that the dynamic relocations are done
   correctly, so in some cases we force the original symbol to be
   used.  */
int
tc_m68k_fix_adjustable (fixS *fixP)
{
  /* Adjust_reloc_syms doesn't know about the GOT.  */
  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_8_GOT_PCREL:
    case BFD_RELOC_16_GOT_PCREL:
    case BFD_RELOC_32_GOT_PCREL:
    case BFD_RELOC_8_GOTOFF:
    case BFD_RELOC_16_GOTOFF:
    case BFD_RELOC_32_GOTOFF:
    case BFD_RELOC_8_PLT_PCREL:
    case BFD_RELOC_16_PLT_PCREL:
    case BFD_RELOC_32_PLT_PCREL:
    case BFD_RELOC_8_PLTOFF:
    case BFD_RELOC_16_PLTOFF:
    case BFD_RELOC_32_PLTOFF:
      return 0;

    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      return 0;

    default:
      return 1;
    }
}

#else /* !OBJ_ELF */

#define get_reloc_code(SIZE,PCREL,OTHER) NO_RELOC

#define relaxable_symbol(symbol) 1

#endif /* OBJ_ELF */

arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *reloc;
  bfd_reloc_code_real_type code;

  /* If the tcbit is set, then this was a fixup of a negative value
     that was never resolved.  We do not have a reloc to handle this,
     so just return.  We assume that other code will have detected this
     situation and produced a helpful error message, so we just tell the
     user that the reloc cannot be produced.  */
  if (fixp->fx_tcbit)
    {
      if (fixp->fx_addsy)
	as_bad_where (fixp->fx_file, fixp->fx_line,
		      _("Unable to produce reloc against symbol '%s'"),
		      S_GET_NAME (fixp->fx_addsy));
      return NULL;
    }

  if (fixp->fx_r_type != BFD_RELOC_NONE)
    {
      code = fixp->fx_r_type;

      /* Since DIFF_EXPR_OK is defined in tc-m68k.h, it is possible
         that fixup_segment converted a non-PC relative reloc into a
         PC relative reloc.  In such a case, we need to convert the
         reloc code.  */
      if (fixp->fx_pcrel)
	{
	  switch (code)
	    {
	    case BFD_RELOC_8:
	      code = BFD_RELOC_8_PCREL;
	      break;
	    case BFD_RELOC_16:
	      code = BFD_RELOC_16_PCREL;
	      break;
	    case BFD_RELOC_32:
	      code = BFD_RELOC_32_PCREL;
	      break;
	    case BFD_RELOC_8_PCREL:
	    case BFD_RELOC_16_PCREL:
	    case BFD_RELOC_32_PCREL:
	    case BFD_RELOC_8_GOT_PCREL:
	    case BFD_RELOC_16_GOT_PCREL:
	    case BFD_RELOC_32_GOT_PCREL:
	    case BFD_RELOC_8_GOTOFF:
	    case BFD_RELOC_16_GOTOFF:
	    case BFD_RELOC_32_GOTOFF:
	    case BFD_RELOC_8_PLT_PCREL:
	    case BFD_RELOC_16_PLT_PCREL:
	    case BFD_RELOC_32_PLT_PCREL:
	    case BFD_RELOC_8_PLTOFF:
	    case BFD_RELOC_16_PLTOFF:
	    case BFD_RELOC_32_PLTOFF:
	      break;
	    default:
	      as_bad_where (fixp->fx_file, fixp->fx_line,
			    _("Cannot make %s relocation PC relative"),
			    bfd_get_reloc_code_name (code));
	    }
	}
    }
  else
    {
#define F(SZ,PCREL)		(((SZ) << 1) + (PCREL))
      switch (F (fixp->fx_size, fixp->fx_pcrel))
	{
#define MAP(SZ,PCREL,TYPE)	case F(SZ,PCREL): code = (TYPE); break
	  MAP (1, 0, BFD_RELOC_8);
	  MAP (2, 0, BFD_RELOC_16);
	  MAP (4, 0, BFD_RELOC_32);
	  MAP (1, 1, BFD_RELOC_8_PCREL);
	  MAP (2, 1, BFD_RELOC_16_PCREL);
	  MAP (4, 1, BFD_RELOC_32_PCREL);
	default:
	  abort ();
	}
    }
#undef F
#undef MAP

  reloc = (arelent *) xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
#ifndef OBJ_ELF
  if (fixp->fx_pcrel)
    reloc->addend = fixp->fx_addnumber;
  else
    reloc->addend = 0;
#else
  if (!fixp->fx_pcrel)
    reloc->addend = fixp->fx_addnumber;
  else
    reloc->addend = (section->vma
		     /* Explicit sign extension in case char is
			unsigned.  */
		     + ((fixp->fx_pcrel_adjust & 0xff) ^ 0x80) - 0x80
		     + fixp->fx_addnumber
		     + md_pcrel_from (fixp));
#endif

  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);
  assert (reloc->howto != 0);

  return reloc;
}

/* Handle of the OPCODE hash table.  NULL means any use before
   m68k_ip_begin() will crash.  */
static struct hash_control *op_hash;

/* Assemble an m68k instruction.  */

static void
m68k_ip (char *instring)
{
  register char *p;
  register struct m68k_op *opP;
  register const struct m68k_incant *opcode;
  register const char *s;
  register int tmpreg = 0, baseo = 0, outro = 0, nextword;
  char *pdot, *pdotmove;
  enum m68k_size siz1, siz2;
  char c;
  int losing;
  int opsfound;
  struct m68k_op operands_backup[6];
  LITTLENUM_TYPE words[6];
  LITTLENUM_TYPE *wordp;
  unsigned long ok_arch = 0;

  if (*instring == ' ')
    instring++;			/* Skip leading whitespace.  */

  /* Scan up to end of operation-code, which MUST end in end-of-string
     or exactly 1 space.  */
  pdot = 0;
  for (p = instring; *p != '\0'; p++)
    {
      if (*p == ' ')
	break;
      if (*p == '.')
	pdot = p;
    }

  if (p == instring)
    {
      the_ins.error = _("No operator");
      return;
    }

  /* p now points to the end of the opcode name, probably whitespace.
     Make sure the name is null terminated by clobbering the
     whitespace, look it up in the hash table, then fix it back.
     Remove a dot, first, since the opcode tables have none.  */
  if (pdot != NULL)
    {
      for (pdotmove = pdot; pdotmove < p; pdotmove++)
	*pdotmove = pdotmove[1];
      p--;
    }

  c = *p;
  *p = '\0';
  opcode = (const struct m68k_incant *) hash_find (op_hash, instring);
  *p = c;

  if (pdot != NULL)
    {
      for (pdotmove = p; pdotmove > pdot; pdotmove--)
	*pdotmove = pdotmove[-1];
      *pdot = '.';
      ++p;
    }

  if (opcode == NULL)
    {
      the_ins.error = _("Unknown operator");
      return;
    }

  /* Found a legitimate opcode, start matching operands.  */
  while (*p == ' ')
    ++p;

  if (opcode->m_operands == 0)
    {
      char *old = input_line_pointer;
      *old = '\n';
      input_line_pointer = p;
      /* Ahh - it's a motorola style psuedo op.  */
      mote_pseudo_table[opcode->m_opnum].poc_handler
	(mote_pseudo_table[opcode->m_opnum].poc_val);
      input_line_pointer = old;
      *old = 0;

      return;
    }

  if (flag_mri && opcode->m_opnum == 0)
    {
      /* In MRI mode, random garbage is allowed after an instruction
         which accepts no operands.  */
      the_ins.args = opcode->m_operands;
      the_ins.numargs = opcode->m_opnum;
      the_ins.numo = opcode->m_codenum;
      the_ins.opcode[0] = getone (opcode);
      the_ins.opcode[1] = gettwo (opcode);
      return;
    }

  for (opP = &the_ins.operands[0]; *p; opP++)
    {
      p = crack_operand (p, opP);

      if (opP->error)
	{
	  the_ins.error = opP->error;
	  return;
	}
    }

  opsfound = opP - &the_ins.operands[0];

  /* This ugly hack is to support the floating pt opcodes in their
     standard form.  Essentially, we fake a first enty of type COP#1 */
  if (opcode->m_operands[0] == 'I')
    {
      int n;

      for (n = opsfound; n > 0; --n)
	the_ins.operands[n] = the_ins.operands[n - 1];

      memset (&the_ins.operands[0], '\0', sizeof (the_ins.operands[0]));
      the_ins.operands[0].mode = CONTROL;
      the_ins.operands[0].reg = m68k_float_copnum;
      opsfound++;
    }

  /* We've got the operands.  Find an opcode that'll accept them.  */
  for (losing = 0;;)
    {
      /* If we didn't get the right number of ops, or we have no
	 common model with this pattern then reject this pattern.  */

      ok_arch |= opcode->m_arch;
      if (opsfound != opcode->m_opnum
	  || ((opcode->m_arch & current_architecture) == 0))
	++losing;
      else
	{
	  int i;

	  /* Make a copy of the operands of this insn so that
	     we can modify them safely, should we want to.  */
	  assert (opsfound <= (int) ARRAY_SIZE (operands_backup));
	  for (i = 0; i < opsfound; i++)
	    operands_backup[i] = the_ins.operands[i];

	  for (s = opcode->m_operands, opP = &operands_backup[0];
	       *s && !losing;
	       s += 2, opP++)
	    {
	      /* Warning: this switch is huge! */
	      /* I've tried to organize the cases into this order:
		 non-alpha first, then alpha by letter.  Lower-case
		 goes directly before uppercase counterpart.  */
	      /* Code with multiple case ...: gets sorted by the lowest
		 case ... it belongs to.  I hope this makes sense.  */
	      switch (*s)
		{
		case '!':
		  switch (opP->mode)
		    {
		    case IMMED:
		    case DREG:
		    case AREG:
		    case FPREG:
		    case CONTROL:
		    case AINC:
		    case ADEC:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case '<':
		  switch (opP->mode)
		    {
		    case DREG:
		    case AREG:
		    case FPREG:
		    case CONTROL:
		    case IMMED:
		    case ADEC:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case '>':
		  switch (opP->mode)
		    {
		    case DREG:
		    case AREG:
		    case FPREG:
		    case CONTROL:
		    case IMMED:
		    case AINC:
		    case REGLST:
		      losing++;
		      break;
		    case ABSL:
		      break;
		    default:
		      if (opP->reg == PC
			  || opP->reg == ZPC)
			losing++;
		      break;
		    }
		  break;

		case 'm':
		  switch (opP->mode)
		    {
		    case DREG:
		    case AREG:
		    case AINDR:
		    case AINC:
		    case ADEC:
		      break;
		    default:
		      losing++;
		    }
		  break;

		case 'n':
		  switch (opP->mode)
		    {
		    case DISP:
		      break;
		    default:
		      losing++;
		    }
		  break;

		case 'o':
		  switch (opP->mode)
		    {
		    case BASE:
		    case ABSL:
		    case IMMED:
		      break;
		    default:
		      losing++;
		    }
		  break;

		case 'p':
		  switch (opP->mode)
		    {
		    case DREG:
		    case AREG:
		    case AINDR:
		    case AINC:
		    case ADEC:
		      break;
		    case DISP:
		      if (opP->reg == PC || opP->reg == ZPC)
			losing++;
		      break;
		    default:
		      losing++;
		    }
		  break;

		case 'q':
		  switch (opP->mode)
		    {
		    case DREG:
		    case AINDR:
		    case AINC:
		    case ADEC:
		      break;
		    case DISP:
		      if (opP->reg == PC || opP->reg == ZPC)
			losing++;
		      break;
		    default:
		      losing++;
		      break;
		    }
		  break;

		case 'v':
		  switch (opP->mode)
		    {
		    case DREG:
		    case AINDR:
		    case AINC:
		    case ADEC:
		    case ABSL:
		      break;
		    case DISP:
		      if (opP->reg == PC || opP->reg == ZPC)
			losing++;
		      break;
		    default:
		      losing++;
		      break;
		    }
		  break;

		case '#':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (s[1] == 'b'
			   && ! isvar (&opP->disp)
			   && (opP->disp.exp.X_op != O_constant
			       || ! isbyte (opP->disp.exp.X_add_number)))
		    losing++;
		  else if (s[1] == 'B'
			   && ! isvar (&opP->disp)
			   && (opP->disp.exp.X_op != O_constant
			       || ! issbyte (opP->disp.exp.X_add_number)))
		    losing++;
		  else if (s[1] == 'w'
			   && ! isvar (&opP->disp)
			   && (opP->disp.exp.X_op != O_constant
			       || ! isword (opP->disp.exp.X_add_number)))
		    losing++;
		  else if (s[1] == 'W'
			   && ! isvar (&opP->disp)
			   && (opP->disp.exp.X_op != O_constant
			       || ! issword (opP->disp.exp.X_add_number)))
		    losing++;
		  break;

		case '^':
		case 'T':
		  if (opP->mode != IMMED)
		    losing++;
		  break;

		case '$':
		  if (opP->mode == AREG
		      || opP->mode == CONTROL
		      || opP->mode == FPREG
		      || opP->mode == IMMED
		      || opP->mode == REGLST
		      || (opP->mode != ABSL
			  && (opP->reg == PC
			      || opP->reg == ZPC)))
		    losing++;
		  break;

		case '%':
		  if (opP->mode == CONTROL
		      || opP->mode == FPREG
		      || opP->mode == REGLST
		      || opP->mode == IMMED
		      || (opP->mode != ABSL
			  && (opP->reg == PC
			      || opP->reg == ZPC)))
		    losing++;
		  break;

		case '&':
		  switch (opP->mode)
		    {
		    case DREG:
		    case AREG:
		    case FPREG:
		    case CONTROL:
		    case IMMED:
		    case AINC:
		    case ADEC:
		    case REGLST:
		      losing++;
		      break;
		    case ABSL:
		      break;
		    default:
		      if (opP->reg == PC
			  || opP->reg == ZPC)
			losing++;
		      break;
		    }
		  break;

		case '*':
		  if (opP->mode == CONTROL
		      || opP->mode == FPREG
		      || opP->mode == REGLST)
		    losing++;
		  break;

		case '+':
		  if (opP->mode != AINC)
		    losing++;
		  break;

		case '-':
		  if (opP->mode != ADEC)
		    losing++;
		  break;

		case '/':
		  switch (opP->mode)
		    {
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case AINC:
		    case ADEC:
		    case IMMED:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case ';':
		  switch (opP->mode)
		    {
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case '?':
		  switch (opP->mode)
		    {
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case AINC:
		    case ADEC:
		    case IMMED:
		    case REGLST:
		      losing++;
		      break;
		    case ABSL:
		      break;
		    default:
		      if (opP->reg == PC || opP->reg == ZPC)
			losing++;
		      break;
		    }
		  break;

		case '@':
		  switch (opP->mode)
		    {
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case IMMED:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case '~':	/* For now! (JF FOO is this right?) */
		  switch (opP->mode)
		    {
		    case DREG:
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case IMMED:
		    case REGLST:
		      losing++;
		      break;
		    case ABSL:
		      break;
		    default:
		      if (opP->reg == PC
			  || opP->reg == ZPC)
			losing++;
		      break;
		    }
		  break;

		case '3':
		  if (opP->mode != CONTROL
		      || (opP->reg != TT0 && opP->reg != TT1))
		    losing++;
		  break;

		case 'A':
		  if (opP->mode != AREG)
		    losing++;
		  break;

		case 'a':
		  if (opP->mode != AINDR)
		    ++losing;
		  break;

		case '4':
		  if (opP->mode != AINDR && opP->mode != AINC && opP->mode != ADEC
		      && (opP->mode != DISP
			   || opP->reg < ADDR0
			   || opP->reg > ADDR7))
		    ++losing;
		  break;

		case 'B':	/* FOO */
		  if (opP->mode != ABSL
		      || (flag_long_jumps
			  && strncmp (instring, "jbsr", 4) == 0))
		    losing++;
		  break;

                case 'b':
                  switch (opP->mode)
                    {
                    case IMMED:
                    case ABSL:
                    case AREG:
                    case FPREG:
                    case CONTROL:
                    case POST:
                    case PRE:
                    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
                    }
                  break;

		case 'C':
		  if (opP->mode != CONTROL || opP->reg != CCR)
		    losing++;
		  break;

		case 'd':
		  if (opP->mode != DISP
		      || opP->reg < ADDR0
		      || opP->reg > ADDR7)
		    losing++;
		  break;

		case 'D':
		  if (opP->mode != DREG)
		    losing++;
		  break;

		case 'E':
		  if (opP->reg != ACC)
		    losing++;
		  break;

		case 'e':
		  if (opP->reg != ACC && opP->reg != ACC1
		      && opP->reg != ACC2 && opP->reg != ACC3)
		    losing++;
		  break;

		case 'F':
		  if (opP->mode != FPREG)
		    losing++;
		  break;

		case 'G':
		  if (opP->reg != MACSR)
		    losing++;
		  break;

		case 'g':
		  if (opP->reg != ACCEXT01 && opP->reg != ACCEXT23)
		    losing++;
		  break;

		case 'H':
		  if (opP->reg != MASK)
		    losing++;
		  break;

		case 'I':
		  if (opP->mode != CONTROL
		      || opP->reg < COP0
		      || opP->reg > COP7)
		    losing++;
		  break;

		case 'i':
		  if (opP->mode != LSH && opP->mode != RSH)
		    losing++;
		  break;

		case 'J':
		  if (opP->mode != CONTROL
		      || opP->reg < USP
		      || opP->reg > last_movec_reg
		      || !control_regs)
		    losing++;
		  else
		    {
		      const enum m68k_register *rp;
		      
		      for (rp = control_regs; *rp; rp++)
			if (*rp == opP->reg)
			  break;
		      if (*rp == 0)
			losing++;
		    }
		  break;

		case 'k':
		  if (opP->mode != IMMED)
		    losing++;
		  break;

		case 'l':
		case 'L':
		  if (opP->mode == DREG
		      || opP->mode == AREG
		      || opP->mode == FPREG)
		    {
		      if (s[1] == '8')
			losing++;
		      else
			{
			  switch (opP->mode)
			    {
			    case DREG:
			      opP->mask = 1 << (opP->reg - DATA0);
			      break;
			    case AREG:
			      opP->mask = 1 << (opP->reg - ADDR0 + 8);
			      break;
			    case FPREG:
			      opP->mask = 1 << (opP->reg - FP0 + 16);
			      break;
			    default:
			      abort ();
			    }
			  opP->mode = REGLST;
			}
		    }
		  else if (opP->mode == CONTROL)
		    {
		      if (s[1] != '8')
			losing++;
		      else
			{
			  switch (opP->reg)
			    {
			    case FPI:
			      opP->mask = 1 << 24;
			      break;
			    case FPS:
			      opP->mask = 1 << 25;
			      break;
			    case FPC:
			      opP->mask = 1 << 26;
			      break;
			    default:
			      losing++;
			      break;
			    }
			  opP->mode = REGLST;
			}
		    }
		  else if (opP->mode != REGLST)
		    losing++;
		  else if (s[1] == '8' && (opP->mask & 0x0ffffff) != 0)
		    losing++;
		  else if (s[1] == '3' && (opP->mask & 0x7000000) != 0)
		    losing++;
		  break;

		case 'M':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (opP->disp.exp.X_op != O_constant
			   || ! issbyte (opP->disp.exp.X_add_number))
		    losing++;
		  else if (! m68k_quick
			   && instring[3] != 'q'
			   && instring[4] != 'q')
		    losing++;
		  break;

		case 'O':
		  if (opP->mode != DREG
		      && opP->mode != IMMED
		      && opP->mode != ABSL)
		    losing++;
		  break;

		case 'Q':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (opP->disp.exp.X_op != O_constant
			   || TRUNC (opP->disp.exp.X_add_number) - 1 > 7)
		    losing++;
		  else if (! m68k_quick
			   && (strncmp (instring, "add", 3) == 0
			       || strncmp (instring, "sub", 3) == 0)
			   && instring[3] != 'q')
		    losing++;
		  break;

		case 'R':
		  if (opP->mode != DREG && opP->mode != AREG)
		    losing++;
		  break;

		case 'r':
		  if (opP->mode != AINDR
		      && (opP->mode != BASE
			  || (opP->reg != 0
			      && opP->reg != ZADDR0)
			  || opP->disp.exp.X_op != O_absent
			  || ((opP->index.reg < DATA0
			       || opP->index.reg > DATA7)
			      && (opP->index.reg < ADDR0
				  || opP->index.reg > ADDR7))
			  || opP->index.size != SIZE_UNSPEC
			  || opP->index.scale != 1))
		    losing++;
		  break;

		case 's':
		  if (opP->mode != CONTROL
		      || ! (opP->reg == FPI
			    || opP->reg == FPS
			    || opP->reg == FPC))
		    losing++;
		  break;

		case 'S':
		  if (opP->mode != CONTROL || opP->reg != SR)
		    losing++;
		  break;

		case 't':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (opP->disp.exp.X_op != O_constant
			   || TRUNC (opP->disp.exp.X_add_number) > 7)
		    losing++;
		  break;

		case 'U':
		  if (opP->mode != CONTROL || opP->reg != USP)
		    losing++;
		  break;

		case 'x':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (opP->disp.exp.X_op != O_constant
			   || (TRUNC (opP->disp.exp.X_add_number) != 0xffffffff
			       && TRUNC (opP->disp.exp.X_add_number) - 1 > 6))
		    losing++;
		  break;

		  /* JF these are out of order.  We could put them
		     in order if we were willing to put up with
		     bunches of #ifdef m68851s in the code.

		     Don't forget that you need these operands
		     to use 68030 MMU instructions.  */
#ifndef NO_68851
		  /* Memory addressing mode used by pflushr.  */
		case '|':
		  if (opP->mode == CONTROL
		      || opP->mode == FPREG
		      || opP->mode == DREG
		      || opP->mode == AREG
		      || opP->mode == REGLST)
		    losing++;
		  /* We should accept immediate operands, but they
                     supposedly have to be quad word, and we don't
                     handle that.  I would like to see what a Motorola
                     assembler does before doing something here.  */
		  if (opP->mode == IMMED)
		    losing++;
		  break;

		case 'f':
		  if (opP->mode != CONTROL
		      || (opP->reg != SFC && opP->reg != DFC))
		    losing++;
		  break;

		case '0':
		  if (opP->mode != CONTROL || opP->reg != TC)
		    losing++;
		  break;

		case '1':
		  if (opP->mode != CONTROL || opP->reg != AC)
		    losing++;
		  break;

		case '2':
		  if (opP->mode != CONTROL
		      || (opP->reg != CAL
			  && opP->reg != VAL
			  && opP->reg != SCC))
		    losing++;
		  break;

		case 'V':
		  if (opP->mode != CONTROL
		      || opP->reg != VAL)
		    losing++;
		  break;

		case 'W':
		  if (opP->mode != CONTROL
		      || (opP->reg != DRP
			  && opP->reg != SRP
			  && opP->reg != CRP))
		    losing++;
		  break;

		case 'w':
		  switch (opP->mode)
		    {
		      case IMMED:
		      case ABSL:
		      case AREG:
		      case DREG:
		      case FPREG:
		      case CONTROL:
		      case POST:
		      case PRE:
		      case REGLST:
			losing++;
			break;
		      default:
			break;
		    }
		  break;

		case 'X':
		  if (opP->mode != CONTROL
		      || (!(opP->reg >= BAD && opP->reg <= BAD + 7)
			  && !(opP->reg >= BAC && opP->reg <= BAC + 7)))
		    losing++;
		  break;

		case 'Y':
		  if (opP->mode != CONTROL || opP->reg != PSR)
		    losing++;
		  break;

		case 'Z':
		  if (opP->mode != CONTROL || opP->reg != PCSR)
		    losing++;
		  break;
#endif
		case 'c':
		  if (opP->mode != CONTROL
		      || (opP->reg != NC
			  && opP->reg != IC
			  && opP->reg != DC
			  && opP->reg != BC))
		    losing++;
		  break;

		case '_':
		  if (opP->mode != ABSL)
		    ++losing;
		  break;

		case 'u':
		  if (opP->reg < DATA0L || opP->reg > ADDR7U)
		    losing++;
		  /* FIXME: kludge instead of fixing parser:
                     upper/lower registers are *not* CONTROL
                     registers, but ordinary ones.  */
		  if ((opP->reg >= DATA0L && opP->reg <= DATA7L)
		      || (opP->reg >= DATA0U && opP->reg <= DATA7U))
		    opP->mode = DREG;
		  else
		    opP->mode = AREG;
		  break;

		 case 'y':
		   if (!(opP->mode == AINDR
			 || (opP->mode == DISP
			     && !(opP->reg == PC || opP->reg == ZPC))))
		     losing++;
		   break;

		 case 'z':
		   if (!(opP->mode == AINDR || opP->mode == DISP))
		     losing++;
		   break;

		default:
		  abort ();
		}

	      if (losing)
		break;
	    }

	  /* Since we have found the correct instruction, copy
	     in the modifications that we may have made.  */
	  if (!losing)
	    for (i = 0; i < opsfound; i++)
	      the_ins.operands[i] = operands_backup[i];
	}

      if (!losing)
	break;

      opcode = opcode->m_next;

      if (!opcode)
	{
	  if (ok_arch
	      && !(ok_arch & current_architecture))
	    {
	      const struct m68k_cpu *cpu;
	      int any = 0;
	      size_t space = 400;
	      char *buf = xmalloc (space + 1);
	      size_t len;
	      int paren = 1;

	      the_ins.error = buf;
	      /* Make sure there's a NUL at the end of the buffer -- strncpy
		 won't write one when it runs out of buffer */
	      buf[space] = 0;
#define APPEND(STRING) \
  (strncpy (buf, STRING, space), len = strlen (buf), buf += len, space -= len)

	      APPEND (_("invalid instruction for this architecture; needs "));
	      switch (ok_arch)
		{
		case mcfisa_a:
		  APPEND (_("ColdFire ISA_A"));
		  break;
		case mcfhwdiv:
		  APPEND (_("ColdFire hardware divide"));
		  break;
		case mcfisa_aa:
		  APPEND (_("ColdFire ISA_A+"));
		  break;
		case mcfisa_b:
		  APPEND (_("ColdFire ISA_B"));
		  break;
		case cfloat:
		  APPEND (_("ColdFire fpu"));
		  break;
		case mfloat:
		  APPEND (_("M68K fpu"));
		  break;
		case mmmu:
		  APPEND (_("M68K mmu"));
		  break;
		case m68020up:
		  APPEND (_("68020 or higher"));
		  break;
		case m68000up:
		  APPEND (_("68000 or higher"));
		  break;
		case m68010up:
		  APPEND (_("68010 or higher"));
		  break;
		default:
		  paren = 0;
		}
	      if (paren)
		APPEND (" (");

	      for (cpu = m68k_cpus; cpu->name; cpu++)
		if (!cpu->alias && (cpu->arch & ok_arch))
		  {
		    const struct m68k_cpu *alias;

		    if (any)
		      APPEND (", ");
		    any = 0;
		    APPEND (cpu->name);
		    APPEND (" [");
		    if (cpu != m68k_cpus)
		      for (alias = cpu - 1; alias->alias; alias--)
			{
			  if (any)
			    APPEND (", ");
			  APPEND (alias->name);
			  any = 1;
			}
		    for (alias = cpu + 1; alias->alias; alias++)
		      {
			if (any)
			  APPEND (", ");
			APPEND (alias->name);
			any = 1;
		      }
		    
		    APPEND ("]");
		    any = 1;
		  }
	      if (paren)
		APPEND (")");
#undef APPEND
	      if (!space)
		{
		  /* we ran out of space, so replace the end of the list
		     with ellipsis.  */
		  buf -= 4;
		  while (*buf != ' ')
		    buf--;
		  strcpy (buf, " ...");
		}
	    }
	  else
	    the_ins.error = _("operands mismatch");
	  return;
	}

      losing = 0;
    }

  /* Now assemble it.  */
  the_ins.args = opcode->m_operands;
  the_ins.numargs = opcode->m_opnum;
  the_ins.numo = opcode->m_codenum;
  the_ins.opcode[0] = getone (opcode);
  the_ins.opcode[1] = gettwo (opcode);

  for (s = the_ins.args, opP = &the_ins.operands[0]; *s; s += 2, opP++)
    {
      /* This switch is a doozy.
	 Watch the first step; its a big one! */
      switch (s[0])
	{

	case '*':
	case '~':
	case '%':
	case ';':
	case '@':
	case '!':
	case '&':
	case '$':
	case '?':
	case '/':
	case '<':
	case '>':
	case 'b':
	case 'm':
	case 'n':
	case 'o':
	case 'p':
	case 'q':
	case 'v':
	case 'w':
	case 'y':
	case 'z':
	case '4':
#ifndef NO_68851
	case '|':
#endif
	  switch (opP->mode)
	    {
	    case IMMED:
	      tmpreg = 0x3c;	/* 7.4 */
	      if (strchr ("bwl", s[1]))
		nextword = get_num (&opP->disp, 90);
	      else
		nextword = get_num (&opP->disp, 0);
	      if (isvar (&opP->disp))
		add_fix (s[1], &opP->disp, 0, 0);
	      switch (s[1])
		{
		case 'b':
		  if (!isbyte (nextword))
		    opP->error = _("operand out of range");
		  addword (nextword);
		  baseo = 0;
		  break;
		case 'w':
		  if (!isword (nextword))
		    opP->error = _("operand out of range");
		  addword (nextword);
		  baseo = 0;
		  break;
		case 'W':
		  if (!issword (nextword))
		    opP->error = _("operand out of range");
		  addword (nextword);
		  baseo = 0;
		  break;
		case 'l':
		  addword (nextword >> 16);
		  addword (nextword);
		  baseo = 0;
		  break;

		case 'f':
		  baseo = 2;
		  outro = 8;
		  break;
		case 'F':
		  baseo = 4;
		  outro = 11;
		  break;
		case 'x':
		  baseo = 6;
		  outro = 15;
		  break;
		case 'p':
		  baseo = 6;
		  outro = -1;
		  break;
		default:
		  abort ();
		}
	      if (!baseo)
		break;

	      /* We gotta put out some float.  */
	      if (op (&opP->disp) != O_big)
		{
		  valueT val;
		  int gencnt;

		  /* Can other cases happen here?  */
		  if (op (&opP->disp) != O_constant)
		    abort ();

		  val = (valueT) offs (&opP->disp);
		  gencnt = 0;
		  do
		    {
		      generic_bignum[gencnt] = (LITTLENUM_TYPE) val;
		      val >>= LITTLENUM_NUMBER_OF_BITS;
		      ++gencnt;
		    }
		  while (val != 0);
		  offs (&opP->disp) = gencnt;
		}
	      if (offs (&opP->disp) > 0)
		{
		  if (offs (&opP->disp) > baseo)
		    {
		      as_warn (_("Bignum too big for %c format; truncated"),
			       s[1]);
		      offs (&opP->disp) = baseo;
		    }
		  baseo -= offs (&opP->disp);
		  while (baseo--)
		    addword (0);
		  for (wordp = generic_bignum + offs (&opP->disp) - 1;
		       offs (&opP->disp)--;
		       --wordp)
		    addword (*wordp);
		  break;
		}
	      gen_to_words (words, baseo, (long) outro);
	      for (wordp = words; baseo--; wordp++)
		addword (*wordp);
	      break;
	    case DREG:
	      tmpreg = opP->reg - DATA;	/* 0.dreg */
	      break;
	    case AREG:
	      tmpreg = 0x08 + opP->reg - ADDR;	/* 1.areg */
	      break;
	    case AINDR:
	      tmpreg = 0x10 + opP->reg - ADDR;	/* 2.areg */
	      break;
	    case ADEC:
	      tmpreg = 0x20 + opP->reg - ADDR;	/* 4.areg */
	      break;
	    case AINC:
	      tmpreg = 0x18 + opP->reg - ADDR;	/* 3.areg */
	      break;
	    case DISP:

	      nextword = get_num (&opP->disp, 90);

	      /* Convert mode 5 addressing with a zero offset into
		 mode 2 addressing to reduce the instruction size by a
		 word.  */
	      if (! isvar (&opP->disp)
		  && (nextword == 0)
		  && (opP->disp.size == SIZE_UNSPEC)
		  && (opP->reg >= ADDR0)
		  && (opP->reg <= ADDR7))
		{
		  tmpreg = 0x10 + opP->reg - ADDR; /* 2.areg */
		  break;
		}

	      if (opP->reg == PC
		  && ! isvar (&opP->disp)
		  && m68k_abspcadd)
		{
		  opP->disp.exp.X_op = O_symbol;
		  opP->disp.exp.X_add_symbol =
		    section_symbol (absolute_section);
		}

	      /* Force into index mode.  Hope this works.  */

	      /* We do the first bit for 32-bit displacements, and the
		 second bit for 16 bit ones.  It is possible that we
		 should make the default be WORD instead of LONG, but
		 I think that'd break GCC, so we put up with a little
		 inefficiency for the sake of working output.  */

	      if (!issword (nextword)
		  || (isvar (&opP->disp)
		      && ((opP->disp.size == SIZE_UNSPEC
			   && flag_short_refs == 0
			   && cpu_of_arch (current_architecture) >= m68020
			   && ! arch_coldfire_p (current_architecture))
			  || opP->disp.size == SIZE_LONG)))
		{
		  if (cpu_of_arch (current_architecture) < m68020
		      || arch_coldfire_p (current_architecture))
		    opP->error =
		      _("displacement too large for this architecture; needs 68020 or higher");
		  if (opP->reg == PC)
		    tmpreg = 0x3B;	/* 7.3 */
		  else
		    tmpreg = 0x30 + opP->reg - ADDR;	/* 6.areg */
		  if (isvar (&opP->disp))
		    {
		      if (opP->reg == PC)
			{
			  if (opP->disp.size == SIZE_LONG
#ifdef OBJ_ELF
			      /* If the displacement needs pic
				 relocation it cannot be relaxed.  */
			      || opP->disp.pic_reloc != pic_none
#endif
			      )
			    {
			      addword (0x0170);
			      add_fix ('l', &opP->disp, 1, 2);
			    }
			  else
			    {
			      add_frag (adds (&opP->disp),
					SEXT (offs (&opP->disp)),
					TAB (PCREL1632, SZ_UNDEF));
			      break;
			    }
			}
		      else
			{
			  addword (0x0170);
			  add_fix ('l', &opP->disp, 0, 0);
			}
		    }
		  else
		    addword (0x0170);
		  addword (nextword >> 16);
		}
	      else
		{
		  if (opP->reg == PC)
		    tmpreg = 0x3A;	/* 7.2 */
		  else
		    tmpreg = 0x28 + opP->reg - ADDR;	/* 5.areg */

		  if (isvar (&opP->disp))
		    {
		      if (opP->reg == PC)
			{
			  add_fix ('w', &opP->disp, 1, 0);
			}
		      else
			add_fix ('w', &opP->disp, 0, 0);
		    }
		}
	      addword (nextword);
	      break;

	    case POST:
	    case PRE:
	    case BASE:
	      nextword = 0;
	      baseo = get_num (&opP->disp, 90);
	      if (opP->mode == POST || opP->mode == PRE)
		outro = get_num (&opP->odisp, 90);
	      /* Figure out the `addressing mode'.
		 Also turn on the BASE_DISABLE bit, if needed.  */
	      if (opP->reg == PC || opP->reg == ZPC)
		{
		  tmpreg = 0x3b;	/* 7.3 */
		  if (opP->reg == ZPC)
		    nextword |= 0x80;
		}
	      else if (opP->reg == 0)
		{
		  nextword |= 0x80;
		  tmpreg = 0x30;	/* 6.garbage */
		}
	      else if (opP->reg >= ZADDR0 && opP->reg <= ZADDR7)
		{
		  nextword |= 0x80;
		  tmpreg = 0x30 + opP->reg - ZADDR0;
		}
	      else
		tmpreg = 0x30 + opP->reg - ADDR;	/* 6.areg */

	      siz1 = opP->disp.size;
	      if (opP->mode == POST || opP->mode == PRE)
		siz2 = opP->odisp.size;
	      else
		siz2 = SIZE_UNSPEC;

	      /* Index register stuff.  */
	      if (opP->index.reg != 0
		  && opP->index.reg >= DATA
		  && opP->index.reg <= ADDR7)
		{
		  nextword |= (opP->index.reg - DATA) << 12;

		  if (opP->index.size == SIZE_LONG
		      || (opP->index.size == SIZE_UNSPEC
			  && m68k_index_width_default == SIZE_LONG))
		    nextword |= 0x800;

		  if ((opP->index.scale != 1
		       && cpu_of_arch (current_architecture) < m68020)
		      || (opP->index.scale == 8
			  && (arch_coldfire_p (current_architecture)
                              && !arch_coldfire_fpu (current_architecture))))
		    {
		      opP->error =
			_("scale factor invalid on this architecture; needs cpu32 or 68020 or higher");
		    }

		  if (arch_coldfire_p (current_architecture)
		      && opP->index.size == SIZE_WORD)
		    opP->error = _("invalid index size for coldfire");

		  switch (opP->index.scale)
		    {
		    case 1:
		      break;
		    case 2:
		      nextword |= 0x200;
		      break;
		    case 4:
		      nextword |= 0x400;
		      break;
		    case 8:
		      nextword |= 0x600;
		      break;
		    default:
		      abort ();
		    }
		  /* IF its simple,
		     GET US OUT OF HERE! */

		  /* Must be INDEX, with an index register.  Address
		     register cannot be ZERO-PC, and either :b was
		     forced, or we know it will fit.  For a 68000 or
		     68010, force this mode anyways, because the
		     larger modes aren't supported.  */
		  if (opP->mode == BASE
		      && ((opP->reg >= ADDR0
			   && opP->reg <= ADDR7)
			  || opP->reg == PC))
		    {
		      if (siz1 == SIZE_BYTE
			  || cpu_of_arch (current_architecture) < m68020
			  || arch_coldfire_p (current_architecture)
			  || (siz1 == SIZE_UNSPEC
			      && ! isvar (&opP->disp)
			      && issbyte (baseo)))
			{
 			  nextword += baseo & 0xff;
 			  addword (nextword);
 			  if (isvar (&opP->disp))
			    {
			      /* Do a byte relocation.  If it doesn't
				 fit (possible on m68000) let the
				 fixup processing complain later.  */
			      if (opP->reg == PC)
				add_fix ('B', &opP->disp, 1, 1);
			      else
				add_fix ('B', &opP->disp, 0, 0);
			    }
			  else if (siz1 != SIZE_BYTE)
			    {
			      if (siz1 != SIZE_UNSPEC)
				as_warn (_("Forcing byte displacement"));
			      if (! issbyte (baseo))
				opP->error = _("byte displacement out of range");
			    }

			  break;
			}
		      else if (siz1 == SIZE_UNSPEC
			       && opP->reg == PC
			       && isvar (&opP->disp)
			       && subs (&opP->disp) == NULL
#ifdef OBJ_ELF
			       /* If the displacement needs pic
				  relocation it cannot be relaxed.  */
			       && opP->disp.pic_reloc == pic_none
#endif
			       )
			{
			  /* The code in md_convert_frag_1 needs to be
                             able to adjust nextword.  Call frag_grow
                             to ensure that we have enough space in
                             the frag obstack to make all the bytes
                             contiguous.  */
			  frag_grow (14);
			  nextword += baseo & 0xff;
			  addword (nextword);
			  add_frag (adds (&opP->disp),
				    SEXT (offs (&opP->disp)),
				    TAB (PCINDEX, SZ_UNDEF));

			  break;
			}
		    }
		}
	      else
		{
		  nextword |= 0x40;	/* No index reg.  */
		  if (opP->index.reg >= ZDATA0
		      && opP->index.reg <= ZDATA7)
		    nextword |= (opP->index.reg - ZDATA0) << 12;
		  else if (opP->index.reg >= ZADDR0
			   || opP->index.reg <= ZADDR7)
		    nextword |= (opP->index.reg - ZADDR0 + 8) << 12;
		}

	      /* It isn't simple.  */

	      if (cpu_of_arch (current_architecture) < m68020
		  || arch_coldfire_p (current_architecture))
		opP->error =
		  _("invalid operand mode for this architecture; needs 68020 or higher");

	      nextword |= 0x100;
	      /* If the guy specified a width, we assume that it is
		 wide enough.  Maybe it isn't.  If so, we lose.  */
	      switch (siz1)
		{
		case SIZE_UNSPEC:
		  if (isvar (&opP->disp)
		      ? m68k_rel32
		      : ! issword (baseo))
		    {
		      siz1 = SIZE_LONG;
		      nextword |= 0x30;
		    }
		  else if (! isvar (&opP->disp) && baseo == 0)
		    nextword |= 0x10;
		  else
		    {
		      nextword |= 0x20;
		      siz1 = SIZE_WORD;
		    }
		  break;
		case SIZE_BYTE:
		  as_warn (_(":b not permitted; defaulting to :w"));
		  /* Fall through.  */
		case SIZE_WORD:
		  nextword |= 0x20;
		  break;
		case SIZE_LONG:
		  nextword |= 0x30;
		  break;
		}

	      /* Figure out inner displacement stuff.  */
	      if (opP->mode == POST || opP->mode == PRE)
		{
		  if (cpu_of_arch (current_architecture) & cpu32)
		    opP->error = _("invalid operand mode for this architecture; needs 68020 or higher");
		  switch (siz2)
		    {
		    case SIZE_UNSPEC:
		      if (isvar (&opP->odisp)
			  ? m68k_rel32
			  : ! issword (outro))
			{
			  siz2 = SIZE_LONG;
			  nextword |= 0x3;
			}
		      else if (! isvar (&opP->odisp) && outro == 0)
			nextword |= 0x1;
		      else
			{
			  nextword |= 0x2;
			  siz2 = SIZE_WORD;
			}
		      break;
		    case 1:
		      as_warn (_(":b not permitted; defaulting to :w"));
		      /* Fall through.  */
		    case 2:
		      nextword |= 0x2;
		      break;
		    case 3:
		      nextword |= 0x3;
		      break;
		    }
		  if (opP->mode == POST
		      && (nextword & 0x40) == 0)
		    nextword |= 0x04;
		}
	      addword (nextword);

	      if (siz1 != SIZE_UNSPEC && isvar (&opP->disp))
		{
		  if (opP->reg == PC || opP->reg == ZPC)
		    add_fix (siz1 == SIZE_LONG ? 'l' : 'w', &opP->disp, 1, 2);
		  else
		    add_fix (siz1 == SIZE_LONG ? 'l' : 'w', &opP->disp, 0, 0);
		}
	      if (siz1 == SIZE_LONG)
		addword (baseo >> 16);
	      if (siz1 != SIZE_UNSPEC)
		addword (baseo);

	      if (siz2 != SIZE_UNSPEC && isvar (&opP->odisp))
		add_fix (siz2 == SIZE_LONG ? 'l' : 'w', &opP->odisp, 0, 0);
	      if (siz2 == SIZE_LONG)
		addword (outro >> 16);
	      if (siz2 != SIZE_UNSPEC)
		addword (outro);

	      break;

	    case ABSL:
	      nextword = get_num (&opP->disp, 90);
	      switch (opP->disp.size)
		{
		default:
		  abort ();
		case SIZE_UNSPEC:
		  if (!isvar (&opP->disp) && issword (offs (&opP->disp)))
		    {
		      tmpreg = 0x38;	/* 7.0 */
		      addword (nextword);
		      break;
		    }
		  if (isvar (&opP->disp)
		      && !subs (&opP->disp)
		      && adds (&opP->disp)
#ifdef OBJ_ELF
		      /* If the displacement needs pic relocation it
			 cannot be relaxed.  */
		      && opP->disp.pic_reloc == pic_none
#endif
		      && !flag_long_jumps
		      && !strchr ("~%&$?", s[0]))
		    {
		      tmpreg = 0x3A;	/* 7.2 */
		      add_frag (adds (&opP->disp),
				SEXT (offs (&opP->disp)),
				TAB (ABSTOPCREL, SZ_UNDEF));
		      break;
		    }
		  /* Fall through into long.  */
		case SIZE_LONG:
		  if (isvar (&opP->disp))
		    add_fix ('l', &opP->disp, 0, 0);

		  tmpreg = 0x39;/* 7.1 mode */
		  addword (nextword >> 16);
		  addword (nextword);
		  break;

		case SIZE_BYTE:
		  as_bad (_("unsupported byte value; use a different suffix"));
		  /* Fall through.  */

		case SIZE_WORD:
		  if (isvar (&opP->disp))
		    add_fix ('w', &opP->disp, 0, 0);

		  tmpreg = 0x38;/* 7.0 mode */
		  addword (nextword);
		  break;
		}
	      break;
	    case CONTROL:
	    case FPREG:
	    default:
	      as_bad (_("unknown/incorrect operand"));
	      /* abort (); */
	    }

	  /* If s[0] is '4', then this is for the mac instructions
	     that can have a trailing_ampersand set.  If so, set 0x100
	     bit on tmpreg so install_gen_operand can check for it and
	     set the appropriate bit (word2, bit 5).  */
	  if (s[0] == '4')
	    {
	      if (opP->trailing_ampersand)
		tmpreg |= 0x100;
	    }
	  install_gen_operand (s[1], tmpreg);
	  break;

	case '#':
	case '^':
	  switch (s[1])
	    {			/* JF: I hate floating point! */
	    case 'j':
	      tmpreg = 70;
	      break;
	    case '8':
	      tmpreg = 20;
	      break;
	    case 'C':
	      tmpreg = 50;
	      break;
	    case '3':
	    default:
	      tmpreg = 90;
	      break;
	    }
	  tmpreg = get_num (&opP->disp, tmpreg);
	  if (isvar (&opP->disp))
	    add_fix (s[1], &opP->disp, 0, 0);
	  switch (s[1])
	    {
	    case 'b':		/* Danger:  These do no check for
				   certain types of overflow.
				   user beware! */
	      if (!isbyte (tmpreg))
		opP->error = _("out of range");
	      insop (tmpreg, opcode);
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n =
		  (opcode->m_codenum) * 2 + 1;
	      break;
	    case 'B':
	      if (!issbyte (tmpreg))
		opP->error = _("out of range");
	      the_ins.opcode[the_ins.numo - 1] |= tmpreg & 0xff;
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n = opcode->m_codenum * 2 - 1;
	      break;
	    case 'w':
	      if (!isword (tmpreg))
		opP->error = _("out of range");
	      insop (tmpreg, opcode);
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n = (opcode->m_codenum) * 2;
	      break;
	    case 'W':
	      if (!issword (tmpreg))
		opP->error = _("out of range");
	      insop (tmpreg, opcode);
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n = (opcode->m_codenum) * 2;
	      break;
	    case 'l':
	      /* Because of the way insop works, we put these two out
		 backwards.  */
	      insop (tmpreg, opcode);
	      insop (tmpreg >> 16, opcode);
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n = (opcode->m_codenum) * 2;
	      break;
	    case '3':
	      tmpreg &= 0xFF;
	    case '8':
	    case 'C':
	    case 'j':
	      install_operand (s[1], tmpreg);
	      break;
	    default:
	      abort ();
	    }
	  break;

	case '+':
	case '-':
	case 'A':
	case 'a':
	  install_operand (s[1], opP->reg - ADDR);
	  break;

	case 'B':
	  tmpreg = get_num (&opP->disp, 90);
	  switch (s[1])
	    {
	    case 'B':
	      add_fix ('B', &opP->disp, 1, -1);
	      break;
	    case 'W':
	      add_fix ('w', &opP->disp, 1, 0);
	      addword (0);
	      break;
	    case 'L':
	    long_branch:
	      if (! HAVE_LONG_BRANCH (current_architecture))
		as_warn (_("Can't use long branches on 68000/68010/5200"));
	      the_ins.opcode[0] |= 0xff;
	      add_fix ('l', &opP->disp, 1, 0);
	      addword (0);
	      addword (0);
	      break;
	    case 'g':
	      if (subs (&opP->disp))	/* We can't relax it.  */
		goto long_branch;

#ifdef OBJ_ELF
	      /* If the displacement needs pic relocation it cannot be
		 relaxed.  */
	      if (opP->disp.pic_reloc != pic_none)
		goto long_branch;
#endif
	      /* This could either be a symbol, or an absolute
		 address.  If it's an absolute address, turn it into
		 an absolute jump right here and keep it out of the
		 relaxer.  */
	      if (adds (&opP->disp) == 0)
		{
		  if (the_ins.opcode[0] == 0x6000)	/* jbra */
		    the_ins.opcode[0] = 0x4EF9;
		  else if (the_ins.opcode[0] == 0x6100)	/* jbsr */
		    the_ins.opcode[0] = 0x4EB9;
		  else					/* jCC */
		    {
		      the_ins.opcode[0] ^= 0x0100;
		      the_ins.opcode[0] |= 0x0006;
		      addword (0x4EF9);
		    }
		  add_fix ('l', &opP->disp, 0, 0);
		  addword (0);
		  addword (0);
		  break;
		}

	      /* Now we know it's going into the relaxer.  Now figure
		 out which mode.  We try in this order of preference:
		 long branch, absolute jump, byte/word branches only.  */
	      if (HAVE_LONG_BRANCH (current_architecture))
		add_frag (adds (&opP->disp),
			  SEXT (offs (&opP->disp)),
			  TAB (BRANCHBWL, SZ_UNDEF));
	      else if (! flag_keep_pcrel)
		{
		  if ((the_ins.opcode[0] == 0x6000)
		      || (the_ins.opcode[0] == 0x6100))
		    add_frag (adds (&opP->disp),
			      SEXT (offs (&opP->disp)),
			      TAB (BRABSJUNC, SZ_UNDEF));
		  else
		    add_frag (adds (&opP->disp),
			      SEXT (offs (&opP->disp)),
			      TAB (BRABSJCOND, SZ_UNDEF));
		}
	      else
		add_frag (adds (&opP->disp),
			  SEXT (offs (&opP->disp)),
			  TAB (BRANCHBW, SZ_UNDEF));
	      break;
	    case 'w':
	      if (isvar (&opP->disp))
		{
		  /* Check for DBcc instructions.  We can relax them,
		     but only if we have long branches and/or absolute
		     jumps.  */
		  if (((the_ins.opcode[0] & 0xf0f8) == 0x50c8)
		      && (HAVE_LONG_BRANCH (current_architecture)
			  || (! flag_keep_pcrel)))
		    {
		      if (HAVE_LONG_BRANCH (current_architecture))
			add_frag (adds (&opP->disp),
				  SEXT (offs (&opP->disp)),
				  TAB (DBCCLBR, SZ_UNDEF));
		      else
			add_frag (adds (&opP->disp),
				  SEXT (offs (&opP->disp)),
				  TAB (DBCCABSJ, SZ_UNDEF));
		      break;
		    }
		  add_fix ('w', &opP->disp, 1, 0);
		}
	      addword (0);
	      break;
	    case 'C':		/* Fixed size LONG coproc branches.  */
	      add_fix ('l', &opP->disp, 1, 0);
	      addword (0);
	      addword (0);
	      break;
	    case 'c':		/* Var size Coprocesssor branches.  */
	      if (subs (&opP->disp) || (adds (&opP->disp) == 0))
		{
		  the_ins.opcode[the_ins.numo - 1] |= 0x40;
		  add_fix ('l', &opP->disp, 1, 0);
		  addword (0);
		  addword (0);
		}
	      else
		add_frag (adds (&opP->disp),
			  SEXT (offs (&opP->disp)),
			  TAB (FBRANCH, SZ_UNDEF));
	      break;
	    default:
	      abort ();
	    }
	  break;

	case 'C':		/* Ignore it.  */
	  break;

	case 'd':		/* JF this is a kludge.  */
	  install_operand ('s', opP->reg - ADDR);
	  tmpreg = get_num (&opP->disp, 90);
	  if (!issword (tmpreg))
	    {
	      as_warn (_("Expression out of range, using 0"));
	      tmpreg = 0;
	    }
	  addword (tmpreg);
	  break;

	case 'D':
	  install_operand (s[1], opP->reg - DATA);
	  break;

	case 'e':  /* EMAC ACCx, reg/reg.  */
	  install_operand (s[1], opP->reg - ACC);
	  break;
	  
	case 'E':		/* Ignore it.  */
	  break;

	case 'F':
	  install_operand (s[1], opP->reg - FP0);
	  break;

	case 'g':  /* EMAC ACCEXTx.  */
	  install_operand (s[1], opP->reg - ACCEXT01);
	  break;

	case 'G':		/* Ignore it.  */
	case 'H':
	  break;

	case 'I':
	  tmpreg = opP->reg - COP0;
	  install_operand (s[1], tmpreg);
	  break;

	case 'i':  /* MAC/EMAC scale factor.  */
	  install_operand (s[1], opP->mode == LSH ? 0x1 : 0x3);
	  break;

	case 'J':		/* JF foo.  */
	  switch (opP->reg)
	    {
	    case SFC:
	      tmpreg = 0x000;
	      break;
	    case DFC:
	      tmpreg = 0x001;
	      break;
	    case CACR:
	      tmpreg = 0x002;
	      break;
	    case TC:
	      tmpreg = 0x003;
	      break;
	    case ACR0:
	    case ITT0:
	      tmpreg = 0x004;
	      break;
	    case ACR1:
	    case ITT1:
	      tmpreg = 0x005;
	      break;
	    case ACR2:
	    case DTT0:
	      tmpreg = 0x006;
	      break;
	    case ACR3:
	    case DTT1:
	      tmpreg = 0x007;
	      break;
	    case BUSCR:
	      tmpreg = 0x008;
	      break;

	    case USP:
	      tmpreg = 0x800;
	      break;
	    case VBR:
	      tmpreg = 0x801;
	      break;
	    case CAAR:
	      tmpreg = 0x802;
	      break;
	    case MSP:
	      tmpreg = 0x803;
	      break;
	    case ISP:
	      tmpreg = 0x804;
	      break;
	    case MMUSR:
	      tmpreg = 0x805;
	      break;
	    case URP:
	      tmpreg = 0x806;
	      break;
	    case SRP:
	      tmpreg = 0x807;
	      break;
	    case PCR:
	      tmpreg = 0x808;
	      break;
            case ROMBAR:
	      tmpreg = 0xC00;
	      break;
            case ROMBAR1:
              tmpreg = 0xC01;
              break;
	    case FLASHBAR:
	    case RAMBAR0:
	      tmpreg = 0xC04;
	      break;
	    case RAMBAR:
	    case RAMBAR1:
	      tmpreg = 0xC05;
	      break;
            case MPCR:
              tmpreg = 0xC0C;
              break;
            case EDRAMBAR:
              tmpreg = 0xC0D;
              break;
            case MBAR0:
            case MBAR2:
            case SECMBAR:
              tmpreg = 0xC0E;
              break;
            case MBAR1:
	    case MBAR:
	      tmpreg = 0xC0F;
	      break;
            case PCR1U0:
              tmpreg = 0xD02;
              break;
            case PCR1L0:
              tmpreg = 0xD03;
              break;
            case PCR2U0:
              tmpreg = 0xD04;
              break;
            case PCR2L0:
              tmpreg = 0xD05;
              break;
            case PCR3U0:
              tmpreg = 0xD06;
              break;
            case PCR3L0:
              tmpreg = 0xD07;
              break;
            case PCR1L1:
              tmpreg = 0xD0A;
              break;
            case PCR1U1:
              tmpreg = 0xD0B;
              break;
            case PCR2L1:
              tmpreg = 0xD0C;
              break;
            case PCR2U1:
              tmpreg = 0xD0D;
              break;
            case PCR3L1:
              tmpreg = 0xD0E;
              break;
            case PCR3U1:
              tmpreg = 0xD0F;
              break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;

	case 'k':
	  tmpreg = get_num (&opP->disp, 55);
	  install_operand (s[1], tmpreg & 0x7f);
	  break;

	case 'l':
	  tmpreg = opP->mask;
	  if (s[1] == 'w')
	    {
	      if (tmpreg & 0x7FF0000)
		as_bad (_("Floating point register in register list"));
	      insop (reverse_16_bits (tmpreg), opcode);
	    }
	  else
	    {
	      if (tmpreg & 0x700FFFF)
		as_bad (_("Wrong register in floating-point reglist"));
	      install_operand (s[1], reverse_8_bits (tmpreg >> 16));
	    }
	  break;

	case 'L':
	  tmpreg = opP->mask;
	  if (s[1] == 'w')
	    {
	      if (tmpreg & 0x7FF0000)
		as_bad (_("Floating point register in register list"));
	      insop (tmpreg, opcode);
	    }
	  else if (s[1] == '8')
	    {
	      if (tmpreg & 0x0FFFFFF)
		as_bad (_("incorrect register in reglist"));
	      install_operand (s[1], tmpreg >> 24);
	    }
	  else
	    {
	      if (tmpreg & 0x700FFFF)
		as_bad (_("wrong register in floating-point reglist"));
	      else
		install_operand (s[1], tmpreg >> 16);
	    }
	  break;

	case 'M':
	  install_operand (s[1], get_num (&opP->disp, 60));
	  break;

	case 'O':
	  tmpreg = ((opP->mode == DREG)
		    ? 0x20 + (int) (opP->reg - DATA)
		    : (get_num (&opP->disp, 40) & 0x1F));
	  install_operand (s[1], tmpreg);
	  break;

	case 'Q':
	  tmpreg = get_num (&opP->disp, 10);
	  if (tmpreg == 8)
	    tmpreg = 0;
	  install_operand (s[1], tmpreg);
	  break;

	case 'R':
	  /* This depends on the fact that ADDR registers are eight
	     more than their corresponding DATA regs, so the result
	     will have the ADDR_REG bit set.  */
	  install_operand (s[1], opP->reg - DATA);
	  break;

	case 'r':
	  if (opP->mode == AINDR)
	    install_operand (s[1], opP->reg - DATA);
	  else
	    install_operand (s[1], opP->index.reg - DATA);
	  break;

	case 's':
	  if (opP->reg == FPI)
	    tmpreg = 0x1;
	  else if (opP->reg == FPS)
	    tmpreg = 0x2;
	  else if (opP->reg == FPC)
	    tmpreg = 0x4;
	  else
	    abort ();
	  install_operand (s[1], tmpreg);
	  break;

	case 'S':		/* Ignore it.  */
	  break;

	case 'T':
	  install_operand (s[1], get_num (&opP->disp, 30));
	  break;

	case 'U':		/* Ignore it.  */
	  break;

	case 'c':
	  switch (opP->reg)
	    {
	    case NC:
	      tmpreg = 0;
	      break;
	    case DC:
	      tmpreg = 1;
	      break;
	    case IC:
	      tmpreg = 2;
	      break;
	    case BC:
	      tmpreg = 3;
	      break;
	    default:
	      as_fatal (_("failed sanity check"));
	    }			/* switch on cache token.  */
	  install_operand (s[1], tmpreg);
	  break;
#ifndef NO_68851
	  /* JF: These are out of order, I fear.  */
	case 'f':
	  switch (opP->reg)
	    {
	    case SFC:
	      tmpreg = 0;
	      break;
	    case DFC:
	      tmpreg = 1;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;

	case '0':
	case '1':
	case '2':
	  switch (opP->reg)
	    {
	    case TC:
	      tmpreg = 0;
	      break;
	    case CAL:
	      tmpreg = 4;
	      break;
	    case VAL:
	      tmpreg = 5;
	      break;
	    case SCC:
	      tmpreg = 6;
	      break;
	    case AC:
	      tmpreg = 7;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;

	case 'V':
	  if (opP->reg == VAL)
	    break;
	  abort ();

	case 'W':
	  switch (opP->reg)
	    {
	    case DRP:
	      tmpreg = 1;
	      break;
	    case SRP:
	      tmpreg = 2;
	      break;
	    case CRP:
	      tmpreg = 3;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;

	case 'X':
	  switch (opP->reg)
	    {
	    case BAD:
	    case BAD + 1:
	    case BAD + 2:
	    case BAD + 3:
	    case BAD + 4:
	    case BAD + 5:
	    case BAD + 6:
	    case BAD + 7:
	      tmpreg = (4 << 10) | ((opP->reg - BAD) << 2);
	      break;

	    case BAC:
	    case BAC + 1:
	    case BAC + 2:
	    case BAC + 3:
	    case BAC + 4:
	    case BAC + 5:
	    case BAC + 6:
	    case BAC + 7:
	      tmpreg = (5 << 10) | ((opP->reg - BAC) << 2);
	      break;

	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;
	case 'Y':
	  know (opP->reg == PSR);
	  break;
	case 'Z':
	  know (opP->reg == PCSR);
	  break;
#endif /* m68851 */
	case '3':
	  switch (opP->reg)
	    {
	    case TT0:
	      tmpreg = 2;
	      break;
	    case TT1:
	      tmpreg = 3;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;
	case 't':
	  tmpreg = get_num (&opP->disp, 20);
	  install_operand (s[1], tmpreg);
	  break;
	case '_':	/* used only for move16 absolute 32-bit address.  */
	  if (isvar (&opP->disp))
	    add_fix ('l', &opP->disp, 0, 0);
	  tmpreg = get_num (&opP->disp, 90);
	  addword (tmpreg >> 16);
	  addword (tmpreg & 0xFFFF);
	  break;
	case 'u':
	  install_operand (s[1], opP->reg - DATA0L);
	  opP->reg -= (DATA0L);
	  opP->reg &= 0x0F;	/* remove upper/lower bit.  */
	  break;
	case 'x':
	  tmpreg = get_num (&opP->disp, 80);
	  if (tmpreg == -1)
	    tmpreg = 0;
	  install_operand (s[1], tmpreg);
	  break;
	default:
	  abort ();
	}
    }

  /* By the time whe get here (FINALLY) the_ins contains the complete
     instruction, ready to be emitted. . .  */
}

static int
reverse_16_bits (int in)
{
  int out = 0;
  int n;

  static int mask[16] =
  {
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000
  };
  for (n = 0; n < 16; n++)
    {
      if (in & mask[n])
	out |= mask[15 - n];
    }
  return out;
}				/* reverse_16_bits() */

static int
reverse_8_bits (int in)
{
  int out = 0;
  int n;

  static int mask[8] =
  {
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
  };

  for (n = 0; n < 8; n++)
    {
      if (in & mask[n])
	out |= mask[7 - n];
    }
  return out;
}				/* reverse_8_bits() */

/* Cause an extra frag to be generated here, inserting up to 10 bytes
   (that value is chosen in the frag_var call in md_assemble).  TYPE
   is the subtype of the frag to be generated; its primary type is
   rs_machine_dependent.

   The TYPE parameter is also used by md_convert_frag_1 and
   md_estimate_size_before_relax.  The appropriate type of fixup will
   be emitted by md_convert_frag_1.

   ADD becomes the FR_SYMBOL field of the frag, and OFF the FR_OFFSET.  */
static void
install_operand (int mode, int val)
{
  switch (mode)
    {
    case 's':
      the_ins.opcode[0] |= val & 0xFF;	/* JF FF is for M kludge.  */
      break;
    case 'd':
      the_ins.opcode[0] |= val << 9;
      break;
    case '1':
      the_ins.opcode[1] |= val << 12;
      break;
    case '2':
      the_ins.opcode[1] |= val << 6;
      break;
    case '3':
      the_ins.opcode[1] |= val;
      break;
    case '4':
      the_ins.opcode[2] |= val << 12;
      break;
    case '5':
      the_ins.opcode[2] |= val << 6;
      break;
    case '6':
      /* DANGER!  This is a hack to force cas2l and cas2w cmds to be
	 three words long! */
      the_ins.numo++;
      the_ins.opcode[2] |= val;
      break;
    case '7':
      the_ins.opcode[1] |= val << 7;
      break;
    case '8':
      the_ins.opcode[1] |= val << 10;
      break;
#ifndef NO_68851
    case '9':
      the_ins.opcode[1] |= val << 5;
      break;
#endif

    case 't':
      the_ins.opcode[1] |= (val << 10) | (val << 7);
      break;
    case 'D':
      the_ins.opcode[1] |= (val << 12) | val;
      break;
    case 'g':
      the_ins.opcode[0] |= val = 0xff;
      break;
    case 'i':
      the_ins.opcode[0] |= val << 9;
      break;
    case 'C':
      the_ins.opcode[1] |= val;
      break;
    case 'j':
      the_ins.opcode[1] |= val;
      the_ins.numo++;		/* What a hack.  */
      break;
    case 'k':
      the_ins.opcode[1] |= val << 4;
      break;
    case 'b':
    case 'w':
    case 'W':
    case 'l':
      break;
    case 'e':
      the_ins.opcode[0] |= (val << 6);
      break;
    case 'L':
      the_ins.opcode[1] = (val >> 16);
      the_ins.opcode[2] = val & 0xffff;
      break;
    case 'm':
      the_ins.opcode[0] |= ((val & 0x8) << (6 - 3));
      the_ins.opcode[0] |= ((val & 0x7) << 9);
      the_ins.opcode[1] |= ((val & 0x10) << (7 - 4));
      break;
    case 'n': /* MAC/EMAC Rx on !load.  */
      the_ins.opcode[0] |= ((val & 0x8) << (6 - 3));
      the_ins.opcode[0] |= ((val & 0x7) << 9);
      the_ins.opcode[1] |= ((val & 0x10) << (7 - 4));
      break;
    case 'o': /* MAC/EMAC Rx on load.  */
      the_ins.opcode[1] |= val << 12;
      the_ins.opcode[1] |= ((val & 0x10) << (7 - 4));
      break;
    case 'M': /* MAC/EMAC Ry on !load.  */
      the_ins.opcode[0] |= (val & 0xF);
      the_ins.opcode[1] |= ((val & 0x10) << (6 - 4));
      break;
    case 'N': /* MAC/EMAC Ry on load.  */
      the_ins.opcode[1] |= (val & 0xF);
      the_ins.opcode[1] |= ((val & 0x10) << (6 - 4));
      break;
    case 'h':
      the_ins.opcode[1] |= ((val != 1) << 10);
      break;
    case 'F':
      the_ins.opcode[0] |= ((val & 0x3) << 9);
      break;
    case 'f':
      the_ins.opcode[0] |= ((val & 0x3) << 0);
      break;
    case 'G':  /* EMAC accumulator in a EMAC load instruction.  */
      the_ins.opcode[0] |= ((~val & 0x1) << 7);
      the_ins.opcode[1] |= ((val & 0x2) << (4 - 1));
      break;
    case 'H':  /* EMAC accumulator in a EMAC non-load instruction.  */
      the_ins.opcode[0] |= ((val & 0x1) << 7);
      the_ins.opcode[1] |= ((val & 0x2) << (4 - 1));
      break;
    case 'I':
      the_ins.opcode[1] |= ((val & 0x3) << 9);
      break;
    case ']':
      the_ins.opcode[0] |= (val & 0x1) <<10;
      break;
    case 'c':
    default:
      as_fatal (_("failed sanity check."));
    }
}

static void
install_gen_operand (int mode, int val)
{
  switch (mode)
    {
    case '/':  /* Special for mask loads for mac/msac insns with
		  possible mask; trailing_ampersend set in bit 8.  */
      the_ins.opcode[0] |= (val & 0x3f);
      the_ins.opcode[1] |= (((val & 0x100) >> 8) << 5);
      break;
    case 's':
      the_ins.opcode[0] |= val;
      break;
    case 'd':
      /* This is a kludge!!! */
      the_ins.opcode[0] |= (val & 0x07) << 9 | (val & 0x38) << 3;
      break;
    case 'b':
    case 'w':
    case 'l':
    case 'f':
    case 'F':
    case 'x':
    case 'p':
      the_ins.opcode[0] |= val;
      break;
      /* more stuff goes here.  */
    default:
      as_fatal (_("failed sanity check."));
    }
}

/* Verify that we have some number of paren pairs, do m68k_ip_op(), and
   then deal with the bitfield hack.  */

static char *
crack_operand (char *str, struct m68k_op *opP)
{
  register int parens;
  register int c;
  register char *beg_str;
  int inquote = 0;

  if (!str)
    {
      return str;
    }
  beg_str = str;
  for (parens = 0; *str && (parens > 0 || inquote || notend (str)); str++)
    {
      if (! inquote)
	{
	  if (*str == '(')
	    parens++;
	  else if (*str == ')')
	    {
	      if (!parens)
		{			/* ERROR.  */
		  opP->error = _("Extra )");
		  return str;
		}
	      --parens;
	    }
	}
      if (flag_mri && *str == '\'')
	inquote = ! inquote;
    }
  if (!*str && parens)
    {				/* ERROR.  */
      opP->error = _("Missing )");
      return str;
    }
  c = *str;
  *str = '\0';
  if (m68k_ip_op (beg_str, opP) != 0)
    {
      *str = c;
      return str;
    }
  *str = c;
  if (c == '}')
    c = *++str;			/* JF bitfield hack.  */
  if (c)
    {
      c = *++str;
      if (!c)
	as_bad (_("Missing operand"));
    }

  /* Detect MRI REG symbols and convert them to REGLSTs.  */
  if (opP->mode == CONTROL && (int)opP->reg < 0)
    {
      opP->mode = REGLST;
      opP->mask = ~(int)opP->reg;
      opP->reg = 0;
    }

  return str;
}

/* This is the guts of the machine-dependent assembler.  STR points to a
   machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.
   */

static void
insert_reg (const char *regname, int regnum)
{
  char buf[100];
  int i;

#ifdef REGISTER_PREFIX
  if (!flag_reg_prefix_optional)
    {
      buf[0] = REGISTER_PREFIX;
      strcpy (buf + 1, regname);
      regname = buf;
    }
#endif

  symbol_table_insert (symbol_new (regname, reg_section, regnum,
				   &zero_address_frag));

  for (i = 0; regname[i]; i++)
    buf[i] = TOUPPER (regname[i]);
  buf[i] = '\0';

  symbol_table_insert (symbol_new (buf, reg_section, regnum,
				   &zero_address_frag));
}

struct init_entry
  {
    const char *name;
    int number;
  };

static const struct init_entry init_table[] =
{
  { "d0", DATA0 },
  { "d1", DATA1 },
  { "d2", DATA2 },
  { "d3", DATA3 },
  { "d4", DATA4 },
  { "d5", DATA5 },
  { "d6", DATA6 },
  { "d7", DATA7 },
  { "a0", ADDR0 },
  { "a1", ADDR1 },
  { "a2", ADDR2 },
  { "a3", ADDR3 },
  { "a4", ADDR4 },
  { "a5", ADDR5 },
  { "a6", ADDR6 },
  { "fp", ADDR6 },
  { "a7", ADDR7 },
  { "sp", ADDR7 },
  { "ssp", ADDR7 },
  { "fp0", FP0 },
  { "fp1", FP1 },
  { "fp2", FP2 },
  { "fp3", FP3 },
  { "fp4", FP4 },
  { "fp5", FP5 },
  { "fp6", FP6 },
  { "fp7", FP7 },
  { "fpi", FPI },
  { "fpiar", FPI },
  { "fpc", FPI },
  { "fps", FPS },
  { "fpsr", FPS },
  { "fpc", FPC },
  { "fpcr", FPC },
  { "control", FPC },
  { "status", FPS },
  { "iaddr", FPI },

  { "cop0", COP0 },
  { "cop1", COP1 },
  { "cop2", COP2 },
  { "cop3", COP3 },
  { "cop4", COP4 },
  { "cop5", COP5 },
  { "cop6", COP6 },
  { "cop7", COP7 },
  { "pc", PC },
  { "zpc", ZPC },
  { "sr", SR },

  { "ccr", CCR },
  { "cc", CCR },

  { "acc", ACC },
  { "acc0", ACC },
  { "acc1", ACC1 },
  { "acc2", ACC2 },
  { "acc3", ACC3 },
  { "accext01", ACCEXT01 },
  { "accext23", ACCEXT23 },
  { "macsr", MACSR },
  { "mask", MASK },

  /* Control registers.  */
  { "sfc", SFC },		/* Source Function Code.  */
  { "sfcr", SFC },
  { "dfc", DFC },		/* Destination Function Code.  */
  { "dfcr", DFC },
  { "cacr", CACR },		/* Cache Control Register.  */
  { "caar", CAAR },		/* Cache Address Register.  */

  { "usp", USP },		/* User Stack Pointer.  */
  { "vbr", VBR },		/* Vector Base Register.  */
  { "msp", MSP },		/* Master Stack Pointer.  */
  { "isp", ISP },		/* Interrupt Stack Pointer.  */

  { "itt0", ITT0 },		/* Instruction Transparent Translation Reg 0.  */
  { "itt1", ITT1 },		/* Instruction Transparent Translation Reg 1.  */
  { "dtt0", DTT0 },		/* Data Transparent Translation Register 0.  */
  { "dtt1", DTT1 },		/* Data Transparent Translation Register 1.  */

  /* 68ec040 versions of same */
  { "iacr0", ITT0 },		/* Instruction Access Control Register 0.  */
  { "iacr1", ITT1 },		/* Instruction Access Control Register 0.  */
  { "dacr0", DTT0 },		/* Data Access Control Register 0.  */
  { "dacr1", DTT1 },		/* Data Access Control Register 0.  */

  /* mcf5200 versions of same.  The ColdFire programmer's reference
     manual indicated that the order is 2,3,0,1, but Ken Rose
     <rose@netcom.com> says that 0,1,2,3 is the correct order.  */
  { "acr0", ACR0 },		/* Access Control Unit 0.  */
  { "acr1", ACR1 },		/* Access Control Unit 1.  */
  { "acr2", ACR2 },		/* Access Control Unit 2.  */
  { "acr3", ACR3 },		/* Access Control Unit 3.  */

  { "tc", TC },			/* MMU Translation Control Register.  */
  { "tcr", TC },

  { "mmusr", MMUSR },		/* MMU Status Register.  */
  { "srp", SRP },		/* User Root Pointer.  */
  { "urp", URP },		/* Supervisor Root Pointer.  */

  { "buscr", BUSCR },
  { "pcr", PCR },

  { "rombar", ROMBAR },		/* ROM Base Address Register.  */
  { "rambar0", RAMBAR0 },	/* ROM Base Address Register.  */
  { "rambar1", RAMBAR1 },	/* ROM Base Address Register.  */
  { "mbar", MBAR },		/* Module Base Address Register.  */

  { "mbar0",    MBAR0 },	/* mcfv4e registers.  */
  { "mbar1",    MBAR1 },	/* mcfv4e registers.  */
  { "rombar0",  ROMBAR },	/* mcfv4e registers.  */
  { "rombar1",  ROMBAR1 },	/* mcfv4e registers.  */
  { "mpcr",     MPCR },		/* mcfv4e registers.  */
  { "edrambar", EDRAMBAR },	/* mcfv4e registers.  */
  { "secmbar",  SECMBAR },	/* mcfv4e registers.  */
  { "asid",     TC },		/* mcfv4e registers.  */
  { "mmubar",   BUSCR },	/* mcfv4e registers.  */
  { "pcr1u0",   PCR1U0 },	/* mcfv4e registers.  */
  { "pcr1l0",   PCR1L0 },	/* mcfv4e registers.  */
  { "pcr2u0",   PCR2U0 },	/* mcfv4e registers.  */
  { "pcr2l0",   PCR2L0 },	/* mcfv4e registers.  */
  { "pcr3u0",   PCR3U0 },	/* mcfv4e registers.  */
  { "pcr3l0",   PCR3L0 },	/* mcfv4e registers.  */
  { "pcr1u1",   PCR1U1 },	/* mcfv4e registers.  */
  { "pcr1l1",   PCR1L1 },	/* mcfv4e registers.  */
  { "pcr2u1",   PCR2U1 },	/* mcfv4e registers.  */
  { "pcr2l1",   PCR2L1 },	/* mcfv4e registers.  */
  { "pcr3u1",   PCR3U1 },	/* mcfv4e registers.  */
  { "pcr3l1",   PCR3L1 },	/* mcfv4e registers.  */

  { "flashbar", FLASHBAR }, 	/* mcf528x registers.  */
  { "rambar",   RAMBAR },  	/* mcf528x registers.  */

  { "mbar2",    MBAR2 },  	/* mcf5249 registers.  */
  /* End of control registers.  */

  { "ac", AC },
  { "bc", BC },
  { "cal", CAL },
  { "crp", CRP },
  { "drp", DRP },
  { "pcsr", PCSR },
  { "psr", PSR },
  { "scc", SCC },
  { "val", VAL },
  { "bad0", BAD0 },
  { "bad1", BAD1 },
  { "bad2", BAD2 },
  { "bad3", BAD3 },
  { "bad4", BAD4 },
  { "bad5", BAD5 },
  { "bad6", BAD6 },
  { "bad7", BAD7 },
  { "bac0", BAC0 },
  { "bac1", BAC1 },
  { "bac2", BAC2 },
  { "bac3", BAC3 },
  { "bac4", BAC4 },
  { "bac5", BAC5 },
  { "bac6", BAC6 },
  { "bac7", BAC7 },

  { "ic", IC },
  { "dc", DC },
  { "nc", NC },

  { "tt0", TT0 },
  { "tt1", TT1 },
  /* 68ec030 versions of same.  */
  { "ac0", TT0 },
  { "ac1", TT1 },
  /* 68ec030 access control unit, identical to 030 MMU status reg.  */
  { "acusr", PSR },

  /* Suppressed data and address registers.  */
  { "zd0", ZDATA0 },
  { "zd1", ZDATA1 },
  { "zd2", ZDATA2 },
  { "zd3", ZDATA3 },
  { "zd4", ZDATA4 },
  { "zd5", ZDATA5 },
  { "zd6", ZDATA6 },
  { "zd7", ZDATA7 },
  { "za0", ZADDR0 },
  { "za1", ZADDR1 },
  { "za2", ZADDR2 },
  { "za3", ZADDR3 },
  { "za4", ZADDR4 },
  { "za5", ZADDR5 },
  { "za6", ZADDR6 },
  { "za7", ZADDR7 },

  /* Upper and lower data and address registers, used by macw and msacw.  */
  { "d0l", DATA0L },
  { "d1l", DATA1L },
  { "d2l", DATA2L },
  { "d3l", DATA3L },
  { "d4l", DATA4L },
  { "d5l", DATA5L },
  { "d6l", DATA6L },
  { "d7l", DATA7L },

  { "a0l", ADDR0L },
  { "a1l", ADDR1L },
  { "a2l", ADDR2L },
  { "a3l", ADDR3L },
  { "a4l", ADDR4L },
  { "a5l", ADDR5L },
  { "a6l", ADDR6L },
  { "a7l", ADDR7L },

  { "d0u", DATA0U },
  { "d1u", DATA1U },
  { "d2u", DATA2U },
  { "d3u", DATA3U },
  { "d4u", DATA4U },
  { "d5u", DATA5U },
  { "d6u", DATA6U },
  { "d7u", DATA7U },

  { "a0u", ADDR0U },
  { "a1u", ADDR1U },
  { "a2u", ADDR2U },
  { "a3u", ADDR3U },
  { "a4u", ADDR4U },
  { "a5u", ADDR5U },
  { "a6u", ADDR6U },
  { "a7u", ADDR7U },

  { 0, 0 }
};

static void
init_regtable (void)
{
  int i;
  for (i = 0; init_table[i].name; i++)
    insert_reg (init_table[i].name, init_table[i].number);
}

void
md_assemble (char *str)
{
  const char *er;
  short *fromP;
  char *toP = NULL;
  int m, n = 0;
  char *to_beg_P;
  int shorts_this_frag;
  fixS *fixP;

  if (!selected_cpu && !selected_arch)
    {
      /* We've not selected an architecture yet.  Set the default
	 now.  We do this lazily so that an initial .cpu or .arch directive
	 can specify.  */
      if (!m68k_set_cpu (TARGET_CPU, 1, 1))
	as_bad (_("unrecognized default cpu `%s'"), TARGET_CPU);
    }
  if (!initialized)
    m68k_init_arch ();
  
  /* In MRI mode, the instruction and operands are separated by a
     space.  Anything following the operands is a comment.  The label
     has already been removed.  */
  if (flag_mri)
    {
      char *s;
      int fields = 0;
      int infield = 0;
      int inquote = 0;

      for (s = str; *s != '\0'; s++)
	{
	  if ((*s == ' ' || *s == '\t') && ! inquote)
	    {
	      if (infield)
		{
		  ++fields;
		  if (fields >= 2)
		    {
		      *s = '\0';
		      break;
		    }
		  infield = 0;
		}
	    }
	  else
	    {
	      if (! infield)
		infield = 1;
	      if (*s == '\'')
		inquote = ! inquote;
	    }
	}
    }

  memset (&the_ins, '\0', sizeof (the_ins));
  m68k_ip (str);
  er = the_ins.error;
  if (!er)
    {
      for (n = 0; n < the_ins.numargs; n++)
	if (the_ins.operands[n].error)
	  {
	    er = the_ins.operands[n].error;
	    break;
	  }
    }
  if (er)
    {
      as_bad (_("%s -- statement `%s' ignored"), er, str);
      return;
    }

  /* If there is a current label, record that it marks an instruction.  */
  if (current_label != NULL)
    {
      current_label->text = 1;
      current_label = NULL;
    }

#ifdef OBJ_ELF
  /* Tie dwarf2 debug info to the address at the start of the insn.  */
  dwarf2_emit_insn (0);
#endif

  if (the_ins.nfrag == 0)
    {
      /* No frag hacking involved; just put it out.  */
      toP = frag_more (2 * the_ins.numo);
      fromP = &the_ins.opcode[0];
      for (m = the_ins.numo; m; --m)
	{
	  md_number_to_chars (toP, (long) (*fromP), 2);
	  toP += 2;
	  fromP++;
	}
      /* Put out symbol-dependent info.  */
      for (m = 0; m < the_ins.nrel; m++)
	{
	  switch (the_ins.reloc[m].wid)
	    {
	    case 'B':
	      n = 1;
	      break;
	    case 'b':
	      n = 1;
	      break;
	    case '3':
	      n = 1;
	      break;
	    case 'w':
	    case 'W':
	      n = 2;
	      break;
	    case 'l':
	      n = 4;
	      break;
	    default:
	      as_fatal (_("Don't know how to figure width of %c in md_assemble()"),
			the_ins.reloc[m].wid);
	    }

	  fixP = fix_new_exp (frag_now,
			      ((toP - frag_now->fr_literal)
			       - the_ins.numo * 2 + the_ins.reloc[m].n),
			      n,
			      &the_ins.reloc[m].exp,
			      the_ins.reloc[m].pcrel,
			      get_reloc_code (n, the_ins.reloc[m].pcrel,
					      the_ins.reloc[m].pic_reloc));
	  fixP->fx_pcrel_adjust = the_ins.reloc[m].pcrel_fix;
	  if (the_ins.reloc[m].wid == 'B')
	    fixP->fx_signed = 1;
	}
      return;
    }

  /* There's some frag hacking.  */
  {
    /* Calculate the max frag size.  */
    int wid;

    wid = 2 * the_ins.fragb[0].fragoff;
    for (n = 1; n < the_ins.nfrag; n++)
      wid += 2 * (the_ins.numo - the_ins.fragb[n - 1].fragoff);
    /* frag_var part.  */
    wid += 10;
    /* Make sure the whole insn fits in one chunk, in particular that
       the var part is attached, as we access one byte before the
       variable frag for byte branches.  */
    frag_grow (wid);
  }

  for (n = 0, fromP = &the_ins.opcode[0]; n < the_ins.nfrag; n++)
    {
      int wid;

      if (n == 0)
	wid = 2 * the_ins.fragb[n].fragoff;
      else
	wid = 2 * (the_ins.numo - the_ins.fragb[n - 1].fragoff);
      toP = frag_more (wid);
      to_beg_P = toP;
      shorts_this_frag = 0;
      for (m = wid / 2; m; --m)
	{
	  md_number_to_chars (toP, (long) (*fromP), 2);
	  toP += 2;
	  fromP++;
	  shorts_this_frag++;
	}
      for (m = 0; m < the_ins.nrel; m++)
	{
	  if ((the_ins.reloc[m].n) >= 2 * shorts_this_frag)
	    {
	      the_ins.reloc[m].n -= 2 * shorts_this_frag;
	      break;
	    }
	  wid = the_ins.reloc[m].wid;
	  if (wid == 0)
	    continue;
	  the_ins.reloc[m].wid = 0;
	  wid = (wid == 'b') ? 1 : (wid == 'w') ? 2 : (wid == 'l') ? 4 : 4000;

	  fixP = fix_new_exp (frag_now,
			      ((toP - frag_now->fr_literal)
			       - the_ins.numo * 2 + the_ins.reloc[m].n),
			      wid,
			      &the_ins.reloc[m].exp,
			      the_ins.reloc[m].pcrel,
			      get_reloc_code (wid, the_ins.reloc[m].pcrel,
					      the_ins.reloc[m].pic_reloc));
	  fixP->fx_pcrel_adjust = the_ins.reloc[m].pcrel_fix;
	}
      (void) frag_var (rs_machine_dependent, 10, 0,
		       (relax_substateT) (the_ins.fragb[n].fragty),
		       the_ins.fragb[n].fadd, the_ins.fragb[n].foff, to_beg_P);
    }
  n = (the_ins.numo - the_ins.fragb[n - 1].fragoff);
  shorts_this_frag = 0;
  if (n)
    {
      toP = frag_more (n * 2);
      while (n--)
	{
	  md_number_to_chars (toP, (long) (*fromP), 2);
	  toP += 2;
	  fromP++;
	  shorts_this_frag++;
	}
    }
  for (m = 0; m < the_ins.nrel; m++)
    {
      int wid;

      wid = the_ins.reloc[m].wid;
      if (wid == 0)
	continue;
      the_ins.reloc[m].wid = 0;
      wid = (wid == 'b') ? 1 : (wid == 'w') ? 2 : (wid == 'l') ? 4 : 4000;

      fixP = fix_new_exp (frag_now,
			  ((the_ins.reloc[m].n + toP - frag_now->fr_literal)
			   - shorts_this_frag * 2),
			  wid,
			  &the_ins.reloc[m].exp,
			  the_ins.reloc[m].pcrel,
			  get_reloc_code (wid, the_ins.reloc[m].pcrel,
					  the_ins.reloc[m].pic_reloc));
      fixP->fx_pcrel_adjust = the_ins.reloc[m].pcrel_fix;
    }
}

/* Comparison function used by qsort to rank the opcode entries by name.  */

static int
m68k_compare_opcode (const void * v1, const void * v2)
{
  struct m68k_opcode * op1, * op2;
  int ret;

  if (v1 == v2)
    return 0;

  op1 = *(struct m68k_opcode **) v1;
  op2 = *(struct m68k_opcode **) v2;

  /* Compare the two names.  If different, return the comparison.
     If the same, return the order they are in the opcode table.  */
  ret = strcmp (op1->name, op2->name);
  if (ret)
    return ret;
  if (op1 < op2)
    return -1;
  return 1;
}

void
md_begin (void)
{
  const struct m68k_opcode *ins;
  struct m68k_incant *hack, *slak;
  const char *retval = 0;	/* Empty string, or error msg text.  */
  int i;

  /* Set up hash tables with 68000 instructions.
     similar to what the vax assembler does.  */
  /* RMS claims the thing to do is take the m68k-opcode.h table, and make
     a copy of it at runtime, adding in the information we want but isn't
     there.  I think it'd be better to have an awk script hack the table
     at compile time.  Or even just xstr the table and use it as-is.  But
     my lord ghod hath spoken, so we do it this way.  Excuse the ugly var
     names.  */

  if (flag_mri)
    {
      flag_reg_prefix_optional = 1;
      m68k_abspcadd = 1;
      if (! m68k_rel32_from_cmdline)
	m68k_rel32 = 0;
    }

  /* First sort the opcode table into alphabetical order to seperate
     the order that the assembler wants to see the opcodes from the
     order that the disassembler wants to see them.  */
  m68k_sorted_opcodes = xmalloc (m68k_numopcodes * sizeof (* m68k_sorted_opcodes));
  if (!m68k_sorted_opcodes)
    as_fatal (_("Internal Error:  Can't allocate m68k_sorted_opcodes of size %d"),
	      m68k_numopcodes * ((int) sizeof (* m68k_sorted_opcodes)));

  for (i = m68k_numopcodes; i--;)
    m68k_sorted_opcodes[i] = m68k_opcodes + i;

  qsort (m68k_sorted_opcodes, m68k_numopcodes,
	 sizeof (m68k_sorted_opcodes[0]), m68k_compare_opcode);

  op_hash = hash_new ();

  obstack_begin (&robyn, 4000);
  for (i = 0; i < m68k_numopcodes; i++)
    {
      hack = slak = (struct m68k_incant *) obstack_alloc (&robyn, sizeof (struct m68k_incant));
      do
	{
	  ins = m68k_sorted_opcodes[i];

	  /* We *could* ignore insns that don't match our
	     arch here by just leaving them out of the hash.  */
	  slak->m_operands = ins->args;
	  slak->m_opnum = strlen (slak->m_operands) / 2;
	  slak->m_arch = ins->arch;
	  slak->m_opcode = ins->opcode;
	  /* This is kludgey.  */
	  slak->m_codenum = ((ins->match) & 0xffffL) ? 2 : 1;
	  if (i + 1 != m68k_numopcodes
	      && !strcmp (ins->name, m68k_sorted_opcodes[i + 1]->name))
	    {
	      slak->m_next = obstack_alloc (&robyn, sizeof (struct m68k_incant));
	      i++;
	    }
	  else
	    slak->m_next = 0;
	  slak = slak->m_next;
	}
      while (slak);

      retval = hash_insert (op_hash, ins->name, (char *) hack);
      if (retval)
	as_fatal (_("Internal Error:  Can't hash %s: %s"), ins->name, retval);
    }

  for (i = 0; i < m68k_numaliases; i++)
    {
      const char *name = m68k_opcode_aliases[i].primary;
      const char *alias = m68k_opcode_aliases[i].alias;
      PTR val = hash_find (op_hash, name);

      if (!val)
	as_fatal (_("Internal Error: Can't find %s in hash table"), name);
      retval = hash_insert (op_hash, alias, val);
      if (retval)
	as_fatal (_("Internal Error: Can't hash %s: %s"), alias, retval);
    }

  /* In MRI mode, all unsized branches are variable sized.  Normally,
     they are word sized.  */
  if (flag_mri)
    {
      static struct m68k_opcode_alias mri_aliases[] =
	{
	  { "bhi",	"jhi", },
	  { "bls",	"jls", },
	  { "bcc",	"jcc", },
	  { "bcs",	"jcs", },
	  { "bne",	"jne", },
	  { "beq",	"jeq", },
	  { "bvc",	"jvc", },
	  { "bvs",	"jvs", },
	  { "bpl",	"jpl", },
	  { "bmi",	"jmi", },
	  { "bge",	"jge", },
	  { "blt",	"jlt", },
	  { "bgt",	"jgt", },
	  { "ble",	"jle", },
	  { "bra",	"jra", },
	  { "bsr",	"jbsr", },
	};

      for (i = 0;
	   i < (int) (sizeof mri_aliases / sizeof mri_aliases[0]);
	   i++)
	{
	  const char *name = mri_aliases[i].primary;
	  const char *alias = mri_aliases[i].alias;
	  PTR val = hash_find (op_hash, name);

	  if (!val)
	    as_fatal (_("Internal Error: Can't find %s in hash table"), name);
	  retval = hash_jam (op_hash, alias, val);
	  if (retval)
	    as_fatal (_("Internal Error: Can't hash %s: %s"), alias, retval);
	}
    }

  for (i = 0; i < (int) sizeof (notend_table); i++)
    {
      notend_table[i] = 0;
      alt_notend_table[i] = 0;
    }

  notend_table[','] = 1;
  notend_table['{'] = 1;
  notend_table['}'] = 1;
  alt_notend_table['a'] = 1;
  alt_notend_table['A'] = 1;
  alt_notend_table['d'] = 1;
  alt_notend_table['D'] = 1;
  alt_notend_table['#'] = 1;
  alt_notend_table['&'] = 1;
  alt_notend_table['f'] = 1;
  alt_notend_table['F'] = 1;
#ifdef REGISTER_PREFIX
  alt_notend_table[REGISTER_PREFIX] = 1;
#endif

  /* We need to put '(' in alt_notend_table to handle
       cas2 %d0:%d2,%d3:%d4,(%a0):(%a1)  */
  alt_notend_table['('] = 1;

  /* We need to put '@' in alt_notend_table to handle
       cas2 %d0:%d2,%d3:%d4,@(%d0):@(%d1)  */
  alt_notend_table['@'] = 1;

  /* We need to put digits in alt_notend_table to handle
       bfextu %d0{24:1},%d0  */
  alt_notend_table['0'] = 1;
  alt_notend_table['1'] = 1;
  alt_notend_table['2'] = 1;
  alt_notend_table['3'] = 1;
  alt_notend_table['4'] = 1;
  alt_notend_table['5'] = 1;
  alt_notend_table['6'] = 1;
  alt_notend_table['7'] = 1;
  alt_notend_table['8'] = 1;
  alt_notend_table['9'] = 1;

#ifndef MIT_SYNTAX_ONLY
  /* Insert pseudo ops, these have to go into the opcode table since
     gas expects pseudo ops to start with a dot.  */
  {
    int n = 0;

    while (mote_pseudo_table[n].poc_name)
      {
	hack = obstack_alloc (&robyn, sizeof (struct m68k_incant));
	hash_insert (op_hash,
		     mote_pseudo_table[n].poc_name, (char *) hack);
	hack->m_operands = 0;
	hack->m_opnum = n;
	n++;
      }
  }
#endif

  init_regtable ();

#ifdef OBJ_ELF
  record_alignment (text_section, 2);
  record_alignment (data_section, 2);
  record_alignment (bss_section, 2);
#endif
}


/* This is called when a label is defined.  */

void
m68k_frob_label (symbolS *sym)
{
  struct label_line *n;

  n = (struct label_line *) xmalloc (sizeof *n);
  n->next = labels;
  n->label = sym;
  as_where (&n->file, &n->line);
  n->text = 0;
  labels = n;
  current_label = n;

#ifdef OBJ_ELF
  dwarf2_emit_label (sym);
#endif
}

/* This is called when a value that is not an instruction is emitted.  */

void
m68k_flush_pending_output (void)
{
  current_label = NULL;
}

/* This is called at the end of the assembly, when the final value of
   the label is known.  We warn if this is a text symbol aligned at an
   odd location.  */

void
m68k_frob_symbol (symbolS *sym)
{
  if (S_GET_SEGMENT (sym) == reg_section
      && (int) S_GET_VALUE (sym) < 0)
    {
      S_SET_SEGMENT (sym, absolute_section);
      S_SET_VALUE (sym, ~(int)S_GET_VALUE (sym));
    }
  else if ((S_GET_VALUE (sym) & 1) != 0)
    {
      struct label_line *l;

      for (l = labels; l != NULL; l = l->next)
	{
	  if (l->label == sym)
	    {
	      if (l->text)
		as_warn_where (l->file, l->line,
			       _("text label `%s' aligned to odd boundary"),
			       S_GET_NAME (sym));
	      break;
	    }
	}
    }
}

/* This is called if we go in or out of MRI mode because of the .mri
   pseudo-op.  */

void
m68k_mri_mode_change (int on)
{
  if (on)
    {
      if (! flag_reg_prefix_optional)
	{
	  flag_reg_prefix_optional = 1;
#ifdef REGISTER_PREFIX
	  init_regtable ();
#endif
	}
      m68k_abspcadd = 1;
      if (! m68k_rel32_from_cmdline)
	m68k_rel32 = 0;
    }
  else
    {
      if (! reg_prefix_optional_seen)
	{
#ifdef REGISTER_PREFIX_OPTIONAL
	  flag_reg_prefix_optional = REGISTER_PREFIX_OPTIONAL;
#else
	  flag_reg_prefix_optional = 0;
#endif
#ifdef REGISTER_PREFIX
	  init_regtable ();
#endif
	}
      m68k_abspcadd = 0;
      if (! m68k_rel32_from_cmdline)
	m68k_rel32 = 1;
    }
}

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (int type, char *litP, int *sizeP)
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char *t;

  switch (type)
    {
    case 'f':
    case 'F':
    case 's':
    case 'S':
      prec = 2;
      break;

    case 'd':
    case 'D':
    case 'r':
    case 'R':
      prec = 4;
      break;

    case 'x':
    case 'X':
      prec = 6;
      break;

    case 'p':
    case 'P':
      prec = 6;
      break;

    default:
      *sizeP = 0;
      return _("Bad call to MD_ATOF()");
    }
  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  for (wordP = words; prec--;)
    {
      md_number_to_chars (litP, (long) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

void
md_number_to_chars (char *buf, valueT val, int n)
{
  number_to_chars_bigendian (buf, val, n);
}

void
md_apply_fix (fixS *fixP, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
  offsetT val = *valP;
  addressT upper_limit;
  offsetT lower_limit;

  /* This is unnecessary but it convinces the native rs6000 compiler
     to generate the code we want.  */
  char *buf = fixP->fx_frag->fr_literal;
  buf += fixP->fx_where;
  /* End ibm compiler workaround.  */

  val = SEXT (val);

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;

#ifdef OBJ_ELF
  if (fixP->fx_addsy)
    {
      memset (buf, 0, fixP->fx_size);
      fixP->fx_addnumber = val;	/* Remember value for emit_reloc.  */

      if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
	  && !S_IS_DEFINED (fixP->fx_addsy)
	  && !S_IS_WEAK (fixP->fx_addsy))
	S_SET_WEAK (fixP->fx_addsy);
      return;
    }
#endif

  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return;

  switch (fixP->fx_size)
    {
      /* The cast to offsetT below are necessary to make code
	 correct for machines where ints are smaller than offsetT.  */
    case 1:
      *buf++ = val;
      upper_limit = 0x7f;
      lower_limit = - (offsetT) 0x80;
      break;
    case 2:
      *buf++ = (val >> 8);
      *buf++ = val;
      upper_limit = 0x7fff;
      lower_limit = - (offsetT) 0x8000;
      break;
    case 4:
      *buf++ = (val >> 24);
      *buf++ = (val >> 16);
      *buf++ = (val >> 8);
      *buf++ = val;
      upper_limit = 0x7fffffff;
      lower_limit = - (offsetT) 0x7fffffff - 1;	/* Avoid constant overflow.  */
      break;
    default:
      BAD_CASE (fixP->fx_size);
    }

  /* Fix up a negative reloc.  */
  if (fixP->fx_addsy == NULL && fixP->fx_subsy != NULL)
    {
      fixP->fx_addsy = fixP->fx_subsy;
      fixP->fx_subsy = NULL;
      fixP->fx_tcbit = 1;
    }

  /* For non-pc-relative values, it's conceivable we might get something
     like "0xff" for a byte field.  So extend the upper part of the range
     to accept such numbers.  We arbitrarily disallow "-0xff" or "0xff+0xff",
     so that we can do any range checking at all.  */
  if (! fixP->fx_pcrel && ! fixP->fx_signed)
    upper_limit = upper_limit * 2 + 1;

  if ((addressT) val > upper_limit
      && (val > 0 || val < lower_limit))
    as_bad_where (fixP->fx_file, fixP->fx_line, _("value out of range"));

  /* A one byte PC-relative reloc means a short branch.  We can't use
     a short branch with a value of 0 or -1, because those indicate
     different opcodes (branches with longer offsets).  fixup_segment
     in write.c may have clobbered fx_pcrel, so we need to examine the
     reloc type.  */
  if ((fixP->fx_pcrel
       || fixP->fx_r_type == BFD_RELOC_8_PCREL)
      && fixP->fx_size == 1
      && (fixP->fx_addsy == NULL
	  || S_IS_DEFINED (fixP->fx_addsy))
      && (val == 0 || val == -1))
    as_bad_where (fixP->fx_file, fixP->fx_line, _("invalid byte branch offset"));
}

/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size  There is UGLY
   MAGIC here. ..
   */
static void
md_convert_frag_1 (fragS *fragP)
{
  long disp;
  fixS *fixP;

  /* Address in object code of the displacement.  */
  register int object_address = fragP->fr_fix + fragP->fr_address;

  /* Address in gas core of the place to store the displacement.  */
  /* This convinces the native rs6000 compiler to generate the code we
     want.  */
  register char *buffer_address = fragP->fr_literal;
  buffer_address += fragP->fr_fix;
  /* End ibm compiler workaround.  */

  /* The displacement of the address, from current location.  */
  disp = fragP->fr_symbol ? S_GET_VALUE (fragP->fr_symbol) : 0;
  disp = (disp + fragP->fr_offset) - object_address;

  switch (fragP->fr_subtype)
    {
    case TAB (BRANCHBWL, BYTE):
    case TAB (BRABSJUNC, BYTE):
    case TAB (BRABSJCOND, BYTE):
    case TAB (BRANCHBW, BYTE):
      know (issbyte (disp));
      if (disp == 0)
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("short branch with zero offset: use :w"));
      fixP = fix_new (fragP, fragP->fr_fix - 1, 1, fragP->fr_symbol,
		      fragP->fr_offset, 1, RELAX_RELOC_PC8);
      fixP->fx_pcrel_adjust = -1;
      break;
    case TAB (BRANCHBWL, SHORT):
    case TAB (BRABSJUNC, SHORT):
    case TAB (BRABSJCOND, SHORT):
    case TAB (BRANCHBW, SHORT):
      fragP->fr_opcode[1] = 0x00;
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol, fragP->fr_offset,
	       1, RELAX_RELOC_PC16);
      fragP->fr_fix += 2;
      break;
    case TAB (BRANCHBWL, LONG):
      fragP->fr_opcode[1] = (char) 0xFF;
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol, fragP->fr_offset,
	       1, RELAX_RELOC_PC32);
      fragP->fr_fix += 4;
      break;
    case TAB (BRABSJUNC, LONG):
      if (fragP->fr_opcode[0] == 0x61)		/* jbsr */
	{
	  if (flag_keep_pcrel)
    	    as_fatal (_("Tried to convert PC relative BSR to absolute JSR"));
	  fragP->fr_opcode[0] = 0x4E;
	  fragP->fr_opcode[1] = (char) 0xB9; /* JSR with ABSL LONG operand.  */
	  fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol, fragP->fr_offset,
		   0, RELAX_RELOC_ABS32);
	  fragP->fr_fix += 4;
	}
      else if (fragP->fr_opcode[0] == 0x60)	/* jbra */
	{
	  if (flag_keep_pcrel)
	    as_fatal (_("Tried to convert PC relative branch to absolute jump"));
	  fragP->fr_opcode[0] = 0x4E;
	  fragP->fr_opcode[1] = (char) 0xF9; /* JMP with ABSL LONG operand.  */
	  fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol, fragP->fr_offset,
		   0, RELAX_RELOC_ABS32);
	  fragP->fr_fix += 4;
	}
      else
	{
	  /* This cannot happen, because jbsr and jbra are the only two
	     unconditional branches.  */
	  abort ();
	}
      break;
    case TAB (BRABSJCOND, LONG):
      if (flag_keep_pcrel)
    	as_fatal (_("Tried to convert PC relative conditional branch to absolute jump"));

      /* Only Bcc 68000 instructions can come here
	 Change bcc into b!cc/jmp absl long.  */
      fragP->fr_opcode[0] ^= 0x01;	/* Invert bcc.  */
      fragP->fr_opcode[1]  = 0x06;	/* Branch offset = 6.  */

      /* JF: these used to be fr_opcode[2,3], but they may be in a
	   different frag, in which case referring to them is a no-no.
	   Only fr_opcode[0,1] are guaranteed to work.  */
      *buffer_address++ = 0x4e;	/* put in jmp long (0x4ef9) */
      *buffer_address++ = (char) 0xf9;
      fragP->fr_fix += 2;	/* Account for jmp instruction.  */
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol,
	       fragP->fr_offset, 0, RELAX_RELOC_ABS32);
      fragP->fr_fix += 4;
      break;
    case TAB (FBRANCH, SHORT):
      know ((fragP->fr_opcode[1] & 0x40) == 0);
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol, fragP->fr_offset,
	       1, RELAX_RELOC_PC16);
      fragP->fr_fix += 2;
      break;
    case TAB (FBRANCH, LONG):
      fragP->fr_opcode[1] |= 0x40;	/* Turn on LONG bit.  */
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol, fragP->fr_offset,
	       1, RELAX_RELOC_PC32);
      fragP->fr_fix += 4;
      break;
    case TAB (DBCCLBR, SHORT):
    case TAB (DBCCABSJ, SHORT):
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol, fragP->fr_offset,
	       1, RELAX_RELOC_PC16);
      fragP->fr_fix += 2;
      break;
    case TAB (DBCCLBR, LONG):
      /* Only DBcc instructions can come here.
	 Change dbcc into dbcc/bral.
	 JF: these used to be fr_opcode[2-7], but that's wrong.  */
      if (flag_keep_pcrel)
    	as_fatal (_("Tried to convert DBcc to absolute jump"));

      *buffer_address++ = 0x00;	/* Branch offset = 4.  */
      *buffer_address++ = 0x04;
      *buffer_address++ = 0x60;	/* Put in bra pc+6.  */
      *buffer_address++ = 0x06;
      *buffer_address++ = 0x60;     /* Put in bral (0x60ff).  */
      *buffer_address++ = (char) 0xff;

      fragP->fr_fix += 6;	/* Account for bra/jmp instructions.  */
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol, fragP->fr_offset, 1,
	       RELAX_RELOC_PC32);
      fragP->fr_fix += 4;
      break;
    case TAB (DBCCABSJ, LONG):
      /* Only DBcc instructions can come here.
	 Change dbcc into dbcc/jmp.
	 JF: these used to be fr_opcode[2-7], but that's wrong.  */
      if (flag_keep_pcrel)
    	as_fatal (_("Tried to convert PC relative conditional branch to absolute jump"));

      *buffer_address++ = 0x00;		/* Branch offset = 4.  */
      *buffer_address++ = 0x04;
      *buffer_address++ = 0x60;		/* Put in bra pc + 6.  */
      *buffer_address++ = 0x06;
      *buffer_address++ = 0x4e;		/* Put in jmp long (0x4ef9).  */
      *buffer_address++ = (char) 0xf9;

      fragP->fr_fix += 6;		/* Account for bra/jmp instructions.  */
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol, fragP->fr_offset, 0,
	       RELAX_RELOC_ABS32);
      fragP->fr_fix += 4;
      break;
    case TAB (PCREL1632, SHORT):
      fragP->fr_opcode[1] &= ~0x3F;
      fragP->fr_opcode[1] |= 0x3A; /* 072 - mode 7.2 */
      fix_new (fragP, (int) (fragP->fr_fix), 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, RELAX_RELOC_PC16);
      fragP->fr_fix += 2;
      break;
    case TAB (PCREL1632, LONG):
      /* Already set to mode 7.3; this indicates: PC indirect with
	 suppressed index, 32-bit displacement.  */
      *buffer_address++ = 0x01;
      *buffer_address++ = 0x70;
      fragP->fr_fix += 2;
      fixP = fix_new (fragP, (int) (fragP->fr_fix), 4, fragP->fr_symbol,
		      fragP->fr_offset, 1, RELAX_RELOC_PC32);
      fixP->fx_pcrel_adjust = 2;
      fragP->fr_fix += 4;
      break;
    case TAB (PCINDEX, BYTE):
      assert (fragP->fr_fix >= 2);
      buffer_address[-2] &= ~1;
      fixP = fix_new (fragP, fragP->fr_fix - 1, 1, fragP->fr_symbol,
		      fragP->fr_offset, 1, RELAX_RELOC_PC8);
      fixP->fx_pcrel_adjust = 1;
      break;
    case TAB (PCINDEX, SHORT):
      assert (fragP->fr_fix >= 2);
      buffer_address[-2] |= 0x1;
      buffer_address[-1] = 0x20;
      fixP = fix_new (fragP, (int) (fragP->fr_fix), 2, fragP->fr_symbol,
		      fragP->fr_offset, 1, RELAX_RELOC_PC16);
      fixP->fx_pcrel_adjust = 2;
      fragP->fr_fix += 2;
      break;
    case TAB (PCINDEX, LONG):
      assert (fragP->fr_fix >= 2);
      buffer_address[-2] |= 0x1;
      buffer_address[-1] = 0x30;
      fixP = fix_new (fragP, (int) (fragP->fr_fix), 4, fragP->fr_symbol,
		      fragP->fr_offset, 1, RELAX_RELOC_PC32);
      fixP->fx_pcrel_adjust = 2;
      fragP->fr_fix += 4;
      break;
    case TAB (ABSTOPCREL, SHORT):
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol, fragP->fr_offset,
	       1, RELAX_RELOC_PC16);
      fragP->fr_fix += 2;
      break;
    case TAB (ABSTOPCREL, LONG):
      if (flag_keep_pcrel)
    	as_fatal (_("Tried to convert PC relative conditional branch to absolute jump"));
      /* The thing to do here is force it to ABSOLUTE LONG, since
	 ABSTOPCREL is really trying to shorten an ABSOLUTE address anyway.  */
      if ((fragP->fr_opcode[1] & 0x3F) != 0x3A)
	abort ();
      fragP->fr_opcode[1] &= ~0x3F;
      fragP->fr_opcode[1] |= 0x39;	/* Mode 7.1 */
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol, fragP->fr_offset,
	       0, RELAX_RELOC_ABS32);
      fragP->fr_fix += 4;
      break;
    }
}

void
md_convert_frag (bfd *abfd ATTRIBUTE_UNUSED,
		 segT sec ATTRIBUTE_UNUSED,
		 fragS *fragP)
{
  md_convert_frag_1 (fragP);
}

/* Force truly undefined symbols to their maximum size, and generally set up
   the frag list to be relaxed
   */
int
md_estimate_size_before_relax (fragS *fragP, segT segment)
{
  /* Handle SZ_UNDEF first, it can be changed to BYTE or SHORT.  */
  switch (fragP->fr_subtype)
    {
    case TAB (BRANCHBWL, SZ_UNDEF):
    case TAB (BRABSJUNC, SZ_UNDEF):
    case TAB (BRABSJCOND, SZ_UNDEF):
      {
	if (S_GET_SEGMENT (fragP->fr_symbol) == segment
	    && relaxable_symbol (fragP->fr_symbol))
	  {
	    fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), BYTE);
	  }
	else if (flag_short_refs)
	  {
	    /* Symbol is undefined and we want short ref.  */
	    fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), SHORT);
	  }
	else
	  {
	    /* Symbol is still undefined.  Make it LONG.  */
	    fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), LONG);
	  }
	break;
      }

    case TAB (BRANCHBW, SZ_UNDEF):
      {
	if (S_GET_SEGMENT (fragP->fr_symbol) == segment
	    && relaxable_symbol (fragP->fr_symbol))
	  {
	    fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), BYTE);
	  }
	else
	  {
	    /* Symbol is undefined and we don't have long branches.  */
	    fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), SHORT);
	  }
	break;
      }

    case TAB (FBRANCH, SZ_UNDEF):
    case TAB (DBCCLBR, SZ_UNDEF):
    case TAB (DBCCABSJ, SZ_UNDEF):
    case TAB (PCREL1632, SZ_UNDEF):
      {
	if ((S_GET_SEGMENT (fragP->fr_symbol) == segment
	     && relaxable_symbol (fragP->fr_symbol))
	    || flag_short_refs)
	  {
	    fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), SHORT);
	  }
	else
	  {
	    fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), LONG);
	  }
	break;
      }

    case TAB (PCINDEX, SZ_UNDEF):
      if ((S_GET_SEGMENT (fragP->fr_symbol) == segment
	   && relaxable_symbol (fragP->fr_symbol)))
	{
	  fragP->fr_subtype = TAB (PCINDEX, BYTE);
	}
      else
	{
	  fragP->fr_subtype = TAB (PCINDEX, LONG);
	}
      break;

    case TAB (ABSTOPCREL, SZ_UNDEF):
      {
	if ((S_GET_SEGMENT (fragP->fr_symbol) == segment
	     && relaxable_symbol (fragP->fr_symbol)))
	  {
	    fragP->fr_subtype = TAB (ABSTOPCREL, SHORT);
	  }
	else
	  {
	    fragP->fr_subtype = TAB (ABSTOPCREL, LONG);
	  }
	break;
      }

    default:
      break;
    }

  /* Now that SZ_UNDEF are taken care of, check others.  */
  switch (fragP->fr_subtype)
    {
    case TAB (BRANCHBWL, BYTE):
    case TAB (BRABSJUNC, BYTE):
    case TAB (BRABSJCOND, BYTE):
    case TAB (BRANCHBW, BYTE):
      /* We can't do a short jump to the next instruction, so in that
	 case we force word mode.  If the symbol is at the start of a
	 frag, and it is the next frag with any data in it (usually
	 this is just the next frag, but assembler listings may
	 introduce empty frags), we must use word mode.  */
      if (fragP->fr_symbol)
	{
	  fragS *sym_frag;

	  sym_frag = symbol_get_frag (fragP->fr_symbol);
	  if (S_GET_VALUE (fragP->fr_symbol) == sym_frag->fr_address)
	    {
	      fragS *l;

	      for (l = fragP->fr_next; l && l != sym_frag; l = l->fr_next)
		if (l->fr_fix != 0)
		  break;
	      if (l == sym_frag)
		fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), SHORT);
	    }
	}
      break;
    default:
      break;
    }
  return md_relax_table[fragP->fr_subtype].rlx_length;
}

#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
/* the bit-field entries in the relocation_info struct plays hell
   with the byte-order problems of cross-assembly.  So as a hack,
   I added this mach. dependent ri twiddler.  Ugly, but it gets
   you there. -KWK  */
/* on m68k: first 4 bytes are normal unsigned long, next three bytes
   are symbolnum, most sig. byte first.  Last byte is broken up with
   bit 7 as pcrel, bits 6 & 5 as length, bit 4 as pcrel, and the lower
   nibble as nuthin. (on Sun 3 at least) */
/* Translate the internal relocation information into target-specific
   format.  */
#ifdef comment
void
md_ri_to_chars (char *the_bytes, struct reloc_info_generic *ri)
{
  /* This is easy.  */
  md_number_to_chars (the_bytes, ri->r_address, 4);
  /* Now the fun stuff.  */
  the_bytes[4] = (ri->r_symbolnum >> 16) & 0x0ff;
  the_bytes[5] = (ri->r_symbolnum >>  8) & 0x0ff;
  the_bytes[6] =  ri->r_symbolnum        & 0x0ff;
  the_bytes[7] = (((ri->r_pcrel << 7) & 0x80)
		  | ((ri->r_length << 5) & 0x60)
		  | ((ri->r_extern << 4) & 0x10));
}

#endif

#endif /* OBJ_AOUT or OBJ_BOUT */

#ifndef WORKING_DOT_WORD
int md_short_jump_size = 4;
int md_long_jump_size = 6;

void
md_create_short_jump (char *ptr, addressT from_addr, addressT to_addr,
		      fragS *frag ATTRIBUTE_UNUSED,
		      symbolS *to_symbol ATTRIBUTE_UNUSED)
{
  valueT offset;

  offset = to_addr - (from_addr + 2);

  md_number_to_chars (ptr, (valueT) 0x6000, 2);
  md_number_to_chars (ptr + 2, (valueT) offset, 2);
}

void
md_create_long_jump (char *ptr, addressT from_addr, addressT to_addr,
		     fragS *frag, symbolS *to_symbol)
{
  valueT offset;

  if (!HAVE_LONG_BRANCH (current_architecture))
    {
      if (flag_keep_pcrel)
    	as_fatal (_("Tried to convert PC relative branch to absolute jump"));
      offset = to_addr - S_GET_VALUE (to_symbol);
      md_number_to_chars (ptr, (valueT) 0x4EF9, 2);
      md_number_to_chars (ptr + 2, (valueT) offset, 4);
      fix_new (frag, (ptr + 2) - frag->fr_literal, 4, to_symbol, (offsetT) 0,
	       0, NO_RELOC);
    }
  else
    {
      offset = to_addr - (from_addr + 2);
      md_number_to_chars (ptr, (valueT) 0x60ff, 2);
      md_number_to_chars (ptr + 2, (valueT) offset, 4);
    }
}

#endif

/* Different values of OK tell what its OK to return.  Things that
   aren't OK are an error (what a shock, no?)

   0:  Everything is OK
   10:  Absolute 1:8	   only
   20:  Absolute 0:7	   only
   30:  absolute 0:15	   only
   40:  Absolute 0:31	   only
   50:  absolute 0:127	   only
   55:  absolute -64:63    only
   60:  absolute -128:127  only
   70:  absolute 0:4095	   only
   80:  absolute -1, 1:7   only
   90:  No bignums.          */

static int
get_num (struct m68k_exp *exp, int ok)
{
  if (exp->exp.X_op == O_absent)
    {
      /* Do the same thing the VAX asm does.  */
      op (exp) = O_constant;
      adds (exp) = 0;
      subs (exp) = 0;
      offs (exp) = 0;
      if (ok == 10)
	{
	  as_warn (_("expression out of range: defaulting to 1"));
	  offs (exp) = 1;
	}
    }
  else if (exp->exp.X_op == O_constant)
    {
      switch (ok)
	{
	case 10:
	  if ((valueT) TRUNC (offs (exp)) - 1 > 7)
	    {
	      as_warn (_("expression out of range: defaulting to 1"));
	      offs (exp) = 1;
	    }
	  break;
	case 20:
	  if ((valueT) TRUNC (offs (exp)) > 7)
	    goto outrange;
	  break;
	case 30:
	  if ((valueT) TRUNC (offs (exp)) > 15)
	    goto outrange;
	  break;
	case 40:
	  if ((valueT) TRUNC (offs (exp)) > 32)
	    goto outrange;
	  break;
	case 50:
	  if ((valueT) TRUNC (offs (exp)) > 127)
	    goto outrange;
	  break;
	case 55:
	  if ((valueT) SEXT (offs (exp)) + 64 > 127)
	    goto outrange;
	  break;
	case 60:
	  if ((valueT) SEXT (offs (exp)) + 128 > 255)
	    goto outrange;
	  break;
	case 70:
	  if ((valueT) TRUNC (offs (exp)) > 4095)
	    {
	    outrange:
	      as_warn (_("expression out of range: defaulting to 0"));
	      offs (exp) = 0;
	    }
	  break;
	case 80:
	  if ((valueT) TRUNC (offs (exp)) != 0xffffffff
              && (valueT) TRUNC (offs (exp)) - 1 > 6)
	    {
	      as_warn (_("expression out of range: defaulting to 1"));
	      offs (exp) = 1;
	    }
	  break;
	default:
	  break;
	}
    }
  else if (exp->exp.X_op == O_big)
    {
      if (offs (exp) <= 0	/* flonum.  */
	  && (ok == 90		/* no bignums */
	      || (ok > 10	/* Small-int ranges including 0 ok.  */
		  /* If we have a flonum zero, a zero integer should
		     do as well (e.g., in moveq).  */
		  && generic_floating_point_number.exponent == 0
		  && generic_floating_point_number.low[0] == 0)))
	{
	  /* HACK! Turn it into a long.  */
	  LITTLENUM_TYPE words[6];

	  gen_to_words (words, 2, 8L);	/* These numbers are magic!  */
	  op (exp) = O_constant;
	  adds (exp) = 0;
	  subs (exp) = 0;
	  offs (exp) = words[1] | (words[0] << 16);
	}
      else if (ok != 0)
	{
	  op (exp) = O_constant;
	  adds (exp) = 0;
	  subs (exp) = 0;
	  offs (exp) = (ok == 10) ? 1 : 0;
	  as_warn (_("Can't deal with expression; defaulting to %ld"),
		   (long) offs (exp));
	}
    }
  else
    {
      if (ok >= 10 && ok <= 80)
	{
	  op (exp) = O_constant;
	  adds (exp) = 0;
	  subs (exp) = 0;
	  offs (exp) = (ok == 10) ? 1 : 0;
	  as_warn (_("Can't deal with expression; defaulting to %ld"),
		   (long) offs (exp));
	}
    }

  if (exp->size != SIZE_UNSPEC)
    {
      switch (exp->size)
	{
	case SIZE_UNSPEC:
	case SIZE_LONG:
	  break;
	case SIZE_BYTE:
	  if (!isbyte (offs (exp)))
	    as_warn (_("expression doesn't fit in BYTE"));
	  break;
	case SIZE_WORD:
	  if (!isword (offs (exp)))
	    as_warn (_("expression doesn't fit in WORD"));
	  break;
	}
    }

  return offs (exp);
}

/* These are the back-ends for the various machine dependent pseudo-ops.  */

static void
s_data1 (int ignore ATTRIBUTE_UNUSED)
{
  subseg_set (data_section, 1);
  demand_empty_rest_of_line ();
}

static void
s_data2 (int ignore ATTRIBUTE_UNUSED)
{
  subseg_set (data_section, 2);
  demand_empty_rest_of_line ();
}

static void
s_bss (int ignore ATTRIBUTE_UNUSED)
{
  /* We don't support putting frags in the BSS segment, we fake it
     by marking in_bss, then looking at s_skip for clues.  */

  subseg_set (bss_section, 0);
  demand_empty_rest_of_line ();
}

static void
s_even (int ignore ATTRIBUTE_UNUSED)
{
  register int temp;
  register long temp_fill;

  temp = 1;			/* JF should be 2? */
  temp_fill = get_absolute_expression ();
  if (!need_pass_2)		/* Never make frag if expect extra pass.  */
    frag_align (temp, (int) temp_fill, 0);
  demand_empty_rest_of_line ();
  record_alignment (now_seg, temp);
}

static void
s_proc (int ignore ATTRIBUTE_UNUSED)
{
  demand_empty_rest_of_line ();
}

/* Pseudo-ops handled for MRI compatibility.  */

/* This function returns non-zero if the argument is a conditional
   pseudo-op.  This is called when checking whether a pending
   alignment is needed.  */

int
m68k_conditional_pseudoop (pseudo_typeS *pop)
{
  return (pop->poc_handler == s_mri_if
	  || pop->poc_handler == s_mri_else);
}

/* Handle an MRI style chip specification.  */

static void
mri_chip (void)
{
  char *s;
  char c;
  int i;

  s = input_line_pointer;
  /* We can't use get_symbol_end since the processor names are not proper
     symbols.  */
  while (is_part_of_name (c = *input_line_pointer++))
    ;
  *--input_line_pointer = 0;
  for (i = 0; m68k_cpus[i].name; i++)
    if (strcasecmp (s, m68k_cpus[i].name) == 0)
      break;
  if (!m68k_cpus[i].name)
    {
      as_bad (_("%s: unrecognized processor name"), s);
      *input_line_pointer = c;
      ignore_rest_of_line ();
      return;
    }
  *input_line_pointer = c;

  if (*input_line_pointer == '/')
    current_architecture = 0;
  else
    current_architecture &= m68881 | m68851;
  current_architecture |= m68k_cpus[i].arch & ~(m68881 | m68851);
  control_regs = m68k_cpus[i].control_regs;

  while (*input_line_pointer == '/')
    {
      ++input_line_pointer;
      s = input_line_pointer;
      /* We can't use get_symbol_end since the processor names are not
	 proper symbols.  */
      while (is_part_of_name (c = *input_line_pointer++))
	;
      *--input_line_pointer = 0;
      if (strcmp (s, "68881") == 0)
	current_architecture |= m68881;
      else if (strcmp (s, "68851") == 0)
	current_architecture |= m68851;
      *input_line_pointer = c;
    }
}

/* The MRI CHIP pseudo-op.  */

static void
s_chip (int ignore ATTRIBUTE_UNUSED)
{
  char *stop = NULL;
  char stopc;

  if (flag_mri)
    stop = mri_comment_field (&stopc);
  mri_chip ();
  if (flag_mri)
    mri_comment_end (stop, stopc);
  demand_empty_rest_of_line ();
}

/* The MRI FOPT pseudo-op.  */

static void
s_fopt (int ignore ATTRIBUTE_UNUSED)
{
  SKIP_WHITESPACE ();

  if (strncasecmp (input_line_pointer, "ID=", 3) == 0)
    {
      int temp;

      input_line_pointer += 3;
      temp = get_absolute_expression ();
      if (temp < 0 || temp > 7)
	as_bad (_("bad coprocessor id"));
      else
	m68k_float_copnum = COP0 + temp;
    }
  else
    {
      as_bad (_("unrecognized fopt option"));
      ignore_rest_of_line ();
      return;
    }

  demand_empty_rest_of_line ();
}

/* The structure used to handle the MRI OPT pseudo-op.  */

struct opt_action
{
  /* The name of the option.  */
  const char *name;

  /* If this is not NULL, just call this function.  The first argument
     is the ARG field of this structure, the second argument is
     whether the option was negated.  */
  void (*pfn) (int arg, int on);

  /* If this is not NULL, and the PFN field is NULL, set the variable
     this points to.  Set it to the ARG field if the option was not
     negated, and the NOTARG field otherwise.  */
  int *pvar;

  /* The value to pass to PFN or to assign to *PVAR.  */
  int arg;

  /* The value to assign to *PVAR if the option is negated.  If PFN is
     NULL, and PVAR is not NULL, and ARG and NOTARG are the same, then
     the option may not be negated.  */
  int notarg;
};

/* The table used to handle the MRI OPT pseudo-op.  */

static void skip_to_comma (int, int);
static void opt_nest (int, int);
static void opt_chip (int, int);
static void opt_list (int, int);
static void opt_list_symbols (int, int);

static const struct opt_action opt_table[] =
{
  { "abspcadd", 0, &m68k_abspcadd, 1, 0 },

  /* We do relaxing, so there is little use for these options.  */
  { "b", 0, 0, 0, 0 },
  { "brs", 0, 0, 0, 0 },
  { "brb", 0, 0, 0, 0 },
  { "brl", 0, 0, 0, 0 },
  { "brw", 0, 0, 0, 0 },

  { "c", 0, 0, 0, 0 },
  { "cex", 0, 0, 0, 0 },
  { "case", 0, &symbols_case_sensitive, 1, 0 },
  { "cl", 0, 0, 0, 0 },
  { "cre", 0, 0, 0, 0 },
  { "d", 0, &flag_keep_locals, 1, 0 },
  { "e", 0, 0, 0, 0 },
  { "f", 0, &flag_short_refs, 1, 0 },
  { "frs", 0, &flag_short_refs, 1, 0 },
  { "frl", 0, &flag_short_refs, 0, 1 },
  { "g", 0, 0, 0, 0 },
  { "i", 0, 0, 0, 0 },
  { "m", 0, 0, 0, 0 },
  { "mex", 0, 0, 0, 0 },
  { "mc", 0, 0, 0, 0 },
  { "md", 0, 0, 0, 0 },
  { "nest", opt_nest, 0, 0, 0 },
  { "next", skip_to_comma, 0, 0, 0 },
  { "o", 0, 0, 0, 0 },
  { "old", 0, 0, 0, 0 },
  { "op", skip_to_comma, 0, 0, 0 },
  { "pco", 0, 0, 0, 0 },
  { "p", opt_chip, 0, 0, 0 },
  { "pcr", 0, 0, 0, 0 },
  { "pcs", 0, 0, 0, 0 },
  { "r", 0, 0, 0, 0 },
  { "quick", 0, &m68k_quick, 1, 0 },
  { "rel32", 0, &m68k_rel32, 1, 0 },
  { "s", opt_list, 0, 0, 0 },
  { "t", opt_list_symbols, 0, 0, 0 },
  { "w", 0, &flag_no_warnings, 0, 1 },
  { "x", 0, 0, 0, 0 }
};

#define OPTCOUNT ((int) (sizeof opt_table / sizeof opt_table[0]))

/* The MRI OPT pseudo-op.  */

static void
s_opt (int ignore ATTRIBUTE_UNUSED)
{
  do
    {
      int t;
      char *s;
      char c;
      int i;
      const struct opt_action *o;

      SKIP_WHITESPACE ();

      t = 1;
      if (*input_line_pointer == '-')
	{
	  ++input_line_pointer;
	  t = 0;
	}
      else if (strncasecmp (input_line_pointer, "NO", 2) == 0)
	{
	  input_line_pointer += 2;
	  t = 0;
	}

      s = input_line_pointer;
      c = get_symbol_end ();

      for (i = 0, o = opt_table; i < OPTCOUNT; i++, o++)
	{
	  if (strcasecmp (s, o->name) == 0)
	    {
	      if (o->pfn)
		{
		  /* Restore input_line_pointer now in case the option
		     takes arguments.  */
		  *input_line_pointer = c;
		  (*o->pfn) (o->arg, t);
		}
	      else if (o->pvar != NULL)
		{
		  if (! t && o->arg == o->notarg)
		    as_bad (_("option `%s' may not be negated"), s);
		  *input_line_pointer = c;
		  *o->pvar = t ? o->arg : o->notarg;
		}
	      else
		*input_line_pointer = c;
	      break;
	    }
	}
      if (i >= OPTCOUNT)
	{
	  as_bad (_("option `%s' not recognized"), s);
	  *input_line_pointer = c;
	}
    }
  while (*input_line_pointer++ == ',');

  /* Move back to terminating character.  */
  --input_line_pointer;
  demand_empty_rest_of_line ();
}

/* Skip ahead to a comma.  This is used for OPT options which we do
   not support and which take arguments.  */

static void
skip_to_comma (int arg ATTRIBUTE_UNUSED, int on ATTRIBUTE_UNUSED)
{
  while (*input_line_pointer != ','
	 && ! is_end_of_line[(unsigned char) *input_line_pointer])
    ++input_line_pointer;
}

/* Handle the OPT NEST=depth option.  */

static void
opt_nest (int arg ATTRIBUTE_UNUSED, int on ATTRIBUTE_UNUSED)
{
  if (*input_line_pointer != '=')
    {
      as_bad (_("bad format of OPT NEST=depth"));
      return;
    }

  ++input_line_pointer;
  max_macro_nest = get_absolute_expression ();
}

/* Handle the OPT P=chip option.  */

static void
opt_chip (int arg ATTRIBUTE_UNUSED, int on ATTRIBUTE_UNUSED)
{
  if (*input_line_pointer != '=')
    {
      /* This is just OPT P, which we do not support.  */
      return;
    }

  ++input_line_pointer;
  mri_chip ();
}

/* Handle the OPT S option.  */

static void
opt_list (int arg ATTRIBUTE_UNUSED, int on)
{
  listing_list (on);
}

/* Handle the OPT T option.  */

static void
opt_list_symbols (int arg ATTRIBUTE_UNUSED, int on)
{
  if (on)
    listing |= LISTING_SYMBOLS;
  else
    listing &= ~LISTING_SYMBOLS;
}

/* Handle the MRI REG pseudo-op.  */

static void
s_reg (int ignore ATTRIBUTE_UNUSED)
{
  char *s;
  int c;
  struct m68k_op rop;
  int mask;
  char *stop = NULL;
  char stopc;

  if (line_label == NULL)
    {
      as_bad (_("missing label"));
      ignore_rest_of_line ();
      return;
    }

  if (flag_mri)
    stop = mri_comment_field (&stopc);

  SKIP_WHITESPACE ();

  s = input_line_pointer;
  while (ISALNUM (*input_line_pointer)
#ifdef REGISTER_PREFIX
	 || *input_line_pointer == REGISTER_PREFIX
#endif
	 || *input_line_pointer == '/'
	 || *input_line_pointer == '-')
    ++input_line_pointer;
  c = *input_line_pointer;
  *input_line_pointer = '\0';

  if (m68k_ip_op (s, &rop) != 0)
    {
      if (rop.error == NULL)
	as_bad (_("bad register list"));
      else
	as_bad (_("bad register list: %s"), rop.error);
      *input_line_pointer = c;
      ignore_rest_of_line ();
      return;
    }

  *input_line_pointer = c;

  if (rop.mode == REGLST)
    mask = rop.mask;
  else if (rop.mode == DREG)
    mask = 1 << (rop.reg - DATA0);
  else if (rop.mode == AREG)
    mask = 1 << (rop.reg - ADDR0 + 8);
  else if (rop.mode == FPREG)
    mask = 1 << (rop.reg - FP0 + 16);
  else if (rop.mode == CONTROL
	   && rop.reg == FPI)
    mask = 1 << 24;
  else if (rop.mode == CONTROL
	   && rop.reg == FPS)
    mask = 1 << 25;
  else if (rop.mode == CONTROL
	   && rop.reg == FPC)
    mask = 1 << 26;
  else
    {
      as_bad (_("bad register list"));
      ignore_rest_of_line ();
      return;
    }

  S_SET_SEGMENT (line_label, reg_section);
  S_SET_VALUE (line_label, ~mask);
  symbol_set_frag (line_label, &zero_address_frag);

  if (flag_mri)
    mri_comment_end (stop, stopc);

  demand_empty_rest_of_line ();
}

/* This structure is used for the MRI SAVE and RESTORE pseudo-ops.  */

struct save_opts
{
  struct save_opts *next;
  int abspcadd;
  int symbols_case_sensitive;
  int keep_locals;
  int short_refs;
  int architecture;
  const enum m68k_register *control_regs;
  int quick;
  int rel32;
  int listing;
  int no_warnings;
  /* FIXME: We don't save OPT S.  */
};

/* This variable holds the stack of saved options.  */

static struct save_opts *save_stack;

/* The MRI SAVE pseudo-op.  */

static void
s_save (int ignore ATTRIBUTE_UNUSED)
{
  struct save_opts *s;

  s = (struct save_opts *) xmalloc (sizeof (struct save_opts));
  s->abspcadd = m68k_abspcadd;
  s->symbols_case_sensitive = symbols_case_sensitive;
  s->keep_locals = flag_keep_locals;
  s->short_refs = flag_short_refs;
  s->architecture = current_architecture;
  s->control_regs = control_regs;
  s->quick = m68k_quick;
  s->rel32 = m68k_rel32;
  s->listing = listing;
  s->no_warnings = flag_no_warnings;

  s->next = save_stack;
  save_stack = s;

  demand_empty_rest_of_line ();
}

/* The MRI RESTORE pseudo-op.  */

static void
s_restore (int ignore ATTRIBUTE_UNUSED)
{
  struct save_opts *s;

  if (save_stack == NULL)
    {
      as_bad (_("restore without save"));
      ignore_rest_of_line ();
      return;
    }

  s = save_stack;
  save_stack = s->next;

  m68k_abspcadd = s->abspcadd;
  symbols_case_sensitive = s->symbols_case_sensitive;
  flag_keep_locals = s->keep_locals;
  flag_short_refs = s->short_refs;
  current_architecture = s->architecture;
  control_regs = s->control_regs;
  m68k_quick = s->quick;
  m68k_rel32 = s->rel32;
  listing = s->listing;
  flag_no_warnings = s->no_warnings;

  free (s);

  demand_empty_rest_of_line ();
}

/* Types of MRI structured control directives.  */

enum mri_control_type
{
  mri_for,
  mri_if,
  mri_repeat,
  mri_while
};

/* This structure is used to stack the MRI structured control
   directives.  */

struct mri_control_info
{
  /* The directive within which this one is enclosed.  */
  struct mri_control_info *outer;

  /* The type of directive.  */
  enum mri_control_type type;

  /* Whether an ELSE has been in an IF.  */
  int else_seen;

  /* The add or sub statement at the end of a FOR.  */
  char *incr;

  /* The label of the top of a FOR or REPEAT loop.  */
  char *top;

  /* The label to jump to for the next iteration, or the else
     expression of a conditional.  */
  char *next;

  /* The label to jump to to break out of the loop, or the label past
     the end of a conditional.  */
  char *bottom;
};

/* The stack of MRI structured control directives.  */

static struct mri_control_info *mri_control_stack;

/* The current MRI structured control directive index number, used to
   generate label names.  */

static int mri_control_index;

/* Assemble an instruction for an MRI structured control directive.  */

static void
mri_assemble (char *str)
{
  char *s;

  /* md_assemble expects the opcode to be in lower case.  */
  for (s = str; *s != ' ' && *s != '\0'; s++)
    *s = TOLOWER (*s);

  md_assemble (str);
}

/* Generate a new MRI label structured control directive label name.  */

static char *
mri_control_label (void)
{
  char *n;

  n = (char *) xmalloc (20);
  sprintf (n, "%smc%d", FAKE_LABEL_NAME, mri_control_index);
  ++mri_control_index;
  return n;
}

/* Create a new MRI structured control directive.  */

static struct mri_control_info *
push_mri_control (enum mri_control_type type)
{
  struct mri_control_info *n;

  n = (struct mri_control_info *) xmalloc (sizeof (struct mri_control_info));

  n->type = type;
  n->else_seen = 0;
  if (type == mri_if || type == mri_while)
    n->top = NULL;
  else
    n->top = mri_control_label ();
  n->next = mri_control_label ();
  n->bottom = mri_control_label ();

  n->outer = mri_control_stack;
  mri_control_stack = n;

  return n;
}

/* Pop off the stack of MRI structured control directives.  */

static void
pop_mri_control (void)
{
  struct mri_control_info *n;

  n = mri_control_stack;
  mri_control_stack = n->outer;
  if (n->top != NULL)
    free (n->top);
  free (n->next);
  free (n->bottom);
  free (n);
}

/* Recognize a condition code in an MRI structured control expression.  */

static int
parse_mri_condition (int *pcc)
{
  char c1, c2;

  know (*input_line_pointer == '<');

  ++input_line_pointer;
  c1 = *input_line_pointer++;
  c2 = *input_line_pointer++;

  if (*input_line_pointer != '>')
    {
      as_bad (_("syntax error in structured control directive"));
      return 0;
    }

  ++input_line_pointer;
  SKIP_WHITESPACE ();

  c1 = TOLOWER (c1);
  c2 = TOLOWER (c2);

  *pcc = (c1 << 8) | c2;

  return 1;
}

/* Parse a single operand in an MRI structured control expression.  */

static int
parse_mri_control_operand (int *pcc, char **leftstart, char **leftstop,
			   char **rightstart, char **rightstop)
{
  char *s;

  SKIP_WHITESPACE ();

  *pcc = -1;
  *leftstart = NULL;
  *leftstop = NULL;
  *rightstart = NULL;
  *rightstop = NULL;

  if (*input_line_pointer == '<')
    {
      /* It's just a condition code.  */
      return parse_mri_condition (pcc);
    }

  /* Look ahead for the condition code.  */
  for (s = input_line_pointer; *s != '\0'; ++s)
    {
      if (*s == '<' && s[1] != '\0' && s[2] != '\0' && s[3] == '>')
	break;
    }
  if (*s == '\0')
    {
      as_bad (_("missing condition code in structured control directive"));
      return 0;
    }

  *leftstart = input_line_pointer;
  *leftstop = s;
  if (*leftstop > *leftstart
      && ((*leftstop)[-1] == ' ' || (*leftstop)[-1] == '\t'))
    --*leftstop;

  input_line_pointer = s;
  if (! parse_mri_condition (pcc))
    return 0;

  /* Look ahead for AND or OR or end of line.  */
  for (s = input_line_pointer; *s != '\0'; ++s)
    {
      /* We must make sure we don't misinterpret AND/OR at the end of labels!
         if d0 <eq> #FOOAND and d1 <ne> #BAROR then
                        ^^^                 ^^ */
      if ((s == input_line_pointer
	   || *(s-1) == ' '
	   || *(s-1) == '\t')
	  && ((strncasecmp (s, "AND", 3) == 0
	       && (s[3] == '.' || ! is_part_of_name (s[3])))
	      || (strncasecmp (s, "OR", 2) == 0
		  && (s[2] == '.' || ! is_part_of_name (s[2])))))
	break;
    }

  *rightstart = input_line_pointer;
  *rightstop = s;
  if (*rightstop > *rightstart
      && ((*rightstop)[-1] == ' ' || (*rightstop)[-1] == '\t'))
    --*rightstop;

  input_line_pointer = s;

  return 1;
}

#define MCC(b1, b2) (((b1) << 8) | (b2))

/* Swap the sense of a condition.  This changes the condition so that
   it generates the same result when the operands are swapped.  */

static int
swap_mri_condition (int cc)
{
  switch (cc)
    {
    case MCC ('h', 'i'): return MCC ('c', 's');
    case MCC ('l', 's'): return MCC ('c', 'c');
    /* <HS> is an alias for <CC>.  */
    case MCC ('h', 's'):
    case MCC ('c', 'c'): return MCC ('l', 's');
    /* <LO> is an alias for <CS>.  */
    case MCC ('l', 'o'):
    case MCC ('c', 's'): return MCC ('h', 'i');
    case MCC ('p', 'l'): return MCC ('m', 'i');
    case MCC ('m', 'i'): return MCC ('p', 'l');
    case MCC ('g', 'e'): return MCC ('l', 'e');
    case MCC ('l', 't'): return MCC ('g', 't');
    case MCC ('g', 't'): return MCC ('l', 't');
    case MCC ('l', 'e'): return MCC ('g', 'e');
    /* Issue a warning for conditions we can not swap.  */
    case MCC ('n', 'e'): return MCC ('n', 'e'); // no problem here
    case MCC ('e', 'q'): return MCC ('e', 'q'); // also no problem
    case MCC ('v', 'c'):
    case MCC ('v', 's'):
    default :
	   as_warn (_("Condition <%c%c> in structured control directive can not be encoded correctly"),
		         (char) (cc >> 8), (char) (cc));
      break;
    }
  return cc;
}

/* Reverse the sense of a condition.  */

static int
reverse_mri_condition (int cc)
{
  switch (cc)
    {
    case MCC ('h', 'i'): return MCC ('l', 's');
    case MCC ('l', 's'): return MCC ('h', 'i');
    /* <HS> is an alias for <CC> */
    case MCC ('h', 's'): return MCC ('l', 'o');
    case MCC ('c', 'c'): return MCC ('c', 's');
    /* <LO> is an alias for <CS> */
    case MCC ('l', 'o'): return MCC ('h', 's');
    case MCC ('c', 's'): return MCC ('c', 'c');
    case MCC ('n', 'e'): return MCC ('e', 'q');
    case MCC ('e', 'q'): return MCC ('n', 'e');
    case MCC ('v', 'c'): return MCC ('v', 's');
    case MCC ('v', 's'): return MCC ('v', 'c');
    case MCC ('p', 'l'): return MCC ('m', 'i');
    case MCC ('m', 'i'): return MCC ('p', 'l');
    case MCC ('g', 'e'): return MCC ('l', 't');
    case MCC ('l', 't'): return MCC ('g', 'e');
    case MCC ('g', 't'): return MCC ('l', 'e');
    case MCC ('l', 'e'): return MCC ('g', 't');
    }
  return cc;
}

/* Build an MRI structured control expression.  This generates test
   and branch instructions.  It goes to TRUELAB if the condition is
   true, and to FALSELAB if the condition is false.  Exactly one of
   TRUELAB and FALSELAB will be NULL, meaning to fall through.  QUAL
   is the size qualifier for the expression.  EXTENT is the size to
   use for the branch.  */

static void
build_mri_control_operand (int qual, int cc, char *leftstart, char *leftstop,
			   char *rightstart, char *rightstop,
			   const char *truelab, const char *falselab,
			   int extent)
{
  char *buf;
  char *s;

  if (leftstart != NULL)
    {
      struct m68k_op leftop, rightop;
      char c;

      /* Swap the compare operands, if necessary, to produce a legal
	 m68k compare instruction.  Comparing a register operand with
	 a non-register operand requires the register to be on the
	 right (cmp, cmpa).  Comparing an immediate value with
	 anything requires the immediate value to be on the left
	 (cmpi).  */

      c = *leftstop;
      *leftstop = '\0';
      (void) m68k_ip_op (leftstart, &leftop);
      *leftstop = c;

      c = *rightstop;
      *rightstop = '\0';
      (void) m68k_ip_op (rightstart, &rightop);
      *rightstop = c;

      if (rightop.mode == IMMED
	  || ((leftop.mode == DREG || leftop.mode == AREG)
	      && (rightop.mode != DREG && rightop.mode != AREG)))
	{
	  char *temp;

	  /* Correct conditional handling:
	     if #1 <lt> d0 then  ;means if (1 < d0)
		...
	     endi

	     should assemble to:

		cmp #1,d0        if we do *not* swap the operands
		bgt true         we need the swapped condition!
		ble false
	     true:
		...
	     false:
	  */
	  temp = leftstart;
	  leftstart = rightstart;
	  rightstart = temp;
	  temp = leftstop;
	  leftstop = rightstop;
	  rightstop = temp;
	}
      else
	{
	  cc = swap_mri_condition (cc);
	}
    }

  if (truelab == NULL)
    {
      cc = reverse_mri_condition (cc);
      truelab = falselab;
    }

  if (leftstart != NULL)
    {
      buf = (char *) xmalloc (20
			      + (leftstop - leftstart)
			      + (rightstop - rightstart));
      s = buf;
      *s++ = 'c';
      *s++ = 'm';
      *s++ = 'p';
      if (qual != '\0')
	*s++ = TOLOWER (qual);
      *s++ = ' ';
      memcpy (s, leftstart, leftstop - leftstart);
      s += leftstop - leftstart;
      *s++ = ',';
      memcpy (s, rightstart, rightstop - rightstart);
      s += rightstop - rightstart;
      *s = '\0';
      mri_assemble (buf);
      free (buf);
    }

  buf = (char *) xmalloc (20 + strlen (truelab));
  s = buf;
  *s++ = 'b';
  *s++ = cc >> 8;
  *s++ = cc & 0xff;
  if (extent != '\0')
    *s++ = TOLOWER (extent);
  *s++ = ' ';
  strcpy (s, truelab);
  mri_assemble (buf);
  free (buf);
}

/* Parse an MRI structured control expression.  This generates test
   and branch instructions.  STOP is where the expression ends.  It
   goes to TRUELAB if the condition is true, and to FALSELAB if the
   condition is false.  Exactly one of TRUELAB and FALSELAB will be
   NULL, meaning to fall through.  QUAL is the size qualifier for the
   expression.  EXTENT is the size to use for the branch.  */

static void
parse_mri_control_expression (char *stop, int qual, const char *truelab,
			      const char *falselab, int extent)
{
  int c;
  int cc;
  char *leftstart;
  char *leftstop;
  char *rightstart;
  char *rightstop;

  c = *stop;
  *stop = '\0';

  if (! parse_mri_control_operand (&cc, &leftstart, &leftstop,
				   &rightstart, &rightstop))
    {
      *stop = c;
      return;
    }

  if (strncasecmp (input_line_pointer, "AND", 3) == 0)
    {
      const char *flab;

      if (falselab != NULL)
	flab = falselab;
      else
	flab = mri_control_label ();

      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, (const char *) NULL, flab, extent);

      input_line_pointer += 3;
      if (*input_line_pointer != '.'
	  || input_line_pointer[1] == '\0')
	qual = '\0';
      else
	{
	  qual = input_line_pointer[1];
	  input_line_pointer += 2;
	}

      if (! parse_mri_control_operand (&cc, &leftstart, &leftstop,
				       &rightstart, &rightstop))
	{
	  *stop = c;
	  return;
	}

      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, truelab, falselab, extent);

      if (falselab == NULL)
	colon (flab);
    }
  else if (strncasecmp (input_line_pointer, "OR", 2) == 0)
    {
      const char *tlab;

      if (truelab != NULL)
	tlab = truelab;
      else
	tlab = mri_control_label ();

      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, tlab, (const char *) NULL, extent);

      input_line_pointer += 2;
      if (*input_line_pointer != '.'
	  || input_line_pointer[1] == '\0')
	qual = '\0';
      else
	{
	  qual = input_line_pointer[1];
	  input_line_pointer += 2;
	}

      if (! parse_mri_control_operand (&cc, &leftstart, &leftstop,
				       &rightstart, &rightstop))
	{
	  *stop = c;
	  return;
	}

      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, truelab, falselab, extent);

      if (truelab == NULL)
	colon (tlab);
    }
  else
    {
      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, truelab, falselab, extent);
    }

  *stop = c;
  if (input_line_pointer != stop)
    as_bad (_("syntax error in structured control directive"));
}

/* Handle the MRI IF pseudo-op.  This may be a structured control
   directive, or it may be a regular assembler conditional, depending
   on its operands.  */

static void
s_mri_if (int qual)
{
  char *s;
  int c;
  struct mri_control_info *n;

  /* A structured control directive must end with THEN with an
     optional qualifier.  */
  s = input_line_pointer;
  /* We only accept '*' as introduction of comments if preceded by white space
     or at first column of a line (I think this can't actually happen here?)
     This is important when assembling:
       if d0 <ne> 12(a0,d0*2) then
       if d0 <ne> #CONST*20   then.  */
  while (! (is_end_of_line[(unsigned char) *s]
            || (flag_mri
                && *s == '*'
                && (s == input_line_pointer
                    || *(s-1) == ' '
                    || *(s-1) == '\t'))))
    ++s;
  --s;
  while (s > input_line_pointer && (*s == ' ' || *s == '\t'))
    --s;

  if (s - input_line_pointer > 1
      && s[-1] == '.')
    s -= 2;

  if (s - input_line_pointer < 3
      || strncasecmp (s - 3, "THEN", 4) != 0)
    {
      if (qual != '\0')
	{
	  as_bad (_("missing then"));
	  ignore_rest_of_line ();
	  return;
	}

      /* It's a conditional.  */
      s_if (O_ne);
      return;
    }

  /* Since this might be a conditional if, this pseudo-op will be
     called even if we are supported to be ignoring input.  Double
     check now.  Clobber *input_line_pointer so that ignore_input
     thinks that this is not a special pseudo-op.  */
  c = *input_line_pointer;
  *input_line_pointer = 0;
  if (ignore_input ())
    {
      *input_line_pointer = c;
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
      demand_empty_rest_of_line ();
      return;
    }
  *input_line_pointer = c;

  n = push_mri_control (mri_if);

  parse_mri_control_expression (s - 3, qual, (const char *) NULL,
				n->next, s[1] == '.' ? s[2] : '\0');

  if (s[1] == '.')
    input_line_pointer = s + 3;
  else
    input_line_pointer = s + 1;

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI else pseudo-op.  If we are currently doing an MRI
   structured IF, associate the ELSE with the IF.  Otherwise, assume
   it is a conditional else.  */

static void
s_mri_else (int qual)
{
  int c;
  char *buf;
  char q[2];

  if (qual == '\0'
      && (mri_control_stack == NULL
	  || mri_control_stack->type != mri_if
	  || mri_control_stack->else_seen))
    {
      s_else (0);
      return;
    }

  c = *input_line_pointer;
  *input_line_pointer = 0;
  if (ignore_input ())
    {
      *input_line_pointer = c;
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
      demand_empty_rest_of_line ();
      return;
    }
  *input_line_pointer = c;

  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_if
      || mri_control_stack->else_seen)
    {
      as_bad (_("else without matching if"));
      ignore_rest_of_line ();
      return;
    }

  mri_control_stack->else_seen = 1;

  buf = (char *) xmalloc (20 + strlen (mri_control_stack->bottom));
  q[0] = TOLOWER (qual);
  q[1] = '\0';
  sprintf (buf, "bra%s %s", q, mri_control_stack->bottom);
  mri_assemble (buf);
  free (buf);

  colon (mri_control_stack->next);

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI ENDI pseudo-op.  */

static void
s_mri_endi (int ignore ATTRIBUTE_UNUSED)
{
  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_if)
    {
      as_bad (_("endi without matching if"));
      ignore_rest_of_line ();
      return;
    }

  /* ignore_input will not return true for ENDI, so we don't need to
     worry about checking it again here.  */

  if (! mri_control_stack->else_seen)
    colon (mri_control_stack->next);
  colon (mri_control_stack->bottom);

  pop_mri_control ();

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI BREAK pseudo-op.  */

static void
s_mri_break (int extent)
{
  struct mri_control_info *n;
  char *buf;
  char ex[2];

  n = mri_control_stack;
  while (n != NULL
	 && n->type != mri_for
	 && n->type != mri_repeat
	 && n->type != mri_while)
    n = n->outer;
  if (n == NULL)
    {
      as_bad (_("break outside of structured loop"));
      ignore_rest_of_line ();
      return;
    }

  buf = (char *) xmalloc (20 + strlen (n->bottom));
  ex[0] = TOLOWER (extent);
  ex[1] = '\0';
  sprintf (buf, "bra%s %s", ex, n->bottom);
  mri_assemble (buf);
  free (buf);

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI NEXT pseudo-op.  */

static void
s_mri_next (int extent)
{
  struct mri_control_info *n;
  char *buf;
  char ex[2];

  n = mri_control_stack;
  while (n != NULL
	 && n->type != mri_for
	 && n->type != mri_repeat
	 && n->type != mri_while)
    n = n->outer;
  if (n == NULL)
    {
      as_bad (_("next outside of structured loop"));
      ignore_rest_of_line ();
      return;
    }

  buf = (char *) xmalloc (20 + strlen (n->next));
  ex[0] = TOLOWER (extent);
  ex[1] = '\0';
  sprintf (buf, "bra%s %s", ex, n->next);
  mri_assemble (buf);
  free (buf);

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI FOR pseudo-op.  */

static void
s_mri_for (int qual)
{
  const char *varstart, *varstop;
  const char *initstart, *initstop;
  const char *endstart, *endstop;
  const char *bystart, *bystop;
  int up;
  int by;
  int extent;
  struct mri_control_info *n;
  char *buf;
  char *s;
  char ex[2];

  /* The syntax is
       FOR.q var = init { TO | DOWNTO } end [ BY by ] DO.e
     */

  SKIP_WHITESPACE ();
  varstart = input_line_pointer;

  /* Look for the '='.  */
  while (! is_end_of_line[(unsigned char) *input_line_pointer]
	 && *input_line_pointer != '=')
    ++input_line_pointer;
  if (*input_line_pointer != '=')
    {
      as_bad (_("missing ="));
      ignore_rest_of_line ();
      return;
    }

  varstop = input_line_pointer;
  if (varstop > varstart
      && (varstop[-1] == ' ' || varstop[-1] == '\t'))
    --varstop;

  ++input_line_pointer;

  initstart = input_line_pointer;

  /* Look for TO or DOWNTO.  */
  up = 1;
  initstop = NULL;
  while (! is_end_of_line[(unsigned char) *input_line_pointer])
    {
      if (strncasecmp (input_line_pointer, "TO", 2) == 0
	  && ! is_part_of_name (input_line_pointer[2]))
	{
	  initstop = input_line_pointer;
	  input_line_pointer += 2;
	  break;
	}
      if (strncasecmp (input_line_pointer, "DOWNTO", 6) == 0
	  && ! is_part_of_name (input_line_pointer[6]))
	{
	  initstop = input_line_pointer;
	  up = 0;
	  input_line_pointer += 6;
	  break;
	}
      ++input_line_pointer;
    }
  if (initstop == NULL)
    {
      as_bad (_("missing to or downto"));
      ignore_rest_of_line ();
      return;
    }
  if (initstop > initstart
      && (initstop[-1] == ' ' || initstop[-1] == '\t'))
    --initstop;

  SKIP_WHITESPACE ();
  endstart = input_line_pointer;

  /* Look for BY or DO.  */
  by = 0;
  endstop = NULL;
  while (! is_end_of_line[(unsigned char) *input_line_pointer])
    {
      if (strncasecmp (input_line_pointer, "BY", 2) == 0
	  && ! is_part_of_name (input_line_pointer[2]))
	{
	  endstop = input_line_pointer;
	  by = 1;
	  input_line_pointer += 2;
	  break;
	}
      if (strncasecmp (input_line_pointer, "DO", 2) == 0
	  && (input_line_pointer[2] == '.'
	      || ! is_part_of_name (input_line_pointer[2])))
	{
	  endstop = input_line_pointer;
	  input_line_pointer += 2;
	  break;
	}
      ++input_line_pointer;
    }
  if (endstop == NULL)
    {
      as_bad (_("missing do"));
      ignore_rest_of_line ();
      return;
    }
  if (endstop > endstart
      && (endstop[-1] == ' ' || endstop[-1] == '\t'))
    --endstop;

  if (! by)
    {
      bystart = "#1";
      bystop = bystart + 2;
    }
  else
    {
      SKIP_WHITESPACE ();
      bystart = input_line_pointer;

      /* Look for DO.  */
      bystop = NULL;
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	{
	  if (strncasecmp (input_line_pointer, "DO", 2) == 0
	      && (input_line_pointer[2] == '.'
		  || ! is_part_of_name (input_line_pointer[2])))
	    {
	      bystop = input_line_pointer;
	      input_line_pointer += 2;
	      break;
	    }
	  ++input_line_pointer;
	}
      if (bystop == NULL)
	{
	  as_bad (_("missing do"));
	  ignore_rest_of_line ();
	  return;
	}
      if (bystop > bystart
	  && (bystop[-1] == ' ' || bystop[-1] == '\t'))
	--bystop;
    }

  if (*input_line_pointer != '.')
    extent = '\0';
  else
    {
      extent = input_line_pointer[1];
      input_line_pointer += 2;
    }

  /* We have fully parsed the FOR operands.  Now build the loop.  */
  n = push_mri_control (mri_for);

  buf = (char *) xmalloc (50 + (input_line_pointer - varstart));

  /* Move init,var.  */
  s = buf;
  *s++ = 'm';
  *s++ = 'o';
  *s++ = 'v';
  *s++ = 'e';
  if (qual != '\0')
    *s++ = TOLOWER (qual);
  *s++ = ' ';
  memcpy (s, initstart, initstop - initstart);
  s += initstop - initstart;
  *s++ = ',';
  memcpy (s, varstart, varstop - varstart);
  s += varstop - varstart;
  *s = '\0';
  mri_assemble (buf);

  colon (n->top);

  /* cmp end,var.  */
  s = buf;
  *s++ = 'c';
  *s++ = 'm';
  *s++ = 'p';
  if (qual != '\0')
    *s++ = TOLOWER (qual);
  *s++ = ' ';
  memcpy (s, endstart, endstop - endstart);
  s += endstop - endstart;
  *s++ = ',';
  memcpy (s, varstart, varstop - varstart);
  s += varstop - varstart;
  *s = '\0';
  mri_assemble (buf);

  /* bcc bottom.  */
  ex[0] = TOLOWER (extent);
  ex[1] = '\0';
  if (up)
    sprintf (buf, "blt%s %s", ex, n->bottom);
  else
    sprintf (buf, "bgt%s %s", ex, n->bottom);
  mri_assemble (buf);

  /* Put together the add or sub instruction used by ENDF.  */
  s = buf;
  if (up)
    strcpy (s, "add");
  else
    strcpy (s, "sub");
  s += 3;
  if (qual != '\0')
    *s++ = TOLOWER (qual);
  *s++ = ' ';
  memcpy (s, bystart, bystop - bystart);
  s += bystop - bystart;
  *s++ = ',';
  memcpy (s, varstart, varstop - varstart);
  s += varstop - varstart;
  *s = '\0';
  n->incr = buf;

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI ENDF pseudo-op.  */

static void
s_mri_endf (int ignore ATTRIBUTE_UNUSED)
{
  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_for)
    {
      as_bad (_("endf without for"));
      ignore_rest_of_line ();
      return;
    }

  colon (mri_control_stack->next);

  mri_assemble (mri_control_stack->incr);

  sprintf (mri_control_stack->incr, "bra %s", mri_control_stack->top);
  mri_assemble (mri_control_stack->incr);

  free (mri_control_stack->incr);

  colon (mri_control_stack->bottom);

  pop_mri_control ();

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI REPEAT pseudo-op.  */

static void
s_mri_repeat (int ignore ATTRIBUTE_UNUSED)
{
  struct mri_control_info *n;

  n = push_mri_control (mri_repeat);
  colon (n->top);
  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }
  demand_empty_rest_of_line ();
}

/* Handle the MRI UNTIL pseudo-op.  */

static void
s_mri_until (int qual)
{
  char *s;

  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_repeat)
    {
      as_bad (_("until without repeat"));
      ignore_rest_of_line ();
      return;
    }

  colon (mri_control_stack->next);

  for (s = input_line_pointer; ! is_end_of_line[(unsigned char) *s]; s++)
    ;

  parse_mri_control_expression (s, qual, (const char *) NULL,
				mri_control_stack->top, '\0');

  colon (mri_control_stack->bottom);

  input_line_pointer = s;

  pop_mri_control ();

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI WHILE pseudo-op.  */

static void
s_mri_while (int qual)
{
  char *s;

  struct mri_control_info *n;

  s = input_line_pointer;
  /* We only accept '*' as introduction of comments if preceded by white space
     or at first column of a line (I think this can't actually happen here?)
     This is important when assembling:
       while d0 <ne> 12(a0,d0*2) do
       while d0 <ne> #CONST*20   do.  */
  while (! (is_end_of_line[(unsigned char) *s]
	    || (flag_mri
		&& *s == '*'
		&& (s == input_line_pointer
		    || *(s-1) == ' '
		    || *(s-1) == '\t'))))
    s++;
  --s;
  while (*s == ' ' || *s == '\t')
    --s;
  if (s - input_line_pointer > 1
      && s[-1] == '.')
    s -= 2;
  if (s - input_line_pointer < 2
      || strncasecmp (s - 1, "DO", 2) != 0)
    {
      as_bad (_("missing do"));
      ignore_rest_of_line ();
      return;
    }

  n = push_mri_control (mri_while);

  colon (n->next);

  parse_mri_control_expression (s - 1, qual, (const char *) NULL, n->bottom,
				s[1] == '.' ? s[2] : '\0');

  input_line_pointer = s + 1;
  if (*input_line_pointer == '.')
    input_line_pointer += 2;

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI ENDW pseudo-op.  */

static void
s_mri_endw (int ignore ATTRIBUTE_UNUSED)
{
  char *buf;

  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_while)
    {
      as_bad (_("endw without while"));
      ignore_rest_of_line ();
      return;
    }

  buf = (char *) xmalloc (20 + strlen (mri_control_stack->next));
  sprintf (buf, "bra %s", mri_control_stack->next);
  mri_assemble (buf);
  free (buf);

  colon (mri_control_stack->bottom);

  pop_mri_control ();

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Parse a .cpu directive.  */

static void
s_m68k_cpu (int ignored ATTRIBUTE_UNUSED)
{
  char saved_char;
  char *name;

  if (initialized)
    {
      as_bad (_("already assembled instructions"));
      ignore_rest_of_line ();
      return;
    }
  
  name = input_line_pointer;
  while (*input_line_pointer && !ISSPACE(*input_line_pointer))
    input_line_pointer++;
  saved_char = *input_line_pointer;
  *input_line_pointer = 0;

  m68k_set_cpu (name, 1, 0);
  
  *input_line_pointer = saved_char;
  demand_empty_rest_of_line ();
  return;
}

/* Parse a .arch directive.  */

static void
s_m68k_arch (int ignored ATTRIBUTE_UNUSED)
{
  char saved_char;
  char *name;

  if (initialized)
    {
      as_bad (_("already assembled instructions"));
      ignore_rest_of_line ();
      return;
    }
  
  name = input_line_pointer;
  while (*input_line_pointer && *input_line_pointer != ','
	 && !ISSPACE (*input_line_pointer))
    input_line_pointer++;
  saved_char = *input_line_pointer;
  *input_line_pointer = 0;

  if (m68k_set_arch (name, 1, 0))
    {
      /* Scan extensions. */
      do
	{
	  *input_line_pointer++ = saved_char;
	  if (!*input_line_pointer || ISSPACE (*input_line_pointer))
	    break;
	  name = input_line_pointer;
	  while (*input_line_pointer && *input_line_pointer != ','
		 && !ISSPACE (*input_line_pointer))
	    input_line_pointer++;
	  saved_char = *input_line_pointer;
	  *input_line_pointer = 0;
	}
      while (m68k_set_extension (name, 1, 0));
    }
  
  *input_line_pointer = saved_char;
  demand_empty_rest_of_line ();
  return;
}

/* Lookup a cpu name in TABLE and return the slot found.  Return NULL
   if none is found, the caller is responsible for emitting an error
   message.  If ALLOW_M is non-zero, we allow an initial 'm' on the
   cpu name, if it begins with a '6' (possibly skipping an intervening
   'c'.  We also allow a 'c' in the same place.  if NEGATED is
   non-zero, we accept a leading 'no-' and *NEGATED is set to true, if
   the option is indeed negated.  */

static const struct m68k_cpu *
m68k_lookup_cpu (const char *arg, const struct m68k_cpu *table,
		 int allow_m, int *negated)
{
  /* allow negated value? */
  if (negated)
    {
      *negated = 0;

      if (arg[0] == 'n' && arg[1] == 'o' && arg[2] == '-')
	{
	  arg += 3;
	  *negated = 1;
	}
    }
  
  /* Remove 'm' or 'mc' prefix from 68k variants.  */
  if (allow_m)
    {
      if (arg[0] == 'm')
	{
	  if (arg[1] == '6')
	    arg += 1;
	  else if (arg[1] == 'c'  && arg[2] == '6')
	    arg += 2;
	}
    }
  else if (arg[0] == 'c' && arg[1] == '6')
    arg += 1;

  for (; table->name; table++)
    if (!strcmp (arg, table->name))
      {
	if (table->alias < -1 || table->alias > 1)
	  as_bad (_("`%s' is deprecated, use `%s'"),
		  table->name, table[table->alias < 0 ? 1 : -1].name);
	return table;
      }
  return 0;
}

/* Set the cpu, issuing errors if it is unrecognized, or invalid */

static int
m68k_set_cpu (char const *name, int allow_m, int silent)
{
  const struct m68k_cpu *cpu;

  cpu = m68k_lookup_cpu (name, m68k_cpus, allow_m, NULL);

  if (!cpu)
    {
      if (!silent)
	as_bad (_("cpu `%s' unrecognized"), name);
      return 0;
    }
      
  if (selected_cpu && selected_cpu != cpu)
    {
      as_bad (_("already selected `%s' processor"),
	      selected_cpu->name);
      return 0;
    }
  selected_cpu = cpu;
  return 1;
}

/* Set the architecture, issuing errors if it is unrecognized, or invalid */

static int
m68k_set_arch (char const *name, int allow_m, int silent)
{
  const struct m68k_cpu *arch;

  arch = m68k_lookup_cpu (name, m68k_archs, allow_m, NULL);

  if (!arch)
    {
      if (!silent)
	as_bad (_("architecture `%s' unrecognized"), name);
      return 0;
    }
      
  if (selected_arch && selected_arch != arch)
    {
      as_bad (_("already selected `%s' architecture"),
	      selected_arch->name);
      return 0;
    }
  
  selected_arch = arch;
  return 1;
}

/* Set the architecture extension, issuing errors if it is
   unrecognized, or invalid */

static int
m68k_set_extension (char const *name, int allow_m, int silent)
{
  int negated;
  const struct m68k_cpu *ext;

  ext = m68k_lookup_cpu (name, m68k_extensions, allow_m, &negated);

  if (!ext)
    {
      if (!silent)
	as_bad (_("extension `%s' unrecognized"), name);
      return 0;
    }

  if (negated)
    not_current_architecture |= ext->arch;
  else
    current_architecture |= ext->arch;
  return 1;
}

/* md_parse_option
   Invocation line includes a switch not recognized by the base assembler.
 */

#ifdef OBJ_ELF
const char *md_shortopts = "lSA:m:kQ:V";
#else
const char *md_shortopts = "lSA:m:k";
#endif

struct option md_longopts[] = {
#define OPTION_PIC (OPTION_MD_BASE)
  {"pic", no_argument, NULL, OPTION_PIC},
#define OPTION_REGISTER_PREFIX_OPTIONAL (OPTION_MD_BASE + 1)
  {"register-prefix-optional", no_argument, NULL,
     OPTION_REGISTER_PREFIX_OPTIONAL},
#define OPTION_BITWISE_OR (OPTION_MD_BASE + 2)
  {"bitwise-or", no_argument, NULL, OPTION_BITWISE_OR},
#define OPTION_BASE_SIZE_DEFAULT_16 (OPTION_MD_BASE + 3)
  {"base-size-default-16", no_argument, NULL, OPTION_BASE_SIZE_DEFAULT_16},
#define OPTION_BASE_SIZE_DEFAULT_32 (OPTION_MD_BASE + 4)
  {"base-size-default-32", no_argument, NULL, OPTION_BASE_SIZE_DEFAULT_32},
#define OPTION_DISP_SIZE_DEFAULT_16 (OPTION_MD_BASE + 5)
  {"disp-size-default-16", no_argument, NULL, OPTION_DISP_SIZE_DEFAULT_16},
#define OPTION_DISP_SIZE_DEFAULT_32 (OPTION_MD_BASE + 6)
  {"disp-size-default-32", no_argument, NULL, OPTION_DISP_SIZE_DEFAULT_32},
#define OPTION_PCREL (OPTION_MD_BASE + 7)
  {"pcrel", no_argument, NULL, OPTION_PCREL},
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c, char *arg)
{
  switch (c)
    {
    case 'l':			/* -l means keep external to 2 bit offset
				   rather than 16 bit one.  */
      flag_short_refs = 1;
      break;

    case 'S':			/* -S means that jbsr's always turn into
				   jsr's.  */
      flag_long_jumps = 1;
      break;

    case OPTION_PCREL:		/* --pcrel means never turn PC-relative
				   branches into absolute jumps.  */
      flag_keep_pcrel = 1;
      break;

    case OPTION_PIC:
    case 'k':
      flag_want_pic = 1;
      break;			/* -pic, Position Independent Code.  */

    case OPTION_REGISTER_PREFIX_OPTIONAL:
      flag_reg_prefix_optional = 1;
      reg_prefix_optional_seen = 1;
      break;

      /* -V: SVR4 argument to print version ID.  */
    case 'V':
      print_version_id ();
      break;

      /* -Qy, -Qn: SVR4 arguments controlling whether a .comment section
	 should be emitted or not.  FIXME: Not implemented.  */
    case 'Q':
      break;

    case OPTION_BITWISE_OR:
      {
	char *n, *t;
	const char *s;

	n = (char *) xmalloc (strlen (m68k_comment_chars) + 1);
	t = n;
	for (s = m68k_comment_chars; *s != '\0'; s++)
	  if (*s != '|')
	    *t++ = *s;
	*t = '\0';
	m68k_comment_chars = n;
      }
      break;

    case OPTION_BASE_SIZE_DEFAULT_16:
      m68k_index_width_default = SIZE_WORD;
      break;

    case OPTION_BASE_SIZE_DEFAULT_32:
      m68k_index_width_default = SIZE_LONG;
      break;

    case OPTION_DISP_SIZE_DEFAULT_16:
      m68k_rel32 = 0;
      m68k_rel32_from_cmdline = 1;
      break;

    case OPTION_DISP_SIZE_DEFAULT_32:
      m68k_rel32 = 1;
      m68k_rel32_from_cmdline = 1;
      break;

    case 'A':
#if WARN_DEPRECATED
      as_tsktsk (_ ("option `-A%s' is deprecated: use `-%s'",
		    arg, arg));
#endif
      /* Intentional fall-through.  */
    case 'm':
      if (!strncmp (arg, "arch=", 5))
	m68k_set_arch (arg + 5, 1, 0);
      else if (!strncmp (arg, "cpu=", 4))
	m68k_set_cpu (arg + 4, 1, 0);
      else if (m68k_set_extension (arg, 0, 1))
	;
      else if (m68k_set_arch (arg, 0, 1))
	;
      else if (m68k_set_cpu (arg, 0, 1))
	;
      else
	return 0;
      break;

    default:
      return 0;
    }

  return 1;
}

/* Setup tables from the selected arch and/or cpu */

static void
m68k_init_arch (void)
{
  if (not_current_architecture & current_architecture)
    {
      as_bad (_("architecture features both enabled and disabled"));
      not_current_architecture &= ~current_architecture;
    }
  if (selected_arch)
    {
      current_architecture |= selected_arch->arch;
      control_regs = selected_arch->control_regs;
    }
  else
    current_architecture |= selected_cpu->arch;
  
  current_architecture &= ~not_current_architecture;

  if ((current_architecture & (cfloat | m68881)) == (cfloat | m68881))
    {
      /* Determine which float is really meant.  */
      if (current_architecture & (m68k_mask & ~m68881))
	current_architecture ^= cfloat;
      else
	current_architecture ^= m68881;
    }

  if (selected_cpu)
    {
      control_regs = selected_cpu->control_regs;
      if (current_architecture & ~selected_cpu->arch)
	{
	  as_bad (_("selected processor does not have all features of selected architecture"));
	  current_architecture
	    = selected_cpu->arch & ~not_current_architecture;
	}
    }

  if ((current_architecture & m68k_mask)
      && (current_architecture & ~m68k_mask))
    {
      as_bad (_ ("m68k and cf features both selected"));
      if (current_architecture & m68k_mask)
	current_architecture &= m68k_mask;
      else
	current_architecture &= ~m68k_mask;
    }
  
  /* Permit m68881 specification with all cpus; those that can't work
     with a coprocessor could be doing emulation.  */
  if (current_architecture & m68851)
    {
      if (current_architecture & m68040)
	as_warn (_("68040 and 68851 specified; mmu instructions may assemble incorrectly"));
    }
  /* What other incompatibilities could we check for?  */

  if (cpu_of_arch (current_architecture) < m68020
      || arch_coldfire_p (current_architecture))
    md_relax_table[TAB (PCINDEX, BYTE)].rlx_more = 0;
  
  initialized = 1;
}

void
md_show_usage (FILE *stream)
{
  const char *default_cpu = TARGET_CPU;
  int i;
  unsigned int default_arch;

  /* Get the canonical name for the default target CPU.  */
  if (*default_cpu == 'm')
    default_cpu++;
  for (i = 0; m68k_cpus[i].name; i++)
    {
      if (strcasecmp (default_cpu, m68k_cpus[i].name) == 0)
	{
	  default_arch = m68k_cpus[i].arch;
	  while (m68k_cpus[i].alias > 0)
	    i--;
	  while (m68k_cpus[i].alias < 0)
	    i++;
	  default_cpu = m68k_cpus[i].name;
	}
    }

  fprintf (stream, _("\
-march=<arch>		set architecture\n\
-mcpu=<cpu>		set cpu [default %s]\n\
"), default_cpu);
  for (i = 0; m68k_extensions[i].name; i++)
    fprintf (stream, _("\
-m[no-]%-16s enable/disable%s architecture extension\n\
"), m68k_extensions[i].name,
	     m68k_extensions[i].alias > 0 ? " ColdFire"
	     : m68k_extensions[i].alias < 0 ? " m68k" : "");
  
  fprintf (stream, _("\
-l			use 1 word for refs to undefined symbols [default 2]\n\
-pic, -k		generate position independent code\n\
-S			turn jbsr into jsr\n\
--pcrel                 never turn PC-relative branches into absolute jumps\n\
--register-prefix-optional\n\
			recognize register names without prefix character\n\
--bitwise-or		do not treat `|' as a comment character\n\
--base-size-default-16	base reg without size is 16 bits\n\
--base-size-default-32	base reg without size is 32 bits (default)\n\
--disp-size-default-16	displacement with unknown size is 16 bits\n\
--disp-size-default-32	displacement with unknown size is 32 bits (default)\n\
"));
  
  fprintf (stream, _("Architecture variants are: "));
  for (i = 0; m68k_archs[i].name; i++)
    {
      if (i)
	fprintf (stream, " | ");
      fprintf (stream, m68k_archs[i].name);
    }
  fprintf (stream, "\n");

  fprintf (stream, _("Processor variants are: "));
  for (i = 0; m68k_cpus[i].name; i++)
    {
      if (i)
	fprintf (stream, " | ");
      fprintf (stream, m68k_cpus[i].name);
    }
  fprintf (stream, _("\n"));
}

#ifdef TEST2

/* TEST2:  Test md_assemble() */
/* Warning, this routine probably doesn't work anymore.  */
int
main (void)
{
  struct m68k_it the_ins;
  char buf[120];
  char *cp;
  int n;

  m68k_ip_begin ();
  for (;;)
    {
      if (!gets (buf) || !*buf)
	break;
      if (buf[0] == '|' || buf[1] == '.')
	continue;
      for (cp = buf; *cp; cp++)
	if (*cp == '\t')
	  *cp = ' ';
      if (is_label (buf))
	continue;
      memset (&the_ins, '\0', sizeof (the_ins));
      m68k_ip (&the_ins, buf);
      if (the_ins.error)
	{
	  printf (_("Error %s in %s\n"), the_ins.error, buf);
	}
      else
	{
	  printf (_("Opcode(%d.%s): "), the_ins.numo, the_ins.args);
	  for (n = 0; n < the_ins.numo; n++)
	    printf (" 0x%x", the_ins.opcode[n] & 0xffff);
	  printf ("    ");
	  print_the_insn (&the_ins.opcode[0], stdout);
	  (void) putchar ('\n');
	}
      for (n = 0; n < strlen (the_ins.args) / 2; n++)
	{
	  if (the_ins.operands[n].error)
	    {
	      printf ("op%d Error %s in %s\n", n, the_ins.operands[n].error, buf);
	      continue;
	    }
	  printf ("mode %d, reg %d, ", the_ins.operands[n].mode,
		  the_ins.operands[n].reg);
	  if (the_ins.operands[n].b_const)
	    printf ("Constant: '%.*s', ",
		    1 + the_ins.operands[n].e_const - the_ins.operands[n].b_const,
		    the_ins.operands[n].b_const);
	  printf ("ireg %d, isiz %d, imul %d, ", the_ins.operands[n].ireg,
		  the_ins.operands[n].isiz, the_ins.operands[n].imul);
	  if (the_ins.operands[n].b_iadd)
	    printf ("Iadd: '%.*s',",
		    1 + the_ins.operands[n].e_iadd - the_ins.operands[n].b_iadd,
		    the_ins.operands[n].b_iadd);
	  putchar ('\n');
	}
    }
  m68k_ip_end ();
  return 0;
}

int
is_label (char *str)
{
  while (*str == ' ')
    str++;
  while (*str && *str != ' ')
    str++;
  if (str[-1] == ':' || str[1] == '=')
    return 1;
  return 0;
}

#endif

/* Possible states for relaxation:

   0 0	branch offset	byte	(bra, etc)
   0 1			word
   0 2			long

   1 0	indexed offsets	byte	a0@(32,d4:w:1) etc
   1 1			word
   1 2			long

   2 0	two-offset index word-word a0@(32,d4)@(45) etc
   2 1			word-long
   2 2			long-word
   2 3			long-long

   */

/* We have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Round up a section size to the appropriate boundary.  */
valueT
md_section_align (segT segment ATTRIBUTE_UNUSED, valueT size)
{
#ifdef OBJ_AOUT
  /* For a.out, force the section size to be aligned.  If we don't do
     this, BFD will align it for us, but it will not write out the
     final bytes of the section.  This may be a bug in BFD, but it is
     easier to fix it here since that is how the other a.out targets
     work.  */
  int align;

  align = bfd_get_section_alignment (stdoutput, segment);
  size = ((size + (1 << align) - 1) & ((valueT) -1 << align));
#endif

  return size;
}

/* Exactly what point is a PC-relative offset relative TO?
   On the 68k, it is relative to the address of the first extension
   word.  The difference between the addresses of the offset and the
   first extension word is stored in fx_pcrel_adjust.  */
long
md_pcrel_from (fixS *fixP)
{
  int adjust;

  /* Because fx_pcrel_adjust is a char, and may be unsigned, we explicitly
     sign extend the value here.  */
  adjust = ((fixP->fx_pcrel_adjust & 0xff) ^ 0x80) - 0x80;
  if (adjust == 64)
    adjust = -1;
  return fixP->fx_where + fixP->fx_frag->fr_address - adjust;
}

#ifdef OBJ_ELF
void
m68k_elf_final_processing (void)
{
  unsigned flags = 0;
  
  if (arch_coldfire_fpu (current_architecture))
    flags |= EF_M68K_CFV4E;
  /* Set file-specific flags if this is a cpu32 processor.  */
  if (cpu_of_arch (current_architecture) & cpu32)
    flags |= EF_M68K_CPU32;
  else if ((cpu_of_arch (current_architecture) & m68000up)
	   && !(cpu_of_arch (current_architecture) & m68020up))
    flags |= EF_M68K_M68000;
  
  if (current_architecture & mcfisa_a)
    {
      static const unsigned isa_features[][2] =
      {
	{EF_M68K_ISA_A_NODIV, mcfisa_a},
	{EF_M68K_ISA_A,	mcfisa_a|mcfhwdiv},
	{EF_M68K_ISA_A_PLUS,mcfisa_a|mcfisa_aa|mcfhwdiv|mcfusp},
	{EF_M68K_ISA_B_NOUSP,mcfisa_a|mcfisa_b|mcfhwdiv},
	{EF_M68K_ISA_B,	mcfisa_a|mcfisa_b|mcfhwdiv|mcfusp},
	{0,0},
      };
      static const unsigned mac_features[][2] =
      {
	{EF_M68K_MAC, mcfmac},
	{EF_M68K_EMAC, mcfemac},
	{0,0},
      };
      unsigned ix;
      unsigned pattern;
      
      pattern = (current_architecture
		 & (mcfisa_a|mcfisa_aa|mcfisa_b|mcfhwdiv|mcfusp));
      for (ix = 0; isa_features[ix][1]; ix++)
	{
	  if (pattern == isa_features[ix][1])
	    {
	      flags |= isa_features[ix][0];
	      break;
	    }
	}
      if (!isa_features[ix][1])
	{
	cf_bad:
	  as_warn (_("Not a defined coldfire architecture"));
	}
      else
	{
	  if (current_architecture & cfloat)
	    flags |= EF_M68K_FLOAT | EF_M68K_CFV4E;

	  pattern = current_architecture & (mcfmac|mcfemac);
	  if (pattern)
	    {
	      for (ix = 0; mac_features[ix][1]; ix++)
		{
		  if (pattern == mac_features[ix][1])
		    {
		      flags |= mac_features[ix][0];
		      break;
		    }
		}
	      if (!mac_features[ix][1])
		goto cf_bad;
	    }
	}
    }
  elf_elfheader (stdoutput)->e_flags |= flags;
}
#endif

int
tc_m68k_regname_to_dw2regnum (const char *regname)
{
  unsigned int regnum;
  static const char *const regnames[] =
    {
      "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
      "a0", "a1", "a2", "a3", "a4", "a5", "a6", "sp",
      "fp0", "fp1", "fp2", "fp3", "fp4", "fp5", "fp6", "fp7",
      "pc"
    };

  for (regnum = 0; regnum < ARRAY_SIZE (regnames); regnum++)
    if (strcmp (regname, regnames[regnum]) == 0)
      return regnum;

  return -1;
}

void
tc_m68k_frame_initial_instructions (void)
{
  static int sp_regno = -1;

  if (sp_regno < 0)
    sp_regno = tc_m68k_regname_to_dw2regnum ("sp");

  cfi_add_CFA_def_cfa (sp_regno, -DWARF2_CIE_DATA_ALIGNMENT);
  cfi_add_CFA_offset (DWARF2_DEFAULT_RETURN_COLUMN, DWARF2_CIE_DATA_ALIGNMENT);
}
