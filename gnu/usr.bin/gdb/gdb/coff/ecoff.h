#ifndef ECOFF_H
#define ECOFF_H

/* Generic ECOFF support.
   This does not include symbol information, found in sym.h and
   symconst.h.  */

/* Mips magic numbers used in filehdr.  MIPS_MAGIC_LITTLE is used on
   little endian machines.  MIPS_MAGIC_BIG is used on big endian
   machines.  Where is MIPS_MAGIC_1 from?  */
#define MIPS_MAGIC_1 0x0180
#define MIPS_MAGIC_LITTLE 0x0162
#define MIPS_MAGIC_BIG 0x0160

/* These are the magic numbers used for MIPS code compiled at ISA
   level 2.  */
#define MIPS_MAGIC_LITTLE2 0x0166
#define MIPS_MAGIC_BIG2 0x0163

/* These are the magic numbers used for MIPS code compiled at ISA
   level 3.  */
#define MIPS_MAGIC_LITTLE3 0x142
#define MIPS_MAGIC_BIG3 0x140

/* Alpha magic numbers used in filehdr.  */
#define ALPHA_MAGIC 0x183

/* Magic numbers used in a.out header.  */
#define ECOFF_AOUT_OMAGIC 0407	/* not demand paged (ld -N).  */
#define ECOFF_AOUT_ZMAGIC 0413	/* demand load format, eg normal ld output */

/* Names of special sections.  */
#define _TEXT   ".text"
#define _DATA   ".data"
#define _BSS    ".bss"
#define _RDATA	".rdata"
#define _SDATA	".sdata"
#define _SBSS	".sbss"
#define _LIT4	".lit4"
#define _LIT8	".lit8"
#define _LIB	".lib"
#define _INIT	".init"
#define _FINI	".fini"

/* ECOFF uses some additional section flags.  */
#define STYP_RDATA 0x100
#define STYP_SDATA 0x200
#define STYP_SBSS 0x400
#define STYP_ECOFF_FINI 0x1000000
#define STYP_LIT8 0x8000000
#define STYP_LIT4 0x10000000
#define STYP_ECOFF_INIT 0x80000000
#define STYP_OTHER_LOAD (STYP_ECOFF_INIT | STYP_ECOFF_FINI)

/* The linker needs a section to hold small common variables while
   linking.  There is no convenient way to create it when the linker
   needs it, so we always create one for each BFD.  We then avoid
   writing it out.  */
#define SCOMMON ".scommon"

/* The ECOFF a.out header carries information about register masks and
   the gp value.  The assembler needs to be able to write out this
   information, and objcopy needs to be able to copy it from one file
   to another.  To handle this in BFD, we use a dummy section to hold
   the information.  We call this section .reginfo, since MIPS ELF has
   a .reginfo section which serves a similar purpose.  When BFD
   recognizes an ECOFF object, it copies the information into a
   private data structure.  When the .reginfo section is read, the
   information is retrieved from the private data structure.  When the
   .reginfo section is written, the information in the private data
   structure is updated.  The contents of the .reginfo section, as
   seen by programs outside BFD, is a ecoff_reginfo structure.  The
   contents of the structure are as seen on the host, so no swapping
   issues arise.

   The assembler used to update the private BFD data structures
   directly.  With this approach, it instead just creates a .reginfo
   section and updates that.  The real advantage of this approach is
   that objcopy works automatically.  */
#define REGINFO ".reginfo"
struct ecoff_reginfo
{
  bfd_vma gp_value;		/* GP register value.		*/
  unsigned long gprmask;	/* General registers used.	*/
  unsigned long cprmask[4];	/* Coprocessor registers used.	*/
  unsigned long fprmask;	/* Floating pointer registers used.  */
};  

/* If the extern bit in a reloc is 1, then r_symndx is an index into
   the external symbol table.  If the extern bit is 0, then r_symndx
   indicates a section, and is one of the following values.  */
