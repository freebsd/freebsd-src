/* tc-i386.h -- Header file for tc-i386.c
   Copyright (C) 1989, 92, 93, 94, 95, 96, 97, 1998 Free Software Foundation.

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

#ifdef ANSI_PROTOTYPES
struct fix;
#endif

#define TARGET_BYTES_BIG_ENDIAN	0

#ifdef TE_LYNX
#define TARGET_FORMAT		"coff-i386-lynx"
#endif

#ifdef BFD_ASSEMBLER
/* This is used to determine relocation types in tc-i386.c.  The first
   parameter is the current relocation type, the second one is the desired
   type.  The idea is that if the original type is already some kind of PIC
   relocation, we leave it alone, otherwise we give it the desired type */

#define TC_RELOC(X,Y) (((X) != BFD_RELOC_386_PLT32 && \
	   (X) != BFD_RELOC_386_GOTOFF && \
	   (X) != BFD_RELOC_386_GOT32 && \
	   (X) != BFD_RELOC_386_GOTPC) ? Y : X)

#define tc_fix_adjustable(X)  tc_i386_fix_adjustable(X)
extern int tc_i386_fix_adjustable PARAMS ((struct fix *));

/* This is the relocation type for direct references to GLOBAL_OFFSET_TABLE.
 * It comes up in complicated expressions such as 
 * _GLOBAL_OFFSET_TABLE_+[.-.L284], which cannot be expressed normally with
 * the regular expressions.  The fixup specified here when used at runtime 
 * implies that we should add the address of the GOT to the specified location,
 * and as a result we have simplified the expression into something we can use.
 */
#define TC_RELOC_GLOBAL_OFFSET_TABLE BFD_RELOC_386_GOTPC

/* This expression evaluates to false if the relocation is for a local object
   for which we still want to do the relocation at runtime.  True if we
   are willing to perform this relocation while building the .o file.
   This is only used for pcrel relocations, so GOTOFF does not need to be
   checked here.  I am not sure if some of the others are ever used with
   pcrel, but it is easier to be safe than sorry. */

#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)  \
  ((FIX)->fx_r_type != BFD_RELOC_386_PLT32 \
   && (FIX)->fx_r_type != BFD_RELOC_386_GOT32 \
   && (FIX)->fx_r_type != BFD_RELOC_386_GOTPC)

#define TARGET_ARCH		bfd_arch_i386

#ifdef OBJ_AOUT
#ifdef TE_NetBSD
#define TARGET_FORMAT		"a.out-i386-netbsd"
#endif
#ifdef TE_386BSD
#define TARGET_FORMAT		"a.out-i386-bsd"
#endif
#ifdef TE_LINUX
#define TARGET_FORMAT		"a.out-i386-linux"
#endif
#ifdef TE_Mach
#define TARGET_FORMAT		"a.out-mach3"
#endif
#ifdef TE_DYNIX
#define TARGET_FORMAT		"a.out-i386-dynix"
#endif
#ifndef TARGET_FORMAT
#define TARGET_FORMAT		"a.out-i386"
#endif
#endif /* OBJ_AOUT */

#ifdef OBJ_ELF
#define TARGET_FORMAT		"elf32-i386"
#endif

#ifdef OBJ_MAYBE_ELF
#ifdef OBJ_MAYBE_COFF
extern const char *i386_target_format PARAMS ((void));
#define TARGET_FORMAT i386_target_format ()
#endif
#endif

#else /* ! BFD_ASSEMBLER */

/* COFF STUFF */

#define COFF_MAGIC I386MAGIC
#define BFD_ARCH bfd_arch_i386
#define COFF_FLAGS F_AR32WR
#define TC_COUNT_RELOC(x) ((x)->fx_addsy || (x)->fx_r_type==7)
#define TC_FORCE_RELOCATION(x) ((x)->fx_r_type==7)
#define TC_COFF_FIX2RTYPE(fixP) tc_coff_fix2rtype(fixP)
extern short tc_coff_fix2rtype PARAMS ((struct fix *));
#define TC_COFF_SIZEMACHDEP(frag) tc_coff_sizemachdep(frag)
extern int tc_coff_sizemachdep PARAMS ((fragS *frag));
#define SUB_SEGMENT_ALIGN(SEG) 2
#define TC_RVA_RELOC 7
/* Need this for PIC relocations */
#define NEED_FX_R_TYPE


