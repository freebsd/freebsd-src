#	@(#)Makefile	5.1 (Berkeley) 8/24/92

PROG=	sed
SRCS=	compile.c main.c misc.c process.c 
CFLAGS+=-I${.CURDIR} -DHISTORIC_PRACTICE
LDADD+=	-lgnuregex
DPADD+=	/usr/lib/libgnuregex.a

.include <bsd.prog.mk>
