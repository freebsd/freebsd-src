# $FreeBSD$

.PATH:  ${.CURDIR}/../../net
KMOD=   if_disc
SRCS=   if_disc.c opt_inet.h
NOMAN=

NBPF?=	1

CFLAGS+= ${PROTOS}

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
