#       $Id: Makefile,v 1.7 1999/04/28 01:17:58 msmith Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_disc
SRCS=   if_disc.c bpf.h	opt_inet.h
NOMAN=

NBPF?=	1

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpf.h	opt_inet.h

bpf.h:
	echo "#define NBPF ${NBPF}" > bpf.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
