/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
static char rcsid[] =
    "@(#) $Header: gencode.c,v 1.33 92/05/22 16:38:39 mccanne Exp $ (LBL)";
#endif

#ifdef __STDC__
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <sys/time.h>
#include <net/bpf.h>

#include "interface.h"
#include "gencode.h"
#include "nametoaddr.h"
#include "extract.h"

#define JMP(c) ((c)|BPF_JMP|BPF_K)

extern struct bpf_insn *icode_to_fcode();
extern u_long net_mask();
static void init_linktype();

static int alloc_reg();
static void free_reg();

static struct block *root;

/*
 * We divy out chunks of memory rather than call malloc each time so
 * we don't have to worry about leaking memory.  It's probably
 * not a big deal if all this memory was wasted but it this ever
 * goes into a library that would probably not be a good idea.
 */
#define NCHUNKS 16
#define CHUNK0SIZE 1024
struct chunk {
	u_int n_left;
	void *m;
};

static struct chunk chunks[NCHUNKS];
static int cur_chunk;

static void *
newchunk(n)
	u_int n;
{
	struct chunk *cp;
	int k, size;
	
	/* XXX Round up to nearest long. */
	n = (n + sizeof(long) - 1) & ~(sizeof(long) - 1);

	cp = &chunks[cur_chunk];
	if (n > cp->n_left) {
		++cp, k = ++cur_chunk;
		if (k >= NCHUNKS)
			error("out of memory");
		size = CHUNK0SIZE << k;
		cp->m = (void *)malloc(size);
		bzero((char *)cp->m, size);
		cp->n_left = size;
		if (n > size)
			error("out of memory");
	}
	cp->n_left -= n;
	return (void *)((char *)cp->m + cp->n_left);
}

static void
freechunks()
{
	int i;

	for (i = 0; i < NCHUNKS; ++i)
		if (chunks[i].m)
			free(chunks[i].m);
}

static inline struct block *
new_block(code)
	int code;
{
	struct block *p;

	p = (struct block *)newchunk(sizeof(*p));
	p->s.code = code;
	p->head = p;

	return p;
}

static inline struct slist *
new_stmt(code)
	int code;
{
	struct slist *p;

	p = (struct slist *)newchunk(sizeof(*p));
	p->s.code = code;

	return p;
}

static struct block *
gen_retblk(v)
	int v;
{
	struct block *b = new_block(BPF_RET|BPF_K);

	b->s.k = v;
	return b;
}

static inline void
syntax()
{
	error("syntax error in filter expression");
}

static u_long netmask;

struct bpf_program *
parse(buf, Oflag, linktype, mask)
	char *buf;
	int Oflag;
	int linktype;
	u_long mask;
{
	extern int n_errors;
	static struct bpf_program F;
	struct bpf_insn *p;
	int len;

	netmask = mask;

	F.bf_insns = 0;
	F.bf_len = 0;

	lex_init(buf ? buf : "");
	init_linktype(linktype);
	yyparse();

	if (n_errors)
		syntax();

	if (root == 0)
		root = gen_retblk(snaplen);

	if (Oflag) {
		optimize(&root);
		if (root == 0 || 
		    (root->s.code == (BPF_RET|BPF_K) && root->s.k == 0))
			error("expression rejects all packets");
	}
	p = icode_to_fcode(root, &len);
	F.bf_insns = p;
	F.bf_len  = len;

	freechunks();
	return &F;
}

/*
 * Backpatch the blocks in 'list' to 'target'.  The 'sense' field indicates
 * which of the jt and jf fields has been resolved and which is a pointer
 * back to another unresolved block (or nil).  At least one of the fields
 * in each block is already resolved.
 */
static void
backpatch(list, target)
	struct block *list, *target;
{
	struct block *next;

	while (list) {
		if (!list->sense) {
			next = JT(list);
			JT(list) = target;
		} else {
			next = JF(list);
			JF(list) = target;
		}
		list = next;
	}
}

