PROG=	dc
CFLAGS+=-I${.CURDIR} -DHAVE_BCOPY=1 -DHAVE_BZERO=1
SRCS=	dc.c decimal.c
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
