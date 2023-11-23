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
# KERNBUILDDIR	Set to the location of the kernel build directory where
#		the opt_*.h files, .o's and kernel winds up.
#
# BLOB_OBJS	Prebuilt binary blobs .o's from the src tree to be linked into
#		the module. These are precious and not removed in make clean.
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

AWK?=		awk
KMODLOAD?=	/sbin/kldload
KMODUNLOAD?=	/sbin/kldunload
KMODISLOADED?=	/sbin/kldstat -q -n
OBJCOPY?=	objcopy

.include "kmod.opts.mk"
.include <bsd.sysdir.mk>

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S .m

# amd64 uses direct linking for kmod, all others use shared binaries
.if ${MACHINE_CPUARCH} != amd64
__KLD_SHARED=yes
.else
__KLD_SHARED=no
.endif

.if !empty(CFLAGS:M-O[23s]) && empty(CFLAGS:M-fno-strict-aliasing)
CFLAGS+=	-fno-strict-aliasing
.endif
WERROR?=	-Werror

LINUXKPI_GENSRCS+= \
	backlight_if.h \
	bus_if.h \
	device_if.h \
	iicbus_if.h \
	iicbb_if.h \
	lkpi_iic_if.c \
	lkpi_iic_if.h \
	pci_if.h \
	pci_iov_if.h \
	pcib_if.h \
	vnode_if.h \
	usb_if.h \
	opt_usb.h \
	opt_stack.h

LINUXKPI_INCLUDES+= \
	-I${SYSDIR}/compat/linuxkpi/common/include \
	-I${SYSDIR}/compat/linuxkpi/dummy/include

CFLAGS+=	${WERROR}
CFLAGS+=	-D_KERNEL
CFLAGS+=	-DKLD_MODULE
.if defined(MODULE_TIED)
CFLAGS+=	-DKLD_TIED
.endif

# Don't use any standard or source-relative include directories.
NOSTDINC=	-nostdinc
CFLAGS:=	${CFLAGS:N-I*} ${NOSTDINC} ${INCLMAGIC} ${CFLAGS:M-I*}
.if defined(KERNBUILDDIR)
CFLAGS+=	-DHAVE_KERNEL_OPTION_HEADERS -include ${KERNBUILDDIR}/opt_global.h
.else
SRCS+=		opt_global.h
CFLAGS+=	-include ${.OBJDIR}/opt_global.h
.endif

# Add -I paths for system headers.  Individual module makefiles don't
# need any -I paths for this.  Similar defaults for .PATH can't be
# set because there are no standard paths for non-headers.
CFLAGS+=	-I. -I${SYSDIR} -I${SYSDIR}/contrib/ck/include

CFLAGS.gcc+=	-finline-limit=${INLINE_LIMIT}
CFLAGS.gcc+=	-fms-extensions
CFLAGS.gcc+= --param inline-unit-growth=100
CFLAGS.gcc+= --param large-function-growth=1000

# Disallow common variables, and if we end up with commons from
# somewhere unexpected, allocate storage for them in the module itself.
#
# -fno-common is the default for src builds, but this should be left in place
# until at least we catch up to GCC10/LLVM11 or otherwise enable -fno-common
# in <bsd.sys.mk> instead.  For now, we will have duplicate -fno-common in
# CFLAGS for in-tree module builds as they will also pick it up from
# share/mk/src.sys.mk, but the following is important for out-of-tree modules
# (e.g. ports).
CFLAGS+=	-fno-common
.if ${LINKER_TYPE} != "lld" || ${LINKER_VERSION} < 140000
# lld >= 14 warns that -d is deprecated, and will be removed.
LDFLAGS+=	-d
.endif
LDFLAGS+=	-warn-common

.if defined(LINKER_FEATURES) && ${LINKER_FEATURES:Mbuild-id}
LDFLAGS+=	--build-id=sha1
.endif

CFLAGS+=	${DEBUG_FLAGS}
.if ${MACHINE_CPUARCH} == aarch64 || ${MACHINE_CPUARCH} == amd64 || \
    ${MACHINE_CPUARCH} == riscv
CFLAGS+=	-fno-omit-frame-pointer -mno-omit-leaf-frame-pointer
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "riscv" || \
    ${MACHINE_CPUARCH} == "powerpc" || ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-fPIC
.endif

