#       $Id: Makefile,v 1.2.6.1 1998/03/06 05:26:56 jkh Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_sl_mod
SRCS=   if_sl.c slcompress.c bpfilter.h opt_inet.h sl.h
NOMAN=
PSEUDO_LKM=

NBPFILTER?=	0
NSL?=		2
PROTOS?=	-DINET

CFLAGS+= -I. ${PROTOS}
CLEANFILES+=	bpfilter.h opt_inet.h sl.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

sl.h:
	echo "#define NSL ${NSL}" > sl.h

.include <bsd.kmod.mk>
