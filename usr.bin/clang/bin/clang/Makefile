# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=clang

SRCDIR=	tools/clang/tools/driver
SRCS=	driver.cpp
MAN=

TGHDRS=	DiagnosticCommonKinds
LIBDEPS=clangdriver clangfrontend clanglex clangbasic \
	llvmbitreader llvmbitwriter llvmsupport llvmsystem

.include "../../clang.prog.mk"
