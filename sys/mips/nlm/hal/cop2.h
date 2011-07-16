/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __NLM_COP2_H__
#define __NLM_COP2_H__

#define XLP_COP2_TX_BUF_REG		0
#define XLP_COP2_RX_BUF_REG		1
#define XLP_COP2_TXMSGSTATUS_REG	2
#define XLP_COP2_RXMSGSTATUS_REG	3
#define XLP_COP2_MSGSTATUS1_REG		4
#define XLP_COP2_MSGCONFIG_REG		5
#define XLP_COP2_MSGCONFIG1_REG		6

#define CROSSTHR_POPQ_EN		0x01
#define VC0_POPQ_EN			0x02
#define VC1_POPQ_EN			0x04
#define VC2_POPQ_EN			0x08
#define VC3_POPQ_EN			0x10
#define ALL_VC_POPQ_EN			0x1E
#define ALL_VC_CT_POPQ_EN		0x1F

struct nlm_fmn_msg {
	uint64_t msg[4];
};

#define NLM_DEFINE_COP2_ACCESSORS32(name, reg, sel)		\
static inline uint32_t nlm_read_c2_##name(void)			\
{								\
	uint32_t __rv;						\
	__asm__ __volatile__ (					\
	".set	push\n"						\
	".set	noreorder\n"					\
	".set	mips64\n"					\
	"mfc2	%0, $%1, %2\n"					\
	".set	pop\n"						\
	: "=r" (__rv)						\
	: "i" (reg), "i" (sel)					\
	);							\
	return __rv;						\
}								\
								\
static inline void nlm_write_c2_##name(uint32_t val)		\
{								\
	__asm__ __volatile__(					\
	".set	push\n"						\
	".set	noreorder\n"					\
	".set	mips64\n"					\
	"mtc2	%0, $%1, %2\n"					\
	".set	pop\n"						\
	:: "r" (val), "i" (reg), "i" (sel)			\
	);							\
} struct __hack

#if (__mips == 64)
#define NLM_DEFINE_COP2_ACCESSORS64(name, reg, sel)		\
static inline uint64_t nlm_read_c2_##name(void)			\
{								\
	uint64_t __rv;						\
	__asm__ __volatile__ (					\
	".set	push\n"						\
	".set	noreorder\n"					\
	".set	mips64\n"					\
	"dmfc2	%0, $%1, %2\n"					\
	".set	pop\n"						\
	: "=r" (__rv)						\
	: "i" (reg), "i" (sel) );				\
	return __rv;						\
}								\
								\
static inline void nlm_write_c2_##name(uint64_t val)		\
{								\
	__asm__ __volatile__ (					\
	".set	push\n"						\
	".set	noreorder\n"					\
	".set	mips64\n"					\
	"dmtc2	%0, $%1, %2\n"					\
	".set	pop\n"						\
	:: "r" (val), "i" (reg), "i" (sel) );			\
} struct __hack

#else

#define NLM_DEFINE_COP2_ACCESSORS64(name, reg, sel)		\
static inline uint64_t nlm_read_c2_##name(void)			\
{								\
	uint32_t __high, __low;					\
	__asm__ __volatile__ (					\
	".set	push\n"						\
	".set	noreorder\n"					\
	".set	mips64\n"					\
	"dmfc2	$8, $%2, %3\n"					\
	"dsra32	%0, $8, 0\n"					\
	"sll	%1, $8, 0\n"					\
	".set	pop\n"						\
	: "=r"(__high), "=r"(__low)				\
	: "i"(reg), "i"(sel)					\
	: "$8" );						\
								\
	return (((uint64_t)__high << 32) | __low);		\
}								\
								\
static inline void nlm_write_c2_##name(uint64_t val)		\
{								\
       uint32_t __high = val >> 32;				\
       uint32_t __low = val & 0xffffffff;			\
	__asm__ __volatile__ (					\
	".set	push\n"						\
	".set	noreorder\n"					\
	".set	mips64\n"					\
	"dsll32	$8, %1, 0\n"					\
	"dsll32	$9, %0, 0\n"					\
	"dsrl32	$8, $8, 0\n"					\
	"or	$8, $8, $9\n"					\
	"dmtc2	$8, $%2, %3\n"					\
	".set	pop\n"						\
	:: "r"(__high), "r"(__low),  "i"(reg), "i"(sel)		\
	:"$8", "$9");						\
} struct __hack

#endif

NLM_DEFINE_COP2_ACCESSORS64(txbuf0, XLP_COP2_TX_BUF_REG, 0);
NLM_DEFINE_COP2_ACCESSORS64(txbuf1, XLP_COP2_TX_BUF_REG, 1);
NLM_DEFINE_COP2_ACCESSORS64(txbuf2, XLP_COP2_TX_BUF_REG, 2);
NLM_DEFINE_COP2_ACCESSORS64(txbuf3, XLP_COP2_TX_BUF_REG, 3);

NLM_DEFINE_COP2_ACCESSORS64(rxbuf0, XLP_COP2_RX_BUF_REG, 0);
NLM_DEFINE_COP2_ACCESSORS64(rxbuf1, XLP_COP2_RX_BUF_REG, 1);
NLM_DEFINE_COP2_ACCESSORS64(rxbuf2, XLP_COP2_RX_BUF_REG, 2);
NLM_DEFINE_COP2_ACCESSORS64(rxbuf3, XLP_COP2_RX_BUF_REG, 3);

