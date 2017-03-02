# $FreeBSD$

NAME=	nbt
SRCS=	alias_nbt.c

.include <bsd.compiler.mk>

.if ${COMPILER_TYPE} == "clang"
CFLAGS+=	-Wno-address-of-packed-member
.endif

.include <bsd.lib.mk>
