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
# KMODISLOADED	Command to check whether a kernel module is
#		loaded [/sbin/kldstat -q -n]
#
# PROG		The name of the kernel module to build.
#		If not supplied, ${KMOD}.ko is used.
#
# SRCS		List of source files.
#
# FIRMWS	List of firmware images in format filename:shortname:version
#
# FIRMWARE_LICENSE
#		Set to the name of the license the user has to agree on in
#		order to use this firmware. See /usr/share/doc/legal
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
#	reload:
#		Unload if loaded, then load.
#

# backwards compat option for older systems.
MACHINE_CPUARCH?=${MACHINE_ARCH:C/mips(n32|64)?(el)?/mips/:C/arm(v6)?(eb)?/arm/:C/powerpc64/powerpc/}

AWK?=		awk
KMODLOAD?=	/sbin/kldload
KMODUNLOAD?=	/sbin/kldunload
KMODISLOADED?=	/sbin/kldstat -q -n
OBJCOPY?=	objcopy

.if defined(KMODDEPS)
.error "Do not use KMODDEPS on 5.0+; use MODULE_VERSION/MODULE_DEPEND"
.endif

.include <bsd.init.mk>
.include <bsd.compiler.mk>

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

# amd64 and mips use direct linking for kmod, all others use shared binaries
.if ${MACHINE_CPUARCH} != amd64 && ${MACHINE_CPUARCH} != mips
__KLD_SHARED=yes
.else
__KLD_SHARED=no
.endif

.if !empty(CFLAGS:M-O[23s]) && empty(CFLAGS:M-fno-strict-aliasing)
CFLAGS+=	-fno-strict-aliasing
.endif
WERROR?=	-Werror
CFLAGS+=	${WERROR}
CFLAGS+=	-D_KERNEL
CFLAGS+=	-DKLD_MODULE

# Don't use any standard or source-relative include directories.
CSTD=		c99
NOSTDINC=	-nostdinc
CFLAGS:=	${CFLAGS:N-I*} ${NOSTDINC} ${INCLMAGIC} ${CFLAGS:M-I*}
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

.if ${COMPILER_TYPE} != "clang"
CFLAGS+=	-finline-limit=${INLINE_LIMIT}
CFLAGS+= --param inline-unit-growth=100
CFLAGS+= --param large-function-growth=1000
.endif

# Disallow common variables, and if we end up with commons from
# somewhere unexpected, allocate storage for them in the module itself.
CFLAGS+=	-fno-common
LDFLAGS+=	-d -warn-common

CFLAGS+=	${DEBUG_FLAGS}
.if ${MACHINE_CPUARCH} == amd64
CFLAGS+=	-fno-omit-frame-pointer -mno-omit-leaf-frame-pointer
.endif

# Temporary workaround for PR 196407, which contains the fascinating details.
# Don't allow clang to use fpu instructions or registers in kernel modules.
.if ${MACHINE_CPUARCH} == arm
CFLAGS.clang+=	-mllvm -arm-use-movt=0
CFLAGS.clang+=	-mfpu=none
.endif

.if ${MACHINE_CPUARCH} == powerpc
CFLAGS+=	-mlongcall -fno-omit-frame-pointer
.endif

.if ${MACHINE_CPUARCH} == mips
CFLAGS+=	-G0 -fno-pic -mno-abicalls -mlong-calls
.endif

.if defined(DEBUG) || defined(DEBUG_FLAGS)
CTFFLAGS+=	-g
.endif

.if defined(FIRMWS)
.if !exists(@)
${KMOD:S/$/.c/}: @
.else
${KMOD:S/$/.c/}: @/tools/fw_stub.awk
.endif
	${AWK} -f @/tools/fw_stub.awk ${FIRMWS} -m${KMOD} -c${KMOD:S/$/.c/g} \
	    ${FIRMWARE_LICENSE:C/.+/-l/}${FIRMWARE_LICENSE}

SRCS+=	${KMOD:S/$/.c/}
CLEANFILES+=	${KMOD:S/$/.c/}

