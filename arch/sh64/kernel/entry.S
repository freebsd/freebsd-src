/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/entry.S
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sys.h>

#include <asm/processor.h>
#include <asm/registers.h>
#include <asm/unistd.h>

/*
 * A few defines that ought to come from sched.h referring
 * to the task structure. Byte offsets within the task
 * structure and related flags.
 */
#define flags		4
#define sigpending	8
#define need_resched	20
#define ptrace		24

#define PT_TRACESYS	0x00000002


/*
 * SR fields.
 */
#define SR_ASID_MASK	0x00ff0000
#define SR_FD_MASK	0x00008000
#define SR_SS		0x08000000
#define SR_BL		0x10000000
#define SR_MD		0x40000000
#define SR_MMU		0x80000000

/*
 * Event code.
 */
#define	EVENT_INTERRUPT		0
#define	EVENT_FAULT_TLB		1
#define	EVENT_FAULT_NOT_TLB	2
#define	EVENT_DEBUG		3

/* EXPEVT values */
#define	RESET_CAUSE		0x20
#define DEBUGSS_CAUSE		0x980

/*
 * Frame layout. Quad index.
 */
#define	FRAME_T(x)	FRAME_TBASE+(x*8)
#define	FRAME_R(x)	FRAME_RBASE+(x*8)
#define	FRAME_S(x)	FRAME_SBASE+(x*8)
#define FSPC		0
#define FSSR		1
#define FSYSCALL_ID	2

/* Arrange the save frame to be a multiple of 32 bytes long */
#define FRAME_SBASE	0
#define FRAME_RBASE	(FRAME_SBASE+(3*8))	/* SYSCALL_ID - SSR - SPC */
#define FRAME_TBASE	(FRAME_RBASE+(63*8))	/* r0 - r62 */
#define FRAME_PBASE	(FRAME_TBASE+(8*8))	/* t0 -t7 */
#define	FRAME_SIZE	(FRAME_PBASE+(2*8))	/* pad0-pad1 */

#define FP_FRAME_SIZE	FP_FRAME_BASE+(33*8)	/* dr0 - dr31 + fpscr */
#define FP_FRAME_BASE	0

#define	SAVED_R2	0*8
#define	SAVED_R3	1*8
#define	SAVED_R4	2*8
#define	SAVED_R5	3*8
#define	SAVED_R18	4*8
#define	SAVED_R6	5*8
#define	SAVED_T0	6*8

/* These are the registers saved in the TLB path that aren't saved in the first
   level of the normal one. */
#define	TLB_SAVED_R25	7*8
#define	TLB_SAVED_T1	8*8
#define	TLB_SAVED_T2	9*8
#define	TLB_SAVED_T3	10*8
#define	TLB_SAVED_T4	11*8
/* Save R0/R1 : PT-migrating compiler currently dishounours -ffixed-r0 and -ffixed-r1 causing
   breakage otherwise. */
#define	TLB_SAVED_R0	12*8
#define	TLB_SAVED_R1	13*8

#define STI()				\
	getcon	SR, r6;			\
	andi	r6, ~0xf0, r6;		\
	putcon	r6, SR;

	.section	.data, "aw"

#define FAST_TLBMISS_STACK_CACHELINES 4
#define FAST_TLBMISS_STACK_QUADWORDS (4*FAST_TLBMISS_STACK_CACHELINES)

/* Register back-up area for all exceptions */
	.balign	32
	/* Allow for 16 quadwords to be pushed by fast tlbmiss handling
	 * register saves etc. */
	.fill FAST_TLBMISS_STACK_QUADWORDS, 8, 0x0
/* This is 32 byte aligned by construction */
/* Register back-up area for all exceptions */
reg_save_area:
	.quad	0
	.quad	0
	.quad	0
	.quad	0

	.quad	0
	.quad	0
	.quad	0
	.quad	0

	.quad	0
	.quad	0
	.quad	0
	.quad	0

	.quad	0
	.quad   0

/* Save area for RESVEC exceptions. We cannot use reg_save_area because of
 * reentrancy. Note this area may be accessed via physical address.
 * Align so this fits a whole single cache line, for ease of purging.
 */
	.balign 32,0,32
resvec_save_area:
	.quad	0
	.quad	0
	.quad	0
	.quad	0
	.quad	0
	.balign 32,0,32

/* Jump table of 3rd level handlers  */
trap_jtable:
	.long	do_exception_error		/* 0x000 */
	.long	do_exception_error		/* 0x020 */
	.long	tlb_miss_load				/* 0x040 */
	.long	tlb_miss_store				/* 0x060 */
	.long	do_exception_error		/* 0x080 */
	.long	tlb_miss_load				/* 0x0A0 */
	.long	tlb_miss_store				/* 0x0C0 */
	.long	do_address_error_load	/* 0x0E0 */
	.long	do_address_error_store	/* 0x100 */
#ifndef CONFIG_NOFPU_SUPPORT
	.long	do_fpu_error		/* 0x120 */
#else
	.long	do_exception_error		/* 0x120 */
#endif
	.long	do_exception_error		/* 0x140 */
	.long	system_call				/* 0x160 */
	.long	do_reserved_inst		/* 0x180 */
	.long	do_illegal_slot_inst	/* 0x1A0 */
	.long	do_NMI			/* 0x1C0 */
	.long	do_exception_error		/* 0x1E0 */
	.rept 15
		.long do_IRQ		/* 0x200 - 0x3C0 */
	.endr
	.long	do_exception_error		/* 0x3E0 */
	.rept 32
		.long do_IRQ		/* 0x400 - 0x7E0 */
	.endr
	.long	fpu_error_or_IRQA			/* 0x800 */
	.long	fpu_error_or_IRQB			/* 0x820 */
	.long	do_IRQ			/* 0x840 */
	.long	do_IRQ			/* 0x860 */
	.rept 6
		.long do_exception_error	/* 0x880 - 0x920 */
	.endr
	.long	do_software_break_point	/* 0x940 */
	.long	do_exception_error		/* 0x960 */
	.long	do_single_step		/* 0x980 */

	.rept 3
		.long do_exception_error	/* 0x9A0 - 0x9E0 */
	.endr
	.long	do_IRQ			/* 0xA00 */
	.long	do_IRQ			/* 0xA20 */
	.long	itlb_miss_or_IRQ			/* 0xA40 */
	.long	do_IRQ			/* 0xA60 */
	.long	do_IRQ			/* 0xA80 */
	.long	itlb_miss_or_IRQ			/* 0xAA0 */
	.long	do_exception_error		/* 0xAC0 */
	.long	do_address_error_exec	/* 0xAE0 */
	.rept 8
		.long do_exception_error	/* 0xB00 - 0xBE0 */
	.endr
	.rept 18
		.long do_IRQ		/* 0xC00 - 0xE20 */
	.endr


/* System calls jump table */

