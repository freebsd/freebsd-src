#	from: @(#)bsd.lib.mk	5.26 (Berkeley) 5/2/91
# $FreeBSD$
#

.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.endif

.if exists(${.CURDIR}/shlib_version)
SHLIB_MAJOR != . ${.CURDIR}/shlib_version ; echo $$major
.if ${OBJFORMAT} == aout
SHLIB_MINOR != . ${.CURDIR}/shlib_version ; echo $$minor
.endif
.endif

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

.if defined(DESTDIR) && !defined(BOOTSTRAPPING)
CFLAGS+= -I${DESTDIR}/usr/include
CXXINCLUDES+= -I${DESTDIR}/usr/include/g++
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

.MAIN: all

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
# .So used for PIC object files
.SUFFIXES:
.SUFFIXES: .out .o .po .So .S .s .c .cc .cpp .cxx .m .C .f .y .l

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
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
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
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
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
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
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
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.s.o:
	${CC} -x assembler-with-cpp ${CFLAGS:M-[BID]*} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.s.po:
	${CC} -x assembler-with-cpp -DPROF ${CFLAGS:M-[BID]*} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.s.So:
	${CC} -x assembler-with-cpp -fpic -DPIC ${CFLAGS:M-[BID]*} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.S.o:
	${CC} ${CFLAGS:M-[BID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.S.po:
	${CC} -DPROF ${CFLAGS:M-[BID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.S.So:
	${CC} -fpic -DPIC ${CFLAGS:M-[BID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.if !defined(INTERNALLIB) || defined(INTERNALSTATICLIB)
.if !defined(NOPROFILE) && !defined(INTERNALLIB)
_LIBS=lib${LIB}.a lib${LIB}_p.a
.else
_LIBS=lib${LIB}.a
.endif
.endif

.if defined(SHLIB_NAME)
_LIBS+=${SHLIB_NAME}
.endif
.if defined(INSTALL_PIC_ARCHIVE)
_LIBS+=lib${LIB}_pic.a
.endif

.if !defined(PICFLAG)
PICFLAG=-fpic
.endif

.if !defined(NOMAN)
all: objwarn ${_LIBS} all-man _SUBDIR # llib-l${LIB}.ln
.else
all: objwarn ${_LIBS} _SUBDIR # llib-l${LIB}.ln
.endif

OBJS+=	${SRCS:N*.h:R:S/$/.o/g}

lib${LIB}.a:: ${OBJS} ${STATICOBJS}
	@${ECHO} building static ${LIB} library
	@rm -f lib${LIB}.a
	@${AR} cq lib${LIB}.a `lorder ${OBJS} ${STATICOBJS} | tsort -q` ${ARADD}
	${RANLIB} lib${LIB}.a

POBJS+=	${OBJS:.o=.po} ${STATICOBJS:.o=.po}
.if !defined(NOPROFILE)
lib${LIB}_p.a:: ${POBJS}
	@${ECHO} building profiled ${LIB} library
	@rm -f lib${LIB}_p.a
	@${AR} cq lib${LIB}_p.a `lorder ${POBJS} | tsort -q` ${ARADD}
	${RANLIB} lib${LIB}_p.a
.endif

.if defined(DESTDIR) && !defined(BOOTSTRAPPING)
LDDESTDIRENV?=	LIBRARY_PATH=${DESTDIR}${SHLIBDIR}:${DESTDIR}${LIBDIR}
.endif

SOBJS+= ${OBJS:.o=.So}

.if defined(SHLIB_NAME)
${SHLIB_NAME}: ${SOBJS}
	@${ECHO} building shared library ${SHLIB_NAME}
	@rm -f ${SHLIB_NAME} ${SHLIB_LINK}
.if defined(SHLIB_LINK)
	@ln -sf ${SHLIB_NAME} ${SHLIB_LINK}
.endif
.if ${OBJFORMAT} == aout
	@${LDDESTDIRENV} ${CC} -shared -Wl,-x,-assert,pure-text \
	    -o ${SHLIB_NAME} \
	    `lorder ${SOBJS} | tsort -q` ${LDDESTDIR} ${LDADD}
.else
	@${LDDESTDIRENV} ${CC} -shared -Wl,-x \
	    -o ${SHLIB_NAME} -Wl,-soname,${SONAME} \
	    `lorder ${SOBJS} | tsort -q` ${LDDESTDIR} ${LDADD}
.endif
.endif

.if defined(INSTALL_PIC_ARCHIVE)
lib${LIB}_pic.a:: ${SOBJS}
	@${ECHO} building special pic ${LIB} library
	@rm -f lib${LIB}_pic.a
	@${AR} cq lib${LIB}_pic.a ${SOBJS} ${ARADD}
	${RANLIB} lib${LIB}_pic.a
.endif

llib-l${LIB}.ln: ${SRCS}
	${LINT} -C${LIB} ${CFLAGS:M-[DIU]*} ${.ALLSRC:M*.c}

.if !target(clean)
clean:	_SUBDIR
	rm -f a.out ${OBJS} ${STATICOBJS} ${OBJS:S/$/.tmp/} ${CLEANFILES}
	rm -f lib${LIB}.a # llib-l${LIB}.ln
	rm -f ${POBJS} ${POBJS:S/$/.tmp/} lib${LIB}_p.a
	rm -f ${SOBJS} ${SOBJS:.So=.so} ${SOBJS:S/$/.tmp/} \
	    ${SHLIB_NAME} ${SHLIB_LINK} \
	    lib${LIB}.so.* lib${LIB}.so lib${LIB}_pic.a
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
	    `${LDDESTDIRENV} ${CC} -shared -Wl,-f ${LDDESTDIR} ${LDADD}` \
	    >> ${DEPENDFILE}
.else
.if defined(DPADD) && !empty(DPADD)
	echo ${SHLIB_NAME}: ${DPADD} >> ${DEPENDFILE}
.endif
.endif
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall: _includeinstall
.endif

_includeinstall:
.if defined(INCS)
.for header in ${INCS}
	cd ${.CURDIR} && \
	${INSTALL} -C -o ${INCOWN} -g ${INCGRP} -m ${INCMODE} \
		${header} ${DESTDIR}${INCDIR}

.endfor
.endif

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

realinstall: beforeinstall
.if !defined(INTERNALLIB)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}.a ${DESTDIR}${LIBDIR}
.if !defined(NOPROFILE)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_p.a ${DESTDIR}${LIBDIR}
.endif
.endif
.if defined(SHLIB_NAME)
	${INSTALL} ${COPY} ${STRIP} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${_SHLINSTALLFLAGS} \
	    ${SHLIB_NAME} ${DESTDIR}${SHLIBDIR}
.if defined(SHLIB_LINK)
	ln -sf ${SHLIB_NAME} ${DESTDIR}${SHLIBDIR}/${SHLIB_LINK}
.endif
.endif
.if defined(INSTALL_PIC_ARCHIVE)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_pic.a ${DESTDIR}${LIBDIR}
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

install: afterinstall _SUBDIR
.if !defined(NOMAN)
afterinstall: realinstall maninstall
.else
afterinstall: realinstall
.endif
.endif

.if !target(regress)
regress:
.endif

DISTRIBUTION?=	bin
.if !target(distribute)
distribute:	_SUBDIR
.for dist in ${DISTRIBUTION}
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
.endif

.if !target(lint)
lint:
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

.include <bsd.dep.mk>

.if !exists(${DEPENDFILE})
${OBJS} ${STATICOBJS} ${POBJS} ${SOBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>

.include <bsd.sys.mk>
