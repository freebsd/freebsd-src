# $Id: Makefile.inc,v 1.1.1.1 1998/03/09 06:34:42 jb Exp $

MDASM+=	Ovfork.S brk.S cerror.S exect.S fork.S pipe.S ptrace.S \
	sbrk.S setlogin.S sigpending.S sigprocmask.S sigreturn.S \
	sigsuspend.S syscall.S

# Don't generate default code for these syscalls:
.ifdef NETBSD_SYSCALLS
NOASM=	break.o exit.o ftruncate.o lseek.o mmap.o posix_rename.o sstk.o \
	truncate.o vfork.o vtrace.o
.else
NOASM=	__semctl.o break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mlockall.o mmap.o msgctl.o msgget.o msgrcv.o msgsnd.o \
	munlockall.o openbsd_poll.o semconfig.o semget.o semop.o \
	setdomainname.o shmat.o shmctl.o shmdt.o shmget.o sstk.o \
	thr_sleep.o thr_wakeup.o truncate.o uname.o vfork.o yield.o
.endif

PSEUDO=	_getlogin.o

# Pseudo syscalls that are renamed as _thread_sys_{pseudo} when
# building libc_r.
PSEUDOR= _exit.o