.globl  sys_call_table
sys_call_table:
	.long sys_ni_syscall	/* 0  -  old "setup()" system call  */
	.long sys_exit
	.long sys_fork
	.long sys_read
	.long sys_write
	.long sys_open		/* 5 */
	.long sys_close
	.long sys_waitpid
	.long sys_creat
	.long sys_link
	.long sys_unlink		/* 10 */
	.long sys_execve
	.long sys_chdir
	.long sys_time
	.long sys_mknod
	.long sys_chmod		/* 15 */
	.long sys_lchown16
	.long sys_ni_syscall	/* old break syscall holder */
	.long sys_stat
	.long sys_lseek
	.long sys_getpid		/* 20 */
	.long sys_mount
	.long sys_oldumount
	.long sys_setuid16
	.long sys_getuid16
	.long sys_stime		/* 25 */
	.long sys_ptrace
	.long sys_alarm
	.long sys_fstat
	.long sys_pause
	.long sys_utime		/* 30 */
	.long sys_ni_syscall	/* old stty syscall holder */
	.long sys_ni_syscall	/* old gtty syscall holder */
	.long sys_access
	.long sys_nice
	.long sys_ni_syscall	/* 35 */
						/* old ftime syscall holder */
	.long sys_sync
	.long sys_kill
	.long sys_rename
	.long sys_mkdir
	.long sys_rmdir		/* 40 */
	.long sys_dup
	.long sys_pipe
	.long sys_times
	.long sys_ni_syscall	/* old prof syscall holder */
	.long sys_brk		/* 45 */
	.long sys_setgid16
	.long sys_getgid16
	.long sys_signal
	.long sys_geteuid16
	.long sys_getegid16	/* 50 */
	.long sys_acct
	.long sys_umount		/* recycled never used phys( */
	.long sys_ni_syscall	/* old lock syscall holder */
	.long sys_ioctl
	.long sys_fcntl		/* 55 */
	.long sys_ni_syscall	/* old mpx syscall holder */
	.long sys_setpgid
	.long sys_ni_syscall	/* old ulimit syscall holder */
	.long sys_ni_syscall	/* sys_olduname */
	.long sys_umask		/* 60 */
	.long sys_chroot
	.long sys_ustat
	.long sys_dup2
	.long sys_getppid
	.long sys_getpgrp		/* 65 */
	.long sys_setsid
	.long sys_sigaction
	.long sys_sgetmask
	.long sys_ssetmask
	.long sys_setreuid16	/* 70 */
	.long sys_setregid16
	.long sys_sigsuspend
	.long sys_sigpending
	.long sys_sethostname
	.long sys_setrlimit	/* 75 */
	.long sys_old_getrlimit
	.long sys_getrusage
	.long sys_gettimeofday
	.long sys_settimeofday
	.long sys_getgroups16	/* 80 */
	.long sys_setgroups16
	.long sys_ni_syscall	/* sys_oldselect */
	.long sys_symlink
	.long sys_lstat
	.long sys_readlink		/* 85 */
	.long sys_uselib
	.long sys_swapon
	.long sys_reboot
	.long old_readdir
	.long old_mmap		/* 90 */
	.long sys_munmap
	.long sys_truncate
	.long sys_ftruncate
	.long sys_fchmod
	.long sys_fchown16		/* 95 */
	.long sys_getpriority
	.long sys_setpriority
	.long sys_ni_syscall	/* old profil syscall holder */
	.long sys_statfs
	.long sys_fstatfs		/* 100 */
	.long sys_ni_syscall	/* ioperm */
	.long sys_socketcall	/* Obsolete implementation of socket syscall */
	.long sys_syslog
	.long sys_setitimer
	.long sys_getitimer	/* 105 */
	.long sys_newstat
	.long sys_newlstat
	.long sys_newfstat
	.long sys_uname
	.long sys_ni_syscall	/* 110 */ /* iopl */
	.long sys_vhangup
	.long sys_ni_syscall	/* idle */
	.long sys_ni_syscall	/* vm86old */
	.long sys_wait4
	.long sys_swapoff		/* 115 */
	.long sys_sysinfo
	.long sys_ipc		/* Obsolete ipc syscall implementation */
	.long sys_fsync
	.long sys_sigreturn
	.long sys_clone		/* 120 */
	.long sys_setdomainname
	.long sys_newuname
	.long sys_ni_syscall	/* sys_modify_ldt */
	.long sys_adjtimex
	.long sys_mprotect		/* 125 */
	.long sys_sigprocmask
	.long sys_create_module
	.long sys_init_module
	.long sys_delete_module
	.long sys_get_kernel_syms	/* 130 */
	.long sys_quotactl
	.long sys_getpgid
	.long sys_fchdir
	.long sys_bdflush
	.long sys_sysfs		/* 135 */
	.long sys_personality
	.long sys_ni_syscall	/* for afs_syscall */
	.long sys_setfsuid16
	.long sys_setfsgid16
	.long sys_llseek		/* 140 */
	.long sys_getdents
	.long sys_select
	.long sys_flock
	.long sys_msync
	.long sys_readv		/* 145 */
	.long sys_writev
	.long sys_getsid
	.long sys_fdatasync
	.long sys_sysctl
	.long sys_mlock		/* 150 */
	.long sys_munlock
	.long sys_mlockall
	.long sys_munlockall
	.long sys_sched_setparam
	.long sys_sched_getparam   	/* 155 */
	.long sys_sched_setscheduler
	.long sys_sched_getscheduler
	.long sys_sched_yield
	.long sys_sched_get_priority_max
	.long sys_sched_get_priority_min  /* 160 */
	.long sys_sched_rr_get_interval
	.long sys_nanosleep
	.long sys_mremap
	.long sys_setresuid16
	.long sys_getresuid16	/* 165 */
	.long sys_ni_syscall	/* vm86 */
	.long sys_query_module
	.long sys_poll
	.long sys_nfsservctl
	.long sys_setresgid16	/* 170 */
	.long sys_getresgid16
	.long sys_prctl
	.long sys_rt_sigreturn
	.long sys_rt_sigaction
	.long sys_rt_sigprocmask	/* 175 */
	.long sys_rt_sigpending
	.long sys_rt_sigtimedwait
	.long sys_rt_sigqueueinfo
	.long sys_rt_sigsuspend
	.long sys_pread		/* 180 */
	.long sys_pwrite
	.long sys_chown16
	.long sys_getcwd
	.long sys_capget
	.long sys_capset       	/* 185 */
	.long sys_sigaltstack
	.long sys_sendfile
	.long sys_ni_syscall	/* streams1 */
	.long sys_ni_syscall	/* streams2 */
	.long sys_vfork        	/* 190 */
	.long sys_getrlimit
	.long sys_mmap2
	.long sys_truncate64
	.long sys_ftruncate64
	.long sys_stat64		/* 195 */
	.long sys_lstat64
	.long sys_fstat64
	.long sys_lchown
	.long sys_getuid
	.long sys_getgid		/* 200 */
	.long sys_geteuid
	.long sys_getegid
	.long sys_setreuid
	.long sys_setregid
	.long sys_getgroups	/* 205 */
	.long sys_setgroups
	.long sys_fchown
	.long sys_setresuid
	.long sys_getresuid
	.long sys_setresgid	/* 210 */
	.long sys_getresgid
	.long sys_chown
	.long sys_setuid
	.long sys_setgid
	.long sys_setfsuid		/* 215 */
	.long sys_setfsgid
	.long sys_pivot_root
	.long sys_mincore
	.long sys_madvise
	.long sys_socket		/* 220 */
	.long sys_bind
	.long sys_connect
	.long sys_listen
	.long sys_accept
	.long sys_getsockname	/* 225 */
	.long sys_getpeername
	.long sys_socketpair
	.long sys_send
	.long sys_sendto
	.long sys_recv		/* 230*/
	.long sys_recvfrom
	.long sys_shutdown
	.long sys_setsockopt
	.long sys_getsockopt
	.long sys_sendmsg		/* 235 */
	.long sys_recvmsg
	.long sys_semop		/* New ipc syscall implementation */
	.long sys_semget
	.long sys_semctl
	.long sys_msgsnd		/* 240 */
	.long sys_msgrcv
	.long sys_msgget
	.long sys_msgctl
	.long sys_shmatcall
	.long sys_shmdt		/* 245 */
	.long sys_shmget
	.long sys_shmctl

	/*
	 * NOTE!! This doesn't have to be exact - we just have
	 * to make sure we have _enough_ of the "sys_ni_syscall"
	 * entries. Don't panic if you notice that this hasn't
	 * been shrunk every time we add a new system call.
	 */
	.rept NR_syscalls-247
		.long sys_ni_syscall
	.endr

	.section	.text64, "ax"

/*
 * --- Exception/Interrupt/Event Handling Section
 */

/*
 * VBR and RESVEC blocks.
 *
 * First level handler for VBR-based exceptions.
 *
 * To avoid waste of space, align to the maximum text block size.
 * This is assumed to be at most 128 bytes or 32 instructions.
 * DO NOT EXCEED 32 instructions on the first level handlers !
 *
 * Also note that RESVEC is contained within the VBR block
 * where the room left (1KB - TEXT_SIZE) allows placing
 * the RESVEC block (at most 512B + TEXT_SIZE). 
 * 
 * So first (and only) level handler for RESVEC-based exceptions.
 *
 * Where the fault/interrupt is handled (not_a_tlb_miss, tlb_miss
 * and interrupt) we are a lot tight with register space until
 * saving onto the stack frame, which is done in handle_exception().
 *
 */

#define	TEXT_SIZE 	128
#define	BLOCK_SIZE 	1664 		/* Dynamic check, 13*128 */

	.balign TEXT_SIZE
LVBR_block:
	.space	256, 0			/* Power-on class handler, */
					/* not required here       */
not_a_tlb_miss:
	/* Save original stack pointer into KCR1 */
	putcon	SP, KCR1

	/* Save other original registers into reg_save_area */
        _loada  reg_save_area, SP
	st.q	SP, SAVED_R2, r2
	st.q	SP, SAVED_R3, r3
	st.q	SP, SAVED_R4, r4
	st.q	SP, SAVED_R5, r5
	st.q	SP, SAVED_R6, r6
	st.q	SP, SAVED_R18, r18
	gettr	t0, r3
	st.q	SP, SAVED_T0, r3

#ifdef CONFIG_SH64_PAGE_TABLE_AUDIT
	/* This use of DCR is not really legal, but there's nowhere else convenient to store this and it's only for last-resort debug anyway. */
	movi    0x100, r3
	putcon  r3, dcr
#endif
	
	/* Set args for Non-debug, Not a TLB miss class handler */
	getcon	EXPEVT, r2
	_loada	ret_from_exception, r3
	ori	r3, 1, r3
	movi	EVENT_FAULT_NOT_TLB, r4
	or	SP, ZERO, r5
	getcon	KCR1, SP
	_ptar	handle_exception, t0
	blink	t0, ZERO

	/*
	 * Instead of the natural .balign 1024 place RESVEC here
	 * respecting the final 1KB alignment.
	 */
	.balign TEXT_SIZE
	/*
	 * Instead of '.space 1024-TEXT_SIZE' place the RESVEC
	 * block making sure the final alignment is correct.
	 */
LRESVEC_block:
	/* Panic handler. Called with MMU off. Possible causes/actions:
	 * - Reset:		Jump to program start.
	 * - Single Step:	Turn off Single Step & return.
	 * - Others:		Call panic handler, passing PC as arg.
	 *			(this may need to be extended...)
	 */
