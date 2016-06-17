/*
 * Kernel support for the ptrace() and syscall tracing interfaces.
 *
 * Copyright (C) 1999-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * Derived from the x86 and Alpha versions.  Most of the code in here
 * could actually be factored into a common set of routines.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/smp_lock.h>
#include <linux/user.h>

#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/ptrace_offsets.h>
#include <asm/rse.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unwind.h>
#ifdef CONFIG_PERFMON
#include <asm/perfmon.h>
#endif

#define offsetof(type,field)    ((unsigned long) &((type *) 0)->field)

/*
 * Bits in the PSR that we allow ptrace() to change:
 *	be, up, ac, mfl, mfh (the user mask; five bits total)
 *	db (debug breakpoint fault; one bit)
 *	id (instruction debug fault disable; one bit)
 *	dd (data debug fault disable; one bit)
 *	ri (restart instruction; two bits)
 *	is (instruction set; one bit)
 */
#define IPSR_WRITE_MASK \
	(IA64_PSR_UM | IA64_PSR_DB | IA64_PSR_IS | IA64_PSR_ID | IA64_PSR_DD | IA64_PSR_RI)
#define IPSR_READ_MASK	IPSR_WRITE_MASK

#define PTRACE_DEBUG	1

#if PTRACE_DEBUG
# define dprintk(format...)	printk(format)
# define inline
#else
# define dprintk(format...)
#endif

/*
 * Collect the NaT bits for r1-r31 from scratch_unat and return a NaT
 * bitset where bit i is set iff the NaT bit of register i is set.
 */
