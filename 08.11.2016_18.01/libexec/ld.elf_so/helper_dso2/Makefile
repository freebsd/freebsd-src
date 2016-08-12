# $NetBSD: Makefile,v 1.1 2011/03/31 23:34:48 joerg Exp $

.include <bsd.own.mk>

LIB=			h_helper_dso2
SRCS=			h_helper_dso2.c

DSO1DIR!=	cd ${.CURDIR}/../helper_dso1 && ${PRINTOBJDIR}
LDADD+=		-Wl,-rpath,${TESTSDIR} -L${DSO1DIR} -lh_helper_dso1

LIBDIR=		${TESTSBASE}/libexec/ld.elf_so
SHLIBDIR=	${TESTSBASE}/libexec/ld.elf_so
SHLIB_MAJOR=	1

MKSTATICLIB=	no
MKPROFILE=	no
MKPICINSTALL=	no
MKLINT=		no

NOMAN=		# defined

.include <bsd.lib.mk>
