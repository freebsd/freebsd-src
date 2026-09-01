.include <src.opts.mk>

PROG=	truss
SRCS=	main.c setup.c syscall_filter.c syscalls.c

LIBADD=	sysdecode

#CFLAGS+= -I${.CURDIR} -I. -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/sys

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
