#	From: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
#	$Id: bsd.kmod.mk,v 1.32 1997/04/28 00:02:22 fsmp Exp $
#
# The include file <bsd.kmod.mk> handles installing Loadable Kernel Modules.
# <bsd.kmod.mk> includes the file named "../Makefile.inc" if it exists,
# as well as the include file <bsd.obj.mk>, <bsd.dep.mk>, and
# may be <bsd.man.mk>
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
# LN_FLAGS	Flags for ln(1) (see variable LINKS)
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
# SUBDIR        A list of subdirectories that should be built as well.
#               Each of the targets will execute the same target in the
#               subdirectories.
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
# 	tags:
#		Create a tags file for the source files.
#
# 	unload:
#		Unload LKM.
#
# bsd.obj.mk: clean, cleandir and obj
# bsd.dep.mk: depend
# bsd.man.mk: maninstall
#

MODLOAD?=	/sbin/modload
MODUNLOAD?=	/sbin/modunload

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

#
# A temporary fix to survive SMP changes.   
#
CFLAGS+= -I.
beforedepend: opt_smp.h opt_smp_invltlb.h

opt_smp.h:
	touch opt_smp.h

opt_smp_invltlb.h:
	touch opt_smp_invltlb.h

#
# Assume that we are in /usr/src/foo/bar, so /sys is
# ${.CURDIR}/../../sys.  We don't bother adding a .PATH since nothing
# actually lives in /sys directly.
#
CFLAGS+=${COPTS} -DKERNEL -DACTUALLY_LKM_NOT_KERNEL -I${.CURDIR}/../../sys \
	${CWARNFLAGS}

EXPORT_SYMS?= _${KMOD}

.if defined(VFS_LKM)
CFLAGS+= -DVFS_LKM -DMODVNOPS=${KMOD}vnops -I.
SRCS+=	vnode_if.h
CLEANFILES+=	vnode_if.h vnode_if.c
.endif

.if defined(PSEUDO_LKM)
CFLAGS+= -DPSEUDO_LKM
.endif

DPSRCS+= ${SRCS:M*.h}
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.if !defined(PROG)
PROG=	${KMOD}.o
.endif

${PROG}: ${DPSRCS} ${OBJS} ${DPADD} 
	${LD} -r ${LDFLAGS} -o tmp.o ${OBJS}
.if defined(EXPORT_SYMS)
	@rm -f symb.tmp
	@for i in ${EXPORT_SYMS} ; do echo $$i >> symb.tmp ; done
	symorder -c symb.tmp tmp.o
	@rm -f symb.tmp
.endif
	mv tmp.o ${.TARGET}

.if !defined(NOMAN)
.include <bsd.man.mk>
.if !defined(_MANPAGES) || empty(_MANPAGES)
MAN1=	${KMOD}.4
.endif

.elif !target(maninstall)
maninstall: _SUBDIR
all-man:
.endif

.MAIN: all
all: objwarn ${PROG} all-man _SUBDIR

CLEANFILES+=${PROG} ${OBJS} 

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
		rm -f $$t; \
		ln ${LN_FLAGS} $$l $$t; \
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
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${DISTRIBUTION} SHARED=copies
.endif

.if defined(NOTAGS)
tags:
.endif

.if !target(tags)
tags: ${SRCS} _SUBDIR
.if defined(PROG)
	@cd ${.CURDIR} && gtags ${GTAGSFLAGS}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS}
.endif
.endif
.endif


.if !target(load)
load:	${PROG}
	${MODLOAD} -o ${KMOD} -e${KMOD} ${PROG}
.endif

.if !target(unload)
unload:	${PROG}
	${MODUNLOAD} -n ${KMOD}
.endif

KERN=	${.CURDIR}/../../sys/kern

vnode_if.h:	${KERN}/vnode_if.sh ${KERN}/vnode_if.src
	sh ${KERN}/vnode_if.sh ${KERN}/vnode_if.src

./vnode_if.h:	vnode_if.h

.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.kern.mk>
