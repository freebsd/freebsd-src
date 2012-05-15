# $FreeBSD$

PROG_CXX=llvm-mc
NO_MAN=

SRCDIR=	tools/llvm-mc
SRCS=	llvm-mc.cpp \
	Disassembler.cpp

LIBDEPS=llvmmcdisassembler \
	llvmarmdisassembler \
	llvmarmasmparser \
	llvmarmcodegen \
	llvmarmdesc \
	llvmarminstprinter \
	llvmarminfo \
	llvmmipscodegen \
	llvmmipsdesc \
	llvmmipsinstprinter \
	llvmmipsinfo \
	llvmpowerpccodegen \
	llvmpowerpcdesc \
	llvmpowerpcinstprinter \
	llvmpowerpcinfo \
	llvmx86disassembler \
	llvmx86asmparser \
	llvmx86codegen \
	llvmx86desc \
	llvmselectiondag \
	llvmasmprinter \
	llvmmcparser \
	llvmcodegen \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmx86instprinter \
	llvmx86utils \
	llvmcore \
	llvmx86info \
	llvmmc \
	llvmsupport

.include "../clang.prog.mk"
