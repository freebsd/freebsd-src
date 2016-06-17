/* 
 * Linux/PA-RISC Project (http://www.parisc-linux.org/)
 * 
 * System call entry code Copyright (c) Matthew Wilcox 1999 <willy@bofh.ai>
 * Licensed under the GNU GPL.
 * thanks to Philipp Rumpf, Mike Shaver and various others
 * sorry about the wall, puffin..
 */

#include <asm/offset.h>
#include <asm/unistd.h>
#include <asm/errno.h>
#include <asm/psw.h>

#include <asm/assembly.h>
#include <asm/processor.h>
#include <linux/version.h>

#ifdef __LP64__
	.level          2.0w
#else
	.level		1.1
#endif
	.text

#ifdef __LP64__
#define FRAME_SIZE	128
#else
#define FRAME_SIZE	64
#endif

	.import syscall_exit,code
	.import syscall_exit_rfi,code
	.export linux_gateway_page

	/* Linux gateway page is aliased to virtual page 0 in the kernel
	 * address space. Since it is a gateway page it cannot be
	 * dereferenced, so null pointers will still fault. We start
	 * the actual entry point at 0x100. We put break instructions
	 * at the beginning of the page to trap null indirect function
	 * pointers.
	 */

	.align 4096
linux_gateway_page:

	.rept 56
	break   0,0
	.endr

set_thread_pointer:
	gate	.+8, %r0		/* increase privilege */
	depi	3, 31, 2, %r31		/* Ensure we return into user mode. */
	be	0(%sr7,%r31)		/* return to user space */
	mtctl	%r26, %cr27		/* move arg0 to the control register */

	.rept 4
	break   0,0
	.endr

/* This address must remain fixed, or user binaries go splat. */
	.align 256
linux_gateway_entry:
	gate	.+8, %r0			/* become privileged */
	mtsp	%r0,%sr4			/* get kernel space into sr4 */
	mtsp	%r0,%sr5			/* get kernel space into sr5 */
	mtsp	%r0,%sr6			/* get kernel space into sr6 */
	mfsp    %sr7,%r1                        /* save user sr7 */
	mtsp    %r1,%sr3                        /* and store it in sr3 */

#ifdef __LP64__
	/* for now we can *always* set the W bit on entry to the syscall
	 * since we don't support wide userland processes.  We could
	 * also save the current SM other than in r0 and restore it on
	 * exit from the syscall, and also use that value to know
	 * whether to do narrow or wide syscalls. -PB
	 */
	ssm	PSW_SM_W, %r0
	/* The top halves of argument registers must be cleared on syscall
	 * entry.
	 */
	depdi	0, 31, 32, %r26
	depdi	0, 31, 32, %r25
	depdi	0, 31, 32, %r24
	depdi	0, 31, 32, %r23
	depdi	0, 31, 32, %r22
	depdi	0, 31, 32, %r21
