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
.if ${OBJFORMAT} == elf
.if !defined(SHLIB_NAME) && defined(SHLIB_MAJOR)
SHLIB_NAME=	lib${LIB}.so.${SHLIB_MAJOR}
SHLIB_LINK?=	lib${LIB}.so
.endif
SONAME?=	${SHLIB_NAME}
.else
.if defined(SHLIB_MAJOR) && defined(SHLIB_MINOR)
SHLIB_NAME?=	lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
.endif
.endif
.endif

.if defined(DEBUG_FLAGS)
CFLAGS+= ${DEBUG_FLAGS}
.endif

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

.if ${OBJFORMAT} != aout || make(checkdpadd) || defined(NEED_LIBNAMES)
.include <bsd.libnames.mk>
.endif

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

.c.ln:
	${LINT} ${LINTOBJFLAGS} ${CFLAGS:M-[DIU]*} ${.IMPSRC} || \
	    touch ${.TARGET}

.cc.ln .C.ln .cpp.ln .cxx.ln:
	${LINT} ${LINTOBJFLAGS} ${CXXFLAGS:M-[DIU]*} ${.IMPSRC} || \
	    touch ${.TARGET}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.c.po:
	${CC} -pg ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.c.So:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} ${LDFLAGS} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.cc.o .C.o .cpp.o .cxx.o:
	${CXX} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.cc.po .C.po .cpp.po .cxx.po:
	${CXX} -pg ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.cc.So .C.So .cpp.So .cxx.So:
	${CXX} ${PICFLAG} -DPIC ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} ${LDFLAGS} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.f.o:
	${FC} ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC} 
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.f.po:
	${FC} -pg ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC} 
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.f.So:
	${FC} ${PICFLAG} -DPIC ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	@${LD} ${LDFLAGS} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.m.o:
	${OBJC} ${OBJCFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.m.po:
	${OBJC} ${OBJCFLAGS} -pg -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.m.So:
	${OBJC} ${PICFLAG} -DPIC ${OBJCFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} ${LDFLAGS} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.s.o .asm.o:
	${CC} -x assembler-with-cpp ${CFLAGS} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}
	@${LD} ${LDFLAGS} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.s.po .asm.po:
	${CC} -x assembler-with-cpp -DPROF ${CFLAGS} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}
	@${LD} ${LDFLAGS} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.s.So .asm.So:
	${CC} -x assembler-with-cpp ${PICFLAG} -DPIC ${CFLAGS} \
	    ${AINC} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.S.o:
	${CC} ${CFLAGS} ${AINC} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} ${LDFLAGS} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.S.po:
	${CC} -DPROF ${CFLAGS} ${AINC} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} ${LDFLAGS} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.S.So:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} ${AINC} -c ${.IMPSRC} \
	    -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

all: objwarn

.if defined(LIB) && !empty(LIB)
_LIBS=		lib${LIB}.a
OBJS+=		${SRCS:N*.h:R:S/$/.o/}

lib${LIB}.a: ${OBJS} ${STATICOBJS}
	@${ECHO} building static ${LIB} library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} `lorder ${OBJS} ${STATICOBJS} | tsort -q` ${ARADD}
	${RANLIB} ${.TARGET}

.if !defined(INTERNALLIB)

.if !defined(NOPROFILE)
_LIBS+=		lib${LIB}_p.a
POBJS+=		${OBJS:.o=.po} ${STATICOBJS:.o=.po}

lib${LIB}_p.a: ${POBJS}
	@${ECHO} building profiled ${LIB} library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} `lorder ${POBJS} | tsort -q` ${ARADD}
	${RANLIB} ${.TARGET}
.endif

SOBJS+=		${OBJS:.o=.So}

.if defined(SHLIB_NAME)
_LIBS+=		${SHLIB_NAME}

${SHLIB_NAME}: ${SOBJS}
	@${ECHO} building shared library ${SHLIB_NAME}
	@rm -f ${.TARGET} ${SHLIB_LINK}
.if defined(SHLIB_LINK)
	@ln -fs ${.TARGET} ${SHLIB_LINK}
.endif
.if ${OBJFORMAT} == aout
	@${CC} -shared -Wl,-x,-assert,pure-text \
	    -o ${.TARGET} \
	    `lorder ${SOBJS} | tsort -q` ${LDADD}
