# $NetBSD: Makefile,v 1.4 2014/06/10 04:28:39 he Exp $
#
.include "Makefile.inc"
.include <bsd.own.mk>

.if ${MACHINE_ARCH} != "m68000"
TESTS_SUBDIRS=		ld.elf_so
.endif

TESTSDIR=	${TESTSBASE}/libexec

.include <bsd.test.mk>
