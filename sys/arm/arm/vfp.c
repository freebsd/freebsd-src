/*
 * Copyright (c) 2012 Mark Tinguely
 *
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef VFP
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <machine/frame.h>
#include <machine/fp.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/vfp.h>

/* function prototypes */
unsigned int get_coprocessorACR(void);
int	vfp_bounce(u_int, u_int, struct trapframe *, int);
void	vfp_discard(void);
void	vfp_enable(void);
void	vfp_restore(struct vfp_state *);
void	vfp_store(struct vfp_state *);
void	set_coprocessorACR(u_int);

extern int vfp_exists;
static struct undefined_handler vfp10_uh, vfp11_uh;
/* If true the VFP unit has 32 double registers, otherwise it has 16 */
static int is_d32;

/* The VFMXR command using coprocessor commands */
#define fmxr(reg, val) \
    __asm __volatile("mcr p10, 7, %0, " __STRING(reg) " , c0, 0" :: "r"(val));

/* The VFMRX command using coprocessor commands */
#define fmrx(reg) \
({ u_int val = 0;\
    __asm __volatile("mrc p10, 7, %0, " __STRING(reg) " , c0, 0" : "=r"(val));\
    val; \
})

u_int
get_coprocessorACR(void)
{
	u_int val;
	__asm __volatile("mrc p15, 0, %0, c1, c0, 2" : "=r" (val) : : "cc");
	return val;
}

void
set_coprocessorACR(u_int val)
{
	__asm __volatile("mcr p15, 0, %0, c1, c0, 2\n\t"
	 : : "r" (val) : "cc");
	isb();
}


	/* called for each cpu */
void
vfp_init(void)
{
	u_int fpsid, fpexc, tmp;
	u_int coproc, vfp_arch;

	coproc = get_coprocessorACR();
	coproc |= COPROC10 | COPROC11;
	set_coprocessorACR(coproc);
	
	fpsid = fmrx(VFPSID);		/* read the vfp system id */
	fpexc = fmrx(VFPEXC);		/* read the vfp exception reg */

	if (!(fpsid & VFPSID_HARDSOFT_IMP)) {
		vfp_exists = 1;
		is_d32 = 0;
		PCPU_SET(vfpsid, fpsid);	/* save the VFPSID */

		vfp_arch =
		    (fpsid & VFPSID_SUBVERSION2_MASK) >> VFPSID_SUBVERSION_OFF;

		if (vfp_arch >= VFP_ARCH3) {
			tmp = fmrx(VMVFR0);
			PCPU_SET(vfpmvfr0, tmp);

			if ((tmp & VMVFR0_RB_MASK) == 2)
				is_d32 = 1;

			tmp = fmrx(VMVFR1);
			PCPU_SET(vfpmvfr1, tmp);
		}

		/* initialize the coprocess 10 and 11 calls
		 * These are called to restore the registers and enable
		 * the VFP hardware.
		 */
		if (vfp10_uh.uh_handler == NULL) {
			vfp10_uh.uh_handler = vfp_bounce;
			vfp11_uh.uh_handler = vfp_bounce;
			install_coproc_handler_static(10, &vfp10_uh);
			install_coproc_handler_static(11, &vfp11_uh);
		}
	}
}

SYSINIT(vfp, SI_SUB_CPU, SI_ORDER_ANY, vfp_init, NULL);


/* start VFP unit, restore the vfp registers from the PCB  and retry
 * the instruction
 */
int
vfp_bounce(u_int addr, u_int insn, struct trapframe *frame, int code)
{
	u_int fpexc;
	struct pcb *curpcb;
	struct thread *vfptd;

	if (!vfp_exists)
		return 1;		/* vfp does not exist */
	fpexc = fmrx(VFPEXC);		/* read the vfp exception reg */
	if (fpexc & VFPEXC_EN) {
		vfptd = PCPU_GET(vfpcthread);
		/* did the kernel call the vfp or exception that expect us
		 * to emulate the command. Newer hardware does not require
		 * emulation, so we don't emulate yet.
		 */
#ifdef SMP
		/* don't save if newer registers are on another processor */
		if (vfptd /* && (vfptd == curthread) */ &&
		   (vfptd->td_pcb->pcb_vfpcpu == PCPU_GET(cpu)))
#else
		/* someone did not save their registers, */
		if (vfptd /* && (vfptd == curthread) */)
#endif
			vfp_store(&vfptd->td_pcb->pcb_vfpstate);

		fpexc &= ~VFPEXC_EN;
		fmxr(VFPEXC, fpexc);	/* turn vfp hardware off */
		if (vfptd == curthread) {
			/* kill the process - we do not handle emulation */
			killproc(curthread->td_proc, "vfp emulation");
			return 1;
		}
		/* should not happen. someone did not save their context */
		printf("vfp_bounce: vfpcthread: %p curthread: %p\n",
			vfptd, curthread);
	}
	fpexc |= VFPEXC_EN;
	fmxr(VFPEXC, fpexc);	/* enable the vfp and repeat command */
	curpcb = PCPU_GET(curpcb);
	/* If we were the last process to use the VFP, the process did not
	 * use a VFP on another processor, then the registers in the VFP
	 * will still be ours and are current. Eventually, we will make the
	 * restore smarter.
	 */
	vfp_restore(&curpcb->pcb_vfpstate);
#ifdef SMP
	curpcb->pcb_vfpcpu = PCPU_GET(cpu);
#endif
	PCPU_SET(vfpcthread, PCPU_GET(curthread));
	return 0;
}

