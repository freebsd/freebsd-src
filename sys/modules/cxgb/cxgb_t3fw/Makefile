# $FreeBSD: src/sys/modules/cxgb/cxgb_t3fw/Makefile,v 1.2.2.3.2.1 2010/02/10 00:26:20 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB} 

KMOD= cxgb_t3fw
SRCS+= cxgb_t3fw.c
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
