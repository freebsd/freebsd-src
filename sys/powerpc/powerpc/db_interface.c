/*	$FreeBSD$ */
/*	$NetBSD: db_interface.c,v 1.20 2002/05/13 20:30:09 matt Exp $ */
/*	$OpenBSD: db_interface.c,v 1.2 1996/12/28 06:21:50 rahnds Exp $	*/

#define USERACC

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/cons.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/trap.h>
#ifdef PPC_IBM4XX
#include <machine/tlb.h>
#include <powerpc/spr.h>
#include <uvm/uvm_extern.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>
#endif

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <dev/ofw/openfirm.h>

int	db_active = 0;

db_regs_t ddb_regs;

void ddb_trap(void);				/* Call into trap_subr.S */
int ddb_trap_glue(struct trapframe *);		/* Called from trap_subr.S */
#ifdef PPC_IBM4XX
static void db_ppc4xx_ctx(db_expr_t, int, db_expr_t, char *);
static void db_ppc4xx_pv(db_expr_t, int, db_expr_t, char *);
static void db_ppc4xx_reset(db_expr_t, int, db_expr_t, char *);
static void db_ppc4xx_tf(db_expr_t, int, db_expr_t, char *);
static void db_ppc4xx_dumptlb(db_expr_t, int, db_expr_t, char *);
#ifdef USERACC
static void db_ppc4xx_useracc(db_expr_t, int, db_expr_t, char *);
#endif
#endif /* PPC_IBM4XX */

#ifdef DDB
void
Debugger(const char *msg)
{
	db_printf("Welcome to Debugger, %s\n", msg);
	ddb_trap();
}
#endif

int
ddb_trap_glue(frame)
	struct trapframe *frame;
{
	if (!(frame->srr1 & PSL_PR)
	    && (frame->exc == EXC_TRC || frame->exc == EXC_RUNMODETRC
		|| (frame->exc == EXC_PGM
		    && (frame->srr1 & 0x20000))
		|| frame->exc == EXC_BPT)) {
		int type = frame->exc;
		if (type == EXC_PGM && (frame->srr1 & 0x20000)) {
			type = T_BREAKPOINT;
		}
		return kdb_trap(type, frame);
	}
	return 0;
}

int
kdb_trap(type, v)
	int type;
	void *v;
{
	struct trapframe *frame = v;

#if 0
	switch (type) {
	case T_BREAKPOINT:
	case -1:
		break;
	default:
		if (!db_onpanic && db_recover == 0)
			return 0;
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}
#endif

	/* XXX Should switch to kdb's own stack here. */

	memcpy(DDB_REGS->r, frame->fixreg, 32 * sizeof(u_int32_t));
	DDB_REGS->iar = frame->srr0;
	DDB_REGS->msr = frame->srr1;
	DDB_REGS->lr = frame->lr;
	DDB_REGS->ctr = frame->ctr;
	DDB_REGS->cr = frame->cr;
	DDB_REGS->xer = frame->xer;
#ifdef PPC_IBM4XX
	DDB_REGS->dear = frame->dear;
	DDB_REGS->esr = frame->esr;
	DDB_REGS->pid = frame->pid;
#endif

#ifdef DDB
	db_active++;
	cndbctl(1);
	db_trap(type, 0);
	cndbctl(0);
	db_active--;
#elif defined(KGDB)
	if (!kgdb_trap(type, DDB_REGS))
		return 0;
#endif

	/* KGDB isn't smart about advancing PC if we
	 * take a breakpoint trap after kgdb_active is set.
	 * Therefore, we help out here.
	 */
	if (IS_BREAKPOINT_TRAP(type, 0)) {
		int bkpt;
		db_read_bytes(PC_REGS(DDB_REGS),BKPT_SIZE,(void *)&bkpt);
		if (bkpt== BKPT_INST) {
			PC_REGS(DDB_REGS) += BKPT_SIZE;
		}
	}

	memcpy(frame->fixreg, DDB_REGS->r, 32 * sizeof(u_int32_t));
	frame->srr0 = DDB_REGS->iar;
	frame->srr1 = DDB_REGS->msr;
	frame->lr = DDB_REGS->lr;
	frame->ctr = DDB_REGS->ctr;
	frame->cr = DDB_REGS->cr;
	frame->xer = DDB_REGS->xer;
#ifdef PPC_IBM4XX
	frame->dear = DDB_REGS->dear;
	frame->esr = DDB_REGS->esr;
	frame->pid = DDB_REGS->pid;
#endif

	return 1;
}

