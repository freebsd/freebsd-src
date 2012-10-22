#	from: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
# $FreeBSD$

.include <bsd.init.mk>

.SUFFIXES: .out .o .c .cc .cpp .cxx .C .m .y .l .ln .s .S .asm

.if ${MK_MAN} == "no"
NO_MAN=
.endif

# Legacy knobs
.if defined(PROG) || defined(PROG_CXX)
. if defined(PROG)
PROGS=	${PROG}
. endif
. if defined(PROG_CXX)
PROGS=	${PROG_CXX}
PROGS_CXX= ${PROG_CXX}
. endif
# Loop once to keep pattern and avoid namespace pollution
. for _P in ${PROGS}
.  if defined(INTERNALPROG)
INTERNALPROG.${_P}=
.  endif
.  if !defined(NO_MAN)
.   if defined(MAN)
MAN.${_P}= ${MAN}
.   else
.    for sect in 1 1aout 2 3 4 5 6 7 8 9
.     if defined(MAN${sect})
MAN.${_P}= ${MAN${sect}}
.     endif
.    endfor
.   endif
.  endif # defined(NO_MAN)
.  if defined(NLSNAME) && !empty(NLSNAME)
NLSNAME.${P}:=	${NLSNAME}
.  endif
.  if defined(OBJS)
OBJS.${_P}:=	${OBJS}
.  endif
.  if defined(PRECIOUSPROG)
PRECIOUSPROG.${_P}=
.  endif
.  if defined(PROGNAME)
PROGNAME.${_P}=	${PROGNAME}
.  endif
.  if defined(SRCS)
SRCS.${_P}:=	${SRCS}
.  endif
. endfor
.else # !defined(PROG) && !defined(PROG_CXX)
. if defined(PROGS_CXX) && !empty(PROGS_CXX)
PROGS+=		${PROGS_CXX}
. endif
.endif # defined(PROG) || defined(PROG_CXX)

.if defined(PROGS_CXX) && !empty(PROGS_CXX)
. for _P in ${PROGS_CXX}
PROG_CXX.${_P}=
. endfor
.endif

# Avoid recursive variables
.undef NLSNAME

.if defined(COPTS)
CFLAGS+=${COPTS}
.endif

.if defined(DEBUG_FLAGS)
. if ${MK_CTF} != "no" && ${DEBUG_FLAGS:M-g} != ""
CTFFLAGS+= -g
. endif
CFLAGS+=${DEBUG_FLAGS}
CXXFLAGS+=${DEBUG_FLAGS}
.endif

STRIP?=	-s

.if ${MK_ASSERT_DEBUG} == "no"
CFLAGS+= -DNDEBUG
NO_WERROR=
.endif

.for _P in ${PROGS}

BINDIR.${_P}?= ${BINDIR}
BINGRP.${_P}?= ${BINGRP}
BINMODE.${_P}?=	${BINMODE}
BINOWN.${_P}?= ${BINOWN}

CFLAGS.${_P}+= ${CFLAGS}
CXXFLAGS.${_P}+= ${CXXFLAGS}
DPADD.${_P}+= ${DPADD}
LDADD.${_P}+= ${LDADD}
LDFLAGS.${_P}+=	${LDFLAGS}

INSTALLFLAGS.${_P}?= ${INSTALLFLAGS}

. if defined(PRECIOUSPROG.${_P})
.  if !defined(NO_FSCHG) && !defined(NO_FSCHG.${_P})
INSTALLFLAGS.${_P}+= -fschg
.  endif
INSTALLFLAGS.${_P}+= -S
. endif

NO_SHARED.${_P}?=	${NO_SHARED}

. if !defined(NLSDIR.${_P})
NLSDIR.${_P}:=	${NLSDIR}
. endif
. undef NLSDIR

. if !empty(NO_SHARED.${_P}) && ${NO_SHARED.${_P}:tl} != "no"
LDFLAGS.${_P}+= -static
. endif

. if defined(SRCS.${_P})

_SRCS:=		${SRCS.${_P}}
OBJS.${_P}+=	${_SRCS:N*.h:R:S/$/.o/g}

