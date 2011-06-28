# $FreeBSD$

PROG_CXX=llvm-rtdyld
NO_MAN=

SRCDIR=	tools/llvm-rtdyld
SRCS=	llvm-rtdyld.cpp

LIBDEPS=llvmjit \
	llvmexecutionengine \
	llvmruntimedyld \
	llvmobject \
	llvmarmdisassembler \
	llvmarmasmparser \
	llvmarmcodegen \
	llvmarminstprinter \
	llvmarminfo \
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
