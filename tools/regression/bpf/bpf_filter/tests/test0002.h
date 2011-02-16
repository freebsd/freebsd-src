/*-
 * Test 0002:	BPF_RET+BPF_K
 *
 * $FreeBSD: src/tools/regression/bpf/bpf_filter/tests/test0002.h,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_RET+BPF_K, 0xdeadc0de),
};

/* Packet */
u_char	pkt[] = {
	0x00,
};

/* Packet length seen on wire */
u_int	wirelen =	sizeof(pkt);

/* Packet length passed on buffer */
u_int	buflen =	sizeof(pkt);

/* Invalid instruction */
int	invalid =	0;

/* Expected return value */
u_int	expect =	0xdeadc0de;

/* Expected signal */
int	expect_signal =	0;
