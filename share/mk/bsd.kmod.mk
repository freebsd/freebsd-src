#	From: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
#	$Id: bsd.kmod.mk,v 1.11 1995/03/20 19:18:51 wollman Exp $

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

#
# Assume that we are in /usr/src/foo/bar, so /sys is
# ${.CURDIR}/../../sys.  We don't bother adding a .PATH since nothing
# actually lives in /sys directly.
#
CFLAGS+=${COPTS} -DKERNEL -I${.CURDIR}/../../sys -W -Wcomment -Wredundant-decls

KMODGRP?=	bin
KMODOWN?=	bin
KMODMODE?=	555

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
	${LD} -r ${LDFLAGS} -o ${.TARGET} ${OBJS}

.if	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(NOMAN)
MAN1=	${KMOD}.4
.endif

_PROGSUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(${ECHODIR} "===> $$entry"; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			cd ${.CURDIR}/$${entry}.${MACHINE}; \
		else \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:S/realinstall/install/:S/.depend/depend/}); \
	done
.endif

.MAIN: all
all: ${PROG} all-man _PROGSUBDIR

.if !target(clean)
clean: _PROGSUBDIR
	rm -f a.out [Ee]rrs mklog ${PROG} ${OBJS} ${CLEANFILES} 
.endif

.if !target(cleandir)
cleandir: _PROGSUBDIR
	rm -f a.out [Ee]rrs mklog ${PROG} ${OBJS} ${CLEANFILES}
	rm -f ${.CURDIR}/tags .depend
	cd ${.CURDIR}; rm -rf obj;
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

realinstall: _PROGSUBDIR
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}
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

install: afterinstall
.if !defined(NOMAN)
afterinstall: realinstall maninstall
.else
afterinstall: realinstall
.endif
realinstall: beforeinstall
.endif

DISTRIBUTION?=	bin
.if !target(distribute)
distribute:
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${DISTRIBUTION} SHARED=copies
.endif

.if !target(obj)
.if defined(NOOBJ)
obj: _PROGSUBDIR
.else
obj: _PROGSUBDIR
	@cd ${.CURDIR}; rm -rf obj; \
	here=`pwd`; dest=/usr/obj`echo $$here | sed 's,^/usr/src,,'`; \
	${ECHO} "$$here -> $$dest"; ln -s $$dest obj; \
	if test -d /usr/obj -a ! -d $$dest; then \
		mkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

.if !target(tags)
tags: ${SRCS} _PROGSUBDIR
.if defined(PROG)
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC} | \
	    sed "s;\${.CURDIR}/;;" > tags
.endif
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.elif !target(maninstall)
maninstall:
all-man:
.endif

.if !target(load)
load:	${PROG}
	/sbin/modload -o ${KMOD} -e${KMOD} ${PROG}
.endif

.if !target(unload)
unload:	${PROG}
	/sbin/modunload -n ${KMOD}
.endif

KERN=	${.CURDIR}/../../sys/kern

vnode_if.h:	${KERN}/vnode_if.sh ${KERN}/vnode_if.src
	sh ${KERN}/vnode_if.sh ${KERN}/vnode_if.src

./vnode_if.h:	vnode_if.h

_DEPSUBDIR=	_PROGSUBDIR
.include <bsd.dep.mk>