.else
	@${CC} ${LDFLAGS} -shared -Wl,-x \
	    -o ${.TARGET} -Wl,-soname,${SONAME} \
	    `lorder ${SOBJS} | tsort -q` ${LDADD}
.endif
.endif

.if defined(INSTALL_PIC_ARCHIVE)
_LIBS+=		lib${LIB}_pic.a

lib${LIB}_pic.a: ${SOBJS}
	@${ECHO} building special pic ${LIB} library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} ${SOBJS} ${ARADD}
	${RANLIB} ${.TARGET}
.endif

.if defined(WANT_LINT)
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

.endif defined(LIB) && !empty(LIB)

.if !defined(NOMAN)
all: _manpages
.endif

.if !target(clean)
clean:
	rm -f ${CLEANFILES}
.if defined(LIB) && !empty(LIB)
	rm -f a.out ${OBJS} ${OBJS:S/$/.tmp/} ${STATICOBJS}
.if !defined(INTERNALLIB)
.if !defined(NOPROFILE)
	rm -f ${POBJS} ${POBJS:S/$/.tmp/}
.endif
	rm -f ${SOBJS} ${SOBJS:.So=.so} ${SOBJS:S/$/.tmp/}
.if defined(SHLIB_NAME)
	rm -f ${SHLIB_LINK} lib${LIB}.so.* lib${LIB}.so
.endif
.if defined(WANT_LINT)
	rm -f ${LINTOBJS}
.endif
.endif !defined(INTERNALLIB)
	rm -f ${_LIBS}
.endif defined(LIB) && !empty(LIB)
.if defined(CLEANDIRS) && !empty(CLEANDIRS)
	rm -rf ${CLEANDIRS}
.endif
.endif

_EXTRADEPEND:
	@TMP=_depend$$$$; \
	sed -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.po \1.So:/' < ${DEPENDFILE} \
	    > $$TMP; \
	mv $$TMP ${DEPENDFILE}
.if !defined(NOEXTRADEPEND) && defined(SHLIB_NAME)
.if ${OBJFORMAT} == aout
	echo ${SHLIB_NAME}: \
	    `${CC} -shared -Wl,-f ${LDADD}` \
	    >> ${DEPENDFILE}
.else
.if defined(DPADD) && !empty(DPADD)
	echo ${SHLIB_NAME}: ${DPADD} >> ${DEPENDFILE}
.endif
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

.if defined(LIB) && !empty(LIB) && !defined(INTERNALLIB)
realinstall: _libinstall
_libinstall:
.if !defined(NOINSTALLLIB)
	${INSTALL} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}.a ${DESTDIR}${LIBDIR}
.endif
.if !defined(NOPROFILE)
	${INSTALL} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_p.a ${DESTDIR}${LIBDIR}
.endif
.if defined(SHLIB_NAME)
	${INSTALL} ${COPY} ${STRIP} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${_SHLINSTALLFLAGS} \
	    ${SHLIB_NAME} ${DESTDIR}${SHLIBDIR}
.if defined(SHLIB_LINK)
	ln -fs ${SHLIB_NAME} ${DESTDIR}${SHLIBDIR}/${SHLIB_LINK}
.endif
.endif
.if defined(INSTALL_PIC_ARCHIVE)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_pic.a ${DESTDIR}${LIBDIR}
.endif
.if defined(WANT_LINT)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${LINTLIB} ${DESTDIR}${LINTLIBDIR}
.endif
.endif defined(LIB) && !empty(LIB) && !defined(INTERNALLIB)

realinstall:
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

.include <bsd.files.mk>
.include <bsd.incs.mk>

.if !defined(NOMAN)
realinstall: _maninstall
.endif

.endif

.if !target(lint)
lint: ${SRCS:M*.c}
	${LINT} ${LINTOBJFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC}
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif

.include <bsd.dep.mk>

.if defined(LIB) && !empty(LIB)
.if !exists(${.OBJDIR}/${DEPENDFILE})
${OBJS} ${STATICOBJS} ${POBJS} ${SOBJS}: ${SRCS:M*.h}
.endif
.endif

.include <bsd.obj.mk>

.include <bsd.sys.mk>
