#       $Id: Makefile,v 1.6 1998/10/16 04:30:44 peter Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_disc
SRCS=   if_disc.c bpfilter.h	opt_inet.h
NOMAN=

NBPFILTER?=	1

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h	opt_inet.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
