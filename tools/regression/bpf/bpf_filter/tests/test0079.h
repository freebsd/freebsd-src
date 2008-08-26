/*-
 * Test 0079:	An empty filter program.
 *
 * $FreeBSD$
 */

/* BPF program */
struct bpf_insn pc[] = {
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
u_int	expect =	(u_int)-1;

/* Expeced signal */
int	expect_signal =	0;
