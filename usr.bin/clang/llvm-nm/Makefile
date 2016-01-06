# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-nm

SRCDIR=	tools/llvm-nm
SRCS=	llvm-nm.cpp

TGHDRS=	Attributes

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
	llvmcodegen \
	llvmtarget \
	llvmscalaropts \
	llvminstcombine \
	llvminstrumentation \
	llvmprofiledata \
	llvmtransformutils \
	llvmbitwriter \
	llvmanalysis \
	llvmx86desc \
	llvmobject \
	llvmmcparser \
	llvmbitreader \
	llvmmcdisassembler \
	llvmx86info \
	llvmx86asmprinter \
	llvmmc \
	llvmx86utils \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
