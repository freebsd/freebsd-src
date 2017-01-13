# $NetBSD: Makefile,v 1.3 2014/02/14 20:33:32 joerg Exp $

.include <bsd.own.mk>

LIB=		h_helper_symver_dso
SRCS=		h_helper_symver_dso2.c

LIBDIR=		${TESTSBASE}/libexec/ld.elf_so/h_helper_symver_dso2
SHLIBDIR=	${TESTSBASE}/libexec/ld.elf_so/h_helper_symver_dso2
SHLIB_MAJOR=	1

MKSTATICLIB=	no
MKPROFILE=	no
MKPICINSTALL=	no
MKLINT=		no

NOMAN=		# defined

FILESDIR=	${TESTSBASE}/libexec/ld.elf_so/h_helper_symver_dso2

LDFLAGS.lib${LIB} += -Wl,--version-script=${.CURDIR}/h_helper_symver_dso2.map

.include <bsd.lib.mk>
