# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-lto

SRCDIR=	tools/llvm-lto
SRCS=	llvm-lto.cpp

TGHDRS=	Intrinsics
LIBDEPS=llvmaarch64disassembler \
	llvmaarch64codegen \
	llvmaarch64asmparser \
	llvmaarch64desc \
	llvmaarch64info \
	llvmaarch64instprinter \
	llvmaarch64utils \
	llvmarmdisassembler \
	llvmarmcodegen \
	llvmarmasmparser \
	llvmarmdesc \
	llvmarminfo \
	llvmarminstprinter \
	llvmmipsdisassembler \
	llvmmipscodegen \
	llvmmipsasmparser \
	llvmmipsdesc \
	llvmmipsinfo \
	llvmmipsinstprinter \
	llvmpowerpcdisassembler \
	llvmpowerpccodegen \
	llvmpowerpcasmparser \
	llvmpowerpcdesc \
	llvmpowerpcinfo \
	llvmpowerpcinstprinter \
	llvmsparcdisassembler \
	llvmsparccodegen \
	llvmsparcasmparser \
	llvmsparcdesc \
	llvmsparcinfo \
	llvmsparcinstprinter \
	llvmx86disassembler \
	llvmx86asmparser \
	llvmx86codegen \
	llvmselectiondag \
	llvmasmprinter \
	llvmx86desc \
	llvmx86info \
	llvmx86instprinter \
	llvmx86utils \
	llvmmcdisassembler \
	llvmlto \
	llvmobjcarcopts \
	llvmlinker \
	llvmipo \
	llvmvectorize \
	llvmcodegen \
	llvmtarget \
	llvmscalaropts \
	llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvminstcombine \
	llvminstrumentation \
	llvmtransformutils \
	llvmipa \
	llvmmc \
	llvmbitwriter \
	llvmbitreader \
	llvmanalysis \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"

DPADD+=	${LIBZ}
LDADD+=	-lz
