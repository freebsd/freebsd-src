# $FreeBSD: head/usr.bin/clang/lli/Makefile 231057 2012-02-05 23:56:22Z dim $

PROG_CXX=lli

SRCDIR=	tools/lli
SRCS=	lli.cpp

LIBDEPS=llvmasmparser \
	llvmbitreader \
	llvmia64codegen \
	llvmia64desc \
	llvmia64info \
	llvmia64instprinter \
	llvmx86codegen \
	llvmx86desc \
	llvmx86info \
	llvmx86instprinter \
	llvmx86utils \
	llvmselectiondag \
	llvmasmprinter \
	llvmmcparser \
	llvminterpreter \
	llvmjit \
	llvmcodegen \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmmcjit \
	llvmruntimedyld \
	llvmexecutionengine \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
