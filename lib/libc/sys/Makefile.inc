#	From: @(#)Makefile.inc	5.11 (Berkeley) 6/23/91
#	$Id: Makefile.inc,v 1.16 1994/03/16 19:01:06 wollman Exp $

# sys sources
.PATH: ${.CURDIR}/${MACHINE}/sys ${.CURDIR}/sys

# modules with non-default implementations on at least one architecture:
SRCS+=	Ovfork.S brk.S cerror.S exect.S fork.S pipe.S ptrace.S reboot.S \
	sbrk.S setlogin.S sigpending.S sigprocmask.S sigreturn.S \
	sigsuspend.S syscall.S

# modules with default implementations on all architectures:
ASM=	accept.o access.o acct.o adjtime.o async_daemon.o bind.o chdir.o \
	chflags.o chmod.o chown.o chroot.o close.o connect.o dup.o dup2.o \
	execve.o fchdir.o fchflags.o fchmod.o fchown.o fcntl.o flock.o \
	fstat.o fstatfs.o fsync.o ftruncate.o getdirentries.o getdomainname.o \
	getdtablesize.o getegid.o geteuid.o getfh.o getfsstat.o getgid.o \
	getgroups.o gethostid.o gethostname.o getitimer.o getkerninfo.o \
	getpagesize.o getpeername.o getpgrp.o getpid.o getppid.o \
	getpriority.o getrlimit.o getrusage.o getsockname.o getsockopt.o \
	gettimeofday.o getuid.o ioctl.o kill.o ktrace.o link.o listen.o \
	lseek.o lstat.o madvise.o mincore.o mkdir.o mkfifo.o mknod.o \
	mmap.o mount.o mprotect.o msgsys.o msync.o munmap.o nfssvc.o \
	ntp_adjtime.o ntp_gettime.o open.o \
	profil.o quotactl.o read.o readlink.o readv.o recvfrom.o recvmsg.o \
	rename.o revoke.o rmdir.o select.o semsys.o sendmsg.o sendto.o \
	setdomainname.o setegid.o seteuid.o setgid.o setgroups.o sethostid.o \
	sethostname.o setitimer.o setpgid.o setpriority.o setregid.o \
	setreuid.o setrlimit.o setsid.o setsockopt.o settimeofday.o setuid.o \
	shmsys.o shutdown.o sigaction.o sigstack.o socket.o socketpair.o \
	stat.o statfs.o swapon.o symlink.o sync.o sysarch.o truncate.o \
	umask.o uname.o unlink.o unmount.o utimes.o vadvise.o vm_allocate.o \
	vm_deallocate.o vm_inherit.o vm_protect.o wait4.o write.o writev.o


PASM=		${ASM:.o=.po}
SASM=		${ASM:.o=.so}

PSEUDO=	_exit.o _getlogin.o
PPSEUDO=	${PSEUDO:.o=.po}
SPSEUDO=	${PSEUDO:.o=.so}

OBJS+=		${ASM} ${PSEUDO}
POBJS+=		${PASM} ${PPSEUDO}
SOBJS+=		${SASM} ${SPSEUDO}

${ASM}: ${.CURDIR}/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.PREFIX}.o
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.o
#	@${LD} -x -r ${.PREFIX}.o
#	@mv a.out ${.PREFIX}.o

${PASM}: ${.CURDIR}/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.PREFIX}.po
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.po
#	@${LD} -x -r ${.PREFIX}.po
#	@mv a.out ${.PREFIX}.po

${SASM}: ${.CURDIR}/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	   ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -k -o ${.TARGET}

${PSEUDO}: ${.CURDIR}/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.PREFIX}.o 
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.o
#	@${LD} -x -r ${.PREFIX}.o
#	@mv a.out ${.PREFIX}.o

${PPSEUDO}: ${.CURDIR}/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.PREFIX}.po
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.po
#	@${LD} -x -r ${.PREFIX}.po
#	@mv a.out ${.PREFIX}.po