#endif
	mfctl   %cr30,%r1
	xor     %r1,%r30,%r30                   /* ye olde xor trick */
	xor     %r1,%r30,%r1
	xor     %r1,%r30,%r30
	ldo     TASK_SZ_ALGN+FRAME_SIZE(%r30),%r30  /* set up kernel stack */

	/* N.B.: It is critical that we don't set sr7 to 0 until r30
	 *       contains a valid kernel stack pointer. It is also
	 *       critical that we don't start using the kernel stack
	 *       until after sr7 has been set to 0.
	 */

	mtsp	%r0,%sr7			/* get kernel space into sr7 */
	STREG   %r1,TASK_PT_GR30-TASK_SZ_ALGN-FRAME_SIZE(%r30) /* save usp */
	ldo     -TASK_SZ_ALGN-FRAME_SIZE(%r30),%r1   /* get task ptr in %r1 */

	/* Save some registers for sigcontext and potential task
	   switch (see entry.S for the details of which ones are
	   saved/restored).  TASK_PT_PSW is zeroed so we can see whether
	   a process is on a syscall or not.  For an interrupt the real
	   PSW value is stored.  This is needed for gdb and sys_ptrace. */
	STREG	%r0,  TASK_PT_PSW(%r1)
	STREG	%r2,  TASK_PT_GR2(%r1)		/* preserve rp */
	STREG	%r19, TASK_PT_GR19(%r1)
	STREG	%r20, TASK_PT_GR20(%r1)
	STREG	%r21, TASK_PT_GR21(%r1)
	STREG	%r22, TASK_PT_GR22(%r1)
	STREG	%r23, TASK_PT_GR23(%r1)		/* 4th argument */
	STREG	%r24, TASK_PT_GR24(%r1)		/* 3rd argument */
	STREG	%r25, TASK_PT_GR25(%r1)		/* 2nd argument */
	STREG	%r26, TASK_PT_GR26(%r1)	 	/* 1st argument */
	STREG	%r27, TASK_PT_GR27(%r1)		/* user dp */
	STREG   %r28, TASK_PT_GR28(%r1)         /* return value 0 */
	STREG   %r28, TASK_PT_ORIG_R28(%r1)     /* return value 0 (saved for signals) */
	STREG	%r29, TASK_PT_GR29(%r1)		/* return value 1 */
	STREG	%r31, TASK_PT_GR31(%r1)		/* preserve syscall return ptr */
	
	ldo	TASK_PT_FR0(%r1), %r27		/* save fpregs from the kernel */
	save_fp	%r27				/* or potential task switch  */

	mfctl	%cr11, %r27			/* i.e. SAR */
	STREG	%r27, TASK_PT_SAR(%r1)

	loadgp

#ifdef __LP64__
	ldo	-16(%r30),%r29			/* Reference param save area */
#else
	/* no need to save these on stack in wide mode because the first 8
	 * args are passed in registers */
	stw     %r22, -52(%r30)                 /* 5th argument */
	stw     %r21, -56(%r30)                 /* 6th argument */
#endif

	/* for some unknown reason, task_struct.ptrace is an unsigned long so use LDREG */
	LDREG	TASK_PTRACE(%r1), %r19		/* Are we being ptraced? */

	bb,<,n	%r19, 31, .Ltracesys		/* must match PT_PTRACE bit */
	
	/* Note!  We cannot use the syscall table that is mapped
	nearby since the gateway page is mapped execute-only. */

	ldil	L%sys_call_table, %r1
	ldo     R%sys_call_table(%r1), %r19
	
	comiclr,>>=	__NR_Linux_syscalls, %r20, %r0
	b,n	.Lsyscall_nosys
	
#ifdef __LP64__
	ldd,s   %r20(%r19), %r19
#else
	ldwx,s  %r20(%r19), %r19
#endif
	/* If this is a sys_rt_sigreturn call, and the signal was received
	 * when not in_syscall, then we want to return via syscall_exit_rfi,
	 * not syscall_exit.  Signal no. in r20, in_syscall in r25 (see
	 * trampoline code in signal.c).
	 */
	ldi	__NR_rt_sigreturn,%r2
	comb,=	%r2,%r20,.Lrt_sigreturn
.Lin_syscall:
	ldil	L%syscall_exit,%r2
	be      0(%sr7,%r19)
	ldo	R%syscall_exit(%r2),%r2
.Lrt_sigreturn:
	comib,<> 0,%r25,.Lin_syscall
	ldil	L%syscall_exit_rfi,%r2
	be      0(%sr7,%r19)
	ldo	R%syscall_exit_rfi(%r2),%r2

	/* Note!  Because we are not running where we were linked, any
	calls to functions external to this file must be indirect.  To
	be safe, we apply the opposite rule to functions within this
	file, with local labels given to them to ensure correctness. */
	
.Lsyscall_nosys:
syscall_nosys:
	ldil	L%syscall_exit,%r1
	be	R%syscall_exit(%sr7,%r1)
	ldo	-ENOSYS(%r0),%r28		   /* set errno */


/* Warning! This trace code is a virtual duplicate of the code above so be
 * sure to maintain both! */
