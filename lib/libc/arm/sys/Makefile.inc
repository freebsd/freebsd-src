SRCS+=	__vdso_gettc.c \
	sched_getcpu_gen.c

MDASM= vfork.S cerror.S syscall.S
