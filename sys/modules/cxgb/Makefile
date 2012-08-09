# $FreeBSD$
SUBDIR= cxgb
SUBDIR+= cxgb_t3fw
SUBDIR+= ${_tom}
SUBDIR+= ${_iw_cxgb}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
_tom=		tom
_iw_cxgb=	iw_cxgb
.endif

.include <bsd.subdir.mk>