.Ltracesys:
tracesys:
	/* Need to save more registers so the debugger can see where we
	 * are.  This saves only the lower 8 bits of PSW, so that the C
	 * bit is still clear on syscalls, and the D bit is set if this
	 * full register save path has been executed.  We check the D
	 * bit on syscall_return_rfi to determine which registers to
	 * restore.  An interrupt results in a full PSW saved with the
	 * C bit set, a non-straced syscall entry results in C and D clear
	 * in the saved PSW.
	 */
	ldo     -TASK_SZ_ALGN-FRAME_SIZE(%r30),%r1      /* get task ptr */
	ssm	0,%r2
	STREG	%r2,TASK_PT_PSW(%r1)		/* Lower 8 bits only!! */
	mfsp	%sr0,%r2
	STREG	%r2,TASK_PT_SR0(%r1)
	mfsp	%sr1,%r2
	STREG	%r2,TASK_PT_SR1(%r1)
	mfsp	%sr2,%r2
	STREG	%r2,TASK_PT_SR2(%r1)
	mfsp	%sr3,%r2
	STREG	%r2,TASK_PT_SR3(%r1)
	STREG	%r2,TASK_PT_SR4(%r1)
	STREG	%r2,TASK_PT_SR5(%r1)
	STREG	%r2,TASK_PT_SR6(%r1)
	STREG	%r2,TASK_PT_SR7(%r1)
	STREG	%r2,TASK_PT_IASQ0(%r1)
	STREG	%r2,TASK_PT_IASQ1(%r1)
	LDREG	TASK_PT_GR31(%r1),%r2
	STREG	%r2,TASK_PT_IAOQ0(%r1)
	ldo	4(%r2),%r2
	STREG	%r2,TASK_PT_IAOQ1(%r1)
	ldo	TASK_REGS(%r1),%r2
	/* reg_save %r2 */
	STREG	%r3,PT_GR3(%r2)
	STREG	%r4,PT_GR4(%r2)
	STREG	%r5,PT_GR5(%r2)
	STREG	%r6,PT_GR6(%r2)
	STREG	%r7,PT_GR7(%r2)
	STREG	%r8,PT_GR8(%r2)
	STREG	%r9,PT_GR9(%r2)
	STREG	%r10,PT_GR10(%r2)
	STREG	%r11,PT_GR11(%r2)
	STREG	%r12,PT_GR12(%r2)
	STREG	%r13,PT_GR13(%r2)
	STREG	%r14,PT_GR14(%r2)
	STREG	%r15,PT_GR15(%r2)
	STREG	%r16,PT_GR16(%r2)
	STREG	%r17,PT_GR17(%r2)
	STREG	%r18,PT_GR18(%r2)
	/* Finished saving things for the debugger */

	ldil	L%syscall_trace,%r1
	ldil	L%tracesys_next,%r2
	be	R%syscall_trace(%sr7,%r1)
	ldo	R%tracesys_next(%r2),%r2
	
tracesys_next:	
	ldil	L%sys_call_table,%r1
	ldo     R%sys_call_table(%r1), %r19

	ldo     -TASK_SZ_ALGN-FRAME_SIZE(%r30),%r1      /* get task ptr */
	LDREG   TASK_PT_GR20(%r1), %r20
	LDREG   TASK_PT_GR26(%r1), %r26		/* Restore the users args */
	LDREG   TASK_PT_GR25(%r1), %r25
	LDREG   TASK_PT_GR24(%r1), %r24
	LDREG   TASK_PT_GR23(%r1), %r23
#ifdef __LP64__
	LDREG   TASK_PT_GR22(%r1), %r22
	LDREG   TASK_PT_GR21(%r1), %r21
	ldo	-16(%r30),%r29			/* Reference param save area */
#endif

	comiclr,>>=	__NR_Linux_syscalls, %r20, %r0
	b,n	.Lsyscall_nosys

#ifdef __LP64__
	ldd,s   %r20(%r19), %r19
#else
	ldwx,s  %r20(%r19), %r19
