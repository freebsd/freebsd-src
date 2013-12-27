# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= cxgb
SUBDIR+= cxgb_t3fw
SUBDIR+= ${_tom}
SUBDIR+= ${_iw_cxgb}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
_tom=		tom
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_iw_cxgb=	iw_cxgb
.endif
.endif

.include <bsd.subdir.mk>
