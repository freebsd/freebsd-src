#       $FreeBSD$

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_tun_mod
SRCS=   if_tun.c bpfilter.h tun.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NBPFILTER?=	0
NTUN?=		2
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h tun.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

tun.h:
	echo "#define NTUN ${NTUN}" > tun.h

.include <bsd.kmod.mk>