#ifdef TE_386BSD
/* The BSDI linker apparently rejects objects with a machine type of
   M_386 (100).  */
#define AOUT_MACHTYPE 0
#else
#define AOUT_MACHTYPE 100
#endif

#undef REVERSE_SORT_RELOCS

#endif /* ! BFD_ASSEMBLER */

#ifdef BFD_ASSEMBLER
#define NO_RELOC BFD_RELOC_NONE
#else
#define NO_RELOC 0
#endif
#define tc_coff_symbol_emit_hook(a)	;	/* not used */

#ifndef BFD_ASSEMBLER
#ifndef OBJ_AOUT
#ifndef TE_PE
/* Local labels starts with .L */
#define LOCAL_LABEL(name) (name[0] == '.' \
		 && (name[1] == 'L' || name[1] == 'X' || name[1] == '.'))
#endif
#endif
#endif

#define LOCAL_LABELS_FB 1

#define tc_aout_pre_write_hook(x)	{;}	/* not used */
#define tc_crawl_symbol_chain(a)	{;}	/* not used */
#define tc_headers_hook(a)		{;}	/* not used */

#define MAX_OPERANDS 3		/* max operands per insn */
#define MAX_PREFIXES 5		/* max prefixes per opcode */
#define MAX_IMMEDIATE_OPERANDS 2/* max immediates per insn */
#define MAX_MEMORY_OPERANDS 2	/* max memory ref per insn (lcall uses 2) */

/* we define the syntax here (modulo base,index,scale syntax) */
#define REGISTER_PREFIX '%'
#define IMMEDIATE_PREFIX '$'
#define ABSOLUTE_PREFIX '*'
#define PREFIX_SEPERATOR '/'

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

/* these are the att as opcode suffixes, making movl --> mov, for example */
#define DWORD_OPCODE_SUFFIX 'l'
#define WORD_OPCODE_SUFFIX  'w'
#define BYTE_OPCODE_SUFFIX  'b'

/* modrm.mode = REGMEM_FIELD_HAS_REG when a register is in there */
#define REGMEM_FIELD_HAS_REG 0x3/* always = 0x3 */
#define REGMEM_FIELD_HAS_MEM (~REGMEM_FIELD_HAS_REG)

#define END_OF_INSN '\0'

/*
  When an operand is read in it is classified by its type.  This type includes
  all the possible ways an operand can be used.  Thus, '%eax' is both 'register
  # 0' and 'The Accumulator'.  In our language this is expressed by OR'ing
  'Reg32' (any 32 bit register) and 'Acc' (the accumulator).
  Operands are classified so that we can match given operand types with
  the opcode table in i386-opcode.h.
  */