.if ${MACHINE_CPUARCH} == "aarch64"
# https://bugs.freebsd.org/264094
# lld >= 14 and recent GNU ld can relax adrp+add and adrp+ldr instructions,
# which breaks VNET.
LDFLAGS+=	--no-relax
.endif

# Temporary workaround for PR 196407, which contains the fascinating details.
# Don't allow clang to use fpu instructions or registers in kernel modules.
.if ${MACHINE_CPUARCH} == arm
CFLAGS.clang+=	-mno-movt
CFLAGS.clang+=	-mfpu=none
CFLAGS+=	-funwind-tables
.endif

.if ${MACHINE_CPUARCH} == powerpc
CFLAGS+=	-mlongcall -fno-omit-frame-pointer
.if ${LINKER_TYPE} == "lld"
# TOC optimization in LLD (9.0) currently breaks kernel modules, so disable it
LDFLAGS+=	--no-toc-optimize
.endif
.endif

.if defined(DEBUG) || defined(DEBUG_FLAGS)
CTFFLAGS+=	-g
.endif

.if defined(FIRMWS)
${KMOD:S/$/.c/}: ${SYSDIR}/tools/fw_stub.awk
	${AWK} -f ${SYSDIR}/tools/fw_stub.awk ${FIRMWS} -m${KMOD} -c${KMOD:S/$/.c/g} \
	    ${FIRMWARE_LICENSE:C/.+/-l/}${FIRMWARE_LICENSE}

SRCS+=	${KMOD:S/$/.c/}
CLEANFILES+=	${KMOD:S/$/.c/}

