#       $Id: Makefile,v 1.5 1998/02/01 18:12:13 bde Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_disc
SRCS=   if_disc.c bpfilter.h	opt_inet.h
NOMAN=

NBPFILTER?=	0

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h	opt_inet.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