reset_or_panic:
	putcon	SP, DCR
	/* First save r0-1 and tr0, as we need to use these */
	_loada	resvec_save_area-CONFIG_CACHED_MEMORY_OFFSET, SP
	st.q	SP, 0, r0
	st.q	SP, 8, r1
	gettr	t0, r0
	st.q	SP, 32, r0
	
	/* Check cause */
	getcon	EXPEVT, r0
	movi	RESET_CAUSE, r1
	sub	r1, r0, r1		/* r1=0 if reset */
	_loada	_stext-CONFIG_CACHED_MEMORY_OFFSET, r0
	ori	r0, 1, r0
	ptabs	r0, t0
	beqi	r1, 0, t0		/* Jump to start address if reset */

	getcon	EXPEVT, r0
	movi	DEBUGSS_CAUSE, r1
	sub	r1, r0, r1		/* r1=0 if single step */
	_ptar	single_step_panic, t0
	beqi	r1, 0, t0		/* jump if single step */

	/* If we get here, we have an unknown panic. Just call the panic
	 * handler, passing saved PC. We never expect to return, so we can
	 * use any regs now. */
	getcon	SPC,r2
	getcon	SSR,r3
	getcon	EXPEVT,r4
	/* Prepare to jump to C - physical address */
	_loada	panic_handler-CONFIG_CACHED_MEMORY_OFFSET, r1
	ori	r1, 1, r1
	ptabs   r1, t0
	getcon	DCR, SP
	blink	t0, ZERO

single_step_panic:
	/* We are in a handler with Single Step set. We need to resume the
	 * handler, by turning on MMU & turning off Single Step. */
	getcon	SSR, r0
	movi	SR_MMU, r1
	or	r0, r1, r0
	movi	~SR_SS, r1
	and	r0, r1, r0
	putcon	r0, SSR
	/* Restore EXPEVT, as the rte won't do this */
	getcon	PEXPEVT, r0
	putcon	r0, EXPEVT
	/* Restore regs */
	ld.q	SP, 32, r0
	ptabs	r0, tr0
	ld.q	SP, 0, r0
	ld.q	SP, 8, r1
	getcon	DCR, SP
	synco
	rte

	.balign	TEXT_SIZE
debug_exception:
	/*
	 * Single step/software_break_point first level handler.
	 * Called with MMU off, so the first thing we do is enable it
	 * by doing an rte with appropriate SSR.
	 */
	putcon	SP, DCR
	/* Save SSR & SPC, together with R0 & R1, as we need to use 2 regs. */
	_loada	resvec_save_area-CONFIG_CACHED_MEMORY_OFFSET, SP

	/* With the MMU off, we are bypassing the cache, so purge any 
         * data that will be made stale by the following stores.
         */
	ocbp	SP, 0
	synco

	st.q	SP, 0, r0
	st.q	SP, 8, r1
	getcon	SPC, r0
	st.q	SP, 16, r0
	getcon	SSR, r0
	st.q	SP, 24, r0

	/* Enable MMU, block exceptions, set priv mode, disable single step */
	movi	SR_MMU | SR_BL | SR_MD, r1
	or	r0, r1, r0
	movi	~SR_SS, r1
	and	r0, r1, r0
	putcon	r0, SSR
	/* Force control to debug_exception_2 when rte is executed */
	_loada	debug_exeception_2, r0
	ori	r0, 1, r0      /* force SHmedia, just in case */
	putcon	r0, SPC
	getcon	DCR, SP
	synco
	rte
debug_exeception_2:
	/* Restore saved regs */
	putcon	SP, KCR1
	_loada	resvec_save_area, SP
	ld.q	SP, 24, r0
	putcon	r0, SSR
	ld.q	SP, 16, r0
	putcon	r0, SPC
	ld.q	SP, 0, r0
	ld.q	SP, 8, r1

	/* Save other original registers into reg_save_area */
        _loada  reg_save_area, SP
	st.q	SP, SAVED_R2, r2
	st.q	SP, SAVED_R3, r3
	st.q	SP, SAVED_R4, r4
	st.q	SP, SAVED_R5, r5
	st.q	SP, SAVED_R6, r6
	st.q	SP, SAVED_R18, r18
	gettr	t0, r3
	st.q	SP, SAVED_T0, r3

	/* Set args for debug class handler */
	getcon	EXPEVT, r2
	_loada	ret_from_exception, r3
	ori	r3, 1, r3
	movi	EVENT_DEBUG, r4
	or	SP, ZERO, r5
	getcon	KCR1, SP
	_ptar	handle_exception, t0
	blink	t0, ZERO

	.balign	TEXT_SIZE
debug_interrupt:
	/*
	 * Not supported. If we ever get here loop forever
	 * We may be MMUOFF or MMUON. Just use pic code.
	 */
	_ptar	debug_interrupt, t0
	blink	t0, ZERO
	.balign	TEXT_SIZE

LRESVEC_block_end:			/* Marker. Unused. */
tlb_miss:
	putcon	SP, KCR1
	_loada	reg_save_area, SP
	/* SP is guaranteed 32-byte aligned. */
	st.q	SP, TLB_SAVED_R0 , r0
	st.q	SP, TLB_SAVED_R1 , r1
	st.q	SP, SAVED_R2 , r2
	st.q	SP, SAVED_R3 , r3
	st.q	SP, SAVED_R4 , r4
	st.q	SP, SAVED_R5 , r5
	st.q	SP, SAVED_R6 , r6
	st.q	SP, SAVED_R18, r18

	/* Save R25 for safety; as/ld may want to use it to achieve the call to
	 * the code in mm/tlbmiss.c */
	st.q	SP, TLB_SAVED_R25, r25
	gettr	tr0, r2
	gettr	tr1, r3
	gettr	tr2, r4
	gettr	tr3, r5
	gettr	tr4, r18
	st.q	SP, SAVED_T0 , r2
	st.q	SP, TLB_SAVED_T1 , r3
	st.q	SP, TLB_SAVED_T2 , r4
	st.q	SP, TLB_SAVED_T3 , r5
	st.q	SP, TLB_SAVED_T4 , r18

	pt	do_fast_page_fault, tr0
	getcon	SSR, r2
	getcon	EXPEVT, r3
	getcon	TEA, r4
	shlri	r2, 30, r2
	andi	r2, 1, r2	/* r2 = SSR.MD */
	blink 	tr0, r18

	pt	fixup_to_invoke_general_handler, tr1

	/* If the fast path handler fixed the fault, just drop through quickly
	   to the restore code right away to return to the excepting context.
	   */
	beqi/u	r2, 0, tr1

fast_tlb_miss_restore:
	ld.q	SP, SAVED_T0, r2
	ld.q	SP, TLB_SAVED_T1, r3
	ld.q	SP, TLB_SAVED_T2, r4

	ld.q	SP, TLB_SAVED_T3, r5
	ld.q	SP, TLB_SAVED_T4, r18

	ptabs	r2, tr0
	ptabs	r3, tr1
	ptabs	r4, tr2
	ptabs	r5, tr3
	ptabs	r18, tr4

	ld.q	SP, TLB_SAVED_R0, r0
	ld.q	SP, TLB_SAVED_R1, r1
	ld.q	SP, SAVED_R2, r2
	ld.q	SP, SAVED_R3, r3
	ld.q	SP, SAVED_R4, r4
	ld.q	SP, SAVED_R5, r5
	ld.q	SP, SAVED_R6, r6
	ld.q	SP, SAVED_R18, r18
	ld.q	SP, TLB_SAVED_R25, r25

	getcon	KCR1, SP
	rte
	nop /* for safety, in case the code is run on sh5-101 cut1.x */

fixup_to_invoke_general_handler:

	/* OK, new method.  Restore stuff that's not expected to get saved into
	   the 'first-level' reg save area, then just fall through to setting
	   up the registers and calling the second-level handler. */

	/* 2nd level expects r2,3,4,5,6,18,tr0 to be saved.  So we must restore
	   r25,tr1-4 and save r6 to get into the right state.  */

	ld.q	SP, TLB_SAVED_T1, r3
	ld.q	SP, TLB_SAVED_T2, r4
	ld.q	SP, TLB_SAVED_T3, r5
	ld.q	SP, TLB_SAVED_T4, r18
	ld.q	SP, TLB_SAVED_R25, r25
	
	ld.q	SP, TLB_SAVED_R0, r0
	ld.q	SP, TLB_SAVED_R1, r1

	ptabs/u	r3, tr1
	ptabs/u	r4, tr2
	ptabs/u	r5, tr3
	ptabs/u	r18, tr4

#ifdef CONFIG_SH64_PAGE_TABLE_AUDIT
	/* This use of DCR is not really legal, but there's nowhere else convenient to store this and it's only for last-resort debug anyway. */
	movi    0x400, r3
	putcon  r3, dcr
#endif

	/* Set args for Non-debug, TLB miss class handler */
	getcon	EXPEVT, r2
	_loada	ret_from_exception, r3
	ori	r3, 1, r3
	movi	EVENT_FAULT_TLB, r4
	or	SP, ZERO, r5
	getcon	KCR1, SP
	_ptar	handle_exception, t0
	blink	t0, ZERO

