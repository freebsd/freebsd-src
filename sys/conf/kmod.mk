#	From: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
#	$Id: bsd.kmod.mk,v 1.58 1998/11/11 07:40:44 peter Exp $
#
# The include file <bsd.kmod.mk> handles installing Loadable Kernel Modules.
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# DISTRIBUTION  Name of distribution. [bin]
#
# EXPORT_SYMS	???
#
# KERN		Main Kernel source directory. [${.CURDIR}/../../sys/kern]
#
# KMOD          The name of the loadable kernel module to build.
#
# KMODDIR	Base path for loadable kernel modules
#		(see lkm(4)). [/lkm]
#
# KMODOWN	LKM owner. [${BINOWN}]
#
# KMODGRP	LKM group. [${BINGRP}]
#
# KMODMODE	LKM mode. [${BINMODE}]
#
# LINKS		The list of LKM links; should be full pathnames, the
#               linked-to file coming first, followed by the linked
#               file.  The files are hard-linked.  For example, to link
#               /lkm/master and /lkm/meister, use:
#
#			LINKS=  /lkm/master /lkm/meister
#
# MODLOAD	Command to load a kernel module [/sbin/modload]
#
# MODUNLOAD	Command to unload a kernel module [/sbin/modunload]
#
# NOMAN		LKM does not have a manual page if set.
#
# PROG          The name of the loadable kernel module to build. 
#		If not supplied, ${KMOD}.o is used.
#
# PSEUDO_LKM	???
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
#		Load LKM.
#
# 	unload:
#		Unload LKM.
#
# bsd.obj.mk: clean, cleandir and obj
# bsd.dep.mk: cleandepend, depend and tags
# bsd.man.mk: maninstall
#

MODLOAD?=	/sbin/modload
MODUNLOAD?=	/sbin/modunload

.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.endif

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

CFLAGS+=	${COPTS} -DKERNEL ${CWARNFLAGS}
.if defined(KLDMOD)
CFLAGS+=	-DKLD_MODULE
.else
CFLAGS+=	-DACTUALLY_LKM_NOT_KERNEL
.endif

# Damn bsd.own.mk is included too early.
.if defined(KLDMOD)
.if ${KMODDIR} == /lkm
KMODDIR=	/modules
.endif
.endif

# Don't use any standard or source-relative include directories.
# Since -nostdinc will annull any previous -I paths, we repeat all
# such paths after -nostdinc.  It doesn't seem to be possible to
# add to the front of `make' variable.
_ICFLAGS:=	${CFLAGS:M-I*}
CFLAGS+=	-nostdinc -I- ${_ICFLAGS}

# Add -I paths for system headers.  Individual LKM makefiles don't
# need any -I paths for this.  Similar defaults for .PATH can't be
# set because there are no standard paths for non-headers.
CFLAGS+=	-I${.OBJDIR} -I${.OBJDIR}/@

# XXX this is now dubious.
.if defined(DESTDIR)
CFLAGS+=	-I${DESTDIR}/usr/include
.endif

.if !defined(KLDMOD)
# XXX temporary until we build ELF kernels.
CFLAGS+=	-aout
LDFLAGS+=	-aout
.endif

.if defined(NOSHARED) && ( ${NOSHARED} != "no" && ${NOSHARED} != "NO" )
LDFLAGS+= -static
.endif

EXPORT_SYMS?= _${KMOD}

.if defined(VFS_LKM)
CFLAGS+= -DVFS_LKM -DMODVNOPS=${KMOD}vnops
SRCS+=	vnode_if.h
CLEANFILES+=	vnode_if.h vnode_if.c
.endif

.if defined(VFS_KLD)
CFLAGS+= -DVFS_LKM -DVFS_KLD
SRCS+=	vnode_if.h
CLEANFILES+=	vnode_if.h vnode_if.c
.endif

.if defined(KLDMOD) && ${OBJFORMAT} == elf
CLEANFILES+=	setdef0.c setdef1.c setdefs.h
CLEANFILES+=	setdef0.o setdef1.o
.endif

.if defined(PSEUDO_LKM)
CFLAGS+= -DPSEUDO_LKM
.endif

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.if !defined(PROG)
.if defined(KLDMOD)
PROG=	${KMOD}.ko
.else
PROG=	${KMOD}.o
.endif
.endif

${PROG}: ${OBJS} ${DPADD} ${KMODDEPS}
.if defined(KLDMOD)
.if ${OBJFORMAT} == elf
	gensetdefs ${OBJS}
	${CC} ${CFLAGS} -c setdef0.c
	${CC} ${CFLAGS} -c setdef1.c
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} setdef0.o ${OBJS} setdef1.o  ${KMODDEPS}
.else
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} ${OBJS} ${KMODDEPS}
.endif
.else
	${LD} -r ${LDFLAGS:N-static} -o tmp.o ${OBJS}
.if defined(EXPORT_SYMS)
	rm -f symb.tmp
	for i in ${EXPORT_SYMS} ; do echo $$i >> symb.tmp ; done
	symorder -c symb.tmp tmp.o
	rm -f symb.tmp
.endif
	mv tmp.o ${.TARGET}
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

.if !defined(KLDMOD)
# The search for the link targets works best if we are in a normal src
# tree, and not too deeply below src/lkm.  If we are near "/", then
# we may find /sys - this is harmless.  Other abnormal "sys" directories
# found in the search are likely to cause problems.  If nothing is found,
# then the links default to /usr/include and /usr/include/machine.
${_ILINKS}:
	@for up in ../.. ../../.. ; do \
		case ${.TARGET} in \
		machine) \
			path=${.CURDIR}/$$up/sys/${MACHINE_ARCH}/include ; \
			defaultpath=/usr/include/machine ;; \
		@) \
			path=${.CURDIR}/$$up/sys ; \
			defaultpath=/usr/include ;; \
		esac ; \
		if [ -d $$path ] ; then break ; fi ; \
		path=$$defaultpath ; \
	done ; \
	path=`(cd $$path && /bin/pwd)` ; \
	${ECHO} ${.TARGET} "->" $$path ; \
	ln -s $$path ${.TARGET}
.else
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
.endif

CLEANFILES+= ${PROG} ${OBJS} ${_ILINKS} lkm_verify_tmp symb.tmp tmp.o

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

realinstall: _SUBDIR
	${INSTALL} ${COPY} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}
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
	${MODLOAD} -o ${KMOD} -e${KMOD} ${PROG}
.endif

.if !target(unload)
unload:	${PROG}
	${MODUNLOAD} -n ${KMOD}
.endif

.if exists(${.CURDIR}/../../kern)
KERN=	${.CURDIR}/../../kern
.else
KERN=	${.CURDIR}/../../sys/kern
.endif

vnode_if.h:	${KERN}/vnode_if.sh ${KERN}/vnode_if.src
	sh ${KERN}/vnode_if.sh ${KERN}/vnode_if.src

_sysregress:	${_INLINKS} ${PROG}
	ld -A /sys/compile/LKM/kernel ${PROG} ${DEPLKMS} -o lkm_verify_tmp
	rm lkm_verify_tmp

regress:	_sysregress

.include <bsd.dep.mk>

.if !exists(${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>

.include <bsd.kern.mk>
