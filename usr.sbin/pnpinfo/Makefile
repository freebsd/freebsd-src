# $FreeBSD$

PROG=	pnpinfo

SRCS=	pnpinfo.c
CFLAGS+=-I${.CURDIR}/../../sys
MAN8=	pnpinfo.8

.if ${MACHINE_ARCH} == "alpha"
LDADD+=	-lio
.endif

.PATH:	${.CURDIR}/../../contrib/pnpinfo

.include <bsd.prog.mk>
