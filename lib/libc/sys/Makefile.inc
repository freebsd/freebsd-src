#	@(#)Makefile.inc	8.3 (Berkeley) 10/24/94
#	$Id: Makefile.inc,v 1.43 1997/10/15 16:29:14 bde Exp $

# sys sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/sys ${.CURDIR}/../libc/sys

.include "${.CURDIR}/../libc/${MACHINE}/sys/Makefile.inc"

# modules with non-default implementations on at least one architecture:
SRCS+=	Ovfork.S brk.S cerror.S exect.S fork.S pipe.S ptrace.S reboot.S \
	rfork.S sbrk.S setlogin.S sigpending.S sigprocmask.S sigreturn.S \
	sigsuspend.S syscall.S

# glue to provide compatibility between GCC 1.X and 2.X
SRCS+=	ftruncate.c lseek.c mmap.c truncate.c

# modules with default implementations on all architectures:
ASM=	__getcwd.o __syscall.o __sysctl.o \
	access.o acct.o adjtime.o \
	aio_cancel.o aio_error.o aio_read.o aio_return.o aio_suspend.o \
	aio_write.o \
	chdir.o chflags.o chmod.o chown.o chroot.o \
	clock_getres.o clock_gettime.o clock_settime.o \
	getdtablesize.o getegid.o \
	geteuid.o getfh.o getfsstat.o getgid.o getgroups.o getitimer.o \
	getpgrp.o getpgid.o getpid.o getppid.o getpriority.o \
	getrlimit.o getrusage.o getsid.o gettimeofday.o \
	getuid.o issetugid.o kill.o \
	kldfind.o kldfirstmod.o kldload.o kldnext.o kldstat.o kldunload.o \
	ktrace.o lchown.o \
	lfs_bmapv.o lfs_markv.o \
	lfs_segclean.o lfs_segwait.o link.o lio_listio.o lstat.o \
	madvise.o mincore.o minherit.o mkdir.o mlock.o \
	modfind.o modfnext.o modnext.o modstat.o \
	mount.o \
	mprotect.o msgsys.o msync.o munlock.o munmap.o \
	ntp_adjtime.o pathconf.o profil.o quotactl.o \
	readlink.o rename.o revoke.o rmdir.o \
	rtprio.o semsys.o setegid.o seteuid.o setgid.o \
	setgroups.o setitimer.o	setpgid.o setpriority.o	\
	setregid.o setreuid.o setrlimit.o \
	setsid.o settimeofday.o setuid.o shmsys.o \
	stat.o statfs.o \
	swapon.o symlink.o sync.o sysarch.o \
	umask.o undelete.o unlink.o unmount.o utimes.o utrace.o \
	vadvise.o

# Syscalls renamed as _thread_sys_{syscall} when building libc_r.
ASMR=	accept.o bind.o close.o connect.o dup.o dup2.o \
	execve.o fchdir.o fchflags.o fchmod.o fchown.o fcntl.o \
	flock.o fpathconf.o fstat.o fstatfs.o fsync.o getdirentries.o \
	getpeername.o getsockname.o getsockopt.o ioctl.o listen.o \
	mkfifo.o mknod.o nanosleep.o nfssvc.o open.o poll.o read.o readv.o \
	recvfrom.o recvmsg.o select.o sendmsg.o sendto.o setsockopt.o \
	shutdown.o sigaction.o sigaltstack.o signanosleep.o socket.o \
	socketpair.o \
	wait4.o write.o writev.o

PSEUDO=	_getlogin.o

# Pseudo syscalls that are renamed as _thread_sys_{pseudo} when
# building libc_r.
PSEUDOR=	_exit.o

OBJS+=	${ASM} ${ASMR} ${PSEUDO} ${PSEUDOR}

SASM=	${ASM:S/.o/.S/}

SASMR=	${ASMR:S/.o/.S/}

SPSEUDO= ${PSEUDO:S/.o/.S/}

SPSEUDOR= ${PSEUDOR:S/.o/.S/}

SRCS+=	${SASM} ${SASMR} ${SPSEUDO} ${SPSEUDOR}

# Generated files
CLEANFILES+=	${SASM} ${SASMR} ${SPSEUDO} ${SPSEUDOR}

${SASM}:
	printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' > ${.TARGET}

