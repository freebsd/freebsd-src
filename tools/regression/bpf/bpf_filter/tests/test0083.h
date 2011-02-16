/*-
 * Test 0083:	Check that the last instruction is BPF_RET.
 *
 * $FreeBSD: src/tools/regression/bpf/bpf_filter/tests/test0083.h,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_JUMP(BPF_JMP+BPF_JA, 0, 0, 0),
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
u_int	expect =	0;

/* Expected signal */
#ifdef BPF_JIT_COMPILER
int	expect_signal =	SIGSEGV;
#else
int	expect_signal =	SIGABRT;
#endif
