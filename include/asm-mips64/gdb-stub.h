/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 Andreas Busse
 */
#ifndef __ASM_MIPS_GDB_STUB_H
#define __ASM_MIPS_GDB_STUB_H


/*
 * important register numbers
 */

#define REG_EPC			37
#define REG_FP			72
#define REG_SP			29

/*
 * Stack layout for the GDB exception handler
 * Derived from the stack layout described in asm-mips/stackframe.h
 *
 * The first PTRSIZE*6 bytes are argument save space for C subroutines.
 */
#define NUMREGS			90

#define GDB_FR_REG0		(PTRSIZE*6)			/* 0 */
#define GDB_FR_REG1		((GDB_FR_REG0) + 8)		/* 1 */
#define GDB_FR_REG2		((GDB_FR_REG1) + 8)		/* 2 */
#define GDB_FR_REG3		((GDB_FR_REG2) + 8)		/* 3 */
#define GDB_FR_REG4		((GDB_FR_REG3) + 8)		/* 4 */
#define GDB_FR_REG5		((GDB_FR_REG4) + 8)		/* 5 */
#define GDB_FR_REG6		((GDB_FR_REG5) + 8)		/* 6 */
#define GDB_FR_REG7		((GDB_FR_REG6) + 8)		/* 7 */
#define GDB_FR_REG8		((GDB_FR_REG7) + 8)		/* 8 */
#define GDB_FR_REG9	        ((GDB_FR_REG8) + 8)		/* 9 */
#define GDB_FR_REG10		((GDB_FR_REG9) + 8)		/* 10 */
#define GDB_FR_REG11		((GDB_FR_REG10) + 8)		/* 11 */
#define GDB_FR_REG12		((GDB_FR_REG11) + 8)		/* 12 */
#define GDB_FR_REG13		((GDB_FR_REG12) + 8)		/* 13 */
#define GDB_FR_REG14		((GDB_FR_REG13) + 8)		/* 14 */
#define GDB_FR_REG15		((GDB_FR_REG14) + 8)		/* 15 */
#define GDB_FR_REG16		((GDB_FR_REG15) + 8)		/* 16 */
#define GDB_FR_REG17		((GDB_FR_REG16) + 8)		/* 17 */
#define GDB_FR_REG18		((GDB_FR_REG17) + 8)		/* 18 */
#define GDB_FR_REG19		((GDB_FR_REG18) + 8)		/* 19 */
#define GDB_FR_REG20		((GDB_FR_REG19) + 8)		/* 20 */
#define GDB_FR_REG21		((GDB_FR_REG20) + 8)		/* 21 */
#define GDB_FR_REG22		((GDB_FR_REG21) + 8)		/* 22 */
#define GDB_FR_REG23		((GDB_FR_REG22) + 8)		/* 23 */
#define GDB_FR_REG24		((GDB_FR_REG23) + 8)		/* 24 */
#define GDB_FR_REG25		((GDB_FR_REG24) + 8)		/* 25 */
#define GDB_FR_REG26		((GDB_FR_REG25) + 8)		/* 26 */
#define GDB_FR_REG27		((GDB_FR_REG26) + 8)		/* 27 */
#define GDB_FR_REG28		((GDB_FR_REG27) + 8)		/* 28 */
#define GDB_FR_REG29		((GDB_FR_REG28) + 8)		/* 29 */
#define GDB_FR_REG30		((GDB_FR_REG29) + 8)		/* 30 */
#define GDB_FR_REG31		((GDB_FR_REG30) + 8)		/* 31 */

/*
 * Saved special registers
 */
#define GDB_FR_STATUS		((GDB_FR_REG31) + 8)		/* 32 */
#define GDB_FR_LO		((GDB_FR_STATUS) + 8)		/* 33 */
#define GDB_FR_HI		((GDB_FR_LO) + 8)		/* 34 */
#define GDB_FR_BADVADDR		((GDB_FR_HI) + 8)		/* 35 */
#define GDB_FR_CAUSE		((GDB_FR_BADVADDR) + 8)		/* 36 */
#define GDB_FR_EPC		((GDB_FR_CAUSE) + 8)		/* 37 */