/* NB TAKE GREAT CARE HERE TO ENSURE THAT THE INTERRUPT CODE
   DOES END UP AT VBR+0x600 */
	nop
	nop
	nop
	nop
	nop
	nop
#if 0
	.balign 256
	nop
#endif
	.balign 256
	/* VBR + 0x600 */

interrupt:
	/* Save original stack pointer into KCR1 */
	putcon	SP, KCR1

	/* Save other original registers into reg_save_area */
        _loada  reg_save_area, SP
	st.q	SP, SAVED_R2, r2
	st.q	SP, SAVED_R3, r3
	st.q	SP, SAVED_R4, r4
	st.q	SP, SAVED_R5, r5
	st.q	SP, SAVED_R6, r6
	st.q	SP, SAVED_R18, r18
	gettr	t0, r3
	st.q	SP, SAVED_T0, r3

#ifdef CONFIG_SH64_PAGE_TABLE_AUDIT
	/* This use of DCR is not really legal, but there's nowhere else convenient to store this and it's only for last-resort debug anyway. */
	movi    0x600, r3
	putcon  r3, dcr
#endif

	/* Set args for interrupt class handler */
	getcon	INTEVT, r2
	_loada	ret_from_irq, r3
	ori	r3, 1, r3
	movi	EVENT_INTERRUPT, r4
	or	SP, ZERO, r5
	getcon	KCR1, SP
	_ptar	handle_exception, t0
	blink	t0, ZERO
	.balign	TEXT_SIZE		/* let's waste the bare minimum */

LVBR_block_end:				/* Marker. Used for total checking */
	

/*
 * Second level handler for VBR-based exceptions. Pre-handler.
 * In common to all stack-frame sensitive handlers.
 *
 * Inputs:
 * (KCR0) Current [current task union]
 * (KCR1) Original SP
 * (r2)   INTEVT/EXPEVT
 * (r3)   appropriate return address
 * (r4)   Event (0 = interrupt, 1 = TLB miss fault, 2 = Not TLB miss fault, 3=debug)
 * (r5)   Pointer to reg_save_area
 * (SP)   Original SP
 *
 * Available registers:
 * (r6)
 * (r18)
 * (t0)
 *
 */
handle_exception:
	/* Common 2nd level handler. */

	/* First thing we need an appropriate stack pointer */
	getcon	SSR, r6
	shlri	r6, 30, r6
	andi	r6, 1, r6
	_ptar	stack_ok, t0
	bne	r6, ZERO, t0		/* Original stack pointer is fine */

	/* Set stack pointer for user fault */
	getcon	KCR0, SP
	movi	THREAD_SIZE, r6		/* Point to the end */
	add	SP, r6, SP

stack_ok:
	/* Make some room for the BASIC frame. */
	movi	-(FRAME_SIZE), r6
	add	SP, r6, SP

/* Could do this with no stalling if we had another spare register, but the
   code below will be OK. */
	ld.q	r5, SAVED_R2, r6
	ld.q	r5, SAVED_R3, r18
	st.q	SP, FRAME_R(2), r6
	ld.q	r5, SAVED_R4, r6
	st.q	SP, FRAME_R(3), r18
	ld.q	r5, SAVED_R5, r18
	st.q	SP, FRAME_R(4), r6
	ld.q	r5, SAVED_R6, r6
	st.q	SP, FRAME_R(5), r18
	ld.q	r5, SAVED_R18, r18
	st.q	SP, FRAME_R(6), r6
	ld.q	r5, SAVED_T0, r6
	st.q	SP, FRAME_R(18), r18
	st.q	SP, FRAME_T(0), r6

	/* Keep old SP around */
	getcon	KCR1, r6

	/* Save the rest of the general purpose registers */
	st.q	SP, FRAME_R(0), r0
	st.q	SP, FRAME_R(1), r1
	st.q	SP, FRAME_R(7), r7
	st.q	SP, FRAME_R(8), r8
	st.q	SP, FRAME_R(9), r9
	st.q	SP, FRAME_R(10), r10
	st.q	SP, FRAME_R(11), r11
	st.q	SP, FRAME_R(12), r12
	st.q	SP, FRAME_R(13), r13
	st.q	SP, FRAME_R(14), r14

	/* SP is somewhere else */
	st.q	SP, FRAME_R(15), r6

	st.q	SP, FRAME_R(16), r16
	st.q	SP, FRAME_R(17), r17
	/* r18 is saved earlier. */
	st.q	SP, FRAME_R(19), r19
	st.q	SP, FRAME_R(20), r20
	st.q	SP, FRAME_R(21), r21
	st.q	SP, FRAME_R(22), r22
	st.q	SP, FRAME_R(23), r23
	st.q	SP, FRAME_R(24), r24
	st.q	SP, FRAME_R(25), r25
	st.q	SP, FRAME_R(26), r26
	st.q	SP, FRAME_R(27), r27
	st.q	SP, FRAME_R(28), r28
	st.q	SP, FRAME_R(29), r29
	st.q	SP, FRAME_R(30), r30
	st.q	SP, FRAME_R(31), r31
	st.q	SP, FRAME_R(32), r32
	st.q	SP, FRAME_R(33), r33
	st.q	SP, FRAME_R(34), r34
	st.q	SP, FRAME_R(35), r35
	st.q	SP, FRAME_R(36), r36
	st.q	SP, FRAME_R(37), r37
	st.q	SP, FRAME_R(38), r38
	st.q	SP, FRAME_R(39), r39
	st.q	SP, FRAME_R(40), r40
	st.q	SP, FRAME_R(41), r41
	st.q	SP, FRAME_R(42), r42
	st.q	SP, FRAME_R(43), r43
	st.q	SP, FRAME_R(44), r44
	st.q	SP, FRAME_R(45), r45
	st.q	SP, FRAME_R(46), r46
	st.q	SP, FRAME_R(47), r47
	st.q	SP, FRAME_R(48), r48
	st.q	SP, FRAME_R(49), r49
	st.q	SP, FRAME_R(50), r50
	st.q	SP, FRAME_R(51), r51
	st.q	SP, FRAME_R(52), r52
	st.q	SP, FRAME_R(53), r53
	st.q	SP, FRAME_R(54), r54
	st.q	SP, FRAME_R(55), r55
	st.q	SP, FRAME_R(56), r56
	st.q	SP, FRAME_R(57), r57
	st.q	SP, FRAME_R(58), r58
	st.q	SP, FRAME_R(59), r59
	st.q	SP, FRAME_R(60), r60
	st.q	SP, FRAME_R(61), r61
	st.q	SP, FRAME_R(62), r62

	/*
	 * Save the S* registers.
	 */
	getcon	SSR, r61
	st.q	SP, FRAME_S(FSSR), r61
	getcon	SPC, r62
	st.q	SP, FRAME_S(FSPC), r62
	movi	-1, r62			/* Reset syscall_nr */
	st.q	SP, FRAME_S(FSYSCALL_ID), r62

	/* Save the rest of the target registers */
	gettr	t1, r6
	st.q	SP, FRAME_T(1), r6
	gettr	t2, r6
	st.q	SP, FRAME_T(2), r6
	gettr	t3, r6
	st.q	SP, FRAME_T(3), r6
	gettr	t4, r6
	st.q	SP, FRAME_T(4), r6
	gettr	t5, r6
	st.q	SP, FRAME_T(5), r6
	gettr	t6, r6
	st.q	SP, FRAME_T(6), r6
	gettr	t7, r6
	st.q	SP, FRAME_T(7), r6

/*#define POOR_MANS_STRACE 1*/
	
#ifdef POOR_MANS_STRACE
	/* We've pushed all the registers now, so only r2-r4 hold anything
	 * useful. Move them into callee save registers */
	or	r2, ZERO, r28
	or	r3, ZERO, r29
	or	r4, ZERO, r30

	/* Preserve r2 as the event code */
	_loada	evt_debug, r3
	ori	r3, 1, r3
	ptabs	r3, t0

	/* or	SP, ZERO, r5 */
	getcon	TRA, r5
	blink	t0, LINK

	or	r28, ZERO, r2
	or	r29, ZERO, r3
	or	r30, ZERO, r4
#endif

	
	/* For syscall and debug race condition, get TRA now */
	getcon	TRA, r5
	
	/* We are in a safe position to turn SR.BL off, but set IMASK=0xf
	 * Also set FD, to catch FPU usage in the kernel.
	 *
	 * benedict.gaster@superh.com 29/07/2002
	 *
	 * On all SH5-101 revisions it is unsafe to raise the IMASK and at the 
	 * same time change BL from 1->0, as any pending interrupt of a level
	 * higher than he previous value of IMASK will leak through and be 
	 * taken unexpectedly.
	 *
	 * To avoid this we raise the IMASK and then issue another PUTCON to
	 * enable interrupts.
         */
	getcon	SR, r6
	movi	SR_IMASK | SR_FD, r7
	or	r6, r7, r6
	putcon	r6, SR
	movi	SR_UNBLOCK_EXC, r7
	and	r6, r7, r6
	putcon	r6, SR

	
	/* Now call the appropriate 3rd level handler */
	or	r3, ZERO, LINK
	_loada	trap_jtable, r3
	shlri	r2, 3, r2
	ldx.l	r2, r3, r3
	shlri	r2, 2, r2
	ptabs	r3, t0
	or	SP, ZERO, r3
	blink	t0, ZERO

