# $FreeBSD: src/sys/modules/cxgb/cxgb_t3fw/Makefile,v 1.2.2.3.4.1 2010/12/21 17:10:29 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB} 

KMOD= cxgb_t3fw
SRCS+= cxgb_t3fw.c
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