.for _firmw in ${FIRMWS}
${_firmw:C/\:.*$/.fwo/:T}:	${_firmw:C/\:.*$//}
	@${ECHO} ${_firmw:C/\:.*$//} ${.ALLSRC:M*${_firmw:C/\:.*$//}}
	@if [ -e ${_firmw:C/\:.*$//} ]; then			\
		${LD} -b binary --no-warn-mismatch ${LDFLAGS}	\
		    -r -d -o ${.TARGET}	${_firmw:C/\:.*$//};	\
	else							\
		ln -s ${.ALLSRC:M*${_firmw:C/\:.*$//}} ${_firmw:C/\:.*$//}; \
		${LD} -b binary --no-warn-mismatch ${LDFLAGS}	\
		    -r -d -o ${.TARGET}	${_firmw:C/\:.*$//};	\
		rm ${_firmw:C/\:.*$//};				\
	fi

OBJS+=	${_firmw:C/\:.*$/.fwo/:T}
.endfor
.endif

OBJS+=	${SRCS:N*.h:R:S/$/.o/g}

.if !defined(PROG)
PROG=	${KMOD}.ko
.endif

.if !defined(DEBUG_FLAGS)
FULLPROG=	${PROG}
.else
FULLPROG=	${PROG}.debug
${PROG}: ${FULLPROG} ${PROG}.symbols
	${OBJCOPY} --strip-debug --add-gnu-debuglink=${PROG}.symbols\
	    ${FULLPROG} ${.TARGET}
${PROG}.symbols: ${FULLPROG}
	${OBJCOPY} --only-keep-debug ${FULLPROG} ${.TARGET}
.endif

.if ${__KLD_SHARED} == yes
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

.if ${__KLD_SHARED} == yes
${KMOD}.kld: ${OBJS}
.else
${FULLPROG}: ${OBJS}
.endif
	${LD} ${LDFLAGS} -r -d -o ${.TARGET} ${OBJS}
.if defined(MK_CTF) && ${MK_CTF} != "no"
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS}
.endif
.if defined(EXPORT_SYMS)
.if ${EXPORT_SYMS} != YES
.if ${EXPORT_SYMS} == NO
	:> export_syms
.elif !exists(${.CURDIR}/${EXPORT_SYMS})
	echo ${EXPORT_SYMS} > export_syms
.else
	grep -v '^#' < ${EXPORT_SYMS} > export_syms
.endif
	awk -f ${SYSDIR}/conf/kmod_syms.awk ${.TARGET} \
	    export_syms | xargs -J% ${OBJCOPY} % ${.TARGET}
.endif
.endif
.if !defined(DEBUG_FLAGS) && ${__KLD_SHARED} == no
	${OBJCOPY} --strip-debug ${.TARGET}
.endif

_ILINKS=@ machine
.if ${MACHINE} != ${MACHINE_CPUARCH}
_ILINKS+=${MACHINE_CPUARCH}
.endif
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_ILINKS+=x86
.endif

all: objwarn ${PROG}

beforedepend: ${_ILINKS}

# Ensure that the links exist without depending on it when it exists which
# causes all the modules to be rebuilt when the directory pointed to changes.
.for _link in ${_ILINKS}
.if !exists(${.OBJDIR}/${_link})
${OBJS}: ${.OBJDIR}/${_link}
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

.for _link in ${_ILINKS}
.PHONY: ${_link}
${_link}: ${.OBJDIR}/${_link}

${.OBJDIR}/${_link}:
	@case ${.TARGET:T} in \
	machine) \
		path=${SYSDIR}/${MACHINE}/include ;; \
	@) \
		path=${SYSDIR} ;; \
	*) \
		path=${SYSDIR}/${.TARGET:T}/include ;; \
	esac ; \
	path=`(cd $$path && /bin/pwd)` ; \
	${ECHO} ${.TARGET:T} "->" $$path ; \
	ln -sf $$path ${.TARGET:T}
.endfor

CLEANFILES+= ${PROG} ${KMOD}.kld ${OBJS}

.if defined(DEBUG_FLAGS)
CLEANFILES+= ${FULLPROG} ${PROG}.symbols
.endif

.if !target(install)

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

.if !target(realinstall)
realinstall: _kmodinstall
.ORDER: beforeinstall _kmodinstall
_kmodinstall:
	${INSTALL} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}
.if defined(DEBUG_FLAGS) && !defined(INSTALL_NODEBUG) && ${MK_KERNEL_SYMBOLS} != "no"
	${INSTALL} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG}.symbols ${DESTDIR}${KMODDIR}
.endif

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
.endif # !target(realinstall)

.endif # !target(install)

.if !target(load)
load: ${PROG}
	${KMODLOAD} -v ${.OBJDIR}/${PROG}
.endif

.if !target(unload)
unload:
	if ${KMODISLOADED} ${PROG} ; then ${KMODUNLOAD} -v ${PROG} ; fi
.endif

.if !target(reload)
reload: unload load
.endif

.if defined(KERNBUILDDIR)
.PATH: ${KERNBUILDDIR}
CFLAGS+=	-I${KERNBUILDDIR}
.for _src in ${SRCS:Mopt_*.h}
CLEANFILES+=	${_src}
.if !target(${_src})
${_src}:
	ln -sf ${KERNBUILDDIR}/${_src} ${.TARGET}
.endif
.endfor
.else
.for _src in ${SRCS:Mopt_*.h}
CLEANFILES+=	${_src}
.if !target(${_src})
${_src}:
	:> ${.TARGET}
.endif
.endfor
.endif

# Respect configuration-specific C flags.
CFLAGS+=	${CONF_CFLAGS}

MFILES?= dev/acpica/acpi_if.m dev/acpi_support/acpi_wmi_if.m \
	dev/agp/agp_if.m dev/ata/ata_if.m dev/eisa/eisa_if.m \
	dev/fb/fb_if.m dev/gpio/gpio_if.m dev/gpio/gpiobus_if.m \
	dev/iicbus/iicbb_if.m dev/iicbus/iicbus_if.m \
	dev/mmc/mmcbr_if.m dev/mmc/mmcbus_if.m \
	dev/mii/miibus_if.m dev/mvs/mvs_if.m dev/ofw/ofw_bus_if.m \
	dev/pccard/card_if.m dev/pccard/power_if.m dev/pci/pci_if.m \
	dev/pci/pcib_if.m dev/ppbus/ppbus_if.m \
	dev/sdhci/sdhci_if.m dev/smbus/smbus_if.m dev/spibus/spibus_if.m \
	dev/sound/pci/hda/hdac_if.m \
	dev/sound/pcm/ac97_if.m dev/sound/pcm/channel_if.m \
	dev/sound/pcm/feeder_if.m dev/sound/pcm/mixer_if.m \
	dev/sound/midi/mpu_if.m dev/sound/midi/mpufoi_if.m \
	dev/sound/midi/synth_if.m dev/usb/usb_if.m isa/isa_if.m \
	kern/bus_if.m kern/clock_if.m \
	kern/cpufreq_if.m kern/device_if.m kern/serdev_if.m \
	libkern/iconv_converter_if.m opencrypto/cryptodev_if.m \
	pc98/pc98/canbus_if.m dev/etherswitch/mdio_if.m

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

.if !empty(SRCS:Mvnode_if.c)
CLEANFILES+=	vnode_if.c
.if !exists(@)
vnode_if.c: @
.else
vnode_if.c: @/tools/vnode_if.awk @/kern/vnode_if.src
.endif
	${AWK} -f @/tools/vnode_if.awk @/kern/vnode_if.src -c
.endif

.if !empty(SRCS:Mvnode_if.h)
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
.if !empty(SRCS:M${_i}devs.h)
CLEANFILES+=	${_i}devs.h
.if !exists(@)
${_i}devs.h: @
.else
${_i}devs.h: @/tools/${_i}devs2h.awk @/dev/${_i}/${_i}devs
.endif
	${AWK} -f @/tools/${_i}devs2h.awk @/dev/${_i}/${_i}devs
.endif
.endfor # _i

.if !empty(SRCS:Musbdevs.h)
CLEANFILES+=	usbdevs.h
.if !exists(@)
usbdevs.h: @
.else
usbdevs.h: @/tools/usbdevs2h.awk @/dev/usb/usbdevs
.endif
	${AWK} -f @/tools/usbdevs2h.awk @/dev/usb/usbdevs -h
.endif

.if !empty(SRCS:Musbdevs_data.h)
CLEANFILES+=	usbdevs_data.h
.if !exists(@)
usbdevs_data.h: @
.else
usbdevs_data.h: @/tools/usbdevs2h.awk @/dev/usb/usbdevs
.endif
	${AWK} -f @/tools/usbdevs2h.awk @/dev/usb/usbdevs -d
.endif

.if !empty(SRCS:Macpi_quirks.h)
CLEANFILES+=	acpi_quirks.h
.if !exists(@)
acpi_quirks.h: @
.else
acpi_quirks.h: @/tools/acpi_quirks2h.awk @/dev/acpica/acpi_quirks
.endif
	${AWK} -f @/tools/acpi_quirks2h.awk @/dev/acpica/acpi_quirks
.endif

.if !empty(SRCS:Massym.s)
CLEANFILES+=	assym.s genassym.o
assym.s: genassym.o
.if defined(KERNBUILDDIR)
genassym.o: opt_global.h
.endif
.if !exists(@)
assym.s: @
.else
assym.s: @/kern/genassym.sh
.endif
	sh @/kern/genassym.sh genassym.o > ${.TARGET}
.if exists(@)
genassym.o: @/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}/genassym.c
.endif
genassym.o: @ machine ${SRCS:Mopt_*.h}
	${CC} -c ${CFLAGS:N-fno-common} \
	    @/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}/genassym.c
.endif

lint: ${SRCS}
	${LINT} ${LINTKERNFLAGS} ${CFLAGS:M-[DILU]*} ${.ALLSRC:M*.c}

.if defined(KERNBUILDDIR)
${OBJS}: opt_global.h
.endif

.include <bsd.dep.mk>

cleandepend: cleanilinks
# .depend needs include links so we remove them only together.
cleanilinks:
	rm -f ${_ILINKS}

.if !exists(${.OBJDIR}/${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>
.include "kern.mk"
