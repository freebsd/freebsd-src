
.-include <local.sysdir.mk>

# Search for kernel source tree in standard places.
.if !defined(SYSDIR)
.for _dir in ${SRCTOP:D${SRCTOP}/sys} \
    ${.CURDIR}/../.. ${.CURDIR}/../../.. ${.CURDIR}/../../../.. \
    ${.CURDIR}/../../../../.. /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/) && exists(${_dir}/conf/kmod.mk)
SYSDIR=	${_dir:tA}
.endif
.endfor
.endif
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern/) || \
    !exists(${SYSDIR}/conf/kmod.mk)
.error Unable to locate the kernel source tree. Set SYSDIR to override.
.endif
