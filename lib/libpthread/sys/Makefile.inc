#	@(#)Makefile.inc	8.1 (Berkeley) 6/17/93

# sys sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/sys ${.CURDIR}/../libc/sys \
	${.CURDIR}/sys

# modules with non-default implementations on at least one architecture:
SRCS+=	Ovfork.S brk.S cerror.S exect.S fork.S pipe.S ptrace.S reboot.S \
	sbrk.S setlogin.S sigpending.S sigprocmask.S sigreturn.S \
	sigsuspend.S syscall.S __error.c

# glue to provide compatibility between GCC 1.X and 2.X
SRCS+=	ftruncate.c lseek.c mmap.c truncate.c

# modules with default implementations on all architectures:
ASM=	access.o acct.o adjtime.o chdir.o chflags.o chmod.o \
	chown.o chroot.o getdtablesize.o getegid.o geteuid.o \
	getfh.o getfsstat.o getgid.o getgroups.o getitimer.o \
	getpgrp.o getpid.o getppid.o getpriority.o \
	getrlimit.o getrusage.o gettimeofday.o getuid.o kill.o \
	ktrace.o lfs_bmapv.o lfs_markv.o lfs_segclean.o \
	lfs_segwait.o link.o lstat.o madvise.o mincore.o \
	mkdir.o mlock.o mount.o mprotect.o msgsys.o msync.o \
	munlock.o munmap.o ntp_adjtime.o pathconf.o profil.o \
	quotactl.o readlink.o rename.o revoke.o rmdir.o rtprio.o \
	semsys.o setegid.o seteuid.o setgid.o setgroups.o setitimer.o \
	setpgid.o setpriority.o setregid.o setreuid.o setrlimit.o \
	setsid.o settimeofday.o setuid.o shmsys.o stat.o statfs.o \
	swapon.o symlink.o sync.o sysarch.o umask.o unlink.o \
	unmount.o utimes.o utrace.o vadvise.o __syscall.o __sysctl.o

# Syscalls renamed as _thread_sys_{syscall}.
THREADASM=	accept.o bind.o close.o connect.o dup.o dup2.o \
	execve.o fchdir.o fchflags.o fchmod.o fchown.o fcntl.o \
	flock.o fpathconf.o fstat.o fstatfs.o fsync.o getdirentries.o \
	getpeername.o getsockname.o getsockopt.o ioctl.o listen.o \
	mkfifo.o mknod.o nfssvc.o open.o read.o readv.o recvfrom.o \
	recvmsg.o select.o sendmsg.o sendto.o setsockopt.o \
	shutdown.o sigaction.o sigaltstack.o socket.o socketpair.o \
	wait4.o write.o writev.o

PSEUDO=	_getlogin.o

THREADPSEUDO=	_exit.o

OBJS+=	${ASM} ${THREADASM} ${PSEUDO} ${THREADPSEUDO}

${ASM}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.o
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.o
	@${LD} -x -r ${.PREFIX}.o
	@mv a.out ${.PREFIX}.o

PASM=	${ASM:.o=.po}
${PASM}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.po
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.po
	@${LD} -x -r ${.PREFIX}.po
	@mv a.out ${.PREFIX}.po

SASM=	${ASM:.o=.so}
${SASM}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.so
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -k -o ${.PREFIX}.so

${THREADASM}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.o
	@printf '#include "SYS.h"\nPRSYSCALL(${.PREFIX})\n' > ${.PREFIX}.S
	@${CC} ${CFLAGS:M-[ID]*} ${AINC} -c ${.PREFIX}.S -o ${.PREFIX}.o
	@rm -f ${.PREFIX}.S

PTHREADASM=	${THREADASM:.o=.po}
${PTHREADASM}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.po
	@printf '#include "SYS.h"\nPRSYSCALL(${.PREFIX})\n' > ${.PREFIX}.S
	@${CC} -DPROF ${CFLAGS:M-[ID]*} ${AINC} -c ${.PREFIX}.S -o ${.PREFIX}.po
	@rm -f ${.PREFIX}.S

STHREADASM=	${THREADASM:.o=.so}
${STHREADASM}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.so
	@printf '#include "SYS.h"\nPRSYSCALL(${.PREFIX})\n' > ${.PREFIX}.S
	${CC} -fpic -DPIC ${CFLAGS:M-[ID]*} ${AINC} -c ${.PREFIX}.S -o ${.PREFIX}.so
	@${LD} -O ${.TARGET} -x -r ${.TARGET}
	@rm -f ${.PREFIX}.S

${PSEUDO}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.o
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.o
	@${LD} -x -r ${.PREFIX}.o
	@mv a.out ${.PREFIX}.o

PPSEUDO=${PSEUDO:.o=.po}
${PPSEUDO}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.po
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.PREFIX}.po
	@${LD} -x -r ${.PREFIX}.po
	@mv a.out ${.PREFIX}.po

SPSEUDO=${PSEUDO:.o=.so}
${SPSEUDO}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.so
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -k -o ${.PREFIX}.so

${THREADPSEUDO}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.o
	@printf '#include "SYS.h"\nPPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' > ${.PREFIX}.S
	@${CC} ${CFLAGS:M-[ID]*} ${AINC} -c ${.PREFIX}.S -o ${.PREFIX}.o
	@rm -f ${.PREFIX}.S

THREADPPSEUDO=${THREADPSEUDO:.o=.po}
${THREADPPSEUDO}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.po
	@printf '#include "SYS.h"\nPPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' > ${.PREFIX}.S
	@${CC} -DPROF ${CFLAGS:M-[ID]*} ${AINC} -c ${.PREFIX}.S -o ${.PREFIX}.po
	@rm -f ${.PREFIX}.S

THREADSPSEUDO=${THREADPSEUDO:.o=.so}
${THREADSPSEUDO}: ${.CURDIR}/../libc/${MACHINE}/SYS.h /usr/include/sys/syscall.h
	@${ECHO} creating ${.PREFIX}.so
	@printf '#include "SYS.h"\nPPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' > ${.PREFIX}.S
	${CC} -fpic -DPIC ${CFLAGS:M-[ID]*} ${AINC} -c ${.PREFIX}.S -o ${.PREFIX}.so
	@${LD} -O ${.TARGET} -x -r ${.TARGET}
	@rm -f ${.PREFIX}.S
