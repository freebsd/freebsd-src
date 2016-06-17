/*    $Id: unaligned.c,v 1.10 2002/09/22 02:21:05 tausq Exp $
 *
 *    Unaligned memory access handler
 *
 *    Copyright (C) 2001 Randolph Chung <tausq@debian.org>
 *    Significantly tweaked by LaMont Jones <lamont@debian.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/atomic.h>

#include <asm/smp.h>
#include <asm/pdc.h>

/* #define DEBUG_UNALIGNED 1 */

#ifdef DEBUG_UNALIGNED
#define DPRINTF(fmt, args...) do { printk(KERN_DEBUG "%s:%d:%s ", __FILE__, __LINE__, __FUNCTION__ ); printk(KERN_DEBUG fmt, ##args ); } while (0)
#else
#define DPRINTF(fmt, args...)
#endif

#ifdef __LP64__
#define RFMT "%016lx"
#else
#define RFMT "%08lx"
#endif

/* 1111 1100 0000 0000 0001 0011 1100 0000 */
#define OPCODE1(a,b,c)	((a)<<26|(b)<<12|(c)<<6) 
#define OPCODE2(a,b)	((a)<<26|(b)<<1)
#define OPCODE3(a,b)	((a)<<26|(b)<<2)
#define OPCODE4(a)	((a)<<26)
#define OPCODE1_MASK	OPCODE1(0x3f,1,0xf)
#define OPCODE2_MASK 	OPCODE2(0x3f,1)
#define OPCODE3_MASK	OPCODE3(0x3f,1)
#define OPCODE4_MASK    OPCODE4(0x3f)

/* skip LDB - never unaligned (index) */
#define OPCODE_LDH_I	OPCODE1(0x03,0,0x1)
#define OPCODE_LDW_I	OPCODE1(0x03,0,0x2)
#define OPCODE_LDD_I	OPCODE1(0x03,0,0x3)
#define OPCODE_LDDA_I	OPCODE1(0x03,0,0x4)
#define OPCODE_LDCD_I	OPCODE1(0x03,0,0x5)
#define OPCODE_LDWA_I	OPCODE1(0x03,0,0x6)
#define OPCODE_LDCW_I	OPCODE1(0x03,0,0x7)
/* skip LDB - never unaligned (short) */
#define OPCODE_LDH_S	OPCODE1(0x03,1,0x1)
#define OPCODE_LDW_S	OPCODE1(0x03,1,0x2)
#define OPCODE_LDD_S	OPCODE1(0x03,1,0x3)
#define OPCODE_LDDA_S	OPCODE1(0x03,1,0x4)
#define OPCODE_LDCD_S	OPCODE1(0x03,1,0x5)
#define OPCODE_LDWA_S	OPCODE1(0x03,1,0x6)
#define OPCODE_LDCW_S	OPCODE1(0x03,1,0x7)
/* skip STB - never unaligned */
#define OPCODE_STH	OPCODE1(0x03,1,0x9)
#define OPCODE_STW	OPCODE1(0x03,1,0xa)
#define OPCODE_STD	OPCODE1(0x03,1,0xb)
/* skip STBY - never unaligned */
/* skip STDBY - never unaligned */
#define OPCODE_STWA	OPCODE1(0x03,1,0xe)
#define OPCODE_STDA	OPCODE1(0x03,1,0xf)

#define OPCODE_LDD_L	OPCODE2(0x14,0)
#define OPCODE_FLDD_L	OPCODE2(0x14,1)
#define OPCODE_STD_L	OPCODE2(0x1c,0)
#define OPCODE_FSTD_L	OPCODE2(0x1c,1)

#define OPCODE_LDW_M	OPCODE3(0x17,1)
#define OPCODE_FLDW_L	OPCODE3(0x17,0)
#define OPCODE_FSTW_L	OPCODE3(0x1f,0)
#define OPCODE_STW_M	OPCODE3(0x1f,1)

