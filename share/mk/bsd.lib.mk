#	from: @(#)bsd.lib.mk	5.26 (Berkeley) 5/2/91
#	$Id: bsd.lib.mk,v 1.46.2.8 1997/06/21 15:48:18 jkh Exp $
#

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.if exists(${.CURDIR}/shlib_version)
SHLIB_MAJOR != . ${.CURDIR}/shlib_version ; echo $$major
SHLIB_MINOR != . ${.CURDIR}/shlib_version ; echo $$minor
.endif

.if defined(DESTDIR)
CFLAGS+= -I${DESTDIR}/usr/include
CXXINCLUDES+= -I${DESTDIR}/usr/include/g++
.endif

.if defined(DEBUG_FLAGS)
CFLAGS+= ${DEBUG_FLAGS}
.endif

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

.include <bsd.libnames.mk>

.MAIN: all

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
# .so used for PIC object files
.SUFFIXES:
.SUFFIXES: .out .o .po .so .s .S .c .cc .cpp .cxx .m .C .f .y .l

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.c.po:
	${CC} -pg ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}

.c.so:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.cc.o .C.o .cpp.o .cxx.o:
	${CXX} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.cc.po .C.po .cpp.po .cxx.po:
	${CXX} -pg ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}

.cc.so .C.so .cpp.so .cxx.so:
	${CXX} ${PICFLAG} -DPIC ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.f.o:
	${FC} ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC} 
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.f.po:
	${FC} -pg ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC} 
	@${LD} -O ${.TARGET} -X -r ${.TARGET}

.f.so:
	${FC} ${PICFLAG} -DPIC ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.s.o:
	${CC} -x assembler-with-cpp ${CFLAGS:M-[BID]*} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.s.po:
	${CC} -x assembler-with-cpp -DPROF ${CFLAGS:M-[BID]*} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}

.s.so:
	${CC} -x assembler-with-cpp -fpic -DPIC ${CFLAGS:M-[BID]*} ${AINC} -c \
	    ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.S.o:
	${CC} ${CFLAGS:M-[BID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.S.po:
	${CC} -DPROF ${CFLAGS:M-[BID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.S.so:
	${CC} -fpic -DPIC ${CFLAGS:M-[BID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.m.po:
	${CC} ${CFLAGS} -fgnu-runtime -pg -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}

.m.o:
	${CC} ${CFLAGS} -fgnu-runtime -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}

.if !defined(INTERNALLIB) || defined(INTERNALSTATICLIB)
.if !defined(NOPROFILE) && !defined(INTERNALLIB)
_LIBS=lib${LIB}.a lib${LIB}_p.a
.else
_LIBS=lib${LIB}.a
.endif
.endif

.if !defined(NOPIC)
.if defined(SHLIB_MAJOR) && defined(SHLIB_MINOR)
_LIBS+=lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
.endif
.if defined(INSTALL_PIC_ARCHIVE)
_LIBS+=lib${LIB}_pic.a
.endif
.endif

.if !defined(PICFLAG)
PICFLAG=-fpic
.endif

all: objwarn ${_LIBS} all-man _SUBDIR # llib-l${LIB}.ln

OBJS+=	${SRCS:N*.h:R:S/$/.o/g}

lib${LIB}.a:: ${OBJS}
	@${ECHO} building standard ${LIB} library
	@rm -f lib${LIB}.a
	@${AR} cq lib${LIB}.a `lorder ${OBJS} | tsort -q` ${ARADD}
	${RANLIB} lib${LIB}.a

.if !defined(NOPROFILE)
POBJS+=	${OBJS:.o=.po}
lib${LIB}_p.a:: ${POBJS}
	@${ECHO} building profiled ${LIB} library
	@rm -f lib${LIB}_p.a
	@${AR} cq lib${LIB}_p.a `lorder ${POBJS} | tsort -q` ${ARADD}
	${RANLIB} lib${LIB}_p.a
.endif

.if defined(DESTDIR)
LDDESTDIRENV?=	LIBRARY_PATH=${DESTDIR}${SHLIBDIR}:${DESTDIR}/usr/lib
.endif

.if !defined(NOPIC)
SOBJS+= ${OBJS:.o=.so}
lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}: ${SOBJS}
	@${ECHO} building shared ${LIB} library \(version ${SHLIB_MAJOR}.${SHLIB_MINOR}\)
	@rm -f lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
	@${LDDESTDIRENV} ${CC} -shared -Wl,-x \
	    -o lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	    `lorder ${SOBJS} | tsort -q` ${LDDESTDIR} ${LDADD}

lib${LIB}_pic.a:: ${SOBJS}
	@${ECHO} building special pic ${LIB} library
	@rm -f lib${LIB}_pic.a
	@${AR} cq lib${LIB}_pic.a ${SOBJS} ${ARADD}
	${RANLIB} lib${LIB}_pic.a
.endif

llib-l${LIB}.ln: ${SRCS}
	${LINT} -C${LIB} ${CFLAGS} ${.ALLSRC:M*.c}

.if !target(clean)
clean:	_SUBDIR
	rm -f a.out Errs errs mklog ${CLEANFILES} ${OBJS}
	rm -f lib${LIB}.a llib-l${LIB}.ln
	rm -f ${POBJS} profiled/*.o lib${LIB}_p.a
	rm -f ${SOBJS} shared/*.o
	rm -f lib${LIB}.so.*.* lib${LIB}_pic.a
.if defined(CLEANDIRS) && !empty(CLEANDIRS)
	rm -rf ${CLEANDIRS}
.endif
.endif

.if defined(SRCS)
afterdepend:
	@(TMP=_depend$$$$; \
	sed -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.po \1.so:/' < .depend > $$TMP; \
	mv $$TMP .depend)
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

.if defined(PRECIOUSLIB)
SHLINSTALLFLAGS+= -fschg
.endif

realinstall: beforeinstall
.if !defined(INTERNALLIB)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${INSTALLFLAGS} lib${LIB}.a ${DESTDIR}${LIBDIR}
.if !defined(NOPROFILE)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${INSTALLFLAGS} lib${LIB}_p.a ${DESTDIR}${LIBDIR}
.endif
.endif
.if !defined(NOPIC)
.if defined(SHLIB_MAJOR) && defined(SHLIB_MINOR)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${INSTALLFLAGS} ${SHLINSTALLFLAGS} \
	    lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	    ${DESTDIR}${SHLIBDIR}
.endif
.if defined(INSTALL_PIC_ARCHIVE)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${INSTALLFLAGS} lib${LIB}_pic.a ${DESTDIR}${LIBDIR}
.endif
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
distribute:	_SUBDIR
.for dist in ${DISTRIBUTION}
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
.endif

.if !target(lint)
lint:
.endif

.if defined(NOTAGS)
tags:
.endif

.if !target(tags)
tags: ${SRCS} _SUBDIR
	@cd ${.CURDIR} && gtags ${GTAGSFLAGS}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS}
.endif
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.elif !target(maninstall)
maninstall:
all-man:
.endif

.include <bsd.dep.mk>
.include <bsd.obj.mk>
