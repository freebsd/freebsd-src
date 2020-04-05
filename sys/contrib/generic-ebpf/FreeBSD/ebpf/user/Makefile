BASE=	../../..
.include "${BASE}/Makefile.inc"
.include <bsd.own.mk>
.if defined(COVERAGE) && !empty(COVERAGE)
CFLAGS+=	-coverage
LDFLAGS+=	-coverage
.endif
.include "Makefile.common"
