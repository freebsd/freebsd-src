/*-
 * Test 0070:	Check boundary conditions (BPF_LD+BPF_B+BPF_IND)
 */

/* BPF program */
static struct bpf_insn	pc[] = {
	BPF_STMT(BPF_LD+BPF_IMM, 0xdeadc0de),
	BPF_STMT(BPF_LDX+BPF_IMM, 1),
	BPF_STMT(BPF_LD+BPF_B+BPF_IND, 0),
	BPF_STMT(BPF_RET+BPF_A, 0),
};

/* Packet */
static u_char	pkt[] = {
	0x01, 0x23, 0x45,
};

/* Packet length seen on wire */
static u_int	wirelen =	sizeof(pkt);

/* Packet length passed on buffer */
static u_int	buflen =	0;

/* Invalid instruction */
static int	invalid =	0;

/* Expected return value */
static u_int	expect =	0;

/* Expected signal */
static int	expect_signal =	0;
