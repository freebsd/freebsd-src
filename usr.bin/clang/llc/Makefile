# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llc

SRCDIR=	tools/llc
SRCS=	llc.cpp

LIBDEPS=llvmasmparser \
	llvmbitreader \
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
