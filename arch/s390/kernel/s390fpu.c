/*
 *  arch/s390/kernel/s390fpu.c
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 *
 *  s390fpu.h functions for saving & restoring the fpu state.
 *
 *  I couldn't inline these as linux/sched.h included half the world
 *  & was required to at the task structure.
 *  & the functions were too complex to make macros from.
 *  ( & as usual I didn't feel like debugging inline code ).
 */

#include <linux/config.h>
#include <linux/sched.h>

int save_fp_regs1(s390_fp_regs *fpregs)
{
	int has_ieee=MACHINE_HAS_IEEE;
/*
  I don't think we can use STE here as this would load
  fp registers 0 & 2 into memory locations 0 & 1 etc. 
 */	
	asm volatile ("STD   0,8(%0)\n\t"
		      "STD   2,24(%0)\n\t"
		      "STD   4,40(%0)\n\t"
		      "STD   6,56(%0)"
                      : 
		      : "a" (fpregs)
		      : "memory"
		);
	if(has_ieee)
	{
		asm volatile ("STFPC 0(%0)\n\t"
			      "STD   1,16(%0)\n\t"
			      "STD   3,32(%0)\n\t"
			      "STD   5,48(%0)\n\t"
			      "STD   7,64(%0)\n\t"
			      "STD   8,72(%0)\n\t"
			      "STD   9,80(%0)\n\t"
			      "STD   10,88(%0)\n\t"
			      "STD   11,96(%0)\n\t"
			      "STD   12,104(%0)\n\t"
			      "STD   13,112(%0)\n\t"
			      "STD   14,120(%0)\n\t"
			      "STD   15,128(%0)\n\t"
			      : 
			      : "a" (fpregs)
			      : "memory"
			);
	}
	return(has_ieee);
}


void save_fp_regs(s390_fp_regs *fpregs)
{
#if CONFIG_MATHEMU
	s390_fp_regs *currentfprs;

	if(!save_fp_regs1(fpregs))
	{
		currentfprs=&current->thread.fp_regs;
		fpregs->fpc=currentfprs->fpc;
		fpregs->fprs[1].d=currentfprs->fprs[1].d;
		fpregs->fprs[3].d=currentfprs->fprs[3].d;
		fpregs->fprs[5].d=currentfprs->fprs[5].d;
		fpregs->fprs[7].d=currentfprs->fprs[7].d;
		memcpy(&fpregs->fprs[8].d,&currentfprs->fprs[8].d,sizeof(freg_t)*8);
	}
#else
	save_fp_regs1(fpregs);
#endif
}


int restore_fp_regs1(s390_fp_regs *fpregs)
{
	int has_ieee=MACHINE_HAS_IEEE;

	/* If we don't mask with the FPC_VALID_MASK here
	 * we've got a very quick shutdown -h now command
         * via a kernel specification exception.
	 */
	fpregs->fpc&=FPC_VALID_MASK;
	asm volatile ("LD   0,8(%0)\n\t"
		      "LD   2,24(%0)\n\t"
		      "LD   4,40(%0)\n\t"
		      "LD   6,56(%0)"
                      : 
		      : "a" (fpregs)
		      : "memory"
		);
	if(has_ieee)
	{
		asm volatile ("LFPC 0(%0)\n\t"
			      "LD   1,16(%0)\n\t"
			      "LD   3,32(%0)\n\t"
			      "LD   5,48(%0)\n\t"
			      "LD   7,64(%0)\n\t"
			      "LD   8,72(%0)\n\t"
			      "LD   9,80(%0)\n\t"
			      "LD   10,88(%0)\n\t"
			      "LD   11,96(%0)\n\t"
			      "LD   12,104(%0)\n\t"
			      "LD   13,112(%0)\n\t"
			      "LD   14,120(%0)\n\t"
			      "LD   15,128(%0)\n\t"
			      : 
			      : "a" (fpregs)
			      : "memory"
			);
	}
	return(has_ieee);
}

void restore_fp_regs(s390_fp_regs *fpregs)
{
#if CONFIG_MATHEMU
	s390_fp_regs *currentfprs;

	if(!restore_fp_regs1(fpregs))
	{
		currentfprs=&current->thread.fp_regs;
		currentfprs->fpc=fpregs->fpc;
		currentfprs->fprs[1].d=fpregs->fprs[1].d;
		currentfprs->fprs[3].d=fpregs->fprs[3].d;
		currentfprs->fprs[5].d=fpregs->fprs[5].d;
		currentfprs->fprs[7].d=fpregs->fprs[7].d;
		memcpy(&currentfprs->fprs[8].d,&fpregs->fprs[8].d,sizeof(freg_t)*8);
	}
#else
	restore_fp_regs1(fpregs);
#endif
}

