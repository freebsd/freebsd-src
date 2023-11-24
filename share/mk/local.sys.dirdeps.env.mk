
# For universe we want to potentially
# build for multiple MACHINE_ARCH per MACHINE
# so we need more than MACHINE in TARGET_SPEC
TARGET_SPEC_VARS?= MACHINE MACHINE_ARCH
 
# this is sufficient for most of the tree.
.MAKE.DEPENDFILE_DEFAULT = ${.MAKE.DEPENDFILE_PREFIX}

# but if we have a machine qualified file it should be used in preference
.MAKE.DEPENDFILE_PREFERENCE = \
	${.MAKE.DEPENDFILE_PREFIX}.${MACHINE} \
	${.MAKE.DEPENDFILE_PREFIX}

# some corner cases
BOOT_MACHINE_DIR.amd64 = stand/i386

.-include <site.sys.dirdeps.env.mk>

ALL_MACHINE_LIST?= ${TARGET_MACHINE_LIST}

.for m in ${ALL_MACHINE_LIST:O:u}
BOOT_MACHINE_DIR.$m ?= stand/$m
.endfor

HOST_OBJTOP ?= ${OBJROOT}${HOST_TARGET}
HOST_OBJTOP32 ?= ${OBJROOT}${HOST_TARGET32}

.if ${.MAKE.LEVEL} == 0
.if ${REQUESTED_MACHINE:U${MACHINE}} == "host"
MACHINE= host
.if ${TARGET_MACHINE:Uno} == ${HOST_TARGET}
# not what we want
TARGET_MACHINE= host
.endif
.elif ${REQUESTED_MACHINE:U${MACHINE}} == "host32"
MACHINE= host32
.endif
.endif

.if ${MACHINE:Nhost*} == ""
MACHINE_ARCH= ${MACHINE_ARCH_${MACHINE}}
.if ${MACHINE} == "host32"
.MAKE.DEPENDFILE_PREFERENCE= \
	${.CURDIR}/${.MAKE.DEPENDFILE_PREFIX}.host32 \
	${.CURDIR}/${.MAKE.DEPENDFILE_PREFIX}.host \
	${.CURDIR}/${.MAKE.DEPENDFILE_PREFIX}
.endif
.endif


.if ${.MAKE.LEVEL} == 0 || empty(PYTHON)
PYTHON ?= /usr/local/bin/python
.export PYTHON

# _SKIP_BUILD is not 100% as it requires wrapping all 'all:' targets to avoid
# building in MAKELEVEL0.  Just prohibit 'all' entirely in this case to avoid
# problems.
.if make(all)
.error DIRDEPS_BUILD: Please run '${MAKE}' instead of '${MAKE} all'.
.endif
.endif

.if ${.MAKE.OS} != "FreeBSD" || ${_HOST_OSREL:R} < ${OS_REVISION:R}
# a pseudo option to indicate we need libegacy for host
MK_host_egacy= yes
.endif
