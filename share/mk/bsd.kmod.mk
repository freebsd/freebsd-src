# $FreeBSD: src/share/mk/bsd.kmod.mk,v 1.91.18.1.4.1 2010/02/10 00:26:20 kensmith Exp $

# Search for kernel source tree in standard places.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. ${.CURDIR}/../../../.. \
    ${.CURDIR}/../../../../.. /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/) && exists(${_dir}/conf/kmod.mk)
SYSDIR=	${_dir}
.endif
.endfor
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern/) || \
    !exists(${SYSDIR}/conf/kmod.mk)
.error "can't find kernel source tree"
.endif

.include "${SYSDIR}/conf/kmod.mk"

.include <bsd.sys.mk>
