#	From: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
# $FreeBSD$
#
# The include file <bsd.kmod.mk> handles installing Kernel Loadable Device
# drivers (KLD's).
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# KMOD          The name of the kernel module to build.
#
# KMODDIR	Base path for kernel modules (see kld(4)). [/boot/kernel]
#
# KMODOWN	KLD owner. [${BINOWN}]
#
# KMODGRP	KLD group. [${BINGRP}]
#
# KMODMODE	KLD mode. [${BINMODE}]
#
# KMODLOAD	Command to load a kernel module [/sbin/kldload]
#
# KMODUNLOAD	Command to unload a kernel module [/sbin/kldunload]
#
# PROG          The name of the kernel module to build.
#		If not supplied, ${KMOD}.o is used.
#
# SRCS          List of source files
#
# DESTDIR	Change the tree where the module gets installed. [not set]
#
# MFILES	Optionally a list of interfaces used by the module.
#		This file contains a default list of interfaces.
#
# EXPORT_SYMS	A list of symbols that should be exported from the module,
#		or the name of a file containing a list of symbols, or YES
#		to export all symbols.  If not defined, no symbols are
#		exported.
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
# 	load:
#		Load KLD.
#
# 	unload:
#		Unload KLD.
#
# bsd.obj.mk: clean, cleandir and obj
# bsd.dep.mk: cleandepend, depend and tags
#

AWK?=		awk
KMODLOAD?=	/sbin/kldload
KMODUNLOAD?=	/sbin/kldunload
OBJCOPY?=	objcopy

.if defined(KMODDEPS)
.error "Do not use KMODDEPS on 5.0+, use MODULE_VERSION/MODULE_DEPEND"
.endif

.include <bsd.init.mk>

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

CFLAGS+=	${COPTS} -D_KERNEL ${CWARNFLAGS}
CFLAGS+=	-DKLD_MODULE

# Don't use any standard or source-relative include directories.
# Since -nostdinc will annull any previous -I paths, we repeat all
# such paths after -nostdinc.  It doesn't seem to be possible to
# add to the front of `make' variable.
_ICFLAGS:=	${CFLAGS:M-I*}
CFLAGS+=	-nostdinc -I- ${INCLMAGIC} ${_ICFLAGS}

# Add -I paths for system headers.  Individual KLD makefiles don't
# need any -I paths for this.  Similar defaults for .PATH can't be
# set because there are no standard paths for non-headers.
CFLAGS+=	-I. -I@ -I@/dev

# Add a -I path to standard headers like <stddef.h>.  Use a relative
# path to src/include if possible.  If the @ symlink hasn't been built
# yet, then we can't tell if the relative path exists.  Add both the
# potential relative path and an absolute path in that case.
.if exists(@)
.if exists(@/../include)
CFLAGS+=	-I@/../include
.else
CFLAGS+=	-I${DESTDIR}/usr/include
.endif
.else # !@
CFLAGS+=	-I@/../include -I${DESTDIR}/usr/include
.endif # @

# Disallow common variables, and if we end up with commons from
# somewhere unexpected, allocate storage for them in the module itself.
CFLAGS+=	-fno-common
LDFLAGS+=	-d -warn-common

CFLAGS+=	${DEBUG_FLAGS}

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.if !defined(PROG)
PROG=	${KMOD}.ko
.endif

.if !defined(DEBUG)
FULLPROG=	${PROG}
.else
FULLPROG=	${PROG}.debug
${PROG}: ${FULLPROG}
	${OBJCOPY} --strip-debug ${FULLPROG} ${PROG}
.endif

${FULLPROG}: ${KMOD}.kld
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} ${KMOD}.kld

EXPORT_SYMS?=	NO
.if ${EXPORT_SYMS} != YES
CLEANFILES+=	${.OBJDIR}/export_syms
.endif

${KMOD}.kld: ${OBJS}
	${LD} ${LDFLAGS} -r -d -o ${.TARGET} ${OBJS}
.if defined(EXPORT_SYMS)
.if ${EXPORT_SYMS} != YES
.if ${EXPORT_SYMS} == NO
	touch ${.OBJDIR}/export_syms
.elif !exists(${.CURDIR}/${EXPORT_SYMS})
	echo ${EXPORT_SYMS} > ${.OBJDIR}/export_syms
.else
	grep -v '^#' < ${EXPORT_SYMS} >  ${.OBJDIR}/export_syms
.endif
	awk -f ${SYSDIR}/conf/kmod_syms.awk ${.TARGET} \
		${.OBJDIR}/export_syms | \
	xargs -J% ${OBJCOPY} % ${.TARGET}
.endif
.endif

_ILINKS=@ machine

all: objwarn ${PROG}

beforedepend: ${_ILINKS}
	@rm -f .depend

