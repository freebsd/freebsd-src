
.include "Makefile.common"

SUBDIR=		variant_pcs_dso

ATF_TESTS_C=	variant_pcs

SRCS.variant_pcs=	variant_pcs.c variant_pcs_helper.S
DPADD.variant_pcs+=	${.OBJDIR}/dso/libh_variant_pcs.so
LDFLAGS.variant_pcs+=	-Wl,-rpath,${TESTSDIR} -L${.OBJDIR}/variant_pcs_dso
LDADD.variant_pcs+=	-lh_variant_pcs

# Ensure the dso is built first
variant_pcs: variant_pcs_dso

.include <bsd.test.mk>