/*
 * Saved floating point registers
 */
#define GDB_FR_FPR0		((GDB_FR_EPC) + 8)		/* 38 */
#define GDB_FR_FPR1		((GDB_FR_FPR0) + 8)		/* 39 */
#define GDB_FR_FPR2		((GDB_FR_FPR1) + 8)		/* 40 */
#define GDB_FR_FPR3		((GDB_FR_FPR2) + 8)		/* 41 */
#define GDB_FR_FPR4		((GDB_FR_FPR3) + 8)		/* 42 */
#define GDB_FR_FPR5		((GDB_FR_FPR4) + 8)		/* 43 */
#define GDB_FR_FPR6		((GDB_FR_FPR5) + 8)		/* 44 */
#define GDB_FR_FPR7		((GDB_FR_FPR6) + 8)		/* 45 */
#define GDB_FR_FPR8		((GDB_FR_FPR7) + 8)		/* 46 */
#define GDB_FR_FPR9		((GDB_FR_FPR8) + 8)		/* 47 */
#define GDB_FR_FPR10		((GDB_FR_FPR9) + 8)		/* 48 */
#define GDB_FR_FPR11		((GDB_FR_FPR10) + 8)		/* 49 */
#define GDB_FR_FPR12		((GDB_FR_FPR11) + 8)		/* 50 */
#define GDB_FR_FPR13		((GDB_FR_FPR12) + 8)		/* 51 */
#define GDB_FR_FPR14		((GDB_FR_FPR13) + 8)		/* 52 */
#define GDB_FR_FPR15		((GDB_FR_FPR14) + 8)		/* 53 */
#define GDB_FR_FPR16		((GDB_FR_FPR15) + 8)		/* 54 */
#define GDB_FR_FPR17		((GDB_FR_FPR16) + 8)		/* 55 */
#define GDB_FR_FPR18		((GDB_FR_FPR17) + 8)		/* 56 */
#define GDB_FR_FPR19		((GDB_FR_FPR18) + 8)		/* 57 */
#define GDB_FR_FPR20		((GDB_FR_FPR19) + 8)		/* 58 */
#define GDB_FR_FPR21		((GDB_FR_FPR20) + 8)		/* 59 */
#define GDB_FR_FPR22		((GDB_FR_FPR21) + 8)		/* 60 */
#define GDB_FR_FPR23		((GDB_FR_FPR22) + 8)		/* 61 */
#define GDB_FR_FPR24		((GDB_FR_FPR23) + 8)		/* 62 */
#define GDB_FR_FPR25		((GDB_FR_FPR24) + 8)		/* 63 */
#define GDB_FR_FPR26		((GDB_FR_FPR25) + 8)		/* 64 */
#define GDB_FR_FPR27		((GDB_FR_FPR26) + 8)		/* 65 */
#define GDB_FR_FPR28		((GDB_FR_FPR27) + 8)		/* 66 */
#define GDB_FR_FPR29		((GDB_FR_FPR28) + 8)		/* 67 */
#define GDB_FR_FPR30		((GDB_FR_FPR29) + 8)		/* 68 */
#define GDB_FR_FPR31		((GDB_FR_FPR30) + 8)		/* 69 */

#define GDB_FR_FSR		((GDB_FR_FPR31) + 8)		/* 70 */
#define GDB_FR_FIR		((GDB_FR_FSR) + 8)		/* 71 */
#define GDB_FR_FRP		((GDB_FR_FIR) + 8)		/* 72 */

#define GDB_FR_DUMMY		((GDB_FR_FRP) + 8)		/* 73, unused ??? */

/*
 * Again, CP0 registers
 */
