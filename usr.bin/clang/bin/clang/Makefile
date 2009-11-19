# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=clang

SRCDIR=	tools/clang/tools/driver
SRCS=	cc1_main.cpp driver.cpp
MAN=

.if ${MK_CLANG_IS_CC} != "no"
LINKS=	${BINDIR}/clang ${BINDIR}/cc \
	${BINDIR}/clang ${BINDIR}/CC \
	${BINDIR}/clang ${BINDIR}/c++ \
	${BINDIR}/clang ${BINDIR}/clang++
.endif

TGHDRS=	DiagnosticCommonKinds DiagnosticDriverKinds CC1Options
LIBDEPS=clangdriver clangfrontend clanglex clangbasic \
	llvmbitreader llvmbitwriter llvmsupport llvmsystem

.include "../../clang.prog.mk"