.for _firmw in ${FIRMWS}
${_firmw:C/\:.*$/.fwo/:T}:	${_firmw:C/\:.*$//} ${SYSDIR}/kern/firmw.S
	@${ECHO} ${_firmw:C/\:.*$//} ${.ALLSRC:M*${_firmw:C/\:.*$//}}
	${CC:N${CCACHE_BIN}} -c -x assembler-with-cpp -DLOCORE 	\
	    ${CFLAGS} ${WERROR} 				\
	    -DFIRMW_FILE=\""${.ALLSRC:M*${_firmw:C/\:.*$//}}"\"	\
	    -DFIRMW_SYMBOL="${_firmw:C/\:.*$//:C/[-.\/@]/_/g}"	\
	    ${SYSDIR}/kern/firmw.S -o ${.TARGET}

OBJS+=	${_firmw:C/\:.*$/.fwo/:T}
.endfor
.endif

# Conditionally include SRCS based on kernel config options.
.for _o in ${KERN_OPTS}
SRCS+=${SRCS.${_o}}
.endfor

OBJS+=	${SRCS:N*.h:R:S/$/.o/g}

.if !defined(PROG)
PROG=	${KMOD}.ko
.endif

.if !defined(DEBUG_FLAGS) || ${MK_SPLIT_KERNEL_DEBUG} == "no"
FULLPROG=	${PROG}
.else
FULLPROG=	${PROG}.full
${PROG}: ${FULLPROG} ${PROG}.debug
	${OBJCOPY} --strip-debug --add-gnu-debuglink=${PROG}.debug \
	    ${FULLPROG} ${.TARGET}
${PROG}.debug: ${FULLPROG}
	${OBJCOPY} --only-keep-debug ${FULLPROG} ${.TARGET}
.endif

.if ${__KLD_SHARED} == yes
${FULLPROG}: ${KMOD}.kld
	${LD} -m ${LD_EMULATION} -Bshareable -znotext -znorelro ${_LDFLAGS} \
	    -o ${.TARGET} ${KMOD}.kld
.if !defined(DEBUG_FLAGS)
	${OBJCOPY} --strip-debug ${.TARGET}
.endif
.endif

EXPORT_SYMS?=	NO
.if ${EXPORT_SYMS} != YES
CLEANFILES+=	export_syms
.endif

.if exists(${SYSDIR}/conf/ldscript.kmod.${MACHINE})
LDSCRIPT_FLAGS?= -T ${SYSDIR}/conf/ldscript.kmod.${MACHINE}
.endif

.if ${__KLD_SHARED} == yes
${KMOD}.kld: ${OBJS} ${BLOB_OBJS}
.else
${FULLPROG}: ${OBJS} ${BLOB_OBJS}
.endif
	${LD} -m ${LD_EMULATION} ${_LDFLAGS} ${LDSCRIPT_FLAGS} -r \
	    -o ${.TARGET} ${OBJS} ${BLOB_OBJS}
.if ${MK_CTF} != "no"
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS} ${BLOB_OBJS}
.endif
.if defined(EXPORT_SYMS)
.if ${EXPORT_SYMS} != YES
.if ${EXPORT_SYMS} == NO
	:> export_syms
.elif !exists(${.CURDIR}/${EXPORT_SYMS})
	printf '%s' "${EXPORT_SYMS:@s@$s${.newline}@}" > export_syms
.else
	grep -v '^#' < ${EXPORT_SYMS} > export_syms
.endif
	${AWK} -f ${SYSDIR}/conf/kmod_syms.awk ${.TARGET} \
	    export_syms | xargs -J% ${OBJCOPY} % ${.TARGET}
.endif
.endif # defined(EXPORT_SYMS)
.if defined(PREFIX_SYMS)
	${AWK} -v prefix=${PREFIX_SYMS} -f ${SYSDIR}/conf/kmod_syms_prefix.awk \
	    ${.TARGET} /dev/null | xargs -J% ${OBJCOPY} % ${.TARGET}
.endif
.if !defined(DEBUG_FLAGS) && ${__KLD_SHARED} == no
	${OBJCOPY} --strip-debug ${.TARGET}
.endif

_ILINKS=machine
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_ILINKS+=x86
.endif
.if ${MACHINE_CPUARCH} == "amd64"
_ILINKS+=i386
.endif
CLEANFILES+=${_ILINKS}

all: ${PROG}

beforedepend: ${_ILINKS}
beforebuild: ${_ILINKS}

# Ensure that the links exist without depending on it when it exists which
# causes all the modules to be rebuilt when the directory pointed to changes.
# Ensure that debug info references the path in the source tree.
.for _link in ${_ILINKS}
.if !exists(${.OBJDIR}/${_link})
OBJS_DEPEND_GUESS+=	${_link}
.endif
.if ${_link} == "machine"
CFLAGS+= -fdebug-prefix-map=./machine=${SYSDIR}/${MACHINE}/include
.else
CFLAGS+= -fdebug-prefix-map=./${_link}=${SYSDIR}/${_link}/include
.endif
.endfor

.NOPATH: ${_ILINKS}

${_ILINKS}:
	@case ${.TARGET} in \
	machine) \
		path=${SYSDIR}/${MACHINE}/include ;; \
	*) \
		path=${SYSDIR}/${.TARGET:T}/include ;; \
	esac ; \
	path=`realpath $$path`; \
	${ECHO} ${.TARGET:T} "->" $$path ; \
	ln -fns $$path ${.TARGET:T}

CLEANFILES+= ${PROG} ${KMOD}.kld ${OBJS}

.if defined(DEBUG_FLAGS) && ${MK_SPLIT_KERNEL_DEBUG} != "no"
CLEANFILES+= ${FULLPROG} ${PROG}.debug
.endif

.if !target(install)

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

.if !target(realinstall)
KERN_DEBUGDIR?=	${DEBUGDIR}
realinstall: _kmodinstall
.ORDER: beforeinstall _kmodinstall
_kmodinstall: .PHONY
	${INSTALL} -T release -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}/
.if defined(DEBUG_FLAGS) && !defined(INSTALL_NODEBUG) && ${MK_KERNEL_SYMBOLS} != "no"
	${INSTALL} -T dbg -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG}.debug ${DESTDIR}${KERN_DEBUGDIR}${KMODDIR}/
.endif

.include <bsd.links.mk>

.if !defined(NO_XREF)
afterinstall: _kldxref
.ORDER: realinstall _kldxref
.ORDER: _installlinks _kldxref
_kldxref: .PHONY
	@if type kldxref >/dev/null 2>&1; then \
		${ECHO} ${KLDXREF_CMD} ${DESTDIR}${KMODDIR}; \
		${KLDXREF_CMD} ${DESTDIR}${KMODDIR}; \
	fi
.endif
.endif # !target(realinstall)

.endif # !target(install)

.if !target(load)
load: ${PROG} .PHONY
	${KMODLOAD} -v ${.OBJDIR}/${PROG}
.endif

.if !target(unload)
unload: .PHONY
	if ${KMODISLOADED} ${PROG} ; then ${KMODUNLOAD} -v ${PROG} ; fi
.endif

.if !target(reload)
reload: unload load .PHONY
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

# Add the sanitizer C flags
CFLAGS+=	${SAN_CFLAGS}

