#       $Id: Makefile,v 1.3 1995/04/15 17:36:53 ats Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_ppp_mod
SRCS=	bsd_comp.c if_ppp.c ppp_tty.c slcompress.c bpfilter.h ppp.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NBPFILTER?=	0
NPPP?=		2
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h ppp.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

ppp.h:
	echo "#define NPPP ${NPPP}" > ppp.h

.include <bsd.kmod.mk>
