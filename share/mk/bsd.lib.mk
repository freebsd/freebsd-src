#	from: @(#)bsd.lib.mk	5.26 (Berkeley) 5/2/91
# $FreeBSD$
#

.include <bsd.init.mk>

# Set up the variables controlling shared libraries.  After this section,
# SHLIB_NAME will be defined only if we are to create a shared library.
# SHLIB_LINK will be defined only if we are to create a link to it.
# INSTALL_PIC_ARCHIVE will be defined only if we are to create a PIC archive.
.if defined(NOPIC)
.undef SHLIB_NAME
.undef INSTALL_PIC_ARCHIVE
.else
.if !defined(SHLIB_NAME) && defined(LIB) && defined(SHLIB_MAJOR)
SHLIB_NAME=	lib${LIB}.so.${SHLIB_MAJOR}
.endif
.if defined(SHLIB_NAME) && ${SHLIB_NAME:M*.so.*}
SHLIB_LINK?=	${SHLIB_NAME:R}
.endif
SONAME?=	${SHLIB_NAME}
.endif

CFLAGS+=	${CRUNCH_CFLAGS}

.if defined(DEBUG_FLAGS)
CFLAGS+= ${DEBUG_FLAGS}
.endif

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

.include <bsd.libnames.mk>

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
# .So used for PIC object files
.SUFFIXES:
.SUFFIXES: .out .o .po .So .S .s .asm .c .cc .cpp .cxx .m .C .f .y .l .ln

.if !defined(PICFLAG)
.if ${MACHINE_ARCH} == "sparc64"
PICFLAG=-fPIC
.else
PICFLAG=-fpic
.endif
.endif

.c.po:
	${CC} -pg ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.c.So:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.po .C.po .cpp.po .cxx.po:
	${CXX} -pg ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.So .C.So .cpp.So .cxx.So:
	${CXX} ${PICFLAG} -DPIC ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.f.po:
	${FC} -pg ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC} 

.f.So:
	${FC} ${PICFLAG} -DPIC ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}

.m.po:
	${OBJC} ${OBJCFLAGS} -pg -c ${.IMPSRC} -o ${.TARGET}

.m.So:
	${OBJC} ${PICFLAG} -DPIC ${OBJCFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.s.o .asm.o:
	${CC} -x assembler-with-cpp ${CFLAGS} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}

.s.po .asm.po:
	${CC} -x assembler-with-cpp -DPROF ${CFLAGS} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}

.s.So .asm.So:
	${CC} -x assembler-with-cpp ${PICFLAG} -DPIC ${CFLAGS} \
	    ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.S.po:
	${CC} -DPROF ${CFLAGS} ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.S.So:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} ${AINC} -c ${.IMPSRC} \
	    -o ${.TARGET}

all: objwarn

.if defined(LIB) && !empty(LIB) || defined(SHLIB_NAME)
OBJS+=		${SRCS:N*.h:R:S/$/.o/}
.endif

.if defined(LIB) && !empty(LIB)
_LIBS=		lib${LIB}.a

lib${LIB}.a: ${OBJS} ${STATICOBJS}
	@${ECHO} building static ${LIB} library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} `lorder ${OBJS} ${STATICOBJS} | tsort -q` ${ARADD}
	${RANLIB} ${.TARGET}
.endif

.if !defined(INTERNALLIB)

.if !defined(NOPROFILE) && defined(LIB) && !empty(LIB)
_LIBS+=		lib${LIB}_p.a
POBJS+=		${OBJS:.o=.po} ${STATICOBJS:.o=.po}

lib${LIB}_p.a: ${POBJS}
	@${ECHO} building profiled ${LIB} library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} `lorder ${POBJS} | tsort -q` ${ARADD}
	${RANLIB} ${.TARGET}
.endif

.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
SOBJS+=		${OBJS:.o=.So}
.endif

.if defined(SHLIB_NAME)
_LIBS+=		${SHLIB_NAME}

${SHLIB_NAME}: ${SOBJS}
	@${ECHO} building shared library ${SHLIB_NAME}
	@rm -f ${.TARGET} ${SHLIB_LINK}
