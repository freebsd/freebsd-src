# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR:H}/common

SRCS=		crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o crt1.o Scrt1.o
CFLAGS+=	-I${.CURDIR:H}/common \
		-I${SRCTOP}/lib/libc/include
.if ${MK_CHERI}	!= "no"
CFLAGS+=	-DCHERI
.endif

FILES=		${OBJS}
FILESMODE=	${LIBMODE}
FILESOWN=	${LIBOWN}
FILESGRP=	${LIBGRP}
FILESDIR=	${LIBDIR}
# These FILES qualify as libraries for the purpose of LIBRARIES_ONLY.
.undef LIBRARIES_ONLY

CLEANFILES=	${OBJS} crt1_c.o crt1_s.o gcrt1_c.o Scrt1_c.o
CLEANFILES+=	crt1_c.s gcrt1_c.s Scrt1_c.s

.if defined(LIBCHERI)
# We don't support .init and .fini on CheriABI so don't build the files.
FILES:=	${FILES:Ncrti.o:Ncrtn.o}
STRIP_EXCEPTIONS=	objcopy -R .eh_frame ${.TARGET}
.endif

# crt1 is split into an assembly part (crt1_s.s) and a C part (crt1_c.c) so
# that raw assembly can be used to re-align the stack to be 32-byte aligned.
# This is necessary for CHERI because capabilities must be 32-byte aligned.
# crt1_s.s contains the __start symbol, which re-aligns the stack and then
# calls __start1 in crt1_c.c, which is a normal C function using normal calling
# conventions, which finishes off program initialisation.
#
# See the comment in lib/csu/common/crtbrand.c for the reason crt1_c.c is not
# directly compiled to .o files.
#
# crt1 is compiled three times:
#  - gcrt1 is non-position-independent, with profiling support
#  - crt1 is non-position-independent, without profiling support
#  - Scrt1 is position-independent, without profiling support

crt1_s.o: crt1_s.S
	${CC} ${CFLAGS} ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}
	${STRIP_EXCEPTIONS}

# Compile crt1_c.c with -DGCRT
gcrt1_c.s: crt1_c.c
	${CC} ${CFLAGS} -DGCRT -S -o ${.TARGET} ${.CURDIR}/crt1_c.c
	sed ${SED_FIX_NOTE} ${.TARGET}

gcrt1_c.o: gcrt1_c.s
	${CC} ${CFLAGS:N-g} ${ACFLAGS} -c -o ${.TARGET} gcrt1_c.s

gcrt1.o: gcrt1_c.o crt1_s.o
	${LD} ${_LDFLAGS} -o gcrt1.o -r crt1_s.o gcrt1_c.o

# Compile crt1_c.c without -DGCRT
crt1_c.s: crt1_c.c
	${CC} ${CFLAGS} -S -o ${.TARGET} ${.CURDIR}/crt1_c.c
	sed ${SED_FIX_NOTE} ${.TARGET}

crt1_c.o: crt1_c.s
	${CC} ${CFLAGS:N-g} ${ACFLAGS} -c -o ${.TARGET} crt1_c.s

crt1.o:	crt1_c.o crt1_s.o
	${LD} ${_LDFLAGS} -o crt1.o -r crt1_s.o crt1_c.o

# Compile crt1_c.c with -fPIC -DPIC
Scrt1_c.s: crt1_c.c
	${CC} ${CFLAGS} -fPIC -DPIC -S -o ${.TARGET} ${.CURDIR}/crt1_c.c
	sed ${SED_FIX_NOTE} ${.TARGET}

Scrt1_c.o: Scrt1_c.s
	${CC} ${CFLAGS:N-g} ${ACFLAGS} -c -o ${.TARGET} Scrt1_c.s

Scrt1.o: Scrt1_c.o crt1_s.o
	${LD} ${_LDFLAGS} -o Scrt1.o -r crt1_s.o Scrt1_c.o

.include <bsd.lib.mk>

.if defined(LIBCHERI)
CC:=	${CC:N-g}
.endif
