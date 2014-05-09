# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangdriver

SRCDIR=	tools/clang/lib/Driver
SRCS=	Action.cpp \
	CC1AsOptions.cpp \
	Compilation.cpp \
	Driver.cpp \
	DriverOptions.cpp \
	Job.cpp \
	Phases.cpp \
	SanitizerArgs.cpp \
	Tool.cpp \
	ToolChain.cpp \
	ToolChains.cpp \
	Tools.cpp \
	Types.cpp \
	WindowsToolChain.cpp

TGHDRS=	CC1AsOptions \
	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticSemaKinds \
	Options

.include "../clang.lib.mk"