#define GDB_FR_CP0_INDEX	((GDB_FR_DUMMY) + 8)		/* 74 */
#define GDB_FR_CP0_RANDOM	((GDB_FR_CP0_INDEX) + 8)	/* 75 */
#define GDB_FR_CP0_ENTRYLO0	((GDB_FR_CP0_RANDOM) + 8)	/* 76 */
#define GDB_FR_CP0_ENTRYLO1	((GDB_FR_CP0_ENTRYLO0) + 8)	/* 77 */
#define GDB_FR_CP0_CONTEXT	((GDB_FR_CP0_ENTRYLO1) + 8)	/* 78 */
#define GDB_FR_CP0_PAGEMASK	((GDB_FR_CP0_CONTEXT) + 8)	/* 79 */
#define GDB_FR_CP0_WIRED	((GDB_FR_CP0_PAGEMASK) + 8)	/* 80 */
#define GDB_FR_CP0_REG7		((GDB_FR_CP0_WIRED) + 8)	/* 81 */
#define GDB_FR_CP0_REG8		((GDB_FR_CP0_REG7) + 8)		/* 82 */
#define GDB_FR_CP0_REG9		((GDB_FR_CP0_REG8) + 8)		/* 83 */
#define GDB_FR_CP0_ENTRYHI	((GDB_FR_CP0_REG9) + 8)		/* 84 */
#define GDB_FR_CP0_REG11	((GDB_FR_CP0_ENTRYHI) + 8)	/* 85 */
#define GDB_FR_CP0_REG12	((GDB_FR_CP0_REG11) + 8)	/* 86 */
#define GDB_FR_CP0_REG13	((GDB_FR_CP0_REG12) + 8)	/* 87 */
#define GDB_FR_CP0_REG14	((GDB_FR_CP0_REG13) + 8)	/* 88 */
#define GDB_FR_CP0_PRID		((GDB_FR_CP0_REG14) + 8)	/* 89 */

#define GDB_FR_SIZE		((((GDB_FR_CP0_PRID) + 8) + (PTRSIZE-1)) & ~(PTRSIZE-1))

#ifndef __ASSEMBLY__

/*
 * This is the same as above, but for the high-level
 * part of the GDB stub.
 */

struct gdb_regs {
	/*
	 * Pad bytes for argument save space on the stack
	 * 24/48 Bytes for 32/64 bit code
	 */
	unsigned long pad0[6];

	/*
	 * saved main processor registers
	 */
	long	 reg0,  reg1,  reg2,  reg3,  reg4,  reg5,  reg6,  reg7;
	long	 reg8,  reg9, reg10, reg11, reg12, reg13, reg14, reg15;
	long	reg16, reg17, reg18, reg19, reg20, reg21, reg22, reg23;
	long	reg24, reg25, reg26, reg27, reg28, reg29, reg30, reg31;

	/*
	 * Saved special registers
	 */
	long	cp0_status;
	long	lo;
	long	hi;
	long	cp0_badvaddr;
	long	cp0_cause;
	long	cp0_epc;

	/*
	 * Saved floating point registers
	 */
	long	fpr0,  fpr1,  fpr2,  fpr3,  fpr4,  fpr5,  fpr6,  fpr7;
	long	fpr8,  fpr9,  fpr10, fpr11, fpr12, fpr13, fpr14, fpr15;
	long	fpr16, fpr17, fpr18, fpr19, fpr20, fpr21, fpr22, fpr23;
	long	fpr24, fpr25, fpr26, fpr27, fpr28, fpr29, fpr30, fpr31;

	long	cp1_fsr;
	long	cp1_fir;

	/*
	 * Frame pointer
	 */
	long	frame_ptr;
	long    dummy;		/* unused */

	/*
	 * saved cp0 registers
	 */
	long	cp0_index;
	long	cp0_random;
	long	cp0_entrylo0;
	long	cp0_entrylo1;
	long	cp0_context;
	long	cp0_pagemask;
	long	cp0_wired;
	long	cp0_reg7;
	long	cp0_reg8;
	long	cp0_reg9;
	long	cp0_entryhi;
	long	cp0_reg11;
	long	cp0_reg12;
	long	cp0_reg13;
	long	cp0_reg14;
	long	cp0_prid;
};

/*
 * Prototypes
 */

void set_debug_traps(void);
void set_async_breakpoint(unsigned long *epc);

#endif /* !__ASSEMBLY__ */
#endif /* __ASM_MIPS_GDB_STUB_H */