/*
 * Merge the lists in b0 and b1, using the 'sense' field to indicate
 * which of jt and jf is the link.
 */
static void
merge(b0, b1)
	struct block *b0, *b1;
{
	register struct block **p = &b0;

	/* Find end of list. */
	while (*p)
		p = !((*p)->sense) ? &JT(*p) : &JF(*p);

	/* Concatenate the lists. */
	*p = b1;
}

void
finish_parse(p)
	struct block *p;
{
	backpatch(p, gen_retblk(snaplen));
	p->sense = !p->sense;
	backpatch(p, gen_retblk(0));
	root = p->head;
}

void
gen_and(b0, b1)
	struct block *b0, *b1;
{
	backpatch(b0, b1->head);
	b0->sense = !b0->sense;
	b1->sense = !b1->sense;
	merge(b1, b0);
	b1->sense = !b1->sense;
	b1->head = b0->head;
}

void
gen_or(b0, b1)
	struct block *b0, *b1;
{
	b0->sense = !b0->sense;
	backpatch(b0, b1->head);
	b0->sense = !b0->sense;
	merge(b1, b0);
	b1->head = b0->head;
}

void
gen_not(b)
	struct block *b;
{
	b->sense = !b->sense;
}

static struct block *
gen_cmp(offset, size, v)
	u_int offset, size;
	long v;
{
	struct slist *s;
	struct block *b;

	s = new_stmt(BPF_LD|BPF_ABS|size);
	s->s.k = offset;

	b = new_block(JMP(BPF_JEQ));
	b->stmts = s;
	b->s.k = v;

	return b;
}

struct block *
gen_mcmp(offset, size, v, mask)
	u_int offset, size;
	long v;
	u_long mask;
{
	struct block *b = gen_cmp(offset, size, v);
	struct slist *s;

	if (mask != 0xffffffff) {
		s = new_stmt(BPF_ALU|BPF_AND|BPF_K);
		s->s.k = mask;
		b->stmts->next = s;
	}
	return b;
}

struct block *
gen_bcmp(offset, size, v)
	u_int offset;
	u_int size;
	u_char *v;
{
	struct block *b, *tmp;
	int k;

	b = 0;
	while (size >= 4) {
		k = size - 4;
		tmp = gen_cmp(offset + k, BPF_W, EXTRACT_LONG(&v[k]));
		if (b != 0)
			gen_and(b, tmp);
		b = tmp;
		size -= 4;
	}
	while (size >= 2) {
		k = size - 2;
		tmp = gen_cmp(offset + k, BPF_H, (long)EXTRACT_SHORT(&v[k]));
		if (b != 0)
			gen_and(b, tmp);
		b = tmp;
		size -= 2;
	}
	if (size > 0) {
		tmp = gen_cmp(offset, BPF_B, (long)v[0]);
		if (b != 0)
			gen_and(b, tmp);
		b = tmp;
	}
	return b;
}

/*
 * Various code contructs need to know the layout of the data link
 * layer.  These variables give the necessary offsets.  off_linktype
 * is set to -1 for no encapsulation, in which case, IP is assumed.
 */
static u_int off_linktype;
static u_int off_nl;
static int linktype;

static void
init_linktype(type)
	int type;
{
	linktype = type;

	switch (type) {

	case DLT_EN10MB:
		off_linktype = 12;
		off_nl = 14;
		return;

	case DLT_SLIP:
		/*
		 * SLIP doesn't have a link level type.  The 16 byte
		 * header is hacked into our SLIP driver.
		 */
		off_linktype = -1;
		off_nl = 16;
		return;

	case DLT_NULL:
		off_linktype = -1;
		off_nl = 0;
		return;

	case DLT_PPP:
		off_linktype = 2;
		off_nl = 4;
		return;

	case DLT_FDDI:
		off_linktype = 19;
		off_nl = 21;
		return;

	case DLT_IEEE802:
		off_linktype = 20;
		off_nl = 22;
		return;
	}
	error("unknown data link type 0x%x", linktype);
	/* NOTREACHED */
}

