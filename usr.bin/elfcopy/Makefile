# $FreeBSD$

.include <src.opts.mk>

ELFTCDIR=	${SRCTOP}/contrib/elftoolchain
ELFCOPYDIR=	${ELFTCDIR}/elfcopy

.PATH: ${ELFCOPYDIR}

.if ${MK_ELFCOPY_AS_OBJCOPY} != "no"
PROG=	objcopy
objcopy.1: elfcopy.1
	sed -e 's/\.Dt ELFCOPY 1/.Dt OBJCOPY 1/' \
	    -e 's/\.Nm elfcopy/.Nm objcopy/' < ${.ALLSRC} > ${.TARGET}
CLEANFILES+= objcopy.1
.else
PROG=	elfcopy
.endif

SRCS=	archive.c ascii.c binary.c main.c pe.c sections.c segments.c symbols.c

WARNS?=	5

LIBADD=	archive elftc elf pe

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/libpe -I${ELFTCDIR}/common
CFLAGS+=-DWITH_PE=1

MAN=	${PROG}.1 strip.1

LINKS=	${BINDIR}/${PROG} ${BINDIR}/strip

.include <bsd.prog.mk>
