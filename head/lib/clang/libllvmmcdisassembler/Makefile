# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmmcdisassembler

SRCDIR=	lib/MC/MCDisassembler
SRCS=	Disassembler.cpp \
	MCRelocationInfo.cpp

.if ${MK_CLANG_EXTRAS} != "no" || ${MK_LLDB} != "no"
SRCS+=	MCDisassembler.cpp \
	MCExternalSymbolizer.cpp
.endif

.include "../clang.lib.mk"
