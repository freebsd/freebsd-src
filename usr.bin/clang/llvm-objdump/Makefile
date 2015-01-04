# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-objdump
DPADD=	${LIBZ}
LDADD=	-lz

SRCDIR=	tools/llvm-objdump
SRCS=	llvm-objdump.cpp \
	COFFDump.cpp \
	ELFDump.cpp \
	MachODump.cpp

LIBDEPS=llvmmcdisassembler \
	llvmmcanalysis \
	llvmdebuginfo \
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
	llvmcodegen \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmx86desc \
	llvmobject \
	llvmmcparser \
	llvmbitreader \
	llvmcore \
	llvmx86info \
	llvmx86instprinter \
	llvmmc \
	llvmx86utils \
	llvmsupport

.include "../clang.prog.mk"

DPADD+=	${LIBZ}
LDADD+=	-lz
