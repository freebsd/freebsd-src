#
# $FreeBSD$
#

.include "${.CURDIR}/../Makefile.inc0"

GASES=		${BINUTIL_ARCH}

# Only build the cross tools when not cross-compiling.
__MA!=/sbin/sysctl -b hw.machine_arch
__M!=/sbin/sysctl -b hw.machine
.if ${__MA} == ${MACHINE_ARCH} || ${__M} == ${MACHINE}
.if defined(CROSS_ARCH)
.for _g in ${CROSS_ARCH}
.if (${GASES:R:M${_g:R}} == "")
GASES+=$(_g)
.endif
.endfor
.endif
.endif

.for _g in ${GASES}
.if exists(${.CURDIR}/as_${_g})
SUBDIR+=	as_${_g}
.endif
.if defined(CROSS_FORMATS)
.for _f in ${CROSS_FORMATS}
.if exists(${.CURDIR}/${_g}-${_f})
SUBDIR+=	${_g}-${_f}
.endif
.endfor
.endif
.endfor

.include <bsd.subdir.mk>