/*
 * Second level handler for VBR-based exceptions. Post-handlers.
 *
 * Post-handlers for interrupts (ret_from_irq), exceptions
 * (ret_from_exception) and common reentrance doors (restore_all
 * to get back to the original context, ret_from_syscall loop to
 * check kernel exiting).
 *
 * ret_with_reschedule and check_signals are an inner lables of
 * the ret_from_syscall loop.
 *
 * In common to all stack-frame sensitive handlers.
 *
 * Inputs:
 * (SP)   struct pt_regs *, original register's frame pointer (basic)
 *
 */
ret_from_irq:
	ld.q	SP, FRAME_S(FSSR), r6
	shlri	r6, 30, r6
	andi	r6, 1, r6
	_ptar	restore_all, t0
	bne	r6, ZERO, t0		/* no further checks */
	STI()
	_ptar	ret_with_reschedule, t0
	blink	t0, ZERO		/* Do not check softirqs */
	
ret_from_exception:
	ld.q	SP, FRAME_S(FSSR), r6
	shlri	r6, 30, r6
	andi	r6, 1, r6
	_ptar	restore_all, t0
	bne	r6, ZERO, t0		/* no further checks */

	/* Check softirqs */

	/*
	 * Fall-through:
	 * _ptar   ret_from_syscall, t0
	 * blink   t0, ZERO
	 */

ret_from_syscall:
	
ret_with_reschedule:
	getcon	KCR0, r6
	ld.l	r6, need_resched, r7
	_ptar	check_signals, t0
	beq	r7, ZERO, t0

	_ptar	ret_from_syscall, t0
	gettr	t0, LINK
	_loada	schedule, r6
	ptabs	r6, t0
	blink	t0, ZERO		/* Call schedule(), return on top */

check_signals:
	getcon	KCR0, r6
	ld.l	r6, sigpending, r7
	_ptar	restore_all, t0
	beq	r7, ZERO, t0

	_loada	do_signal, r6
	ptabs	r6, t0
	or	SP, ZERO, r2
	or	ZERO, ZERO, r3
	blink	t0, LINK	    /* Call do_signal(regs, 0), return here */
	
#ifdef CONFIG_SH64_PAGE_TABLE_AUDIT
	/* Check page tables before returning (with obvious performance penalty). */
	_loada  audit_mm, r6
	ptabs   r6, t0
	getcon  expevt, r2
	getcon  intevt, r3
	getcon  tra, r4
	getcon  dcr, r5
	blink   t0, LINK
#endif

restore_all:
	/* Do prefetches */
	
	ld.q	SP, FRAME_T(0), r6
	ld.q	SP, FRAME_T(1), r7
	ld.q	SP, FRAME_T(2), r8
	ld.q	SP, FRAME_T(3), r9
	ptabs	r6, t0
	ptabs	r7, t1
	ptabs	r8, t2
	ptabs	r9, t3
	ld.q	SP, FRAME_T(4), r6
	ld.q	SP, FRAME_T(5), r7
	ld.q	SP, FRAME_T(6), r8
	ld.q	SP, FRAME_T(7), r9
	ptabs	r6, t4
	ptabs	r7, t5
	ptabs	r8, t6
	ptabs	r9, t7

	ld.q	SP, FRAME_R(0), r0
	ld.q	SP, FRAME_R(1), r1
	ld.q	SP, FRAME_R(2), r2
	ld.q	SP, FRAME_R(3), r3
	ld.q	SP, FRAME_R(4), r4
	ld.q	SP, FRAME_R(5), r5
	ld.q	SP, FRAME_R(6), r6
	ld.q	SP, FRAME_R(7), r7
	ld.q	SP, FRAME_R(8), r8
	ld.q	SP, FRAME_R(9), r9
	ld.q	SP, FRAME_R(10), r10
	ld.q	SP, FRAME_R(11), r11
	ld.q	SP, FRAME_R(12), r12
	ld.q	SP, FRAME_R(13), r13
	ld.q	SP, FRAME_R(14), r14

	ld.q	SP, FRAME_R(16), r16
	ld.q	SP, FRAME_R(17), r17
	ld.q	SP, FRAME_R(18), r18
	ld.q	SP, FRAME_R(19), r19
	ld.q	SP, FRAME_R(20), r20
	ld.q	SP, FRAME_R(21), r21
	ld.q	SP, FRAME_R(22), r22
	ld.q	SP, FRAME_R(23), r23
	ld.q	SP, FRAME_R(24), r24
	ld.q	SP, FRAME_R(25), r25
	ld.q	SP, FRAME_R(26), r26
	ld.q	SP, FRAME_R(27), r27
	ld.q	SP, FRAME_R(28), r28
	ld.q	SP, FRAME_R(29), r29
	ld.q	SP, FRAME_R(30), r30
	ld.q	SP, FRAME_R(31), r31
	ld.q	SP, FRAME_R(32), r32
	ld.q	SP, FRAME_R(33), r33
	ld.q	SP, FRAME_R(34), r34
	ld.q	SP, FRAME_R(35), r35
	ld.q	SP, FRAME_R(36), r36
	ld.q	SP, FRAME_R(37), r37
	ld.q	SP, FRAME_R(38), r38
	ld.q	SP, FRAME_R(39), r39
	ld.q	SP, FRAME_R(40), r40
	ld.q	SP, FRAME_R(41), r41
	ld.q	SP, FRAME_R(42), r42
	ld.q	SP, FRAME_R(43), r43
	ld.q	SP, FRAME_R(44), r44
	ld.q	SP, FRAME_R(45), r45
	ld.q	SP, FRAME_R(46), r46
	ld.q	SP, FRAME_R(47), r47
	ld.q	SP, FRAME_R(48), r48
	ld.q	SP, FRAME_R(49), r49
	ld.q	SP, FRAME_R(50), r50
	ld.q	SP, FRAME_R(51), r51
	ld.q	SP, FRAME_R(52), r52
	ld.q	SP, FRAME_R(53), r53
	ld.q	SP, FRAME_R(54), r54
	ld.q	SP, FRAME_R(55), r55
	ld.q	SP, FRAME_R(56), r56
	ld.q	SP, FRAME_R(57), r57
	ld.q	SP, FRAME_R(58), r58

	getcon	SR, r59
	movi	SR_BLOCK_EXC, r60
	or	r59, r60, r59
	putcon	r59, SR			/* SR.BL = 1, keep nesting out */
	ld.q	SP, FRAME_S(FSSR), r61
	ld.q	SP, FRAME_S(FSPC), r62
	movi	SR_ASID_MASK, r60
	and	r59, r60, r59
	andc	r61, r60, r61		/* Clear out older ASID */
	or	r59, r61, r61		/* Retain current ASID */
	putcon	r61, SSR
	putcon	r62, SPC

	/* Ignore FSYSCALL_ID */

	ld.q	SP, FRAME_R(59), r59
	ld.q	SP, FRAME_R(60), r60
	ld.q	SP, FRAME_R(61), r61
	ld.q	SP, FRAME_R(62), r62

	/* Last touch */
	ld.q	SP, FRAME_R(15), SP
	rte
	nop

/*
 * Third level handlers for VBR-based exceptions. Adapting args to
 * and/or deflecting to fourth level handlers.
 *
 * Fourth level handlers interface.
 * Most are C-coded handlers directly pointed by the trap_jtable.
 * (Third = Fourth level)
 * Inputs:
 * (r2)   fault/interrupt code, entry number (e.g. NMI = 14,
 *	  IRL0-3 (0000) = 16, RTLBMISS = 2, SYSCALL = 11, etc ...)
 * (r3)   struct pt_regs *, original register's frame pointer
 * (r4)   Event (0 = interrupt, 1 = TLB miss fault, 2 = Not TLB miss fault)
 * (r5)   TRA control register (for syscall/debug benefit only)
 * (LINK) return address
 * (SP)   = r3
 *
 * Kernel TLB fault handlers will get a slightly different interface.
 * (r2)   struct pt_regs *, original register's frame pointer
 * (r3)   writeaccess, whether it's a store fault as opposed to load fault
 * (r4)   execaccess, whether it's a ITLB fault as opposed to DTLB fault
 * (r5)   Effective Address of fault
 * (LINK) return address
 * (SP)   = r2
 *
 * fpu_error_or_IRQ? is a helper to deflect to the right cause.
 *
 */
tlb_miss_load:
	or	SP, ZERO, r2
	or	ZERO, ZERO, r3		/* Read */
	or	ZERO, ZERO, r4		/* Data */
	getcon	TEA, r5
	_ptar	call_do_page_fault, t0
	beq	ZERO, ZERO, tr0

tlb_miss_store:
	or	SP, ZERO, r2
	movi	1, r3			/* Write */
	or	ZERO, ZERO, r4		/* Data */
	getcon	TEA, r5
	_ptar	call_do_page_fault, t0
	beq	ZERO, ZERO, tr0