#define RELOC_SECTION_NONE	0
#define RELOC_SECTION_TEXT	1
#define RELOC_SECTION_RDATA	2
#define RELOC_SECTION_DATA	3
#define RELOC_SECTION_SDATA	4
#define RELOC_SECTION_SBSS	5
#define RELOC_SECTION_BSS	6
#define RELOC_SECTION_INIT	7
#define RELOC_SECTION_LIT8	8
#define RELOC_SECTION_LIT4	9
#define RELOC_SECTION_XDATA    10
#define RELOC_SECTION_PDATA    11
#define RELOC_SECTION_FINI     12
#define RELOC_SECTION_LITA     13
#define RELOC_SECTION_ABS      14

/********************** STABS **********************/

/* gcc uses mips-tfile to output type information in special stabs
   entries.  These must match the corresponding definition in
   gcc/config/mips.h.  At some point, these should probably go into a
   shared include file, but currently gcc and gdb do not share any
   directories. */
#define CODE_MASK 0x8F300
#define ECOFF_IS_STAB(sym) (((sym)->index & 0xFFF00) == CODE_MASK)
#define ECOFF_MARK_STAB(code) ((code)+CODE_MASK)
#define ECOFF_UNMARK_STAB(code) ((code)-CODE_MASK)
#define STABS_SYMBOL "@stabs"

/********************** COFF **********************/

/* gcc also uses mips-tfile to output COFF debugging information.
   These are the values it uses when outputting the .type directive.
   These should also be in a shared include file.  */
#define N_BTMASK	(017)
#define N_TMASK		(060)
#define N_BTSHFT	(4)
#define N_TSHIFT	(2)

/********************** AUX **********************/

/* The auxiliary type information is the same on all known ECOFF
   targets.  I can't see any reason that it would ever change, so I am
   going to gamble and define the external structures here, in the
   target independent ECOFF header file.  The internal forms are
   defined in coff/sym.h, which was originally donated by MIPS
   Computer Systems.  */

/* Type information external record */

struct tir_ext {
	unsigned char	t_bits1[1];
	unsigned char	t_tq45[1];
	unsigned char	t_tq01[1];
	unsigned char	t_tq23[1];
};

#define	TIR_BITS1_FBITFIELD_BIG		0x80
#define	TIR_BITS1_FBITFIELD_LITTLE	0x01

#define	TIR_BITS1_CONTINUED_BIG		0x40
#define	TIR_BITS1_CONTINUED_LITTLE	0x02

#define	TIR_BITS1_BT_BIG		0x3F
#define	TIR_BITS1_BT_SH_BIG		0
#define	TIR_BITS1_BT_LITTLE		0xFC
#define	TIR_BITS1_BT_SH_LITTLE		2

#define	TIR_BITS_TQ4_BIG		0xF0
#define	TIR_BITS_TQ4_SH_BIG		4
#define	TIR_BITS_TQ5_BIG		0x0F
#define	TIR_BITS_TQ5_SH_BIG		0
#define	TIR_BITS_TQ4_LITTLE		0x0F
#define	TIR_BITS_TQ4_SH_LITTLE		0
#define	TIR_BITS_TQ5_LITTLE		0xF0
#define	TIR_BITS_TQ5_SH_LITTLE		4

#define	TIR_BITS_TQ0_BIG		0xF0
#define	TIR_BITS_TQ0_SH_BIG		4
#define	TIR_BITS_TQ1_BIG		0x0F
#define	TIR_BITS_TQ1_SH_BIG		0
#define	TIR_BITS_TQ0_LITTLE		0x0F
#define	TIR_BITS_TQ0_SH_LITTLE		0
#define	TIR_BITS_TQ1_LITTLE		0xF0
#define	TIR_BITS_TQ1_SH_LITTLE		4

#define	TIR_BITS_TQ2_BIG		0xF0
#define	TIR_BITS_TQ2_SH_BIG		4
#define	TIR_BITS_TQ3_BIG		0x0F
#define	TIR_BITS_TQ3_SH_BIG		0
#define	TIR_BITS_TQ2_LITTLE		0x0F
#define	TIR_BITS_TQ2_SH_LITTLE		0
#define	TIR_BITS_TQ3_LITTLE		0xF0
#define	TIR_BITS_TQ3_SH_LITTLE		4

/* Relative symbol external record */

struct rndx_ext {
	unsigned char	r_bits[4];
};

