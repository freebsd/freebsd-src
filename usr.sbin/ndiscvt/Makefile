# $FreeBSD: src/usr.sbin/ndiscvt/Makefile,v 1.7.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../sys/compat/ndis

PROG=	ndiscvt
SRCS=	ndiscvt.c
SRCS+=	subr_pe.c
SRCS+=	inf.c inf-token.l inf-parse.y y.tab.h

MAN=	ndiscvt.8
MAN+=	ndisgen.8

WARNS?=	4

DPADD=  ${LIBL}
LDADD=  -ll

YFLAGS+=-v

CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../sys

CLEANFILES= y.output

FILES= windrv_stub.c
FILESNAME= windrv_stub.c
FILESDIR= /usr/share/misc

SCRIPTS= ndisgen.sh

.include <bsd.prog.mk>
