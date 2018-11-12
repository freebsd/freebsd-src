# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-lto

SRCDIR=	tools/llvm-lto
SRCS=	llvm-lto.cpp

TGHDRS=	Attributes \
	Intrinsics

LIBDEPS=llvmaarch64disassembler \
	llvmaarch64codegen \
	llvmaarch64asmparser \
	llvmaarch64desc \
	llvmaarch64info \
	llvmaarch64asmprinter \
	llvmaarch64utils \
	llvmarmdisassembler \
	llvmarmcodegen \
	llvmarmasmparser \
	llvmarmdesc \
	llvmarminfo \
	llvmarmasmprinter \
	llvmmipsdisassembler \
	llvmmipscodegen \
	llvmmipsasmparser \
	llvmmipsdesc \
	llvmmipsinfo \
	llvmmipsasmprinter \
	llvmpowerpcdisassembler \
	llvmpowerpccodegen \
	llvmpowerpcasmparser \
	llvmpowerpcdesc \
	llvmpowerpcinfo \
	llvmpowerpcasmprinter \
	llvmsparcdisassembler \
	llvmsparccodegen \
	llvmsparcasmparser \
	llvmsparcdesc \
	llvmsparcinfo \
	llvmsparcasmprinter \
	llvmx86disassembler \
	llvmx86asmparser \
	llvmx86codegen \
	llvmselectiondag \
	llvmasmprinter \
	llvmx86desc \
	llvmx86info \
	llvmx86asmprinter \
	llvmx86utils \
	llvmmcdisassembler \
	llvmlto \
	llvmobjcarcopts \
	llvmipo \
	llvmvectorize \
	llvmlinker \
	llvmirreader \
	llvmasmparser \
	llvmcodegen \
	llvmtarget \
	llvmscalaropts \
	llvminstcombine \
	llvminstrumentation \
	llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvmtransformutils \
	llvmmc \
	llvmbitwriter \
	llvmbitreader \
	llvmanalysis \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
