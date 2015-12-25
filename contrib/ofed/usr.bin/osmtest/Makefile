# $FreeBSD$

.include "../Makefile.inc"

OPENSM=	${.CURDIR}/../../management/opensm
.PATH:	${OPENSM}/osmtest ${OPENSM}/man

BINDIR=	${TESTSBASE}/usr.bin/opensm
PROG=	osmtest

SRCS=	main.c \
	osmt_inform.c \
	osmt_mtl_regular_qp.c \
	osmt_multicast.c \
	osmt_service.c \
	osmt_slvl_vl_arb.c \
	osmtest.c

LIBADD= ibcommon ibmad ibumad osmvendor osmcomp opensm pthread

CFLAGS=	-DVENDOR_RMPP_SUPPORT -DDUAL_SIDED_RMPP \
	-I${OPENSM}/osmtest/include

MAN=	osmtest.8

WARNS?= 1

.include <bsd.prog.mk>
