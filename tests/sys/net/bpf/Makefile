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

TEST_METADATA.bpf+=	execenv="jail" \
			execenv_jail_params="vnet allow.raw_sockets"

.include <bsd.test.mk>
