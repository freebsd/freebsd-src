SRCS+=	\
	amd64_get_fsbase.c \
	amd64_get_gsbase.c \
	amd64_set_fsbase.c \
	amd64_set_gsbase.c

MDASM=	vfork.S cerror.S getcontext.S

# Don't generate default code for these syscalls:
NOASM+=	sbrk.o vfork.o
