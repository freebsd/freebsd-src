#       $FreeBSD$

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_disc_mod
SRCS=   if_disc.c bpfilter.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NBPFILTER?=	0
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

.include <bsd.kmod.mk>
