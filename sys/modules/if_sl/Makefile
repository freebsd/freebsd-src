# $FreeBSD: src/sys/modules/if_sl/Makefile,v 1.13 1999/11/28 18:52:53 bde Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_sl
SRCS=   if_sl.c slcompress.c opt_inet.h sl.h
NOMAN=

NSL?=		2
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES=	sl.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

sl.h:
	echo "#define NSL ${NSL}" > sl.h

.include <bsd.kmod.mk>
