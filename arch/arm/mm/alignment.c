/*
 *  linux/arch/arm/mm/alignment.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Modifications for ARM processor (c) 1995-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include <linux/compiler.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/bitops.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/unaligned.h>

extern void
do_bad_area(struct task_struct *tsk, struct mm_struct *mm, unsigned long addr,
	    int error_code, struct pt_regs *regs);

/*
 * 32-bit misaligned trap handler (c) 1998 San Mehat (CCC) -July 1998
 * /proc/sys/debug/alignment, modified and integrated into
 * Linux 2.1 by Russell King
 *
 * Speed optimisations and better fault handling by Russell King.
 *
 * *** NOTE ***
 * This code is not portable to processors with late data abort handling.
 */
#define CODING_BITS(i)	(i & 0x0e000000)

#define LDST_I_BIT(i)	(i & (1 << 26))		/* Immediate constant	*/
#define LDST_P_BIT(i)	(i & (1 << 24))		/* Preindex		*/
#define LDST_U_BIT(i)	(i & (1 << 23))		/* Add offset		*/
#define LDST_W_BIT(i)	(i & (1 << 21))		/* Writeback		*/
#define LDST_L_BIT(i)	(i & (1 << 20))		/* Load			*/

#define LDST_P_EQ_U(i)	((((i) ^ ((i) >> 1)) & (1 << 23)) == 0)

#define LDSTH_I_BIT(i)	(i & (1 << 22))		/* half-word immed	*/
#define LDM_S_BIT(i)	(i & (1 << 22))		/* write CPSR from SPSR	*/

#define RN_BITS(i)	((i >> 16) & 15)	/* Rn			*/
#define RD_BITS(i)	((i >> 12) & 15)	/* Rd			*/
#define RM_BITS(i)	(i & 15)		/* Rm			*/

#define REGMASK_BITS(i)	(i & 0xffff)
#define OFFSET_BITS(i)	(i & 0x0fff)

#define IS_SHIFT(i)	(i & 0x0ff0)
#define SHIFT_BITS(i)	((i >> 7) & 0x1f)
#define SHIFT_TYPE(i)	(i & 0x60)
#define SHIFT_LSL	0x00
#define SHIFT_LSR	0x20
#define SHIFT_ASR	0x40
#define SHIFT_RORRRX	0x60

static unsigned long ai_user;
static unsigned long ai_sys;
static unsigned long ai_skipped;
static unsigned long ai_half;
static unsigned long ai_word;
static unsigned long ai_multi;
static int ai_usermode;

#ifdef CONFIG_PROC_FS
static const char *usermode_action[] = {
	"ignored",
	"warn",
	"fixup",
	"fixup+warn",
	"signal",
	"signal+warn"
};

