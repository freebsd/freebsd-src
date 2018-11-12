# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangdriver

SRCDIR=	tools/clang/lib/Driver
SRCS=	Action.cpp \
	Compilation.cpp \
	CrossWindowsToolChain.cpp \
	Driver.cpp \
	DriverOptions.cpp \
	Job.cpp \
	MSVCToolChain.cpp \
	MinGWToolChain.cpp \
	Multilib.cpp \
	Phases.cpp \
	SanitizerArgs.cpp \
	Tool.cpp \
	ToolChain.cpp \
	ToolChains.cpp \
	Tools.cpp \
	Types.cpp

TGHDRS=	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticSemaKinds \
	Options

.include "../clang.lib.mk"
