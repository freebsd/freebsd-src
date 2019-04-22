# $FreeBSD$

PACKAGE=	tests

ATF_TESTS_SH+= be_create

PROGS=		target_prog
SRCS_target_prog=	target_prog.c
BINDIR_target_prog=	${TESTSDIR}

LIBADD+= zfs
LIBADD+= nvpair
LIBADD+= be

CFLAGS+= -I${SRCTOP}/lib/libbe
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common

CFLAGS+= -DNEED_SOLARIS_BOOLEAN

.include <bsd.test.mk>
