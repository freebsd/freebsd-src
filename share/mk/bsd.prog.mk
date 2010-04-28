#	from: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
# $FreeBSD$

.include <bsd.init.mk>

.SUFFIXES: .out .o .c .cc .cpp .cxx .C .m .y .l .ln .s .S .asm

# XXX The use of COPTS in modern makefiles is discouraged.
.if defined(COPTS)
CFLAGS+=${COPTS}
.endif

.if ${MK_ASSERT_DEBUG} == "no"
CFLAGS+= -DNDEBUG
NO_WERROR=
.endif

# Enable CTF conversion on request.
.if defined(WITH_CTF)
.undef NO_CTF
.endif

.if defined(DEBUG_FLAGS)
CFLAGS+=${DEBUG_FLAGS}
CXXFLAGS+=${DEBUG_FLAGS}

.if !defined(NO_CTF) && (${DEBUG_FLAGS:M-g} != "")
CTFFLAGS+= -g
.endif
.endif

.if defined(CRUNCH_CFLAGS)
CFLAGS+=${CRUNCH_CFLAGS}
.endif

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

.if defined(NO_SHARED) && (${NO_SHARED} != "no" && ${NO_SHARED} != "NO")
LDFLAGS+= -static
.endif

.if defined(PROG_CXX)
PROG=	${PROG_CXX}
.endif

.if defined(PROG)
.if defined(SRCS)

# If there are Objective C sources, link with Objective C libraries.
.if !empty(SRCS:M*.m)
.if defined(OBJCLIBS)
LDADD+=	${OBJCLIBS}
.else
DPADD+=	${LIBOBJC} ${LIBPTHREAD}
LDADD+=	-lobjc -lpthread
.endif
.endif

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

${PROG}: ${OBJS}
.if defined(PROG_CXX)
	${CXX} ${CXXFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}
.else
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}
.endif
	@[ -z "${CTFMERGE}" -o -n "${NO_CTF}" ] || ${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS}

.else	# !defined(SRCS)

.if !target(${PROG})
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

${PROG}: ${OBJS}
.if defined(PROG_CXX)
	${CXX} ${CXXFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}
.else
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}
.endif
	@[ -z "${CTFMERGE}" -o -n "${NO_CTF}" ] || ${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS}
.endif

.endif

.if	${MK_MAN} != "no" && !defined(MAN) && \
	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(MAN9) && \
	!defined(MAN1aout)
MAN=	${PROG}.1
MAN1=	${MAN}
.endif
.endif

all: objwarn ${PROG} ${SCRIPTS}
.if ${MK_MAN} != "no"
all: _manpages
.endif

.if defined(PROG)
CLEANFILES+= ${PROG}
.endif

.if defined(OBJS)
CLEANFILES+= ${OBJS}
.endif

.include <bsd.libnames.mk>

.if defined(PROG)
_EXTRADEPEND:
.if defined(LDFLAGS) && !empty(LDFLAGS:M-nostdlib)
.if defined(DPADD) && !empty(DPADD)
	echo ${PROG}: ${DPADD} >> ${DEPENDFILE}
.endif
.else
	echo ${PROG}: ${LIBC} ${DPADD} >> ${DEPENDFILE}
.if defined(PROG_CXX)
	echo ${PROG}: ${LIBSTDCPLUSPLUS} >> ${DEPENDFILE}
.endif
.endif
.endif

.if !target(install)

.if defined(PRECIOUSPROG)
.if !defined(NO_FSCHG)
INSTALLFLAGS+= -fschg
.endif
INSTALLFLAGS+= -S
.endif

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

.if !target(realinstall) && !defined(INTERNALPROG)
realinstall: _proginstall
.ORDER: beforeinstall _proginstall
_proginstall:
.if defined(PROG)
.if defined(PROGNAME)
	${INSTALL} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}/${PROGNAME}
.else
	${INSTALL} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}
.endif
.endif
.endif	# !target(realinstall)

.if defined(SCRIPTS) && !empty(SCRIPTS)
realinstall: _scriptsinstall
.ORDER: beforeinstall _scriptsinstall

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
	${INSTALL} -o ${SCRIPTSOWN_${.ALLSRC:T}} \
	    -g ${SCRIPTSGRP_${.ALLSRC:T}} -m ${SCRIPTSMODE_${.ALLSRC:T}} \
	    ${.ALLSRC} \
	    ${DESTDIR}${SCRIPTSDIR_${.ALLSRC:T}}/${SCRIPTSNAME_${.ALLSRC:T}}
.endfor
.endif

NLSNAME?=	${PROG}
.include <bsd.nls.mk>

.include <bsd.files.mk>
.include <bsd.incs.mk>
.include <bsd.links.mk>

.if ${MK_MAN} != "no"
realinstall: _maninstall
.ORDER: beforeinstall _maninstall
.endif

.endif

.if !target(lint)
lint: ${SRCS:M*.c}
.if defined(PROG)
	${LINT} ${LINTFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC}
.endif
.endif

.if ${MK_MAN} != "no"
.include <bsd.man.mk>
.endif

.include <bsd.dep.mk>

.if defined(PROG) && !exists(${.OBJDIR}/${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>

.include <bsd.sys.mk>

.if defined(PORTNAME)
.include <bsd.pkg.mk>
.endif