# Add the gcov flags
CFLAGS+=	${GCOV_CFLAGS}

# Respect configuration-specific C flags.
CFLAGS+=	${ARCH_FLAGS} ${CONF_CFLAGS}

.if !empty(SRCS:Mvnode_if.c)
CLEANFILES+=	vnode_if.c
vnode_if.c: ${SYSDIR}/tools/vnode_if.awk ${SYSDIR}/kern/vnode_if.src
	${AWK} -f ${SYSDIR}/tools/vnode_if.awk ${SYSDIR}/kern/vnode_if.src -c
.endif

.if !empty(SRCS:Mvnode_if.h)
CLEANFILES+=	vnode_if.h vnode_if_newproto.h vnode_if_typedef.h
vnode_if.h vnode_if_newproto.h vnode_if_typedef.h: ${SYSDIR}/tools/vnode_if.awk \
    ${SYSDIR}/kern/vnode_if.src
vnode_if.h: vnode_if_newproto.h vnode_if_typedef.h
	${AWK} -f ${SYSDIR}/tools/vnode_if.awk ${SYSDIR}/kern/vnode_if.src -h
vnode_if_newproto.h:
	${AWK} -f ${SYSDIR}/tools/vnode_if.awk ${SYSDIR}/kern/vnode_if.src -p
vnode_if_typedef.h:
	${AWK} -f ${SYSDIR}/tools/vnode_if.awk ${SYSDIR}/kern/vnode_if.src -q
.endif

# Build _if.[ch] from _if.m, and clean them when we're done.
# __MPATH defined in config.mk
_MFILES=${__MPATH:T:O}
_MPATH=${__MPATH:H:O:u}
.PATH.m: ${_MPATH}
.for _i in ${SRCS:M*_if.[ch]}
_MATCH=M${_i:R:S/$/.m/}
_MATCHES=${_MFILES:${_MATCH}}
.if !empty(_MATCHES)
CLEANFILES+=	${_i}
.endif
.endfor # _i
.m.c:	${SYSDIR}/tools/makeobjops.awk
	${AWK} -f ${SYSDIR}/tools/makeobjops.awk ${.IMPSRC} -c

.m.h:	${SYSDIR}/tools/makeobjops.awk
	${AWK} -f ${SYSDIR}/tools/makeobjops.awk ${.IMPSRC} -h

.for _i in mii pccard
.if !empty(SRCS:M${_i}devs.h)
CLEANFILES+=	${_i}devs.h
${_i}devs.h: ${SYSDIR}/tools/${_i}devs2h.awk ${SYSDIR}/dev/${_i}/${_i}devs
	${AWK} -f ${SYSDIR}/tools/${_i}devs2h.awk ${SYSDIR}/dev/${_i}/${_i}devs
.endif
.endfor # _i

.if !empty(SRCS:Mbhnd_nvram_map.h)
CLEANFILES+=	bhnd_nvram_map.h
bhnd_nvram_map.h: ${SYSDIR}/dev/bhnd/tools/nvram_map_gen.awk \
    ${SYSDIR}/dev/bhnd/tools/nvram_map_gen.sh \
    ${SYSDIR}/dev/bhnd/nvram/nvram_map
bhnd_nvram_map.h:
	sh ${SYSDIR}/dev/bhnd/tools/nvram_map_gen.sh \
	    ${SYSDIR}/dev/bhnd/nvram/nvram_map -h
.endif

.if !empty(SRCS:Mbhnd_nvram_map_data.h)
CLEANFILES+=	bhnd_nvram_map_data.h
bhnd_nvram_map_data.h: ${SYSDIR}/dev/bhnd/tools/nvram_map_gen.awk \
    ${SYSDIR}/dev/bhnd/tools/nvram_map_gen.sh \
    ${SYSDIR}/dev/bhnd/nvram/nvram_map
bhnd_nvram_map_data.h:
	sh ${SYSDIR}/dev/bhnd/tools/nvram_map_gen.sh \
	    ${SYSDIR}/dev/bhnd/nvram/nvram_map -d
.endif

.if !empty(SRCS:Musbdevs.h)
CLEANFILES+=	usbdevs.h
usbdevs.h: ${SYSDIR}/tools/usbdevs2h.awk ${SYSDIR}/dev/usb/usbdevs
	${AWK} -f ${SYSDIR}/tools/usbdevs2h.awk ${SYSDIR}/dev/usb/usbdevs -h
