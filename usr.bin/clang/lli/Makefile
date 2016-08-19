# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=lli

SRCDIR=	tools/lli
SRCS=	OrcLazyJIT.cpp \
	lli.cpp

TGHDRS=	Attributes \
	Intrinsics

LIBDEPS=llvmirreader \
	llvminterpreter \
	llvmmcjit \
	llvmorcjit \
	llvmx86codegen \
	llvmx86asmparser \
	llvmx86desc \
	llvmx86disassembler \
	llvmasmparser \
	llvmexecutionengine \
	llvmruntimedyld \
	llvmselectiondag \
	llvmasmprinter \
	llvmcodegen \
	llvminstrumentation \
	llvmbitwriter \
	llvmscalaropts \
	llvminstcombine \
	llvmtarget \
	llvmtransformutils \
	llvmanalysis \
	llvmprofiledata \
	llvmdebuginfocodeview \
	llvmobject \
	llvmbitreader \
	llvmx86asmprinter \
	llvmx86utils \
	llvmcore \
	llvmmcparser \
	llvmx86info \
	llvmmcdisassembler \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
