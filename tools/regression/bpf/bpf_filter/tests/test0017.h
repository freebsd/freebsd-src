/*-
 * Test 0017:	BPF_JMP+BPF_JGE+BPF_K
 */

/* BPF program */
static struct bpf_insn	pc[] = {
	BPF_STMT(BPF_LD+BPF_IMM, 0x01234567),
	BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 0x01234568, 2, 0),
	BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 0x01234567, 2, 1),
	BPF_STMT(BPF_LD+BPF_IMM, 0xdeadc0de),
	BPF_STMT(BPF_RET+BPF_A, 0),
	BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 0x01234566, 0, 1),
	BPF_STMT(BPF_LD+BPF_IMM, 0xc0decafe),
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
static u_int	expect =	0xc0decafe;

/* Expected signal */
static int	expect_signal =	0;
