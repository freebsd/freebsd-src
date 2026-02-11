# Define the NetBSD ATF test scripts
NETBSD_ATF_TESTS_SH=	expr_test

# Apply sed transformations to the expr_test script
ATF_TESTS_SH_SED_expr_test+=	-e 's/eval expr/eval expr --/g'
ATF_TESTS_SH_SED_expr_test+=	-e 's/"expr: integer overflow or underflow occurred for operation.*"/"expr: overflow"/g'

# Include NetBSD ATF test framework
.include <netbsd-tests.test.mk>

# Include BSD test framework
.include <bsd.test.mk>
