# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmmirparser

SRCDIR=	lib/CodeGen/MIRParser
SRCS=	MIParser.cpp \
	MIRParser.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	MILexer.cpp
.endif
	
.include "../clang.lib.mk"
