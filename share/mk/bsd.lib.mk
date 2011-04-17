#	from: @(#)bsd.lib.mk	5.26 (Berkeley) 5/2/91
# $FreeBSD$
#

.include <bsd.init.mk>

# Set up the variables controlling shared libraries.  After this section,
# SHLIB_NAME will be defined only if we are to create a shared library.
# SHLIB_LINK will be defined only if we are to create a link to it.
# INSTALL_PIC_ARCHIVE will be defined only if we are to create a PIC archive.
.if defined(NO_PIC)
.undef SHLIB_NAME
.undef INSTALL_PIC_ARCHIVE
.else
.if !defined(SHLIB) && defined(LIB)
SHLIB=		${LIB}
.endif
.if !defined(SHLIB_NAME) && defined(SHLIB) && defined(SHLIB_MAJOR)
SHLIB_NAME=	lib${SHLIB}.so.${SHLIB_MAJOR}
.endif
.if defined(SHLIB_NAME) && !empty(SHLIB_NAME:M*.so.*)
SHLIB_LINK?=	${SHLIB_NAME:R}
.endif
SONAME?=	${SHLIB_NAME}
.endif

.if defined(CRUNCH_CFLAGS)
CFLAGS+=	${CRUNCH_CFLAGS}
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
CFLAGS+= ${DEBUG_FLAGS}

.if !defined(NO_CTF) && (${DEBUG_FLAGS:M-g} != "")
CTFFLAGS+= -g
.endif
.endif

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

.include <bsd.libnames.mk>

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
# .So used for PIC object files
.SUFFIXES:
.SUFFIXES: .out .o .po .So .S .asm .s .c .cc .cpp .cxx .C .f .y .l .ln

.if !defined(PICFLAG)
.if ${MACHINE_CPUARCH} == "sparc64"
PICFLAG=-fPIC
.else
PICFLAG=-fpic
.endif
.endif

.if ${CC:T:Micc} == "icc"
PO_FLAG=-p
.else
PO_FLAG=-pg
.endif

.c.po:
	${CC} ${PO_FLAG} ${PO_CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

.c.So:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

.cc.po .C.po .cpp.po .cxx.po:
	${CXX} ${PO_FLAG} ${PO_CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.So .C.So .cpp.So .cxx.So:
	${CXX} ${PICFLAG} -DPIC ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.f.po:
	${FC} -pg ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

.f.So:
	${FC} ${PICFLAG} -DPIC ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

.s.po .s.So:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

.asm.po:
	${CC} -x assembler-with-cpp -DPROF ${PO_CFLAGS} ${ACFLAGS} \
		-c ${.IMPSRC} -o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

.asm.So:
	${CC} -x assembler-with-cpp ${PICFLAG} -DPIC ${CFLAGS} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

.S.po:
	${CC} -DPROF ${PO_CFLAGS} ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

.S.So:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFCONVERT} ${CTFFLAGS} ${.TARGET} && \
		${CTFCONVERT} ${CTFFLAGS} ${.TARGET})

all: objwarn

.include <bsd.symver.mk>

# Allow libraries to specify their own version map or have it
# automatically generated (see bsd.symver.mk above).
.if ${MK_SYMVER} == "yes" && !empty(VERSION_MAP)
${SHLIB_NAME}:	${VERSION_MAP}
LDFLAGS+=	-Wl,--version-script=${VERSION_MAP}
.endif

.if defined(LIB) && !empty(LIB) || defined(SHLIB_NAME)
OBJS+=		${SRCS:N*.h:R:S/$/.o/}
.endif

.if defined(LIB) && !empty(LIB)
_LIBS=		lib${LIB}.a

lib${LIB}.a: ${OBJS} ${STATICOBJS}
	@${ECHO} building static ${LIB} library
	@rm -f ${.TARGET}
.if !defined(NM)
	@${AR} cq ${.TARGET} `lorder ${OBJS} ${STATICOBJS} | tsort -q` ${ARADD}
