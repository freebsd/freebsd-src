#	$Id: Makefile,v 1.2 1998/03/12 12:54:25 eivind Exp $

PROG=	pnpinfo

SRCS=	pnpinfo.c
CFLAGS+=-I${.CURDIR}/../../sys
MAN8=	pnpinfo.8

.if ${MACHINE_ARCH} == "alpha"
LDADD+=	-lio
.endif

.PATH:	${.CURDIR}/../../contrib/pnpinfo

.include <bsd.prog.mk>