.if defined(SHLIB_LINK)
	@ln -fs ${.TARGET} ${SHLIB_LINK}
.endif
	@${CC} ${LDFLAGS} -shared -Wl,-x \
	    -o ${.TARGET} -Wl,-soname,${SONAME} \
	    `lorder ${SOBJS} | tsort -q` ${LDADD}
.endif

.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
_LIBS+=		lib${LIB}_pic.a

lib${LIB}_pic.a: ${SOBJS}
	@${ECHO} building special pic ${LIB} library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} ${SOBJS} ${ARADD}
	${RANLIB} ${.TARGET}
.endif

.if defined(WANT_LINT) && !defined(NOLINT) && defined(LIB) && !empty(LIB)
LINTLIB=	llib-l${LIB}.ln
_LIBS+=		${LINTLIB}
LINTOBJS+=	${SRCS:M*.c:.c=.ln}

${LINTLIB}: ${LINTOBJS}
	@${ECHO} building lint library ${.TARGET}
	@rm -f ${.TARGET}
	${LINT} ${LINTLIBFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC}
.endif

.endif !defined(INTERNALLIB)

all: ${_LIBS}

.if !defined(NOMAN)
all: _manpages
.endif

_EXTRADEPEND:
	@TMP=_depend$$$$; \
	sed -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.po \1.So:/' < ${DEPENDFILE} \
	    > $$TMP; \
	mv $$TMP ${DEPENDFILE}
.if !defined(NOEXTRADEPEND) && defined(SHLIB_NAME)
.if defined(DPADD) && !empty(DPADD)
	echo ${SHLIB_NAME}: ${DPADD} >> ${DEPENDFILE}
.endif
.endif

.if !target(install)

.if defined(PRECIOUSLIB) && !defined(NOFSCHG)
SHLINSTALLFLAGS+= -fschg
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
.if defined(LIB) && !empty(LIB) && !defined(NOINSTALLLIB)
	${INSTALL} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}.a ${DESTDIR}${LIBDIR}
.endif
.if !defined(NOPROFILE) && defined(LIB) && !empty(LIB)
	${INSTALL} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_p.a ${DESTDIR}${LIBDIR}
.endif
.if defined(SHLIB_NAME)
	${INSTALL} ${STRIP} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${_SHLINSTALLFLAGS} \
	    ${SHLIB_NAME} ${DESTDIR}${SHLIBDIR}
.if defined(SHLIB_LINK)
	ln -fs ${SHLIB_NAME} ${DESTDIR}${SHLIBDIR}/${SHLIB_LINK}
.endif
.endif
.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_pic.a ${DESTDIR}${LIBDIR}
.endif
.if defined(WANT_LINT) && !defined(NOLINT) && defined(LIB) && !empty(LIB)
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${LINTLIB} ${DESTDIR}${LINTLIBDIR}
.endif
.endif !defined(INTERNALLIB)

.include <bsd.files.mk>
.include <bsd.incs.mk>
.include <bsd.links.mk>

.if !defined(NOMAN)
realinstall: _maninstall
.ORDER: beforeinstall _maninstall
.endif

.endif

.if !target(lint)
lint: ${SRCS:M*.c}
	${LINT} ${LINTFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC}
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif

.include <bsd.dep.mk>

.if !exists(${.OBJDIR}/${DEPENDFILE})
.if defined(LIB) && !empty(LIB)
${OBJS} ${STATICOBJS} ${POBJS}: ${SRCS:M*.h}
.endif
.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
${SOBJS}: ${SRCS:M*.h}
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
.if !defined(NOPROFILE) && defined(LIB) && !empty(LIB)
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
.endif !defined(INTERNALLIB)
.if defined(_LIBS) && !empty(_LIBS)
	rm -f ${_LIBS}
.endif
.if defined(CLEANDIRS) && !empty(CLEANDIRS)
	rm -rf ${CLEANDIRS}
.endif
.endif

.include <bsd.obj.mk>

.include <bsd.sys.mk>
