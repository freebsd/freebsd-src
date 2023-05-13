
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
BOOT_MACHINE_DIR.amd64 = boot/i386

.-include <site.sys.dirdeps.env.mk>

ALL_MACHINE_LIST?= ${TARGET_MACHINE_LIST}

.for m in ${ALL_MACHINE_LIST:O:u}
BOOT_MACHINE_DIR.$m ?= boot/$m
.endfor

HOST_OBJTOP ?= ${OBJROOT}${HOST_TARGET}

.if ${REQUESTED_MACHINE:U${MACHINE}} == "host"
MACHINE= host
.if ${TARGET_MACHINE:Uno} == ${HOST_TARGET}
# not what we want
TARGET_MACHINE= host
.endif
.endif
.if ${MACHINE} == "host"
OBJTOP := ${HOST_OBJTOP}
MACHINE_ARCH= ${MACHINE_ARCH_${MACHINE}}
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
