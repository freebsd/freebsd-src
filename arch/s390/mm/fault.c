/*
 *  arch/s390/mm/fault.c
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com)
 *               Ulrich Weigand (uweigand@de.ibm.com)
 *
 *  Derived from "arch/i386/mm/fault.c"
 *    Copyright (C) 1995  Linus Torvalds
 */

#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/compatmac.h>
#include <linux/init.h>
#include <linux/console.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/hardirq.h>

#ifdef CONFIG_SYSCTL
extern int sysctl_userprocess_debug;
#endif

extern void die(const char *,struct pt_regs *,long);

extern spinlock_t timerlist_lock;

/*
 * Unlock any spinlocks which will prevent us from getting the
 * message out (timerlist_lock is acquired through the
 * console unblank code)
 */
void bust_spinlocks(int yes)
{
	spin_lock_init(&timerlist_lock);
	if (yes) {
		oops_in_progress = 1;
	} else {
		int loglevel_save = console_loglevel;
		oops_in_progress = 0;
		console_unblank();
		/*
		 * OK, the message is on the console.  Now we call printk()
		 * without oops_in_progress set so that printk will give klogd
		 * a poke.  Hold onto your hats...
		 */
		console_loglevel = 15;
		printk(" ");
		console_loglevel = loglevel_save;
	}
}

/*
 * Check which address space is addressed by the access
 * register in S390_lowcore.exc_access_id.
 * Returns 1 for user space and 0 for kernel space.
 */
static int __check_access_register(struct pt_regs *regs, int error_code)
{
	int areg = S390_lowcore.exc_access_id;

	if (areg == 0)
		/* Access via access register 0 -> kernel address */
		return 0;
	if (regs && areg < NUM_ACRS && regs->acrs[areg] <= 1)
		/*
		 * access register contains 0 -> kernel address,
		 * access register contains 1 -> user space address
		 */
		return regs->acrs[areg];

	/* Something unhealthy was done with the access registers... */
	die("page fault via unknown access register", regs, error_code);
	do_exit(SIGKILL);
	return 0;
}

/*
 * Check which address space the address belongs to.
 * Returns 1 for user space and 0 for kernel space.
 */
static inline int check_user_space(struct pt_regs *regs, int error_code)
{
	/*
	 * The lowest two bits of S390_lowcore.trans_exc_code indicate
	 * which paging table was used:
	 *   0: Primary Segment Table Descriptor
	 *   1: STD determined via access register
	 *   2: Secondary Segment Table Descriptor
	 *   3: Home Segment Table Descriptor
	 */
	int descriptor = S390_lowcore.trans_exc_code & 3;
	if (descriptor == 1)
		return __check_access_register(regs, error_code);
	return descriptor >> 1;
}

/*
 * Send SIGSEGV to task.  This is an external routine
 * to keep the stack usage of do_page_fault small.
 */
static void force_sigsegv(struct pt_regs *regs, unsigned long error_code,
			  int si_code, unsigned long address)
{
	struct siginfo si;

#if defined(CONFIG_SYSCTL) || defined(CONFIG_PROCESS_DEBUG)
#if defined(CONFIG_SYSCTL)
	if (sysctl_userprocess_debug)
#endif
	{
		printk("User process fault: interruption code 0x%lX\n",
		       error_code);
		printk("failing address: %lX\n", address);
		show_regs(regs);
	}
#endif
	si.si_signo = SIGSEGV;
	si.si_code = si_code;
	si.si_addr = (void *) address;
	force_sig_info(SIGSEGV, &si, current);
}

/*
 * This routine handles page faults.  It determines the address,
 * and the problem, and then passes it off to one of the appropriate
 * routines.
 *
 * error_code:
 *   04       Protection           ->  Write-Protection  (suprression)
 *   10       Segment translation  ->  Not present       (nullification)
 *   11       Page translation     ->  Not present       (nullification)
 */
