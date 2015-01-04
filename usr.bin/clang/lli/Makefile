# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=lli

SRCDIR=	tools/lli
SRCS=	lli.cpp \
	RemoteMemoryManager.cpp \
	RemoteTarget.cpp \
	RemoteTargetExternal.cpp

LIBDEPS=llvmx86disassembler \
	llvmx86asmparser \
	llvmirreader \
	llvmasmparser \
	llvmx86codegen \
	llvmx86desc \
	llvmx86info \
	llvmx86instprinter \
	llvmx86utils \
	llvmselectiondag \
	llvmasmprinter \
	llvminterpreter \
	llvminstrumentation \
	llvmjit \
	llvmcodegen \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmmcjit \
	llvmtarget \
	llvmruntimedyld \
	llvmobject \
	llvmmcparser \
	llvmbitreader \
	llvmexecutionengine \
	llvmmc \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"

DPADD+=	${LIBZ}
LDADD+=	-lz
