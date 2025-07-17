.include <src.opts.mk>

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/net/bpf
BINDIR=		${TESTSDIR}

LIBADD+=	nv

PROGS=	bpf_multi_read
LIBADD.bpf_multi_read+=	pcap

ATF_TESTS_SH=	bpf

.include <bsd.test.mk>
