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
# DISTRIBUTION  Name of distribution. [bin]
#
# KERN		Main Kernel source directory. [${.CURDIR}/../../sys/kern]
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

CFLAGS+=	${COPTS} -DKERNEL ${CWARNFLAGS}
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
CFLAGS+=	-I${.OBJDIR} -I${.OBJDIR}/@

# XXX this is now dubious.
.if defined(DESTDIR)
CFLAGS+=	-I${DESTDIR}/usr/include
.endif

.if defined(VFS_KLD)
SRCS+=		vnode_if.h
CLEANFILES+=	vnode_if.h vnode_if.c
.endif

.if ${OBJFORMAT} == elf
CLEANFILES+=	setdef0.c setdef1.c setdefs.h
CLEANFILES+=	setdef0.o setdef1.o
.endif

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.if !defined(PROG)
PROG=	${KMOD}.ko
.endif

${PROG}: ${OBJS} ${DPADD} ${KMODDEPS}
.if ${OBJFORMAT} == elf
	gensetdefs ${OBJS}
	${CC} ${CFLAGS} -c setdef0.c
	${CC} ${CFLAGS} -c setdef1.c
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} setdef0.o ${OBJS} setdef1.o  ${KMODDEPS}
.else
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} ${OBJS} ${KMODDEPS}
.endif

.if defined(KMODDEPS)
.for dep in ${KMODDEPS}
CLEANFILES+=	${dep} __${dep}_hack_dep.c

${dep}:
	touch __${dep}_hack_dep.c
	${CC} -shared ${CFLAGS} -o ${dep} __${dep}_hack_dep.c
.endfor
.endif

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

# The search for the link targets works best if we are in a normal src
# tree, and not too deeply below src/sys/modules.  If we are near "/", then
# we may find /sys - this is harmless.  Other abnormal "sys" directories
# found in the search are likely to cause problems.  If nothing is found,
# then the links default to /usr/include and /usr/include/machine.
${_ILINKS}:
	@set +x; for up in ../.. ../../.. ; do \
		case ${.TARGET} in \
		machine) \
			testpath=${.CURDIR}/$$up/${MACHINE_ARCH}/include ; \
			path=${.CURDIR}/$$up/${MACHINE_ARCH}/include ; \
			defaultpath=/usr/include/machine ;; \
		@) \
			testpath=${.CURDIR}/$$up/sys ; \
			path=${.CURDIR}/$$up ; \
			defaultpath=/usr/include ;; \
		esac ; \
		if [ -d $$testpath ] ; then break ; fi ; \
		path=$$defaultpath ; \
	done ; \
	path=`(cd $$path && /bin/pwd)` ; \
	${ECHO} ${.TARGET} "->" $$path ; \
	ln -s $$path ${.TARGET}

CLEANFILES+= ${PROG} ${OBJS} ${_ILINKS} symb.tmp tmp.o

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
load:	${PROG} install
	${KMODLOAD} ${KMOD}
.endif

.if !target(unload)
unload:
	${KMODUNLOAD} ${KMOD}
.endif

.if exists(${.CURDIR}/../../kern)
KERN=	${.CURDIR}/../../kern
.else
KERN=	${.CURDIR}/../../sys/kern
.endif

vnode_if.c vnode_if.h:	${KERN}/vnode_if.sh ${KERN}/vnode_if.src
	sh ${KERN}/vnode_if.sh ${KERN}/vnode_if.src

regress:

.include <bsd.dep.mk>

.if !exists(${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>

.include <bsd.kern.mk>
