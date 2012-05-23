# $FreeBSD$

.include <bsd.own.mk>

PROG=	jls
MAN=	jls.8
DPADD=	${LIBJAIL}
LDADD=	-ljail

WARNS?=	6

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif
.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif

.include <bsd.prog.mk>
