#       $Id: Makefile,v 1.6 1997/02/22 12:48:06 peter Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_ppp_mod
SRCS=	bsd_comp.c if_ppp.c ppp_tty.c slcompress.c bpfilter.h ppp.h opt_ppp.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NBPFILTER?=	0
NPPP?=		2
PPP_BSDCOMP?=	1
PPP_DEFLATE?=	1
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h ppp.h opt_ppp.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

ppp.h:
	echo "#define NPPP ${NPPP}" > ppp.h

opt_ppp.h:
	echo "#define PPP_BSDCOMP ${PPP_BSDCOMP}" > opt_ppp.h
	echo "#define PPP_DEFLATE ${PPP_DEFLATE}" >> opt_ppp.h

.include <bsd.kmod.mk>
