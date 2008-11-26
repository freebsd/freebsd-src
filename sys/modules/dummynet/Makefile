# $FreeBSD: src/sys/modules/dummynet/Makefile,v 1.3.14.1.8.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:  ${.CURDIR}/../../netinet
KMOD=   dummynet
SRCS=   ip_dummynet.c
SRCS+=	opt_bdg.h opt_inet6.h

.if !defined(KERNBUILDDIR)
.if !defined(NO_INET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif


.include <bsd.kmod.mk>