itlb_miss_or_IRQ:
	_ptar	its_IRQ, t0
	beqi/u	r4, EVENT_INTERRUPT, t0
	or	SP, ZERO, r2
	or	ZERO, ZERO, r3		/* Read */
	movi	1, r4			/* Text */
	getcon	TEA, r5
	/* Fall through */

call_do_page_fault:
	_loada	do_page_fault, r6
        ptabs	r6, t0
        blink	t0, ZERO	
	
fpu_error_or_IRQA:
	_ptar	its_IRQ, t0
	beqi/l	r4, EVENT_INTERRUPT, t0
#ifndef CONFIG_NOFPU_SUPPORT 
	_loada	do_fpu_state_restore, r6
#else
	_loada	do_exception_error, r6
#endif
	ptabs	r6, t0
	blink	t0, ZERO

fpu_error_or_IRQB:
	_ptar	its_IRQ, t0
	beqi/l	r4, EVENT_INTERRUPT, t0
#ifndef CONFIG_NOFPU_SUPPORT
	_loada	do_fpu_state_restore, r6
#else
	_loada	do_exception_error, r6
#endif
	ptabs	r6, t0
	blink	t0, ZERO

its_IRQ:
	_loada	do_IRQ, r6
	ptabs	r6, t0
	blink	t0, ZERO

/*
 * system_call/unknown_trap third level handler:
 * 
 * Inputs:
 * (r2)   fault/interrupt code, entry number (TRAP = 11)
 * (r3)   struct pt_regs *, original register's frame pointer
 * (r4)   Not used. Event (0=interrupt, 1=TLB miss fault, 2=Not TLB miss fault)
 * (r5)   TRA Control Reg (0x00xyzzzz: x=1 SYSCALL, y = #args, z=nr)
 * (SP)   = r3
 * (LINK) return address: ret_from_exception
 * (*r3)  Syscall parms: SC#, arg0, arg1, ..., arg5 in order (Saved r2/r7)
 *
 * Outputs:
 * (*r3)  Syscall reply (Saved r2)
 * (LINK) In case of syscall only it can be scrapped.
 *        Common second level post handler will be ret_from_syscall.
 *        Common (non-trace) exit point to that is syscall_ret (saving
 *        result to r2). Common bad exit point is syscall_bad (returning
 *        ENOSYS then saved to r2).
 *
 */

unknown_trap:
	/* Unknown Trap or User Trace */
	_loada	do_unknown_trapa, r6
	ptabs	r6, t0
        ld.q    r3, FRAME_R(9), r2	/* r2 = #arg << 16 | syscall # */
        andi    r2, 0x1ff, r2		/* r2 = syscall # */
	blink	t0, LINK

	_ptar	syscall_ret, t0
	blink	t0, ZERO

        /* New syscall implementation*/
system_call:
	_ptar	unknown_trap, t0
        or      r5, ZERO, r4            /* TRA (=r5) -> r4 */
        shlri   r4, 20, r4
	bnei	r4, 1, t0		/* unknown_trap if not 0x1yzzzz */

        /* It's a system call */
	st.q    r3, FRAME_S(FSYSCALL_ID), r5 	/* ID (0x1yzzzz) -> stack */
	andi    r5, 0x1ff, r5			/* syscall # -> r5	  */

	STI()

	_ptar	syscall_allowed, t0
	movi	NR_syscalls - 1, r4	/* Last valid */
	bgeu/l	r4, r5, t0

syscall_bad:
	/* Return ENOSYS ! */
	movi	-(ENOSYS), r2		/* Fall-through */
syscall_ret:
	st.q	SP, FRAME_R(9), r2	/* Expecting SP back to BASIC frame */

#ifdef POOR_MANS_STRACE
	/* nothing useful in registers at this point */

	_loada	evt_debug2, r5
	ori	r5, 1, r5
	ptabs	r5, t0
	ld.q	SP, FRAME_R(9), r2
	or	SP, ZERO, r3
	blink	t0, LINK
#endif
	
	ld.q	SP, FRAME_S(FSPC), r2
	addi	r2, 4, r2		/* Move PC, being pre-execution event */
	st.q	SP, FRAME_S(FSPC), r2
	_ptar	ret_from_syscall, t0
	blink	t0, ZERO


/*  A different return path for ret_from_fork, because we now need 
 *  to call schedule_tail with the later kernels. Because prev is
 *  loaded into r2 by switch_to() means we can just call it straight  away
 */
	
.global	ret_from_fork
ret_from_fork:

	_loada	schedule_tail,r5
	ori	r5, 1, r5
	ptabs	r5, t0
	blink	t0, LINK
	
#ifdef POOR_MANS_STRACE
	/* nothing useful in registers at this point */

	_loada	evt_debug2, r5
	ori	r5, 1, r5
	ptabs	r5, t0
	ld.q	SP, FRAME_R(9), r2
	or	SP, ZERO, r3
	blink	t0, LINK
#endif
	
	ld.q	SP, FRAME_S(FSPC), r2
	addi	r2, 4, r2		/* Move PC, being pre-execution event */
	st.q	SP, FRAME_S(FSPC), r2
	_ptar	ret_from_syscall, t0
	blink	t0, ZERO
	
	
		
syscall_allowed:
	/* Use LINK to deflect the exit point, default is syscall_ret */
	_ptar	syscall_ret, t0
	gettr	t0, LINK
	_ptar	syscall_notrace, t0

	getcon	KCR0, r2
	ld.l	r2, ptrace, r4
	andi	r4, PT_TRACESYS, r4
	beq/l	r4, ZERO, t0

	/* Trace it by calling syscall_trace before and after */
	_loada	syscall_trace, r4
	ptabs	r4, t0
	blink	t0, LINK
	/* Reload syscall number as r5 is trashed by syscall_trace */
	ld.q	SP, FRAME_S(FSYSCALL_ID), r5
	andi	r5, 0x1ff, r5

	_ptar	syscall_ret_trace, t0
	gettr	t0, LINK

syscall_notrace:
	/* Now point to the appropriate 4th level syscall handler */
	_loada	sys_call_table, r4
	shlli	r5, 2, r5
	ldx.l	r4, r5, r5
	ptabs	r5, t0

	/* Prepare original args */
	ld.q	SP, FRAME_R(2), r2
	ld.q	SP, FRAME_R(3), r3
	ld.q	SP, FRAME_R(4), r4
	ld.q	SP, FRAME_R(5), r5
	ld.q	SP, FRAME_R(6), r6
	ld.q	SP, FRAME_R(7), r7

	/* And now the trick for those syscalls requiring regs * ! */
	or	SP, ZERO, r8

	/* Call it */
	blink	t0, ZERO	/* LINK is already properly set */

syscall_ret_trace:
	/* We get back here only if under trace */
	st.q	SP, FRAME_R(9), r2	/* Save return value */

	/*  ... usage of a pt relative (_ptar _syscall_trace) fails on CDC */
	_loada	syscall_trace, LINK
	ptabs	LINK, t0
	blink	t0, LINK

	/* This needs to be done after any syscall tracing */
	ld.q	SP, FRAME_S(FSPC), r2
	addi	r2, 4, r2	/* Move PC, being pre-execution event */
	st.q	SP, FRAME_S(FSPC), r2

	_ptar	ret_from_syscall, t0
	blink	t0, ZERO		/* Resume normal return sequence */

/*
 * --- Switch to running under a particular ASID and return the previous ASID value
 * --- The caller is assumed to have done a cli before calling this.
 *
 * Input r2 : new ASID
 * Output r2 : old ASID
 */

	.global switch_and_save_asid
switch_and_save_asid:
	getcon	sr, r0
	movi	255, r4
	shlli 	r4, 16, r4	/* r4 = mask to select ASID */
	and	r0, r4, r3	/* r3 = shifted old ASID */
	andi	r2, 255, r2	/* mask down new ASID */
	shlli	r2, 16, r2	/* align new ASID against SR.ASID */
	andc	r0, r4, r0	/* efface old ASID from SR */
	or	r0, r2, r0	/* insert the new ASID */
	putcon	r0, ssr
	_loada	1f, r0
	putcon	r0, spc
	rte
	nop
1:
	ptabs	r18, tr0
	shlri	r3, 16, r2	/* r2 = old ASID */
	blink tr0, r63

	.global	route_to_panic_handler
route_to_panic_handler:
	/* Switch to real mode, goto panic_handler, don't return.  Useful for
	   last-chance debugging, e.g. if no output wants to go to the console.
	   */

	_loada	panic_handler - CONFIG_CACHED_MEMORY_OFFSET, r1
	ptabs	r1, tr0
	pta	1f, tr1
	gettr	tr1, r0
	putcon	r0, spc
	getcon	sr, r0
	movi	1, r1
	shlli	r1, 31, r1
	andc	r0, r1, r0
	putcon	r0, ssr
	rte
	nop
1:	/* Now in real mode */
	blink tr0, r63
	nop

	.global peek_real_address_q
