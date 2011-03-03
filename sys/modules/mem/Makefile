# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mem
.PATH: ${.CURDIR}/../../${MACHINE}/${MACHINE}
.PATH: ${.CURDIR}/../../${MACHINE_ARCH}/${MACHINE_ARCH}

KMOD=	mem
SRCS=	memdev.c mem.c memutil.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=	i686_mem.c k6_mem.c
.endif
.if ${MACHINE_ARCH} == "amd64"
SRCS+=	amd64_mem.c
.endif
SRCS+=	bus_if.h device_if.h

.if ${MACHINE} == "sun4v"
SRCS+=		opt_global.h

.if defined(KERNBUILDDIR)
MKDEP=		-include ${KERNBUILDDIR}/opt_global.h
.else
CFLAGS+=	-include opt_global.h
MKDEP=		-include opt_global.h

opt_global.h:
	echo "#define SUN4V 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
