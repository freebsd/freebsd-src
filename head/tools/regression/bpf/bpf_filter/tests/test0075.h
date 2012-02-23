/*-
 * Test 0075:	Check boundary conditions (BPF_LD+BPF_MEM)
 *
 * $FreeBSD$
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_LD+BPF_IMM, 0xdeadc0de),
	BPF_STMT(BPF_LD+BPF_MEM, 0x8fffffff),
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
#ifdef __amd64__
int	expect_signal =	SIGBUS;
#else
int	expect_signal =	SIGSEGV;
#endif
