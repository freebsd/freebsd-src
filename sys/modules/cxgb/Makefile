# $FreeBSD$
SUBDIR= cxgb
SUBDIR+= toecore
SUBDIR+= tom
#SUBDIR+= ${_iw_cxgb}
SUBDIR+= cxgb_t3fw

.if ${MACHINE_ARCH} == "i386"
_iw_cxgb = iw_cxgb
.endif

.if ${MACHINE_ARCH} == "amd64"
_iw_cxgb = iw_cxgb
.endif

.include <bsd.subdir.mk>
