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
	llvmarminstprinter \
	llvmarminfo \
	llvmia64codegen \
	llvmia64info \
	llvmia64instprinter \
	llvmmipscodegen \
	llvmmipsinfo \
	llvmpowerpccodegen \
	llvmpowerpcinstprinter \
	llvmpowerpcinfo \
	llvmx86disassembler \
	llvmx86asmparser \
	llvmx86codegen \
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
