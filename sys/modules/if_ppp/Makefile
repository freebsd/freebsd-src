#       $Id: Makefile,v 1.7 1997/08/21 10:17:29 jmg Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_ppp_mod
SRCS=	bsd_comp.c if_ppp.c ppp_tty.c slcompress.c bpfilter.h ppp.h opt_ppp.h
NOMAN=
PSEUDO_LKM=
CFLAGS+= -I.

NPPP?=		2
PPP_BSDCOMP?=	1	# 0/1
PPP_DEFLATE?=	1	# 0/1
PPP_FILTER?=	0	# 0/1 - requires bpf to be configured in kernel
PROTOS?=	-DINET	# add -DIPX if you have IPX in the kernel

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h ppp.h opt_ppp.h

bpfilter.h:
	echo "#define NBPFILTER ${PPP_FILTER}" > bpfilter.h

ppp.h:
	echo "#define NPPP ${NPPP}" > ppp.h

opt_ppp.h:
.if ${PPP_BSDCOMP} > 0
	echo "#define PPP_BSDCOMP ${PPP_BSDCOMP}" > opt_ppp.h
.endif
.if ${PPP_DEFLATE} > 0
	echo "#define PPP_DEFLATE ${PPP_DEFLATE}" >> opt_ppp.h
.endif
.if ${PPP_FILTER} > 0
	echo "#define PPP_FILTER ${PPP_FILTER}" >> opt_ppp.h
.endif

.include <bsd.kmod.mk>