static struct block *
gen_uncond(rsense)
	int rsense;
{
	struct block *b;
	struct slist *s;

	s = new_stmt(BPF_LD|BPF_IMM);
	s->s.k = !rsense;
	b = new_block(JMP(BPF_JEQ));
	b->stmts = s;

	return b;
}

static inline struct block *
gen_true()
{
	return gen_uncond(1);
}

static inline struct block *
gen_false()
{
	return gen_uncond(0);
}

struct block *
gen_linktype(proto)
	int proto;
{
	switch (linktype) {
	case DLT_SLIP:
		if (proto == ETHERTYPE_IP)
			return gen_true();
		else
			return gen_false();

	case DLT_PPP:
		if (proto == ETHERTYPE_IP)
			proto = 0x0021;		/* XXX - need ppp.h defs */
		break;
	}
	return gen_cmp(off_linktype, BPF_H, (long)proto);
}

static struct block *
gen_hostop(addr, mask, dir, proto, src_off, dst_off)
	u_long addr;
	u_long mask;
	int dir, proto;
	u_int src_off, dst_off;
{
	struct block *b0, *b1;
	u_int offset;

	switch (dir) {

	case Q_SRC:
		offset = src_off;
		break;

	case Q_DST:
		offset = dst_off;
		break;

	case Q_AND:
		b0 = gen_hostop(addr, mask, Q_SRC, proto, src_off, dst_off);
		b1 = gen_hostop(addr, mask, Q_DST, proto, src_off, dst_off);
		gen_and(b0, b1);
		return b1;

	case Q_OR:
	case Q_DEFAULT:
		b0 = gen_hostop(addr, mask, Q_SRC, proto, src_off, dst_off);
		b1 = gen_hostop(addr, mask, Q_DST, proto, src_off, dst_off);
		gen_or(b0, b1);
		return b1;

	default:
		abort();
	}
	b0 = gen_linktype(proto);
	b1 = gen_mcmp(offset, BPF_W, (long)addr, mask);
	gen_and(b0, b1);
	return b1;
}

static struct block *
gen_ehostop(eaddr, dir)
	u_char *eaddr;
	int dir;
{
	struct block *b0, *b1;

	switch (dir) {
	case Q_SRC:
		return gen_bcmp(6, 6, eaddr);

	case Q_DST:
		return gen_bcmp(0, 6, eaddr);

	case Q_AND:
		b0 = gen_ehostop(eaddr, Q_SRC);
		b1 = gen_ehostop(eaddr, Q_DST);
		gen_and(b0, b1);
		return b1;
		
	case Q_DEFAULT:
	case Q_OR:
		b0 = gen_ehostop(eaddr, Q_SRC);
		b1 = gen_ehostop(eaddr, Q_DST);
		gen_or(b0, b1);
		return b1;
	}
	abort();
	/* NOTREACHED */
}

static struct block *
gen_host(addr, mask, proto, dir)
	u_long addr;
	u_long mask;
	int proto;
	int dir;
{
	struct block *b0, *b1;

	switch (proto) {

	case Q_DEFAULT:
		b0 = gen_host(addr, mask, Q_IP, dir);
		b1 = gen_host(addr, mask, Q_ARP, dir);
		gen_or(b0, b1);
		b0 = gen_host(addr, mask, Q_RARP, dir);
		gen_or(b1, b0);
		return b0;

	case Q_IP:
		return gen_hostop(addr, mask, dir, ETHERTYPE_IP, 
				  off_nl + 12, off_nl + 16);

	case Q_RARP:
		return gen_hostop(addr, mask, dir, ETHERTYPE_REVARP,
				  off_nl + 14, off_nl + 24);

	case Q_ARP:
		return gen_hostop(addr, mask, dir, ETHERTYPE_ARP,
				  off_nl + 14, off_nl + 24);

	case Q_TCP:
		error("'tcp' modifier applied to host");

	case Q_UDP:
		error("'udp' modifier applied to host");

	case Q_ICMP:
		error("'icmp' modifier applied to host");
	}
	abort();
	/* NOTREACHED */
}