NLM_DEFINE_COP2_ACCESSORS32(txmsgstatus, XLP_COP2_TXMSGSTATUS_REG, 0);
NLM_DEFINE_COP2_ACCESSORS32(rxmsgstatus, XLP_COP2_RXMSGSTATUS_REG, 0);
NLM_DEFINE_COP2_ACCESSORS32(msgstatus1, XLP_COP2_MSGSTATUS1_REG, 0);
NLM_DEFINE_COP2_ACCESSORS32(msgconfig, XLP_COP2_MSGCONFIG_REG, 0);
NLM_DEFINE_COP2_ACCESSORS32(msgconfig1, XLP_COP2_MSGCONFIG1_REG, 0);

/* successful completion returns 1, else 0 */
static __inline__ int nlm_msgsend(int val)
{
	int result;
	__asm__ volatile (
		".set push			\n"
		".set noreorder			\n"
		".set mips64			\n"
		"move	$8, %1			\n"
		"sync				\n"
		"/* msgsnds	$9, $8 */	\n"
		".word	0x4a084801		\n"
		"move	%0, $9			\n"
		".set pop			\n"
		: "=r" (result)
		: "r" (val)
		: "$8", "$9"
	);
	return result;
}

static __inline__ int nlm_msgld(int vc)
{
	int val;
	__asm__ volatile (
		".set push			\n"
		".set noreorder			\n"
		".set mips64			\n"
		"move	$8, %1			\n"
		"/* msgld	$9, $8 */	\n"
		".word 0x4a084802		\n"
		"move	%0, $9			\n"
		".set pop			\n"
		: "=r" (val)
		: "r" (vc)
		: "$8", "$9"
	);
	return val;
}

static __inline__ void nlm_msgwait(int vc)
{
	__asm__ volatile (
		".set push			\n"
		".set noreorder			\n"
		".set mips64			\n"
		"move	$8, %0			\n"
		"/* msgwait	$8 */		\n"
		".word 0x4a080003		\n"
		".set pop			\n"
		:: "r" (vc)
		: "$8"
	);
}

/* TODO this is not needed in n32 and n64 */
static __inline uint32_t
nlm_fmn_saveflags(void)
{
	uint32_t sr = mips_rd_status();

	mips_wr_status((sr & ~MIPS_SR_INT_IE) | MIPS_SR_COP_2_BIT);
	return (sr);
}

static __inline void
nlm_fmn_restoreflags(uint32_t sr)
{

	mips_wr_status(sr);
}

static __inline__ int nlm_fmn_msgsend(int dstid, int size, int swcode,
		struct nlm_fmn_msg *m)
{
	uint32_t flags, status;
	int rv;

	size -= 1;
	flags = nlm_fmn_saveflags();
	switch(size) {
		case 3: nlm_write_c2_txbuf3(m->msg[3]);
		case 2: nlm_write_c2_txbuf2(m->msg[2]);
		case 1: nlm_write_c2_txbuf1(m->msg[1]);
		case 0: nlm_write_c2_txbuf0(m->msg[0]);
	}

	dstid |= ((swcode << 24) | (size << 16));
	status = nlm_msgsend(dstid);
	rv = !status;
	if (rv != 0)
		rv = nlm_read_c2_txmsgstatus();
	nlm_fmn_restoreflags(flags);

	return (rv);
}

static __inline__ int nlm_fmn_msgrcv(int vc, int *srcid, int *size, int *code,
    struct nlm_fmn_msg *m)
{
	uint32_t status;
	uint32_t msg_status, flags;
	int tmp_sz, rv;

	flags = nlm_fmn_saveflags();
	status = nlm_msgld(vc); /* will return 0, if error */
	rv = !status;
	if (rv == 0) {
		msg_status = nlm_read_c2_rxmsgstatus();
		*size = ((msg_status >> 26) & 0x3) + 1;
		*code = (msg_status >> 18) & 0xff;
		*srcid = (msg_status >> 4) & 0xfff;
		tmp_sz = *size - 1;
		switch(tmp_sz) {
			case 3: m->msg[3] = nlm_read_c2_rxbuf3();
			case 2: m->msg[2] = nlm_read_c2_rxbuf2();
			case 1: m->msg[1] = nlm_read_c2_rxbuf1();
			case 0: m->msg[0] = nlm_read_c2_rxbuf0();
		}
	}
	nlm_fmn_restoreflags(flags);

	return rv;
}

/**
 * nlm_fmn_cpu_init() initializes the per-h/w thread cop2 w.r.t the following
 * configuration parameters. It needs to be individually setup on each
 * hardware thread.
 *
 * int_vec - interrupt vector getting placed into msgconfig reg
 * ctpe - cross thread message pop enable. When set to 1, the thread (h/w cpu)
 *        associated where this cop2 register is setup, can pop messages
 *        intended for any other thread in the same core.
 * v0pe - VC0 pop message request mode enable. When set to 1, the thread
 * 	  can send pop requests to vc0.
 * v1pe - VC1 pop message request mode enable. When set to 1, the thread
 * 	  can send pop requests to vc1.
 * v2pe - VC2 pop message request mode enable. When set to 1, the thread
 * 	  can send pop requests to vc2.
 * v3pe - VC3 pop message request mode enable. When set to 1, the thread
 * 	  can send pop requests to vc3.
 */
static __inline__ void nlm_fmn_cpu_init(int int_vec, int ctpe, int v0pe,
    int v1pe, int v2pe, int v3pe)
{
	uint32_t val = nlm_read_c2_msgconfig();

	/* Note: in XLP PRM 0.8.1, the int_vec bits are un-documented
	 * in msgconfig register of cop2.
	 * As per chip/cpu RTL, [16:20] bits consist of int_vec.
	 */
	val |= ((int_vec & 0x1f) << 16) |
		((v3pe & 0x1) << 4) |
		((v2pe & 0x1) << 3) |
		((v1pe & 0x1) << 2) |
		((v0pe & 0x1) << 1) |
		(ctpe & 0x1);

	nlm_write_c2_msgconfig(val);
}
#endif
