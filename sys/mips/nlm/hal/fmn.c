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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/mmio.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/fmn.h>
#include <sys/systm.h>

uint32_t bad_xlp_num_nodes = 4;
/* XLP can take upto 16K of FMN messages per hardware queue, as spill.
* But, configuring all 16K causes the total spill memory to required
* to blow upto 192MB for single chip configuration, and 768MB in four
* chip configuration. Hence for now, we will setup the per queue spill
* as 1K FMN messages. With this, the total spill memory needed for 1024
* hardware queues (with 12bytes per single entry FMN message) becomes
* (1*1024)*12*1024queues = 12MB. For the four chip config, the memory
* needed = 12 * 4 = 48MB.
*/
uint64_t nlm_cms_spill_total_messages = 1 * 1024;

/* On a XLP832, we have the following FMN stations:
* CPU    stations: 8
* PCIE0  stations: 1
* PCIE1  stations: 1
* PCIE2  stations: 1
* PCIE3  stations: 1
* GDX    stations: 1
* CRYPTO stations: 1
* RSA    stations: 1
* CMP    stations: 1
* POE    stations: 1
* NAE    stations: 1
* ==================
* Total          : 18 stations per chip
*
* For all 4 nodes, there are 18*4 = 72 FMN stations
*/
uint32_t nlm_cms_total_stations = 18 * 4 /*xlp_num_nodes*/;
uint32_t cms_onchip_seg_availability[XLP_CMS_ON_CHIP_PER_QUEUE_SPACE];

int nlm_cms_verify_credit_config (int spill_en, int tot_credit)
{
	/* Note: In XLP there seem to be no mechanism to read back
	 * the credit count that has been programmed into a sid / did pair;
	 * since we have only one register 0x2000 to read.
	 * Hence it looks like all credit mgmt/verification needs to
	 * be done by software. Software could keep track of total credits
	 * getting programmed and verify it from this function.
	 */

	if (spill_en) {
		/* TODO */
	}

	if (tot_credit > (XLP_CMS_ON_CHIP_MESG_SPACE*bad_xlp_num_nodes))
		return 1; /* credits overflowed - should not happen */

	return 0;
}

/**
 * Takes inputs as node, queue_size and maximum number of queues.
 * Calculates the base, start & end and returns the same for a
 * defined qid.
 *
 * The output queues are maintained in the internal output buffer
 * which is a on-chip SRAM structure. For the actial hardware
 * internal implementation, It is a structure which consists
 * of eight banks of 4096-entry x message-width SRAMs. The SRAM
 * implementation is designed to run at 1GHz with a 1-cycle read/write
 * access. A read/write transaction can be initiated for each bank
 * every cycle for a total of eight accesses per cycle. Successive
 * entries of the same output queue are placed in successive banks.
 * This is done to spread different read & write accesses to same/different
 * output queue over as many different banks as possible so that they
 * can be scheduled concurrently. Spreading the accesses to as many banks
 * as possible to maximize the concurrency internally is important for
 * achieving the desired peak throughput. This is done by h/w implementation
 * itself.
 *
 * Output queues are allocated from this internal output buffer by
 * software. The total capacity of the output buffer is 32K-entry.
 * Each output queue can be sized from 32-entry to 1024-entry in
 * increments of 32-entry. This is done by specifying a Start & a
 * End pointer: pointers to the first & last 32-entry chunks allocated
 * to the output queue.
 *
 * To optimize the storage required for 1024 OQ pointers, the upper 5-bits
 * are shared by the Start & the End pointer. The side-effect of this
 * optimization is that an OQ can't cross a 1024-entry boundary. Also, the
 * lower 5-bits don't need to be specified in the Start & the End pointer
 * as the allocation is in increments of 32-entries.
 *
 * Queue occupancy is tracked by a Head & a Tail pointer. Tail pointer
 * indicates the location to which next entry will be written & Head
 * pointer indicates the location from which next entry will be read. When
 * these pointers reach the top of the allocated space (indicated by the
 * End pointer), they are reset to the bottom of the allocated space
 * (indicated by the Start pointer).
 *
 * Output queue pointer information:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *   14               10 9              5 4                 0
 *   ------------------
 *   | base ptr       |
 *   ------------------
 *                       ----------------
 *                       | start ptr    |
 *                       ----------------
 *                       ----------------
 *                       | end   ptr    |
 *                       ----------------
 *                       ------------------------------------
 *                       |           head ptr               |
 *                       ------------------------------------
 *                       ------------------------------------
 *                       |           tail ptr               |
 *                       ------------------------------------
 * Note:
 * A total of 1024 segments can sit on one software-visible "bank"
 * of internal SRAM. Each segment contains 32 entries. Also note
 * that sw-visible "banks" are not the same as the actual internal
 * 8-bank implementation of hardware. It is an optimization of
 * internal access.
 *
 */

