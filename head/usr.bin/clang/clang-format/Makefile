# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=clang-format

SRCDIR=	tools/clang/tools/clang-format
SRCS=	ClangFormat.cpp

LIBDEPS=clangrewrite \
	clanglex \
	clangformat \
	clangbasic \
	clangast \
	clangtoolingcore \
	llvmmc \
	llvmcore \
	llvmsupport \

TGHDRS=	DiagnosticCommonKinds

.include "../clang.prog.mk"
