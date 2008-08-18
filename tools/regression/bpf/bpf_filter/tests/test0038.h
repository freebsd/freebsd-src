/*-
 * Test 0038:	BPF_ALU|BPF_LSH|BPF_K
 *
 * $FreeBSD$
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_LD|BPF_IMM, 0xdefc0),
	BPF_STMT(BPF_ALU|BPF_LSH|BPF_K, 9),
	BPF_STMT(BPF_RET|BPF_A, 0),
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
u_int	expect =	0x1bdf8000;

/* Expeced signal */
int	expect_signal =	0;
