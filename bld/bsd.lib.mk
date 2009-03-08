#	from: @(#)bsd.lib.mk	5.26 (Berkeley) 5/2/91
# $FreeBSD: src/share/mk/bsd.lib.mk,v 1.185 2008/05/22 01:14:43 jb Exp $
#
CFLAGS += -DJBUILD
.if defined(HOSTPROG)
CFLAGS+=	-DHOSTPROG
CC=		${HOST_CC}
CXX=		${HOST_CXX}
LORDER=		lorder
RANLIB=		ranlib
AR=		ar
NO_STRIP=	yes
.else
# If we are linking a lib - it must be a shared lib
# and we want to use the PIC version of libgcc.
#.if defined(LIB) && ${LIB} != "gcc"
#LDADD_LAST+=	-lgcc_pic
#.endif
_CFLAGS:=	${CFLAGS}
CFLAGS=		${CFLAGS_BSD} ${_CFLAGS}
AFLAGS+=	${AFLAGS.${MACHINE}}
CFLAGS+=	${CFLAGS.${MACHINE}}
LDADD+=		-B${STAGEDIR}/lib
LDADD+=		-B${STAGEDIR}/usr/lib
LDADD+=		-L${STAGEDIR}/lib
LDADD+=		-L${STAGEDIR}/usr/lib
.if !empty(SHAREDSTAGEDIR)
CFLAGS+=	-I${SHAREDSTAGEDIR}/usr/include
LDADD+=		-B${SHAREDSTAGEDIR}/lib
LDADD+=		-B${SHAREDSTAGEDIR}/usr/lib
LDADD+=		-L${SHAREDSTAGEDIR}/lib
LDADD+=		-L${SHAREDSTAGEDIR}/usr/lib
.endif
.endif
CFLAGS+=	-I${STAGEDIR}/usr/include
CFLAGS+=	-I${COMMONSTAGEDIR}/usr/include
CFLAGS+=	-I${COMMONSHAREDSTAGEDIR}/usr/include
CXXFLAGS+=	-I${STAGEDIR}/usr/include/c++/4.2
CXXFLAGS+=	-I${COMMONSTAGEDIR}/usr/include/c++/4.2
.if !empty(SHAREDSTAGEDIR)
CFLAGS+=	-I${SHAREDSTAGEDIR}/usr/include
CFLAGS+=	-I${COMMONSHAREDSTAGEDIR}/usr/include
CXXFLAGS+=	-I${SHAREDSTAGEDIR}/usr/include/c++/4.2
CXXFLAGS+=	-I${COMMONSHAREDSTAGEDIR}/usr/include/c++/4.2
.endif

# make it easy to add per target flags
CFLAGS+= ${XCFLAGS_${.TARGET:T:R}}

NOT_MACHINE_ARCH+= common

.if defined(NOT_MACHINE_ARCH) && !empty(NOT_MACHINE_ARCH:M${MACHINE_ARCH})
DONT_DO_IT=
.endif

.if defined(NOT_MACHINE) && !empty(NOT_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if defined(ONLY_MACHINE) && empty(ONLY_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if defined(HOSTPROG) && ${MACHINE} != "host"
DONT_DO_IT=
.endif

.if defined(DONT_DO_IT)

.if ${__MKLVL__} != 1
all:	.PHONY
.endif

.else

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

.if defined(DEBUG_FLAGS)
CFLAGS+= ${DEBUG_FLAGS}

.endif

.if empty(STRIP)
NO_STRIP = yes
.endif

.if !defined(DEBUG_FLAGS) || empty(DEBUG_FLAGS)
STRIPFLAG?=	-s
.endif

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
# .So used for PIC object files
.SUFFIXES:
.SUFFIXES: .out .o .po .So .S .asm .s .c .cc .cpp .cxx .m .C .f .y .l .ln

.if !defined(PICFLAG)
.if ${MACHINE_ARCH} == "sparc64"
PICFLAG=-fPIC
.else
PICFLAG=-fpic
.endif
.endif

.if ${CC} == "icc"
PO_FLAG=-p
.else
PO_FLAG=-pg
.endif

PROFILE_FLAGS+= ${PO_FLAG}

.c.po:
	${CC} ${PROFILE_FLAGS} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.c.So:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.o .C.o .cpp.o .cxx.o:
	${CXX} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.po .C.po .cpp.po .cxx.po:
	${CXX} ${PO_FLAG} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.So .C.So .cpp.So .cxx.So:
	${CXX} ${PICFLAG} -DPIC ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.f.o:
	${FC} ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC} 

.f.po:
	${FC} -pg ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC} 

.f.So:
	${FC} ${PICFLAG} -DPIC ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}

