#       $Id: Makefile,v 1.8 1999/04/28 01:18:05 msmith Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_sl
SRCS=   if_sl.c slcompress.c bpf.h opt_inet.h sl.h
NOMAN=

NBPF?=	1
NSL?=		2
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpf.h opt_inet.h sl.h

bpf.h:
	echo "#define NBPF ${NBPF}" > bpf.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

sl.h:
	echo "#define NSL ${NSL}" > sl.h

.include <bsd.kmod.mk>
