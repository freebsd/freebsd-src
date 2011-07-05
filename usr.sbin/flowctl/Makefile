#
# $FreeBSD$
#

.include <bsd.own.mk>
	
PROG=	flowctl
MAN=	flowctl.8

WARNS?= 2
DPADD=  ${LIBNETGRAPH}
LDADD=  -lnetgraph

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

CFLAGS+= -I.

.include <bsd.prog.mk>
