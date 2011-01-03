/*
 * Copyright (c) 2006, 2007, 2008, 2009 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>

#include "qib.h"

/*
 * The size has to be longer than this string, so we can append
 * board/chip information to it in the init code.
 */
const char ib_qib_version[] = QIB_IDSTR "\n";

DEFINE_SPINLOCK(qib_devs_lock);
LIST_HEAD(qib_dev_list);
DEFINE_MUTEX(qib_mutex);	/* general driver use */

unsigned qib_debug;
module_param_named(debug, qib_debug, uint, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(debug, "mask for debug prints");

unsigned qib_ibmtu;
module_param_named(ibmtu, qib_ibmtu, uint, S_IRUGO);
MODULE_PARM_DESC(ibmtu, "Set max IB MTU (0=2KB, 1=256, 2=512, ... 5=4096");

unsigned qib_compat_ddr_negotiate = 1;
module_param_named(compat_ddr_negotiate, qib_compat_ddr_negotiate, uint,
		   S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(compat_ddr_negotiate,
		 "Attempt pre-IBTA 1.2 DDR speed negotiation");

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("QLogic <support@qlogic.com>");
MODULE_DESCRIPTION("QLogic IB driver");

/*
 * QIB_PIO_MAXIBHDR is the max IB header size allowed for in our
 * PIO send buffers.  This is well beyond anything currently
 * defined in the InfiniBand spec.
 */
#define QIB_PIO_MAXIBHDR 128

/*
 * QIB_MAX_PKT_RCV is the max # if packets processed per receive interrupt.
 */
#define QIB_MAX_PKT_RECV 64

struct qlogic_ib_stats qib_stats;

const char *qib_get_unit_name(int unit)
{
	static char iname[16];

	snprintf(iname, sizeof iname, "infinipath%u", unit);
	return iname;
}

/*
 * Return count of units with at least one port ACTIVE.
 */
int qib_count_active_units(void)
{
	struct qib_devdata *dd;
	struct qib_pportdata *ppd;
	unsigned long flags;
	int pidx, nunits_active = 0;

	spin_lock_irqsave(&qib_devs_lock, flags);
	list_for_each_entry(dd, &qib_dev_list, list) {
		if (!(dd->flags & QIB_PRESENT) || !dd->kregbase)
			continue;
		for (pidx = 0; pidx < dd->num_pports; ++pidx) {
			ppd = dd->pport + pidx;
			if (ppd->lid && (ppd->lflags & (QIBL_LINKINIT |
					 QIBL_LINKARMED | QIBL_LINKACTIVE))) {
				nunits_active++;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&qib_devs_lock, flags);
	return nunits_active;
}

/*
 * Return count of all units, optionally return in arguments
 * the number of usable (present) units, and the number of
 * ports that are up.
 */
int qib_count_units(int *npresentp, int *nupp)
{
	int nunits = 0, npresent = 0, nup = 0;
	struct qib_devdata *dd;
	unsigned long flags;
	int pidx;
	struct qib_pportdata *ppd;

	spin_lock_irqsave(&qib_devs_lock, flags);

	list_for_each_entry(dd, &qib_dev_list, list) {
		nunits++;
		if ((dd->flags & QIB_PRESENT) && dd->kregbase)
			npresent++;
		for (pidx = 0; pidx < dd->num_pports; ++pidx) {
			ppd = dd->pport + pidx;
			if (ppd->lid && (ppd->lflags & (QIBL_LINKINIT |
					 QIBL_LINKARMED | QIBL_LINKACTIVE)))
				nup++;
		}
	}

	spin_unlock_irqrestore(&qib_devs_lock, flags);

	if (npresentp)
		*npresentp = npresent;
	if (nupp)
		*nupp = nup;

	return nunits;
}

/**
 * qib_wait_linkstate - wait for an IB link state change to occur
 * @dd: the qlogic_ib device
 * @state: the state to wait for
 * @msecs: the number of milliseconds to wait
 *
 * wait up to msecs milliseconds for IB link state change to occur for
 * now, take the easy polling route.  Currently used only by
 * qib_set_linkstate.  Returns 0 if state reached, otherwise
 * -ETIMEDOUT state can have multiple states set, for any of several
 * transitions.
 */
int qib_wait_linkstate(struct qib_pportdata *ppd, u32 state, int msecs)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&ppd->lflags_lock, flags);
	if (ppd->state_wanted) {
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		ret = -EBUSY;
		goto bail;
	}
	ppd->state_wanted = state;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	wait_event_interruptible_timeout(ppd->state_wait,
					 (ppd->lflags & state),
					 msecs_to_jiffies(msecs));
	spin_lock_irqsave(&ppd->lflags_lock, flags);
	ppd->state_wanted = 0;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);

	if (!(ppd->lflags & state))
		ret = -ETIMEDOUT;
	else
		ret = 0;
bail:
	return ret;
}

int qib_set_linkstate(struct qib_pportdata *ppd, u8 newstate)
{
	u32 lstate;
	int ret;
	struct qib_devdata *dd = ppd->dd;
	unsigned long flags;

	switch (newstate) {
	case QIB_IB_LINKDOWN_ONLY:
		dd->f_set_ib_cfg(ppd, QIB_IB_CFG_LSTATE,
				 IB_LINKCMD_DOWN | IB_LINKINITCMD_NOP);
		/* don't wait */
		ret = 0;
		goto bail;

	case QIB_IB_LINKDOWN:
		dd->f_set_ib_cfg(ppd, QIB_IB_CFG_LSTATE,
				 IB_LINKCMD_DOWN | IB_LINKINITCMD_POLL);
		/* don't wait */
		ret = 0;
		goto bail;

	case QIB_IB_LINKDOWN_SLEEP:
		dd->f_set_ib_cfg(ppd, QIB_IB_CFG_LSTATE,
				 IB_LINKCMD_DOWN | IB_LINKINITCMD_SLEEP);
		/* don't wait */
		ret = 0;
		goto bail;

	case QIB_IB_LINKDOWN_DISABLE:
		dd->f_set_ib_cfg(ppd, QIB_IB_CFG_LSTATE,
				 IB_LINKCMD_DOWN | IB_LINKINITCMD_DISABLE);
		/* don't wait */
		ret = 0;
		goto bail;

	case QIB_IB_LINKARM:
		if (ppd->lflags & QIBL_LINKARMED) {
			qib_dbg("Asked for ARM, already there, skip\n");
			ret = 0;
			goto bail;
		}
		if (!(ppd->lflags & (QIBL_LINKINIT | QIBL_LINKACTIVE))) {
			qib_dbg("Asked for ARM, lflags %x, error\n",
				ppd->lflags);
			ret = -EINVAL;
			goto bail;
		}
		/*
		 * Since the port can be ACTIVE when we ask for ARMED,
		 * clear QIBL_LINKV so we can wait for a transition.
		 * If the link isn't ARMED, then something else happened
		 * and there is no point waiting for ARMED.
		 */
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags &= ~QIBL_LINKV;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		dd->f_set_ib_cfg(ppd, QIB_IB_CFG_LSTATE,
				 IB_LINKCMD_ARMED | IB_LINKINITCMD_NOP);
		lstate = QIBL_LINKV;
		break;

	case QIB_IB_LINKACTIVE:
		if (ppd->lflags & QIBL_LINKACTIVE) {
			qib_dbg("Asked for ACTIVE, already there, skip\n");
			ret = 0;
			goto bail;
		}
		if (!(ppd->lflags & QIBL_LINKARMED)) {
			qib_dbg("Asked for ACTIVE, lflags %x, error\n",
				ppd->lflags);
			ret = -EINVAL;
			goto bail;
		}
		dd->f_set_ib_cfg(ppd, QIB_IB_CFG_LSTATE,
				 IB_LINKCMD_ACTIVE | IB_LINKINITCMD_NOP);
		lstate = QIBL_LINKACTIVE;
		break;

	default:
		qib_dbg("Invalid linkstate 0x%x requested\n", newstate);
		ret = -EINVAL;
		goto bail;
	}
	ret = qib_wait_linkstate(ppd, lstate, 10);

bail:
	return ret;
}

/*
 * Get address of eager buffer from it's index (allocated in chunks, not
 * contiguous).
 */
static inline void *qib_get_egrbuf(const struct qib_ctxtdata *rcd, u32 etail)
{
	const u32 chunk = etail / rcd->rcvegrbufs_perchunk;
	const u32 idx =  etail % rcd->rcvegrbufs_perchunk;

	return rcd->rcvegrbuf[chunk] + idx * rcd->dd->rcvegrbufsize;
}

/**
 * get_rhf_errstring - decode RHF errors
 * @err: the err number
 * @msg: the output buffer
 * @len: the length of the output buffer
 *
 * only used one place now, may want more later
 */
static void get_rhf_errstring(u32 err, char *msg, size_t len)
{
	/* if no errors, and so don't need to check what's first */
	*msg = '\0';

	if (err & QLOGIC_IB_RHF_H_ICRCERR)
		strlcat(msg, "icrcerr ", len);
	if (err & QLOGIC_IB_RHF_H_VCRCERR)
		strlcat(msg, "vcrcerr ", len);
	if (err & QLOGIC_IB_RHF_H_PARITYERR)
		strlcat(msg, "parityerr ", len);
	if (err & QLOGIC_IB_RHF_H_LENERR)
		strlcat(msg, "lenerr ", len);
	if (err & QLOGIC_IB_RHF_H_MTUERR)
		strlcat(msg, "mtuerr ", len);
	if (err & QLOGIC_IB_RHF_H_IHDRERR)
		/* qlogic_ib hdr checksum error */
		strlcat(msg, "qibhdrerr ", len);
	if (err & QLOGIC_IB_RHF_H_TIDERR)
		strlcat(msg, "tiderr ", len);
	if (err & QLOGIC_IB_RHF_H_MKERR)
		/* bad ctxt, offset, etc. */
		strlcat(msg, "invalid qibhdr ", len);
	if (err & QLOGIC_IB_RHF_H_IBERR)
		strlcat(msg, "iberr ", len);
	if (err & QLOGIC_IB_RHF_L_SWA)
		strlcat(msg, "swA ", len);
	if (err & QLOGIC_IB_RHF_L_SWB)
		strlcat(msg, "swB ", len);
}

/*
 * Returns 1 if error was a CRC, else 0.
 * Needed for some chip's synthesized error counters.
 */
static u32 qib_rcv_hdrerr(struct qib_ctxtdata *rcd, struct qib_pportdata *ppd,
			  u32 ctxt, u32 eflags, u32 l, u32 etail,
			  __le32 *rhf_addr, struct qib_message_header *rhdr)
{
	char emsg[128];
	u32 ret = 0;

	get_rhf_errstring(eflags, emsg, sizeof emsg);
	qib_cdbg(ERRPKT, "IB%u:%u ctxt %u RHF %x qtail=%x typ=%u "
		 "tlen=%x opcode=%x egridx=%x: %s\n", ppd->dd->unit,
		 ppd->port, ctxt, eflags, l, qib_hdrget_rcv_type(rhf_addr),
		 qib_hdrget_length_in_bytes(rhf_addr),
		 be32_to_cpu(rhdr->bth[0]) >> 24, etail, emsg);

	if (eflags & (QLOGIC_IB_RHF_H_ICRCERR | QLOGIC_IB_RHF_H_VCRCERR))
		ret = 1;
	else if (eflags == QLOGIC_IB_RHF_H_TIDERR) {
		/* For TIDERR and RC QPs premptively schedule a NAK */
		struct qib_ib_header *hdr = (struct qib_ib_header *) rhdr;
		struct qib_other_headers *ohdr = NULL;
		struct qib_ibport *ibp = &ppd->ibport_data;
		struct qib_qp *qp = NULL;
		u32 tlen = qib_hdrget_length_in_bytes(rhf_addr);
		u16 lid  = be16_to_cpu(hdr->lrh[1]);
		int lnh = be16_to_cpu(hdr->lrh[0]) & 3;
		u32 qp_num;
		u32 opcode;
		u32 psn;
		int diff;
		unsigned long flags;

		/* Sanity check packet */
		if (tlen < 24)
			goto drop;

		if (lid < QIB_MULTICAST_LID_BASE) {
			lid &= ~((1 << ppd->lmc) - 1);
			if (unlikely(lid != ppd->lid))
				goto drop;
		}

		/* Check for GRH */
		if (lnh == QIB_LRH_BTH)
			ohdr = &hdr->u.oth;
		else if (lnh == QIB_LRH_GRH) {
			u32 vtf;

			ohdr = &hdr->u.l.oth;
			if (hdr->u.l.grh.next_hdr != IB_GRH_NEXT_HDR)
				goto drop;
			vtf = be32_to_cpu(hdr->u.l.grh.version_tclass_flow);
			if ((vtf >> IB_GRH_VERSION_SHIFT) != IB_GRH_VERSION)
				goto drop;
		} else
			goto drop;

		/* Get opcode and PSN from packet */
		opcode = be32_to_cpu(ohdr->bth[0]);
		opcode >>= 24;
		psn = be32_to_cpu(ohdr->bth[2]);

		/* Get the destination QP number. */
		qp_num = be32_to_cpu(ohdr->bth[1]) & QIB_QPN_MASK;
		if (qp_num != QIB_MULTICAST_QPN) {
			int ruc_res;
			qp = qib_lookup_qpn(ibp, qp_num);
			if (!qp)
				goto drop;

			/*
			 * Handle only RC QPs - for other QP types drop error
			 * packet.
			 */
			spin_lock(&qp->r_lock);

			/* Check for valid receive state. */
			if (!(ib_qib_state_ops[qp->state] &
			      QIB_PROCESS_RECV_OK)) {
				ibp->n_pkt_drops++;
				goto unlock;
			}

			switch (qp->ibqp.qp_type) {
			case IB_QPT_RC:
				spin_lock_irqsave(&qp->s_lock, flags);
				ruc_res =
					qib_ruc_check_hdr(
						ibp, hdr,
						lnh == QIB_LRH_GRH,
						qp,
						be32_to_cpu(ohdr->bth[0]));
				if (ruc_res) {
					spin_unlock_irqrestore(&qp->s_lock,
							       flags);
					goto unlock;
				}
				spin_unlock_irqrestore(&qp->s_lock, flags);

				/* Only deal with RDMA Writes for now */
				if (opcode <
				    IB_OPCODE_RC_RDMA_READ_RESPONSE_FIRST) {
					diff = qib_cmp24(psn, qp->r_psn);
					if (!qp->r_nak_state && diff >= 0) {
						qib_cdbg(ERRPKT,
							 "Sending Premptive"
							 "Seq NAK %u for PSN"
							 " %u when expected "
							 " PSN %u. Opcode: "
							 "%u\n",
							 ibp->n_rc_seqnak,
							 psn, qp->r_psn,
							 opcode);
						ibp->n_rc_seqnak++;
						qp->r_nak_state =
							IB_NAK_PSN_ERROR;
						/* Use the expected PSN. */
						qp->r_ack_psn = qp->r_psn;
						/*
						 * Wait to send the sequence
						 * NAK until all packets
						 * in the receive queue have
						 * been processed.
						 * Otherwise, we end up
						 * propagating congestion.
						 */
						if (list_empty(&qp->rspwait)) {
							qp->r_flags |=
								QIB_R_RSP_NAK;
							atomic_inc(&qp->refcount);
							list_add_tail(&qp->rspwait,
								      &rcd->qp_wait_list);
						}
					} /* Out of sequence NAK */
				} /* QP Request NAKs */
				break;
			case IB_QPT_SMI:
			case IB_QPT_GSI:
			case IB_QPT_UD:
			case IB_QPT_UC:
			default:
				/* For now don't handle any other QP types */
				break;
			}

unlock:
			spin_unlock(&qp->r_lock);
			/*
			 * Notify qib_destroy_qp() if it is waiting
			 * for us to finish.
			 */
			if (atomic_dec_and_test(&qp->refcount))
				wake_up(&qp->wait);
		} /* Unicast QP */
	} /* Valid packet with TIDErr */

drop:
	return ret;
}

/*
 * qib_kreceive - receive a packet
 * @rcd: the qlogic_ib context
 * @llic: gets count of good packets needed to clear lli,
 *          (used with chips that need need to track crcs for lli)
 *
 * called from interrupt handler for errors or receive interrupt
 * Returns number of CRC error packets, needed by some chips for
 * local link integrity tracking.   crcs are adjusted down by following
 * good packets, if any, and count of good packets is also tracked.
 */
u32 qib_kreceive(struct qib_ctxtdata *rcd, u32 *llic, u32 *npkts)
{
	struct qib_devdata *dd = rcd->dd;
	struct qib_pportdata *ppd = rcd->ppd;
	__le32 *rhf_addr;
	void *ebuf;
	const u32 rsize = dd->rcvhdrentsize;        /* words */
	const u32 maxcnt = dd->rcvhdrcnt * rsize;   /* words */
	u32 etail = -1, l, hdrqtail;
	struct qib_message_header *hdr;
	u32 eflags, etype, tlen, i = 0, updegr = 0, crcs = 0;
	int last;
	u64 lval;
	struct qib_qp *qp, *nqp;

	l = rcd->head;
	rhf_addr = (__le32 *) rcd->rcvhdrq + l + dd->rhf_offset;
	if (dd->flags & QIB_NODMA_RTAIL) {
		u32 seq = qib_hdrget_seq(rhf_addr);
		if (seq != rcd->seq_cnt) {
			qib_cdbg(PKT, "ctxt%u: hdrq seq diff @ hdrqhd %x\n",
				 rcd->ctxt, rcd->head);
			goto bail;
		}
		hdrqtail = 0;
	} else {
		hdrqtail = qib_get_rcvhdrtail(rcd);
		if (l == hdrqtail) {
			qib_cdbg(PKT, "ctxt%u: no pkts tail==hdrqhd %x\n",
				 rcd->ctxt, rcd->head);
			goto bail;
		}
		smp_rmb();  /* prevent speculative reads of dma'ed hdrq */
	}

	for (last = 0, i = 1; !last; i += !last) {
		hdr = dd->f_get_msgheader(dd, rhf_addr);
		eflags = qib_hdrget_err_flags(rhf_addr);
		etype = qib_hdrget_rcv_type(rhf_addr);
		/* total length */
		tlen = qib_hdrget_length_in_bytes(rhf_addr);
		ebuf = NULL;
		if ((dd->flags & QIB_NODMA_RTAIL) ?
		    qib_hdrget_use_egr_buf(rhf_addr) :
		    (etype != RCVHQ_RCV_TYPE_EXPECTED)) {
			etail = qib_hdrget_index(rhf_addr);
			updegr = 1;
			if (tlen > sizeof(*hdr) ||
			    etype >= RCVHQ_RCV_TYPE_NON_KD)
				ebuf = qib_get_egrbuf(rcd, etail);
		}
		if (!eflags) {
			u16 lrh_len = be16_to_cpu(hdr->lrh[2]) << 2;

			if (lrh_len != tlen) {
				qib_dbg("IB%u:%u ctxt %u lrh_len %u "
					"!= tlen %u\n",
					dd->unit, ppd->port, rcd->ctxt,
					lrh_len, tlen);
				qib_stats.sps_lenerrs++;
				goto move_along;
			}
		}
		if (etype == RCVHQ_RCV_TYPE_NON_KD && !eflags &&
		    ebuf == NULL &&
		    tlen > (dd->rcvhdrentsize - 2 + 1 -
				qib_hdrget_offset(rhf_addr)) << 2) {
			qib_dbg("IB%d ctxt %u NULL data rhf %08x%08x tlen %u\n",
				ppd->port, rcd->ctxt,
				le32_to_cpu(rhf_addr[1]),
				le32_to_cpu(rhf_addr[0]), tlen);
			goto move_along;
		}

		/*
		 * Both tiderr and qibhdrerr are set for all plain IB
		 * packets; only qibhdrerr should be set.
		 */

		if (etype != RCVHQ_RCV_TYPE_NON_KD &&
		    etype != RCVHQ_RCV_TYPE_ERROR &&
		    qib_hdrget_qib_ver(hdr->iph.ver_ctxt_tid_offset) !=
		    IPS_PROTO_VERSION)
			qib_cdbg(ERRPKT, "Bad InfiniPath protocol version "
				 "%x\n", etype);

		if (unlikely(eflags))
			crcs += qib_rcv_hdrerr(rcd, ppd, rcd->ctxt, eflags, l,
					       etail, rhf_addr, hdr);
		else if (etype == RCVHQ_RCV_TYPE_NON_KD) {
			qib_ib_rcv(rcd, hdr, ebuf, tlen);
			if (crcs)
				crcs--;
			else if (llic && *llic)
				--*llic;
		} else if (etype == RCVHQ_RCV_TYPE_EXPECTED)
			qib_cdbg(ERRPKT, "type=Expected pkt, no err bits\n");
		else if (etype == RCVHQ_RCV_TYPE_EAGER) {
			u8 opcode = be32_to_cpu(hdr->bth[0]) >> 24;
			u32 qpn = be32_to_cpu(hdr->bth[1]) & 0xffffff;

			qib_cdbg(RVPKT, "typ %x, opcode %x (eager, "
				 "qp=%x), len %x; ignored\n", etype,
				 opcode, qpn, tlen);
		} else {
			/*
			 * Error packet, type of error unknown.
			 * Probably type 3, but we don't know, so don't
			 * even try to print the opcode, etc.
			 * Usually caused by a "bad packet", that has no
			 * BTH, when the LRH says it should, or it's
			 * a KD packet with an invalid KDETH.
			 */
			qib_cdbg(ERRPKT, "Error Pkt, but no eflags! egrbuf"
				 " %x, len %x hdrq+%x rhf: %Lx\n", etail,
				 tlen, l, le64_to_cpu(*(__le64 *) rhf_addr));
			if (qib_debug & __QIB_ERRPKTDBG) {
				u32 j, *d, dw = rsize - 2;

				if (rsize > (tlen >> 2))
					dw = tlen >> 2;
				d = (u32 *)hdr;
				printk(KERN_DEBUG "EPkt rcvhdr(%x dw):\n",
				       dw);
				for (j = 0; j < dw; j++)
					printk(KERN_DEBUG "%8x%s", d[j],
						(j%8) == 7 ? "\n" : " ");
				printk(KERN_DEBUG ".\n");
			}
		}
move_along:
		l += rsize;
		if (l >= maxcnt)
			l = 0;
		if (i == QIB_MAX_PKT_RECV)
			last = 1;

		rhf_addr = (__le32 *) rcd->rcvhdrq + l + dd->rhf_offset;
		if (dd->flags & QIB_NODMA_RTAIL) {
			u32 seq = qib_hdrget_seq(rhf_addr);

			if (++rcd->seq_cnt > 13)
				rcd->seq_cnt = 1;
			if (seq != rcd->seq_cnt)
				last = 1;
		} else if (l == hdrqtail)
			last = 1;
		/*
		 * Update head regs etc., every 16 packets, if not last pkt,
		 * to help prevent rcvhdrq overflows, when many packets
		 * are processed and queue is nearly full.
		 * Don't request an interrupt for intermediate updates.
		 */
		lval = l;
		if (!last && !(i & 0xf)) {
			dd->f_update_usrhead(rcd, lval, updegr, etail, i);
			updegr = 0;
		}
	}

	rcd->head = l;
	rcd->pkt_count += i;

	/*
	 * Iterate over all QPs waiting to respond.
	 * The list won't change since the IRQ is only run on one CPU.
	 */
	list_for_each_entry_safe(qp, nqp, &rcd->qp_wait_list, rspwait) {
		list_del_init(&qp->rspwait);
		if (qp->r_flags & QIB_R_RSP_NAK) {
			qp->r_flags &= ~QIB_R_RSP_NAK;
			qib_send_rc_ack(qp);
		}
		if (qp->r_flags & QIB_R_RSP_SEND) {
			unsigned long flags;

			qp->r_flags &= ~QIB_R_RSP_SEND;
			spin_lock_irqsave(&qp->s_lock, flags);
			if (ib_qib_state_ops[qp->state] &
					QIB_PROCESS_OR_FLUSH_SEND)
				qib_schedule_send(qp);
			spin_unlock_irqrestore(&qp->s_lock, flags);
		}
		if (atomic_dec_and_test(&qp->refcount))
			wake_up(&qp->wait);
	}

	qib_cdbg(PKT, "IB%d ctxt %d handled %u packets\n",
		 ppd->port, rcd->ctxt, i);
bail:
	/* Report number of packets consumed */
	if (npkts)
		*npkts = i;

	/*
	 * Always write head at end, and setup rcv interrupt, even
	 * if no packets were processed.
	 */
	lval = (u64)rcd->head | dd->rhdrhead_intr_off;
	dd->f_update_usrhead(rcd, lval, updegr, etail, i);
	return crcs;
}

/**
 * qib_set_mtu - set the MTU
 * @ppd: the perport data
 * @arg: the new MTU
 *
 * We can handle "any" incoming size, the issue here is whether we
 * need to restrict our outgoing size.   For now, we don't do any
 * sanity checking on this, and we don't deal with what happens to
 * programs that are already running when the size changes.
 * NOTE: changing the MTU will usually cause the IBC to go back to
 * link INIT state...
 */
int qib_set_mtu(struct qib_pportdata *ppd, u16 arg)
{
	u32 piosize;
	int ret, chk;

	if (arg != 256 && arg != 512 && arg != 1024 && arg != 2048 &&
	    arg != 4096) {
		qib_dbg("Trying to set invalid mtu %u, failing\n", arg);
		ret = -EINVAL;
		goto bail;
	}
	chk = ib_mtu_enum_to_int(qib_ibmtu);
	if (chk > 0 && arg > chk) {
		qib_dbg("Trying to set mtu %u > ibmtu cap %u, failing\n",
			arg, chk);
		ret = -EINVAL;
		goto bail;
	}

	piosize = ppd->ibmaxlen;
	ppd->ibmtu = arg;

	if (arg >= (piosize - QIB_PIO_MAXIBHDR)) {
		/* Only if it's not the initial value (or reset to it) */
		if (piosize != ppd->init_ibmaxlen) {
			if (arg > piosize && arg <= ppd->init_ibmaxlen)
				piosize = ppd->init_ibmaxlen - 2 * sizeof(u32);
			ppd->ibmaxlen = piosize;
		}
	} else if ((arg + QIB_PIO_MAXIBHDR) != ppd->ibmaxlen) {
		piosize = arg + QIB_PIO_MAXIBHDR - 2 * sizeof(u32);
		ppd->ibmaxlen = piosize;
		qib_cdbg(VERBOSE, "ibmaxlen was 0x%x, setting to 0x%x "
			   "(mtu 0x%x)\n", ppd->ibmaxlen, piosize,
			   arg);
	}

	ppd->dd->f_set_ib_cfg(ppd, QIB_IB_CFG_MTU, 0);

	ret = 0;

bail:
	return ret;
}

int qib_set_lid(struct qib_pportdata *ppd, u32 lid, u8 lmc)
{
	struct qib_devdata *dd = ppd->dd;
	ppd->lid = lid;
	ppd->lmc = lmc;

	dd->f_set_ib_cfg(ppd, QIB_IB_CFG_LIDLMC,
			 lid | (~((1U << lmc) - 1)) << 16);

	qib_devinfo(dd->pcidev, "IB%u:%u got a lid: 0x%x\n",
		    dd->unit, ppd->port, lid);

	return 0;
}

/*
 * Following deal with the "obviously simple" task of overriding the state
 * of the LEDS, which normally indicate link physical and logical status.
 * The complications arise in dealing with different hardware mappings
 * and the board-dependent routine being called from interrupts.
 * and then there's the requirement to _flash_ them.
 */
#define LED_OVER_FREQ_SHIFT 8
#define LED_OVER_FREQ_MASK (0xFF<<LED_OVER_FREQ_SHIFT)
/* Below is "non-zero" to force override, but both actual LEDs are off */
#define LED_OVER_BOTH_OFF (8)

static void qib_run_led_override(unsigned long opaque)
{
	struct qib_pportdata *ppd = (struct qib_pportdata *)opaque;
	struct qib_devdata *dd = ppd->dd;
	int timeoff;
	int ph_idx;

	if (!(dd->flags & QIB_INITTED))
		return;

	ph_idx = ppd->led_override_phase++ & 1;
	ppd->led_override = ppd->led_override_vals[ph_idx];
	timeoff = ppd->led_override_timeoff;

	dd->f_setextled(ppd, 1);
	/*
	 * don't re-fire the timer if user asked for it to be off; we let
	 * it fire one more time after they turn it off to simplify
	 */
	if (ppd->led_override_vals[0] || ppd->led_override_vals[1])
		mod_timer(&ppd->led_override_timer, jiffies + timeoff);
}

void qib_set_led_override(struct qib_pportdata *ppd, unsigned int val)
{
	struct qib_devdata *dd = ppd->dd;
	int timeoff, freq;

	if (!(dd->flags & QIB_INITTED))
		return;

	/* First check if we are blinking. If not, use 1HZ polling */
	timeoff = HZ;
	freq = (val & LED_OVER_FREQ_MASK) >> LED_OVER_FREQ_SHIFT;

	if (freq) {
		/* For blink, set each phase from one nybble of val */
		ppd->led_override_vals[0] = val & 0xF;
		ppd->led_override_vals[1] = (val >> 4) & 0xF;
		timeoff = (HZ << 4)/freq;
	} else {
		/* Non-blink set both phases the same. */
		ppd->led_override_vals[0] = val & 0xF;
		ppd->led_override_vals[1] = val & 0xF;
	}
	ppd->led_override_timeoff = timeoff;

	/*
	 * If the timer has not already been started, do so. Use a "quick"
	 * timeout so the function will be called soon, to look at our request.
	 */
	if (atomic_inc_return(&ppd->led_override_timer_active) == 1) {
		/* Need to start timer */
		init_timer(&ppd->led_override_timer);
		ppd->led_override_timer.function = qib_run_led_override;
		ppd->led_override_timer.data = (unsigned long) ppd;
		ppd->led_override_timer.expires = jiffies + 1;
		add_timer(&ppd->led_override_timer);
	} else {
		if (ppd->led_override_vals[0] || ppd->led_override_vals[1])
			mod_timer(&ppd->led_override_timer, jiffies + 1);
		atomic_dec(&ppd->led_override_timer_active);
	}
}

/**
 * qib_reset_device - reset the chip if possible
 * @unit: the device to reset
 *
 * Whether or not reset is successful, we attempt to re-initialize the chip
 * (that is, much like a driver unload/reload).  We clear the INITTED flag
 * so that the various entry points will fail until we reinitialize.  For
 * now, we only allow this if no user contexts are open that use chip resources
 */
int qib_reset_device(int unit)
{
	int ret, i;
	struct qib_devdata *dd = qib_lookup(unit);
	struct qib_pportdata *ppd;
	unsigned long flags;
	int pidx;

	if (!dd) {
		ret = -ENODEV;
		goto bail;
	}

	qib_devinfo(dd->pcidev, "Reset on unit %u requested\n", unit);

	if (!dd->kregbase || !(dd->flags & QIB_PRESENT)) {
		qib_devinfo(dd->pcidev, "Invalid unit number %u or "
			    "not initialized or not present\n", unit);
		ret = -ENXIO;
		goto bail;
	}

	spin_lock_irqsave(&dd->uctxt_lock, flags);
	if (dd->rcd)
		for (i = dd->first_user_ctxt; i < dd->cfgctxts; i++) {
			if (!dd->rcd[i] || !dd->rcd[i]->cnt)
				continue;
			spin_unlock_irqrestore(&dd->uctxt_lock, flags);
			qib_dbg("unit %u ctxt %d is in use (PID %u cmd %s),"
				" can't reset\n", unit, i,
				dd->rcd[i]->pid,
				dd->rcd[i]->comm);
			ret = -EBUSY;
			goto bail;
		}
	spin_unlock_irqrestore(&dd->uctxt_lock, flags);

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		if (atomic_read(&ppd->led_override_timer_active)) {
			/* Need to stop LED timer, _then_ shut off LEDs */
			del_timer_sync(&ppd->led_override_timer);
			atomic_set(&ppd->led_override_timer_active, 0);
		}

		/* Shut off LEDs after we are sure timer is not running */
		ppd->led_override = LED_OVER_BOTH_OFF;
		dd->f_setextled(ppd, 0);
		if (dd->flags & QIB_HAS_SEND_DMA)
			teardown_sdma(ppd);
	}

	ret = dd->f_reset(dd);
	if (ret == 1) {
		qib_dbg("Reinitializing unit %u after reset attempt\n",
			unit);
		ret = qib_init(dd, 1);
	} else
		ret = -EAGAIN;
	if (ret)
		qib_dev_err(dd, "Reinitialize unit %u after "
			    "reset failed with %d\n", unit, ret);
	else
		qib_devinfo(dd->pcidev, "Reinitialized unit %u after "
			    "resetting\n", unit);

bail:
	return ret;
}
