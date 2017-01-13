# $NetBSD: Makefile,v 1.1 2014/08/25 20:40:53 joerg Exp $

.include <bsd.own.mk>

LIB=			h_helper_ifunc_dso
SRCS=			h_helper_ifunc.c

LIBDIR=		${TESTSBASE}/libexec/ld.elf_so
SHLIBDIR=	${TESTSBASE}/libexec/ld.elf_so
SHLIB_MAJOR=	1

MKSTATICLIB=	no
MKPROFILE=	no
MKPICINSTALL=	no
MKLINT=		no

NOMAN=		# defined

.include <bsd.lib.mk>