extern inline void do_exception(struct pt_regs *regs, unsigned long error_code)
{
        struct task_struct *tsk;
        struct mm_struct *mm;
        struct vm_area_struct * vma;
        unsigned long address;
	int user_address;
        unsigned long fixup;
	int si_code = SEGV_MAPERR;

        tsk = current;
        mm = tsk->mm;
	
	/* 
         * Check for low-address protection.  This needs to be treated
	 * as a special case because the translation exception code 
	 * field is not guaranteed to contain valid data in this case.
	 */
	if (error_code == 4 && !(S390_lowcore.trans_exc_code & 4)) {

		/* Low-address protection hit in kernel mode means 
		   NULL pointer write access in kernel mode.  */
 		if (!(regs->psw.mask & PSW_PROBLEM_STATE)) {
			address = 0;
			user_address = 0;
			goto no_context;
		}

		/* Low-address protection hit in user mode 'cannot happen'.  */
		die ("Low-address protection", regs, error_code);
        	do_exit(SIGKILL);
	}

        /* 
         * get the failing address 
         * more specific the segment and page table portion of 
         * the address 
         */
        address = S390_lowcore.trans_exc_code&0x7ffff000;
	user_address = check_user_space(regs, error_code);

	/*
	 * Verify that the fault happened in user space, that
	 * we are not in an interrupt and that there is a 
	 * user context.
	 */
        if (user_address == 0 || in_interrupt() || !mm)
                goto no_context;

	/*
	 * When we get here, the fault happened in the current
	 * task's user address space, so we can switch on the
	 * interrupts again and then search the VMAs
	 */
	__sti();

        down_read(&mm->mmap_sem);

        vma = find_vma(mm, address);
        if (!vma)
                goto bad_area;
        if (vma->vm_start <= address) 
                goto good_area;
        if (!(vma->vm_flags & VM_GROWSDOWN))
                goto bad_area;
        if (expand_stack(vma, address))
                goto bad_area;
/*
 * Ok, we have a good vm_area for this memory access, so
 * we can handle it..
 */
good_area:
	si_code = SEGV_ACCERR;
	if (error_code != 4) {
		/* page not present, check vm flags */
		if (!(vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)))
			goto bad_area;
	} else {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	}

survive:
	/*
	 * If for any reason at all we couldn't handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.
	 */
	switch (handle_mm_fault(mm, vma, address, error_code == 4)) {
	case 1:
		tsk->min_flt++;
		break;
	case 2:
		tsk->maj_flt++;
		break;
	case 0:
		goto do_sigbus;
	default:
		goto out_of_memory;
	}

        up_read(&mm->mmap_sem);
        return;

/*
 * Something tried to access memory that isn't in our memory map..
 * Fix it, but check if it's kernel or user first..
 */
bad_area:
        up_read(&mm->mmap_sem);

        /* User mode accesses just cause a SIGSEGV */
        if (regs->psw.mask & PSW_PROBLEM_STATE) {
                tsk->thread.prot_addr = address;
                tsk->thread.trap_no = error_code;
		force_sigsegv(regs, error_code, si_code, address);
                return;
	}

no_context:
        /* Are we prepared to handle this kernel fault?  */
        if ((fixup = search_exception_table(regs->psw.addr)) != 0) {
                regs->psw.addr = fixup;
                return;
        }

/*
 * Oops. The kernel tried to access some bad page. We'll have to
 * terminate things with extreme prejudice.
 */
        if (user_address == 0)
                printk(KERN_ALERT "Unable to handle kernel pointer dereference"
        	       " at virtual kernel address %08lx\n", address);
        else
                printk(KERN_ALERT "Unable to handle kernel paging request"
		       " at virtual user address %08lx\n", address);

        die("Oops", regs, error_code);
        do_exit(SIGKILL);


/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
*/
out_of_memory:
	if (tsk->pid == 1) {
		yield();
		goto survive;
	}
	up_read(&mm->mmap_sem);
	printk("VM: killing process %s\n", tsk->comm);
	if (regs->psw.mask & PSW_PROBLEM_STATE)
		do_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);

	/*
	 * Send a sigbus, regardless of whether we were in kernel
	 * or user mode.
	 */
        tsk->thread.prot_addr = address;
        tsk->thread.trap_no = error_code;
	force_sig(SIGBUS, tsk);

	/* Kernel mode? Handle exceptions or die */
	if (!(regs->psw.mask & PSW_PROBLEM_STATE))
		goto no_context;
}