.else
	@${AR} cq ${.TARGET} `NM='${NM}' lorder ${OBJS} ${STATICOBJS} | tsort -q` ${ARADD}
.endif
	${RANLIB} ${.TARGET}
.endif

.if !defined(INTERNALLIB)

.if ${MK_PROFILE} != "no" && defined(LIB) && !empty(LIB)
_LIBS+=		lib${LIB}_p.a
POBJS+=		${OBJS:.o=.po} ${STATICOBJS:.o=.po}

lib${LIB}_p.a: ${POBJS}
	@${ECHO} building profiled ${LIB} library
	@rm -f ${.TARGET}
.if !defined(NM)
	@${AR} cq ${.TARGET} `lorder ${POBJS} | tsort -q` ${ARADD}
.else
	@${AR} cq ${.TARGET} `NM='${NM}' lorder ${POBJS} | tsort -q` ${ARADD}
.endif
	${RANLIB} ${.TARGET}
.endif

.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
SOBJS+=		${OBJS:.o=.So}
.endif

.if defined(SHLIB_NAME)
_LIBS+=		${SHLIB_NAME}

.if target(beforelinking)
${SHLIB_NAME}: ${SOBJS} beforelinking
.else
${SHLIB_NAME}: ${SOBJS}
.endif
	@${ECHO} building shared library ${SHLIB_NAME}
	@rm -f ${.TARGET} ${SHLIB_LINK}
.if defined(SHLIB_LINK)
	@ln -fs ${.TARGET} ${SHLIB_LINK}
.endif
.if !defined(NM)
	@${CC} ${LDFLAGS} ${SSP_CFLAGS} -shared -Wl,-x \
	    -o ${.TARGET} -Wl,-soname,${SONAME} \
	    `lorder ${SOBJS} | tsort -q` ${LDADD}
.else
	@${CC} ${LDFLAGS} ${SSP_CFLAGS} -shared -Wl,-x \
	    -o ${.TARGET} -Wl,-soname,${SONAME} \
	    `NM='${NM}' lorder ${SOBJS} | tsort -q` ${LDADD}
.endif
	@[ -z "${CTFMERGE}" -o -n "${NO_CTF}" ] || \
		(${ECHO} ${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${SOBJS} && \
		${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${SOBJS})
.endif

.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB) && ${MK_TOOLCHAIN} != "no"
_LIBS+=		lib${LIB}_pic.a

lib${LIB}_pic.a: ${SOBJS}
	@${ECHO} building special pic ${LIB} library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} ${SOBJS} ${ARADD}
	${RANLIB} ${.TARGET}
.endif

.if defined(WANT_LINT) && !defined(NO_LINT) && defined(LIB) && !empty(LIB)
LINTLIB=	llib-l${LIB}.ln
_LIBS+=		${LINTLIB}
LINTOBJS+=	${SRCS:M*.c:.c=.ln}

${LINTLIB}: ${LINTOBJS}
	@${ECHO} building lint library ${.TARGET}
	@rm -f ${.TARGET}
	${LINT} ${LINTLIBFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC}
.endif

.endif # !defined(INTERNALLIB)

all: ${_LIBS}

.if ${MK_MAN} != "no"
all: _manpages
.endif

_EXTRADEPEND:
	@TMP=_depend$$$$; \
	sed -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.po \1.So:/' < ${DEPENDFILE} \
	    > $$TMP; \
	mv $$TMP ${DEPENDFILE}
.if !defined(NO_EXTRADEPEND) && defined(SHLIB_NAME)
.if defined(DPADD) && !empty(DPADD)
	echo ${SHLIB_NAME}: ${DPADD} >> ${DEPENDFILE}
.endif
.endif

.if !target(install)

.if defined(PRECIOUSLIB)
.if !defined(NO_FSCHG)
SHLINSTALLFLAGS+= -fschg
.endif
SHLINSTALLFLAGS+= -S
.endif

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor
_SHLINSTALLFLAGS:=	${SHLINSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_SHLINSTALLFLAGS:=	${_SHLINSTALLFLAGS${ie}}
.endfor

.if !defined(INTERNALLIB)
realinstall: _libinstall
.ORDER: beforeinstall _libinstall
_libinstall:
.if defined(LIB) && !empty(LIB) && ${MK_INSTALLLIB} != "no"
	${INSTALL} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}.a ${DESTDIR}${LIBDIR}
