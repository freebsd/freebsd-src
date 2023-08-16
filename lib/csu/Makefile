
.include <src.opts.mk>

.if exists(${.CURDIR}/${MACHINE_ARCH:S/powerpc64le/powerpc64/})
SUBDIR+= ${MACHINE_ARCH:S/powerpc64le/powerpc64/}
.else
SUBDIR+= ${MACHINE_CPUARCH}
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.subdir.mk>
