/* tc-i386.h -- Header file for tc-i386.c
   Copyright 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef TC_I386
#define TC_I386 1

#ifndef BFD_ASSEMBLER
#error So, do you know what you are doing?
#endif

#ifdef ANSI_PROTOTYPES
struct fix;
#endif

#define TARGET_BYTES_BIG_ENDIAN	0

#ifdef TE_LYNX
#define TARGET_FORMAT		"coff-i386-lynx"
#endif

#define TARGET_ARCH		bfd_arch_i386
#define TARGET_MACH		(i386_mach ())
extern unsigned long i386_mach PARAMS ((void));

#ifdef TE_FreeBSD
#define AOUT_TARGET_FORMAT	"a.out-i386-freebsd"
#endif
#ifdef TE_NetBSD
#define AOUT_TARGET_FORMAT	"a.out-i386-netbsd"
#endif
#ifdef TE_386BSD
#define AOUT_TARGET_FORMAT	"a.out-i386-bsd"
#endif
#ifdef TE_LINUX
#define AOUT_TARGET_FORMAT	"a.out-i386-linux"
#endif
#ifdef TE_Mach
#define AOUT_TARGET_FORMAT	"a.out-mach3"
#endif
#ifdef TE_DYNIX
#define AOUT_TARGET_FORMAT	"a.out-i386-dynix"
#endif
#ifndef AOUT_TARGET_FORMAT
#define AOUT_TARGET_FORMAT	"a.out-i386"
#endif

#ifdef TE_FreeBSD
#define ELF_TARGET_FORMAT	"elf32-i386-freebsd"
#endif
#ifndef ELF_TARGET_FORMAT
#define ELF_TARGET_FORMAT	"elf32-i386"
#endif

#if ((defined (OBJ_MAYBE_COFF) && defined (OBJ_MAYBE_AOUT)) \
     || defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF))
extern const char *i386_target_format PARAMS ((void));
#define TARGET_FORMAT i386_target_format ()
#else
#ifdef OBJ_ELF
#define TARGET_FORMAT		ELF_TARGET_FORMAT
#endif
#ifdef OBJ_AOUT
#define TARGET_FORMAT		AOUT_TARGET_FORMAT
#endif
#endif

#if (defined (OBJ_MAYBE_ELF) || defined (OBJ_ELF))
#define md_end i386_elf_emit_arch_note
extern void i386_elf_emit_arch_note PARAMS ((void));
#endif

#define SUB_SEGMENT_ALIGN(SEG, FRCHAIN) 0

#define LOCAL_LABELS_FB 1

extern const char extra_symbol_chars[];
#define tc_symbol_chars extra_symbol_chars

#define MAX_OPERANDS 3		/* max operands per insn */
#define MAX_IMMEDIATE_OPERANDS 2/* max immediates per insn (lcall, ljmp) */
#define MAX_MEMORY_OPERANDS 2	/* max memory refs per insn (string ops) */

/* Prefixes will be emitted in the order defined below.
   WAIT_PREFIX must be the first prefix since FWAIT is really is an
   instruction, and so must come before any prefixes.  */
#define WAIT_PREFIX	0
#define LOCKREP_PREFIX	1
#define ADDR_PREFIX	2
#define DATA_PREFIX	3
#define SEG_PREFIX	4
#define REX_PREFIX	5       /* must come last.  */
#define MAX_PREFIXES	6	/* max prefixes per opcode */

/* we define the syntax here (modulo base,index,scale syntax) */
#define REGISTER_PREFIX '%'
#define IMMEDIATE_PREFIX '$'
#define ABSOLUTE_PREFIX '*'

#define TWO_BYTE_OPCODE_ESCAPE 0x0f
#define NOP_OPCODE (char) 0x90

/* register numbers */
#define EBP_REG_NUM 5
#define ESP_REG_NUM 4

/* modrm_byte.regmem for twobyte escape */
#define ESCAPE_TO_TWO_BYTE_ADDRESSING ESP_REG_NUM
/* index_base_byte.index for no index register addressing */
#define NO_INDEX_REGISTER ESP_REG_NUM
/* index_base_byte.base for no base register addressing */
#define NO_BASE_REGISTER EBP_REG_NUM
#define NO_BASE_REGISTER_16 6