#define	RNDX_BITS0_RFD_SH_LEFT_BIG	4
#define	RNDX_BITS1_RFD_BIG		0xF0
#define	RNDX_BITS1_RFD_SH_BIG		4

#define	RNDX_BITS0_RFD_SH_LEFT_LITTLE	0
#define	RNDX_BITS1_RFD_LITTLE		0x0F
#define	RNDX_BITS1_RFD_SH_LEFT_LITTLE	8

#define	RNDX_BITS1_INDEX_BIG		0x0F
#define	RNDX_BITS1_INDEX_SH_LEFT_BIG	16
#define	RNDX_BITS2_INDEX_SH_LEFT_BIG	8
#define	RNDX_BITS3_INDEX_SH_LEFT_BIG	0

#define	RNDX_BITS1_INDEX_LITTLE		0xF0
#define	RNDX_BITS1_INDEX_SH_LITTLE	4
#define	RNDX_BITS2_INDEX_SH_LEFT_LITTLE	4
#define	RNDX_BITS3_INDEX_SH_LEFT_LITTLE	12

/* Auxiliary symbol information external record */

union aux_ext {
	struct tir_ext	a_ti;
	struct rndx_ext	a_rndx;
	unsigned char	a_dnLow[4];
	unsigned char	a_dnHigh[4];
	unsigned char	a_isym[4];
	unsigned char	a_iss[4];
	unsigned char	a_width[4];
	unsigned char	a_count[4];
};

#define AUX_GET_ANY(bigend, ax, field) \
  ((bigend) ? bfd_getb32 ((ax)->field) : bfd_getl32 ((ax)->field))

#define	AUX_GET_DNLOW(bigend, ax)	AUX_GET_ANY ((bigend), (ax), a_dnLow)
#define	AUX_GET_DNHIGH(bigend, ax)	AUX_GET_ANY ((bigend), (ax), a_dnHigh)
#define	AUX_GET_ISYM(bigend, ax)	AUX_GET_ANY ((bigend), (ax), a_isym)
#define AUX_GET_ISS(bigend, ax)		AUX_GET_ANY ((bigend), (ax), a_iss)
#define AUX_GET_WIDTH(bigend, ax)	AUX_GET_ANY ((bigend), (ax), a_width)
#define AUX_GET_COUNT(bigend, ax)	AUX_GET_ANY ((bigend), (ax), a_count)

#define AUX_PUT_ANY(bigend, val, ax, field) \
  ((bigend) \
   ? (bfd_putb32 ((bfd_vma) (val), (ax)->field), 0) \
   : (bfd_putl32 ((bfd_vma) (val), (ax)->field), 0))

#define AUX_PUT_DNLOW(bigend, val, ax) \
  AUX_PUT_ANY ((bigend), (val), (ax), a_dnLow)
#define AUX_PUT_DNHIGH(bigend, val, ax) \
  AUX_PUT_ANY ((bigend), (val), (ax), a_dnHigh)
#define AUX_PUT_ISYM(bigend, val, ax) \
  AUX_PUT_ANY ((bigend), (val), (ax), a_isym)
#define AUX_PUT_ISS(bigend, val, ax) \
  AUX_PUT_ANY ((bigend), (val), (ax), a_iss)
#define AUX_PUT_WIDTH(bigend, val, ax) \
  AUX_PUT_ANY ((bigend), (val), (ax), a_width)
#define AUX_PUT_COUNT(bigend, val, ax) \
  AUX_PUT_ANY ((bigend), (val), (ax), a_count)

/* Prototypes for the swapping functions.  These require that sym.h be
   included before this file.  */

extern void ecoff_swap_tir_in PARAMS ((int bigend, struct tir_ext *, TIR *));
extern void ecoff_swap_tir_out PARAMS ((int bigend, TIR *, struct tir_ext *));
extern void ecoff_swap_rndx_in PARAMS ((int bigend, struct rndx_ext *,
					RNDXR *));
extern void ecoff_swap_rndx_out PARAMS ((int bigend, RNDXR *,
					 struct rndx_ext *));

#endif /* ! defined (ECOFF_H) */
