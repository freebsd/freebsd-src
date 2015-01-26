# $FreeBSD$

.include <bsd.own.mk>

PROG=	expr
SRCS=	expr.y
YFLAGS=

NO_WMISSING_VARIABLE_DECLARATIONS=

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
