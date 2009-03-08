# $FreeBSD$

.if !defined(COMPAT_32BIT)
SRCS+= \
	i386_clr_watch.c \
	i386_get_ioperm.c \
	i386_set_ioperm.c \
	i386_set_watch.c \
	i386_vm86.c
.endif

SRCS += \
	i386_get_fsbase.c \
	i386_get_gsbase.c \
	i386_get_ldt.c \
	i386_set_fsbase.c \
	i386_set_gsbase.c \
	i386_set_ldt.c

MDASM = \
	Ovfork.S \
	brk.S \
	cerror.S \
	exect.S \
	getcontext.S \
	pipe.S \
	ptrace.S \
	reboot.S \
	sbrk.S \
	setlogin.S \
	sigreturn.S \
	syscall.S

# Don't generate default code for these syscalls:
NOASM = \
	break.o \
	exit.o \
	getlogin.o \
	openbsd_poll.o \
	sstk.o \
	vfork.o \
	yield.o

PSEUDO = \
	_getlogin.o \
	_exit.o

.if !defined(WITHOUT_SYSCALL_COMPAT)
PSEUDO += \
	_pread.o \
	_pwrite.o \
	_lseek.o \
	_mmap.o \
	_ftruncate.o \
	_truncate.o
.endif

