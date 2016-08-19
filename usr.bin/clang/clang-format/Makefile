# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=clang-format

SRCDIR=	tools/clang/tools/clang-format
SRCS=	ClangFormat.cpp

LIBDEPS=clangformat \
	clangtoolingcore \
	clangrewrite \
	clangast \
	clanglex \
	clangbasic \
	llvmcore \
	llvmmc \
	llvmsupport

TGHDRS=	DiagnosticCommonKinds

.include "../clang.prog.mk"
