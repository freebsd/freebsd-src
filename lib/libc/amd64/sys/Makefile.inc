#	from: Makefile.inc,v 1.1 1993/09/03 19:04:23 jtc Exp
#	$Id: Makefile.inc,v 1.7 1997/10/16 14:41:25 bde Exp $

SRCS+=	i386_get_ldt.c i386_set_ldt.c

# _thread_create.S

MDASM=	Ovfork.S brk.S cerror.S exect.S fork.S pipe.S \
	ptrace.S reboot.S rfork.S sbrk.S setlogin.S sigpending.S \
	sigprocmask.S sigreturn.S sigsuspend.S syscall.S

# Don't generate default code for these syscalls:
NOASM=	__semctl.o break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mlockall.o mmap.o msgctl.o msgget.o msgrcv.o msgsnd.o \
	munlockall.o openbsd_poll.o semconfig.o semget.o semop.o \
	setdomainname.o shmat.o shmctl.o shmdt.o shmget.o sstk.o \
	thr_sleep.o thr_wakeup.o truncate.o uname.o vfork.o yield.o

PSEUDO=	_getlogin.o

# Pseudo syscalls that are renamed as _thread_sys_{pseudo} when
# building libc_r.
PSEUDOR=	_exit.o

.if ${LIB} == "c"
MAN2+=	i386_get_ldt.2

MLINKS+=i386_get_ldt.2 i386_set_ldt.2
.endif
