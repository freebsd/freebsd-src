#	From: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
# $FreeBSD: src/sys/conf/kmod.mk,v 1.82.2.2 2000/07/07 09:49:48 peter Exp $
#
# The include file <bsd.kmod.mk> handles installing Kernel Loadable Device
# drivers (KLD's).
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# DISTRIBUTION  Name of distribution. [bin]
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
# LINKS		The list of KLD links; should be full pathnames, the
#               linked-to file coming first, followed by the linked
#               file.  The files are hard-linked.  For example, to link
#               /modules/master and /modules/meister, use:
#
#			LINKS=  /modules/master /modules/meister
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
# SUBDIR        A list of subdirectories that should be built as well.
#               Each of the targets will execute the same target in the
#               subdirectories.
#
# SYMLINKS	Same as LINKS, except it creates symlinks and the
#		linked-to pathname may be relative.
#
# DESTDIR, DISTDIR are set by other Makefiles (e.g. bsd.own.mk)
#
# MFILES	Optionally a list of interfaces used by the module.
#		This file contains a default list of interfaces.
#
# +++ targets +++
#
#       distribute:
#               This is a variant of install, which will
#               put the stuff into the right "distribution".
#
# 	install:
#               install the program and its manual pages; if the Makefile
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

.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.endif

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
.if !defined(_MANPAGES) || empty(_MANPAGES)
MAN1=	${KMOD}.4
.endif

.elif !target(maninstall)
maninstall: _SUBDIR
all-man:
.endif

_ILINKS=@ machine

.MAIN: all
all: objwarn ${PROG} all-man _SUBDIR

beforedepend ${OBJS}: ${_ILINKS}

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
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

realinstall: _SUBDIR
	${INSTALL} ${COPY} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		${ECHO} $$t -\> $$l; \
		ln -f $$l $$t; \
	done; true
.endif
.if defined(SYMLINKS) && !empty(SYMLINKS)
	@set ${SYMLINKS}; \
	while test $$# -ge 2; do \
		l=$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		${ECHO} $$t -\> $$l; \
		ln -fs $$l $$t; \
	done; true
.endif

install: afterinstall _SUBDIR
.if !defined(NOMAN)
afterinstall: realinstall maninstall
.else
afterinstall: realinstall
.endif
realinstall: beforeinstall
.endif

DISTRIBUTION?=	bin
.if !target(distribute)
distribute: _SUBDIR
.for dist in ${DISTRIBUTION}
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
.endif

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
    dev/ppbus/ppbus_if.m dev/smbus/smbus_if.m dev/usb/usb_if.m

.for _srcsrc in ${MFILES}
.for _ext in c h
.for _src in ${SRCS:M${_srcsrc:T:R}.${_ext}}
CLEANFILES+=	${_src}
.if !target(${_src})
${_src}: @
.if exists(@)
${_src}: @/kern/makedevops.pl @/${_srcsrc}
.endif
	perl @/kern/makedevops.pl -${_ext} @/${_srcsrc}
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
