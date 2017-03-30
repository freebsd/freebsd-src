# $FreeBSD$

PROG_CXX=	llvm-cov

SRCDIR=		tools/llvm-cov
SRCS+=		CodeCoverage.cpp
SRCS+=		CoverageExporterJson.cpp
SRCS+=		CoverageFilters.cpp
SRCS+=		CoverageReport.cpp
SRCS+=		CoverageSummaryInfo.cpp
SRCS+=		SourceCoverageView.cpp
SRCS+=		SourceCoverageViewHTML.cpp
SRCS+=		SourceCoverageViewText.cpp
SRCS+=		TestingSupport.cpp
SRCS+=		gcov.cpp
SRCS+=		llvm-cov.cpp

LIBADD+=	z

.include "../llvm.prog.mk"