void nlm_cms_setup_credits(uint64_t base, int destid, int srcid, int credit)
{
	uint32_t val;

	val = ((credit << 24) | (destid << 12) | (srcid << 0));
	nlm_wreg_cms(base, XLP_CMS_OUTPUTQ_CREDIT_CFG_REG, val);

}

int nlm_cms_config_onchip_queue (uint64_t base, uint64_t spill_base,
					int qid, int spill_en)
{

	/* Configure 32 as onchip queue depth */
	nlm_cms_alloc_onchip_q(base, qid, 1);

	/* Spill configuration */
	if (spill_en) {
		/* Configure 4*4KB = 16K as spill size */
		nlm_cms_alloc_spill_q(base, qid, spill_base, 4);
	}

#if 0
	/* configure credits for src cpu0, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CPU0_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src cpu1, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CPU1_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src cpu2, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CPU2_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src cpu3, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CPU3_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src cpu4, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CPU4_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src cpu5, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CPU5_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src cpu6, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CPU6_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src cpu7, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CPU7_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src pcie0, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_PCIE0_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src pcie1, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_PCIE1_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src pcie2, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_PCIE2_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src pcie3, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_PCIE3_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src dte, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_DTE_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src rsa_ecc, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_RSA_ECC_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src crypto, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CRYPTO_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src cmp, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_CMP_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src poe, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_POE_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));

	/* configure credits for src nae, on this queue */
	nlm_cms_setup_credits(base, qid, XLP_CMS_NAE_SRC_STID,
		XLP_CMS_DEFAULT_CREDIT(nlm_cms_total_stations,
			nlm_cms_spill_total_messages));
#endif

	return 0;
}

/*
 * base		- CMS module base address for this node.
 * qid		- is the output queue id otherwise called as vc id
 * spill_base   - is the 40-bit physical address of spill memory. Must be
		  4KB aligned.
 * nsegs	- No of segments where a "1" indicates 4KB. Spill size must be
 *                a multiple of 4KB.
 */
int nlm_cms_alloc_spill_q(uint64_t base, int qid, uint64_t spill_base,
				int nsegs)
{
	uint64_t queue_config;
	uint32_t spill_start;

	if(nsegs > XLP_CMS_MAX_SPILL_SEGMENTS_PER_QUEUE) {
		return 1;
	}

	queue_config = nlm_rdreg_cms(base,(XLP_CMS_OUTPUTQ_CONFIG_REG(qid)));

	spill_start = ((spill_base >> 12) & 0x3F);
	/* Spill configuration */
	queue_config = (((uint64_t)XLP_CMS_SPILL_ENA << 62) |
				(((spill_base >> 18) & 0x3FFFFF) << 27) |
				(spill_start + nsegs - 1) << 21 |
				(spill_start << 15));

	nlm_wreg_cms(base,(XLP_CMS_OUTPUTQ_CONFIG_REG(qid)),queue_config);

	return 0;
}

/*
 * base		- CMS module base address for this node.
 * qid		- is the output queue id otherwise called as vc id
 * nsegs	- No of segments where a "1" indicates 32 credits. On chip
 *                credits must be a multiple of 32.
 */
