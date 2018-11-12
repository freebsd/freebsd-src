# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   saquery
SRCS=   saquery.c ibdiag_common.c
LIBADD= ibmad osmcomp osmvendor opensm
CFLAGS+= -I${DIAGPATH}/include
CFLAGS+= -DOSM_VENDOR_INTF_OPENIB -DVENDOR_RMPP_SUPPORT -DDUAL_SIDED_RMPP
MAN=	saquery.8

WARNS?= 1

.include <bsd.prog.mk>