static struct block *
gen_gateway(eaddr, alist, proto, dir)
	u_char *eaddr;
	u_long **alist;
	int proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	if (dir != 0)
		error("direction applied to 'gateway'");

	switch (proto) {
	case Q_DEFAULT:
	case Q_IP:
	case Q_ARP:
	case Q_RARP:
		b0 = gen_ehostop(eaddr, Q_OR);
		b1 = gen_host(**alist++, 0xffffffffL, proto, Q_OR);
		while (*alist) {
			tmp = gen_host(**alist++, 0xffffffffL, proto, Q_OR);
			gen_or(b1, tmp);
			b1 = tmp;
		}
		gen_not(b1);
		gen_and(b0, b1);
		return b1;
	}
	error("illegal modifier of 'gateway'");
	/* NOTREACHED */
}

struct block *
gen_proto_abbrev(proto)
	int proto;
{
	struct block *b0, *b1;

	switch (proto) {

	case Q_TCP:
		b0 = gen_linktype(ETHERTYPE_IP);
		b1 = gen_cmp(off_nl + 9, BPF_B, (long)IPPROTO_TCP);
		gen_and(b0, b1);
		break;
		
	case Q_UDP:
		b0 =  gen_linktype(ETHERTYPE_IP);
		b1 = gen_cmp(off_nl + 9, BPF_B, (long)IPPROTO_UDP);
		gen_and(b0, b1);
		break;

	case Q_ICMP:
		b0 =  gen_linktype(ETHERTYPE_IP);
		b1 = gen_cmp(off_nl + 9, BPF_B, (long)IPPROTO_ICMP);
		gen_and(b0, b1);
		break;

	case Q_IP:
		b1 =  gen_linktype(ETHERTYPE_IP);
		break;

	case Q_ARP:
		b1 =  gen_linktype(ETHERTYPE_ARP);
		break;

	case Q_RARP:
		b1 =  gen_linktype(ETHERTYPE_REVARP);
		break;

	case Q_LINK:
		error("link layer applied in wrong context");

	default:
		abort();
	}
	return b1;
}

static struct block *
gen_ipfrag()
{
	struct slist *s;
	struct block *b;

	/* not ip frag */
	s = new_stmt(BPF_LD|BPF_H|BPF_ABS);
	s->s.k = off_nl + 6;
	b = new_block(JMP(BPF_JSET));
	b->s.k = 0x1fff;
	b->stmts = s;
	gen_not(b);

	return b;
}

static struct block *
gen_portatom(off, v)
	int off;
	long v;
{
	struct slist *s;
	struct block *b;

	s = new_stmt(BPF_LDX|BPF_MSH|BPF_B);
	s->s.k = off_nl;

	s->next = new_stmt(BPF_LD|BPF_IND|BPF_H);
	s->next->s.k = off_nl + off;

	b = new_block(JMP(BPF_JEQ));
	b->stmts = s;
	b->s.k = v;

	return b;
}

struct block *
gen_portop(port, proto, dir)
	int port;
	int proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	/* ip proto 'proto' */
	tmp = gen_cmp(off_nl + 9, BPF_B, (long)proto);
	b0 = gen_ipfrag();
	gen_and(tmp, b0);

	switch (dir) {
	case Q_SRC:
		b1 = gen_portatom(0, (long)port);
		break;

	case Q_DST:
		b1 = gen_portatom(2, (long)port);
		break;

	case Q_OR:
	case Q_DEFAULT:
		tmp = gen_portatom(0, (long)port);
		b1 = gen_portatom(2, (long)port);
		gen_or(tmp, b1);
		break;

	case Q_AND:
		tmp = gen_portatom(0, (long)port);
		b1 = gen_portatom(2, (long)port);
		gen_and(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);

	return b1;
}

