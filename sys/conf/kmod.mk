#	From: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
# $FreeBSD$
#
# The include file <bsd.kmod.mk> handles building and installing loadable
# kernel modules.
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# EXPORT_SYMS	A list of symbols that should be exported from the module,
#		or the name of a file containing a list of symbols, or YES
#		to export all symbols.  If not defined, no symbols are
#		exported.
#
# KMOD		The name of the kernel module to build.
#
# KMODDIR	Base path for kernel modules (see kld(4)). [/boot/kernel]
#
# KMODOWN	Module file owner. [${BINOWN}]
#
# KMODGRP	Module file group. [${BINGRP}]
#
# KMODMODE	Module file mode. [${BINMODE}]
#
# KMODLOAD	Command to load a kernel module [/sbin/kldload]
#
# KMODUNLOAD	Command to unload a kernel module [/sbin/kldunload]
#
# MFILES	Optionally a list of interfaces used by the module.
#		This file contains a default list of interfaces.
#
# PROG		The name of the kernel module to build.
#		If not supplied, ${KMOD}.ko is used.
#
# SRCS		List of source files.
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
# 	load:
#		Load a module.
#
# 	unload:
#		Unload a module.
#

AWK?=		awk
KMODLOAD?=	/sbin/kldload
KMODUNLOAD?=	/sbin/kldunload
OBJCOPY?=	objcopy

.if defined(KMODDEPS)
.error "Do not use KMODDEPS on 5.0+; use MODULE_VERSION/MODULE_DEPEND"
.endif

.include <bsd.init.mk>

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

.if ${CC} == "icc"
CFLAGS:=	${CFLAGS:C/(-x[^M^K^W]+)[MKW]+|-x[MKW]+/\1/}
.else
. if !empty(CFLAGS:M-O[23s]) && empty(CFLAGS:M-fno-strict-aliasing)
CFLAGS+=	-fno-strict-aliasing
. endif
WERROR?=	-Werror
.endif
CFLAGS+=	${WERROR}
CFLAGS+=	-D_KERNEL
CFLAGS+=	-DKLD_MODULE

# Don't use any standard or source-relative include directories.
# Since -nostdinc will annull any previous -I paths, we repeat all
# such paths after -nostdinc.  It doesn't seem to be possible to
# add to the front of `make' variable.
_ICFLAGS:=	${CFLAGS:M-I*}
.if ${CC} == "icc"
NOSTDINC=	-X
.else
NOSTDINC=	-nostdinc
.endif
CFLAGS+=	${NOSTDINC} -I- ${INCLMAGIC} ${_ICFLAGS}
.if defined(KERNBUILDDIR)
CFLAGS+=	-DHAVE_KERNEL_OPTION_HEADERS -include ${KERNBUILDDIR}/opt_global.h
.endif

# Add -I paths for system headers.  Individual module makefiles don't
# need any -I paths for this.  Similar defaults for .PATH can't be
# set because there are no standard paths for non-headers.
CFLAGS+=	-I. -I@

# Add -I path for altq headers as they are included via net/if_var.h
# for example.
CFLAGS+=	-I@/contrib/altq

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

.if ${CC} != "icc"
CFLAGS+=	-finline-limit=${INLINE_LIMIT}
.endif

# Disallow common variables, and if we end up with commons from
# somewhere unexpected, allocate storage for them in the module itself.
.if ${CC} != "icc"
CFLAGS+=	-fno-common
.endif
LDFLAGS+=	-d -warn-common

CFLAGS+=	${DEBUG_FLAGS}
.if ${MACHINE_ARCH} == amd64
CFLAGS+=	-fno-omit-frame-pointer
.endif

.if ${MACHINE_ARCH} == "powerpc"
CFLAGS+=	-mlongcall -fno-omit-frame-pointer
.endif

OBJS+=	${SRCS:N*.h:R:S/$/.o/g}

.if !defined(PROG)
PROG=	${KMOD}.ko
.endif

.if !defined(DEBUG_FLAGS)
FULLPROG=	${PROG}
.else
FULLPROG=	${PROG}.debug
${PROG}: ${FULLPROG}
	${OBJCOPY} --strip-debug ${FULLPROG} ${PROG}
.endif

.if ${MACHINE_ARCH} != amd64
${FULLPROG}: ${KMOD}.kld
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} ${KMOD}.kld
.if !defined(DEBUG_FLAGS)
	${OBJCOPY} --strip-debug ${.TARGET}