.  if target(beforelinking)
${_P}: ${OBJS.${_P}} beforelinking
.  else
${_P}: ${OBJS.${_P}}
.  endif
.  if defined(PROG_CXX.${_P})
	${CXX} ${CXXFLAGS.${_P}} ${LDFLAGS.${_P}} -o ${.TARGET} ${OBJS.${_P}} \
	    ${LDADD.${_P}}
.  else
	${CC} ${CFLAGS.${_P}} ${LDFLAGS.${_P}} -o ${.TARGET} ${OBJS.${_P}} \
	    ${LDADD.${_P}}
.  endif
.  if ${MK_CTF} != "no"
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS.${_P}}
.  endif

. else # !defined(SRCS.${_P})

.  if !target(${_P})
.   if defined(PROG_CXX.${_P})
SRCS.${_P}?= ${_P}.cc
.   else
SRCS.${_P}?= ${_P}.c
.   endif

# Always make an intermediate object file because:
# - it saves time rebuilding when only the library has changed
# - the name of the object gets put into the executable symbol table instead of
#   the name of a variable temporary object.
# - it's useful to keep objects around for crunching.
OBJS.${_P}:= ${_P}.o

.   if target(beforelinking)
${_P}: ${OBJS.${_P}} beforelinking
.   else
${_P}: ${OBJS.${_P}}
.   endif # target(beforelinking)
.   if defined(PROG_CXX.${_P})
	${CXX} ${CXXFLAGS.${_P}} ${LDFLAGS.${_P}} -o ${.TARGET} ${OBJS.${_P}} \
	    ${LDADD.${_P}}
.   else
	${CC} ${CFLAGS.${_P}} ${LDFLAGS.${_P}} -o ${.TARGET} ${OBJS.${_P}} \
	    ${LDADD.${_P}}
.   endif
.   if ${MK_CTF} != "no"
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS.${_P}}
.   endif

.  endif # !target(${_P})

. endif # defined(SRCS.${_P})

CLEANFILES+= ${OBJS.${_P}}

.endfor # for _P in ${PROGS}

all: objwarn ${PROGS} ${SCRIPTS}

.if !defined(NO_MAN)
. for _P in ${PROGS}
MAN.${_P}?= ${_P}.1
MAN:=	${MAN.${_P}}
.  include <bsd.man.mk>
. endfor
. if target(_manpages) # bsd.man.mk was included
all: _manpages
. endif
.endif

CLEANFILES+= ${PROGS}

.include <bsd.libnames.mk>

_EXTRADEPEND:
.for _P in ${PROGS}
. if !empty(LDFLAGS.${P}:M-nostdlib)
.  if !empty(DPADD.${_P})
	echo ${_P}: ${DPADD.${_P}} >> ${DEPENDFILE}
.  endif
. else
	echo ${_P}: ${LIBC} ${DPADD.${_P}} >> ${DEPENDFILE}
.  if defined(PROG_CXX.${_P})
.   if !empty(CXXFLAGS.${P}:M-stdlib=libc++)
	echo ${_P}: ${LIBCPLUSPLUS} >> ${DEPENDFILE}
.   else
	echo ${_P}: ${LIBSTDCPLUSPLUS} >> ${DEPENDFILE}
.   endif
.  endif
. endif
.endfor

.if !target(install)

. if !target(realinstall)

.  for _P in ${PROGS}

.   if !defined(INTERNALPROG.${_P})

.ORDER: beforeinstall _proginstall.${_P}
_proginstall.${_P}:
.    if defined(PROGNAME.${_P})
	${INSTALL} ${STRIP} -o ${BINOWN.${_P}} -g ${BINGRP.${_P}} \
	    -m ${BINMODE.${_P}} ${INSTALLFLAGS.${_P}} ${_P} \
	    ${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}}
.    else
	${INSTALL} ${STRIP} -o ${BINOWN.${_P}} -g ${BINGRP.${_P}} \
	    -m ${BINMODE.${_P}} ${INSTALLFLAGS.${_P}} ${_P} \
	    ${DESTDIR}${BINDIR.${_P}}