static struct block *
gen_port(port, ip_proto, dir)
	int port;
	int ip_proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	/* ether proto ip */
	b0 =  gen_linktype(ETHERTYPE_IP);
	
	switch (ip_proto) {
	case IPPROTO_UDP:
	case IPPROTO_TCP:
		b1 = gen_portop(port, ip_proto, dir);
		break;

	case PROTO_UNDEF:
		tmp = gen_portop(port, IPPROTO_TCP, dir);
		b1 = gen_portop(port, IPPROTO_UDP, dir);
		gen_or(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);
	return b1;
}

int
lookup_proto(name, proto)
	char *name;
	int proto;
{
	int v;

	switch (proto) {
	case Q_DEFAULT:
	case Q_IP:
		v = s_nametoproto(name);
		if (v == PROTO_UNDEF)
			error("unknown ip proto '%s'", name);
		break;

	case Q_LINK:
		/* XXX should look up h/w protocol type based on linktype */
		v = s_nametoeproto(name);
		if (v == PROTO_UNDEF)
			error("unknown ether proto '%s'", name);
		break;

	default:
		v = PROTO_UNDEF;
		break;
	}
	return v;
}

struct block *
gen_proto(v, proto, dir)
	int v;
	int proto;
	int dir;
{
	struct block *b0, *b1;

	if (dir != Q_DEFAULT)
		error("direction applied to 'proto'");
	
	switch (proto) {
	case Q_DEFAULT:
	case Q_IP:
		b0 = gen_linktype(ETHERTYPE_IP);
		b1 = gen_cmp(off_nl + 9, BPF_B, (long)v);
		gen_and(b0, b1);
		return b1;

	case Q_ARP:
		error("arp does not encapsulate another protocol");
		/* NOTREACHED */
		
	case Q_RARP:
		error("rarp does not encapsulate another protocol");
		/* NOTREACHED */

	case Q_LINK:
		return gen_linktype(v);

	case Q_UDP:
		error("'udp proto' is bogus");

	case Q_TCP:
		error("'tcp proto' is bogus");

	case Q_ICMP:
		error("'icmp proto' is bogus");
	}
	abort();
	/* NOTREACHED */
}
	
struct block *
gen_scode(name, q)
	char *name;
	struct qual q;
{
	int proto = q.proto;
	int dir = q.dir;
	u_char *eaddr;
	u_long mask, addr, **alist;
	struct block *b, *tmp;
	int port, real_proto;

	switch (q.addr) {

	case Q_NET:
		addr = s_nametonetaddr(name);
		if (addr == 0)
			error("unknown network '%s'", name);
		mask = net_mask(&addr);
		return gen_host(addr, mask, proto, dir);

	case Q_DEFAULT:
	case Q_HOST:
		if (proto == Q_LINK) {
			/* XXX Should lookup hw addr based on link layer */
			eaddr = ETHER_hostton(name);
			if (eaddr == 0)
				error("unknown ether host '%s'", name);
			return gen_ehostop(eaddr, dir);

		} else {
			alist = s_nametoaddr(name);
			if (alist == 0 || *alist == 0)
				error("uknown host '%s'", name);
			b = gen_host(**alist++, 0xffffffffL, proto, dir);
			while (*alist) {
				tmp = gen_host(**alist++, 0xffffffffL, 
					       proto, dir);
				gen_or(b, tmp);
				b = tmp;
			}
			return b;
		}

	case Q_PORT: 
		if (proto != Q_DEFAULT && proto != Q_UDP && proto != Q_TCP)
			error("illegal qualifier of 'port'");
		if (s_nametoport(name, &port, &real_proto) == 0)
			error("unknown port '%s'", name);
		if (proto == Q_UDP) {
			if (real_proto == IPPROTO_TCP)
				error("port '%s' is tcp", name);
			else
				/* override PROTO_UNDEF */
				real_proto = IPPROTO_UDP;
		}
		if (proto == Q_TCP) {
			if (real_proto == IPPROTO_UDP)
				error("port '%s' is udp", name);
			else
				/* override PROTO_UNDEF */
				real_proto = IPPROTO_TCP;
		}
		return gen_port(port, real_proto, dir);

	case Q_GATEWAY:
		eaddr = ETHER_hostton(name);
		if (eaddr == 0)
			error("unknown ether host: %s", name);
			
		alist = s_nametoaddr(name);
		if (alist == 0 || *alist == 0)
			error("uknown host '%s'", name);
		return gen_gateway(eaddr, alist, proto, dir);

	case Q_PROTO:
		real_proto = lookup_proto(name, proto);
		if (real_proto >= 0)
			return gen_proto(real_proto, proto, dir);
		else
			error("unknown protocol: %s", name);

	case Q_UNDEF:
		syntax();
		/* NOTREACHED */
	}
	abort();
	/* NOTREACHED */
}