#endif
	/* If this is a sys_rt_sigreturn call, and the signal was received
	 * when not in_syscall, then we want to return via syscall_exit_rfi,
	 * not syscall_exit.  Signal no. in r20, in_syscall in r25 (see
	 * trampoline code in signal.c).
	 */
	ldi	__NR_rt_sigreturn,%r2
	comb,=	%r2,%r20,.Ltrace_rt_sigreturn
.Ltrace_in_syscall:
	ldil	L%tracesys_exit,%r2
	be      0(%sr7,%r19)
	ldo	R%tracesys_exit(%r2),%r2

	/* Do *not* call this function on the gateway page, because it
	makes a direct call to syscall_trace. */
	
tracesys_exit:
	ldo     -TASK_SZ_ALGN-FRAME_SIZE(%r30),%r1      /* get task ptr */
#ifdef __LP64__
	ldo	-16(%r30),%r29			/* Reference param save area */
#endif
	bl	syscall_trace, %r2
	STREG   %r28,TASK_PT_GR28(%r1)          /* save return value now */
	ldo     -TASK_SZ_ALGN-FRAME_SIZE(%r30),%r1      /* get task ptr */
	LDREG   TASK_PT_GR28(%r1), %r28		/* Restore return val. */

	ldil	L%syscall_exit,%r1
	be,n	R%syscall_exit(%sr7,%r1)

.Ltrace_rt_sigreturn:
	comib,<> 0,%r25,.Ltrace_in_syscall
	ldil	L%tracesys_sigexit,%r2
	be      0(%sr7,%r19)
	ldo	R%tracesys_sigexit(%r2),%r2

tracesys_sigexit:
	ldo     -TASK_SZ_ALGN-FRAME_SIZE(%r30),%r1      /* get task ptr */
#ifdef __LP64__
	ldo	-16(%r30),%r29			/* Reference param save area */
#endif
	bl	syscall_trace, %r2
	nop

	ldil	L%syscall_exit_rfi,%r1
	be,n	R%syscall_exit_rfi(%sr7,%r1)

#ifdef __LP64__
/* Use ENTRY_SAME for 32-bit syscalls which are the same on wide and
 * narrow palinux.  Use ENTRY_DIFF for those where a 32-bit specific
 * implementation is required on wide palinux.
 */
#define ENTRY_SAME(_name_) .dword sys_/**/_name_
#define ENTRY_DIFF(_name_) .dword sys32_/**/_name_
#define ENTRY_UHOH(_name_) .dword sys32_/**/unimplemented
#else
#define ENTRY_SAME(_name_) .word sys_/**/_name_
#define ENTRY_DIFF(_name_) .word sys_/**/_name_
#define ENTRY_UHOH(_name_) .word sys_/**/_name_
#endif

	.align 8
	.export sys_call_table
