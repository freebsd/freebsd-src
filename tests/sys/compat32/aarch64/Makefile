PACKAGE=	tests
FILESGROUPS+=	asmprogs

ACFLAGS= -target armv7-unknown-freebsd${OS_REVISION} -nostdlib -Wl,-e -Wl,main -static

TAP_TESTS_SH+=	swp_cond_test
TAP_TESTS_SH+=	swp_test
${PACKAGE}FILES+=	common.sh

# Each test will individually respect the compat.arm.emul_swp
# sysctl upon entry.
TEST_METADATA.swp_cond_test+=	is_exclusive=true
TEST_METADATA.swp_test+=	is_exclusive=true

asmprogsMODE=		0755
asmprogs+=	swp_cond_test_impl swp_test_impl
asmprogsDIR=	${TESTSDIR}

.for aprog in ${asmprogs}
${aprog}: ${aprog}.S
	${CC} ${ACFLAGS} -o ${.TARGET} ${.ALLSRC}
.endfor

.include <bsd.test.mk>
