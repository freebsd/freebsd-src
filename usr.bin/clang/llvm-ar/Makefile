# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-ar

SRCDIR=	tools/llvm-ar
SRCS=	llvm-ar.cpp

TGHDRS=	Attributes

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
	llvmlibdriver \
	llvmobject \
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
	llvmcodegen \
	llvmscalaropts \
	llvminstcombine \
	llvmbitwriter \
	llvminstrumentation \
	llvmtransformutils \
	llvmtarget \
	llvmanalysis \
	llvmprofiledata \
	llvmx86asmprinter \
	llvmx86utils \
	llvmx86info \
	llvmmcdisassembler \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmoption \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
