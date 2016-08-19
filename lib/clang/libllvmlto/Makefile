# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmlto

SRCDIR=	lib/LTO
SRCS=	LTO.cpp \
	LTOCodeGenerator.cpp \
	LTOModule.cpp \
	ThinLTOCodeGenerator.cpp \
	UpdateCompilerUsed.cpp

TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
