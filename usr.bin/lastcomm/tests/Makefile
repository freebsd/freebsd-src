PACKAGE=	tests

TAP_TESTS_SH=	legacy_test
TEST_METADATA.legacy_test+=	allowed_architectures="amd64 i386"
TEST_METADATA.legacy_test+=	required_programs="lastcomm"

${PACKAGE}FILES+=		v1-amd64-acct.in
${PACKAGE}FILES+=		v1-amd64.out
${PACKAGE}FILES+=		v1-i386-acct.in
${PACKAGE}FILES+=		v1-i386.out
${PACKAGE}FILES+=		v2-amd64-acct.in
${PACKAGE}FILES+=		v2-amd64.out
${PACKAGE}FILES+=		v2-i386-acct.in
${PACKAGE}FILES+=		v2-i386.out

.include <bsd.test.mk>
