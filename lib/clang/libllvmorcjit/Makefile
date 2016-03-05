# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmorcjit

SRCDIR=	lib/ExecutionEngine/Orc
SRCS=	ExecutionUtils.cpp \
	IndirectionUtils.cpp \
	NullResolver.cpp \
	OrcArchitectureSupport.cpp \
	OrcCBindings.cpp \
	OrcCBindingsStack.cpp \
	OrcError.cpp \
	OrcMCJITReplacement.cpp \
	OrcRemoteTargetRPCAPI.cpp

TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
