# $FreeBSD: src/sys/modules/if_faith/Makefile,v 1.1.2.1 2001/12/15 01:07:54 brooks Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_faith
SRCS=	if_faith.c opt_inet.h opt_inet6.h
CFLAGS+= -DNFAITH=0
NOMAN=

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

.include <bsd.kmod.mk>