#define OPCODE_LDH_L    OPCODE4(0x11)
#define OPCODE_LDW_L    OPCODE4(0x12)
#define OPCODE_LDWM     OPCODE4(0x13)
#define OPCODE_STH_L    OPCODE4(0x19)
#define OPCODE_STW_L    OPCODE4(0x1A)
#define OPCODE_STWM     OPCODE4(0x1B)

#define MAJOR_OP(i) (((i)>>26)&0x3f)
#define R1(i) (((i)>>21)&0x1f)
#define R2(i) (((i)>>16)&0x1f)
#define R3(i) ((i)&0x1f)
#define IM(i,n) (((i)>>1&((1<<(n-1))-1))|((i)&1?((0-1L)<<(n-1)):0))
#define IM5_2(i) IM((i)>>16,5)
#define IM5_3(i) IM((i),5)
#define IM14(i) IM((i),14)

int unaligned_enabled = 1;

void die_if_kernel (char *str, struct pt_regs *regs, long err);

static int emulate_ldh(struct pt_regs *regs, int toreg)
{
	unsigned long saddr = regs->ior;
	unsigned long val = 0;

	DPRINTF("load " RFMT ":" RFMT " to r%d for 2 bytes\n", 
		regs->isr, regs->ior, toreg);

	__asm__ __volatile__  (
"	mtsp	%3, %%sr1\n"
"	ldbs	0(%%sr1,%2), %%r20\n"
"	ldbs	1(%%sr1,%2), %0\n"
	"depw	%%r20, 23, 24, %0\n"
	: "=r" (val)
	: "0" (val), "r" (saddr), "r" (regs->isr)
	: "r20" );

	DPRINTF("val = 0x" RFMT "\n", val);

	if (toreg)
		regs->gr[toreg] = val;

	return 0;
}
static int emulate_ldw(struct pt_regs *regs, int toreg)
{
	unsigned long saddr = regs->ior;
	unsigned long val = 0;

	DPRINTF("load " RFMT ":" RFMT " to r%d for 4 bytes\n", 
		regs->isr, regs->ior, toreg);

	__asm__ __volatile__  (
"	zdep	%2,28,2,%%r19\n"		/* r19=(ofs&3)*8 */
"	mtsp	%3, %%sr1\n"
"	depw	%%r0,31,2,%2\n"
"	ldw	0(%%sr1,%2),%0\n"
"	ldw	4(%%sr1,%2),%%r20\n"
"	subi	32,%%r19,%%r19\n"
"	mtctl	%%r19,11\n"
"	vshd	%0,%%r20,%0\n"
	: "=r" (val)
	: "0" (val), "r" (saddr), "r" (regs->isr)
	: "r19", "r20" );

	DPRINTF("val = 0x" RFMT "\n", val);

	if (toreg)
		regs->gr[toreg] = val;

	return 0;
}
#ifdef __LP64__
static int emulate_ldd(struct pt_regs *regs, int toreg)
{
	unsigned long saddr = regs->ior;
	unsigned long val = 0;

	DPRINTF("load " RFMT ":" RFMT " to r%d for 8 bytes\n", 
		regs->isr, regs->ior, toreg);

	__asm__ __volatile__  (
"	depd,z	%2,60,3,%%r19\n"		/* r19=(ofs&7)*8 */
"	mtsp	%3, %%sr1\n"
"	depd	%%r0,63,3,%2\n"
"	ldd	0(%%sr1,%2),%0\n"
"	ldd	8(%%sr1,%2),%%r20\n"
"	subi	64,%%r19,%%r19\n"
"	mtsar	%%r19\n"
"	shrpd	%0,%%r20,%%sar,%0\n"
	: "=r" (val)
	: "0" (val), "r" (saddr), "r" (regs->isr)
	: "r19", "r20" );

	DPRINTF("val = 0x" RFMT "\n", val);

	if (toreg)
		regs->gr[toreg] = val;

	return 0;
}
#endif