unsigned long
ia64_get_scratch_nat_bits (struct pt_regs *pt, unsigned long scratch_unat)
{
#	define GET_BITS(first, last, unat)						\
	({										\
		unsigned long bit = ia64_unat_pos(&pt->r##first);			\
		unsigned long mask = ((1UL << (last - first + 1)) - 1) << first;	\
		unsigned long dist;							\
		if (bit < first)							\
			dist = 64 + bit - first;					\
		else									\
			dist = bit - first;						\
		ia64_rotr(unat, dist) & mask;						\
	})
	unsigned long val;

	/*
	 * Registers that are stored consecutively in struct pt_regs can be handled in
	 * parallel.  If the register order in struct_pt_regs changes, this code MUST be
	 * updated.
	 */
	val  = GET_BITS( 1,  1, scratch_unat);
	val |= GET_BITS( 2,  3, scratch_unat);
	val |= GET_BITS(12, 13, scratch_unat);
	val |= GET_BITS(14, 14, scratch_unat);
	val |= GET_BITS(15, 15, scratch_unat);
	val |= GET_BITS( 8, 11, scratch_unat);
	val |= GET_BITS(16, 31, scratch_unat);
	return val;

#	undef GET_BITS
}

/*
 * Set the NaT bits for the scratch registers according to NAT and
 * return the resulting unat (assuming the scratch registers are
 * stored in PT).
 */
unsigned long
ia64_put_scratch_nat_bits (struct pt_regs *pt, unsigned long nat)
{
#	define PUT_BITS(first, last, nat)						\
	({										\
		unsigned long bit = ia64_unat_pos(&pt->r##first);			\
		unsigned long mask = ((1UL << (last - first + 1)) - 1) << first;	\
		long dist;								\
		if (bit < first)							\
			dist = 64 + bit - first;					\
		else									\
			dist = bit - first;						\
		ia64_rotl(nat & mask, dist);						\
	})
	unsigned long scratch_unat;

	/*
	 * Registers that are stored consecutively in struct pt_regs can be handled in
	 * parallel.  If the register order in struct_pt_regs changes, this code MUST be
	 * updated.
	 */
	scratch_unat  = PUT_BITS( 1,  1, nat);
	scratch_unat |= PUT_BITS( 2,  3, nat);
	scratch_unat |= PUT_BITS(12, 13, nat);
	scratch_unat |= PUT_BITS(14, 14, nat);
	scratch_unat |= PUT_BITS(15, 15, nat);
	scratch_unat |= PUT_BITS( 8, 11, nat);
	scratch_unat |= PUT_BITS(16, 31, nat);

	return scratch_unat;

#	undef PUT_BITS
}

#define IA64_MLX_TEMPLATE	0x2
#define IA64_MOVL_OPCODE	6

void
ia64_increment_ip (struct pt_regs *regs)
{
	unsigned long w0, ri = ia64_psr(regs)->ri + 1;

	if (ri > 2) {
		ri = 0;
		regs->cr_iip += 16;
	} else if (ri == 2) {
		get_user(w0, (char *) regs->cr_iip + 0);
		if (((w0 >> 1) & 0xf) == IA64_MLX_TEMPLATE) {
			/*
			 * rfi'ing to slot 2 of an MLX bundle causes
			 * an illegal operation fault.  We don't want
			 * that to happen...
			 */
			ri = 0;
			regs->cr_iip += 16;
		}
	}
	ia64_psr(regs)->ri = ri;
}

void
ia64_decrement_ip (struct pt_regs *regs)
{
	unsigned long w0, ri = ia64_psr(regs)->ri - 1;

	if (ia64_psr(regs)->ri == 0) {
		regs->cr_iip -= 16;
		ri = 2;
		get_user(w0, (char *) regs->cr_iip + 0);
		if (((w0 >> 1) & 0xf) == IA64_MLX_TEMPLATE) {
			/*
			 * rfi'ing to slot 2 of an MLX bundle causes
			 * an illegal operation fault.  We don't want
			 * that to happen...
			 */
			ri = 1;
		}
	}
	ia64_psr(regs)->ri = ri;
}

/*
 * This routine is used to read an rnat bits that are stored on the kernel backing store.
 * Since, in general, the alignment of the user and kernel are different, this is not
 * completely trivial.  In essence, we need to construct the user RNAT based on up to two
 * kernel RNAT values and/or the RNAT value saved in the child's pt_regs.
 *
 * user rbs
 *
 * +--------+ <-- lowest address
 * | slot62 |
 * +--------+
 * |  rnat  | 0x....1f8
 * +--------+
 * | slot00 | \
 * +--------+ |
 * | slot01 | > child_regs->ar_rnat
 * +--------+ |
 * | slot02 | /				kernel rbs
 * +--------+				+--------+
 *	    <- child_regs->ar_bspstore	| slot61 | <-- krbs
 * +- - - - +				+--------+
 *					| slot62 |
 * +- - - - +				+--------+
 *					|  rnat	 |
 * +- - - - +				+--------+
 *   vrnat				| slot00 |
 * +- - - - +				+--------+
 *					=	 =
 *					+--------+
 *					| slot00 | \
 *					+--------+ |
 *					| slot01 | > child_stack->ar_rnat
 *					+--------+ |
 *					| slot02 | /
 *					+--------+
 *						  <--- child_stack->ar_bspstore
 *
 * The way to think of this code is as follows: bit 0 in the user rnat corresponds to some
 * bit N (0 <= N <= 62) in one of the kernel rnat value.  The kernel rnat value holding
 * this bit is stored in variable rnat0.  rnat1 is loaded with the kernel rnat value that
 * form the upper bits of the user rnat value.
 *
 * Boundary cases:
 *
 * o when reading the rnat "below" the first rnat slot on the kernel backing store,
 *   rnat0/rnat1 are set to 0 and the low order bits are merged in from pt->ar_rnat.
 *
 * o when reading the rnat "above" the last rnat slot on the kernel backing store,
 *   rnat0/rnat1 gets its value from sw->ar_rnat.
 */
static unsigned long
get_rnat (struct task_struct *task, struct switch_stack *sw,
	  unsigned long *krbs, unsigned long *urnat_addr, unsigned long *urbs_end)
{
	unsigned long rnat0 = 0, rnat1 = 0, urnat = 0, *slot0_kaddr, umask = 0, mask, m;
	unsigned long *kbsp, *ubspstore, *rnat0_kaddr, *rnat1_kaddr, shift;
	long num_regs, nbits;
	struct pt_regs *pt;

	pt = ia64_task_regs(task);
	kbsp = (unsigned long *) sw->ar_bspstore;
	ubspstore = (unsigned long *) pt->ar_bspstore;

	if (urbs_end < urnat_addr)
		nbits = ia64_rse_num_regs(urnat_addr - 63, urbs_end);
	else
		nbits = 63;
	mask = (1UL << nbits) - 1;
	/*
	 * First, figure out which bit number slot 0 in user-land maps to in the kernel
	 * rnat.  Do this by figuring out how many register slots we're beyond the user's
	 * backingstore and then computing the equivalent address in kernel space.
	 */
	num_regs = ia64_rse_num_regs(ubspstore, urnat_addr + 1);
	slot0_kaddr = ia64_rse_skip_regs(krbs, num_regs);
	shift = ia64_rse_slot_num(slot0_kaddr);
	rnat1_kaddr = ia64_rse_rnat_addr(slot0_kaddr);
	rnat0_kaddr = rnat1_kaddr - 64;

	if (ubspstore + 63 > urnat_addr) {
		/* some bits need to be merged in from pt->ar_rnat */
		umask = ((1UL << ia64_rse_slot_num(ubspstore)) - 1) & mask;
		urnat = (pt->ar_rnat & umask);
		mask &= ~umask;
		if (!mask)
			return urnat;
	}

	m = mask << shift;
	if (rnat0_kaddr >= kbsp)
		rnat0 = sw->ar_rnat;
	else if (rnat0_kaddr > krbs)
		rnat0 = *rnat0_kaddr;
	urnat |= (rnat0 & m) >> shift;

	m = mask >> (63 - shift);
	if (rnat1_kaddr >= kbsp)
		rnat1 = sw->ar_rnat;
	else if (rnat1_kaddr > krbs)
		rnat1 = *rnat1_kaddr;
	urnat |= (rnat1 & m) << (63 - shift);
	return urnat;
}

/*
 * The reverse of get_rnat.
 */
static void
put_rnat (struct task_struct *task, struct switch_stack *sw,
	  unsigned long *krbs, unsigned long *urnat_addr, unsigned long urnat,
	  unsigned long *urbs_end)
{
	unsigned long rnat0 = 0, rnat1 = 0, *slot0_kaddr, umask = 0, mask, m;
	unsigned long *kbsp, *ubspstore, *rnat0_kaddr, *rnat1_kaddr, shift;
	long num_regs, nbits;
	struct pt_regs *pt;
	unsigned long cfm, *urbs_kargs;
	struct unw_frame_info info;

	pt = ia64_task_regs(task);
	kbsp = (unsigned long *) sw->ar_bspstore;
	ubspstore = (unsigned long *) pt->ar_bspstore;

	urbs_kargs = urbs_end;
	if ((long)pt->cr_ifs >= 0) {
		/*
		 * If entered via syscall, don't allow user to set rnat bits
		 * for syscall args.
		 */
		unw_init_from_blocked_task(&info,task);
		if (unw_unwind_to_user(&info) == 0) {
			unw_get_cfm(&info,&cfm);
			urbs_kargs = ia64_rse_skip_regs(urbs_end,-(cfm & 0x7f));
		}
	}

	if (urbs_kargs >= urnat_addr)
		nbits = 63;
	else {
		if ((urnat_addr - 63) >= urbs_kargs)
			return;
		nbits = ia64_rse_num_regs(urnat_addr - 63, urbs_kargs);
	}
	mask = (1UL << nbits) - 1;

	/*
	 * First, figure out which bit number slot 0 in user-land maps to in the kernel
	 * rnat.  Do this by figuring out how many register slots we're beyond the user's
	 * backingstore and then computing the equivalent address in kernel space.
	 */
	num_regs = ia64_rse_num_regs(ubspstore, urnat_addr + 1);
	slot0_kaddr = ia64_rse_skip_regs(krbs, num_regs);
	shift = ia64_rse_slot_num(slot0_kaddr);
	rnat1_kaddr = ia64_rse_rnat_addr(slot0_kaddr);
	rnat0_kaddr = rnat1_kaddr - 64;

	if (ubspstore + 63 > urnat_addr) {
		/* some bits need to be place in pt->ar_rnat: */
		umask = ((1UL << ia64_rse_slot_num(ubspstore)) - 1) & mask;
		pt->ar_rnat = (pt->ar_rnat & ~umask) | (urnat & umask);
		mask &= ~umask;
		if (!mask)
			return;
	}
	/*
	 * Note: Section 11.1 of the EAS guarantees that bit 63 of an
	 * rnat slot is ignored. so we don't have to clear it here.
	 */
	rnat0 = (urnat << shift);
	m = mask << shift;
	if (rnat0_kaddr >= kbsp)
		sw->ar_rnat = (sw->ar_rnat & ~m) | (rnat0 & m);
	else if (rnat0_kaddr > krbs)
		*rnat0_kaddr = ((*rnat0_kaddr & ~m) | (rnat0 & m));

	rnat1 = (urnat >> (63 - shift));
	m = mask >> (63 - shift);
	if (rnat1_kaddr >= kbsp)
		sw->ar_rnat = (sw->ar_rnat & ~m) | (rnat1 & m);
	else if (rnat1_kaddr > krbs)
		*rnat1_kaddr = ((*rnat1_kaddr & ~m) | (rnat1 & m));
}

/*
 * Read a word from the user-level backing store of task CHILD.  ADDR is the user-level
 * address to read the word from, VAL a pointer to the return value, and USER_BSP gives
 * the end of the user-level backing store (i.e., it's the address that would be in ar.bsp
 * after the user executed a "cover" instruction).
 *
 * This routine takes care of accessing the kernel register backing store for those
 * registers that got spilled there.  It also takes care of calculating the appropriate
 * RNaT collection words.
 */
long
ia64_peek (struct task_struct *child, struct switch_stack *child_stack, unsigned long user_rbs_end,
	   unsigned long addr, long *val)
{
	unsigned long *bspstore, *krbs, regnum, *laddr, *urbs_end, *rnat_addr;
	struct pt_regs *child_regs;
	size_t copied;
	long ret;

	urbs_end = (long *) user_rbs_end;
	laddr = (unsigned long *) addr;
	child_regs = ia64_task_regs(child);
	bspstore = (unsigned long *) child_regs->ar_bspstore;
	krbs = (unsigned long *) child + IA64_RBS_OFFSET/8;
	if (laddr >= bspstore && laddr <= ia64_rse_rnat_addr(urbs_end)) {
		/*
		 * Attempt to read the RBS in an area that's actually on the kernel RBS =>
		 * read the corresponding bits in the kernel RBS.
		 */
		rnat_addr = ia64_rse_rnat_addr(laddr);
		ret = get_rnat(child, child_stack, krbs, rnat_addr, urbs_end);

		if (laddr == rnat_addr) {
			/* return NaT collection word itself */
			*val = ret;
			return 0;
		}

		if (((1UL << ia64_rse_slot_num(laddr)) & ret) != 0) {
			/*
			 * It is implementation dependent whether the data portion of a
			 * NaT value gets saved on a st8.spill or RSE spill (e.g., see
			 * EAS 2.6, 4.4.4.6 Register Spill and Fill).  To get consistent
			 * behavior across all possible IA-64 implementations, we return
			 * zero in this case.
			 */
			*val = 0;
			return 0;
		}

		if (laddr < urbs_end) {
			/* the desired word is on the kernel RBS and is not a NaT */
			regnum = ia64_rse_num_regs(bspstore, laddr);
			*val = *ia64_rse_skip_regs(krbs, regnum);
			return 0;
		}
	}
	copied = access_process_vm(child, addr, &ret, sizeof(ret), 0);
	if (copied != sizeof(ret))
		return -EIO;
	*val = ret;
	return 0;
}

long
ia64_poke (struct task_struct *child, struct switch_stack *child_stack, unsigned long user_rbs_end,
	   unsigned long addr, long val)
{
	unsigned long *bspstore, *krbs, regnum, *laddr, *urbs_end = (long *) user_rbs_end;
	struct pt_regs *child_regs;

	laddr = (unsigned long *) addr;
	child_regs = ia64_task_regs(child);
	bspstore = (unsigned long *) child_regs->ar_bspstore;
	krbs = (unsigned long *) child + IA64_RBS_OFFSET/8;
	if (laddr >= bspstore && laddr <= ia64_rse_rnat_addr(urbs_end)) {
		/*
		 * Attempt to write the RBS in an area that's actually on the kernel RBS
		 * => write the corresponding bits in the kernel RBS.
		 */
		if (ia64_rse_is_rnat_slot(laddr))
			put_rnat(child, child_stack, krbs, laddr, val, urbs_end);
		else {
			if (laddr < urbs_end) {
				regnum = ia64_rse_num_regs(bspstore, laddr);
				*ia64_rse_skip_regs(krbs, regnum) = val;
			}
		}
	} else if (access_process_vm(child, addr, &val, sizeof(val), 1) != sizeof(val)) {
		return -EIO;
	}
	return 0;
}

/*
 * Calculate the address of the end of the user-level register backing store.  This is the
 * address that would have been stored in ar.bsp if the user had executed a "cover"
 * instruction right before entering the kernel.  If CFMP is not NULL, it is used to
 * return the "current frame mask" that was active at the time the kernel was entered.
 */
unsigned long
ia64_get_user_rbs_end (struct task_struct *child, struct pt_regs *pt, unsigned long *cfmp)
{
	unsigned long *krbs, *bspstore, cfm;
	struct unw_frame_info info;
	long ndirty;

	krbs = (unsigned long *) child + IA64_RBS_OFFSET/8;
	bspstore = (unsigned long *) pt->ar_bspstore;
	ndirty = ia64_rse_num_regs(krbs, krbs + (pt->loadrs >> 19));
	cfm = pt->cr_ifs & ~(1UL << 63);

	if ((long) pt->cr_ifs >= 0) {
		/*
		 * If bit 63 of cr.ifs is cleared, the kernel was entered via a system
		 * call and we need to recover the CFM that existed on entry to the
		 * kernel by unwinding the kernel stack.
		 */
		unw_init_from_blocked_task(&info, child);
		if (unw_unwind_to_user(&info) == 0) {
			unw_get_cfm(&info, &cfm);
			ndirty += (cfm & 0x7f);
		}
	}
	if (cfmp)
		*cfmp = cfm;
	return (unsigned long) ia64_rse_skip_regs(bspstore, ndirty);
}

/*
 * Synchronize (i.e, write) the RSE backing store living in kernel space to the VM of the
 * CHILD task.  SW and PT are the pointers to the switch_stack and pt_regs structures,
 * respectively.  USER_RBS_END is the user-level address at which the backing store ends.
 */
long
ia64_sync_user_rbs (struct task_struct *child, struct switch_stack *sw,
		    unsigned long user_rbs_start, unsigned long user_rbs_end)
{
	unsigned long addr, val;
	long ret;

	/* now copy word for word from kernel rbs to user rbs: */
	for (addr = user_rbs_start; addr < user_rbs_end; addr += 8) {
		ret = ia64_peek(child, sw, user_rbs_end, addr, &val);
		if (ret < 0)
			return ret;
		if (access_process_vm(child, addr, &val, sizeof(val), 1) != sizeof(val))
			return -EIO;
	}
	return 0;
}

/*
 * Simulate user-level "flushrs".  Note: we can't just add pt->loadrs>>16 to
 * pt->ar_bspstore because the kernel backing store and the user-level backing store may
 * have different alignments (and therefore a different number of intervening rnat slots).
 */
static void
user_flushrs (struct task_struct *task, struct pt_regs *pt)
{
	unsigned long *krbs;
	long ndirty;

	krbs = (unsigned long *) task + IA64_RBS_OFFSET/8;
	ndirty = ia64_rse_num_regs(krbs, krbs + (pt->loadrs >> 19));

	pt->ar_bspstore = (unsigned long) ia64_rse_skip_regs((unsigned long *) pt->ar_bspstore,
							     ndirty);
	pt->loadrs = 0;
}

static inline void
sync_user_rbs_one_thread (struct task_struct *p, int make_writable)
{
	struct switch_stack *sw;
	unsigned long urbs_end;
	struct pt_regs *pt;

	sw = (struct switch_stack *) (p->thread.ksp + 16);
	pt = ia64_task_regs(p);
	urbs_end = ia64_get_user_rbs_end(p, pt, NULL);
	ia64_sync_user_rbs(p, sw, pt->ar_bspstore, urbs_end);
	if (make_writable)
		user_flushrs(p, pt);
}

struct task_list {
	struct task_list *next;
	struct task_struct *task;
};

#ifdef CONFIG_SMP

static inline void
collect_task (struct task_list **listp, struct task_struct *p, int make_writable)
{
	struct task_list *e;

	e = kmalloc(sizeof(*e), GFP_KERNEL);
	if (!e)
		/* oops, can't collect more: finish at least what we collected so far... */
		return;

	get_task_struct(p);
	e->task = p;
	e->next = *listp;
	*listp = e;
}

static inline struct task_list *
finish_task (struct task_list *list, int make_writable)
{
	struct task_list *next = list->next;

	sync_user_rbs_one_thread(list->task, make_writable);
	free_task_struct(list->task);
	kfree(list);
	return next;
}

#else
# define collect_task(list, p, make_writable)	sync_user_rbs_one_thread(p, make_writable)
# define finish_task(list, make_writable)	(NULL)
#endif

/*
 * Synchronize the RSE backing store of CHILD and all tasks that share the address space
 * with it.  CHILD_URBS_END is the address of the end of the register backing store of
 * CHILD.  If MAKE_WRITABLE is set, a user-level "flushrs" is simulated such that the VM
 * can be written via ptrace() and the tasks will pick up the newly written values.  It
 * would be OK to unconditionally simulate a "flushrs", but this would be more intrusive
 * than strictly necessary (e.g., it would make it impossible to obtain the original value
 * of ar.bspstore).
 */
static void
threads_sync_user_rbs (struct task_struct *child, unsigned long child_urbs_end, int make_writable)
{
	struct switch_stack *sw;
	struct task_struct *p;
	struct mm_struct *mm;
	struct pt_regs *pt;
	long multi_threaded;

	task_lock(child);
	{
		mm = child->mm;
		multi_threaded = mm && (atomic_read(&mm->mm_users) > 1);
	}
	task_unlock(child);

	if (!multi_threaded) {
		sw = (struct switch_stack *) (child->thread.ksp + 16);
		pt = ia64_task_regs(child);
		ia64_sync_user_rbs(child, sw, pt->ar_bspstore, child_urbs_end);
		if (make_writable)
			user_flushrs(child, pt);
	} else {
		/*
		 * Note: we can't call ia64_sync_user_rbs() while holding the
		 * tasklist_lock because that may cause a dead-lock: ia64_sync_user_rbs()
		 * may indirectly call tlb_flush_all(), which triggers an IPI.
		 * Furthermore, tasklist_lock is acquired by fork() with interrupts
		 * disabled, so with the right timing, the IPI never completes, hence
		 * tasklist_lock never gets released, hence fork() never completes...
		 */
		struct task_list *list = NULL;

		read_lock(&tasklist_lock);
		{
			for_each_task(p) {
				if (p->mm == mm && p->state != TASK_RUNNING)
					collect_task(&list, p, make_writable);
			}
		}
		read_unlock(&tasklist_lock);

		while (list)
			list = finish_task(list, make_writable);
	}
	child->thread.flags |= IA64_THREAD_KRBS_SYNCED;	/* set the flag in the child thread only */
}

/*
 * Write f32-f127 back to task->thread.fph if it has been modified.
 */
inline void
ia64_flush_fph (struct task_struct *task)
{
	struct ia64_psr *psr = ia64_psr(ia64_task_regs(task));

	if (ia64_is_local_fpu_owner(task) && psr->mfh) {
		psr->mfh = 0;
		task->thread.flags |= IA64_THREAD_FPH_VALID;
		ia64_save_fpu(&task->thread.fph[0]);
	}
}

/*
 * Sync the fph state of the task so that it can be manipulated
 * through thread.fph.  If necessary, f32-f127 are written back to
 * thread.fph or, if the fph state hasn't been used before, thread.fph
 * is cleared to zeroes.  Also, access to f32-f127 is disabled to
 * ensure that the task picks up the state from thread.fph when it
 * executes again.
 */
void
ia64_sync_fph (struct task_struct *task)
{
	struct ia64_psr *psr = ia64_psr(ia64_task_regs(task));

	ia64_flush_fph(task);
	if (!(task->thread.flags & IA64_THREAD_FPH_VALID)) {
		task->thread.flags |= IA64_THREAD_FPH_VALID;
		memset(&task->thread.fph, 0, sizeof(task->thread.fph));
	}
	ia64_drop_fpu(task);
	psr->dfh = 1;
}

static int
access_fr (struct unw_frame_info *info, int regnum, int hi, unsigned long *data, int write_access)
{
	struct ia64_fpreg fpval;
	int ret;

	ret = unw_get_fr(info, regnum, &fpval);
	if (ret < 0)
		return ret;

	if (write_access) {
		fpval.u.bits[hi] = *data;
		ret = unw_set_fr(info, regnum, fpval);
	} else
		*data = fpval.u.bits[hi];
	return ret;
}

static int
access_uarea (struct task_struct *child, unsigned long addr, unsigned long *data, int write_access)
{
	unsigned long *ptr, regnum, urbs_end, rnat_addr;
	struct switch_stack *sw;
	struct unw_frame_info info;
	struct pt_regs *pt;

	pt = ia64_task_regs(child);
	sw = (struct switch_stack *) (child->thread.ksp + 16);

	if ((addr & 0x7) != 0) {
		dprintk("ptrace: unaligned register address 0x%lx\n", addr);
		return -1;
	}

	if (addr < PT_F127 + 16) {
		/* accessing fph */
		if (write_access)
			ia64_sync_fph(child);
		else
			ia64_flush_fph(child);
		ptr = (unsigned long *) ((unsigned long) &child->thread.fph + addr);
	} else if ((addr >= PT_F10) && (addr < PT_F11 + 16)) {
		/* scratch registers untouched by kernel (saved in pt_regs) */
		ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, f10) + addr - PT_F10);
	} else if (addr >= PT_F12 && addr < PT_F15 + 16) {
		/* scratch registers untouched by kernel (saved in switch_stack) */
		ptr = (unsigned long *) ((long) sw + (addr - PT_NAT_BITS - 32));
	} else if (addr < PT_AR_LC + 8) {
		/* preserved state: */
		unsigned long nat_bits, scratch_unat, dummy = 0;
		struct unw_frame_info info;
		char nat = 0;
		int ret;

		unw_init_from_blocked_task(&info, child);
		if (unw_unwind_to_user(&info) < 0)
			return -1;

		switch (addr) {
		      case PT_NAT_BITS:
			if (write_access) {
				nat_bits = *data;
				scratch_unat = ia64_put_scratch_nat_bits(pt, nat_bits);
				if (unw_set_ar(&info, UNW_AR_UNAT, scratch_unat) < 0) {
					dprintk("ptrace: failed to set ar.unat\n");
					return -1;
				}
				for (regnum = 4; regnum <= 7; ++regnum) {
					unw_get_gr(&info, regnum, &dummy, &nat);
					unw_set_gr(&info, regnum, dummy, (nat_bits >> regnum) & 1);
				}
			} else {
				if (unw_get_ar(&info, UNW_AR_UNAT, &scratch_unat) < 0) {
					dprintk("ptrace: failed to read ar.unat\n");
					return -1;
				}
				nat_bits = ia64_get_scratch_nat_bits(pt, scratch_unat);
				for (regnum = 4; regnum <= 7; ++regnum) {
					unw_get_gr(&info, regnum, &dummy, &nat);
					nat_bits |= (nat != 0) << regnum;
				}
				*data = nat_bits;
			}
			return 0;

		      case PT_R4: case PT_R5: case PT_R6: case PT_R7:
			if (write_access) {
				/* read NaT bit first: */
				unsigned long dummy;

				ret = unw_get_gr(&info, (addr - PT_R4)/8 + 4, &dummy, &nat);
				if (ret < 0)
					return ret;
			}
			return unw_access_gr(&info, (addr - PT_R4)/8 + 4, data, &nat,
					     write_access);

		      case PT_B1: case PT_B2: case PT_B3: case PT_B4: case PT_B5:
			return unw_access_br(&info, (addr - PT_B1)/8 + 1, data, write_access);

		      case PT_AR_EC:
			return unw_access_ar(&info, UNW_AR_EC, data, write_access);

		      case PT_AR_LC:
			return unw_access_ar(&info, UNW_AR_LC, data, write_access);

		      default:
			if (addr >= PT_F2 && addr < PT_F5 + 16)
				return access_fr(&info, (addr - PT_F2)/16 + 2, (addr & 8) != 0,
						 data, write_access);
			else if (addr >= PT_F16 && addr < PT_F31 + 16)
				return access_fr(&info, (addr - PT_F16)/16 + 16, (addr & 8) != 0,
						 data, write_access);
			else {
				dprintk("ptrace: rejecting access to register address 0x%lx\n",
					addr);
				return -1;
			}
		}
	} else if (addr < PT_F9+16) {
		/* scratch state */
		switch (addr) {
		      case PT_AR_BSP:
			/*
			 * By convention, we use PT_AR_BSP to refer to the end of the user-level
			 * backing store.  Use ia64_rse_skip_regs(PT_AR_BSP, -CFM.sof) to get
			 * the real value of ar.bsp at the time the kernel was entered.
			 */
			urbs_end = ia64_get_user_rbs_end(child, pt, NULL);
			if (write_access) {
				if (*data != urbs_end) {
					if (ia64_sync_user_rbs(child, sw,
							       pt->ar_bspstore, urbs_end) < 0)
						return -1;
					/* simulate user-level write of ar.bsp: */
					pt->loadrs = 0;
					pt->ar_bspstore = *data;
				}
			} else
				*data = urbs_end;
			return 0;

		      case PT_CFM:
			if ((long) pt->cr_ifs < 0) {
				if (write_access)
					pt->cr_ifs = ((pt->cr_ifs & ~0x3fffffffffUL)
						      | (*data & 0x3fffffffffUL));
				else
					*data = pt->cr_ifs & 0x3fffffffffUL;
			} else {
				/* kernel was entered through a system call */
				unsigned long cfm;

				unw_init_from_blocked_task(&info, child);
				if (unw_unwind_to_user(&info) < 0)
					return -1;

				unw_get_cfm(&info, &cfm);
				if (write_access)
					unw_set_cfm(&info, ((cfm & ~0x3fffffffffU)
							    | (*data & 0x3fffffffffUL)));
				else
					*data = cfm;
			}
			return 0;

		      case PT_CR_IPSR:
			if (write_access)
				pt->cr_ipsr = ((*data & IPSR_WRITE_MASK)
					       | (pt->cr_ipsr & ~IPSR_WRITE_MASK));
			else
				*data = (pt->cr_ipsr & IPSR_READ_MASK);
			return 0;

		      case PT_AR_RNAT:
			urbs_end = ia64_get_user_rbs_end(child, pt, NULL);
			rnat_addr = (long) ia64_rse_rnat_addr((long *) urbs_end);
			if (write_access)
				return ia64_poke(child, sw, urbs_end, rnat_addr, *data);
			else
				return ia64_peek(child, sw, urbs_end, rnat_addr, data);

		      case PT_R1:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, r1));
			break;
		       
		       case PT_R2:  case PT_R3:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, r2) + addr - PT_R2);
			break;
		      case PT_R8:  case PT_R9:  case PT_R10: case PT_R11:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, r8)+  addr - PT_R8);
			break;
		      case PT_R12: case PT_R13: 
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, r12)+  addr - PT_R12);
			break;
		      case PT_R14: 
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, r14));
			break;
		      case PT_R15:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, r15));
			break;
		      case PT_R16: case PT_R17: case PT_R18: case PT_R19:
		      case PT_R20: case PT_R21: case PT_R22: case PT_R23:
		      case PT_R24: case PT_R25: case PT_R26: case PT_R27:
		      case PT_R28: case PT_R29: case PT_R30: case PT_R31:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, r16) + addr - PT_R16);
			break;
		      case PT_B0:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, b0));
			break;
		      case PT_B6:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, b6));
			break;
		      case PT_B7:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, b7));
			break;
		      case PT_F6:  case PT_F6+8: case PT_F7: case PT_F7+8:
		      case PT_F8:  case PT_F8+8: case PT_F9: case PT_F9+8:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, f6) + addr - PT_F6);
			break;
		      case PT_AR_BSPSTORE:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, ar_bspstore));
			break;
		      case PT_AR_RSC: 
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, ar_rsc));
			break;
		      case PT_AR_UNAT: 
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, ar_unat));
			break;
		      case PT_AR_PFS:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, ar_pfs));
			break;
		      case PT_AR_CCV: 
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, ar_ccv));
			break;
		      case PT_AR_FPSR: 
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, ar_fpsr));
			break;
		      case PT_CR_IIP: 
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, cr_iip));
			break;
		      case PT_PR:
			ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, pr));
			break;
			/* scratch register */

		      default:
			/* disallow accessing anything else... */
			dprintk("ptrace: rejecting access to register address 0x%lx\n",
				addr);
			return -1;
		}
	} else if (addr <= PT_AR_SSD) {
		ptr = (unsigned long *) ((long) pt + offsetof(struct pt_regs, ar_csd) + addr - PT_AR_CSD);
	} else {
		/* access debug registers */

		if (addr >= PT_IBR) {
			regnum = (addr - PT_IBR) >> 3;
			ptr = &child->thread.ibr[0];
		} else {
			regnum = (addr - PT_DBR) >> 3;
			ptr = &child->thread.dbr[0];
		}

		if (regnum >= 8) {
			dprintk("ptrace: rejecting access to register address 0x%lx\n", addr);
			return -1;
		}
#ifdef CONFIG_PERFMON
		/*
		 * Check if debug registers are used by perfmon. This test must be done
		 * once we know that we can do the operation, i.e. the arguments are all
		 * valid, but before we start modifying the state.
		 *
		 * Perfmon needs to keep a count of how many processes are trying to
		 * modify the debug registers for system wide monitoring sessions.
		 *
		 * We also include read access here, because they may cause the
		 * PMU-installed debug register state (dbr[], ibr[]) to be reset. The two
		 * arrays are also used by perfmon, but we do not use
		 * IA64_THREAD_DBG_VALID. The registers are restored by the PMU context
		 * switch code.
		 */
		if (pfm_use_debug_registers(child)) return -1;
#endif

		if (!(child->thread.flags & IA64_THREAD_DBG_VALID)) {
			child->thread.flags |= IA64_THREAD_DBG_VALID;
			memset(child->thread.dbr, 0, sizeof(child->thread.dbr));
			memset(child->thread.ibr, 0, sizeof(child->thread.ibr));
		}

		ptr += regnum;

		if (write_access)
			/* don't let the user set kernel-level breakpoints... */
			*ptr = *data & ~(7UL << 56);
		else
			*data = *ptr;
		return 0;
	}
	if (write_access)
		*ptr = *data;
	else
		*data = *ptr;
	return 0;
}