int nlm_cms_alloc_onchip_q(uint64_t base, int qid, int nsegs)
{
	static uint32_t curr_end = 0;
	uint64_t queue_config;
	int onchipbase, start, last;
	uint8_t i;

        if( ((curr_end + nsegs) > XLP_CMS_MAX_ONCHIP_SEGMENTS) ||
		(nsegs > XLP_CMS_ON_CHIP_PER_QUEUE_SPACE) ) {
		/* Invalid configuration */
                return 1;
        }
        if(((curr_end % 32) + nsegs - 1) <= 31) {
                onchipbase = (curr_end / 32);
                start  = (curr_end % 32);
                curr_end += nsegs;
        } else {
                onchipbase = (curr_end / 32) + 1;
                start  = 0;
                curr_end = ((onchipbase * 32) + nsegs);
        }
        last   = start + nsegs - 1;

	for(i = start;i <= last;i++) {
		if(cms_onchip_seg_availability[onchipbase] & (1 << i)) {
			/* Conflict!!! segment is already allocated */
			return 1;
		}
	}
	/* Update the availability bitmap as consumed */
	for(i = start; i <= last; i++) {
		cms_onchip_seg_availability[onchipbase] |= (1 << i);
	}

	queue_config = nlm_rdreg_cms(base,(XLP_CMS_OUTPUTQ_CONFIG_REG(qid)));

	/* On chip configuration */
	queue_config = (((uint64_t)XLP_CMS_QUEUE_ENA << 63) |
			((onchipbase & 0x1f) << 10) |
			((last & 0x1f) << 5) |
			(start & 0x1f));

	nlm_wreg_cms(base,(XLP_CMS_OUTPUTQ_CONFIG_REG(qid)),queue_config);

	return 0;
}

void nlm_cms_default_setup(int node, uint64_t spill_base, int spill_en,
				int popq_en)
{
	int j, k, vc;
	int queue;
	uint64_t base;

	base = nlm_regbase_cms(node);
	for(j=0; j<1024; j++) {
		printf("Qid:0x%04d Val:0x%016jx\n",j, (uintmax_t)nlm_cms_get_onchip_queue (base, j));
	}
	/* Enable all cpu push queues */
	for (j=0; j<XLP_MAX_CORES; j++)
		for (k=0; k<XLP_MAX_THREADS; k++)
			for (vc=0; vc<XLP_CMS_MAX_VCPU_VC; vc++) {
		/* TODO : remove this once SMP works */
		if( (j == 0) && (k == 0) )
			continue;
		queue = XLP_CMS_CPU_PUSHQ(node, j, k, vc);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable pcie 0 push queue */
	for (j=XLP_CMS_PCIE0_QID(0); j<XLP_CMS_PCIE0_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable pcie 1 push queue */
	for (j=XLP_CMS_PCIE1_QID(0); j<XLP_CMS_PCIE1_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable pcie 2 push queue */
	for (j=XLP_CMS_PCIE2_QID(0); j<XLP_CMS_PCIE2_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable pcie 3 push queue */
	for (j=XLP_CMS_PCIE3_QID(0); j<XLP_CMS_PCIE3_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable DTE push queue */
	for (j=XLP_CMS_DTE_QID(0); j<XLP_CMS_DTE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable RSA/ECC push queue */
	for (j=XLP_CMS_RSA_ECC_QID(0); j<XLP_CMS_RSA_ECC_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable crypto push queue */
	for (j=XLP_CMS_CRYPTO_QID(0); j<XLP_CMS_CRYPTO_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable CMP push queue */
	for (j=XLP_CMS_CMP_QID(0); j<XLP_CMS_CMP_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable POE push queue */
	for (j=XLP_CMS_POE_QID(0); j<XLP_CMS_POE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable NAE push queue */
	for (j=XLP_CMS_NAE_QID(0); j<XLP_CMS_NAE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_config_onchip_queue(base, spill_base, queue, spill_en);
	}

	/* Enable all pop queues */
	if (popq_en) {
		for (j=XLP_CMS_POPQ_QID(0); j<XLP_CMS_POPQ_MAXQID; j++) {
			queue = XLP_CMS_POPQ(node, j);
			nlm_cms_config_onchip_queue(base, spill_base, queue,
							spill_en);
		}
	}
}

uint64_t nlm_cms_get_onchip_queue (uint64_t base, int qid)
{
	return nlm_rdreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid));
}

void nlm_cms_set_onchip_queue (uint64_t base, int qid, uint64_t val)
{
	uint64_t rdval;

	rdval = nlm_rdreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid));
	rdval |= val;
	nlm_wreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid), rdval);
}

void nlm_cms_per_queue_level_intr(uint64_t base, int qid, int sub_type,
					int intr_val)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid));

	val |= (((uint64_t)sub_type<<54) |
		((uint64_t)intr_val<<56));

	nlm_wreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid), val);
}