.endif
.endif

EXPORT_SYMS?=	NO
.if ${EXPORT_SYMS} != YES
CLEANFILES+=	export_syms
.endif

.if ${MACHINE_ARCH} != amd64
${KMOD}.kld: ${OBJS}
.else
${FULLPROG}: ${OBJS}
.endif
	${LD} ${LDFLAGS} -r -d -o ${.TARGET} ${OBJS}
.if defined(EXPORT_SYMS)
.if ${EXPORT_SYMS} != YES
.if ${EXPORT_SYMS} == NO
	touch export_syms
.elif !exists(${.CURDIR}/${EXPORT_SYMS})
	echo ${EXPORT_SYMS} > export_syms
.else
	grep -v '^#' < ${EXPORT_SYMS} > export_syms
.endif
	awk -f ${SYSDIR}/conf/kmod_syms.awk ${.TARGET} \
	    export_syms | xargs -J% ${OBJCOPY} % ${.TARGET}
.endif
.endif
.if !defined(DEBUG_FLAGS) && ${MACHINE_ARCH} == amd64
	${OBJCOPY} --strip-debug ${.TARGET}
.endif

_ILINKS=@ machine
.if ${MACHINE} != ${MACHINE_ARCH}
_ILINKS+=${MACHINE_ARCH}
.endif

all: objwarn ${PROG}

beforedepend: ${_ILINKS}

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
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern/)
.error "can't find kernel source tree"
.endif

${_ILINKS}:
	@case ${.TARGET} in \
	${MACHINE_ARCH}) \
		path=${SYSDIR}/${MACHINE_ARCH}/include ;; \
	machine) \
		path=${SYSDIR}/${MACHINE}/include ;; \
	@) \
		path=${SYSDIR} ;; \
	esac ; \
	path=`(cd $$path && /bin/pwd)` ; \
	${ECHO} ${.TARGET} "->" $$path ; \
	ln -s $$path ${.TARGET}

CLEANFILES+= ${PROG} ${KMOD}.kld ${OBJS} ${_ILINKS}

.if defined(DEBUG_FLAGS)
CLEANFILES+= ${FULLPROG}
.endif

.if !target(install)

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

.if !target(install.debug) && defined(DEBUG_FLAGS)
install.debug:
	cd ${.CURDIR}; ${MAKE} -DINSTALL_DEBUG install
.endif

.if !target(realinstall)
realinstall: _kmodinstall
.ORDER: beforeinstall _kmodinstall
_kmodinstall:
.if defined(DEBUG_FLAGS) && defined(INSTALL_DEBUG)
	${INSTALL} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${FULLPROG} ${DESTDIR}${KMODDIR}
.else
	${INSTALL} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}

.include <bsd.links.mk>

.if !defined(NO_XREF)
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
.endif # !target(realinstall)

.endif # !target(install)

.if !target(load)
load: ${PROG}
	${KMODLOAD} -v ${.OBJDIR}/${PROG}
.endif

.if !target(unload)
unload:
	${KMODUNLOAD} -v ${PROG}
.endif

.if defined(KERNBUILDDIR)
.PATH: ${KERNBUILDDIR}
CFLAGS+=	-I${KERNBUILDDIR}
.for _src in ${SRCS:Mopt_*.h}
CLEANFILES+=	${_src}
.if !target(${_src})
${_src}:
	ln -s ${KERNBUILDDIR}/${_src} ${.TARGET}
.endif
.endfor
.else
.for _src in ${SRCS:Mopt_*.h}
CLEANFILES+=	${_src}
.if !target(${_src})
${_src}:
	touch ${.TARGET}
.endif
.endfor
.endif

MFILES?= dev/acpica/acpi_if.m dev/ata/ata_if.m dev/eisa/eisa_if.m \
	dev/iicbus/iicbb_if.m dev/iicbus/iicbus_if.m \
	dev/mii/miibus_if.m dev/ofw/ofw_bus_if.m \
	dev/pccard/card_if.m dev/pccard/power_if.m dev/pci/pci_if.m \
	dev/pci/pcib_if.m dev/ppbus/ppbus_if.m dev/smbus/smbus_if.m \
	dev/sound/pcm/ac97_if.m dev/sound/pcm/channel_if.m \
	dev/sound/pcm/feeder_if.m dev/sound/pcm/mixer_if.m dev/uart/uart_if.m \
	dev/usb/usb_if.m isa/isa_if.m \
	kern/bus_if.m kern/cpufreq_if.m kern/device_if.m \
	libkern/iconv_converter_if.m opencrypto/crypto_if.m \
	pc98/pc98/canbus_if.m pci/agp_if.m

