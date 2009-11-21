# $FreeBSD: src/sys/modules/cxgb/cxgb_t3fw/Makefile,v 1.3.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB} 

KMOD= cxgb_t3fw
SRCS+= cxgb_t3fw.c
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
