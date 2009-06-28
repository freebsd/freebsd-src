# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=clang

SRCDIR=	tools/clang/tools/driver
SRCS=	driver.cpp
MAN=

.if ${MK_CLANG_IS_CC} != "no"
LINKS=	${BINDIR}/clang ${BINDIR}/cc \
	${BINDIR}/clang ${BINDIR}/c++ \
	${BINDIR}/clang ${BINDIR}/CC
.endif

TGHDRS=	DiagnosticCommonKinds
LIBDEPS=clangdriver clangfrontend clanglex clangbasic \
	llvmbitreader llvmbitwriter llvmsupport llvmsystem

.include "../../clang.prog.mk"
