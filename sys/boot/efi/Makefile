# $FreeBSD$

.include <src.opts.mk>

SUBDIR=		libefi

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "arm"
.if ${MK_FDT} != "no"
SUBDIR+=	fdt
.endif
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || \
    ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "arm"
SUBDIR+=	loader boot1
.endif

.include <bsd.subdir.mk>