static int
proc_alignment_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{
	char *p = page;
	int len;

	p += sprintf(p, "User:\t\t%lu\n", ai_user);
	p += sprintf(p, "System:\t\t%lu\n", ai_sys);
	p += sprintf(p, "Skipped:\t%lu\n", ai_skipped);
	p += sprintf(p, "Half:\t\t%lu\n", ai_half);
	p += sprintf(p, "Word:\t\t%lu\n", ai_word);
	p += sprintf(p, "Multi:\t\t%lu\n", ai_multi);
	p += sprintf(p, "User faults:\t%i (%s)\n", ai_usermode,
			usermode_action[ai_usermode]);

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int proc_alignment_write(struct file *file, const char *buffer,
			       unsigned long count, void *data)
{
	int mode;

	if (count > 0) {
		if (get_user(mode, buffer))
			return -EFAULT;
		if (mode >= '0' && mode <= '5')
			   ai_usermode = mode - '0';
	}
	return count;
}

/*
 * This needs to be done after sysctl_init, otherwise sys/ will be
 * overwritten.  Actually, this shouldn't be in sys/ at all since
 * it isn't a sysctl, and it doesn't contain sysctl information.
 * We now locate it in /proc/cpu/alignment instead.
 */
static int __init alignment_init(void)
{
	struct proc_dir_entry *res;

	res = proc_mkdir("cpu", NULL);
	if (!res)
		return -ENOMEM;

	res = create_proc_entry("alignment", S_IWUSR | S_IRUGO, res);
	if (!res)
		return -ENOMEM;

	res->read_proc = proc_alignment_read;
	res->write_proc = proc_alignment_write;

	return 0;
}

__initcall(alignment_init);
#endif /* CONFIG_PROC_FS */

union offset_union {
	unsigned long un;
	  signed long sn;
};

#define TYPE_ERROR	0
#define TYPE_FAULT	1
#define TYPE_LDST	2
#define TYPE_DONE	3

#define __get8_unaligned_check(ins,val,addr,err)	\
	__asm__(					\
	"1:	"ins"	%1, [%2], #1\n"			\
	"2:\n"						\
	"	.section .fixup,\"ax\"\n"		\
	"	.align	2\n"				\
	"3:	mov	%0, #1\n"			\
	"	b	2b\n"				\
	"	.previous\n"				\
	"	.section __ex_table,\"a\"\n"		\
	"	.align	3\n"				\
	"	.long	1b, 3b\n"			\
	"	.previous\n"				\
	: "=r" (err), "=&r" (val), "=r" (addr)		\
	: "0" (err), "2" (addr))

#define __get16_unaligned_check(ins,val,addr)			\
	do {							\
		unsigned int err = 0, v, a = addr;		\
		__get8_unaligned_check(ins,val,a,err);		\
		__get8_unaligned_check(ins,v,a,err);		\
		val |= v << 8;					\
		if (err)					\
			goto fault;				\
	} while (0)

#define get16_unaligned_check(val,addr) \
	__get16_unaligned_check("ldrb",val,addr)

#define get16t_unaligned_check(val,addr) \
	__get16_unaligned_check("ldrbt",val,addr)

#define __get32_unaligned_check(ins,val,addr)			\
	do {							\
		unsigned int err = 0, v, a = addr;		\
		__get8_unaligned_check(ins,val,a,err);		\
		__get8_unaligned_check(ins,v,a,err);		\
		val |= v << 8;					\
		__get8_unaligned_check(ins,v,a,err);		\
		val |= v << 16;					\
		__get8_unaligned_check(ins,v,a,err);		\
		val |= v << 24;					\
		if (err)					\
			goto fault;				\
	} while (0)

#define get32_unaligned_check(val,addr) \
	__get32_unaligned_check("ldrb",val,addr)

#define get32t_unaligned_check(val,addr) \
	__get32_unaligned_check("ldrbt",val,addr)

#define __put16_unaligned_check(ins,val,addr)			\
	do {							\
		unsigned int err = 0, v = val, a = addr;	\
		__asm__(					\
		"1:	"ins"	%1, [%2], #1\n"			\
		"	mov	%1, %1, lsr #8\n"		\
		"2:	"ins"	%1, [%2]\n"			\
		"3:\n"						\
		"	.section .fixup,\"ax\"\n"		\
		"	.align	2\n"				\
		"4:	mov	%0, #1\n"			\
		"	b	3b\n"				\
		"	.previous\n"				\
		"	.section __ex_table,\"a\"\n"		\
		"	.align	3\n"				\
		"	.long	1b, 4b\n"			\
		"	.long	2b, 4b\n"			\
		"	.previous\n"				\
		: "=r" (err), "=&r" (v), "=&r" (a)		\
		: "0" (err), "1" (v), "2" (a));			\
		if (err)					\
			goto fault;				\
	} while (0)

#define put16_unaligned_check(val,addr)  \
	__put16_unaligned_check("strb",val,addr)

#define put16t_unaligned_check(val,addr) \
	__put16_unaligned_check("strbt",val,addr)

#define __put32_unaligned_check(ins,val,addr)			\
	do {							\
		unsigned int err = 0, v = val, a = addr;	\
		__asm__(					\
		"1:	"ins"	%1, [%2], #1\n"			\
		"	mov	%1, %1, lsr #8\n"		\
		"2:	"ins"	%1, [%2], #1\n"			\
		"	mov	%1, %1, lsr #8\n"		\
		"3:	"ins"	%1, [%2], #1\n"			\
		"	mov	%1, %1, lsr #8\n"		\
		"4:	"ins"	%1, [%2]\n"			\
		"5:\n"						\
		"	.section .fixup,\"ax\"\n"		\
		"	.align	2\n"				\
		"6:	mov	%0, #1\n"			\
		"	b	5b\n"				\
		"	.previous\n"				\
		"	.section __ex_table,\"a\"\n"		\
		"	.align	3\n"				\
		"	.long	1b, 6b\n"			\
		"	.long	2b, 6b\n"			\
		"	.long	3b, 6b\n"			\
		"	.long	4b, 6b\n"			\
		"	.previous\n"				\
		: "=r" (err), "=&r" (v), "=&r" (a)		\
		: "0" (err), "1" (v), "2" (a));			\
		if (err)					\
			goto fault;				\
	} while (0)

#define put32_unaligned_check(val,addr)	 \
	__put32_unaligned_check("strb", val, addr)

#define put32t_unaligned_check(val,addr) \
	__put32_unaligned_check("strbt", val, addr)

static void
do_alignment_finish_ldst(unsigned long addr, unsigned long instr, struct pt_regs *regs, union offset_union offset)
{
	if (!LDST_U_BIT(instr))
		offset.un = -offset.un;

	if (!LDST_P_BIT(instr))
		addr += offset.un;

	if (!LDST_P_BIT(instr) || LDST_W_BIT(instr))
		regs->uregs[RN_BITS(instr)] = addr;
}

static int
do_alignment_ldrhstrh(unsigned long addr, unsigned long instr, struct pt_regs *regs)
{
	unsigned int rd = RD_BITS(instr);

	if ((instr & 0x01f00ff0) == 0x01000090)
		goto swp;

	if ((instr & 0x90) != 0x90 || (instr & 0x60) == 0)
		goto bad;

	ai_half += 1;

	if (user_mode(regs))
		goto user;

	if (LDST_L_BIT(instr)) {
		unsigned long val;
		get16_unaligned_check(val, addr);

		/* signed half-word? */
		if (instr & 0x40)
			val = (signed long)((signed short) val);

		regs->uregs[rd] = val;
	} else
		put16_unaligned_check(regs->uregs[rd], addr);

	return TYPE_LDST;

 user:
 	if (LDST_L_BIT(instr)) {
 		unsigned long val;
 		get16t_unaligned_check(val, addr);

 		/* signed half-word? */
 		if (instr & 0x40)
 			val = (signed long)((signed short) val);

 		regs->uregs[rd] = val;
 	} else
 		put16t_unaligned_check(regs->uregs[rd], addr);

 	return TYPE_LDST;

 swp:
	printk(KERN_ERR "Alignment trap: not handling swp instruction\n");
 bad:
	return TYPE_ERROR;

 fault:
	return TYPE_FAULT;
}

static int
do_alignment_ldrstr(unsigned long addr, unsigned long instr, struct pt_regs *regs)
{
	unsigned int rd = RD_BITS(instr);

	ai_word += 1;

	if ((!LDST_P_BIT(instr) && LDST_W_BIT(instr)) || user_mode(regs))
		goto trans;

	if (LDST_L_BIT(instr)) {
		unsigned int val;
		get32_unaligned_check(val, addr);
		regs->uregs[rd] = val;
	} else
		put32_unaligned_check(regs->uregs[rd], addr);
	return TYPE_LDST;

 trans:
	if (LDST_L_BIT(instr)) {
		unsigned int val;
		get32t_unaligned_check(val, addr);
		regs->uregs[rd] = val;
	} else
		put32t_unaligned_check(regs->uregs[rd], addr);
	return TYPE_LDST;

 fault:
	return TYPE_FAULT;
}

/*
 * LDM/STM alignment handler.
 *
 * There are 4 variants of this instruction:
 *
 * B = rn pointer before instruction, A = rn pointer after instruction
 *              ------ increasing address ----->
 *	        |    | r0 | r1 | ... | rx |    |
 * PU = 01             B                    A
 * PU = 11        B                    A
 * PU = 00        A                    B
 * PU = 10             A                    B
 */
static int
do_alignment_ldmstm(unsigned long addr, unsigned long instr, struct pt_regs *regs)
{
	unsigned int rd, rn, correction, nr_regs, regbits;
	unsigned long eaddr, newaddr;

	if (LDM_S_BIT(instr))
		goto bad;

	correction = 4; /* processor implementation defined */
	regs->ARM_pc += correction;

	ai_multi += 1;

	/* count the number of registers in the mask to be transferred */
	nr_regs = hweight16(REGMASK_BITS(instr)) * 4;

	rn = RN_BITS(instr);
	newaddr = eaddr = regs->uregs[rn];

	if (!LDST_U_BIT(instr))
		nr_regs = -nr_regs;
	newaddr += nr_regs;
	if (!LDST_U_BIT(instr))
		eaddr = newaddr;

	if (LDST_P_EQ_U(instr))	/* U = P */
		eaddr += 4;

	/* 
	 * For alignment faults on the ARM922T/ARM920T the MMU  makes
	 * the FSR (and hence addr) equal to the updated base address
	 * of the multiple access rather than the restored value.
	 * Switch this messsage off if we've got a ARM92[02], otherwise
	 * [ls]dm alignment faults are noisy!
	 */
#if !(defined CONFIG_CPU_ARM922T)  && !(defined CONFIG_CPU_ARM920T)
	/*
	 * This is a "hint" - we already have eaddr worked out by the
	 * processor for us.
	 */
	if (addr != eaddr) {
		printk(KERN_ERR "LDMSTM: PC = %08lx, instr = %08lx, "
			"addr = %08lx, eaddr = %08lx\n",
			 instruction_pointer(regs), instr, addr, eaddr);
		show_regs(regs);
	}
#endif

	if (user_mode(regs)) {
		for (regbits = REGMASK_BITS(instr), rd = 0; regbits;
		     regbits >>= 1, rd += 1)
			if (regbits & 1) {
				if (LDST_L_BIT(instr)) {
					unsigned int val;
					get32t_unaligned_check(val, eaddr);
					regs->uregs[rd] = val;
				} else
					put32t_unaligned_check(regs->uregs[rd], eaddr);
				eaddr += 4;
			}
	} else {
		for (regbits = REGMASK_BITS(instr), rd = 0; regbits;
		     regbits >>= 1, rd += 1)
			if (regbits & 1) {
				if (LDST_L_BIT(instr)) {
					unsigned int val;
					get32_unaligned_check(val, eaddr);
					regs->uregs[rd] = val;
				} else
					put32_unaligned_check(regs->uregs[rd], eaddr);
				eaddr += 4;
			}
	}

	if (LDST_W_BIT(instr))
		regs->uregs[rn] = newaddr;
	if (!LDST_L_BIT(instr) || !(REGMASK_BITS(instr) & (1 << 15)))
		regs->ARM_pc -= correction;
	return TYPE_DONE;

fault:
	regs->ARM_pc -= correction;
	return TYPE_FAULT;

bad:
	printk(KERN_ERR "Alignment trap: not handling ldm with s-bit set\n");
	return TYPE_ERROR;
}

int do_alignment(unsigned long addr, int error_code, struct pt_regs *regs)
{
	union offset_union offset;
	unsigned long instr, instrptr;
	int (*handler)(unsigned long addr, unsigned long instr, struct pt_regs *regs);
	unsigned int type;

	instrptr = instruction_pointer(regs);
	instr = *(unsigned long *)instrptr;

	if (user_mode(regs))
		goto user;

	ai_sys += 1;

 fixup:

	regs->ARM_pc += 4;

	switch (CODING_BITS(instr)) {
	case 0x00000000:	/* ldrh or strh */
		if (LDSTH_I_BIT(instr))
			offset.un = (instr & 0xf00) >> 4 | (instr & 15);
		else
			offset.un = regs->uregs[RM_BITS(instr)];
		handler = do_alignment_ldrhstrh;
		break;

	case 0x04000000:	/* ldr or str immediate */
		offset.un = OFFSET_BITS(instr);
		handler = do_alignment_ldrstr;
		break;

	case 0x06000000:	/* ldr or str register */
		offset.un = regs->uregs[RM_BITS(instr)];

		if (IS_SHIFT(instr)) {
			unsigned int shiftval = SHIFT_BITS(instr);

			switch(SHIFT_TYPE(instr)) {
			case SHIFT_LSL:
				offset.un <<= shiftval;
				break;

			case SHIFT_LSR:
				offset.un >>= shiftval;
				break;

			case SHIFT_ASR:
				offset.sn >>= shiftval;
				break;

			case SHIFT_RORRRX:
				if (shiftval == 0) {
					offset.un >>= 1;
					if (regs->ARM_cpsr & CC_C_BIT)
						offset.un |= 1 << 31;
				} else
					offset.un = offset.un >> shiftval |
							  offset.un << (32 - shiftval);
				break;
			}
		}
		handler = do_alignment_ldrstr;
		break;

	case 0x08000000:	/* ldm or stm */
		handler = do_alignment_ldmstm;
		break;

	default:
		goto bad;
	}

	type = handler(addr, instr, regs);

	if (type == TYPE_ERROR || type == TYPE_FAULT)
		goto bad_or_fault;

	if (type == TYPE_LDST)
		do_alignment_finish_ldst(addr, instr, regs, offset);

	return 0;

 bad_or_fault:
	if (type == TYPE_ERROR)
		goto bad;
	regs->ARM_pc -= 4;
	/*
	 * We got a fault - fix it up, or die.
	 */
	do_bad_area(current, current->mm, addr, error_code, regs);
	return 0;

 bad:
	/*
	 * Oops, we didn't handle the instruction.
	 */
	printk(KERN_ERR "Alignment trap: not handling instruction "
		"%08lx at [<%08lx>]\n", instr, instrptr);
	ai_skipped += 1;
	return 1;

 user:
	ai_user += 1;

	if (ai_usermode & 1)
		printk("Alignment trap: %s (%d) PC=0x%08lx Instr=0x%08lx "
		       "Address=0x%08lx Code 0x%02x\n", current->comm,
			current->pid, instrptr, instr, addr, error_code);

	if (ai_usermode & 2)
		goto fixup;

	if (ai_usermode & 4)
		force_sig(SIGBUS, current);
	else
		set_cr(cr_no_alignment);

	return 0;
}