void do_protection_exception(struct pt_regs *regs, unsigned long error_code)
{
	regs->psw.addr -= (error_code >> 16);
	do_exception(regs, 4);
}

void do_segment_exception(struct pt_regs *regs, unsigned long error_code)
{
	do_exception(regs, 0x10);
}

void do_page_exception(struct pt_regs *regs, unsigned long error_code)
{
	do_exception(regs, 0x11);
}

typedef struct _pseudo_wait_t {
       struct _pseudo_wait_t *next;
       wait_queue_head_t queue;
       unsigned long address;
       int resolved;
} pseudo_wait_t;

static pseudo_wait_t *pseudo_lock_queue = NULL;
static spinlock_t pseudo_wait_spinlock; /* spinlock to protect lock queue */

/*
 * This routine handles 'pagex' pseudo page faults.
 */
asmlinkage void
do_pseudo_page_fault(struct pt_regs *regs, unsigned long error_code)
{
        pseudo_wait_t wait_struct;
        pseudo_wait_t *ptr, *last, *next;
        unsigned long address;

        /*
         * get the failing address
         * more specific the segment and page table portion of
         * the address
         */
        address = S390_lowcore.trans_exc_code & 0xfffff000;

        if (address & 0x80000000) {
                /* high bit set -> a page has been swapped in by VM */
                address &= 0x7fffffff;
                spin_lock(&pseudo_wait_spinlock);
                last = NULL;
                ptr = pseudo_lock_queue;
                while (ptr != NULL) {
                        next = ptr->next;
                        if (address == ptr->address) {
				 /*
                                 * This is one of the processes waiting
                                 * for the page. Unchain from the queue.
                                 * There can be more than one process
                                 * waiting for the same page. VM presents
                                 * an initial and a completion interrupt for
                                 * every process that tries to access a 
                                 * page swapped out by VM. 
                                 */
                                if (last == NULL)
                                        pseudo_lock_queue = next;
                                else
                                        last->next = next;
                                /* now wake up the process */
                                ptr->resolved = 1;
                                wake_up(&ptr->queue);
                        } else
                                last = ptr;
                        ptr = next;
                }
                spin_unlock(&pseudo_wait_spinlock);
        } else {
                /* Pseudo page faults in kernel mode is a bad idea */
                if (!(regs->psw.mask & PSW_PROBLEM_STATE)) {
                        /*
			 * VM presents pseudo page faults if the interrupted
			 * state was not disabled for interrupts. So we can
			 * get pseudo page fault interrupts while running
			 * in kernel mode. We simply access the page here
			 * while we are running disabled. VM will then swap
			 * in the page synchronously.
                         */
                         if (check_user_space(regs, error_code) == 0)
                                 /* dereference a virtual kernel address */
                                 __asm__ __volatile__ (
                                         "  ic 0,0(%0)"
                                         : : "a" (address) : "0");
                         else
                                 /* dereference a virtual user address */
                                 __asm__ __volatile__ (
                                         "  la   2,0(%0)\n"
                                         "  sacf 512\n"
                                         "  ic   2,0(2)\n"
					 "0:sacf 0\n"
					 ".section __ex_table,\"a\"\n"
					 "  .align 4\n"
					 "  .long  0b,0b\n"
					 ".previous"
                                         : : "a" (address) : "2" );

                        return;
                }
		/* initialize and add element to pseudo_lock_queue */
                init_waitqueue_head (&wait_struct.queue);
                wait_struct.address = address;
                wait_struct.resolved = 0;
                spin_lock(&pseudo_wait_spinlock);
                wait_struct.next = pseudo_lock_queue;
                pseudo_lock_queue = &wait_struct;
                spin_unlock(&pseudo_wait_spinlock);
                /* go to sleep */
                wait_event(wait_struct.queue, wait_struct.resolved);
        }
}

