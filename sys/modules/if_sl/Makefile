#       $Id: Makefile,v 1.4 1997/02/22 12:48:08 peter Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_sl_mod
SRCS=   if_sl.c slcompress.c bpfilter.h opt_inet.h sl.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

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
