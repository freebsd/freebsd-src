# $FreeBSD: src/sys/modules/cxgb/cxgb_t3fw/Makefile,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB} 

KMOD= cxgb_t3fw
SRCS+= cxgb_t3fw.c
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
