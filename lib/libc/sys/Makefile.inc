#	@(#)Makefile.inc	8.3 (Berkeley) 10/24/94
# $FreeBSD$

# sys sources
.PATH: ${.CURDIR}/../libc/${MACHINE_ARCH}/sys ${.CURDIR}/../libc/sys

# Include the generated makefile containing the *complete* list
# of syscall names in MIASM.
.include "${.CURDIR}/../../sys/sys/syscall.mk"

# Include machine dependent definitions.
#
# MDASM names override the default syscall names in MIASM.
# NOASM will prevent the default syscall code from being generated.
#
.if exists(${.CURDIR}/../libc/${MACHINE_ARCH}/sys/Makefile.inc)
.include "${.CURDIR}/../libc/${MACHINE_ARCH}/sys/Makefile.inc"
.endif

# Sources common to both syscall interfaces:
SRCS+=	ftruncate.c lseek.c mmap.c pread.c pwrite.c sem.c truncate.c __error.c

# Add machine dependent asm sources:
SRCS+=${MDASM}

# Look though the complete list of syscalls (MIASM) for names that are
# not defined with machine dependent implementations (MDASM) and are
# not declared for no generation of default code (NOASM).  Add each
# syscall that satisfies these conditions to the ASM list.
.for _asm in ${MIASM}
.if (${MDASM:R:M${_asm:R}} == "")
.if (${NOASM:R:M${_asm:R}} == "")
ASM+=$(_asm)
.endif
.endif
.endfor

OBJS+=	${ASM} ${PSEUDO}

SASM=	${ASM:S/.o/.S/}

SPSEUDO= ${PSEUDO:S/.o/.S/}

SRCS+=	${SASM} ${SPSEUDO}

# Generated files
CLEANFILES+=	${SASM} ${SPSEUDO}

${SASM}:
	printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' > ${.TARGET}

${SPSEUDO}:
	printf '#include "SYS.h"\nPSEUDO(${.PREFIX:S/_//})\n' \
	    > ${.TARGET}

.if ${LIB} == "c"
MAN+=	_exit.2 accept.2 access.2 acct.2 adjtime.2 \
	aio_cancel.2 aio_error.2 aio_read.2 aio_return.2 \
	aio_suspend.2 aio_waitcomplete.2 aio_write.2 \
	bind.2 brk.2 chdir.2 chflags.2 \
	chmod.2 chown.2 chroot.2 clock_gettime.2 close.2 \
	connect.2 dup.2 execve.2 extattr_get_file.2 \
	fcntl.2 fhopen.2 flock.2 fork.2 fsync.2 \
	getdirentries.2 getdtablesize.2 \
	getfh.2 getfsstat.2 getgid.2 getgroups.2 getitimer.2 getlogin.2 \
	getpeername.2 getpgrp.2 getpid.2 getpriority.2 getrlimit.2 \
	getrusage.2 getsid.2 getsockname.2 \
	getsockopt.2 gettimeofday.2 getuid.2 \
	intro.2 ioctl.2 issetugid.2 jail.2 kenv.2 kill.2 \
	kldfind.2 kldfirstmod.2 kldload.2 kldnext.2 kldstat.2 kldsym.2 \
	kldunload.2 kqueue.2 kse.2 ktrace.2 link.2 listen.2 lseek.2 \
	madvise.2 mincore.2 minherit.2 mkdir.2 mkfifo.2 mknod.2 mlock.2 mmap.2 \
	modfind.2 modnext.2 modstat.2 \
	mount.2 mprotect.2 msync.2 munmap.2 nanosleep.2 \
	nfssvc.2 open.2 pathconf.2 pipe.2 poll.2 profil.2 ptrace.2 quotactl.2 \
	read.2 readlink.2 reboot.2 recv.2 rename.2 revoke.2 rfork.2 rmdir.2 \
	rtprio.2 select.2 semctl.2 semget.2 semop.2 send.2 sendfile.2 \
	setgroups.2 setpgid.2 setregid.2 setresuid.2 setreuid.2 setsid.2 \
	setuid.2 shmat.2 shmctl.2 shmget.2 shutdown.2 \
	sigaction.2 sigaltstack.2 sigpending.2 sigprocmask.2 sigreturn.2 \
	sigstack.2 sigsuspend.2 socket.2 socketpair.2 stat.2 statfs.2 \
	swapon.2 symlink.2 sync.2 sysarch.2 syscall.2 \
	truncate.2 umask.2 undelete.2 \
	unlink.2 utimes.2 uuidgen.2 vfork.2 wait.2 write.2
