
PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet6/frag6
FILESDIR=	${TESTSDIR}

# We split these up so they can run in parallel.
# Seems kyua is not running the test cases from one file in parallel.
# Otherwise we could cat the files together into one shell file.
ATF_TESTS_SH=			\
				frag6_01 \
				frag6_02 \
				frag6_03 \
				frag6_04 \
				frag6_05 \
				frag6_06 \
				frag6_07 \
				frag6_08 \
				frag6_09 \
				frag6_10 \
				frag6_11 \
				frag6_12 \
				frag6_13 \
				frag6_14 \
				frag6_15 \
				frag6_16 \
				frag6_17 \
				frag6_18 \
				frag6_19 \
				frag6_20

${PACKAGE}FILES+=		frag6.subr
${PACKAGE}FILES+=		sniffer.py
${PACKAGE}FILES+=		frag6_01.py
${PACKAGE}FILES+=		frag6_02.py
${PACKAGE}FILES+=		frag6_03.py
${PACKAGE}FILES+=		frag6_04.py
${PACKAGE}FILES+=		frag6_05.py
${PACKAGE}FILES+=		frag6_06.py
${PACKAGE}FILES+=		frag6_07.py
${PACKAGE}FILES+=		frag6_08.py
${PACKAGE}FILES+=		frag6_09.py
${PACKAGE}FILES+=		frag6_10.py
${PACKAGE}FILES+=		frag6_11.py
${PACKAGE}FILES+=		frag6_12.py
${PACKAGE}FILES+=		frag6_13.py
${PACKAGE}FILES+=		frag6_14.py
${PACKAGE}FILES+=		frag6_15.py
${PACKAGE}FILES+=		frag6_16.py
${PACKAGE}FILES+=		frag6_17.py
${PACKAGE}FILES+=		frag6_18.py
${PACKAGE}FILES+=		frag6_19.py
${PACKAGE}FILES+=		frag6_20.py

${PACKAGE}FILESMODE_frag6.subr=		0444
${PACKAGE}FILESMODE_sniffer.py=		0555
${PACKAGE}FILESMODE_frag6_01.py=	0555
${PACKAGE}FILESMODE_frag6_02.py=	0555
${PACKAGE}FILESMODE_frag6_03.py=	0555
${PACKAGE}FILESMODE_frag6_04.py=	0555
${PACKAGE}FILESMODE_frag6_05.py=	0555
${PACKAGE}FILESMODE_frag6_06.py=	0555
${PACKAGE}FILESMODE_frag6_07.py=	0555
${PACKAGE}FILESMODE_frag6_08.py=	0555
${PACKAGE}FILESMODE_frag6_09.py=	0555
${PACKAGE}FILESMODE_frag6_10.py=	0555
${PACKAGE}FILESMODE_frag6_11.py=	0555
${PACKAGE}FILESMODE_frag6_12.py=	0555
${PACKAGE}FILESMODE_frag6_13.py=	0555
${PACKAGE}FILESMODE_frag6_14.py=	0555
${PACKAGE}FILESMODE_frag6_15.py=	0555
${PACKAGE}FILESMODE_frag6_16.py=	0555
${PACKAGE}FILESMODE_frag6_17.py=	0555
${PACKAGE}FILESMODE_frag6_18.py=	0555
${PACKAGE}FILESMODE_frag6_19.py=	0555
${PACKAGE}FILESMODE_frag6_20.py=	0555

.include <bsd.test.mk>
