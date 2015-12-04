#	@(#)Makefile	5.33.1.1 (Berkeley) 5/6/91
# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= lib .WAIT \
	${_tests} usr.bin

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
