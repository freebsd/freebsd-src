# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp0
SRCS=	cppdefault.c cpperror.c cppexp.c cppfiles.c cpphash.c cppinit.c cpplex.c cpplib.c \
	cppmain.c cppulp.c mkdeps.c
BINDIR=	/usr/libexec
NOMAN=	1

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
