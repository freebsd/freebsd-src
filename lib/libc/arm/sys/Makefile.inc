SRCS+=	__vdso_gettc.c \
	sched_getcpu_gen.c

MDASM= Ovfork.S cerror.S syscall.S

# Don't generate default code for these syscalls:
NOASM+=	sbrk.o vfork.o