void nlm_cms_level_intr(int node, int sub_type, int intr_val)
{
	int j, k, vc;
	int queue;
	uint64_t base;

	base = nlm_regbase_cms(node);
	/* setup level intr config on all cpu push queues */
	for (j=0; j<XLP_MAX_CORES; j++)
		for (k=0; k<XLP_MAX_THREADS; k++)
			for (vc=0; vc<XLP_CMS_MAX_VCPU_VC; vc++) {
		queue = XLP_CMS_CPU_PUSHQ(node, j, k, vc);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all pcie 0 push queue */
	for (j=XLP_CMS_PCIE0_QID(0); j<XLP_CMS_PCIE0_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all pcie 1 push queue */
	for (j=XLP_CMS_PCIE1_QID(0); j<XLP_CMS_PCIE1_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all pcie 2 push queue */
	for (j=XLP_CMS_PCIE2_QID(0); j<XLP_CMS_PCIE2_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all pcie 3 push queue */
	for (j=XLP_CMS_PCIE3_QID(0); j<XLP_CMS_PCIE3_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all DTE push queue */
	for (j=XLP_CMS_DTE_QID(0); j<XLP_CMS_DTE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all RSA/ECC push queue */
	for (j=XLP_CMS_RSA_ECC_QID(0); j<XLP_CMS_RSA_ECC_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all crypto push queue */
	for (j=XLP_CMS_CRYPTO_QID(0); j<XLP_CMS_CRYPTO_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all CMP push queue */
	for (j=XLP_CMS_CMP_QID(0); j<XLP_CMS_CMP_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all POE push queue */
	for (j=XLP_CMS_POE_QID(0); j<XLP_CMS_POE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all NAE push queue */
	for (j=XLP_CMS_NAE_QID(0); j<XLP_CMS_NAE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}

	/* setup level intr config on all pop queues */
	for (j=XLP_CMS_POPQ_QID(0); j<XLP_CMS_POPQ_MAXQID; j++) {
		queue = XLP_CMS_POPQ(node, j);
		nlm_cms_per_queue_level_intr(base, queue, sub_type, intr_val);
	}
}

void nlm_cms_per_queue_timer_intr(uint64_t base, int qid, int sub_type,
					int intr_val)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid));

	val |= (((uint64_t)sub_type<<49) |
		((uint64_t)intr_val<<51));

	nlm_wreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid), val);
}

void nlm_cms_timer_intr(int node, int en, int sub_type, int intr_val)
{
	int j, k, vc;
	int queue;
	uint64_t base;

	base = nlm_regbase_cms(node);
	/* setup timer intr config on all cpu push queues */
	for (j=0; j<XLP_MAX_CORES; j++)
		for (k=0; k<XLP_MAX_THREADS; k++)
			for (vc=0; vc<XLP_CMS_MAX_VCPU_VC; vc++) {
		queue = XLP_CMS_CPU_PUSHQ(node, j, k, vc);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all pcie 0 push queue */
	for (j=XLP_CMS_PCIE0_QID(0); j<XLP_CMS_PCIE0_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all pcie 1 push queue */
	for (j=XLP_CMS_PCIE1_QID(0); j<XLP_CMS_PCIE1_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all pcie 2 push queue */
	for (j=XLP_CMS_PCIE2_QID(0); j<XLP_CMS_PCIE2_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all pcie 3 push queue */
	for (j=XLP_CMS_PCIE3_QID(0); j<XLP_CMS_PCIE3_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all DTE push queue */
	for (j=XLP_CMS_DTE_QID(0); j<XLP_CMS_DTE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all RSA/ECC push queue */
	for (j=XLP_CMS_RSA_ECC_QID(0); j<XLP_CMS_RSA_ECC_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all crypto push queue */
	for (j=XLP_CMS_CRYPTO_QID(0); j<XLP_CMS_CRYPTO_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all CMP push queue */
	for (j=XLP_CMS_CMP_QID(0); j<XLP_CMS_CMP_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all POE push queue */
	for (j=XLP_CMS_POE_QID(0); j<XLP_CMS_POE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all NAE push queue */
	for (j=XLP_CMS_NAE_QID(0); j<XLP_CMS_NAE_MAXQID; j++) {
		queue = XLP_CMS_IO_PUSHQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}

	/* setup timer intr config on all pop queues */
	for (j=XLP_CMS_POPQ_QID(0); j<XLP_CMS_POPQ_MAXQID; j++) {
		queue = XLP_CMS_POPQ(node, j);
		nlm_cms_per_queue_timer_intr(base, queue, sub_type, intr_val);
	}
}

/* returns 1 if interrupt has been generated for this output queue */
int nlm_cms_outputq_intr_check(uint64_t base, int qid)
{
	uint64_t val;
	val = nlm_rdreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid));

	return ((val >> 59) & 0x1);
}

void nlm_cms_outputq_clr_intr(uint64_t base, int qid)
{
	uint64_t val;
	val = nlm_rdreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid));
	val |= (1ULL<<59);
	nlm_wreg_cms(base, XLP_CMS_OUTPUTQ_CONFIG_REG(qid), val);
}

void nlm_cms_illegal_dst_error_intr(uint64_t base, int en)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_MSG_CONFIG_REG);
	val |= (en<<8);
	nlm_wreg_cms(base, XLP_CMS_MSG_CONFIG_REG, val);
}

void nlm_cms_timeout_error_intr(uint64_t base, int en)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_MSG_CONFIG_REG);
	val |= (en<<7);
	nlm_wreg_cms(base, XLP_CMS_MSG_CONFIG_REG, val);
}

void nlm_cms_biu_error_resp_intr(uint64_t base, int en)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_MSG_CONFIG_REG);
	val |= (en<<6);
	nlm_wreg_cms(base, XLP_CMS_MSG_CONFIG_REG, val);
}

void nlm_cms_spill_uncorrectable_ecc_error_intr(uint64_t base, int en)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_MSG_CONFIG_REG);
	val |= (en<<5) | (en<<3);
	nlm_wreg_cms(base, XLP_CMS_MSG_CONFIG_REG, val);
}

void nlm_cms_spill_correctable_ecc_error_intr(uint64_t base, int en)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_MSG_CONFIG_REG);
	val |= (en<<4) | (en<<2);
	nlm_wreg_cms(base, XLP_CMS_MSG_CONFIG_REG, val);
}

void nlm_cms_outputq_uncorrectable_ecc_error_intr(uint64_t base, int en)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_MSG_CONFIG_REG);
	val |= (en<<1);
	nlm_wreg_cms(base, XLP_CMS_MSG_CONFIG_REG, val);
}

