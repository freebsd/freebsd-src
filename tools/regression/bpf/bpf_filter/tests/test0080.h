/*-
 * Test 0080:	Check uninitialized scratch memory.
 *
 * Note:	This behavior is not guaranteed with bpf_filter(9).
 *
 * $FreeBSD$
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_LDX+BPF_IMM, 0xffffffff),
	BPF_STMT(BPF_LD+BPF_MEM, 0),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 30, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 1),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 28, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 2),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 26, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 3),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 24, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 4),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 22, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 5),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 20, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 6),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 18, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 7),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 16, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 8),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 14, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 9),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 12, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 10),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 10, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 11),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 8, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 12),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 6, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 13),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 4, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 14),
	BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 2, 0, 0),
	BPF_STMT(BPF_LD+BPF_MEM, 15),
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
int	invalid =	0;

/* Expected return value */
u_int	expect =	0;

/* Expected signal */
int	expect_signal =	0;
