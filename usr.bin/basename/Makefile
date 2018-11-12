#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	basename
MLINKS=	basename.1 dirname.1

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