static int emulate_sth(struct pt_regs *regs, int frreg)
{
	unsigned long val = regs->gr[frreg];
	if (!frreg)
		val = 0;

	DPRINTF("store r%d (0x" RFMT ") to " RFMT ":" RFMT " for 2 bytes\n", frreg, 
		regs->gr[frreg], regs->isr, regs->ior);

	__asm__ __volatile__ (
"	mtsp %2, %%sr1\n"
"	extrw,u %0, 23, 8, %%r19\n"
"	stb %0, 1(%%sr1, %1)\n"
"	stb %%r19, 0(%%sr1, %1)\n"
	:
	: "r" (val), "r" (regs->ior), "r" (regs->isr)
	: "r19" );

	return 0;
}
static int emulate_stw(struct pt_regs *regs, int frreg)
{
	unsigned long val = regs->gr[frreg];
	if (!frreg)
		val = 0;

	DPRINTF("store r%d (0x" RFMT ") to " RFMT ":" RFMT " for 4 bytes\n", frreg, 
		regs->gr[frreg], regs->isr, regs->ior);


	__asm__ __volatile__ (
"	mtsp %2, %%sr1\n"
"	zdep	%1, 28, 2, %%r19\n"
"	dep	%%r0, 31, 2, %1\n"
"	mtsar	%%r19\n"
"	depwi,z	-2, %%sar, 32, %%r19\n"
"	ldw	0(%%sr1,%1),%%r20\n"
"	ldw	4(%%sr1,%1),%%r21\n"
"	vshd	%%r0, %0, %%r22\n"
"	vshd	%0, %%r0, %%r1\n"
"	and	%%r20, %%r19, %%r20\n"
"	andcm	%%r21, %%r19, %%r21\n"
"	or	%%r22, %%r20, %%r20\n"
"	or	%%r1, %%r21, %%r21\n"
"	stw	%%r20,0(%%sr1,%1)\n"
"	stw	%%r21,4(%%sr1,%1)\n"
	:
	: "r" (val), "r" (regs->ior), "r" (regs->isr)
	: "r19", "r20", "r21", "r22", "r1" );

	return 0;
}
#ifdef __LP64__
static int emulate_std(struct pt_regs *regs, int frreg)
{
	unsigned long val = regs->gr[frreg];
	if (!frreg)
		val = 0;

	DPRINTF("store r%d (0x" RFMT ") to " RFMT ":" RFMT " for 8 bytes\n", frreg, 
		regs->gr[frreg], regs->isr, regs->ior);


	__asm__ __volatile__ (
"	mtsp %2, %%sr1\n"
"	depd,z	%1, 60, 3, %%r19\n"
"	depd	%%r0, 63, 3, %1\n"
"	mtsar	%%r19\n"
"	depdi,z	-2, %%sar, 64, %%r19\n"
"	ldd	0(%%sr1,%1),%%r20\n"
"	ldd	8(%%sr1,%1),%%r21\n"
"	shrpd	%%r0, %0, %%sar, %%r22\n"
"	shrpd	%0, %%r0, %%sar, %%r1\n"
"	and	%%r20, %%r19, %%r20\n"
"	andcm	%%r21, %%r19, %%r21\n"
"	or	%%r22, %%r20, %%r20\n"
"	or	%%r1, %%r21, %%r21\n"
"	std	%%r20,0(%%sr1,%1)\n"
"	std	%%r21,8(%%sr1,%1)\n"
	:
	: "r" (val), "r" (regs->ior), "r" (regs->isr)
	: "r19", "r20", "r21", "r22", "r1" );

	return 0;
}
#endif

