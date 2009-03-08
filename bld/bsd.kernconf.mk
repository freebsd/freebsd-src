
.if !defined(KERNEL)
.error "You must define KERNEL!"
.endif

NOT_MACHINE_ARCH+= common

.if defined(NOT_MACHINE_ARCH) && !empty(NOT_MACHINE_ARCH:M${MACHINE_ARCH})
DONT_DO_IT=
.endif

.if defined(NOT_MACHINE) && !empty(NOT_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if defined(ONLY_MACHINE) && empty(ONLY_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if defined(TARGET_MACHINE) && empty(TARGET_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if ${__MKLVL__} != 1
.if defined(DONT_DO_IT)

all:	.PHONY

.else

all: Buildfile.kernel .PHONY

Buildfile.kernel:  ${KERNEL}
	d=`dirname ${.ALLSRC}`; \
	cd $$d && ${HOSTTOOL_STAGEDIR}/buildtools/config -d ${.OBJDIR} -b ${.TARGET} ${.ALLSRC}
	echo "# ${.SRCREL}" > ${.OBJDIR}/opt_global.h.srcrel
	touch ${.OBJDIR}/${.TARGET}

.endif
.endif

.include <bsd.dirdep.mk>
