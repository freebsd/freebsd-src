#	from: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
#	$Id: bsd.prog.mk,v 1.28 1994/06/15 10:14:41 ache Exp $

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

CFLAGS+=${COPTS}
.if defined(DESTDIR)
CFLAGS+= -I${DESTDIR}/usr/include
CXXINCLUDES+= -I${DESTDIR}/usr/include/${CXX}
.endif

STRIP?=	-s

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555

INSTALL?=	install
.if !defined(DESTDIR)
LIBCRT0?=	/usr/lib/crt0.o
LIBC?=		/usr/lib/libc.a
LIBCOMPAT?=	/usr/lib/libcompat.a
LIBCRYPT?=	/usr/lib/libcrypt.a
LIBCURSES?=	/usr/lib/libcurses.a
LIBDBM?=	/usr/lib/libdbm.a
LIBDES?=	/usr/lib/libdes.a
LIBGNUMALLOC?=	/usr/lib/libgnumalloc.a
LIBGNUREGEX?=	/usr/lib/libgnuregex.a
LIBL?=		/usr/lib/libl.a
LIBKDB?=	/usr/lib/libkdb.a
LIBKRB?=	/usr/lib/libkrb.a
LIBM?=		/usr/lib/libm.a
LIBMP?=		/usr/lib/libmp.a
LIBPC?=		/usr/lib/libpc.a
LIBPLOT?=	/usr/lib/libplot.a
LIBREADLINE?=	/usr/lib/libreadline.a
LIBRESOLV?=	/usr/lib/libresolv.a
LIBRPCSVC?=	/usr/lib/librpcsvc.a
LIBSKEY?=	/usr/lib/libskey.a
LIBTELNET?=	/usr/lib/libtelnet.a
LIBTERM?=       /usr/lib/libtermcap.a
LIBUTIL?=	/usr/lib/libutil.a
.else
LIBCRT0?=	${DESTDIR}/usr/lib/crt0.o
LIBC?=		${DESTDIR}/usr/lib/libc.a
LIBCOMPAT?=	${DESTDIR}/usr/lib/libcompat.a
LIBCRYPT?=	${DESTDIR}/usr/lib/libcrypt.a
LIBCURSES?=	${DESTDIR}/usr/lib/libcurses.a
LIBDBM?=	${DESTDIR}/usr/lib/libdbm.a
LIBDES?=	${DESTDIR}/usr/lib/libdes.a
LIBGNUMALLOC?=	${DESTDIR}/usr/lib/libgnumalloc.a
LIBGNUREGEX?=	${DESTDIR}/usr/lib/libgnuregex.a
LIBL?=		${DESTDIR}/usr/lib/libl.a
LIBKDB?=	${DESTDIR}/usr/lib/libkdb.a
LIBKRB?=	${DESTDIR}/usr/lib/libkrb.a
LIBM?=		${DESTDIR}/usr/lib/libm.a
LIBMP?=		${DESTDIR}/usr/lib/libmp.a
LIBPC?=		${DESTDIR}/usr/lib/libpc.a
LIBPLOT?=	${DESTDIR}/usr/lib/libplot.a
LIBREADLINE?=	${DESTDIR}/usr/lib/libreadline.a
LIBRESOLV?=	${DESTDIR}/usr/lib/libresolv.a
LIBRPCSVC?=	${DESTDIR}/usr/lib/librpcsvc.a
LIBSKEY?=	${DESTDIR}/usr/lib/libskey.a
LIBTELNET?=	${DESTDIR}/usr/lib/libtelnet.a
LIBTERM?=       ${DESTDIR}/usr/lib/libtermcap.a
LIBUTIL?=	${DESTDIR}/usr/lib/libutil.a
.endif
.if defined(NOSHARED)
LDFLAGS+= -static
.endif

.if defined(SHAREDSTRINGS)
CLEANFILES+=strings
.c.o:
	${CC} -E ${CFLAGS} ${.IMPSRC} | xstr -c -
	@${CC} ${CFLAGS} -c x.c -o ${.TARGET}
	@rm -f x.c

.cc.o .cxx.o .C.o:
	${CXX} -E ${CXXFLAGS} ${.IMPSRC} | xstr -c -
	@mv -f x.c x.cc
	@${CXX} ${CXXFLAGS} -c x.cc -o ${.TARGET}

.endif

.if defined(DESTDIR)
LDDESTDIR?=	-L${DESTDIR}/usr/lib
.endif

.if defined(PROG)
.if defined(SRCS)

DPSRCS+= ${SRCS:M*.h}
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.if defined(LDONLY)

${PROG}: ${LIBCRT0} ${LIBC} ${DPSRCS} ${OBJS} ${DPADD} 
	${LD} ${LDFLAGS} -o ${.TARGET} ${LIBCRT0} ${OBJS} ${LIBC} ${LDDESTDR} \
		${LDADD}

.else defined(LDONLY)

${PROG}: ${DPSRCS} ${OBJS} ${LIBC} ${DPADD}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDDESTDIR} ${LDADD}

.endif

.else defined(PROG)

SRCS= ${PROG}.c

${PROG}: ${DPSRCS} ${SRCS} ${LIBC} ${DPADD}
	${CC} ${LDFLAGS} ${CFLAGS} -o ${.TARGET} ${.CURDIR}/${SRCS} \
		${LDDESTDIR} ${LDADD}

MKDEP=	-p

.endif

.if	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(NOMAN)
MAN1=	${PROG}.1
.endif
.endif

_PROGSUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(echo "===> $$entry"; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			cd ${.CURDIR}/$${entry}.${MACHINE}; \
		else \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:S/realinstall/install/:S/.depend/depend/}); \
	done
.endif

.MAIN: all
all: ${PROG} _PROGSUBDIR

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
.if defined(PROG)
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}
.endif
.if defined(HIDEGAME)
	(cd ${DESTDIR}/usr/games; rm -f ${PROG}; ln -s dm ${PROG}; \
	    chown games.bin ${PROG})
.endif
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
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

.if !target(lint)
lint: ${SRCS} _PROGSUBDIR
.if defined(PROG)
	@${LINT} ${LINTFLAGS} ${CFLAGS} ${.ALLSRC} | more 2>&1
.endif
.endif

.if !target(obj)
.if defined(NOOBJ)
obj: _PROGSUBDIR
.else
obj: _PROGSUBDIR
	@cd ${.CURDIR}; rm -rf obj; \
	here=`pwd`; dest=/usr/obj`echo $$here | sed 's,^/usr/src,,'`; \
	echo "$$here -> $$dest"; ln -s $$dest obj; \
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
.endif

.include <bsd.dep.mk>