${SASMR}:
	printf '#include "SYS.h"\nPRSYSCALL(${.PREFIX})\n' > ${.TARGET}

${SPSEUDO}:
	printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' \
	    > ${.TARGET}

${SPSEUDOR}:
	printf '#include "SYS.h"\nPPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' \
	    > ${.TARGET}

.if ${LIB} == "c"
MAN2+=	_exit.2 accept.2 access.2 acct.2 adjtime.2 \
	bind.2 brk.2 chdir.2 chflags.2 \
	chmod.2 chown.2 chroot.2 clock_gettime.2 close.2 \
	connect.2 dup.2 execve.2 \
	fcntl.2 flock.2 fork.2 fsync.2 getdirentries.2 getdtablesize.2 \
	getfh.2 getfsstat.2 getgid.2 getgroups.2 getitimer.2 getlogin.2 \
	getpeername.2 getpgrp.2 getpid.2 getpriority.2 getrlimit.2 \
	getrusage.2 getsid.2 getsockname.2 \
	getsockopt.2 gettimeofday.2 getuid.2 \
	intro.2 ioctl.2 issetugid.2 kill.2 ktrace.2 link.2 listen.2 lseek.2 \
	madvise.2 mincore.2 minherit.2 mkdir.2 mkfifo.2 mknod.2 mlock.2 mmap.2 \
	mount.2 mprotect.2 msync.2 munmap.2 nanosleep.2 \
	nfssvc.2 open.2 pathconf.2 pipe.2 poll.2 profil.2 ptrace.2 quotactl.2 \
	read.2 readlink.2 reboot.2 recv.2 rename.2 revoke.2 rfork.2 rmdir.2 \
	rtprio.2 select.2 semctl.2 semget.2 semop.2 send.2 \
	setgroups.2 setpgid.2 setregid.2 setreuid.2 setsid.2 setuid.2 \
	shmat.2 shmctl.2 shmget.2 shutdown.2 \
	sigaction.2 sigaltstack.2 sigpending.2 sigprocmask.2 sigreturn.2 \
	sigstack.2 sigsuspend.2 socket.2 socketpair.2 stat.2 statfs.2 \
	swapon.2 symlink.2 sync.2 syscall.2 truncate.2 umask.2 undelete.2 \
	unlink.2 utimes.2 vfork.2 wait.2 write.2

MLINKS+=brk.2 sbrk.2
MLINKS+=dup.2 dup2.2
MLINKS+=chdir.2 fchdir.2
MLINKS+=chflags.2 fchflags.2
MLINKS+=chmod.2 fchmod.2
MLINKS+=chown.2 fchown.2 chown.2 lchown.2
MLINKS+=clock_gettime.2 clock_getres.2 clock_gettime.2 clock_settime.2
MLINKS+=getgid.2 getegid.2
MLINKS+=getitimer.2 setitimer.2
MLINKS+=getlogin.2 setlogin.2
MLINKS+=getpgrp.2 getpgid.2
MLINKS+=getpid.2 getppid.2
MLINKS+=getpriority.2 setpriority.2
MLINKS+=getrlimit.2 setrlimit.2
MLINKS+=getsockopt.2 setsockopt.2
MLINKS+=gettimeofday.2 settimeofday.2
MLINKS+=getuid.2 geteuid.2
MLINKS+=intro.2 errno.2
MLINKS+=lseek.2 seek.2
MLINKS+=mlock.2 munlock.2
MLINKS+=mount.2 unmount.2
MLINKS+=nanosleep.2 signanosleep.2
MLINKS+=pathconf.2 fpathconf.2
MLINKS+=read.2 readv.2
MLINKS+=recv.2 recvfrom.2 recv.2 recvmsg.2
MLINKS+=send.2 sendmsg.2 send.2 sendto.2
MLINKS+=setpgid.2 setpgrp.2
MLINKS+=setuid.2 setegid.2 setuid.2 seteuid.2 setuid.2 setgid.2
MLINKS+=shmat.2 shmdt.2
MLINKS+=stat.2 fstat.2 stat.2 lstat.2
MLINKS+=statfs.2 fstatfs.2
MLINKS+=syscall.2 __syscall.2
MLINKS+=truncate.2 ftruncate.2
MLINKS+=wait.2 wait3.2 wait.2 wait4.2 wait.2 waitpid.2
MLINKS+=write.2 writev.2
.endif
