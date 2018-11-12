# $FreeBSD$
#
# The include file <dtb.mk> handles building and installing dtb files.
#
# +++ variables +++
#
# DTC		The Device Tree Compiler to use
#
# DTS		List of the dts files to build and install.
#
# DTBDIR	Base path for dtb modules [/boot/dtb]
#
# DTBOWN	.dtb file owner. [${BINOWN}]
#
# DTBGRP	.dtb file group. [${BINGRP}]
#
# DTBMODE	Module file mode. [${BINMODE}]
#
# DESTDIR	The tree where the module gets installed. [not set]
#
# +++ targets +++
#
# 	install:
#               install the kernel module; if the Makefile
#               does not itself define the target install, the targets
#               beforeinstall and afterinstall may also be used to cause
#               actions immediately before and after the install target
#		is executed.
#

.include <bsd.init.mk>
# Grab all the options for a kernel build. For backwards compat, we need to
# do this after bsd.own.mk.
.include "kern.opts.mk"

DTC?=		dtc

# Search for kernel source tree in standard places.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/)
SYSDIR=	${_dir:tA}
.endif
.endfor
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern/)
.error "can't find kernel source tree"
.endif

.SUFFIXES: .dtb .dts .dtbo .dtso

.PATH: ${SYSDIR}/gnu/dts/${MACHINE} ${SYSDIR}/dts/${MACHINE} ${SYSDIR}/dts/${MACHINE}/overlays

DTB=${DTS:R:S/$/.dtb/}
DTBO=${DTSO:R:S/$/.dtbo/}

all: ${DTB} ${DTBO}

.if defined(DTS)
.export DTC
.for _dts in ${DTS}
${_dts:R:S/$/.dtb/}:	${_dts} ${OP_META}
	@${ECHO} Generating ${.TARGET} from ${_dts}
	@env ECHO=${ECHO} ${SYSDIR}/tools/fdt/make_dtb.sh ${SYSDIR} ${_dts} ${.OBJDIR}
CLEANFILES+=${_dts:R:S/$/.dtb/}
.endfor
.endif

.if defined(DTSO)
.export DTC
.for _dtso in ${DTSO}
${_dtso:R:S/$/.dtbo/}:	${_dtso} ${OP_META}
	@${ECHO} Generating ${.TARGET} from ${_dtso}
	@env ECHO=${ECHO} ${SYSDIR}/tools/fdt/make_dtbo.sh ${SYSDIR} overlays/${_dtso} ${.OBJDIR}
CLEANFILES+=${_dtso:R:S/$/.dtbo/}
.endfor
.endif

.if !target(install)
.if !target(realinstall)
realinstall: _dtbinstall
.ORDER: beforeinstall _kmodinstall
_dtbinstall:
# Need to create this because installkernel doesn't invoke mtree with BSD.root.mtree
# to make sure the tree is setup properly. We don't recreate it to avoid duplicate
# entries in the NO_ROOT case.
	test -d ${DESTDIR}${DTBDIR} || ${INSTALL} -d -o ${DTBOWN} -g ${DTBGRP} ${DESTDIR}${DTBDIR}
.for _dtb in ${DTB}
.if ${MACHINE_CPUARCH} == "aarch64"
	test -d ${DESTDIR}${DTBDIR}/${_dtb:H} || ${INSTALL} -d -o ${DTBOWN} -g ${DTBGRP} ${DESTDIR}${DTBDIR}/${_dtb:H}
	${INSTALL} -o ${DTBOWN} -g ${DTBGRP} -m ${DTBMODE} \
	    ${_INSTALLFLAGS} ${_dtb:T} ${DESTDIR}${DTBDIR}/${_dtb:H}
.else
	${INSTALL} -o ${DTBOWN} -g ${DTBGRP} -m ${DTBMODE} \
	    ${_INSTALLFLAGS} ${_dtb} ${DESTDIR}${DTBDIR}/
.endif
.endfor
	test -d ${DESTDIR}${DTBODIR} || ${INSTALL} -d -o ${DTBOWN} -g ${DTBGRP} ${DESTDIR}${DTBODIR}
.for _dtbo in ${DTBO}
	${INSTALL} -o ${DTBOWN} -g ${DTBGRP} -m ${DTBMODE} \
	    ${_INSTALLFLAGS} ${_dtbo} ${DESTDIR}${DTBODIR}/
.endfor
.endif # !target(realinstall)
.endif # !target(install)

.include <bsd.dep.mk>
.include <bsd.obj.mk>
.include <bsd.links.mk>
