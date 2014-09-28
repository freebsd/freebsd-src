#	@(#)Makefile	5.33.1.1 (Berkeley) 5/6/91
# $FreeBSD$

.include <src.opts.mk>

SUBDIR= lib ${_tests} usr.bin

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

.include <bsd.subdir.mk>
