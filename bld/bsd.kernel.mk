
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

.if defined(DONT_DO_IT)

.if ${__MKLVL__} != 1
all:	.PHONY
.endif

.else
.if ${__MKLVL__} != 1
OBJKERNCONF?=	${.OBJDIR}/../config-${KERNEL:L}

.PATH:		${OBJKERNCONF}
_CFLAGS:=	${CFLAGS}
CFLAGS=		${CFLAGS_BSD} ${_CFLAGS}
CFLAGS+=	${CFLAGS.${MACHINE}}
CFLAGS+=	-I${OBJKERNCONF}
_CURDIR=	${.OBJDIR}
S=		${.SRCTOP}/bsd/sys

.if exists(${OBJKERNCONF}/Buildfile.kernel)
.include "${OBJKERNCONF}/Buildfile.kernel"
.endif
.endif
.endif

.include <bsd.dirdep.mk>