/* these are the instruction mnemonic suffixes.  */
#define WORD_MNEM_SUFFIX  'w'
#define BYTE_MNEM_SUFFIX  'b'
#define SHORT_MNEM_SUFFIX 's'
#define LONG_MNEM_SUFFIX  'l'
#define QWORD_MNEM_SUFFIX  'q'
/* Intel Syntax */
#define LONG_DOUBLE_MNEM_SUFFIX 'x'

/* modrm.mode = REGMEM_FIELD_HAS_REG when a register is in there */
#define REGMEM_FIELD_HAS_REG 0x3/* always = 0x3 */
#define REGMEM_FIELD_HAS_MEM (~REGMEM_FIELD_HAS_REG)

#define END_OF_INSN '\0'

/* Intel Syntax */
/* Values 0-4 map onto scale factor */
#define BYTE_PTR     0
#define WORD_PTR     1
#define DWORD_PTR    2
#define QWORD_PTR    3
#define XWORD_PTR    4
#define SHORT        5
#define OFFSET_FLAT  6
#define FLAT         7
#define NONE_FOUND   8

typedef struct
{
  /* instruction name sans width suffix ("mov" for movl insns) */
  char *name;

  /* how many operands */
  unsigned int operands;

  /* base_opcode is the fundamental opcode byte without optional
     prefix(es).  */
  unsigned int base_opcode;

  /* extension_opcode is the 3 bit extension for group <n> insns.
     This field is also used to store the 8-bit opcode suffix for the
     AMD 3DNow! instructions.
     If this template has no extension opcode (the usual case) use None */
  unsigned int extension_opcode;
#define None 0xffff		/* If no extension_opcode is possible.  */

  /* cpu feature flags */
  unsigned int cpu_flags;
#define Cpu086		  0x1	/* Any old cpu will do, 0 does the same */
#define Cpu186		  0x2	/* i186 or better required */
#define Cpu286		  0x4	/* i286 or better required */
#define Cpu386		  0x8	/* i386 or better required */
#define Cpu486		 0x10	/* i486 or better required */
#define Cpu586		 0x20	/* i585 or better required */
#define Cpu686		 0x40	/* i686 or better required */
#define CpuP4		 0x80	/* Pentium4 or better required */
#define CpuK6		0x100	/* AMD K6 or better required*/
#define CpuAthlon	0x200	/* AMD Athlon or better required*/
#define CpuSledgehammer 0x400	/* Sledgehammer or better required */
#define CpuMMX		0x800	/* MMX support required */
#define CpuSSE	       0x1000	/* Streaming SIMD extensions required */
#define CpuSSE2	       0x2000	/* Streaming SIMD extensions 2 required */
#define Cpu3dnow       0x4000	/* 3dnow! support required */
#define CpuPNI	       0x8000	/* Prescott New Instructions required */
#define CpuPadLock    0x10000	/* VIA PadLock required */
#define CpuSSSE3      0x80000	/* Supplemental Streaming SIMD extensions 3 required */

  /* These flags are set by gas depending on the flag_code.  */
#define Cpu64	     0x4000000   /* 64bit support required  */
#define CpuNo64      0x8000000   /* Not supported in the 64bit mode  */

  /* The default value for unknown CPUs - enable all features to avoid problems.  */
#define CpuUnknownFlags (Cpu086|Cpu186|Cpu286|Cpu386|Cpu486|Cpu586|Cpu686|CpuP4|CpuSledgehammer|CpuMMX|CpuSSE|CpuSSE2|CpuPNI|Cpu3dnow|CpuK6|CpuAthlon|CpuPadLock|CpuSSSE3)

  /* the bits in opcode_modifier are used to generate the final opcode from
     the base_opcode.  These bits also are used to detect alternate forms of
     the same instruction */
  unsigned int opcode_modifier;

  /* opcode_modifier bits: */
#define W		   0x1	/* set if operands can be words or dwords
				   encoded the canonical way */
#define D		   0x2	/* D = 0 if Reg --> Regmem;
				   D = 1 if Regmem --> Reg:    MUST BE 0x2 */
#define Modrm		   0x4
#define FloatR		   0x8	/* src/dest swap for floats:   MUST BE 0x8 */
#define ShortForm	  0x10	/* register is in low 3 bits of opcode */
#define FloatMF		  0x20	/* FP insn memory format bit, sized by 0x4 */
#define Jump		  0x40	/* special case for jump insns.  */
#define JumpDword	  0x80  /* call and jump */
#define JumpByte	 0x100  /* loop and jecxz */
#define JumpInterSegment 0x200	/* special case for intersegment leaps/calls */
#define FloatD		 0x400	/* direction for float insns:  MUST BE 0x400 */
#define Seg2ShortForm	 0x800	/* encoding of load segment reg insns */
#define Seg3ShortForm	0x1000	/* fs/gs segment register insns.  */
#define Size16		0x2000	/* needs size prefix if in 32-bit mode */
#define Size32		0x4000	/* needs size prefix if in 16-bit mode */
#define Size64		0x8000	/* needs size prefix if in 16-bit mode */
#define IgnoreSize     0x10000  /* instruction ignores operand size prefix */
#define DefaultSize    0x20000  /* default insn size depends on mode */
#define No_bSuf	       0x40000	/* b suffix on instruction illegal */
#define No_wSuf	       0x80000	/* w suffix on instruction illegal */
#define No_lSuf	      0x100000 	/* l suffix on instruction illegal */
#define No_sSuf	      0x200000	/* s suffix on instruction illegal */
#define No_qSuf       0x400000  /* q suffix on instruction illegal */
#define No_xSuf       0x800000  /* x suffix on instruction illegal */
#define FWait	     0x1000000	/* instruction needs FWAIT */
#define IsString     0x2000000	/* quick test for string instructions */
#define regKludge    0x4000000	/* fake an extra reg operand for clr, imul */
#define IsPrefix     0x8000000	/* opcode is a prefix */
#define ImmExt	    0x10000000	/* instruction has extension in 8 bit imm */
#define NoRex64	    0x20000000  /* instruction don't need Rex64 prefix.  */
#define Rex64	    0x40000000  /* instruction require Rex64 prefix.  */
#define Ugh	    0x80000000	/* deprecated fp insn, gets a warning */

  /* operand_types[i] describes the type of operand i.  This is made
     by OR'ing together all of the possible type masks.  (e.g.
     'operand_types[i] = Reg|Imm' specifies that operand i can be
     either a register or an immediate operand.  */
  unsigned int operand_types[3];

  /* operand_types[i] bits */
  /* register */
#define Reg8		   0x1	/* 8 bit reg */
#define Reg16		   0x2	/* 16 bit reg */
#define Reg32		   0x4	/* 32 bit reg */
#define Reg64		   0x8	/* 64 bit reg */
  /* immediate */
#define Imm8		  0x10	/* 8 bit immediate */
#define Imm8S		  0x20	/* 8 bit immediate sign extended */
#define Imm16		  0x40	/* 16 bit immediate */
#define Imm32		  0x80	/* 32 bit immediate */
#define Imm32S		 0x100	/* 32 bit immediate sign extended */
#define Imm64		 0x200	/* 64 bit immediate */
#define Imm1		 0x400	/* 1 bit immediate */
  /* memory */
#define BaseIndex	 0x800
  /* Disp8,16,32 are used in different ways, depending on the
     instruction.  For jumps, they specify the size of the PC relative
     displacement, for baseindex type instructions, they specify the
     size of the offset relative to the base register, and for memory
     offset instructions such as `mov 1234,%al' they specify the size of
     the offset relative to the segment base.  */
#define Disp8		0x1000	/* 8 bit displacement */
#define Disp16		0x2000	/* 16 bit displacement */
#define Disp32		0x4000	/* 32 bit displacement */
#define Disp32S	        0x8000	/* 32 bit signed displacement */
#define Disp64	       0x10000	/* 64 bit displacement */
  /* specials */
#define InOutPortReg   0x20000	/* register to hold in/out port addr = dx */
#define ShiftCount     0x40000	/* register to hold shift cound = cl */
#define Control	       0x80000	/* Control register */
#define Debug	      0x100000	/* Debug register */
#define Test	      0x200000	/* Test register */
#define FloatReg      0x400000	/* Float register */
#define FloatAcc      0x800000	/* Float stack top %st(0) */
#define SReg2	     0x1000000	/* 2 bit segment register */
#define SReg3	     0x2000000	/* 3 bit segment register */
#define Acc	     0x4000000	/* Accumulator %al or %ax or %eax */
#define JumpAbsolute 0x8000000
#define RegMMX	    0x10000000	/* MMX register */
#define RegXMM	    0x20000000	/* XMM registers in PIII */
#define EsSeg	    0x40000000	/* String insn operand with fixed es segment */

  /* InvMem is for instructions with a modrm byte that only allow a
     general register encoding in the i.tm.mode and i.tm.regmem fields,
     eg. control reg moves.  They really ought to support a memory form,
     but don't, so we add an InvMem flag to the register operand to
     indicate that it should be encoded in the i.tm.regmem field.  */
#define InvMem	    0x80000000

#define Reg	(Reg8|Reg16|Reg32|Reg64) /* gen'l register */
#define WordReg (Reg16|Reg32|Reg64)
#define ImplicitRegister (InOutPortReg|ShiftCount|Acc|FloatAcc)
#define Imm	(Imm8|Imm8S|Imm16|Imm32S|Imm32|Imm64) /* gen'l immediate */
#define EncImm	(Imm8|Imm16|Imm32|Imm32S) /* Encodable gen'l immediate */
#define Disp	(Disp8|Disp16|Disp32|Disp32S|Disp64) /* General displacement */
#define AnyMem	(Disp8|Disp16|Disp32|Disp32S|BaseIndex|InvMem)	/* General memory */
  /* The following aliases are defined because the opcode table
     carefully specifies the allowed memory types for each instruction.
     At the moment we can only tell a memory reference size by the
     instruction suffix, so there's not much point in defining Mem8,
     Mem16, Mem32 and Mem64 opcode modifiers - We might as well just use
     the suffix directly to check memory operands.  */
#define LLongMem AnyMem		/* 64 bits (or more) */
#define LongMem AnyMem		/* 32 bit memory ref */
#define ShortMem AnyMem		/* 16 bit memory ref */
#define WordMem AnyMem		/* 16 or 32 bit memory ref */
#define ByteMem AnyMem		/* 8 bit memory ref */
}
template;

