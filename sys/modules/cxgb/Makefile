# $FreeBSD: src/sys/modules/cxgb/Makefile,v 1.10.2.4.8.1 2010/12/21 17:10:29 kensmith Exp $
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