peek_real_address_q:
	/* Two args:
	   r2 : real mode address to peek
	   r2(out) : result quadword

	   This is provided as a cheapskate way of manipulating device
	   registers for debugging (to avoid the need to onchip_remap the debug
	   module, and to avoid the need to onchip_remap the watchpoint
	   controller in a way that identity maps sufficient bits to avoid the
	   SH5-101 cut2 silicon defect).

	   This code is not performance critical
	*/

	add.l	r2, r63, r2	/* sign extend address */
	getcon	sr, r0		/* r0 = saved original SR */
	movi	1, r1
	shlli	r1, 28, r1
	or	r0, r1, r1	/* r0 with block bit set */
	putcon	r1, sr		/* now in critical section */
	movi	1, r36
	shlli	r36, 31, r36
	andc	r1, r36, r1	/* turn sr.mmu off in real mode section */

	putcon	r1, ssr
	_loada	.peek0 - CONFIG_CACHED_MEMORY_OFFSET, r36 /* real mode target address */
	_loada	1f, r37		/* virtual mode return addr */
	putcon	r36, spc

	synco
	rte
	nop

.peek0:	/* come here in real mode, don't touch caches!!
           still in critical section (sr.bl==1) */
	putcon	r0, ssr
	putcon	r37, spc
	/* Here's the actual peek.  If the address is bad, all bets are now off
	 * what will happen (handlers invoked in real-mode = bad news) */
	ld.q	r2, 0, r2
	synco
	rte	/* Back to virtual mode */
	nop

1:
	ptabs	r18, tr0
	blink	tr0, r63

	.global poke_real_address_q
poke_real_address_q:
	/* Two args:
	   r2 : real mode address to poke
	   r3 : quadword value to write.

	   This is provided as a cheapskate way of manipulating device
	   registers for debugging (to avoid the need to onchip_remap the debug
	   module, and to avoid the need to onchip_remap the watchpoint
	   controller in a way that identity maps sufficient bits to avoid the
	   SH5-101 cut2 silicon defect).

	   This code is not performance critical
	*/

	add.l	r2, r63, r2	/* sign extend address */
	getcon	sr, r0		/* r0 = saved original SR */
	movi	1, r1
	shlli	r1, 28, r1
	or	r0, r1, r1	/* r0 with block bit set */
	putcon	r1, sr		/* now in critical section */
	movi	1, r36
	shlli	r36, 31, r36
	andc	r1, r36, r1	/* turn sr.mmu off in real mode section */

	/* Bodge : force sr.watch high on return.  Can't understand why else this
	   isn't happening. */
	movi	1, r38
	shlli	r38, 26, r38
	or	r38, r0, r0

	putcon	r1, ssr
	_loada	.poke0-CONFIG_CACHED_MEMORY_OFFSET, r36 /* real mode target address */
	_loada	1f, r37		/* virtual mode return addr */
	putcon	r36, spc

	synco
	rte
	nop

.poke0:	/* come here in real mode, don't touch caches!!
           still in critical section (sr.bl==1) */
	putcon	r0, ssr
	putcon	r37, spc
	/* Here's the actual poke.  If the address is bad, all bets are now off
	 * what will happen (handlers invoked in real-mode = bad news) */
	st.q	r2, 0, r3
	synco
	rte	/* Back to virtual mode */
	nop

1:
	ptabs	r18, tr0
	blink	tr0, r63

/*
 * --- User Access Handling Section
 */

/*
 * User Access support. It all moved to non inlined Assembler
 * functions in here.
 *
 * __kernel_size_t __copy_user(void *__to, const void *__from,
 *			       __kernel_size_t __n)
 *
 * Inputs:
 * (r2)  target address
 * (r3)  source address
 * (r4)  size in bytes
 *
 * Ouputs:
 * (*r2) target data
 * (r2)  non-copied bytes
 *
 * If a fault occurs on the user pointer, bail out early and return the
 * number of bytes not copied in r2.
 * Strategy : for large blocks, call a real memcpy function which can
 * move >1 byte at a time using unaligned ld/st instructions, and can
 * manipulate the cache using prefetch + alloco to improve the speed
 * further.  If a fault occurs in that function, just revert to the
 * byte-by-byte approach used for small blocks; this is rare so the
 * performance hit for that case does not matter.
 *
 * For small blocks it's not worth the overhead of setting up and calling
 * the memcpy routine; do the copy a byte at a time.
 *
 */
	.global	__copy_user
__copy_user:
	_ptar	__copy_user_byte_by_byte, t1
	movi	16, r0 ! this value is a best guess, should tune it by benchmarking
	bge/u	r0, r4, t1
	_ptar copy_user_memcpy, t0
	addi	r15, -32, r15
	/* Save arguments in case we have to fix-up unhandled page fault */
	st.q	r15, 0, r2
	st.q	r15, 8, r3
	st.q	r15, 16, r4
	st.q	r15, 24, r35 ! r35 is callee-save
	/* Save LINK in a register to reduce RTS time later (otherwise
	   ld r15,*,LINK;ptabs LINK;trn;blink trn,r63 becomes a critical path) */
	ori	LINK, 0, r35
	blink	t0, LINK

	/* Copy completed normally if we get back here */
	ptabs	r35, tr0
	ld.q	r15, 24, r35
	/* don't restore r2-r4, pointless */
	/* set result=r2 to zero as the copy must have succeeded. */
	or	r63, r63, r2
	addi	r15, 32, r15
	blink	tr0, r63 ! RTS

	.global __copy_user_fixup
__copy_user_fixup:
	/* Restore stack frame */
	ori	r35, 0, LINK
	ld.q	r15, 24, r35
	ld.q	r15, 16, r4
	ld.q	r15,  8, r3
	ld.q	r15,  0, r2
	addi	r15, 32, r15
	/* Fall through to original code, in the 'same' state we entered with */

/* The slow byte-by-byte method is used if the fast copy traps due to a bad
   user address.  In that rare case, the speed drop can be tolerated. */
__copy_user_byte_by_byte:
	_ptar	___copy_user_exit, t1
	_ptar	___copy_user1, t0
	beq/u	r4, r63, t1	/* early exit for zero length copy */
	sub	r2, r3, r0
	addi	r0, -1, r0

___copy_user1:
	ld.b	r3, 0, r5		/* Fault address 1 */

	/* Could rewrite this to use just 1 add, but the second comes 'free'
	   due to load latency */
	addi	r3, 1, r3
	addi	r4, -1, r4		/* No real fixup required */
___copy_user2:
	stx.b	r3, r0, r5		/* Fault address 2 */
	bne     r4, ZERO, t0

___copy_user_exit:
	or	r4, ZERO, r2
	ptabs	LINK, t0
	blink	t0, ZERO

/*
 * __kernel_size_t __clear_user(void *addr, __kernel_size_t size)
 *
 * Inputs:
 * (r2)  target address
 * (r3)  size in bytes
 *
 * Ouputs:
 * (*r2) zero-ed target data
 * (r2)  non-zero-ed bytes
 */
	.global	__clear_user
__clear_user:
	_ptar	___clear_user_exit, t1
	_ptar	___clear_user1, t0
	beq/u	r3, r63, t1

___clear_user1:
	st.b	r2, 0, ZERO		/* Fault address */
	addi	r2, 1, r2
	addi	r3, -1, r3		/* No real fixup required */
	bne     r3, ZERO, t0

___clear_user_exit:
	or	r3, ZERO, r2
	ptabs	LINK, t0
	blink	t0, ZERO


/*
 * int __strncpy_from_user(unsigned long __dest, unsigned long __src,
 *			   int __count)
 *
 * Inputs:
 * (r2)  target address
 * (r3)  source address
 * (r4)  maximum size in bytes
 *
 * Ouputs:
 * (*r2) copied data
 * (r2)  -EFAULT (in case of faulting)
 *       copied data (otherwise)
 */
	.global	__strncpy_from_user
__strncpy_from_user:
	_ptar	___strncpy_from_user1, t0
	_ptar	___strncpy_from_user_done, t1
	or	r4, ZERO, r5		/* r5 = original count */
	beq/u	r4, r63, t1		/* early exit if r4==0 */
	movi	-(EFAULT), r6		/* r6 = reply, no real fixup */
	or	ZERO, ZERO, r7		/* r7 = data, clear top byte of data */

___strncpy_from_user1:
	ld.b	r3, 0, r7		/* Fault address: only in reading */
	st.b	r2, 0, r7
	addi	r2, 1, r2
	addi	r3, 1, r3
	beq/u	ZERO, r7, t1
	addi	r4, -1, r4		/* return real number of copied bytes */
	bne/l	ZERO, r4, t0

___strncpy_from_user_done:
	sub	r5, r4, r6		/* If done, return copied */
	
___strncpy_from_user_exit:
	or	r6, ZERO, r2
	ptabs	LINK, t0
	blink	t0, ZERO

/*
 * extern long __strnlen_user(const char *__s, long __n)
 *
 * Inputs:
 * (r2)  source address
 * (r3)  source size in bytes
 *
 * Ouputs:
 * (r2)  -EFAULT (in case of faulting)
 *       string length (otherwise)
 */
	.global	__strnlen_user
