#       $Id: Makefile,v 1.2 1995/03/20 19:33:35 wollman Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_ppp_mod
SRCS=   if_ppp.c pppcompress.c bpfilter.h ppp.h vnode_if.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NBPFILTER?=	0
NPPP?=		2
PROTOS?=	-DINET

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h ppp.h vnode_if.h vnode_if.c

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

ppp.h:
	echo "#define NPPP ${NPPP}" > ppp.h

.include <bsd.kmod.mk>