.for _srcsrc in ${MFILES}
.for _ext in c h
.for _src in ${SRCS:M${_srcsrc:T:R}.${_ext}}
CLEANFILES+=	${_src}
.if !target(${_src})
.if !exists(@)
${_src}: @
.else
${_src}: @/tools/makeobjops.awk @/${_srcsrc}
.endif
	${AWK} -f @/tools/makeobjops.awk @/${_srcsrc} -${_ext}
.endif
.endfor # _src
.endfor # _ext
.endfor # _srcsrc

.if ${SRCS:Mvnode_if.c} != ""
CLEANFILES+=	vnode_if.c
.if !exists(@)
vnode_if.c: @
.else
vnode_if.c: @/tools/vnode_if.awk @/kern/vnode_if.src
.endif
	${AWK} -f @/tools/vnode_if.awk @/kern/vnode_if.src -c
.endif

.if ${SRCS:Mvnode_if.h} != ""
CLEANFILES+=	vnode_if.h vnode_if_newproto.h vnode_if_typedef.h
.if !exists(@)
vnode_if.h vnode_if_newproto.h vnode_if_typedef.h: @
.else
vnode_if.h vnode_if_newproto.h vnode_if_typedef.h: @/tools/vnode_if.awk \
    @/kern/vnode_if.src
.endif
vnode_if.h: vnode_if_newproto.h vnode_if_typedef.h
	${AWK} -f @/tools/vnode_if.awk @/kern/vnode_if.src -h
vnode_if_newproto.h:
	${AWK} -f @/tools/vnode_if.awk @/kern/vnode_if.src -p
vnode_if_typedef.h:
	${AWK} -f @/tools/vnode_if.awk @/kern/vnode_if.src -q
.endif

.for _i in mii pccard
.if ${SRCS:M${_i}devs.h} != ""
CLEANFILES+=	${_i}devs.h
.if !exists(@)
${_i}devs.h: @
.else
${_i}devs.h: @/tools/${_i}devs2h.awk @/dev/${_i}/${_i}devs
.endif
	${AWK} -f @/tools/${_i}devs2h.awk @/dev/${_i}/${_i}devs
.endif
.endfor # _i

.if ${SRCS:Musbdevs.h} != ""
CLEANFILES+=	usbdevs.h
.if !exists(@)
usbdevs.h: @
.else
usbdevs.h: @/tools/usbdevs2h.awk @/dev/usb/usbdevs
.endif
	${AWK} -f @/tools/usbdevs2h.awk @/dev/usb/usbdevs -h
.endif

.if ${SRCS:Musbdevs_data.h} != ""
CLEANFILES+=	usbdevs_data.h
.if !exists(@)
usbdevs_data.h: @
.else
usbdevs_data.h: @/tools/usbdevs2h.awk @/dev/usb/usbdevs
.endif
	${AWK} -f @/tools/usbdevs2h.awk @/dev/usb/usbdevs -d
.endif

.if ${SRCS:Macpi_quirks.h} != ""
CLEANFILES+=	acpi_quirks.h
.if !exists(@)
acpi_quirks.h: @
.else
acpi_quirks.h: @/tools/acpi_quirks2h.awk @/dev/acpica/acpi_quirks
.endif
	${AWK} -f @/tools/acpi_quirks2h.awk @/dev/acpica/acpi_quirks
.endif

.if ${SRCS:Massym.s} != ""
CLEANFILES+=	assym.s genassym.o
assym.s: genassym.o
.if !exists(@)
assym.s: @
.else
assym.s: @/kern/genassym.sh
.endif
	sh @/kern/genassym.sh genassym.o > ${.TARGET}
genassym.o: @/${MACHINE_ARCH}/${MACHINE_ARCH}/genassym.c @ machine
	${CC} -c ${CFLAGS:N-fno-common} \
	    @/${MACHINE_ARCH}/${MACHINE_ARCH}/genassym.c
.endif

lint: ${SRCS}
	${LINT} ${LINTKERNFLAGS} ${CFLAGS:M-[DILU]*} ${.ALLSRC:M*.c}

.include <bsd.dep.mk>

.if !exists(${.OBJDIR}/${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>
.include "kern.mk"
