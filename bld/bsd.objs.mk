# $FreeBSD$

.if defined(HOSTPROG)
CFLAGS+=	-DHOSTPROG
CC=		${HOST_CC}
.else
AFLAGS+=	${AFLAGS.${MACHINE}}
_CFLAGS:=	${CFLAGS}
CFLAGS=		${CFLAGS_BSD} ${_CFLAGS}
CFLAGS+=	${CFLAGS.${MACHINE}}
CFLAGS+=	-I${STAGEDIR}/usr/include
CFLAGS+=	-I${COMMONSTAGEDIR}/usr/include
.if !empty(SHAREDSTAGEDIR)
CFLAGS+=	-I${SHAREDSTAGEDIR}/usr/include
CFLAGS+=	-I${COMMONSHAREDSTAGEDIR}/usr/include
.endif
.endif

NOT_MACHINE_ARCH+= common host

.if defined(NOT_MACHINE_ARCH) && !empty(NOT_MACHINE_ARCH:M${MACHINE_ARCH})
DONT_DO_IT=
.endif

.if defined(NOT_MACHINE) && !empty(NOT_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if defined(ONLY_MACHINE) && empty(ONLY_MACHINE:M${MACHINE})
DONT_DO_IT=
.endif

.if defined(DONT_DO_IT)

.if ${__MKLVL__} != 1
all:	.PHONY
.endif

.else
.SUFFIXES:
.SUFFIXES: .out .o .po .So .S .asm .s .c .cc .cpp .cxx .m .C .f .y .l .ln

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

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}
.if !defined(NO_SHARED)
SOBJS+=  ${SRCS:N*.h:R:S/$/.So/g}
.endif

.for _F in ${OBJS} ${SOBJS}
.ORDER: dirdep genfiles ${_F}
.endfor

OBJFILES=

.if defined(OBJSDIR) && !empty(OBJSDIR)
.for _F in ${OBJS} ${SOBJS}
${OBJSDIR}/${_F}:	${_F}
	cp ${.ALLSRC} ${.TARGET}
	echo "# ${.SRCREL}" > ${.TARGET}.srcrel
OBJFILES+= ${OBJSDIR}/${_F}
.endfor
.endif

installobjs:	${OBJFILES}

.if ${__MKLVL__} != 1
all: genfiles buildobjs installobjs relfiles
.ORDER: genfiles buildobjs installobjs relfiles
buildobjs: ${OBJS} ${SOBJS}
.endif
.endif

.include <bsd.dirdep.mk>
.include <bsd.incs.mk>
.include <bsd.genfiles.mk>
.include <bsd.relfiles.mk>
