.include <src.opts.mk>

TESTSDIR=	${TESTSBASE}/sys/capsicum

ATF_TESTS_C+=	bindat_connectat
ATF_TESTS_C+=	ioctls_test

CFLAGS+=	-I${SRCTOP}/tests

.if ${MK_GOOGLETEST} != no

GTESTS+=	capsicum-test_privileged
GTESTS+=	capsicum-test_unprivileged

# SRCS required for `capsicum-test*`.
SRCS.capsicum-test=	\
	capsicum-test-main.cc \
	capsicum-test.cc \
	capability-fd.cc \
	copy_file_range.cc \
	fexecve.cc \
	procdesc.cc \
	capmode.cc \
	fcntl.cc \
	ioctl.cc \
	openat.cc \
	sysctl.cc \
	select.cc \
	mqueue.cc \
	socket.cc \
	sctp.cc \
	capability-fd-pair.cc \
	overhead.cc \
	rename.cc

.for gtest in ${GTESTS}
SRCS.${gtest}=			${SRCS.capsicum-test}

LIBADD.${gtest}+=		gtest pthread procstat

# This test script runs the same test suite twice, once as root and once as an
# unprivileged user.  Serialize them since some tests access global namespaces,
# e.g., mqueuefs, and can trample on each other.
TEST_METADATA.${gtest}+=	is_exclusive="true"
TEST_METADATA.${gtest}+=	required_kmods="mqueuefs"
.endfor

TEST_METADATA.capsicum-test_privileged+=	required_user="root"
TEST_METADATA.capsicum-test_unprivileged+=	required_user="unprivileged"

.for p in mini-me mini-me.noexec mini-me.setuid
PROGS+=		$p
NO_SHARED.$p=
SRCS.$p=	mini-me.c
.endfor
.if ${MK_ASAN} != "no" || ${MK_UBSAN} != "no"
# mini-me.o is linked into a static binary so we can't use sanitizers.
# Note: We have to set CFLAGS here since it will be built as part of
# _PROGS_COMMON_OBJS and therefore NO_SHARED.$p does not disable ASAN/UBSAN.
CFLAGS.mini-me.c+=	-fno-sanitize=address -fno-sanitize=undefined
.endif

BINDIR=	${TESTSDIR}

BINMODE.mini-me.noexec=	${NOBINMODE}
BINMODE.mini-me.setuid=	4555

WARNS.capsicum-test=	3

.endif # MK_GOOGLETEST

.include <bsd.test.mk>