.endif

.if !empty(SRCS:Musbdevs_data.h)
CLEANFILES+=	usbdevs_data.h
usbdevs_data.h: ${SYSDIR}/tools/usbdevs2h.awk ${SYSDIR}/dev/usb/usbdevs
	${AWK} -f ${SYSDIR}/tools/usbdevs2h.awk ${SYSDIR}/dev/usb/usbdevs -d
.endif

.if !empty(SRCS:Msdiodevs.h)
CLEANFILES+=	sdiodevs.h
sdiodevs.h: ${SYSDIR}/tools/sdiodevs2h.awk ${SYSDIR}/dev/sdio/sdiodevs
	${AWK} -f ${SYSDIR}/tools/sdiodevs2h.awk ${SYSDIR}/dev/sdio/sdiodevs -h
.endif

.if !empty(SRCS:Msdiodevs_data.h)
CLEANFILES+=	sdiodevs_data.h
sdiodevs_data.h: ${SYSDIR}/tools/sdiodevs2h.awk ${SYSDIR}/dev/sdio/sdiodevs
	${AWK} -f ${SYSDIR}/tools/sdiodevs2h.awk ${SYSDIR}/dev/sdio/sdiodevs -d
.endif

.if !empty(SRCS:Macpi_quirks.h)
CLEANFILES+=	acpi_quirks.h
acpi_quirks.h: ${SYSDIR}/tools/acpi_quirks2h.awk ${SYSDIR}/dev/acpica/acpi_quirks
	${AWK} -f ${SYSDIR}/tools/acpi_quirks2h.awk ${SYSDIR}/dev/acpica/acpi_quirks
.endif

.if !empty(SRCS:Massym.inc) || !empty(DPSRCS:Massym.inc)
CLEANFILES+=	assym.inc genassym.o
DEPENDOBJS+=	genassym.o
DPSRCS+=	offset.inc
.endif
.if defined(MODULE_TIED)
DPSRCS+=	offset.inc
.endif
.if !empty(SRCS:Moffset.inc) || !empty(DPSRCS:Moffset.inc)
CLEANFILES+=	offset.inc genoffset.o
DEPENDOBJS+=	genoffset.o
.endif
assym.inc: genassym.o
offset.inc: genoffset.o
assym.inc: ${SYSDIR}/kern/genassym.sh
	sh ${SYSDIR}/kern/genassym.sh genassym.o > ${.TARGET}
genassym.o: ${SYSDIR}/${MACHINE}/${MACHINE}/genassym.c offset.inc
genassym.o: ${SRCS:Mopt_*.h}
	${CC} -c ${CFLAGS:N-flto:N-fno-common} -fcommon \
	    ${SYSDIR}/${MACHINE}/${MACHINE}/genassym.c
offset.inc: ${SYSDIR}/kern/genoffset.sh genoffset.o
	sh ${SYSDIR}/kern/genoffset.sh genoffset.o > ${.TARGET}
genoffset.o: ${SYSDIR}/kern/genoffset.c
genoffset.o: ${SRCS:Mopt_*.h}
	${CC} -c ${CFLAGS:N-flto:N-fno-common} -fcommon \
	    ${SYSDIR}/kern/genoffset.c

CLEANDEPENDFILES+=	${_ILINKS}
# .depend needs include links so we remove them only together.
cleanilinks:
	rm -f ${_ILINKS}

OBJS_DEPEND_GUESS+= ${SRCS:M*.h}
.if defined(KERNBUILDDIR)
OBJS_DEPEND_GUESS+= opt_global.h
.endif

ZINCDIR=${SYSDIR}/contrib/openzfs/include
OPENZFS_CFLAGS=     \
	-D_SYS_VMEM_H_  \
	-D__KERNEL__ \
	-nostdinc \
	-DSMP \
	-I${ZINCDIR}  \
	-I${ZINCDIR}/os/freebsd \
	-I${ZINCDIR}/os/freebsd/spl \
	-I${ZINCDIR}/os/freebsd/zfs \
	-I${SYSDIR}/cddl/compat/opensolaris \
	-I${SYSDIR}/cddl/contrib/opensolaris/uts/common \
	-include ${ZINCDIR}/os/freebsd/spl/sys/ccompile.h

.include <bsd.dep.mk>
.include <bsd.clang-analyze.mk>
.include <bsd.obj.mk>
.include "kern.mk"
