#       $Id: Makefile,v 1.3 1997/02/22 12:48:04 peter Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_disc_mod
SRCS=   if_disc.c bpfilter.h	opt_inet.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NBPFILTER?=	0

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h	opt_inet.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