.if !defined(NO_P1003_1B)
MAN+=	sched_get_priority_max.2 sched_setparam.2 \
	sched_setscheduler.2 sched_yield.2
.endif

MLINKS+=access.2 eaccess.2
MLINKS+=brk.2 sbrk.2
MLINKS+=chdir.2 fchdir.2
MLINKS+=chflags.2 fchflags.2 chflags.2 lchflags.2
MLINKS+=chmod.2 fchmod.2 chmod.2 lchmod.2
MLINKS+=chown.2 fchown.2 chown.2 lchown.2
MLINKS+=clock_gettime.2 clock_getres.2 clock_gettime.2 clock_settime.2
MLINKS+=dup.2 dup2.2
MLINKS+=extattr_get_file.2 extattr_set_file.2 \
	extattr_get_file.2 extattr_delete_file.2 \
	extattr_get_file.2 extattr_get_fd.2 \
	extattr_get_file.2 extattr_set_fd.2 \
	extattr_get_file.2 extattr_delete_fd.2 \
	extattr_get_file.2 extattr.2
MLINKS+=fhopen.2 fhstat.2 fhopen.2 fhstatfs.2
MLINKS+=getdirentries.2 getdents.2
MLINKS+=getgid.2 getegid.2
MLINKS+=getitimer.2 setitimer.2
MLINKS+=getlogin.2 getlogin_r.3
MLINKS+=getlogin.2 setlogin.2
MLINKS+=getpgrp.2 getpgid.2
MLINKS+=getpid.2 getppid.2
MLINKS+=getpriority.2 setpriority.2
MLINKS+=getrlimit.2 setrlimit.2
MLINKS+=getsockopt.2 setsockopt.2
MLINKS+=gettimeofday.2 settimeofday.2
MLINKS+=getuid.2 geteuid.2
MLINKS+=intro.2 errno.2
MLINKS+=kqueue.2 kevent.2
MLINKS+=kse.2 kse_create.2 kse.2 kse_exit.2 kse.2 kse_release.2 \
	kse.2 kse_wakeup.2 kse.2 kse_thr_interrupt.2
MLINKS+=mlock.2 munlock.2
MLINKS+=modnext.2 modfnext.2
MLINKS+=mount.2 unmount.2
MLINKS+=pathconf.2 fpathconf.2
MLINKS+=read.2 pread.2 read.2 readv.2
MLINKS+=recv.2 recvfrom.2 recv.2 recvmsg.2
MLINKS+=send.2 sendmsg.2 send.2 sendto.2
MLINKS+=setpgid.2 setpgrp.2
MLINKS+=setresuid.2 setresgid.2 setresuid.2 getresuid.2 setresuid.2 getresgid.2
MLINKS+=setuid.2 setegid.2 setuid.2 seteuid.2 setuid.2 setgid.2
MLINKS+=shmat.2 shmdt.2
MLINKS+=stat.2 fstat.2 stat.2 lstat.2
MLINKS+=statfs.2 fstatfs.2
MLINKS+=syscall.2 __syscall.2
MLINKS+=truncate.2 ftruncate.2
MLINKS+=utimes.2 futimes.2 utimes.2 lutimes.2
MLINKS+=wait.2 wait3.2 wait.2 wait4.2 wait.2 waitpid.2
MLINKS+=write.2 pwrite.2 write.2 writev.2
.if !defined(NO_P1003_1B)
MLINKS+=sched_get_priority_max.2 sched_get_priority_min.2 \
	sched_get_priority_max.2 sched_rr_get_interval.2
MLINKS+=sched_setparam.2 sched_getparam.2
MLINKS+=sched_setscheduler.2 sched_getscheduler.2
.endif
.endif
