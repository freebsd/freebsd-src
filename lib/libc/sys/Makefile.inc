#	@(#)Makefile.inc	8.1 (Berkeley) 6/17/93

# sys sources
.PATH: ${.CURDIR}/${MACHINE}/sys ${.CURDIR}/sys

# modules with non-default implementations on at least one architecture:
SRCS+=	Ovfork.s brk.s cerror.s exect.s fork.s pipe.s ptrace.s reboot.s \
	sbrk.s setlogin.s sigpending.s sigprocmask.s sigreturn.s \
	sigsuspend.s syscall.s

# glue to provide compatibility between GCC 1.X and 2.X
SRCS+=	ftruncate.c lseek.c mmap.c truncate.c

# modules with default implementations on all architectures:
ASM=	accept.o access.o acct.o adjtime.o bind.o chdir.o chflags.o chmod.o \
	chown.o chroot.o close.o connect.o dup.o dup2.o execve.o fchdir.o \
	fchflags.o fchmod.o fchown.o fcntl.o flock.o fpathconf.o fstat.o \
	fstatfs.o fsync.o getdirentries.o getdtablesize.o getegid.o \
	geteuid.o getfh.o getfsstat.o getgid.o getgroups.o getitimer.o \
	getpeername.o getpgrp.o getpid.o getppid.o getpriority.o \
	getrlimit.o getrusage.o getsockname.o getsockopt.o gettimeofday.o \
	getuid.o ioctl.o kill.o ktrace.o lfs_bmapv.o lfs_markv.o \
	lfs_segclean.o lfs_segwait.o link.o listen.o lstat.o \
	madvise.o mincore.o mkdir.o mkfifo.o mknod.o mlock.o mount.o \
	mprotect.o msync.o munlock.o munmap.o nfssvc.o open.o \
	pathconf.o profil.o quotactl.o read.o \
	readlink.o readv.o recvfrom.o recvmsg.o rename.o revoke.o rmdir.o \
	select.o sendmsg.o sendto.o setegid.o seteuid.o setgid.o \
	setgroups.o setitimer.o setpgid.o setpriority.o setrlimit.o \
	setsid.o setsockopt.o settimeofday.o setuid.o shutdown.o \
	sigaction.o sigaltstack.o socket.o socketpair.o stat.o statfs.o \
	swapon.o symlink.o sync.o umask.o unlink.o unmount.o \
	utimes.o vadvise.o wait4.o write.o writev.o __syscall.o __sysctl.o

PSEUDO=	_exit.o _getlogin.o

OBJS+=	${ASM} ${PSEUDO}
POBJS+=	${ASM:.o=.po} ${PSEUDO:.o=.po}
CLEANFILES+=${ASM} ${PSEUDO} ${POBJS} 

${ASM}: ${.CURDIR}/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.PREFIX}.o ${.PREFIX}.po
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.o
	@${LD} -x -r ${.PREFIX}.o
	@mv a.out ${.PREFIX}.o
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.po
	@${LD} -x -r ${.PREFIX}.po
	@mv a.out ${.PREFIX}.po

${PSEUDO}: ${.CURDIR}/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.PREFIX}.o ${.PREFIX}.po
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.o
	@${LD} -x -r ${.PREFIX}.o
	@mv a.out ${.PREFIX}.o
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.po
	@${LD} -x -r ${.PREFIX}.po
	@mv a.out ${.PREFIX}.po

MAN2+=	accept.0 access.0 acct.0 adjtime.0 bind.0 brk.0 chdir.0 chflags.0 \
	chmod.0 chown.0 chroot.0 close.0 connect.0 dup.0 execve.0 _exit.0 \
	fcntl.0 flock.0 fork.0 fsync.0 getdirentries.0 getdtablesize.0 \
	getfh.0 getfsstat.0 getgid.0 getgroups.0 getitimer.0 getlogin.0 \
	getpeername.0 getpgrp.0 getpid.0 getpriority.0 getrlimit.0 \
	getrusage.0 getsockname.0 getsockopt.0 gettimeofday.0 getuid.0 \
	intro.0 ioctl.0 kill.0 ktrace.0 link.0 listen.0 lseek.0 \
	mkdir.0 mkfifo.0 mknod.0 madvise.0 mincore.0 mlock.0 mmap.0 \
	mount.0 mprotect.0 msync.0 munmap.0 \
	nfssvc.0 open.0 pathconf.0 pipe.0 profil.0 ptrace.0 quotactl.0 \
	munmap.0 nfssvc.0 open.0 pipe.0 profil.0 ptrace.0 quotactl.0 \
	read.0 readlink.0 reboot.0 recv.0 rename.0 revoke.0 rmdir.0 \
	select.0 send.0 setgroups.0 setpgid.0 setsid.0 setuid.0 shutdown.0 \
	sigaction.0 sigpending.0 sigprocmask.0 sigreturn.0 sigaltstack.0 \
	sigstack.0 sigsuspend.0 socket.0 socketpair.0 stat.0 statfs.0 \
	swapon.0 symlink.0 sync.0 syscall.0 truncate.0 umask.0 unlink.0 \
	utimes.0 vfork.0 wait.0 write.0

MLINKS+=brk.2 sbrk.2
MLINKS+=dup.2 dup2.2
MLINKS+=chdir.2 fchdir.2
MLINKS+=chflags.2 fchflags.2
MLINKS+=chmod.2 fchmod.2
MLINKS+=chown.2 fchown.2
MLINKS+=getgid.2 getegid.2
MLINKS+=getitimer.2 setitimer.2
MLINKS+=getlogin.2 setlogin.2
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
MLINKS+=pathconf.2 fpathconf.2
MLINKS+=read.2 readv.2
MLINKS+=recv.2 recvfrom.2 recv.2 recvmsg.2
MLINKS+=send.2 sendmsg.2 send.2 sendto.2
MLINKS+=setuid.2 setegid.2 setuid.2 seteuid.2 setuid.2 setgid.2
MLINKS+=stat.2 fstat.2 stat.2 lstat.2
MLINKS+=statfs.2 fstatfs.2
MLINKS+=syscall.2 __syscall.2
MLINKS+=truncate.2 ftruncate.2
MLINKS+=wait.2 wait3.2 wait.2 wait4.2 wait.2 waitpid.2
MLINKS+=write.2 writev.2
