# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/mmap/mmap_read
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	mmap_read_test

FILES+=	cleanup.ksh
FILES+=	setup.ksh
FILES+=	mmap_read_001_pos.ksh
FILES+=	mmap_read.cfg

.include <bsd.test.mk>
