# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/mmap/mmap_write
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	mmap_write_test

${PACKAGE}FILES+=	mmap_write_001_pos.ksh
${PACKAGE}FILES+=	mmap_write.cfg
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	setup.ksh

.include <bsd.test.mk>