.    endif 

realinstall: _proginstall.${_P}

.   endif # !defined(INTERNALPROG.${_P})

.  endfor # for _P in ${PROGS}

. endif # !target(realinstall)

. if defined(SCRIPTS) && !empty(SCRIPTS)
SCRIPTSDIR?= ${BINDIR}
SCRIPTSOWN?= ${BINOWN}
SCRIPTSGRP?= ${BINGRP}
SCRIPTSMODE?= ${BINMODE}

.  for S in ${SCRIPTS}

realinstall: scriptsinstall
.ORDER: beforeinstall scriptsinstall

.   if defined(SCRIPTSNAME)
SCRIPTSNAME_${S}?= ${SCRIPTSNAME}
.   else
SCRIPTSNAME_${S}?= ${S:T:R}
.   endif

SCRIPTSDIR_${S}?= ${SCRIPTSDIR}
SCRIPTSOWN_${S}?= ${SCRIPTSOWN}
SCRIPTSGRP_${S}?= ${SCRIPTSGRP}
SCRIPTSMODE_${S}?= ${SCRIPTSMODE}

scriptsinstall: ${DESTDIR}${SCRIPTSDIR_${S}}/${SCRIPTSNAME_${S}}

${DESTDIR}${SCRIPTSDIR_${S}}/${SCRIPTSNAME_${S}}: ${S}
	${INSTALL} -o ${SCRIPTSOWN_${S}} \
	    -g ${SCRIPTSGRP_${S}} \
	    -m ${SCRIPTSMODE_${S}} \
	    ${.ALLSRC} \
	    ${.TARGET}

.  endfor # for S in ${SCRIPTS}

. endif # defined(SCRIPTS) && !empty(SCRIPTS)

.endif # !target(install)

.if !defined(NO_MAN)
. if target(_manpages) # bsd.man.mk was included
realinstall: _maninstall
. endif
.endif

# Wrap bsd.nls.mk because I can't force that Makefile snippet to work only with
# ${PROGS}.
.for _P in ${PROGS}
NLSNAME.${_P}?=	${_P}
NLS:=	${NLS.${_P}}
NLSDIR:= ${NLSDIR.${_P}}
NLSNAME:= ${NLSNAME.${_P}}
.include <bsd.nls.mk>
.endfor

.include <bsd.files.mk>
.include <bsd.incs.mk>
.include <bsd.links.mk>

.if !target(lint)
. for _P in ${PROGS}
.  if !target(lint.${_P})
.   if defined(PROG_CXX.${_P})
lint.${_P}:
.   else
_CFLAGS:= ${CFLAGS.${_P}}
_SRCS:=	${SRCS.${_P}}
lint.${_P}: ${_SRCS:M*.c}
	${LINT} ${LINTFLAGS} ${_CFLAGS:M-[DIU]*} ${.ALLSRC}
.   endif
.  endif
lint: lint.${_P}

. endfor
.endif # !target(lint)

.for _P in ${PROGS}
CFLAGS:= ${CFLAGS.${_P}}
CXXFLAGS:= ${CXXFLAGS.${_P}}
# XXX: Pollutes DPADD.${_P} and LDADD.${_P} above
#DPADD:= ${DPADD.${_P}}
#LDADD:= ${LDADD.${_P}}
SRCS:=	${SRCS.${_P}}
. include <bsd.dep.mk>
# bsd.dep.mk mangles SRCS
SRCS.${_P}:=	${SRCS}
. undef DPADD LDADD
.endfor

# XXX: emulate the old bsd.prog.mk by allowing Makefiles that didn't set
# ${PROG*} to function with this Makefile snippet.
.if empty(PROGS)
. include <bsd.dep.mk>
.endif

.if !exists(${.OBJDIR}/${DEPENDFILE})
. for _P in ${PROGS}
_SRCS:=	${SRCS.${_P}}
${OBJS.${_P}}: ${_SRCS:M*.h}
. endfor
.endif

.include <bsd.obj.mk>

.include <bsd.sys.mk>

.if defined(PORTNAME)
.include <bsd.pkg.mk>
.endif
