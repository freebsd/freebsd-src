PROGS=	pingpong proxy

MAN=

WARNS?=	6

.if defined(SRCDIR)
CFLAGS+=	-I${SRCDIR}/include	\
		-I${SRCDIR}/sys
.endif

.include <bsd.progs.mk>
