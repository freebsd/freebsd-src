# $FreeBSD$

MDASM+=	Ovfork.S brk.S cerror.S exect.S fork.S pipe.S ptrace.S \
	sbrk.S setlogin.S sigreturn.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mmap.o openbsd_poll.o pread.o \
	pwrite.o setdomainname.o sstk.o truncate.o uname.o vfork.o yield.o

PSEUDO=	_getlogin.o _exit.o

