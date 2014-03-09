# $FreeBSD$

NO_MAN=
.include <bsd.own.mk>

PROG_CXX=macho-dump

SRCDIR=	tools/macho-dump
SRCS=	macho-dump.cpp

LIBDEPS=llvmobject \
	llvmsupport

.include "../clang.prog.mk"
