/*
 *
 *    $FreeBSD$
 *
 */

#ifndef _MATH_EMU_H
#define _MATH_EMU_H

struct fpu_reg {
	char    sign;
	char    tag;
	long    exp;
	u_long  sigl;
	u_long  sigh;
};

union i387_union {
	struct i387_hard_struct {
		long    cwd;
		long    swd;
		long    twd;
		long    fip;
		long    fcs;
		long    foo;
		long    fos;
		long    st_space[20];	/* 8*10 bytes for each FP-reg = 80
					 * bytes */
	}       hard;
	struct i387_soft_struct {
		long    cwd;
		long    swd;
		long    twd;
		long    fip;
		long    fcs;
		long    foo;
		long    fos;
		long    top;
		struct fpu_reg regs[8];	/* 8*16 bytes for each FP-reg = 128
					 * bytes */
		unsigned char lookahead;
		struct trapframe *frame;
		unsigned long entry_eip;
		int     orig_eip;
	}       soft;
};
#endif
