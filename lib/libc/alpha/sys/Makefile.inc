# $Id$

MDASM+=	Ovfork.S brk.S cerror.S exect.S fork.S pipe.S ptrace.S \
	sbrk.S setlogin.S sigpending.S sigprocmask.S sigreturn.S \
	sigsuspend.S syscall.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o lseek.o mmap.o posix_rename.o sstk.o \
	truncate.o vfork.o vtrace.o

# Pseudo syscalls that are renamed as _thread_sys_{pseudo} when
# building libc_r.
PSEUDOR= _exit.o
