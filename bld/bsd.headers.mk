
.if defined(HOSTPROG) && ${MACHINE} != ${HOST_MACHINE}

# This is a host program and we're not building the host so all we want to
# do is update our dependencies which will include the host program.
.if ${__MKLVL__} != 1
all : .PHONY
.endif

.include <bsd.dirdep.mk>

.else
.if defined(NOT_MACHINE_ARCH) && !empty(NOT_MACHINE_ARCH:M${MACHINE_ARCH})
DONT_DO_IT=
.endif

.if defined(NOT_MACHINE) && !empty(NOT_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if defined(ONLY_MACHINE) && empty(ONLY_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if defined(COMMONDIR) && ${MACHINE} != common
DONT_DO_IT=
.endif

.if defined(HOSTPROG) && ${MACHINE} != host
DONT_DO_IT=
.endif

.if defined(TARGET_MACHINE) && ${MACHINE} != ${TARGET_MACHINE}
DONT_DO_IT=
.endif

.if defined(DONT_DO_IT)

.if ${__MKLVL__} != 1
all:	.PHONY
.endif

.else
.if ${__MKLVL__} != 1
all	: genfiles allincs relfiles
.ORDER	: genfiles allincs relfiles
.endif
.endif

.if make(checkheaders)
_HEADERS != ls ${.CURDIR}/*.h
.for i in ${_HEADERS:T}
.if empty(INCS:M${i})
X!= echo "${i} is not in INCS" >&2; echo
.endif
.endfor

checkheaders: .PHONY
.endif

.include <bsd.dirdep.mk>
.include <bsd.genfiles.mk>
.include <bsd.relfiles.mk>
.include <bsd.incs.mk>
.endif
