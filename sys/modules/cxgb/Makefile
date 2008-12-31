# $FreeBSD: src/sys/modules/cxgb/Makefile,v 1.10.2.4.2.1 2008/11/25 02:59:29 kensmith Exp $
SUBDIR= cxgb
SUBDIR+= toecore
SUBDIR+= tom
SUBDIR+= ${_iw_cxgb}
SUBDIR+= cxgb_t3fw

.if ${MACHINE_ARCH} == "i386"
_iw_cxgb = iw_cxgb
.endif

.if ${MACHINE_ARCH} == "amd64"
_iw_cxgb = iw_cxgb
.endif

.include <bsd.subdir.mk>