#ifdef CONFIG_PFAULT 
/*
 * 'pfault' pseudo page faults routines.
 */
static int pfault_disable = 0;

static int __init nopfault(char *str)
{
	pfault_disable = 1;
	return 1;
}

__setup("nopfault", nopfault);

typedef struct {
	__u16 refdiagc;
	__u16 reffcode;
	__u16 refdwlen;
	__u16 refversn;
	__u64 refgaddr;
	__u64 refselmk;
	__u64 refcmpmk;
	__u64 reserved;
} __attribute__ ((packed)) pfault_refbk_t;

int pfault_init(void)
{
	pfault_refbk_t refbk =
	{ 0x258, 0, 5, 2, __LC_KERNEL_STACK, 1ULL << 48, 1ULL << 48, 0ULL };
        int rc;

	if (pfault_disable)
		return -1;
        __asm__ __volatile__(
                "    diag  %1,%0,0x258\n"
		"0:  j     2f\n"
		"1:  la    %0,8\n"
		"2:\n"
		".section __ex_table,\"a\"\n"
		"   .align 4\n"
		"   .long  0b,1b\n"
		".previous"
                : "=d" (rc) : "a" (&refbk) : "cc" );
        __ctl_set_bit(0, 9);
        return rc;
}

void pfault_fini(void)
{
	pfault_refbk_t refbk =
	{ 0x258, 1, 5, 2, 0ULL, 0ULL, 0ULL, 0ULL };

	if (pfault_disable)
		return;
	__ctl_clear_bit(0,9);
        __asm__ __volatile__(
                "    diag  %0,0,0x258\n"
		"0:\n"
		".section __ex_table,\"a\"\n"
		"   .align 4\n"
		"   .long  0b,0b\n"
		".previous"
		: : "a" (&refbk) : "cc" );
}

asmlinkage void
pfault_interrupt(struct pt_regs *regs, __u16 error_code)
{
	struct task_struct *tsk;
	wait_queue_head_t queue;
	wait_queue_head_t *qp;
	__u16 subcode;

	/*
	 * Get the external interruption subcode & pfault
	 * initial/completion signal bit. VM stores this 
	 * in the 'cpu address' field associated with the
         * external interrupt. 
	 */
	subcode = S390_lowcore.cpu_addr;
	if ((subcode & 0xff00) != 0x0200)
		return;

	/*
	 * Get the token (= address of kernel stack of affected task).
	 */
	tsk = (struct task_struct *)
		(*((unsigned long *) __LC_PFAULT_INTPARM) - THREAD_SIZE);
	
	/*
	 * We got all needed information from the lowcore and can
	 * now safely switch on interrupts.
	 */
	if (regs->psw.mask & PSW_PROBLEM_STATE)
		__sti();

	if (subcode & 0x0080) {
		/* signal bit is set -> a page has been swapped in by VM */
		qp = (wait_queue_head_t *)
			xchg(&tsk->thread.pfault_wait, -1);
		if (qp != NULL) {
			/* Initial interrupt was faster than the completion
			 * interrupt. pfault_wait is valid. Set pfault_wait
			 * back to zero and wake up the process. This can
			 * safely be done because the task is still sleeping
			 * and can't procude new pfaults. */
			tsk->thread.pfault_wait = 0ULL;
			wake_up(qp);
		}
	} else {
		/* signal bit not set -> a real page is missing. */
                init_waitqueue_head (&queue);
		qp = (wait_queue_head_t *)
			xchg(&tsk->thread.pfault_wait, (addr_t) &queue);
		if (qp != NULL) {
			/* Completion interrupt was faster than the initial
			 * interrupt (swapped in a -1 for pfault_wait). Set
			 * pfault_wait back to zero and exit. This can be
			 * done safely because tsk is running in kernel 
			 * mode and can't produce new pfaults. */
			tsk->thread.pfault_wait = 0ULL;
		}

                /* go to sleep */
                wait_event(queue, tsk->thread.pfault_wait == 0ULL);
	}
}
#endif

