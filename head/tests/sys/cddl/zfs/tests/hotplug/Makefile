# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/hotplug
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	hotplug_test

${PACKAGE}FILES+=	hotplug.cfg
${PACKAGE}FILES+=	hotplug_006_pos.ksh
${PACKAGE}FILES+=	hotplug_002_pos.ksh
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	hotplug_003_pos.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	hotplug_005_pos.ksh
${PACKAGE}FILES+=	hotplug_001_pos.ksh
${PACKAGE}FILES+=	hotplug_009_pos.ksh
${PACKAGE}FILES+=	hotplug_010_pos.ksh
${PACKAGE}FILES+=	hotplug.kshlib
${PACKAGE}FILES+=	hotplug_011_pos.ksh
${PACKAGE}FILES+=	hotplug_008_pos.ksh
${PACKAGE}FILES+=	hotplug_004_pos.ksh

.include <bsd.test.mk>