struct block *
gen_ncode(v, q)
	u_long v;
	struct qual q;
{
	u_long mask;
	int proto = q.proto;
	int dir = q.dir;

	switch (q.addr) {

	case Q_DEFAULT:
	case Q_HOST:
	case Q_NET:
		mask = net_mask(&v);
		return gen_host(v, mask, proto, dir);

	case Q_PORT:
		if (proto == Q_UDP)
			proto = IPPROTO_UDP;
		else if (proto == Q_TCP)
			proto = IPPROTO_TCP;
		else if (proto == Q_DEFAULT)
			proto = PROTO_UNDEF;
		else
			error("illegal qualifier of 'port'");

		return gen_port((int)v, proto, dir);

	case Q_GATEWAY:
		error("'gateway' requires a name");
		/* NOTREACHED */

	case Q_PROTO:
		return gen_proto((int)v, proto, dir);

	case Q_UNDEF:
		syntax();
		/* NOTREACHED */
	}
	abort();
	/* NOTREACHED */
}

struct block *
gen_ecode(eaddr, q)
	u_char *eaddr;
	struct qual q;
{
	if ((q.addr == Q_HOST || q.addr == Q_DEFAULT) && q.proto == Q_LINK)
		return gen_ehostop(eaddr, (int)q.dir);
	else 
		error("ethernet address used in non-ether expression");
	/* NOTREACHED */
}

void
sappend(s0, s1)
	struct slist *s0, *s1;
{
	/*
	 * This is definitely not the best way to do this, but the
	 * lists will rarely get long.
	 */
	while (s0->next)
		s0 = s0->next;
	s0->next = s1;
}

struct slist *
xfer_to_x(a)
	struct arth *a;
{
	struct slist *s;

	s = new_stmt(BPF_LDX|BPF_MEM);
	s->s.k = a->regno;
	return s;
}

struct slist *
xfer_to_a(a)
	struct arth *a;
{
	struct slist *s;

	s = new_stmt(BPF_LD|BPF_MEM);
	s->s.k = a->regno;
	return s;
}

struct arth *
gen_load(proto, index, size)
	int proto;
	struct arth *index;
	int size;
{
	struct slist *s, *tmp;
	struct block *b;
	int regno = alloc_reg();

	free_reg(index->regno);
	switch (size) {

	default:
		error("data size must be 1, 2, or 4");

	case 1:
		size = BPF_B;
		break;

	case 2:
		size = BPF_H;
		break;

	case 4:
		size = BPF_W;
		break;
	}
	switch (proto) {
	default:
		error("unsupported index operation");
		
	case Q_LINK:
		s = xfer_to_x(index);
		tmp = new_stmt(BPF_LD|BPF_IND|size);
		sappend(s, tmp);
		sappend(index->s, s);
		break;

	case Q_IP:
	case Q_ARP:
	case Q_RARP:
		/* XXX Note that we assume a fixed link link header here. */
		s = xfer_to_x(index);
		tmp = new_stmt(BPF_LD|BPF_IND|size);
		tmp->s.k = off_nl;
		sappend(s, tmp);
		sappend(index->s, s);

		b = gen_proto_abbrev(proto);
		if (index->b)
			gen_and(index->b, b);
		index->b = b;
		break;

	case Q_TCP:
	case Q_UDP:
	case Q_ICMP:
		s = new_stmt(BPF_LDX|BPF_MSH|BPF_B);
		s->s.k = off_nl;
		sappend(s, xfer_to_a(index));
		sappend(s, new_stmt(BPF_ALU|BPF_ADD|BPF_X));
		sappend(s, new_stmt(BPF_MISC|BPF_TAX));
		sappend(s, tmp = new_stmt(BPF_LD|BPF_IND|size));
		tmp->s.k = off_nl;
		sappend(index->s, s);
		
		gen_and(gen_proto_abbrev(proto), b = gen_ipfrag());
		if (index->b)
			gen_and(index->b, b);
		index->b = b;
		break;
	}
	index->regno = regno;
	s = new_stmt(BPF_ST);
	s->s.k = regno;
	sappend(index->s, s);

	return index;
}