.m.o:
	${OBJC} ${OBJCFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.m.po:
	${OBJC} ${OBJCFLAGS} -pg -c ${.IMPSRC} -o ${.TARGET}

.m.So:
	${OBJC} ${PICFLAG} -DPIC ${OBJCFLAGS} -c ${.IMPSRC} -o ${.TARGET}

#6.1# now uses ${AS} ${AFLAGS} for .s.o etc
.asm.o .s.o:
	${CC} -x assembler-with-cpp ${CFLAGS:M-[BID]*} \
		${AFLAGS} ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.asm.po .s.po:
	${CC} -x assembler-with-cpp -DPROF ${CFLAGS:M-[BID]*} \
		${AFLAGS} ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.asm.So .s.So:
	${CC} -x assembler-with-cpp -fpic -DPIC \
		${CFLAGS:M-[BIDWm]*} ${AFLAGS} ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.S.o:
	${CC} ${CFLAGS:M-[BIDWm]*} ${AFLAGS} ${AINC} -c ${.IMPSRC} \
		-o ${.TARGET}

.S.po:
	${CC} -DPROF ${CFLAGS:M-[BID]*} ${AFLAGS} ${AINC} -c \
		${.IMPSRC} -o ${.TARGET}

.S.So:
	${CC} -fpic -DPIC ${CFLAGS:M-[BID]*} ${AFLAGS} ${AINC} \
		-c ${.IMPSRC} -o ${.TARGET}

.if defined(MKPROFILE_${MACHINE_ARCH}) && ${MKPROFILE_${MACHINE_ARCH}} == "yes"
NO_PROFILE=no
.endif

.include <bsd.symver.mk>

# Allow libraries to specify their own version map or have it
# automatically generated (see bsd.symver.mk above).
.if !define(NO_SYMVER} && !empty(VERSION_MAP)
${SHLIB_NAME}:	${VERSION_MAP}
LDFLAGS+=	-Wl,--version-script=${VERSION_MAP}
.endif

.if defined(LIB) && !empty(LIB) || defined(SHLIB_NAME)
.if exists(${.CURDIR}/${LIB}.errmsg)
.PATH: ${COMMONSTAGEDIR}/usr/include
.PATH: ${COMMONSHAREDSTAGEDIR}/usr/include

SRCS += ${LIB}_errmsg.c
.endif

OBJS+=		${SRCS:N*.h:R:T:S/$/.o/}

.for _F in ${OBJS} ${STATICOBJS}
.ORDER: dirdep genfiles ${_F}
.endfor
.endif

.if defined(LIB) && !empty(LIB)
_LIBS=		lib${LIB}.a

lib${LIB}.a: ${OBJS} ${STATICOBJS}
	rm -f ${.TARGET}
	${AR} cq ${.TARGET} `NM=${NM} ${LORDER} ${OBJS} ${STATICOBJS} | tsort -q` ${ARADD}; \
	${RANLIB} ${.TARGET}
.endif

.if !defined(INTERNALLIB)
.if !defined(NO_PROFILE) || ${NO_PROFILE} == "no"
MKPROFILE?= yes
.else
MKPROFILE?= no
.endif

.if ${MKPROFILE} != no && defined(LIB) && !empty(OBJS)
POBJS+=		${OBJS:.o=.po} ${STATICOBJS:.o=.po}
.if !empty(POBJS)
_LIBS+=		lib${LIB}_p.a

.for _F in ${POBJS}
.ORDER: dirdep genfiles ${_F}
.endfor

lib${LIB}_p.a: ${POBJS}
	rm -f ${.TARGET}
	${AR} cq ${.TARGET} `NM=${NM} ${LORDER} ${POBJS} | tsort -q` ${ARADD}; \
	${RANLIB} ${.TARGET}
.endif
.endif

.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
SOBJS+=		${OBJS:.o=.So}

.for _F in ${SOBJS}
.ORDER: dirdep genfiles ${_F}
.endfor
.endif

.if defined(SHLIB_NAME)
_LIBS+=		${SHLIB_NAME}

${SHLIB_NAME}: ${SOBJS}
	rm -f ${.TARGET} ${SHLIB_LINK}
.if defined(SHLIB_LINK)
	ln -fs ${.TARGET} ${SHLIB_LINK}
.endif
	${CC} ${LDFLAGS} -shared -Wl,-X ${SHLIB_LDFLAGS.${MACHINE}} \
	    -o ${.TARGET} -Wl,-soname,${SONAME} \
	    `NM=${NM} ${LORDER} ${SOBJS} | tsort -q` ${LDADD} ${LDADD_LAST}
.endif

.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
_LIBS+=		lib${LIB}_pic.a

lib${LIB}_pic.a: ${SOBJS}
	rm -f ${.TARGET}
	${AR} cq ${.TARGET} ${SOBJS} ${ARADD}; \
	${RANLIB} ${.TARGET}
.endif

.endif # !defined(INTERNALLIB)

.if ${__MKLVL__} != 1
all: genfiles allincs buildobjs buildlibs installlibs relfiles
.ORDER: genfiles allincs buildobjs buildlibs installlibs relfiles
.endif

buildlibs: buildobjs ${_LIBS}

buildobjs: ${OBJS} ${STATICOBJS}

.if defined(_LIBS) && !defined(LIBDIR)
.error "You must define LIBDIR!"
.endif

installlibs: ${_LIBS:S,^,${LIBDIR}/,}

.for _l in ${_LIBS}
${LIBDIR}/${_l}:	${_l}
	cp ${.ALLSRC} ${.TARGET}
	echo "# ${.SRCREL}" > ${.TARGET}.srcrel
.if !defined(SHLIB_LINK) || ${SHLIB_LINK} != "no"
	if [ x${_l:M*.so.*} != x ]; then \
		ln -sf ${_l}  ${LIBDIR}/${_l:R}; \
		echo "# ${.SRCREL}" > ${LIBDIR}/${_l:R}.srcrel; \
	fi
.endif
.endfor

#all: _manpages

.endif

.include <bsd.dirdep.mk>
.include <bsd.files.mk>
.include <bsd.incs.mk>
.include <bsd.genfiles.mk>
.include <bsd.relfiles.mk>
