
.if defined(HOSTPROG) && ${MACHINE} != ${HOST_MACHINE}

# This is a host program and we're not building the host so all we want to
# do is update our dependencies which will include the host program.
.if ${__MKLVL__} != 1
all : .PHONY
.endif

.include <bsd.dirdep.mk>

.else

.if defined(HOSTPROG)
CFLAGS+=	-DHOSTPROG -DJBUILD
CC=		${HOST_CC}
LD=		${HOST_LD}
STRIP=		strip
LDEND=
LDSTART=
NO_STRIP=	yes
.else
_CFLAGS:=	${CFLAGS}
CFLAGS=		${CFLAGS_BSD} ${_CFLAGS}
CFLAGS+=	${CFLAGS.${MACHINE}}
.endif
CFLAGS+=	-I${STAGEDIR}/usr/include
.if defined(PROG_CXX)
CFLAGS+=	-I${STAGEDIR}/usr/include/c++/4.2
CFLAGS+=	-I${COMMONSTAGEDIR}/usr/include/c++/4.2
.endif
LDFLAGS+=	-B${STAGEDIR}/lib
LDFLAGS+=	-B${STAGEDIR}/usr/lib
LDFLAGS+=	-L${STAGEDIR}/lib
LDFLAGS+=	-L${STAGEDIR}/usr/lib
LDFLAGS+=	-Wl,-rpath-link=${STAGEDIR}/lib:${STAGEDIR}/usr/lib
.if !empty(SHAREDSTAGEDIR)
CFLAGS+=	-I${SHAREDSTAGEDIR}/usr/include
.if defined(PROG_CXX)
CFLAGS+=	-I${SHAREDSTAGEDIR}/usr/include/c++/4.2
CFLAGS+=	-I${COMMONSHAREDSTAGEDIR}/usr/include/c++/4.2
.endif
LDFLAGS+=	-B${SHAREDSTAGEDIR}/lib
LDFLAGS+=	-B${SHAREDSTAGEDIR}/usr/lib
LDFLAGS+=	-L${SHAREDSTAGEDIR}/lib
LDFLAGS+=	-L${SHAREDSTAGEDIR}/usr/lib
LDFLAGS+=	-Wl,-rpath-link=${SHAREDSTAGEDIR}/lib:${SHAREDSTAGEDIR}/usr/lib
.endif
CFLAGS+=	-I${COMMONSTAGEDIR}/usr/include
CFLAGS+=	-I${COMMONSHAREDSTAGEDIR}/usr/include

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

.if ${__MKLVL__} != 1
.if defined(DONT_DO_IT)

all:	.PHONY

.else
# make it easy to add per target flags
CFLAGS+= ${XCFLAGS_${.TARGET:T:R}}

.SUFFIXES: .out .o .c .cc .cpp .cxx .C .m .y .l .ln .s .S .asm

# XXX The use of COPTS in modern makefiles is discouraged.
.if defined(COPTS)
CFLAGS+=${COPTS}
.endif

.if defined(DEBUG_FLAGS)
CFLAGS+=${DEBUG_FLAGS}

.if !defined(NO_CTF) && (${DEBUG_FLAGS:M-g} != "")
CTFFLAGS+= -g
.endif
.endif

.if defined(CRUNCH_CFLAGS)
CFLAGS+=${CRUNCH_CFLAGS}
.endif

.if empty(STRIP)
NO_STRIP = yes
.endif

.if !defined(DEBUG_FLAGS) || empty(DEBUG_FLAGS)
STRIPFLAG?=	-s
.endif

.if defined(NOSHARED)
NO_SHARED= yes
.endif

.if defined(NO_SHARED) && (${NO_SHARED} != "no" && ${NO_SHARED} != "NO")
LDFLAGS+= -static
CFLAGS+= -DNO_SHARED
.if ${MACHINE} == "xlr" && defined(BTLB_BINARY) && ${BTLB_BINARY} == "yes" 
#----#LDFLAGS+= -T${SB_BACKING_SB:U${RELSRCTOP:H}}/src/sys/conf/ldscript-btlb-static.xlrexe
.endif
.else
.ifndef HOSTPROG
.if ${MACHINE} == "xlr"
.if defined(BTLB_BINARY) && ${BTLB_BINARY} == "yes" 
#----#SHEXE_LDFLAGS.xlr ?= -T${SB_BACKING_SB:U${.SRCTOP:H}}/src/sys/conf/ldscript-btlb-shared.xlrexe
.else
#----#SHEXE_LDFLAGS.xlr ?= -T${SB_BACKING_SB:U${.SRCTOP:H}}/src/sys/conf/ldscript.xlrexe 
.endif
LDFLAGS += ${SHEXE_LDFLAGS.${MACHINE}}
.endif
.if ${MACHINE} == "octeon"
SHEXE_LDFLAGS.octeon ?= -T${.SRCTOP}/bsd/sys/conf/ldscript.mips.octeon.exe 
LDFLAGS += ${SHEXE_LDFLAGS.${MACHINE}}
.endif
.endif
.endif