struct block *
gen_relation(code, a0, a1, reversed)
	int code;
	struct arth *a0, *a1;
	int reversed;
{
	struct slist *s0, *s1, *s2;
	struct block *b, *tmp;

	s0 = xfer_to_x(a1);
	s1 = xfer_to_a(a0);
	s2 = new_stmt(BPF_ALU|BPF_SUB|BPF_X);
	b = new_block(JMP(code));
	if (reversed)
		gen_not(b);

	sappend(s1, s2);
	sappend(s0, s1);
	sappend(a1->s, s0);
	sappend(a0->s, a1->s);

	b->stmts = a0->s;

	free_reg(a0->regno);
	free_reg(a1->regno);

	/* 'and' together protocol checks */
	if (a0->b) {
		if (a1->b) {
			gen_and(a0->b, tmp = a1->b);
		}
		else
			tmp = a0->b;
	} else 
		tmp = a1->b;

	if (tmp)
		gen_and(tmp, b);

	return b;
}

struct arth *
gen_loadlen()
{
	int regno = alloc_reg();
	struct arth *a = (struct arth *)newchunk(sizeof(*a));
	struct slist *s;

	s = new_stmt(BPF_LD|BPF_LEN);
	s->next = new_stmt(BPF_ST);
	s->next->s.k = regno;
	a->s = s;
	a->regno = regno;

	return a;
}

struct arth *
gen_loadi(val)
	int val;
{
	struct arth *a;
	struct slist *s;
	int reg;

	a = (struct arth *)newchunk(sizeof(*a));

	reg = alloc_reg();

	s = new_stmt(BPF_LD|BPF_IMM);
	s->s.k = val;
	s->next = new_stmt(BPF_ST);
	s->next->s.k = reg;
	a->s = s;
	a->regno = reg;

	return a;
}

struct arth *
gen_neg(a)
	struct arth *a;
{
	struct slist *s;

	s = xfer_to_a(a);
	sappend(a->s, s);
	s = new_stmt(BPF_ALU|BPF_NEG);
	s->s.k = 0;
	sappend(a->s, s);
	s = new_stmt(BPF_ST);
	s->s.k = a->regno;
	sappend(a->s, s);

	return a;
}

struct arth *
gen_arth(code, a0, a1)
	int code;
	struct arth *a0, *a1;
{
	struct slist *s0, *s1, *s2;

	s0 = xfer_to_x(a1);
	s1 = xfer_to_a(a0);
	s2 = new_stmt(BPF_ALU|BPF_X|code);

	sappend(s1, s2);
	sappend(s0, s1);
	sappend(a1->s, s0);
	sappend(a0->s, a1->s);
	
	free_reg(a1->regno);

	s0 = new_stmt(BPF_ST);
	a0->regno = s0->s.k = alloc_reg();
	sappend(a0->s, s0);

	return a0;
}

/*
 * Here we handle simple allocation of the scratch registers.
 * If too many registers are alloc'd, the allocator punts.
 */
