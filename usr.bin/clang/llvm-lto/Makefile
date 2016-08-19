# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-lto

SRCDIR=	tools/llvm-lto
SRCS=	llvm-lto.cpp

TGHDRS=	Attributes \
	Intrinsics

LIBDEPS=llvmaarch64codegen \
	llvmaarch64asmparser \
	llvmaarch64disassembler \
	llvmarmcodegen \
	llvmarmasmparser \
	llvmarmdisassembler \
	llvmmipscodegen \
	llvmmipsasmparser \
	llvmmipsdesc \
	llvmmipsdisassembler \
	llvmpowerpccodegen \
	llvmpowerpcasmparser \
	llvmpowerpcdesc \
	llvmpowerpcdisassembler \
	llvmsparccodegen \
	llvmsparcasmparser \
	llvmsparcdesc \
	llvmsparcdisassembler \
	llvmx86codegen \
	llvmx86asmparser \
	llvmx86desc \
	llvmx86disassembler \
	llvmlto \
	llvmglobalisel \
	llvmaarch64desc \
	llvmaarch64asmprinter \
	llvmaarch64info \
	llvmaarch64utils \
	llvmarmdesc \
	llvmarmasmprinter \
	llvmarminfo \
	llvmmipsasmprinter \
	llvmmipsinfo \
	llvmpowerpcasmprinter \
	llvmpowerpcinfo \
	llvmsparcasmprinter \
	llvmsparcinfo \
	llvmasmprinter \
	llvmdebuginfocodeview \
	llvmselectiondag \
	llvmx86asmprinter \
	llvmx86utils \
	llvmx86info \
	llvmmcdisassembler \
	llvmcodegen \
	llvmbitwriter \
	llvmtarget \
	llvmobjcarcopts \
	llvmipo \
	llvmirreader \
	llvmasmparser \
	llvmobject \
	llvmbitreader \
	llvmmcparser \
	llvmscalaropts \
	llvminstcombine \
	llvmlinker \
	llvminstrumentation \
	llvmmc \
	llvmvectorize \
	llvmtransformutils \
	llvmanalysis \
	llvmprofiledata \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
