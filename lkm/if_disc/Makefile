#       $Id: Makefile,v 1.1 1995/03/20 19:25:46 wollman Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_disc_mod
SRCS=   if_disc.c bpfilter.h	opt_inet.h
NOMAN=
PSEUDO_LKM=

NBPFILTER?=	0

CFLAGS+= -I. ${PROTOS}
CLEANFILES+=	bpfilter.h	opt_inet.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
