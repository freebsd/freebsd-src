# $FreeBSD: src/sys/modules/if_sl/Makefile,v 1.7.2.1 1999/08/29 16:27:17 peter Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_sl
SRCS=   if_sl.c slcompress.c bpfilter.h opt_inet.h sl.h
NOMAN=

NBPFILTER?=	0
NSL?=		2
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h opt_inet.h sl.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

sl.h:
	echo "#define NSL ${NSL}" > sl.h

.include <bsd.kmod.mk>
