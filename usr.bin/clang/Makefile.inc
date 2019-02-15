# $FreeBSD$

WARNS?=		0

.include <bsd.compiler.mk>

.if ${COMPILER_TYPE} == "clang"
DEBUG_FILES_CFLAGS= -gline-tables-only
.else
DEBUG_FILES_CFLAGS= -g1
.endif

.include "../Makefile.inc"
