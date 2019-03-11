# $FreeBSD$

.PATH: ${SRCTOP}/contrib/googletest/googletest/samples

GTEST_MAIN_REQ_TESTS+=	sample1_unittest
GTEST_MAIN_REQ_TESTS+=	sample2_unittest
GTEST_MAIN_REQ_TESTS+=	sample3_unittest
GTEST_MAIN_REQ_TESTS+=	sample4_unittest
GTEST_MAIN_REQ_TESTS+=	sample5_unittest
GTEST_MAIN_REQ_TESTS+=	sample6_unittest
GTEST_MAIN_REQ_TESTS+=	sample7_unittest
GTEST_MAIN_REQ_TESTS+=	sample8_unittest

# sample9_unittest's `CustomOutputTest.Fails` fails intentionally to illustrate
# how output format can be adjusted with command-line parameters.
#GTEST_REQ_TESTS+=	sample9_unittest
GTEST_REQ_TESTS+=	sample10_unittest

.for t in ${GTEST_MAIN_REQ_TESTS}
GTESTS+=	$t
LIBADD.$t+=	gtest_main
SRCS.$t+=	$t.cc
.endfor

.for t in ${GTEST_REQ_TESTS}
GTESTS+=	$t
LIBADD.$t+=	gtest
SRCS.$t+=	$t.cc
.endfor

SRCS.sample1_unittest+=	sample1.cc
SRCS.sample2_unittest+=	sample2.cc
SRCS.sample4_unittest+=	sample4.cc
SRCS.sample5_unittest+=	sample1.cc

.include <bsd.test.mk>
