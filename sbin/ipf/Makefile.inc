#	$FreeBSD: src/sbin/ipf/Makefile.inc,v 1.2 2005/05/17 14:29:06 ru Exp $

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
