PROG=	dc
SRCS=	dc.c decimal.c
CFLAGS+=-I${.CURDIR} -DHAVE_BCOPY=1 -DHAVE_BZERO=1
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
