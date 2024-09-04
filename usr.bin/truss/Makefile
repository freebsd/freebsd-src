PROG=	truss
SRCS=	main.c setup.c syscalls.c

LIBADD=	sysdecode

#CFLAGS+= -I${.CURDIR} -I. -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/sys

.include <bsd.prog.mk>
