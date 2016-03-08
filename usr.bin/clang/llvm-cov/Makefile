# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-cov

SRCDIR=	tools/llvm-cov
SRCS=	CodeCoverage.cpp \
	CoverageFilters.cpp \
	CoverageReport.cpp \
	CoverageSummaryInfo.cpp \
	SourceCoverageView.cpp \
	TestingSupport.cpp \
	gcov.cpp \
	llvm-cov.cpp

LIBDEPS=llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
