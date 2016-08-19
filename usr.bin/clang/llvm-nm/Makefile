# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-nm

SRCDIR=	tools/llvm-nm
SRCS=	llvm-nm.cpp

TGHDRS=	Attributes

LIBDEPS=llvmaarch64asmparser \
	llvmarmasmparser \
	llvmmipsasmparser \
	llvmpowerpcasmparser \
	llvmsparcasmparser \
	llvmx86asmparser \
	llvmaarch64desc \
	llvmarmdesc \
	llvmmipsdesc \
	llvmpowerpcdesc \
	llvmsparcdesc \
	llvmx86desc \
	llvmaarch64info \
	llvmarminfo \
	llvmmipsinfo \
	llvmpowerpcinfo \
	llvmsparcinfo \
	llvmx86info \
	llvmobject \
	llvmaarch64asmprinter \
	llvmaarch64utils \
	llvmarmasmprinter \
	llvmmipsasmprinter \
	llvmpowerpcasmprinter \
	llvmsparcasmprinter \
	llvmbitreader \
	llvmmcdisassembler \
	llvmx86asmprinter \
	llvmx86utils \
	llvmcore \
	llvmmcparser \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
