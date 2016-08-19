# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-objdump

SRCDIR=	tools/llvm-objdump
SRCS=	COFFDump.cpp \
	ELFDump.cpp \
	MachODump.cpp \
	llvm-objdump.cpp

LIBDEPS=llvmmipsdesc \
	llvmpowerpcdesc \
	llvmsparcdesc \
	llvmx86desc \
	llvmaarch64disassembler \
	llvmarmdisassembler \
	llvmmipsdisassembler \
	llvmpowerpcdisassembler \
	llvmsparcdisassembler \
	llvmx86disassembler \
	llvmmipsinfo \
	llvmpowerpcinfo \
	llvmsparcinfo \
	llvmx86info \
	llvmcodegen \
	llvmdebuginfodwarf \
	llvmobject \
	llvmmipsasmprinter \
	llvmpowerpcasmprinter \
	llvmsparcasmprinter \
	llvmx86asmprinter \
	llvmx86utils \
	llvmaarch64desc \
	llvmaarch64asmprinter \
	llvmaarch64info \
	llvmaarch64utils \
	llvmarmdesc \
	llvmarmasmprinter \
	llvmarminfo \
	llvmmcdisassembler \
	llvmbitwriter \
	llvminstrumentation \
	llvmscalaropts \
	llvminstcombine \
	llvmtarget \
	llvmtransformutils \
	llvmanalysis \
	llvmprofiledata \
	llvmbitreader \
	llvmcore \
	llvmmcparser \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