.if defined(PROG_CXX)
PROG=	${PROG_CXX}
.endif

.if defined(SRCS)
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}
.else
OBJS+=	${PROG}.o
.endif

.for _F in ${OBJS}
.ORDER: dirdep genfiles ${_F}
.endfor

_LIBNAMES=
_LIBPATHS=
_LIBREFS=
_NOT_FOUND=

.for l in ${LDFLAGS:M-l*:S/^-l//} ${LDADD:M-l*:S/^-l//} ${LDEND:M-l*:S/^-l//}
_LIBNAMES += lib${l}
.endfor

.for p in ${LDFLAGS:M-L*:S/^-L//} ${LDADD:M-L*:S/^-L//} ${LDEND:M-L*:S/^-L//}
_LIBPATHS += ${p}
.endfor

.for l in ${_LIBNAMES}
_found=
.if empty(LDFLAGS:M-static)
.for p in ${_LIBPATHS}
.if empty(_found) && exists(${p}/${l}.so)
_found= ${p}/${l}.so
.endif
.endfor
.endif
.if empty(_found)
.for p in ${_LIBPATHS}
.if empty(_found) && exists(${p}/${l}.a)
_found= ${p}/${l}.a
.endif
.endfor
.endif
.if empty(_found)
_NOT_FOUND+= ${l}
.else
_LIBREFS+= ${found}
.endif
.endfor
.if !empty(_NOT_FOUND) && !defined(HOSTPROG) && make(all)
.error "Could not find ${_NOT_FOUND} in ${_LIBPATHS}"
.endif

.if defined(PROG)
.if defined(SRCS)

# If there are Objective C sources, link with Objective C libraries.
.if !empty(SRCS:M*.m)
.if defined(OBJCLIBS)
LDADD+=	${OBJCLIBS}
.else
LDADD+=	-lobjc -lpthread
.endif
.endif

${PROG}: ${OBJS} ${_LIBREFS}
.if defined(PROG_CXX)
	${CXX} ${CXXFLAGS} ${LDSTART} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD} ${LDEND}
.else
	${CC} ${CFLAGS} ${LDSTART} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD} ${LDEND}
.endif
.if defined(CTFMERGE)
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS}
.endif

.else	# !defined(SRCS)

.if !target(${PROG})
.if defined(PROG_CXX)
SRCS=	${PROG}.cc
.else
SRCS=	${PROG}.c
.endif

${PROG}: ${OBJS} ${_LIBREFS}
.if defined(PROG_CXX)
	${CXX} ${CXXFLAGS} ${LDSTART} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD} ${LDEND}
.else
	${CC} ${CFLAGS} ${LDSTART} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD} ${LDEND}
.endif
.if defined(CTFMERGE)
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS}
.endif
.endif

.endif
.endif

.if defined(BINDIR) && !empty(BINDIR)
_BINFILES=
.if defined(PROG)
${BINDIR}/${PROG}:	${PROG}
	cp ${.ALLSRC} ${.TARGET}
	echo "# ${.SRCREL}" > ${.TARGET}.srcrel
_BINFILES+= ${BINDIR}/${PROG}
.endif
.if defined(FILES)
.for _F in ${FILES}
${BINDIR}/${_F}:	${_F}
	cp ${.ALLSRC} ${.TARGET}; \
	echo "# ${.SRCREL}" > ${.TARGET}.srcrel
_BINFILES+= ${BINDIR}/${_F}
.endfor
.endif

installprog: ${_BINFILES}
.else
installprog:
.endif

.if ${__MKLVL__} != 1
all: genfiles allincs buildobjs buildprog installprog relfiles
.ORDER: genfiles allincs buildobjs buildprog installprog relfiles
buildprog: buildobjs ${PROG}
buildobjs: ${OBJS}
.endif

.endif
.endif

.include <bsd.dirdep.mk>
.include <bsd.incs.mk>
.include <bsd.genfiles.mk>
.include <bsd.relfiles.mk>
.endif
