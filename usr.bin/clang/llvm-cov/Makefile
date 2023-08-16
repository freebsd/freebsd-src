
.include <src.opts.mk>

PROG_CXX=	llvm-cov
LINKS=		${BINDIR}/llvm-cov	${BINDIR}/gcov
MLINKS=		llvm-cov.1		gcov.1

SRCDIR=		llvm/tools/llvm-cov
SRCS+=		CodeCoverage.cpp
SRCS+=		CoverageExporterJson.cpp
SRCS+=		CoverageExporterLcov.cpp
SRCS+=		CoverageFilters.cpp
SRCS+=		CoverageReport.cpp
SRCS+=		CoverageSummaryInfo.cpp
SRCS+=		SourceCoverageView.cpp
SRCS+=		SourceCoverageViewHTML.cpp
SRCS+=		SourceCoverageViewText.cpp
SRCS+=		TestingSupport.cpp
SRCS+=		gcov.cpp
SRCS+=		llvm-cov.cpp

.include "../llvm.prog.mk"