/*
  'templates' is for grouping together 'template' structures for opcodes
  of the same name.  This is only used for storing the insns in the grand
  ole hash table of insns.
  The templates themselves start at START and range up to (but not including)
  END.
  */
typedef struct
{
  const template *start;
  const template *end;
}
templates;

/* these are for register name --> number & type hash lookup */
typedef struct
{
  char *reg_name;
  unsigned int reg_type;
  unsigned int reg_flags;
#define RegRex	    0x1  /* Extended register.  */
#define RegRex64    0x2  /* Extended 8 bit register.  */
  unsigned int reg_num;
}
reg_entry;

typedef struct
{
  char *seg_name;
  unsigned int seg_prefix;
}
seg_entry;

/* 386 operand encoding bytes:  see 386 book for details of this.  */
typedef struct
{
  unsigned int regmem;	/* codes register or memory operand */
  unsigned int reg;	/* codes register operand (or extended opcode) */
  unsigned int mode;	/* how to interpret regmem & reg */
}
modrm_byte;

/* x86-64 extension prefix.  */
typedef int rex_byte;
#define REX_OPCODE	0x40

/* Indicates 64 bit operand size.  */
#define REX_MODE64	8
/* High extension to reg field of modrm byte.  */
#define REX_EXTX	4
/* High extension to SIB index field.  */
#define REX_EXTY	2
/* High extension to base field of modrm or SIB, or reg field of opcode.  */
#define REX_EXTZ	1