${SPSEUDO}: ${.CURDIR}/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	   ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -k -o ${.TARGET}

MAN2+=	sys/accept.2 sys/access.2 sys/acct.2 sys/adjtime.2 sys/async_daemon.2 \
	sys/bind.2 sys/brk.2 sys/chdir.2 sys/chflags.2 sys/chmod.2 sys/chown.2 \
	sys/chroot.2 sys/close.2 sys/connect.2 sys/dup.2 sys/execve.2 \
	sys/_exit.2 sys/fcntl.2 sys/flock.2 sys/fork.2 sys/fsync.2 \
	sys/getdirentries.2 sys/getdomainname.2 sys/getdtablesize.2 \
	sys/getfh.2 sys/getfsstat.2 \
	sys/getgid.2 sys/getgroups.2 sys/gethostid.2 sys/gethostname.2 \
	sys/getitimer.2 sys/getlogin.2 sys/getpagesize.2 sys/getpeername.2 \
	sys/getpgrp.2 sys/getpid.2 sys/getpriority.2 sys/getrlimit.2 \
	sys/getrusage.2 sys/getsockname.2 sys/getsockopt.2 sys/gettimeofday.2 \
	sys/getuid.2 sys/intro.2 sys/ioctl.2 sys/kill.2 sys/link.2 \
	sys/listen.2 sys/lseek.2 sys/mkdir.2 sys/mkfifo.2 sys/mknod.2 \
	sys/madvise.2 sys/mincore.2 sys/mmap.2 sys/mount.2 sys/mprotect.2 \
	sys/msync.2 sys/munmap.2 sys/nfssvc.2 sys/open.2 sys/pipe.2 \
	sys/quotactl.2 sys/read.2 sys/readlink.2 sys/reboot.2 sys/recv.2 \
	sys/rename.2 sys/rmdir.2 sys/select.2 sys/send.2 sys/setgroups.2 \
	sys/setpgid.2 sys/setregid.2 sys/setreuid.2 sys/setsid.2 sys/setuid.2 \
	sys/shutdown.2 sys/sigaction.2 sys/sigprocmask.2 sys/sigreturn.2 \
	sys/sigstack.2 sys/sigsuspend.2 sys/socket.2 sys/socketpair.2 \
	sys/stat.2 sys/statfs.2 sys/swapon.2 sys/symlink.2 sys/sync.2 \
	sys/syscall.2 sys/truncate.2 sys/umask.2 sys/uname.2 sys/unlink.2 \
	sys/utimes.2 sys/vfork.2 sys/wait.2 sys/write.2

MLINKS+=brk.2 sbrk.2
MLINKS+=dup.2 dup2.2
MLINKS+=chdir.2 fchdir.2
MLINKS+=chflags.2 fchflags.2
MLINKS+=chmod.2 fchmod.2
MLINKS+=chown.2 fchown.2
MLINKS+=getdomainname.2 setdomainname.2
MLINKS+=getgid.2 getegid.2
MLINKS+=gethostid.2 sethostid.2
MLINKS+=gethostname.2 sethostname.2
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
MLINKS+=mount.2 unmount.2
MLINKS+=read.2 readv.2
MLINKS+=recv.2 recvfrom.2 recv.2 recvmsg.2
MLINKS+=send.2 sendmsg.2 send.2 sendto.2
MLINKS+=setuid.2 setegid.2 setuid.2 seteuid.2 setuid.2 setgid.2
MLINKS+=setpgid.2 setpgrp.2
MLINKS+=stat.2 fstat.2 stat.2 lstat.2
MLINKS+=statfs.2 fstatfs.2
MLINKS+=truncate.2 ftruncate.2
MLINKS+=wait.2 wait3.2 wait.2 wait4.2 wait.2 waitpid.2
MLINKS+=write.2 writev.2
