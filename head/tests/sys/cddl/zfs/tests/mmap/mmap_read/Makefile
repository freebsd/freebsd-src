# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/mmap/mmap_read
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	mmap_read_test

${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	mmap_read_001_pos.ksh
${PACKAGE}FILES+=	mmap_read.cfg

.include <bsd.test.mk>