#define Unknown 0x0
/* register */
#define Reg8    0x1		/* 8 bit reg */
#define Reg16   0x2		/* 16 bit reg */
#define Reg32   0x4		/* 32 bit reg */
#define Reg     (Reg8|Reg16|Reg32)	/* gen'l register */
#define WordReg (Reg16|Reg32)	/* for push/pop operands */
/* immediate */
#define Imm8    0x8		/* 8 bit immediate */
#define Imm8S	0x10		/* 8 bit immediate sign extended */
#define Imm16   0x20		/* 16 bit immediate */
#define Imm32   0x40		/* 32 bit immediate */
#define Imm1    0x80		/* 1 bit immediate */
#define ImmUnknown Imm32	/* for unknown expressions */
#define Imm     (Imm8|Imm8S|Imm16|Imm32)	/* gen'l immediate */
/* memory */
#define Disp8   0x200		/* 8 bit displacement (for jumps) */
#define Disp16  0x400		/* 16 bit displacement */
#define Disp32  0x800		/* 32 bit displacement */
#define Disp    (Disp8|Disp16|Disp32)	/* General displacement */
#define DispUnknown Disp32	/* for unknown size displacements */
#define Mem8    0x1000
#define Mem16   0x2000
#define Mem32   0x4000
#define BaseIndex 0x8000
#define Mem     (Disp|Mem8|Mem16|Mem32|BaseIndex)	/* General memory */
#define WordMem   (Mem16|Mem32|Disp|BaseIndex)
#define ByteMem   (Mem8|Disp|BaseIndex)
/* specials */
#define InOutPortReg 0x10000	/* register to hold in/out port addr = dx */
#define ShiftCount 0x20000	/* register to hold shift cound = cl */
#define Control 0x40000		/* Control register */
#define Debug   0x80000		/* Debug register */
#define Test    0x100000	/* Test register */
#define FloatReg 0x200000	/* Float register */
#define FloatAcc 0x400000	/* Float stack top %st(0) */
#define SReg2   0x800000	/* 2 bit segment register */
#define SReg3   0x1000000	/* 3 bit segment register */
#define Acc     0x2000000	/* Accumulator %al or %ax or %eax */
#define ImplicitRegister (InOutPortReg|ShiftCount|Acc|FloatAcc)
#define JumpAbsolute 0x4000000
#define Abs8  0x08000000
#define Abs16 0x10000000
#define Abs32 0x20000000
#define Abs (Abs8|Abs16|Abs32)
#define RegMMX 0x40000000	/* MMX register */

#define Byte (Reg8|Imm8|Imm8S)
#define Word (Reg16|Imm16)
#define DWord (Reg32|Imm32)

#define SMALLEST_DISP_TYPE(num) \
    fits_in_signed_byte(num) ? (Disp8|Disp32|Abs8|Abs32) : (Disp32|Abs32)

typedef struct
{
  /* instruction name sans width suffix ("mov" for movl insns) */
  char *name;

  /* how many operands */
  unsigned int operands;

  /* base_opcode is the fundamental opcode byte with a optional prefix(es). */
  unsigned int base_opcode;

  /* extension_opcode is the 3 bit extension for group <n> insns.
     If this template has no extension opcode (the usual case) use None */
  unsigned char extension_opcode;
#define None 0xff		/* If no extension_opcode is possible. */

  /* the bits in opcode_modifier are used to generate the final opcode from
     the base_opcode.  These bits also are used to detect alternate forms of
     the same instruction */
  unsigned int opcode_modifier;

  /* opcode_modifier bits: */
#define W        0x1		/* set if operands are words or dwords */
#define D        0x2		/* D = 0 if Reg --> Regmem; D = 1 if Regmem --> Reg */
  /* direction flag for floating insns:  MUST BE 0x400 */
#define FloatD 0x400
  /* shorthand */
#define DW (D|W)
#define ShortForm 0x10		/* register is in low 3 bits of opcode */
#define ShortFormW 0x20		/* ShortForm and W bit is 0x8 */
#define Seg2ShortForm 0x40	/* encoding of load segment reg insns */
#define Seg3ShortForm 0x80	/* fs/gs segment register insns. */
#define Jump 0x100		/* special case for jump insns. */
#define JumpInterSegment 0x200	/* special case for intersegment leaps/calls */
  /* 0x400 CANNOT BE USED since it's already used by FloatD above */
#define DONT_USE 0x400
#define NoModrm 0x800
#define Modrm 0x1000
#define imulKludge 0x2000
#define JumpByte 0x4000
#define JumpDword 0x8000
#define ReverseRegRegmem 0x10000
#define Data16 0x20000		/* needs data prefix if in 32-bit mode */
#define Data32 0x40000		/* needs data prefix if in 16-bit mode */
#define iclrKludge 0x80000	/* used to convert clr to xor */
#define FWait 0x100000		/* instruction needs FWAIT */

  /* (opcode_modifier & COMES_IN_ALL_SIZES) is true if the
     instuction comes in byte, word, and dword sizes and is encoded into
     machine code in the canonical way. */
#define COMES_IN_ALL_SIZES (W)

  /* (opcode_modifier & COMES_IN_BOTH_DIRECTIONS) indicates that the
     source and destination operands can be reversed by setting either
     the D (for integer insns) or the FloatD (for floating insns) bit
     in base_opcode. */
#define COMES_IN_BOTH_DIRECTIONS (D|FloatD)

  /* operand_types[i] describes the type of operand i.  This is made
     by OR'ing together all of the possible type masks.  (e.g.
     'operand_types[i] = Reg|Imm' specifies that operand i can be
     either a register or an immediate operand */
  unsigned int operand_types[3];
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
    template *start;
    template *end;
  } templates;

