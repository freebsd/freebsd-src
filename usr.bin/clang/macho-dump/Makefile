# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=macho-dump

SRCDIR=	tools/macho-dump
SRCS=	macho-dump.cpp

LIBDEPS=llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
