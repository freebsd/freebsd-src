# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangdriver

SRCDIR=	tools/clang/lib/Driver
SRCS=	Action.cpp \
	Compilation.cpp \
	Driver.cpp \
	DriverOptions.cpp \
	Job.cpp \
	Multilib.cpp \
	Phases.cpp \
	SanitizerArgs.cpp \
	Tool.cpp \
	ToolChain.cpp \
	ToolChains.cpp \
	Tools.cpp \
	Types.cpp \
	WindowsToolChain.cpp

TGHDRS=	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticSemaKinds \
	Options

.include "../clang.lib.mk"
