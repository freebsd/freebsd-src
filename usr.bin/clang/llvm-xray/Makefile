# $FreeBSD$

PROG_CXX=	llvm-xray
MAN=

SRCDIR=		tools/llvm-xray
SRCS+=		func-id-helper.cc
SRCS+=		llvm-xray.cc
SRCS+=		xray-extract.cc
SRCS+=		xray-registry.cc

LIBADD+=	z

.include "../llvm.prog.mk"
