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
# KMODDIR	Base path for kernel modules (see kld(4)). [/modules]
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
# NOMAN		KLD does not have a manual page if set.
#
# PROG          The name of the kernel module to build.
#		If not supplied, ${KMOD}.o is used.
#
# SRCS          List of source files
#
# KMODDEPS	List of modules which this one is dependant on
#
# DESTDIR	Change the tree where the module gets installed. [not set]
#
# MFILES	Optionally a list of interfaces used by the module.
#		This file contains a default list of interfaces.
#
# +++ targets +++
#
# 	install:
#               install the kernel module and its manual pages; if the Makefile
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
# bsd.man.mk: maninstall
#

KMODLOAD?=	/sbin/kldload
KMODUNLOAD?=	/sbin/kldunload

.include <bsd.init.mk>

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

CFLAGS+=	${COPTS} -D_KERNEL ${CWARNFLAGS}
CFLAGS+=	-DKLD_MODULE

# Don't use any standard or source-relative include directories.
# Since -nostdinc will annull any previous -I paths, we repeat all
# such paths after -nostdinc.  It doesn't seem to be possible to
# add to the front of `make' variable.
_ICFLAGS:=	${CFLAGS:M-I*}
CFLAGS+=	-nostdinc -I- ${_ICFLAGS}

# Add -I paths for system headers.  Individual KLD makefiles don't
# need any -I paths for this.  Similar defaults for .PATH can't be
# set because there are no standard paths for non-headers.
CFLAGS+=	-I. -I@

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

CFLAGS+=	${DEBUG_FLAGS}

.if ${OBJFORMAT} == elf
CLEANFILES+=	setdef0.c setdef1.c setdefs.h
CLEANFILES+=	setdef0.o setdef1.o
.endif

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.if !defined(PROG)
PROG=	${KMOD}.ko
.endif

${PROG}: ${KMOD}.kld ${KMODDEPS}
.if ${OBJFORMAT} == elf
	gensetdefs ${KMOD}.kld
	${CC} ${CFLAGS} -c setdef0.c
	${CC} ${CFLAGS} -c setdef1.c
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} setdef0.o ${KMOD}.kld setdef1.o ${KMODDEPS}
.else
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} ${KMOD}.kld ${KMODDEPS}
.endif

.if defined(KMODDEPS)
.for dep in ${KMODDEPS}
CLEANFILES+=	${dep} __${dep}_hack_dep.c

${dep}:
	touch __${dep}_hack_dep.c
	${CC} -shared ${CFLAGS} -o ${dep} __${dep}_hack_dep.c
.endfor
.endif

${KMOD}.kld: ${OBJS}
	${LD} ${LDFLAGS} -r -o ${.TARGET} ${OBJS}

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif

_ILINKS=@ machine

all: objwarn ${PROG}
.if !defined(NOMAN)
all: _manpages
.endif

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
.endif !target(realinstall)

.include <bsd.links.mk>

.if !defined(NOMAN)
realinstall: _maninstall
.ORDER: beforeinstall _maninstall
.endif

.endif !target(install)

.if !target(load)
load:	${PROG}
	${KMODLOAD} -v ./${KMOD}.ko
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
    dev/iicbus/iicbus_if.m isa/isa_if.m dev/mii/miibus_if.m \
    dev/pccard/card_if.m dev/pccard/power_if.m pci/pci_if.m \
    dev/ppbus/ppbus_if.m dev/smbus/smbus_if.m dev/usb/usb_if.m \
    dev/sound/pcm/ac97_if.m dev/sound/pcm/channel_if.m \
    dev/sound/pcm/feeder_if.m dev/sound/pcm/mixer_if.m \
    libkern/iconv_converter_if.m pci/agp_if.m opencrypto/crypto_if.m \
    pc98/pc98/canbus_if.m

.for _srcsrc in ${MFILES}
.for _ext in c h
.for _src in ${SRCS:M${_srcsrc:T:R}.${_ext}}
CLEANFILES+=	${_src}
.if !target(${_src})
${_src}: @
.if exists(@)
${_src}: @/kern/makeops.pl @/${_srcsrc}
.endif
	perl @/kern/makeops.pl -${_ext} @/${_srcsrc}
.endif
.endfor # _src
.endfor # _ext
.endfor # _srcsrc

.for _ext in c h
.if ${SRCS:Mvnode_if.${_ext}} != ""
CLEANFILES+=	vnode_if.${_ext}
vnode_if.${_ext}: @
.if exists(@)
vnode_if.${_ext}: @/kern/vnode_if.pl @/kern/vnode_if.src
.endif
	perl @/kern/vnode_if.pl -${_ext} @/kern/vnode_if.src
.endif
.endfor

regress:

.include <bsd.dep.mk>

.if !exists(${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>
.include <bsd.kern.mk>
