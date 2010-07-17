#	$FreeBSD: src/sbin/ipf/Makefile.inc,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

CFLAGS+=	-I${.CURDIR}/../../../contrib/ipfilter
CFLAGS+=	-I${.CURDIR}/../../../contrib/ipfilter/tools
CFLAGS+=	-I${.CURDIR}/../../../sys
CFLAGS+=	-I${.CURDIR}/../../../sys/contrib/ipfilter
CFLAGS+=	-DSTATETOP -D__UIO_EXPOSE

LIBIPF=		${.OBJDIR}/../libipf/libipf.a
DPADD+=		${LIBIPF} ${LIBKVM}
LDADD+=		${LIBIPF} -lkvm

CLEANFILES+=	y.tab.c y.tab.h

.PATH:	${.CURDIR}/../../../contrib/ipfilter	\
	${.CURDIR}/../../../contrib/ipfilter/lib	\
	${.CURDIR}/../../../contrib/ipfilter/tools	\
	${.CURDIR}/../../../contrib/ipfilter/man

.include "../Makefile.inc"
