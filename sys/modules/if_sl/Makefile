#       $Id: Makefile,v 1.1 1995/03/20 19:25:47 wollman Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_sl_mod
SRCS=   if_sl.c slcompress.c bpfilter.h sl.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NBPFILTER?=	0
NSL?=		2
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h sl.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

sl.h:
	echo "#define NSL ${NSL}" > sl.h

.include <bsd.kmod.mk>
