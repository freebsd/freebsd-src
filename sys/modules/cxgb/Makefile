# $FreeBSD$
SUBDIR= cxgb
SUBDIR+= ${_toecore}
SUBDIR+= ${_tom}
SUBDIR+= ${_iw_cxgb}
SUBDIR+= cxgb_t3fw

.if defined(SYSDIR)
_sysdir = ${SYSDIR}
.endif

# Based on bsd.kmod.mk but we don't modify SYSDIR in this one.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. ${.CURDIR}/../../../.. \
    /sys /usr/src/sys
.if !defined(_sysdir) && exists(${_dir}/kern/) && exists(${_dir}/conf/kmod.mk)
_sysdir = ${_dir}
.endif
.endfor
.if !defined(_sysdir) || !exists(${_sysdir}/kern/) || \
    !exists(${_sysdir}/conf/kmod.mk)
.error "can't find kernel source tree"
.endif

_toe_header = ${_sysdir}/netinet/toedev.h

.if exists(${_toe_header})
_toecore = toecore
_tom = tom
.endif

.if ${MACHINE_ARCH} == "i386" && exists(${_toe_header})
_iw_cxgb = iw_cxgb
.endif

.if ${MACHINE_ARCH} == "amd64" && exists(${_toe_header})
_iw_cxgb = iw_cxgb
.endif

.include <bsd.subdir.mk>
