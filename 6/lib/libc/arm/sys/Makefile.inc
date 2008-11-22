# $FreeBSD$

MDASM= Ovfork.S brk.S cerror.S pipe.S ptrace.S sbrk.S shmat.S sigreturn.S syscall.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mmap.o openbsd_poll.o pread.o \
	pwrite.o setdomainname.o sstk.o truncate.o uname.o vfork.o yield.o
PSEUDO= _exit.o _getlogin.o