static int regused[BPF_MEMWORDS];
static int curreg;

/*
 * Return the next free register.
 */
static int
alloc_reg()
{
	int n = BPF_MEMWORDS;

	while (--n >= 0) {
		if (regused[curreg])
			curreg = (curreg + 1) % BPF_MEMWORDS;
		else {
			regused[curreg] = 1;
			return curreg;
		}
	}
	error("too many registers needed to evaluate expression");
	/* NOTREACHED */
}

/*
 * Return a register to the table so it can
 * be used later.
 */
static void
free_reg(n)
	int n;
{
	regused[n] = 0;
}

static struct block *
gen_len(jmp, n)
	int jmp;
	int n;
{
	struct slist *s;
	struct block *b;

	s = new_stmt(BPF_LD|BPF_LEN);
	s->next = new_stmt(BPF_SUB|BPF_IMM);
	s->next->s.k = n;
	b = new_block(JMP(jmp));
	b->stmts = s;

	return b;
}

struct block *
gen_greater(n)
	int n;
{
	return gen_len(BPF_JGE, n);
}

struct block *
gen_less(n)
	int n;
{
	struct block *b;

	b = gen_len(BPF_JGT, n);
	gen_not(b);

	return b;
}

struct block *
gen_byteop(op, idx, val)
	int op;
	int idx;
	int val;
{
	struct block *b;
	struct slist *s;

	switch (op) {
	default:
		abort();

	case '=':
		return gen_cmp((u_int)idx, BPF_B, (long)val);
		
	case '<':
		b = gen_cmp((u_int)idx, BPF_B, (long)val);
		b->s.code = JMP(BPF_JGE);
		gen_not(b);
		return b;

	case '>':
		b = gen_cmp((u_int)idx, BPF_B, (long)val);
		b->s.code = JMP(BPF_JGT);
		return b;

	case '|':
		s = new_stmt(BPF_ALU|BPF_AND|BPF_K);
		break;

	case '&':
		s = new_stmt(BPF_ALU|BPF_AND|BPF_K);
		break;
	}
	s->s.k = val;
	b = new_block(JMP(BPF_JEQ));
	b->stmts = s;
	gen_not(b);

	return b;
}

struct block *
gen_broadcast(proto)
	int proto;
{
	u_long hostmask;
	struct block *b0, *b1, *b2;
	static u_char ebroadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	switch (proto) {

	case Q_DEFAULT:
	case Q_LINK:
		if (linktype == DLT_EN10MB)
			return gen_ehostop(ebroadcast, Q_DST);
		error("not a broadcast link");
		break;

	case Q_IP:
		b0 = gen_linktype(ETHERTYPE_IP);
		hostmask = ~netmask;
		b1 = gen_mcmp(off_nl + 16, BPF_W, (long)0, hostmask);
		b2 = gen_mcmp(off_nl + 16, BPF_W, 
			      (long)(~0 & hostmask), hostmask);
		gen_or(b1, b2);
		gen_and(b0, b2);
		return b2;
	}
	error("only ether/ip broadcast filters supported");
}

struct block *
gen_multicast(proto)
	int proto;
{
	register struct block *b0, *b1, *b2;
	register struct slist *s;

	switch (proto) {

	case Q_DEFAULT:
	case Q_LINK:
		if (linktype != DLT_EN10MB)
			break;

		/* ether[0] & 1 != 0 */
		s = new_stmt(BPF_LD|BPF_B|BPF_ABS);
		s->s.k = 0;
		b0 = new_block(JMP(BPF_JSET));
		b0->s.k = 1;
		b0->stmts = s;
		return b0;

	case Q_IP:
		b0 = gen_linktype(ETHERTYPE_IP);
		b1 = gen_cmp(off_nl + 16, BPF_B, (long)224);
		b1->s.code = JMP(BPF_JGE);
		gen_and(b0, b1);
		return b1;
	}
	error("only ether/ip multicast filters supported");
}