static long
ptrace_getregs (struct task_struct *child, struct pt_all_user_regs *ppr)
{
	struct switch_stack *sw;
	struct pt_regs *pt;
	long ret, retval;
	struct unw_frame_info info;
	char nat = 0;
	int i;

	retval = verify_area(VERIFY_WRITE, ppr, sizeof(struct pt_all_user_regs));
	if (retval != 0) {
		return -EIO;
	}

	pt = ia64_task_regs(child);
	sw = (struct switch_stack *) (child->thread.ksp + 16);
	unw_init_from_blocked_task(&info, child);
	if (unw_unwind_to_user(&info) < 0) {
		return -EIO;
	}

	if (((unsigned long) ppr & 0x7) != 0) {
		dprintk("ptrace:unaligned register address %p\n", ppr);
		return -EIO;
	}

	retval = 0;

	/* control regs */

	retval |= __put_user(pt->cr_iip, &ppr->cr_iip);
	retval |= access_uarea(child, PT_CR_IPSR, &ppr->cr_ipsr, 0);

	/* app regs */

	retval |= __put_user(pt->ar_pfs, &ppr->ar[PT_AUR_PFS]);
	retval |= __put_user(pt->ar_rsc, &ppr->ar[PT_AUR_RSC]);
	retval |= __put_user(pt->ar_bspstore, &ppr->ar[PT_AUR_BSPSTORE]);
	retval |= __put_user(pt->ar_unat, &ppr->ar[PT_AUR_UNAT]);
	retval |= __put_user(pt->ar_ccv, &ppr->ar[PT_AUR_CCV]);
	retval |= __put_user(pt->ar_fpsr, &ppr->ar[PT_AUR_FPSR]);

	retval |= access_uarea(child, PT_AR_EC, &ppr->ar[PT_AUR_EC], 0);
	retval |= access_uarea(child, PT_AR_LC, &ppr->ar[PT_AUR_LC], 0);
	retval |= access_uarea(child, PT_AR_RNAT, &ppr->ar[PT_AUR_RNAT], 0);
	retval |= access_uarea(child, PT_AR_BSP, &ppr->ar[PT_AUR_BSP], 0);
	retval |= access_uarea(child, PT_CFM, &ppr->cfm, 0);

	/* gr1-gr3 */

	retval |= __copy_to_user(&ppr->gr[1], &pt->r1, sizeof(long));
	retval |= __copy_to_user(&ppr->gr[2], &pt->r2, sizeof(long) *2);

	/* gr4-gr7 */

	for (i = 4; i < 8; i++) {
		retval |= unw_access_gr(&info, i, &ppr->gr[i], &nat, 0);
	}

	/* gr8-gr11 */

	retval |= __copy_to_user(&ppr->gr[8], &pt->r8, sizeof(long) * 4);

	/* gr12-gr15 */

	retval |= __copy_to_user(&ppr->gr[12], &pt->r12, sizeof(long) * 2);
	retval |= __copy_to_user(&ppr->gr[14], &pt->r14, sizeof(long));
	retval |= __copy_to_user(&ppr->gr[15], &pt->r15, sizeof(long));

	/* gr16-gr31 */

	retval |= __copy_to_user(&ppr->gr[16], &pt->r16, sizeof(long) * 16);

	/* b0 */

	retval |= __put_user(pt->b0, &ppr->br[0]);

	/* b1-b5 */

	for (i = 1; i < 6; i++) {
		retval |= unw_access_br(&info, i, &ppr->br[i], 0);
	}

	/* b6-b7 */

	retval |= __put_user(pt->b6, &ppr->br[6]);
	retval |= __put_user(pt->b7, &ppr->br[7]);

	/* fr2-fr5 */

	for (i = 2; i < 6; i++) {
		retval |= access_fr(&info, i, 0, (unsigned long *) &ppr->fr[i], 0);
		retval |= access_fr(&info, i, 1, (unsigned long *) &ppr->fr[i] + 1, 0);
	}

	/* fr6-fr11 */

	retval |= __copy_to_user(&ppr->fr[6], &pt->f6, sizeof(struct ia64_fpreg) * 6);

	/* fp scratch regs(12-15) */

	retval |= __copy_to_user(&ppr->fr[12], &sw->f12, sizeof(struct ia64_fpreg) * 4);

	/* fr16-fr31 */

	for (i = 16; i < 32; i++) {
		retval |= access_fr(&info, i, 0, (unsigned long *) &ppr->fr[i], 0);
		retval |= access_fr(&info, i, 1, (unsigned long *) &ppr->fr[i] + 1, 0);
	}

	/* fph */

	ia64_flush_fph(child);
	retval |= __copy_to_user(&ppr->fr[32], &child->thread.fph, sizeof(ppr->fr[32]) * 96);

	/* preds */

	retval |= __put_user(pt->pr, &ppr->pr);

	/* nat bits */

	retval |= access_uarea(child, PT_NAT_BITS, &ppr->nat, 0);

	ret = retval ? -EIO : 0;
	return ret;
}

