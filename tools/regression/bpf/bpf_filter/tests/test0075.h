/*-
 * Test 0075:	Check boundary conditions (BPF_LD+BPF_MEM)
 *
 * $FreeBSD: src/tools/regression/bpf/bpf_filter/tests/test0075.h,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_LD+BPF_IMM, 0xdeadc0de),
	BPF_STMT(BPF_LD+BPF_MEM, 0xffffffff),
	BPF_STMT(BPF_RET+BPF_A, 0),
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
int	invalid =	1;

/* Expected return value */
u_int	expect =	0xdeadc0de;

/* Expected signal */
#ifdef BPF_JIT_COMPILER
int	expect_signal =	SIGSEGV;
#else
int	expect_signal =	SIGBUS;
#endif