#ifdef PPC_IBM4XX
const struct db_command db_machine_command_table[] = {
	{ "ctx",	db_ppc4xx_ctx,		0,	0 },
	{ "pv",		db_ppc4xx_pv,		0,	0 },
	{ "reset",	db_ppc4xx_reset,	0,	0 },
	{ "tf",		db_ppc4xx_tf,	0,	0 },
	{ "tlb",	db_ppc4xx_dumptlb,	0,	0 },
#ifdef USERACC
	{ "user",	db_ppc4xx_useracc,	0,	0 },
#endif
	{ NULL, }
};

static void
db_ppc4xx_ctx(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	struct proc *p;

	/* XXX LOCKING XXX */
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if (p->p_stat) {
			db_printf("process %p:", p);
			db_printf("pid:%d pmap:%p ctx:%d %s\n",
				p->p_pid, p->p_vmspace->vm_map.pmap,
				p->p_vmspace->vm_map.pmap->pm_ctx,
				p->p_comm);
		}
	}
	return;
}

static void
db_ppc4xx_pv(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	struct pv_entry {
		struct pv_entry *pv_next;	/* Linked list of mappings */
		vaddr_t pv_va;			/* virtual address of mapping */
		struct pmap *pv_pm;
	};
	struct pv_entry *pa_to_pv(paddr_t);
	struct pv_entry *pv;

	if (!have_addr) {
		db_printf("pv: <pa>\n");
		return;
	}
	pv = pa_to_pv(addr);
	db_printf("pv at %p\n", pv);
	while (pv && pv->pv_pm) {
		db_printf("next %p va %p pmap %p\n", pv->pv_next, 
			(void *)pv->pv_va, pv->pv_pm);
		pv = pv->pv_next;
	}
}

static void
db_ppc4xx_reset(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	printf("Reseting...\n");
	ppc4xx_reset();
}

static void
db_ppc4xx_tf(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	struct trapframe *f;


	if (have_addr) {
		f = (struct trapframe *)addr;

		db_printf("r0-r3:  \t%8.8x %8.8x %8.8x %8.8x\n", 
			f->fixreg[0], f->fixreg[1],
			f->fixreg[2], f->fixreg[3]);
		db_printf("r4-r7:  \t%8.8x %8.8x %8.8x %8.8x\n",
			f->fixreg[4], f->fixreg[5],
			f->fixreg[6], f->fixreg[7]);
		db_printf("r8-r11: \t%8.8x %8.8x %8.8x %8.8x\n",
			f->fixreg[8], f->fixreg[9],
			f->fixreg[10], f->fixreg[11]);
		db_printf("r12-r15:\t%8.8x %8.8x %8.8x %8.8x\n",
			f->fixreg[12], f->fixreg[13],
			f->fixreg[14], f->fixreg[15]);
		db_printf("r16-r19:\t%8.8x %8.8x %8.8x %8.8x\n",
			f->fixreg[16], f->fixreg[17],
			f->fixreg[18], f->fixreg[19]);
		db_printf("r20-r23:\t%8.8x %8.8x %8.8x %8.8x\n",
			f->fixreg[20], f->fixreg[21],
			f->fixreg[22], f->fixreg[23]);
		db_printf("r24-r27:\t%8.8x %8.8x %8.8x %8.8x\n",
			f->fixreg[24], f->fixreg[25],
			f->fixreg[26], f->fixreg[27]);
		db_printf("r28-r31:\t%8.8x %8.8x %8.8x %8.8x\n",
			f->fixreg[28], f->fixreg[29],
			f->fixreg[30], f->fixreg[31]);

		db_printf("lr: %8.8x cr: %8.8x xer: %8.8x ctr: %8.8x\n",
			f->lr, f->cr, f->xer, f->ctr);
		db_printf("srr0(pc): %8.8x srr1(msr): %8.8x "
			"dear: %8.8x esr: %8.8x\n",
			f->srr0, f->srr1, f->dear, f->esr);
		db_printf("exc: %8.8x pid: %8.8x\n",
			f->exc, f->pid);
	}
	return;
}