static long
ptrace_setregs (struct task_struct *child, struct pt_all_user_regs *ppr)
{
	struct switch_stack *sw;
	struct pt_regs *pt;
	long ret, retval;
	struct unw_frame_info info;
	char nat = 0;
	int i;

	retval = verify_area(VERIFY_READ, ppr, sizeof(struct pt_all_user_regs));
	if (retval != 0) {
		return -EIO;
	}

	pt = ia64_task_regs(child);
	sw = (struct switch_stack *) (child->thread.ksp + 16);
	unw_init_from_blocked_task(&info, child);
	if (unw_unwind_to_user(&info) < 0) {
		return -EIO;
	}

	if (((unsigned long) ppr & 0x7) != 0) {
		dprintk("ptrace:unaligned register address %p\n", ppr);
		return -EIO;
	}

	retval = 0;

	/* control regs */

	retval |= __get_user(pt->cr_iip, &ppr->cr_iip);
	retval |= access_uarea(child, PT_CR_IPSR, &ppr->cr_ipsr, 1);

	/* app regs */

	retval |= __get_user(pt->ar_pfs, &ppr->ar[PT_AUR_PFS]);
	retval |= __get_user(pt->ar_rsc, &ppr->ar[PT_AUR_RSC]);
	retval |= __get_user(pt->ar_bspstore, &ppr->ar[PT_AUR_BSPSTORE]);
	retval |= __get_user(pt->ar_unat, &ppr->ar[PT_AUR_UNAT]);
	retval |= __get_user(pt->ar_ccv, &ppr->ar[PT_AUR_CCV]);
	retval |= __get_user(pt->ar_fpsr, &ppr->ar[PT_AUR_FPSR]);

	retval |= access_uarea(child, PT_AR_EC, &ppr->ar[PT_AUR_EC], 1);
	retval |= access_uarea(child, PT_AR_LC, &ppr->ar[PT_AUR_LC], 1);
	retval |= access_uarea(child, PT_AR_RNAT, &ppr->ar[PT_AUR_RNAT], 1);
	retval |= access_uarea(child, PT_AR_BSP, &ppr->ar[PT_AUR_BSP], 1);
	retval |= access_uarea(child, PT_CFM, &ppr->cfm, 1);

	/* gr1-gr3 */

	retval |= __copy_from_user(&pt->r1, &ppr->gr[1], sizeof(long));
	retval |= __copy_from_user(&pt->r2, &ppr->gr[2], sizeof(long) * 2);

	/* gr4-gr7 */

	for (i = 4; i < 8; i++) {
		long ret = unw_get_gr(&info, i, &ppr->gr[i], &nat);
		if (ret < 0) {
			return ret;
		}
		retval |= unw_access_gr(&info, i, &ppr->gr[i], &nat, 1);
	}

	/* gr8-gr11 */

	retval |= __copy_from_user(&pt->r8, &ppr->gr[8], sizeof(long) * 4);

	/* gr12-gr15 */

	retval |= __copy_from_user(&pt->r12, &ppr->gr[12], sizeof(long) * 2);
	retval |= __copy_from_user(&pt->r14, &ppr->gr[14], sizeof(long));
	retval |= __copy_from_user(&pt->r15, &ppr->gr[15], sizeof(long));

	/* gr16-gr31 */

	retval |= __copy_from_user(&pt->r16, &ppr->gr[16], sizeof(long) * 16);

	/* b0 */

	retval |= __get_user(pt->b0, &ppr->br[0]);

	/* b1-b5 */

	for (i = 1; i < 6; i++) {
		retval |= unw_access_br(&info, i, &ppr->br[i], 1);
	}

	/* b6-b7 */

	retval |= __get_user(pt->b6, &ppr->br[6]);
	retval |= __get_user(pt->b7, &ppr->br[7]);

	/* fr2-fr5 */

	for (i = 2; i < 6; i++) {
		retval |= access_fr(&info, i, 0, (unsigned long *) &ppr->fr[i], 1);
		retval |= access_fr(&info, i, 1, (unsigned long *) &ppr->fr[i] + 1, 1);
	}

	/* fr6-fr11 */

	retval |= __copy_from_user(&pt->f6, &ppr->fr[6], sizeof(ppr->fr[6]) * 6);

	/* fp scratch regs(12-15) */

	retval |= __copy_from_user(&sw->f12, &ppr->fr[12], sizeof(ppr->fr[12]) * 4);

	/* fr16-fr31 */

	for (i = 16; i < 32; i++) {
		retval |= access_fr(&info, i, 0, (unsigned long *) &ppr->fr[i], 1);
		retval |= access_fr(&info, i, 1, (unsigned long *) &ppr->fr[i] + 1, 1);
	}

	/* fph */

	ia64_sync_fph(child);
	retval |= __copy_from_user(&child->thread.fph, &ppr->fr[32], sizeof(ppr->fr[32]) * 96);

	/* preds */

	retval |= __get_user(pt->pr, &ppr->pr);

	/* nat bits */

	retval |= access_uarea(child, PT_NAT_BITS, &ppr->nat, 1);

	ret = retval ? -EIO : 0;
	return ret;
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure the single step bit is not set.
 */
void
ptrace_disable (struct task_struct *child)
{
	struct ia64_psr *child_psr = ia64_psr(ia64_task_regs(child));

	/* make sure the single step/take-branch tra bits are not set: */
	child_psr->ss = 0;
	child_psr->tb = 0;

	/* Turn off flag indicating that the KRBS is sync'd with child's VM: */
	child->thread.flags &= ~IA64_THREAD_KRBS_SYNCED;
}

asmlinkage long
sys_ptrace (long request, pid_t pid, unsigned long addr, unsigned long data,
	    long arg4, long arg5, long arg6, long arg7, long stack)
{
	struct pt_regs *pt, *regs = (struct pt_regs *) &stack;
	unsigned long urbs_end;
	struct task_struct *child;
	struct switch_stack *sw;
	long ret;

	lock_kernel();
	ret = -EPERM;
	if (request == PTRACE_TRACEME) {
		/* are we already being traced? */
		if (current->ptrace & PT_PTRACED)
			goto out;
		current->ptrace |= PT_PTRACED;
		ret = 0;
		goto out;
	}

	ret = -ESRCH;
	read_lock(&tasklist_lock);
	{
		child = find_task_by_pid(pid);
		if (child)
			get_task_struct(child);
	}
	read_unlock(&tasklist_lock);
	if (!child)
		goto out;
	ret = -EPERM;
	if (pid == 1)		/* no messing around with init! */
		goto out_tsk;

	if (request == PTRACE_ATTACH) {
		ret = ptrace_attach(child);
		goto out_tsk;
	}

	ret = ptrace_check_attach(child, request == PTRACE_KILL);
	if (ret < 0)
		goto out_tsk;

	pt = ia64_task_regs(child);
	sw = (struct switch_stack *) (child->thread.ksp + 16);

	switch (request) {
	      case PTRACE_PEEKTEXT:
	      case PTRACE_PEEKDATA:		/* read word at location addr */
		urbs_end = ia64_get_user_rbs_end(child, pt, NULL);

		if (!(child->thread.flags & IA64_THREAD_KRBS_SYNCED))
			threads_sync_user_rbs(child, urbs_end, 0);

		ret = ia64_peek(child, sw, urbs_end, addr, &data);
		if (ret == 0) {
			ret = data;
			regs->r8 = 0;	/* ensure "ret" is not mistaken as an error code */
		}
		goto out_tsk;

	      case PTRACE_POKETEXT:
	      case PTRACE_POKEDATA:		/* write the word at location addr */
		urbs_end = ia64_get_user_rbs_end(child, pt, NULL);
		if (!(child->thread.flags & IA64_THREAD_KRBS_SYNCED))
			threads_sync_user_rbs(child, urbs_end, 1);

		ret = ia64_poke(child, sw, urbs_end, addr, data);
		goto out_tsk;

	      case PTRACE_PEEKUSR:		/* read the word at addr in the USER area */
		if (access_uarea(child, addr, &data, 0) < 0) {
			ret = -EIO;
			goto out_tsk;
		}
		ret = data;
		regs->r8 = 0;	/* ensure "ret" is not mistaken as an error code */
		goto out_tsk;

	      case PTRACE_POKEUSR:	      /* write the word at addr in the USER area */
		if (access_uarea(child, addr, &data, 1) < 0) {
			ret = -EIO;
			goto out_tsk;
		}
		ret = 0;
		goto out_tsk;

	      case PTRACE_GETSIGINFO:
		ret = -EIO;
		if (!access_ok(VERIFY_WRITE, data, sizeof (siginfo_t)) || !child->thread.siginfo)
			goto out_tsk;
		ret = copy_siginfo_to_user((siginfo_t *) data, child->thread.siginfo);
		goto out_tsk;

	      case PTRACE_SETSIGINFO:
		ret = -EIO;
		if (!access_ok(VERIFY_READ, data, sizeof (siginfo_t))
		    || child->thread.siginfo == 0)
			goto out_tsk;
		ret = copy_siginfo_from_user(child->thread.siginfo, (siginfo_t *) data);
		goto out_tsk;

	      case PTRACE_SYSCALL:	/* continue and stop at next (return from) syscall */
	      case PTRACE_CONT:		/* restart after signal. */
		ret = -EIO;
		if (data > _NSIG)
			goto out_tsk;
		if (request == PTRACE_SYSCALL)
			child->ptrace |= PT_TRACESYS;
		else
			child->ptrace &= ~PT_TRACESYS;
		child->exit_code = data;

		/* make sure the single step/taken-branch trap bits are not set: */
		ia64_psr(pt)->ss = 0;
		ia64_psr(pt)->tb = 0;

		/* Turn off flag indicating that the KRBS is sync'd with child's VM: */
		child->thread.flags &= ~IA64_THREAD_KRBS_SYNCED;

		wake_up_process(child);
		ret = 0;
		goto out_tsk;

	      case PTRACE_KILL:
		/*
		 * Make the child exit.  Best I can do is send it a
		 * sigkill.  Perhaps it should be put in the status
		 * that it wants to exit.
		 */
		if (child->state == TASK_ZOMBIE)		/* already dead */
			goto out_tsk;
		child->exit_code = SIGKILL;

		/* make sure the single step/take-branch tra bits are not set: */
		ia64_psr(pt)->ss = 0;
		ia64_psr(pt)->tb = 0;

		/* Turn off flag indicating that the KRBS is sync'd with child's VM: */
		child->thread.flags &= ~IA64_THREAD_KRBS_SYNCED;

		wake_up_process(child);
		ret = 0;
		goto out_tsk;

	      case PTRACE_SINGLESTEP:		/* let child execute for one instruction */
	      case PTRACE_SINGLEBLOCK:
		ret = -EIO;
		if (data > _NSIG)
			goto out_tsk;

		child->ptrace &= ~PT_TRACESYS;
		if (request == PTRACE_SINGLESTEP) {
			ia64_psr(pt)->ss = 1;
		} else {
			ia64_psr(pt)->tb = 1;
		}
		child->exit_code = data;

		/* Turn off flag indicating that the KRBS is sync'd with child's VM: */
		child->thread.flags &= ~IA64_THREAD_KRBS_SYNCED;

		/* give it a chance to run. */
		wake_up_process(child);
		ret = 0;
		goto out_tsk;

	      case PTRACE_DETACH:		/* detach a process that was attached. */
		ret = ptrace_detach(child, data);
		goto out_tsk;

	      case PTRACE_GETREGS:
		ret = ptrace_getregs(child, (struct pt_all_user_regs*) data);
		goto out_tsk;

	      case PTRACE_SETREGS:
		ret = ptrace_setregs(child, (struct pt_all_user_regs*) data);
		goto out_tsk;

	      default:
		ret = -EIO;
		goto out_tsk;
	}
  out_tsk:
	free_task_struct(child);
  out:
	unlock_kernel();
	return ret;
}

void
syscall_trace (void)
{
	if ((current->ptrace & (PT_PTRACED|PT_TRACESYS)) != (PT_PTRACED|PT_TRACESYS))
		return;
	current->exit_code = SIGTRAP;
	set_current_state(TASK_STOPPED);
	notify_parent(current, SIGCHLD);
	schedule();

	/*
	 * This isn't the same as continuing with a signal, but it will do for normal use.
	 * strace only continues with a signal if the stopping signal is not SIGTRAP.
	 * -brl
	 */
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}
