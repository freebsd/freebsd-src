#	from: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
#	$Id: bsd.prog.mk,v 1.68 1998/05/04 13:54:41 bde Exp $

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

# Default executable format
BINFORMAT?=	aout

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

CFLAGS+=${COPTS} ${DEBUG_FLAGS}
.if defined(DESTDIR)
CFLAGS+= -I${DESTDIR}/usr/include
CXXINCLUDES+= -I${DESTDIR}/usr/include/g++
.endif

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

.if defined(NOSHARED) && ( ${NOSHARED} != "no" && ${NOSHARED} != "NO" )
LDFLAGS+= -static
.endif

.if defined(PROG)
.if defined(SRCS)

CLEANFILES?=

.for _LSRC in ${SRCS:M*.l}
.for _LC in ${_LSRC:S/.l/.c/}
${_LC}: ${_LSRC}
	${LEX} -t ${LFLAGS} ${.ALLSRC} > ${.TARGET}
SRCS:=	${SRCS:S/${_LSRC}/${_LC}/}
CLEANFILES:= ${CLEANFILES} ${_LC}
.endfor
.endfor

.for _YSRC in ${SRCS:M*.y}
.for _YC in ${_YSRC:S/.y/.c/}
.for _YH in ${_YSRC:S/.y/.h/}
.ORDER: ${_YC} ${_YH}
${_YC} ${_YH}: ${_YSRC}
	${YACC} ${YFLAGS} -o ${_YC} ${.ALLSRC}
SRCS:=	${SRCS:S/${_YSRC}/${_YC}/}
CLEANFILES:= ${CLEANFILES} ${_YC}
.if ${YFLAGS:M-d} != ""
y.tab.h:
	ln -sf ${_YH} ${.TARGET}
SRCS:=	${SRCS} y.tab.h
CLEANFILES:= ${CLEANFILES} ${_YH} y.tab.h
.endif
.endfor
.endfor
.endfor

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

${PROG}: ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDDESTDIR} ${LDADD}

.else !defined(SRCS)

.if !target(${PROG})
SRCS=	${PROG}.c

# Always make an intermediate object file because:
# - it saves time rebuilding when only the library has changed
# - the name of the object gets put into the executable symbol table instead of
#   the name of a variable temporary object.
# - it's useful to keep objects around for crunching.
OBJS=	${PROG}.o

${PROG}: ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDDESTDIR} ${LDADD}
.endif

.endif

.if	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(NOMAN)
MAN1=	${PROG}.1
.endif
.endif

.MAIN: all
all: objwarn ${PROG} all-man _SUBDIR

CLEANFILES+= ${PROG} ${OBJS}

.if defined(PROG) && !defined(NOEXTRADEPEND)
_EXTRADEPEND:
.if ${BINFORMAT} == aout
	echo ${PROG}: `${CC} -Wl,-f ${CFLAGS} ${LDFLAGS} ${LDDESTDIR} \
	    ${LDADD:S/^/-Wl,/}` >> ${DEPENDFILE}
.else
.if defined(DPADD) && !empty(DPADD)
	echo ${PROG}: ${DPADD} >> ${DEPENDFILE}
.endif
.endif
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

realinstall: beforeinstall
.if defined(PROG)
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}
.endif
.if defined(HIDEGAME)
	(cd ${DESTDIR}/usr/games; rm -f ${PROG}; ln -s dm ${PROG}; \
	    chown games:bin ${PROG})
.endif
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
.endif

DISTRIBUTION?=	bin
.if !target(distribute)
distribute: _SUBDIR
.for dist in ${DISTRIBUTION}
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
.endif

.if !target(lint)
lint: ${SRCS} _SUBDIR
.if defined(PROG)
	@${LINT} ${LINTFLAGS} ${CFLAGS} ${.ALLSRC} | more 2>&1
.endif
.endif

.if defined(NOTAGS)
tags:
.endif

.if !target(tags)
tags: ${SRCS} _SUBDIR
.if defined(PROG)
	@cd ${.CURDIR} && gtags ${GTAGSFLAGS} ${.OBJDIR}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS} -d ${.OBJDIR} ${.OBJDIR}
.endif
.endif
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.elif !target(maninstall)
maninstall:
all-man:
.endif

.if !target(regress)
regress:
.endif

.if ${BINFORMAT} != aout || make(checkdpadd)
.include <bsd.libnames.mk>
.endif

.include <bsd.dep.mk>

.if defined(PROG) && !exists(${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>
