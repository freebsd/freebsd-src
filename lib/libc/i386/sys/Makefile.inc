#	from: Makefile.inc,v 1.1 1993/09/03 19:04:23 jtc Exp
# $FreeBSD$

.if !defined(COMPAT_32BIT)
SRCS+=	i386_clr_watch.c i386_get_ioperm.c i386_get_ldt.c i386_set_ioperm.c \
	i386_set_ldt.c i386_set_watch.c i386_vm86.c
.else
SRCS+=	_amd64_get_fsbase.c _amd64_get_gsbase.c _amd64_set_fsbase.c _amd64_set_gsbase.c
.endif

MDASM=	Ovfork.S brk.S cerror.S exect.S getcontext.S pipe.S ptrace.S \
	reboot.S sbrk.S setlogin.S sigreturn.S syscall.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mmap.o openbsd_poll.o pread.o \
	pwrite.o setdomainname.o sstk.o truncate.o uname.o vfork.o yield.o

PSEUDO=	_getlogin.o _exit.o

MAN+=	i386_get_ioperm.2 i386_get_ldt.2 i386_vm86.2
MAN+=	i386_set_watch.3

MLINKS+=i386_get_ioperm.2 i386_set_ioperm.2
MLINKS+=i386_get_ldt.2 i386_set_ldt.2
MLINKS+=i386_set_watch.3 i386_clr_watch.3
