#	from: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
# $FreeBSD$

.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.endif

.SUFFIXES: .out .o .c .cc .cpp .cxx .C .m .y .l .s .S

CFLAGS+=${COPTS} ${DEBUG_FLAGS}

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

.if defined(NOSHARED) && ( ${NOSHARED} != "no" && ${NOSHARED} != "NO" )
LDFLAGS+= -static
.endif

.if defined(PROG_CXX)
PROG=	${PROG_CXX}
.endif

.if defined(PROG)
.if !target(${PROG})
.if defined(SRCS)

# If there are Objective C sources, link with Objective C libraries.
.if ${SRCS:M*.m} != ""
OBJCLIBS?= -lobjc
LDADD+=	${OBJCLIBS}
.endif

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.else !defined(SRCS)

.if defined(PROG_CXX)
SRCS=	${PROG}.cc
.else
SRCS=	${PROG}.c
.endif

# Always make an intermediate object file because:
# - it saves time rebuilding when only the library has changed
# - the name of the object gets put into the executable symbol table instead of
#   the name of a variable temporary object.
# - it's useful to keep objects around for crunching.
OBJS=	${PROG}.o
.endif

${PROG}: ${OBJS}
.if defined(PROG_CXX)
	${CXX} ${CXXFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}
.else
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}
.endif

.endif

.if	!defined(NOMAN) && !defined(MAN) && \
	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(MAN9) && \
	!defined(MAN1aout)
MAN=	${PROG}.1
MAN1=	${MAN}
.endif
.endif

.MAIN: all
all: objwarn ${PROG} ${SCRIPTS} ${FILES}
.if !defined(NOMAN)
all: all-man 
.endif
all: _SUBDIR

CLEANFILES+= ${PROG} ${OBJS}

.if defined(PROG)
_EXTRADEPEND:
.if ${OBJFORMAT} == aout
	echo ${PROG}: `${CC} -Wl,-f ${CFLAGS} ${LDFLAGS} \
	    ${LDADD:S/^/-Wl,/}` >> ${DEPENDFILE}
.else
	echo ${PROG}: ${LIBC} ${DPADD} >> ${DEPENDFILE}
.if defined(PROG_CXX)
	echo ${PROG}: ${LIBSTDCPLUSPLUS} >> ${DEPENDFILE}
.endif
.endif
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

realinstall: beforeinstall
.if defined(PROG)
.if defined(PROGNAME)
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}/${PROGNAME}
.else
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}
.endif
.endif
.if defined(HIDEGAME)
	(cd ${DESTDIR}${ORIGBINDIR}; ln -fs dm ${PROG}; \
	    chown -h ${BINOWN}:${ORIGBINGRP} ${PROG})
.endif
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

.if defined(SCRIPTS) && !empty(SCRIPTS)
realinstall: _scriptsinstall

SCRIPTSDIR?=	${BINDIR}
SCRIPTSOWN?=	${BINOWN}
SCRIPTSGRP?=	${BINGRP}
SCRIPTSMODE?=	${BINMODE}

.for script in ${SCRIPTS}
.if defined(SCRIPTSNAME)
SCRIPTSNAME_${script:T}?=	${SCRIPTSNAME}
.else
SCRIPTSNAME_${script:T}?=	${script:T:R}
.endif
SCRIPTSDIR_${script:T}?=	${SCRIPTSDIR}
SCRIPTSOWN_${script:T}?=	${SCRIPTSOWN}
SCRIPTSGRP_${script:T}?=	${SCRIPTSGRP}
SCRIPTSMODE_${script:T}?=	${SCRIPTSMODE}
_scriptsinstall: _SCRIPTSINS_${script:T}
_SCRIPTSINS_${script:T}: ${script}
	${INSTALL} ${COPY} -o ${SCRIPTSOWN_${.ALLSRC:T}} \
	    -g ${SCRIPTSGRP_${.ALLSRC:T}} -m ${SCRIPTSMODE_${.ALLSRC:T}} \
	    ${_INSTALLFLAGS} ${.ALLSRC} \
	    ${DESTDIR}${SCRIPTSDIR_${.ALLSRC:T}}/${SCRIPTSNAME_${.ALLSRC:T}}
.endfor
.endif

.if defined(FILES) && !empty(FILES)
realinstall: _filesinstall

FILESDIR?=	${BINDIR}
FILESOWN?=	${SHAREOWN}
FILESGRP?=	${SHAREGRP}
FILESMODE?=	${SHAREMODE}

.for file in ${FILES}
.if defined(FILESNAME)
FILESNAME_${file:T}?=	${FILESNAME}
.else
FILESNAME_${file:T}?=	${file:T}
.endif
FILESDIR_${file:T}?=	${FILESDIR}
FILESOWN_${file:T}?=	${FILESOWN}
FILESGRP_${file:T}?=	${FILESGRP}
FILESMODE_${file:T}?=	${FILESMODE}
_filesinstall: _FILESINS_${file:T}
_FILESINS_${file:T}: ${file}
	${INSTALL} ${COPY} -o ${FILESOWN_${.ALLSRC:T}} \
	    -g ${FILESGRP_${.ALLSRC:T}} -m ${FILESMODE_${.ALLSRC:T}} \
	    ${_INSTALLFLAGS} ${.ALLSRC} \
	    ${DESTDIR}${FILESDIR_${.ALLSRC:T}}/${FILESNAME_${.ALLSRC:T}}
.endfor
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
	${LINT} ${LINTFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC} | more 2>&1
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
.else
.if !target(all-man)
all-man:
.endif
.if !target(maninstall)
maninstall:
.endif
.endif

.if !target(regress)
regress:
.endif

.if ${OBJFORMAT} != aout || make(checkdpadd) || defined(NEED_LIBNAMES)
.include <bsd.libnames.mk>
.endif

.include <bsd.dep.mk>

.if defined(PROG) && !exists(${.OBJDIR}/${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>

.include <bsd.sys.mk>
