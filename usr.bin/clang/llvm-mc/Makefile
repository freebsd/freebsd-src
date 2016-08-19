# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-mc

SRCDIR=	tools/llvm-mc
SRCS=	Disassembler.cpp \
	llvm-mc.cpp

LIBDEPS=llvmaarch64asmparser \
	llvmarmasmparser \
	llvmmipsasmparser \
	llvmpowerpcasmparser \
	llvmsparcasmparser \
	llvmx86asmparser \
	llvmmipsdesc \
	llvmpowerpcdesc \
	llvmsparcdesc \
	llvmx86desc \
	llvmaarch64disassembler \
	llvmarmdisassembler \
	llvmmipsdisassembler \
	llvmpowerpcdisassembler \
	llvmsparcdisassembler \
	llvmx86disassembler \
	llvmmipsinfo \
	llvmpowerpcinfo \
	llvmsparcinfo \
	llvmx86info \
	llvmmipsasmprinter \
	llvmpowerpcasmprinter \
	llvmsparcasmprinter \
	llvmx86asmprinter \
	llvmx86utils \
	llvmobject \
	llvmbitreader \
	llvmcore \
	llvmmcparser \
	llvmaarch64desc \
	llvmaarch64asmprinter \
	llvmaarch64info \
	llvmaarch64utils \
	llvmarmdesc \
	llvmarmasmprinter \
	llvmarminfo \
	llvmmcdisassembler \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