# Ensure that the links exist without depending on it when it exists which
# causes all the modules to be rebuilt when the directory pointed to changes.
.for _link in ${_ILINKS}
.if !exists(${.OBJDIR}/${_link})
${OBJS}: ${_link}
.endif
.endfor

# Search for kernel source tree in standard places.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/)
SYSDIR=	${_dir}
.endif
.endfor
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern)
.error "can't find kernel source tree"
.endif

${_ILINKS}:
	@case ${.TARGET} in \
	machine) \
		path=${SYSDIR}/${MACHINE_ARCH}/include ;; \
	@) \
		path=${SYSDIR} ;; \
	esac ; \
	path=`(cd $$path && /bin/pwd)` ; \
	${ECHO} ${.TARGET} "->" $$path ; \
	ln -s $$path ${.TARGET}

CLEANFILES+= ${PROG} ${KMOD}.kld ${OBJS} ${_ILINKS} symb.tmp tmp.o

.if defined(DEBUG)
CLEANFILES+= ${FULLPROG}
.endif

.if !target(install)

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

.if defined(DEBUG)
install.debug:
	cd ${.CURDIR}; ${MAKE} -DINSTALL_DEBUG install
.endif

.if !target(realinstall)
realinstall: _kmodinstall
.ORDER: beforeinstall _kmodinstall
.if defined(DEBUG) && defined(INSTALL_DEBUG)
_kmodinstall:
	${INSTALL} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${FULLPROG} ${DESTDIR}${KMODDIR}
.else
_kmodinstall:
	${INSTALL} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}

.include <bsd.links.mk>

.if !defined(NO_XREF) && ${MACHINE_ARCH} != "sparc64"
afterinstall: _kldxref
.ORDER: realinstall _kldxref
.ORDER: _installlinks _kldxref
_kldxref:
	@if type kldxref >/dev/null 2>&1; then \
		${ECHO} kldxref ${DESTDIR}${KMODDIR}; \
		kldxref ${DESTDIR}${KMODDIR}; \
	fi
.endif
.endif
.endif !target(realinstall)

.endif !target(install)

.if !target(load)
load:	${PROG}
	${KMODLOAD} -v ${.OBJDIR}/${KMOD}.ko
.endif

.if !target(unload)
unload:
	${KMODUNLOAD} -v ${KMOD}
.endif

.for _src in ${SRCS:Mopt_*.h}
CLEANFILES+=	${_src}
.if !target(${_src})
${_src}:
	touch ${.TARGET}
.endif
.endfor

MFILES?= kern/bus_if.m kern/device_if.m dev/iicbus/iicbb_if.m \
    dev/iicbus/iicbus_if.m isa/isa_if.m \
    libkern/iconv_converter_if.m \
    dev/mii/miibus_if.m \
    dev/pccard/card_if.m dev/pccard/power_if.m dev/pci/pci_if.m \
    dev/pci/pcib_if.m dev/ppbus/ppbus_if.m dev/smbus/smbus_if.m \
    dev/usb/usb_if.m dev/sound/pcm/ac97_if.m dev/sound/pcm/channel_if.m \
    dev/sound/pcm/feeder_if.m dev/sound/pcm/mixer_if.m pci/agp_if.m \
    opencrypto/crypto_if.m

.for _srcsrc in ${MFILES}
.for _ext in c h
.for _src in ${SRCS:M${_srcsrc:T:R}.${_ext}}
CLEANFILES+=	${_src}
.if !target(${_src})
.if !exists(@)
${_src}: @
.endif
.if exists(@)
${_src}: @/tools/makeobjops.awk @/${_srcsrc}
.endif
	${AWK} -f @/tools/makeobjops.awk @/${_srcsrc} -${_ext}
.endif
.endfor # _src
.endfor # _ext
.endfor # _srcsrc

.for _ext in c h
.if ${SRCS:Mvnode_if.${_ext}} != ""
CLEANFILES+=	vnode_if.${_ext}
.if !exists(@)
vnode_if.${_ext}: @
.endif
.if exists(@)
vnode_if.${_ext}: @/tools/vnode_if.awk @/kern/vnode_if.src
.endif
	${AWK} -f @/tools/vnode_if.awk @/kern/vnode_if.src -${_ext}
.endif
.endfor

.if ${SRCS:Mmiidevs.h} != ""
CLEANFILES+=	miidevs.h
.if !exists(@)
miidevs.h: @
.endif
.if exists(@)
miidevs.h: @/tools/devlist2h.awk @/dev/mii/miidevs
.endif
	${AWK} -f @/tools/devlist2h.awk @/dev/mii/miidevs
.endif

regress:

lint: ${SRCS}
	${LINT} ${LINTKERNFLAGS} ${CFLAGS:M-[DILU]*} ${.ALLSRC:M*.c}

.include <bsd.dep.mk>

.if !exists(${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>
.include <bsd.kern.mk>