/* vfs_store is called from from a VFP command to restore the registers and
 * turn on the VFP hardware.
 * Eventually we will use the information that this process was the last
 * to use the VFP hardware and bypass the restore, just turn on the hardware.
 */
void
vfp_restore(struct vfp_state *vfpsave)
{
	u_int vfpscr = 0;

	/*
	 * Work around an issue with GCC where the asm it generates is
	 * not unified syntax and fails to assemble because it expects
	 * the ldcleq instruction in the form ldc<c>l, not in the UAL
	 * form ldcl<c>, and similar for stcleq.
	 */
#ifdef __clang__
#define	ldclne	"ldclne"
#define	stclne	"stclne"
#else
#define	ldclne	"ldcnel"
#define	stclne	"stcnel"
#endif
	if (vfpsave) {
		__asm __volatile("ldc	p10, c0, [%1], #128\n" /* d0-d15 */
			"cmp	%2, #0\n"		/* -D16 or -D32? */
			ldclne"	p11, c0, [%1], #128\n"	/* d16-d31 */
			"addeq	%1, %1, #128\n"		/* skip missing regs */
			"ldr	%0, [%1]\n"		/* set old vfpscr */
			"mcr	p10, 7, %0, cr1, c0, 0\n"
			: "=&r" (vfpscr) : "r" (vfpsave), "r" (is_d32) : "cc");
		PCPU_SET(vfpcthread, PCPU_GET(curthread));
	}
}

/* vfs_store is called from switch to save the vfp hardware registers
 * into the pcb before switching to another process.
 * we already know that the new process is different from this old
 * process and that this process last used the VFP registers.
 * Below we check to see if the VFP has been enabled since the last
 * register save.
 * This routine will exit with the VFP turned off. The next VFP user
 * will trap to restore its registers and turn on the VFP hardware.
 */
void
vfp_store(struct vfp_state *vfpsave)
{
	u_int tmp, vfpscr = 0;

	tmp = fmrx(VFPEXC);		/* Is the vfp enabled? */
	if (vfpsave && tmp & VFPEXC_EN) {
		__asm __volatile("stc	p11, c0, [%1], #128\n" /* d0-d15 */
			"cmp	%2, #0\n"		/* -D16 or -D32? */
			stclne"	p11, c0, [%1], #128\n"	/* d16-d31 */
			"addeq	%1, %1, #128\n"		/* skip missing regs */
			"mrc	p10, 7, %0, cr1, c0, 0\n" /* fmxr(VFPSCR) */
			"str	%0, [%1]\n"		/* save vfpscr */
			: "=&r" (vfpscr) : "r" (vfpsave), "r" (is_d32) : "cc");
	}
#undef ldcleq
#undef stcleq

#ifndef SMP
		/* eventually we will use this information for UP also */
	PCPU_SET(vfpcthread, 0);
#endif
	tmp &= ~VFPEXC_EN;	/* disable the vfp hardware */
	fmxr(VFPEXC , tmp);
}

/* discard the registers at cpu_thread_free() when fpcurthread == td.
 * Turn off the VFP hardware.
 */
void
vfp_discard()
{
	u_int tmp = 0;

	PCPU_SET(vfpcthread, 0);	/* permanent forget about reg */
	tmp = fmrx(VFPEXC);
	tmp &= ~VFPEXC_EN;		/* turn off VFP hardware */
	fmxr(VFPEXC, tmp);
}

/* Enable the VFP hardware without restoring registers.
 * Called when the registers are still in the VFP unit
 */
void
vfp_enable()
{
	u_int tmp = 0;

	tmp = fmrx(VFPEXC);
	tmp |= VFPEXC_EN;
	fmxr(VFPEXC, tmp);
}
#endif