static const char *const tlbsizes[] = {
	  "1kB",
	  "4kB",
	 "16kB",
	 "64kB",
	"256kB",
	  "1MB",
	  "4MB",
	 "16MB"
};

static void
db_ppc4xx_dumptlb(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int i, zone, tlbsize;
	u_int zpr, pid, opid, msr;
	u_long tlblo, tlbhi, tlbmask;

	zpr = mfspr(SPR_ZPR);
	for (i = 0; i < NTLB; i++) {
		asm volatile("mfmsr %3;"
			"mfpid %4;"
			"li %0,0;"
			"mtmsr %0;"
			"sync; isync;"
			"tlbre %0,%5,1;"
			"tlbre %1,%5,0;"
			"mfpid %2;"
			"mtpid %4;"
			"mtmsr %3;"
			"sync; isync"
			: "=&r" (tlblo), "=&r" (tlbhi), "=r" (pid), 
			"=&r" (msr), "=&r" (opid) : "r" (i));

		if (strchr(modif, 'v') && !(tlbhi & TLB_VALID))
			continue;

		tlbsize = (tlbhi & TLB_SIZE_MASK) >> TLB_SIZE_SHFT;
		/* map tlbsize 0 .. 7 to masks for 1kB .. 16MB */
		tlbmask = ~(1 << (tlbsize * 2 + 10)) + 1;

		if (have_addr && ((tlbhi & tlbmask) != (addr & tlbmask)))
			continue;

		zone = (tlblo & TLB_ZSEL_MASK) >> TLB_ZSEL_SHFT;
		db_printf("tlb%c%2d", tlbhi & TLB_VALID ? ' ' : '*', i);
		db_printf("  PID %3d EPN 0x%08lx %-5s",
		    pid,
		    tlbhi & tlbmask,
		    tlbsizes[tlbsize]);
		db_printf("  RPN 0x%08lx  ZONE %2d%c  %s %s %c%c%c%c%c %s",
		    tlblo & tlbmask,
		    zone,
		    "NTTA"[(zpr >> ((15 - zone) * 2)) & 3],
		    tlblo & TLB_EX ? "EX" : "  ",
		    tlblo & TLB_WR ? "WR" : "  ",
		    tlblo & TLB_W ? 'W' : ' ',
		    tlblo & TLB_I ? 'I' : ' ',
		    tlblo & TLB_M ? 'M' : ' ',
		    tlblo & TLB_G ? 'G' : ' ',
		    tlbhi & TLB_ENDIAN ? 'E' : ' ',
		    tlbhi & TLB_U0 ? "U0" : "  ");
		db_printf("\n");
	}
}

#ifdef USERACC
static void
db_ppc4xx_useracc(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	static paddr_t oldaddr = -1;
	int instr = 0;
	int data;
	extern vaddr_t opc_disasm(vaddr_t loc, int);


	if (!have_addr) {
		addr = oldaddr;
	}
	if (addr == -1) {
		db_printf("no address\n");
		return;
	}
	addr &= ~0x3; /* align */
	{
		register char c, *cp = modif;
		while ((c = *cp++) != 0)
			if (c == 'i')
				instr = 1;
	}
	while (count--) {
		if (db_print_position() == 0) {
			/* Always print the address. */
			db_printf("%8.4lx:\t", addr);
		}
		oldaddr=addr;
		copyin((void *)addr, &data, sizeof(data));
		if (instr) {
			opc_disasm(addr, data);
		} else {
			db_printf("%4.4x\n", data);
		}
		addr += 4;
		db_end_line();
	}

}
#endif

#endif /* PPC_IBM4XX */

void
db_show_mdpcpu(struct pcpu *pc)
{
}