void nlm_cms_outputq_correctable_ecc_error_intr(uint64_t base, int en)
{
	uint64_t val;

	val = nlm_rdreg_cms(base, XLP_CMS_MSG_CONFIG_REG);
	val |= (en<<0);
	nlm_wreg_cms(base, XLP_CMS_MSG_CONFIG_REG, val);
}

uint64_t nlm_cms_network_error_status(uint64_t base)
{
	return nlm_rdreg_cms(base, XLP_CMS_MSG_ERR_REG);
}

int nlm_cms_get_net_error_code(uint64_t err)
{
	return ((err >> 12) & 0xf);
}

int nlm_cms_get_net_error_syndrome(uint64_t err)
{
	return ((err >> 32) & 0x1ff);
}

int nlm_cms_get_net_error_ramindex(uint64_t err)
{
	return ((err >> 44) & 0x7fff);
}

int nlm_cms_get_net_error_outputq(uint64_t err)
{
	return ((err >> 16) & 0xfff);
}

/*========================= FMN Tracing related APIs ================*/

void nlm_cms_trace_setup(uint64_t base, int en, uint64_t trace_base,
				uint64_t trace_limit, int match_dstid_en,
				int dst_id, int match_srcid_en, int src_id,
				int wrap)
{
	uint64_t val;

	nlm_wreg_cms(base, XLP_CMS_TRACE_BASE_ADDR_REG, trace_base);
	nlm_wreg_cms(base, XLP_CMS_TRACE_LIMIT_ADDR_REG, trace_limit);

	val = nlm_rdreg_cms(base, XLP_CMS_TRACE_CONFIG_REG);
	val |= (((uint64_t)match_dstid_en << 39) |
		((dst_id & 0xfff) << 24) |
		(match_srcid_en << 23) |
		((src_id & 0xfff) << 8) |
		(wrap << 1) |
		(en << 0));
	nlm_wreg_cms(base, XLP_CMS_MSG_CONFIG_REG, val);
}

void nlm_cms_endian_byte_swap (uint64_t base, int en)
{
	nlm_wreg_cms(base, XLP_CMS_MSG_ENDIAN_SWAP_REG, en);
}
