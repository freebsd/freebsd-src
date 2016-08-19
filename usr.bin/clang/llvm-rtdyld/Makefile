# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-rtdyld

SRCDIR=	tools/llvm-rtdyld
SRCS=	llvm-rtdyld.cpp

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
	llvmdebuginfodwarf \
	llvmexecutionengine \
	llvmruntimedyld \
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
	llvmx86asmprinter \
	llvmx86utils \
	llvmx86info \
	llvmmcdisassembler \
	llvmobject \
	llvmmcparser \
	llvmbitreader \
	llvmtarget \
	llvmmc \
	llvmanalysis \
	llvmprofiledata \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
