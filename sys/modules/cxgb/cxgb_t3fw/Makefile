# $FreeBSD: src/sys/modules/cxgb/cxgb_t3fw/Makefile,v 1.3.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB} 

KMOD= cxgb_t3fw
SRCS+= cxgb_t3fw.c
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
