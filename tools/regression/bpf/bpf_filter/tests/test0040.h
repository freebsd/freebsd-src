/*-
 * Test 0040:	BPF_ALU+BPF_NEG
 */

/* BPF program */
static struct bpf_insn	pc[] = {
	BPF_STMT(BPF_LD+BPF_IMM, 0x21523f22),
	BPF_STMT(BPF_ALU+BPF_NEG, 0),
	BPF_STMT(BPF_RET+BPF_A, 0),
};

/* Packet */
static u_char	pkt[] = {
	0x00,
};

/* Packet length seen on wire */
static u_int	wirelen =	sizeof(pkt);

/* Packet length passed on buffer */
static u_int	buflen =	sizeof(pkt);

/* Invalid instruction */
static int	invalid =	0;

/* Expected return value */
static u_int	expect =	0xdeadc0de;

/* Expected signal */
static int	expect_signal =	0;
