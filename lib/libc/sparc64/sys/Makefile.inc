# $FreeBSD$

SRCS+=	__sparc_sigtramp_setup.c \
	__sparc_utrap.c \
	__sparc_utrap_align.c \
	__sparc_utrap_emul.c \
	__sparc_utrap_fp_disabled.S \
	__sparc_utrap_gen.S \
	__sparc_utrap_install.c \
	__sparc_utrap_setup.c \
	sigcode.S

CFLAGS+= -I${.CURDIR}/sparc64/fpu

MDASM+=	brk.S cerror.S exect.S pipe.S ptrace.S sbrk.S setlogin.S sigaction.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mlockall.o mmap.o munlockall.o openbsd_poll.o pread.o \
	pwrite.o setdomainname.o sstk.o truncate.o uname.o yield.o

PSEUDO=	_getlogin.o _exit.o

