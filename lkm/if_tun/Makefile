#       $Id: Makefile,v 1.3 1997/02/22 12:48:10 peter Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_tun_mod
SRCS=   if_tun.c bpfilter.h opt_inet.h tun.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NBPFILTER?=	0
NTUN?=		2

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h opt_inet.h tun.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

tun.h:
	echo "#define NTUN ${NTUN}" > tun.h

.include <bsd.kmod.mk>