.endif
.if ${MK_PROFILE} != "no" && defined(LIB) && !empty(LIB)
	${INSTALL} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_p.a ${DESTDIR}${LIBDIR}
.endif
.if defined(SHLIB_NAME)
	${INSTALL} ${STRIP} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${_SHLINSTALLFLAGS} \
	    ${SHLIB_NAME} ${DESTDIR}${SHLIBDIR}
.if defined(SHLIB_LINK)
.if ${SHLIBDIR} == ${LIBDIR}
	ln -fs ${SHLIB_NAME} ${DESTDIR}${LIBDIR}/${SHLIB_LINK}
.else
	ln -fs ${_SHLIBDIRPREFIX}${SHLIBDIR}/${SHLIB_NAME} \
	    ${DESTDIR}${LIBDIR}/${SHLIB_LINK}
.if exists(${DESTDIR}${LIBDIR}/${SHLIB_NAME})
	-chflags noschg ${DESTDIR}${LIBDIR}/${SHLIB_NAME}
	rm -f ${DESTDIR}${LIBDIR}/${SHLIB_NAME}
.endif
.endif
.endif
.endif
.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB) && ${MK_TOOLCHAIN} != "no"
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_pic.a ${DESTDIR}${LIBDIR}
.endif
.if defined(WANT_LINT) && !defined(NO_LINT) && defined(LIB) && !empty(LIB)
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${LINTLIB} ${DESTDIR}${LINTLIBDIR}
.endif
.endif # !defined(INTERNALLIB)

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
	${LINT} ${LINTFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC}
.endif

.if ${MK_MAN} != "no"
.include <bsd.man.mk>
.endif

.include <bsd.dep.mk>

.if !exists(${.OBJDIR}/${DEPENDFILE})
.if defined(LIB) && !empty(LIB)
${OBJS} ${STATICOBJS} ${POBJS}: ${SRCS:M*.h}
.for _S in ${SRCS:N*.[hly]}
${_S:R}.po: ${_S}
.endfor
.endif
.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
${SOBJS}: ${SRCS:M*.h}
.for _S in ${SRCS:N*.[hly]}
${_S:R}.So: ${_S}
.endfor
.endif
.endif

.if !target(clean)
clean:
.if defined(CLEANFILES) && !empty(CLEANFILES)
	rm -f ${CLEANFILES}
.endif
.if defined(LIB) && !empty(LIB)
	rm -f a.out ${OBJS} ${OBJS:S/$/.tmp/} ${STATICOBJS}
.endif
.if !defined(INTERNALLIB)
.if ${MK_PROFILE} != "no" && defined(LIB) && !empty(LIB)
	rm -f ${POBJS} ${POBJS:S/$/.tmp/}
.endif
.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
	rm -f ${SOBJS} ${SOBJS:.So=.so} ${SOBJS:S/$/.tmp/}
.endif
.if defined(SHLIB_NAME)
.if defined(SHLIB_LINK)
	rm -f ${SHLIB_LINK}
.endif
.if defined(LIB) && !empty(LIB)
	rm -f lib${LIB}.so.* lib${LIB}.so
.endif
.endif
.if defined(WANT_LINT) && defined(LIB) && !empty(LIB)
	rm -f ${LINTOBJS}
.endif
.endif # !defined(INTERNALLIB)
.if defined(_LIBS) && !empty(_LIBS)
	rm -f ${_LIBS}
.endif
.if defined(CLEANDIRS) && !empty(CLEANDIRS)
	rm -rf ${CLEANDIRS}
.endif
.if !empty(VERSION_DEF) && !empty(SYMBOL_MAPS)
	rm -f ${VERSION_MAP}
.endif
.endif

.include <bsd.obj.mk>

.include <bsd.sys.mk>
