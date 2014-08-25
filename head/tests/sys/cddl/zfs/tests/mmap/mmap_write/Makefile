# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/mmap/mmap_write
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	mmap_write_test

FILES+=	mmap_write_001_pos.ksh
FILES+=	mmap_write.cfg
FILES+=	cleanup.ksh
FILES+=	setup.ksh

.include <bsd.test.mk>
