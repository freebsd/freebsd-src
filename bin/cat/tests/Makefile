# $FreeBSD$

PACKAGE=	tests

NETBSD_ATF_TESTS_SH=		cat_test

${PACKAGE}FILES+=		d_align.in
${PACKAGE}FILES+=		d_align.out
${PACKAGE}FILES+=		d_se_output.in
${PACKAGE}FILES+=		d_se_output.out

.include <netbsd-tests.test.mk>

d_align.out: ${TESTSRC}/d_align.out
	sed -E -e 's,^[[:space:]]{7}\$$$$,\$$,' < ${.ALLSRC} > ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}

CLEANFILES+=			d_align.out d_align.out.tmp

.include <bsd.test.mk>
