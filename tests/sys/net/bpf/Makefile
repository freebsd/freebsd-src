.include <src.opts.mk>

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/net/bpf
BINDIR=		${TESTSDIR}

LIBADD+=	nv

PROGS=	bpf_multi_read pcap-test
LIBADD.bpf_multi_read+=	pcap
CFLAGS.pcap-test.c+=	-Wno-cast-align
LIBADD.pcap-test+=	pcap

ATF_TESTS_SH=	bpf

.include <bsd.test.mk>
