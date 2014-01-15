# $Id: Makefile 2290 2011-12-04 07:20:46Z jkoshy $

TOP=	..

PROG=	elfcopy

SRCS=	archive.c ascii.c binary.c main.c sections.c segments.c symbols.c

WARNS?=	5

DPADD=	${LIBELF} ${LIBELFTC}
LDADD=	-lelf -lelftc

.if !defined(LIBELF_AR)
LDADD+= -larchive
.endif

MAN=	elfcopy.1 mcs.1 strip.1

NO_SHARED?=	yes

LINKS=	${BINDIR}/elfcopy ${BINDIR}/strip 	\
	${BINDIR}/elfcopy ${BINDIR}/mcs

EXTRA_TARGETS=	strip mcs

CLEANFILES+=	${EXTRA_TARGETS}

# Create in-place symbolic links to "elfcopy" at build time.

all:	${EXTRA_TARGETS}

${EXTRA_TARGETS}:	${PROG}
	ln -s ${PROG} ${.TARGET}

.include "${TOP}/mk/elftoolchain.prog.mk"

.if ${OS_HOST} == "OpenBSD"
CFLAGS+=	-I/usr/local/include
LDFLAGS+=	-L/usr/local/lib
.endif