void handle_unaligned(struct pt_regs *regs)
{
	unsigned long unaligned_count = 0;
	unsigned long last_time = 0;
	unsigned long newbase = regs->gr[R1(regs->iir)];
	int modify = 0;
	int ret = -1;
	struct siginfo si;

	/* if the unaligned access is inside the kernel:
	 *   if the access is caused by a syscall, then we fault the calling
	 *     user process
	 *   otherwise we halt the kernel
	 */
	if (!user_mode(regs))
	{
		const struct exception_table_entry *fix;

		/* see if the offending code have its own
		 * exception handler 
		 */ 

		fix = search_exception_table(regs->iaoq[0]);
		if (fix)
		{
			/* lower bits of fix->skip are flags
			 * upper bits are the handler addr
			 */
			if (fix->skip & 1)
				regs->gr[8] = -EFAULT;
			if (fix->skip & 2)
				regs->gr[9] = 0;

			regs->iaoq[0] += ((fix->skip) & ~3);
			regs->iaoq[1] = regs->iaoq[0] + 4;
			regs->gr[0] &= ~PSW_B;

			return;
		}
	}

	/* log a message with pacing */
	if (user_mode(regs))
	{
		if (unaligned_count > 5 && jiffies - last_time > 5*HZ)
		{
			unaligned_count = 0;
			last_time = jiffies;
		}
		if (++unaligned_count < 5)
		{
			char buf[256];
			sprintf(buf, "%s(%d): unaligned access to 0x" RFMT " at ip=0x" RFMT "\n",
				current->comm, current->pid, regs->ior, regs->iaoq[0]);
			printk(KERN_WARNING "%s", buf);
#ifdef DEBUG_UNALIGNED
			show_regs(regs);
#endif		
		}
	}

	if (!unaligned_enabled)
		goto force_sigbus;

	/* handle modification - OK, it's ugly, see the instruction manual */
	switch (MAJOR_OP(regs->iir))
	{
	case 0x03:
	case 0x09:
	case 0x0b:
		if (regs->iir&0x20)
		{
			modify = 1;
			if (regs->iir&0x1000)		/* short loads */
				if (regs->iir&0x200)
					newbase += IM5_3(regs->iir);
				else
					newbase += IM5_2(regs->iir);
			else if (regs->iir&0x2000)	/* scaled indexed */
			{
				int shift=0;
				switch (regs->iir & OPCODE1_MASK)
				{
				case OPCODE_LDH_I:
					shift= 1; break;
				case OPCODE_LDW_I:
					shift= 2; break;
				case OPCODE_LDD_I:
				case OPCODE_LDDA_I:
					shift= 3; break;
				}
				newbase += (R2(regs->iir)?regs->gr[R2(regs->iir)]:0)<<shift;
			} else				/* simple indexed */
				newbase += (R2(regs->iir)?regs->gr[R2(regs->iir)]:0);
		}
		break;
	case 0x13:
	case 0x1b:
		modify = 1;
		newbase += IM14(regs->iir);
		break;
	case 0x14:
	case 0x1c:
		if (regs->iir&8)
		{
			modify = 1;
			newbase += IM14(regs->iir&~0xe);
		}
		break;
	case 0x16:
	case 0x1e:
		modify = 1;
		newbase += IM14(regs->iir&6);
		break;
	case 0x17:
	case 0x1f:
		if (regs->iir&4)
		{
			modify = 1;
			newbase += IM14(regs->iir&~4);
		}
		break;
	}

	if (regs->isr != regs->sr[7])
	{
		printk(KERN_CRIT "isr verification failed (isr: " RFMT ", sr7: " RFMT "\n",
			regs->isr, regs->sr[7]);

		/* don't kill him though, since he has appropriate access to the page, or we
		 * would never have gotten here.
		 */
	}

	/* TODO: make this cleaner... */
	switch (regs->iir & OPCODE1_MASK)
	{
	case OPCODE_LDH_I:
	case OPCODE_LDH_S:
		ret = emulate_ldh(regs, R3(regs->iir));
		break;

	case OPCODE_LDW_I:
	case OPCODE_LDWA_I:
	case OPCODE_LDW_S:
	case OPCODE_LDWA_S:
		ret = emulate_ldw(regs, R3(regs->iir));
		break;

	case OPCODE_STH:
		ret = emulate_sth(regs, R2(regs->iir));
		break;

	case OPCODE_STW:
	case OPCODE_STWA:
		ret = emulate_stw(regs, R2(regs->iir));
		break;

#ifdef __LP64__
	case OPCODE_LDD_I:
	case OPCODE_LDDA_I:
	case OPCODE_LDD_S:
	case OPCODE_LDDA_S:
		ret = emulate_ldd(regs, R3(regs->iir));
		break;

	case OPCODE_STD:
	case OPCODE_STDA:
		ret = emulate_std(regs, R2(regs->iir));
		break;
#endif

	case OPCODE_LDCD_I:
	case OPCODE_LDCW_I:
	case OPCODE_LDCD_S:
	case OPCODE_LDCW_S:
		ret = -1;	/* "undefined", but lets kill them. */
		break;
	}
#ifdef __LP64__
	switch (regs->iir & OPCODE2_MASK)
	{
	case OPCODE_LDD_L:
	case OPCODE_FLDD_L:
		ret = emulate_ldd(regs, R2(regs->iir));
		break;

	case OPCODE_STD_L:
	case OPCODE_FSTD_L:
		ret = emulate_std(regs, R2(regs->iir));
		break;
	}
#endif
	switch (regs->iir & OPCODE3_MASK)
	{
	case OPCODE_LDW_M:
	case OPCODE_FLDW_L:
		ret = emulate_ldw(regs, R2(regs->iir));
		break;

	case OPCODE_FSTW_L:
	case OPCODE_STW_M:
		ret = emulate_stw(regs, R2(regs->iir));
		break;
	}
	switch (regs->iir & OPCODE4_MASK)
	{
	case OPCODE_LDH_L:
		ret = emulate_ldh(regs, R2(regs->iir));
		break;
	case OPCODE_LDW_L:
	case OPCODE_LDWM:
		ret = emulate_ldw(regs, R2(regs->iir));
		break;
	case OPCODE_STH_L:
		ret = emulate_sth(regs, R2(regs->iir));
		break;
	case OPCODE_STW_L:
	case OPCODE_STWM:
		ret = emulate_stw(regs, R2(regs->iir));
		break;
	}
	/* XXX LJ - need to handle float load/store */

	if (modify && R1(regs->iir))
		regs->gr[R1(regs->iir)] = newbase;


	if (ret < 0)
		printk(KERN_CRIT "Not-handled unaligned insn 0x%08lx\n", regs->iir);

	DPRINTF("ret = %d\n", ret);

	if (ret)
	{
		printk(KERN_CRIT "Unaligned handler failed, ret = %d\n", ret);
		die_if_kernel("Unaligned data reference", regs, 28);
force_sigbus:
		/* couldn't handle it ... */
		si.si_signo = SIGBUS;
		si.si_errno = 0;
		si.si_code = BUS_ADRALN;
		si.si_addr = (void *)regs->ior;
		force_sig_info(SIGBUS, &si, current);
		
		return;
	}

	/* else we handled it, advance the PC.... */
	regs->iaoq[0] = regs->iaoq[1];
	regs->iaoq[1] = regs->iaoq[0] + 4;
}

/*
 * NB: check_unaligned() is only used for PCXS processors right
 * now, so we only check for PA1.1 encodings at this point.
 */

int
check_unaligned(struct pt_regs *regs)
{
	unsigned long align_mask;

	/* Get alignment mask */

	align_mask = 0UL;
	switch (regs->iir & OPCODE1_MASK) {

	case OPCODE_LDH_I:
	case OPCODE_LDH_S:
	case OPCODE_STH:
		align_mask = 1UL;
		break;

	case OPCODE_LDW_I:
	case OPCODE_LDWA_I:
	case OPCODE_LDW_S:
	case OPCODE_LDWA_S:
	case OPCODE_STW:
	case OPCODE_STWA:
		align_mask = 3UL;
		break;

	default:
		switch (regs->iir & OPCODE4_MASK) {
		case OPCODE_LDH_L:
		case OPCODE_STH_L:
			align_mask = 1UL;
			break;
		case OPCODE_LDW_L:
		case OPCODE_LDWM:
		case OPCODE_STW_L:
		case OPCODE_STWM:
			align_mask = 3UL;
			break;
		}
		break;
	}

	return (int)(regs->ior & align_mask);
}