/* 386 opcode byte to code indirect addressing.  */
typedef struct
{
  unsigned base;
  unsigned index;
  unsigned scale;
}
sib_byte;

/* x86 arch names and features */
typedef struct
{
  const char *name;	/* arch name */
  unsigned int flags;	/* cpu feature flags */
}
arch_entry;

/* The name of the global offset table generated by the compiler. Allow
   this to be overridden if need be.  */
#ifndef GLOBAL_OFFSET_TABLE_NAME
#define GLOBAL_OFFSET_TABLE_NAME "_GLOBAL_OFFSET_TABLE_"
#endif

#ifndef LEX_AT
#define TC_PARSE_CONS_EXPRESSION(EXP, NBYTES) x86_cons (EXP, NBYTES)
extern void x86_cons PARAMS ((expressionS *, int));

#define TC_CONS_FIX_NEW(FRAG,OFF,LEN,EXP) x86_cons_fix_new(FRAG, OFF, LEN, EXP)
extern void x86_cons_fix_new
  PARAMS ((fragS *, unsigned int, unsigned int, expressionS *));
#endif

#define DIFF_EXPR_OK    /* foo-. gets turned into PC relative relocs */

#define NO_RELOC BFD_RELOC_NONE

void i386_validate_fix PARAMS ((struct fix *));
#define TC_VALIDATE_FIX(FIX,SEGTYPE,SKIP) i386_validate_fix(FIX)

