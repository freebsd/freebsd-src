PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet6
FILESDIR=	${TESTSDIR}

ATF_TESTS_PYTEST=	test_ip6_output.py
ATF_TESTS_SH=		exthdr \
			mld \
			scapyi386 \
			redirect \
			divert \
			forward6 \
			output6 \
			lpm6 \
			fibs6 \
			ndp \
			proxy_ndp

TEST_METADATA.output6+=	required_programs="python"

${PACKAGE}FILES+=	exthdr.py \
			mld.py \
			scapyi386.py \
			ra.py \
			redirect.py

${PACKAGE}FILESMODE_exthdr.py=	0555
${PACKAGE}FILESMODE_mld.py=	0555
${PACKAGE}FILESMODE_scapyi386.py=0555
${PACKAGE}FILESMODE_ra.py=0555
${PACKAGE}FILESMODE_redirect.py=0555

TESTS_SUBDIRS+=	frag6

.include <bsd.test.mk>
