# $FreeBSD$
#
# The include file <dtb.mk> handles building and installing dtb files.
#
# +++ variables +++
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

# Search for kernel source tree in standard places.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/)
SYSDIR=	${_dir}
.endif
.endfor
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern/)
.error "can't find kernel source tree"
.endif

.SUFFIXES: .dtb .dts

.PATH: ${SYSDIR}/gnu/dts/${MACHINE} ${SYSDIR}/boot/fdt/dts/${MACHINE}

DTB=${DTS:R:S/$/.dtb/}

all: ${DTB}

.if defined(DTS)
.for _dts in ${DTS}
${_dts:R:S/$/.dtb/}:	${_dts}
	@echo Generating ${.TARGET} from ${_dts}
	@${SYSDIR}/tools/fdt/make_dtb.sh ${SYSDIR} ${_dts} ${.OBJDIR}
CLEANFILES+=${_dts:R:S/$/.dtb/}
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
	${INSTALL} -o ${DTBOWN} -g ${DTBGRP} -m ${DTBMODE} \
	    ${_INSTALLFLAGS} ${_dtb} ${DESTDIR}${DTBDIR}/
.endfor
.endif # !target(realinstall)
.endif # !target(install)

.include <bsd.dep.mk>
.include <bsd.obj.mk>