.Lsys_call_table:
sys_call_table:
	ENTRY_SAME(ni_syscall)	/* 0  -  old "setup()" system call*/
	ENTRY_SAME(exit)
	ENTRY_SAME(fork_wrapper)
	ENTRY_SAME(read)
	ENTRY_SAME(write)
	ENTRY_SAME(open)		/* 5 */
	ENTRY_SAME(close)
	ENTRY_SAME(waitpid)
	ENTRY_SAME(creat)
	ENTRY_SAME(link)
	ENTRY_SAME(unlink)		/* 10 */
	ENTRY_DIFF(execve_wrapper)
	ENTRY_SAME(chdir)
	/* See comments in kernel/time.c!!! Maybe we don't need this? */
	ENTRY_DIFF(time)
	ENTRY_SAME(mknod)
	ENTRY_SAME(chmod)		/* 15 */
	ENTRY_SAME(lchown)
	ENTRY_SAME(socket)
	/* struct stat is MAYBE identical wide and narrow ?? */
	ENTRY_DIFF(newstat)
	ENTRY_DIFF(lseek)
	ENTRY_SAME(getpid)		/* 20 */
	/* the 'void * data' parameter may need re-packing in wide */
	ENTRY_DIFF(mount)
	/* concerned about struct sockaddr in wide/narrow */
	/* ---> I think sockaddr is OK unless the compiler packs the struct */
	/*      differently to align the char array */
	ENTRY_SAME(bind)
	ENTRY_SAME(setuid)
	ENTRY_SAME(getuid)
	ENTRY_SAME(stime)		/* 25 */
	ENTRY_SAME(ptrace)
	ENTRY_SAME(alarm)
	/* see stat comment */
	ENTRY_DIFF(newfstat)
	ENTRY_SAME(pause)
	/* struct utimbuf uses time_t which might vary */
	ENTRY_DIFF(utime)		/* 30 */
	/* struct sockaddr... */
	ENTRY_SAME(connect)
	ENTRY_SAME(listen)
	ENTRY_SAME(access)
	ENTRY_SAME(nice)
	/* struct sockaddr... */
	ENTRY_SAME(accept)		/* 35 */
	ENTRY_SAME(sync)
	ENTRY_SAME(kill)
	ENTRY_SAME(rename)
	ENTRY_SAME(mkdir)
	ENTRY_SAME(rmdir)		/* 40 */
	ENTRY_SAME(dup)
	ENTRY_SAME(pipe)
	ENTRY_DIFF(times)
	/* struct sockaddr... */
	ENTRY_SAME(getsockname)
	/* it seems possible brk() could return a >4G pointer... */
	ENTRY_SAME(brk)		/* 45 */
	ENTRY_SAME(setgid)
	ENTRY_SAME(getgid)
	ENTRY_SAME(signal)
	ENTRY_SAME(geteuid)
	ENTRY_SAME(getegid)		/* 50 */
	ENTRY_SAME(acct)
	ENTRY_SAME(umount)
	/* struct sockaddr... */
	ENTRY_SAME(getpeername)
	/* This one's a huge ugly mess */
	ENTRY_DIFF(ioctl)
	/* struct flock? */
	ENTRY_DIFF(fcntl)		/* 55 */
	ENTRY_SAME(socketpair)
	ENTRY_SAME(setpgid)
	ENTRY_SAME(send)
	ENTRY_SAME(newuname)
	ENTRY_SAME(umask)		/* 60 */
	ENTRY_SAME(chroot)
	ENTRY_SAME(ustat)
	ENTRY_SAME(dup2)
	ENTRY_SAME(getppid)
	ENTRY_SAME(getpgrp)		/* 65 */
	ENTRY_SAME(setsid)
	ENTRY_SAME(pivot_root)
	/* I don't like this */
	ENTRY_UHOH(sgetmask)
	ENTRY_UHOH(ssetmask)
	ENTRY_SAME(setreuid)	/* 70 */
	ENTRY_SAME(setregid)
	ENTRY_SAME(mincore)
	ENTRY_DIFF(sigpending)
	ENTRY_SAME(sethostname)
	/* Following 3 have linux-common-code structs containing longs -( */
	ENTRY_DIFF(setrlimit)	/* 75 */
	ENTRY_DIFF(getrlimit)
	ENTRY_DIFF(getrusage)
	/* struct timeval and timezone are maybe?? consistent wide and narrow */
	ENTRY_DIFF(gettimeofday)
	ENTRY_DIFF(settimeofday)
	ENTRY_SAME(getgroups)	/* 80 */
	ENTRY_SAME(setgroups)
	/* struct socketaddr... */
	ENTRY_SAME(sendto)
	ENTRY_SAME(symlink)
	/* see stat comment */
	ENTRY_DIFF(newlstat)
	ENTRY_SAME(readlink)	/* 85 */
	/* suspect we'll need some work for narrow shlibs on wide kernel */
	/* NOTE this doesn't get used when I boot 32-bit userspace */
	/* containing working shlib apps -- can this be nuked? */
	ENTRY_UHOH(uselib)
	ENTRY_SAME(swapon)
	ENTRY_SAME(reboot)
	ENTRY_SAME(mmap2)
	ENTRY_SAME(mmap)		/* 90 */
	ENTRY_SAME(munmap)
	ENTRY_SAME(truncate)
	ENTRY_SAME(ftruncate)
	ENTRY_SAME(fchmod)
	ENTRY_SAME(fchown)		/* 95 */
	ENTRY_SAME(getpriority)
	ENTRY_SAME(setpriority)
	ENTRY_SAME(recv)
	ENTRY_DIFF(statfs)
	ENTRY_DIFF(fstatfs)		/* 100 */
	ENTRY_SAME(stat64)
	/* don't think hppa glibc even provides an entry pt for this
	 * so disable for now */
	ENTRY_UHOH(socketcall)
	ENTRY_SAME(syslog)
	/* even though manpage says struct timeval contains longs, ours has
	 * time_t and suseconds_t -- both of which are safe wide/narrow */
	ENTRY_DIFF(setitimer)
	ENTRY_DIFF(getitimer)	/* 105 */
	ENTRY_SAME(capget)
	ENTRY_SAME(capset)
	ENTRY_DIFF(pread)
	ENTRY_DIFF(pwrite)
	ENTRY_SAME(getcwd)		/* 110 */
	ENTRY_SAME(vhangup)
	ENTRY_SAME(fstat64)
	ENTRY_SAME(vfork_wrapper)
	/* struct rusage contains longs... */
	ENTRY_DIFF(wait4)
	ENTRY_SAME(swapoff)		/* 115 */
	ENTRY_DIFF(sysinfo)
	ENTRY_SAME(shutdown)
	ENTRY_SAME(fsync)
	ENTRY_SAME(madvise)
	ENTRY_SAME(clone_wrapper)	/* 120 */
	ENTRY_SAME(setdomainname)
	ENTRY_SAME(sendfile)
	/* struct sockaddr... */
	ENTRY_SAME(recvfrom)
	/* struct timex contains longs */
	ENTRY_DIFF(adjtimex)
	ENTRY_SAME(mprotect)	/* 125 */
	/* old_sigset_t forced to 32 bits.  Beware glibc sigset_t */
	ENTRY_DIFF(sigprocmask)
	ENTRY_SAME(create_module)
	/* struct module contains longs, but insmod builds a 64 bit struct
	 * if running under a 64 bit kernel */
	ENTRY_SAME(init_module)
	ENTRY_SAME(delete_module)
	/* struct kernel_sym contains a long. Linus never heard of size_t? */
	ENTRY_DIFF(get_kernel_syms)	/* 130 */
	/* time_t inside struct dqblk */
	ENTRY_DIFF(quotactl)
	ENTRY_SAME(getpgid)
	ENTRY_SAME(fchdir)
	ENTRY_SAME(bdflush)
	ENTRY_SAME(sysfs)		/* 135 */
	ENTRY_SAME(personality)
	ENTRY_SAME(ni_syscall)	/* for afs_syscall */
	ENTRY_SAME(setfsuid)
	ENTRY_SAME(setfsgid)
	/* I think this might work */
	ENTRY_SAME(llseek)		/* 140 */
	/* struct linux_dirent has longs, like 'unsigned long d_ino' which
	 * almost definitely should be 'ino_t d_ino' but it's too late now */
	ENTRY_DIFF(getdents)
	/* it is POSSIBLE that select will be OK because even though fd_set
	 * contains longs, the macros and sizes are clever. */
	ENTRY_DIFF(select)
	ENTRY_SAME(flock)
	ENTRY_SAME(msync)
	/* struct iovec contains pointers */
	ENTRY_DIFF(readv)		/* 145 */
	ENTRY_DIFF(writev)
	ENTRY_SAME(getsid)
	ENTRY_SAME(fdatasync)
	/* struct __sysctl_args is a mess */
	ENTRY_DIFF(sysctl)
	ENTRY_SAME(mlock)		/* 150 */
	ENTRY_SAME(munlock)
	ENTRY_SAME(mlockall)
	ENTRY_SAME(munlockall)
	/* struct sched_param is ok for now */
	ENTRY_SAME(sched_setparam)
	ENTRY_SAME(sched_getparam)	/* 155 */
	ENTRY_SAME(sched_setscheduler)
	ENTRY_SAME(sched_getscheduler)
	ENTRY_SAME(sched_yield)
	ENTRY_SAME(sched_get_priority_max)
	ENTRY_SAME(sched_get_priority_min)	/* 160 */
	/* These 2 would've worked if someone had defined struct timespec
	 * carefully, like timeval for example (which is about the same).
	 * Unfortunately it contains a long :-( */
	ENTRY_DIFF(sched_rr_get_interval)
	ENTRY_DIFF(nanosleep)
	ENTRY_SAME(mremap)
	ENTRY_SAME(setresuid)
	ENTRY_SAME(getresuid)	/* 165 */
	ENTRY_DIFF(sigaltstack_wrapper)
	/* struct passed back to user can contain long symbol values */
	ENTRY_DIFF(query_module)
	ENTRY_SAME(poll)
	/* structs contain pointers and an in_addr... */
	ENTRY_DIFF(nfsservctl)
	ENTRY_SAME(setresgid)	/* 170 */
	ENTRY_SAME(getresgid)
	ENTRY_SAME(prctl)
	/* signals need a careful review */
	ENTRY_SAME(rt_sigreturn_wrapper)
	ENTRY_DIFF(rt_sigaction)
	ENTRY_DIFF(rt_sigprocmask)	/* 175 */
	ENTRY_DIFF(rt_sigpending)
	ENTRY_UHOH(rt_sigtimedwait)
	/* even though the struct siginfo_t is different, it appears like
	 * all the paths use values which should be same wide and narrow.
	 * Also the struct is padded to 128 bytes which means we don't have
	 * to worry about faulting trying to copy in a larger 64-bit
	 * struct from a 32-bit user-space app.
	 */
	ENTRY_SAME(rt_sigqueueinfo)
	ENTRY_SAME(rt_sigsuspend_wrapper) /* not really SAME -- see the code */
	ENTRY_SAME(chown)		/* 180 */
	/* setsockopt() used by iptables: SO_SET_REPLACE/SO_SET_ADD_COUNTERS */
	ENTRY_DIFF(setsockopt)
	ENTRY_SAME(getsockopt)
	ENTRY_DIFF(sendmsg)
	ENTRY_DIFF(recvmsg)
	ENTRY_SAME(semop)		/* 185 */
	ENTRY_SAME(semget)
	ENTRY_DIFF(semctl_broken)
	ENTRY_DIFF(msgsnd)
	ENTRY_DIFF(msgrcv)
	ENTRY_SAME(msgget)		/* 190 */
	ENTRY_SAME(msgctl_broken)
	ENTRY_SAME(shmat_wrapper)
	ENTRY_SAME(shmdt)
	ENTRY_SAME(shmget)
	ENTRY_SAME(shmctl_broken)		/* 195 */
	ENTRY_SAME(ni_syscall)		/* streams1 */
	ENTRY_SAME(ni_syscall)		/* streams2 */
	ENTRY_SAME(lstat64)
	ENTRY_DIFF(truncate64)
	ENTRY_DIFF(ftruncate64)	/* 200 */
	ENTRY_SAME(getdents64)
	ENTRY_DIFF(fcntl64)
#ifdef CONFIG_XFS_FS
	ENTRY_SAME(attrctl)
	ENTRY_SAME(acl_get)
	ENTRY_SAME(acl_set)            /* 205 */
#else
	ENTRY_SAME(ni_syscall)
	ENTRY_SAME(ni_syscall)
	ENTRY_SAME(ni_syscall)	       /* 205 */
#endif
	ENTRY_SAME(gettid)             
	ENTRY_SAME(readahead)          
	ENTRY_SAME(tkill)

.end

	/* Make sure nothing else is placed on this page */

	.align 4096
	.export end_linux_gateway_page
end_linux_gateway_page:

