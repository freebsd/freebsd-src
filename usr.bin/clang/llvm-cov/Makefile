# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-cov

SRCDIR=	tools/llvm-cov
SRCS=	CodeCoverage.cpp \
	CoverageFilters.cpp \
	CoverageReport.cpp \
	CoverageSummaryInfo.cpp \
	SourceCoverageView.cpp \
	SourceCoverageViewHTML.cpp \
	SourceCoverageViewText.cpp \
	TestingSupport.cpp \
	gcov.cpp \
	llvm-cov.cpp

LIBDEPS=llvmcoverage \
	llvmprofiledata \
	llvmobject \
	llvmbitreader \
	llvmmcparser \
	llvmmc \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