#define tc_fix_adjustable(X)  tc_i386_fix_adjustable(X)
extern int tc_i386_fix_adjustable PARAMS ((struct fix *));

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* ELF wants external syms kept, as does PE COFF.  */
#if defined (TE_PE) && defined (STRICT_PE_FORMAT)
#define EXTERN_FORCE_RELOC				\
  (OUTPUT_FLAVOR == bfd_target_elf_flavour		\
   || OUTPUT_FLAVOR == bfd_target_coff_flavour)
#else
#define EXTERN_FORCE_RELOC				\
  (OUTPUT_FLAVOR == bfd_target_elf_flavour)
#endif

/* This expression evaluates to true if the relocation is for a local
   object for which we still want to do the relocation at runtime.
   False if we are willing to perform this relocation while building
   the .o file.  GOTOFF does not need to be checked here because it is
   not pcrel.  I am not sure if some of the others are ever used with
   pcrel, but it is easier to be safe than sorry.  */

#define TC_FORCE_RELOCATION_LOCAL(FIX)			\
  (!(FIX)->fx_pcrel					\
   || (FIX)->fx_plt					\
   || (FIX)->fx_r_type == BFD_RELOC_386_PLT32		\
   || (FIX)->fx_r_type == BFD_RELOC_386_GOT32		\
   || (FIX)->fx_r_type == BFD_RELOC_386_GOTPC		\
   || TC_FORCE_RELOCATION (FIX))

#define md_operand(x)

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

extern int optimize_align_code;

#define md_do_align(n, fill, len, max, around)				\
if ((n)									\
    && !need_pass_2							\
    && optimize_align_code						\
    && (!(fill)								\
	|| ((char)*(fill) == (char)0x90 && (len) == 1))			\
    && subseg_text_p (now_seg))						\
  {									\
    frag_align_code ((n), (max));					\
    goto around;							\
  }

#define MAX_MEM_FOR_RS_ALIGN_CODE  15

extern void i386_align_code PARAMS ((fragS *, int));

#define HANDLE_ALIGN(fragP)						\
if (fragP->fr_type == rs_align_code) 					\
  i386_align_code (fragP, (fragP->fr_next->fr_address			\
			   - fragP->fr_address				\
			   - fragP->fr_fix));

void i386_print_statistics PARAMS ((FILE *));
#define tc_print_statistics i386_print_statistics

#define md_number_to_chars number_to_chars_littleendian

#ifdef SCO_ELF
#define tc_init_after_args() sco_id ()
extern void sco_id PARAMS ((void));
#endif

/* We want .cfi_* pseudo-ops for generating unwind info.  */
#define TARGET_USE_CFIPOP 1

extern unsigned int x86_dwarf2_return_column;
#define DWARF2_DEFAULT_RETURN_COLUMN x86_dwarf2_return_column

extern int x86_cie_data_alignment;
#define DWARF2_CIE_DATA_ALIGNMENT x86_cie_data_alignment

#define tc_regname_to_dw2regnum tc_x86_regname_to_dw2regnum
extern int tc_x86_regname_to_dw2regnum PARAMS ((const char *regname));

#define tc_cfi_frame_initial_instructions tc_x86_frame_initial_instructions
extern void tc_x86_frame_initial_instructions PARAMS ((void));

#endif /* TC_I386 */
