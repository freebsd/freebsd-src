# $FreeBSD: head/usr.bin/clang/llvm-rtdyld/Makefile 231057 2012-02-05 23:56:22Z dim $

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
	llvmarmdesc \
	llvmarminstprinter \
	llvmarminfo \
	llvmia64codegen \
	llvmia64desc \
	llvmia64info \
	llvmia64instprinter \
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
