# $FreeBSD$

.include <src.opts.mk>

TESTSDIR=	${TESTSBASE}/sys/capsicum

ATF_TESTS_C+=	bindat_connectat
ATF_TESTS_C+=	ioctls_test

CFLAGS+=	-I${SRCTOP}/tests

.if ${MK_GOOGLETEST} != no

.PATH: ${SRCTOP}/contrib/capsicum-test

GTESTS+=	capsicum-test
GTESTS_WRAPPER_SH.capsicum-test=	functional

SRCS.capsicum-test+=	\
	capsicum-test-main.cc \
	capsicum-test.cc \
	capability-fd.cc \
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

LIBADD.capsicum-test+=	gtest pthread
TEST_METADATA.capsicum-test=	required_user="unprivileged"

.for p in mini-me mini-me.noexec mini-me.setuid
PROGS+=		$p
NO_SHARED.$p=
SRCS.$p=	mini-me.c
.endfor

BINDIR=	${TESTSDIR}

BINMODE.mini-me.noexec=	${NOBINMODE}
BINMODE.mini-me.setuid=	4555

WARNS.capsicum-test=	3

.endif # MK_GOOGLETEST

.include <bsd.test.mk>
