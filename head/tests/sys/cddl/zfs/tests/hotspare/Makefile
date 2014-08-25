# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/hotspare
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	hotspare_test

FILES+=	hotspare_create_001_neg.ksh
FILES+=	setup.ksh
FILES+=	hotspare_add_004_neg.ksh
FILES+=	hotspare_scrub_002_pos.ksh
FILES+=	hotspare_detach_002_pos.ksh
FILES+=	hotspare_remove_001_pos.ksh
FILES+=	hotspare_add_001_pos.ksh
FILES+=	hotspare_import_001_pos.ksh
FILES+=	hotspare_replace_002_neg.ksh
FILES+=	hotspare_clone_002_pos.ksh
FILES+=	hotspare_replace_003_neg.ksh
FILES+=	hotspare_export_001_neg.ksh
FILES+=	hotspare_snapshot_001_pos.ksh
FILES+=	hotspare_remove_004_pos.ksh
FILES+=	hotspare_detach_003_pos.ksh
FILES+=	hotspare_onoffline_004_neg.ksh
FILES+=	cleanup.ksh
FILES+=	hotspare_replace_001_neg.ksh
FILES+=	hotspare_scrub_001_pos.ksh
FILES+=	hotspare_detach_001_pos.ksh
FILES+=	hotspare_add_003_neg.ksh
FILES+=	hotspare_remove_003_neg.ksh
FILES+=	hotspare_add_002_pos.ksh
FILES+=	hotspare_detach_004_pos.ksh
FILES+=	hotspare_snapshot_002_pos.ksh
FILES+=	hotspare.kshlib
FILES+=	hotspare_detach_005_neg.ksh
FILES+=	hotspare_remove_002_neg.ksh
FILES+=	hotspare_onoffline_003_neg.ksh
FILES+=	hotspare_shared_001_pos.ksh
FILES+=	hotspare.cfg
FILES+=	hotspare_clone_001_pos.ksh

.include <bsd.test.mk>
