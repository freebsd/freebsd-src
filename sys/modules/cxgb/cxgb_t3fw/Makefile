# $FreeBSD: src/sys/modules/cxgb/cxgb_t3fw/Makefile,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB} 

KMOD= cxgb_t3fw
SRCS+= cxgb_t3fw.c
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
