PROG=	dc
SRCS=	dc-array.c dc-eval.c dc-misc.c dc-number.c dc-stack.c dc-string.c number.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../bc -D_POSIX_SOURCE
DPADD=	${LIBM}
LDADD=	-lm
SUBDIR+= doc

.PATH: ${.CURDIR}/../bc

.include <bsd.prog.mk>