__strnlen_user:
	_ptar	___strnlen_user_set_reply, t0
	_ptar	___strnlen_user1, t1
	or	ZERO, ZERO, r5		/* r5 = counter */
	movi	-(EFAULT), r6		/* r6 = reply, no real fixup */
	or	ZERO, ZERO, r7		/* r7 = data, clear top byte of data */
	beq	r3, ZERO, t0

___strnlen_user1:
	ldx.b	r2, r5, r7		/* Fault address: only in reading */
	addi	r3, -1, r3		/* No real fixup */
	addi	r5, 1, r5
	beq	r3, ZERO, t0
	bne	r7, ZERO, t1
! The line below used to be active.  This meant led to a junk byte lying between each pair
! of entries in the argv & envp structures in memory.  Whilst the program saw the right data
! via the argv and envp arguments to main, it meant the 'flat' representation visible through
! /proc/$pid/cmdline was corrupt, causing trouble with ps, for example.
!	addi	r5, 1, r5		/* Include '\0' */

___strnlen_user_set_reply:
	or	r5, ZERO, r6		/* If done, return counter */
	
___strnlen_user_exit:
	or	r6, ZERO, r2
	ptabs	LINK, t0
	blink	t0, ZERO

/*
 * extern long __get_user_asm_?(void *val, long addr)
 *
 * Inputs:
 * (r2)  dest address
 * (r3)  source address (in User Space)
 *
 * Ouputs:
 * (r2)  -EFAULT (faulting)
 *       0 	 (not faulting)
 */
	.global	__get_user_asm_b
__get_user_asm_b:
	or	r2, ZERO, r4
	movi	-(EFAULT), r2		/* r2 = reply, no real fixup */
	
___get_user_asm_b1:
	ld.b	r3, 0, r5		/* r5 = data */
	st.b	r4, 0, r5
	or	ZERO, ZERO, r2
	
___get_user_asm_b_exit:
	ptabs	LINK, t0
	blink	t0, ZERO


	.global	__get_user_asm_w
__get_user_asm_w:
	or	r2, ZERO, r4
	movi	-(EFAULT), r2		/* r2 = reply, no real fixup */
	
___get_user_asm_w1:
	ld.w	r3, 0, r5		/* r5 = data */
	st.w	r4, 0, r5
	or	ZERO, ZERO, r2
	
___get_user_asm_w_exit:
	ptabs	LINK, t0
	blink	t0, ZERO


	.global	__get_user_asm_l
__get_user_asm_l:
	or	r2, ZERO, r4
	movi	-(EFAULT), r2		/* r2 = reply, no real fixup */
	
___get_user_asm_l1:
	ld.l	r3, 0, r5		/* r5 = data */
	st.l	r4, 0, r5
	or	ZERO, ZERO, r2
	
___get_user_asm_l_exit:
	ptabs	LINK, t0
	blink	t0, ZERO


	.global	__get_user_asm_q
__get_user_asm_q:
	or	r2, ZERO, r4
	movi	-(EFAULT), r2		/* r2 = reply, no real fixup */
	
___get_user_asm_q1:
	ld.q	r3, 0, r5		/* r5 = data */
	st.q	r4, 0, r5
	or	ZERO, ZERO, r2
	
___get_user_asm_q_exit:
	ptabs	LINK, t0
	blink	t0, ZERO

/*
 * extern long __put_user_asm_?(void *pval, long addr)
 *
 * Inputs:
 * (r2)  kernel pointer to value
 * (r3)  dest address (in User Space)
 *
 * Ouputs:
 * (r2)  -EFAULT (faulting)
 *       0 	 (not faulting)
 */
	.global	__put_user_asm_b
__put_user_asm_b:
	ld.b	r2, 0, r4		/* r4 = data */
	movi	-(EFAULT), r2		/* r2 = reply, no real fixup */
	
___put_user_asm_b1:
	st.b	r3, 0, r4
	or	ZERO, ZERO, r2
	
___put_user_asm_b_exit:
	ptabs	LINK, t0
	blink	t0, ZERO


	.global	__put_user_asm_w
__put_user_asm_w:
	ld.w	r2, 0, r4		/* r4 = data */
	movi	-(EFAULT), r2		/* r2 = reply, no real fixup */
	
___put_user_asm_w1:
	st.w	r3, 0, r4
	or	ZERO, ZERO, r2
	
___put_user_asm_w_exit:
	ptabs	LINK, t0
	blink	t0, ZERO


	.global	__put_user_asm_l
__put_user_asm_l:
	ld.l	r2, 0, r4		/* r4 = data */
	movi	-(EFAULT), r2		/* r2 = reply, no real fixup */
	
___put_user_asm_l1:
	st.l	r3, 0, r4
	or	ZERO, ZERO, r2
	
___put_user_asm_l_exit:
	ptabs	LINK, t0
	blink	t0, ZERO


	.global	__put_user_asm_q
__put_user_asm_q:
	ld.q	r2, 0, r4		/* r4 = data */
	movi	-(EFAULT), r2		/* r2 = reply, no real fixup */
	
___put_user_asm_q1:
	st.q	r3, 0, r4
	or	ZERO, ZERO, r2
	
___put_user_asm_q_exit:
	ptabs	LINK, t0
	blink	t0, ZERO


/*
 * --- Signal Handling Section
 */

/*
 * extern long long _sa_default_rt_restorer
 * extern long long _sa_default_restorer
 *
 *		 or, better,
 *
 * extern void _sa_default_rt_restorer(void)
 * extern void _sa_default_restorer(void)
 *
 * Code prototypes to do a sys_rt_sigreturn() or sys_sysreturn()
 * from user space. Copied into user space by signal management.
 * Both must be quad aligned and 2 quad long (4 instructions).
 *
 */
	.balign 8
	.global sa_default_rt_restorer
sa_default_rt_restorer:
	movi	0x10, r9
	shori	__NR_rt_sigreturn, r9
	trapa	r9
	nop
	
	.balign 8
	.global sa_default_restorer
sa_default_restorer:
	movi	0x10, r9
	shori	__NR_sigreturn, r9
	trapa	r9
	nop
	
/*
 * --- __ex_table Section
 */

/*
 * User Access Exception Table.
 */
	.section	__ex_table,  "a"

	.global asm_uaccess_start	/* Just a marker */
asm_uaccess_start:

	.long	___copy_user1, ___copy_user_exit
	.long	___copy_user2, ___copy_user_exit
	.long	___clear_user1, ___clear_user_exit
	.long	___strncpy_from_user1, ___strncpy_from_user_exit
	.long	___strnlen_user1, ___strnlen_user_exit
	.long	___get_user_asm_b1, ___get_user_asm_b_exit
	.long	___get_user_asm_w1, ___get_user_asm_w_exit
	.long	___get_user_asm_l1, ___get_user_asm_l_exit
	.long	___get_user_asm_q1, ___get_user_asm_q_exit
	.long	___put_user_asm_b1, ___put_user_asm_b_exit
	.long	___put_user_asm_w1, ___put_user_asm_w_exit
	.long	___put_user_asm_l1, ___put_user_asm_l_exit
	.long	___put_user_asm_q1, ___put_user_asm_q_exit

	.global asm_uaccess_end		/* Just a marker */
asm_uaccess_end:
	



/*
 * --- .text.init Section
 */

	.section	.text.init, "ax"

/*
 * void trap_init (void)
 *
 */
	.global	trap_init
trap_init:
	addi	SP, -24, SP			/* Room to save r28/r29/r30 */
	st.q	SP, 0, r28
	st.q	SP, 8, r29
	st.q	SP, 16, r30

	/* Set VBR and RESVEC */
	_loada	LVBR_block, r19
	andi	r19, -4, r19			/* reset MMUOFF + reserved */
	/* For RESVEC exceptions we force the MMU off, which means we need the
	   physical address. */
	_loada	LRESVEC_block-CONFIG_CACHED_MEMORY_OFFSET, r20
	andi	r20, -4, r20			/* reset reserved */
	ori	r20, 1, r20			/* set MMUOFF */
	putcon	r19, VBR
	putcon	r20, RESVEC

	/* Sanity check */
	_loada	LVBR_block_end, r21
	andi	r21, -4, r21
	movi	BLOCK_SIZE, r29			/* r29 = expected size */
	or	r19, ZERO, r30
	add	r19, r29, r19

	/*
	 * Ugly, but better loop forever now than crash afterwards.
	 * We should print a message, but if we touch LVBR or
	 * LRESVEC blocks we should not be surprised if we get stuck
	 * in trap_init().
	 */
	_ptar	trap_init_loop, t1
	gettr	t1, r28				/* r28 = trap_init_loop */
	sub	r21, r30, r30			/* r30 = actual size */

	/*
	 * VBR/RESVEC handlers overlap by being bigger than
	 * allowed. Very bad. Just loop forever.
	 * (r28) panic/loop address
	 * (r29) expected size
	 * (r30) actual size
	 */
trap_init_loop:
	bne	r19, r21, t1

	/* Now that exception vectors are set up reset SR.BL */
	getcon 	SR, r22
	movi	SR_UNBLOCK_EXC, r23
	and	r22, r23, r22
	putcon	r22, SR

	addi	SP, 24, SP
	ptabs	LINK, t0
	blink	t0, ZERO

