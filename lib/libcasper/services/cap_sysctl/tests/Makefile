
.include <src.opts.mk>

ATF_TESTS_C=	sysctl_test

.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_sysctl
CFLAGS+=-DWITH_CASPER
.endif
LIBADD+=	nv

# cap_sysctl tests modify global sysctl values and read them back, so
# cannot be run in parallel.
TEST_METADATA.sysctl_test+=	required_user="root" \
				is_exclusive=true

.include <bsd.test.mk>
