# $FreeBSD$

S=	${.CURDIR}/..
.PATH: $S

PROG=	mrinfo
MAN=	mrinfo.8
BINOWN=	root
BINMODE= 4555

CFLAGS+= -I$S

DPADD=	${LIBMROUTED}
LDADD=	../common/libmrouted.a

.include <bsd.prog.mk>