/* these are for register name --> number & type hash lookup */
typedef struct
  {
    char *reg_name;
    unsigned int reg_type;
    unsigned int reg_num;
  }

reg_entry;

typedef struct
  {
    char *seg_name;
    unsigned int seg_prefix;
  }

seg_entry;

/* these are for prefix name --> prefix code hash lookup */
typedef struct
  {
    char *prefix_name;
    unsigned char prefix_code;
  }

prefix_entry;

/* 386 operand encoding bytes:  see 386 book for details of this. */
typedef struct
  {
    unsigned regmem:3;		/* codes register or memory operand */
    unsigned reg:3;		/* codes register operand (or extended opcode) */
    unsigned mode:2;		/* how to interpret regmem & reg */
  }

modrm_byte;

/* 386 opcode byte to code indirect addressing. */
typedef struct
  {
    unsigned base:3;
    unsigned index:3;
    unsigned scale:2;
  }

base_index_byte;

/* The name of the global offset table generated by the compiler. Allow
   this to be overridden if need be. */
#ifndef GLOBAL_OFFSET_TABLE_NAME
#define GLOBAL_OFFSET_TABLE_NAME "_GLOBAL_OFFSET_TABLE_"
#endif

#ifdef BFD_ASSEMBLER
void i386_validate_fix PARAMS ((struct fix *));
#define TC_VALIDATE_FIX(FIXP,SEGTYPE,SKIP) i386_validate_fix(FIXP)
#endif

#endif /* TC_I386 */

#define md_operand(x)

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table


extern int flag_16bit_code;

#ifdef BFD_ASSEMBLER
#define md_maybe_text() \
  ((bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE) != 0)
#else
#define md_maybe_text() \
  (now_seg != data_section && now_seg != bss_section)
#endif

#define md_do_align(n, fill, len, max, around)				\
if ((n) && !need_pass_2							\
    && (!(fill) || ((char)*(fill) == (char)0x90 && (len) == 1))		\
    && md_maybe_text ())						\
  {									\
    char *p;								\
    p = frag_var (rs_align_code, 15, 1, (relax_substateT) max,		\
		  (symbolS *) 0, (offsetT) (n), (char *) 0);		\
    *p = 0x90;								\
    goto around;							\
  }

extern void i386_align_code PARAMS ((fragS *, int));

#define HANDLE_ALIGN(fragP)						\
if (fragP->fr_type == rs_align_code) 					\
  i386_align_code (fragP, (fragP->fr_next->fr_address			\
			   - fragP->fr_address				\
			   - fragP->fr_fix));

/* call md_apply_fix3 with segment instead of md_apply_fix */
#define MD_APPLY_FIX3

void i386_print_statistics PARAMS ((FILE *));
#define tc_print_statistics i386_print_statistics

#define md_number_to_chars number_to_chars_littleendian

#ifdef SCO_ELF
#define tc_init_after_args() sco_id ()
extern void sco_id PARAMS ((void));
#endif

#define DIFF_EXPR_OK    /* foo-. gets turned into PC relative relocs */

/* end of tc-i386.h */
