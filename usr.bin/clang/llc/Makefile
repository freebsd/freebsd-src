# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llc

SRCDIR=	tools/llc
SRCS=	llc.cpp

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
	llvmasmprinter \
	llvmirreader \
	llvmmirparser \
	llvmselectiondag \
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
	llvmdebuginfocodeview \
	llvmx86asmprinter \
	llvmx86utils \
	llvmobject \
	llvmmcparser \
	llvmx86info \
	llvmmcdisassembler \
	llvmcodegen \
	llvmscalaropts \
	llvminstcombine \
	llvmbitreader \
	llvmbitwriter \
	llvminstrumentation \
	llvmtransformutils \
	llvmtarget \
	llvmanalysis \
	llvmprofiledata \
	llvmmc \
	llvmasmparser \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
