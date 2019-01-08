/*
 * Copyright (c) 2008 Mellanox Technologies Ltd.  All rights reserved.
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

#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <rdma/sdp_socket.h>
#include <linux/vmalloc.h>
#include "sdp.h"

#ifdef CONFIG_PROC_FS

#define DEBUGFS_SDP_BASE "sdp"
#define PROC_SDP_STATS "sdpstats"
#define PROC_SDP_PERF "sdpprf"

#if defined(SDP_SOCK_HISTORY) || defined(SDP_PROFILING)
struct dentry *sdp_dbgfs_base;
#endif
#ifdef SDP_PROFILING
struct dentry *sdp_prof_file = NULL;
#endif

/* just like TCP fs */
struct sdp_seq_afinfo {
	struct module           *owner;
	char                    *name;
	sa_family_t             family;
	int                     (*seq_show) (struct seq_file *m, void *v);
	struct file_operations  *seq_fops;
};

struct sdp_iter_state {
	sa_family_t             family;
	int                     num;
	struct seq_operations   seq_ops;
};

static void *sdp_get_idx(struct seq_file *seq, loff_t pos)
{
	int i = 0;
	struct sdp_sock *ssk;

	if (!list_empty(&sock_list))
		list_for_each_entry(ssk, &sock_list, sock_list) {
			if (i == pos)
				return ssk;
			i++;
		}

	return NULL;
}

#define sdp_sock_hold_return(sk, msg)					\
	({								\
	_sdp_add_to_history(sk, #msg, __func__, __LINE__, HOLD_REF, msg); \
	sdp_dbg(sk, "%s:%d - %s (%s) ref = %d.\n", __func__, __LINE__, \
		"sock_hold", #msg, atomic_read(&(sk)->sk_refcnt)); \
	atomic_inc_return(&(sk)->sk_refcnt);				\
	})

static void *sdp_seq_start(struct seq_file *seq, loff_t *pos)
{
	void *start = NULL;
	struct sdp_iter_state *st = seq->private;

	st->num = 0;

	if (!*pos)
		return SEQ_START_TOKEN;

	spin_lock_irq(&sock_list_lock);
	start = sdp_get_idx(seq, *pos - 1);
	if (!start)
		goto out;

	if (sdp_sock_hold_return((struct sock *)start, SOCK_REF_SEQ) < 2)
		start = NULL;
out:
	spin_unlock_irq(&sock_list_lock);

	return start;
}

static void *sdp_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct sdp_iter_state *st = seq->private;
	void *next = NULL;

	spin_lock_irq(&sock_list_lock);
	if (v == SEQ_START_TOKEN)
		next = sdp_get_idx(seq, 0);
	else
		next = sdp_get_idx(seq, *pos);
	if (!next)
		goto out;

	if (sdp_sock_hold_return((struct sock *)next, SOCK_REF_SEQ) < 2)
		next = NULL;
out:
	spin_unlock_irq(&sock_list_lock);
	*pos += 1;
	st->num++;

	return next;
}

static void sdp_seq_stop(struct seq_file *seq, void *v)
{
}

#define TMPSZ 150

static int sdp_seq_show(struct seq_file *seq, void *v)
{
	struct sdp_iter_state *st;
	struct sock *sk = v;
	char tmpbuf[TMPSZ + 1];
	unsigned int dest;
	unsigned int src;
	int uid;
	unsigned long inode;
	__u16 destp;
	__u16 srcp;
	__u32 rx_queue, tx_queue;

	if (v == SEQ_START_TOKEN) {
		seq_printf(seq, "%-*s\n", TMPSZ - 1,
				"  sl  local_address rem_address        "
				"uid inode   rx_queue tx_queue state");
		goto out;
	}

	st = seq->private;

	dest = inet_sk(sk)->daddr;
	src = inet_sk(sk)->rcv_saddr;
	destp = ntohs(inet_sk(sk)->dport);
	srcp = ntohs(inet_sk(sk)->sport);
	uid = sock_i_uid(sk);
	inode = sock_i_ino(sk);
	rx_queue = rcv_nxt(sdp_sk(sk)) - sdp_sk(sk)->copied_seq;
	tx_queue = sdp_sk(sk)->write_seq - sdp_sk(sk)->tx_ring.una_seq;

	sprintf(tmpbuf, "%4d: %08X:%04X %08X:%04X %5d %lu	%08X:%08X %X",
		st->num, src, srcp, dest, destp, uid, inode,
		rx_queue, tx_queue, sk->sk_state);

	seq_printf(seq, "%-*s\n", TMPSZ - 1, tmpbuf);

	sock_put(sk, SOCK_REF_SEQ);
out:
	return 0;
}

static int sdp_seq_open(struct inode *inode, struct file *file)
{
	struct sdp_seq_afinfo *afinfo = PDE(inode)->data;
	struct seq_file *seq;
	struct sdp_iter_state *s;
	int rc;

	if (unlikely(afinfo == NULL))
		return -EINVAL;

/* Workaround bogus warning by memtrack */
#define _kzalloc(size,flags) kzalloc(size,flags)
#undef kzalloc
	s = kzalloc(sizeof(*s), GFP_KERNEL);
#define kzalloc(s,f) _kzalloc(s,f)
	if (!s)
		return -ENOMEM;
	s->family               = afinfo->family;
	s->seq_ops.start        = sdp_seq_start;
	s->seq_ops.next         = sdp_seq_next;
	s->seq_ops.show         = afinfo->seq_show;
	s->seq_ops.stop         = sdp_seq_stop;

	rc = seq_open(file, &s->seq_ops);
	if (rc)
		goto out_kfree;
	seq          = file->private_data;
	seq->private = s;
out:
	return rc;
out_kfree:
	kfree(s);
	goto out;
}


static struct file_operations sdp_seq_fops;
static struct sdp_seq_afinfo sdp_seq_afinfo = {
	.owner          = THIS_MODULE,
	.name           = "sdp",
	.family         = AF_INET_SDP,
	.seq_show       = sdp_seq_show,
	.seq_fops       = &sdp_seq_fops,
};

#ifdef SDPSTATS_ON
DEFINE_PER_CPU(struct sdpstats, sdpstats);

static void sdpstats_seq_hist(struct seq_file *seq, char *str, u32 *h, int n,
		int is_log)
{
	int i;
	u32 max = 0;
	int first = -1, last = n - 1;

	seq_printf(seq, "%s:\n", str);

	for (i = 0; i < n; i++) {
		if (h[i] > max)
			max = h[i];

		if (first == -1 && h[i])
			first = i;

		if (h[i])
			last = i;
	}

	if (max == 0) {
		seq_printf(seq, " - all values are 0\n");
		return;
	}

	for (i = first; i <= last; i++) {
		char s[51];
		int j = 50 * h[i] / max;
		int val = is_log ? (i == n-1 ? 0 : 1<<i) : i;
		memset(s, '*', j);
		s[j] = '\0';

		seq_printf(seq, "%10d | %-50s - %u\n", val, s, h[i]);
	}
}

#define SDPSTATS_COUNTER_GET(var) ({ \
	u32 __val = 0;						\
	unsigned int __i;                                       \
	for_each_possible_cpu(__i)                              \
		__val += per_cpu(sdpstats, __i).var;		\
	__val;							\
})

#define SDPSTATS_HIST_GET(hist, hist_len, sum) ({ \
	unsigned int __i;                                       \
	for_each_possible_cpu(__i) {                            \
		unsigned int __j;				\
		u32 *h = per_cpu(sdpstats, __i).hist;		\
		for (__j = 0; __j < hist_len; __j++) { 		\
			sum[__j] += h[__j];			\
		} \
	} 							\
})

#define __sdpstats_seq_hist(seq, msg, hist, is_log) ({		\
	int hist_len = ARRAY_SIZE(__get_cpu_var(sdpstats).hist);\
	memset(h, 0, sizeof(*h) * h_len);			\
	SDPSTATS_HIST_GET(hist, hist_len, h);	\
	sdpstats_seq_hist(seq, msg, h, hist_len, is_log);\
})

#define __sdpstats_seq_hist_pcpu(seq, msg, hist) ({		\
	unsigned int __i;                                       \
	memset(h, 0, sizeof(*h) * h_len);				\
	for_each_possible_cpu(__i) {                            \
		h[__i] = per_cpu(sdpstats, __i).hist;		\
	} 							\
	sdpstats_seq_hist(seq, msg, h, NR_CPUS, 0);		\
})

static int sdpstats_seq_show(struct seq_file *seq, void *v)
{
	int i;
	size_t h_len = max(NR_CPUS, SDPSTATS_MAX_HIST_SIZE);
	u32 *h;

	if (!(h = vmalloc(h_len * sizeof(*h))))
		return -ENOMEM;

	seq_printf(seq, "SDP statistics:\n");

	__sdpstats_seq_hist(seq, "sendmsg_seglen", sendmsg_seglen, 1);
	__sdpstats_seq_hist(seq, "send_size", send_size, 1);
	__sdpstats_seq_hist(seq, "credits_before_update",
		credits_before_update, 0);

	seq_printf(seq, "sdp_sendmsg() calls\t\t: %d\n",
		SDPSTATS_COUNTER_GET(sendmsg));
	seq_printf(seq, "bcopy segments     \t\t: %d\n",
		SDPSTATS_COUNTER_GET(sendmsg_bcopy_segment));
	seq_printf(seq, "inline sends       \t\t: %d\n",
		SDPSTATS_COUNTER_GET(inline_sends));
	seq_printf(seq, "bzcopy segments    \t\t: %d\n",
		SDPSTATS_COUNTER_GET(sendmsg_bzcopy_segment));
	seq_printf(seq, "zcopy segments    \t\t: %d\n",
		SDPSTATS_COUNTER_GET(sendmsg_zcopy_segment));
	seq_printf(seq, "post_send_credits  \t\t: %d\n",
		SDPSTATS_COUNTER_GET(post_send_credits));
	seq_printf(seq, "memcpy_count       \t\t: %u\n",
		SDPSTATS_COUNTER_GET(memcpy_count));

        for (i = 0; i < ARRAY_SIZE(__get_cpu_var(sdpstats).post_send); i++) {
                if (mid2str(i)) {
                        seq_printf(seq, "post_send %-20s\t: %d\n",
                                        mid2str(i),
					SDPSTATS_COUNTER_GET(post_send[i]));
                }
        }

	seq_printf(seq, "\n");
	seq_printf(seq, "sdp_recvmsg() calls\t\t: %d\n",
		SDPSTATS_COUNTER_GET(recvmsg));
	seq_printf(seq, "post_recv         \t\t: %d\n",
		SDPSTATS_COUNTER_GET(post_recv));
	seq_printf(seq, "BZCopy poll miss  \t\t: %d\n",
		SDPSTATS_COUNTER_GET(bzcopy_poll_miss));
	seq_printf(seq, "send_wait_for_mem \t\t: %d\n",
		SDPSTATS_COUNTER_GET(send_wait_for_mem));
	seq_printf(seq, "send_miss_no_credits\t\t: %d\n",
		SDPSTATS_COUNTER_GET(send_miss_no_credits));

	seq_printf(seq, "rx_poll_miss      \t\t: %d\n", SDPSTATS_COUNTER_GET(rx_poll_miss));
	seq_printf(seq, "rx_poll_hit       \t\t: %d\n", SDPSTATS_COUNTER_GET(rx_poll_hit));
	__sdpstats_seq_hist(seq, "poll_hit_usec", poll_hit_usec, 1);
	seq_printf(seq, "rx_cq_arm_timer      \t\t: %d\n", SDPSTATS_COUNTER_GET(rx_cq_arm_timer));

	seq_printf(seq, "tx_poll_miss      \t\t: %d\n", SDPSTATS_COUNTER_GET(tx_poll_miss));
	seq_printf(seq, "tx_poll_busy      \t\t: %d\n", SDPSTATS_COUNTER_GET(tx_poll_busy));
	seq_printf(seq, "tx_poll_hit       \t\t: %d\n", SDPSTATS_COUNTER_GET(tx_poll_hit));
	seq_printf(seq, "tx_poll_no_op     \t\t: %d\n", SDPSTATS_COUNTER_GET(tx_poll_no_op));

	seq_printf(seq, "keepalive timer   \t\t: %d\n", SDPSTATS_COUNTER_GET(keepalive_timer));
	seq_printf(seq, "nagle timer       \t\t: %d\n", SDPSTATS_COUNTER_GET(nagle_timer));

	seq_printf(seq, "CQ stats:\n");
	seq_printf(seq, "- RX irq armed  \t\t: %d\n", SDPSTATS_COUNTER_GET(rx_int_arm));
	seq_printf(seq, "- RX interrupts \t\t: %d\n", SDPSTATS_COUNTER_GET(rx_int_count));
	seq_printf(seq, "- RX int wake up\t\t: %d\n", SDPSTATS_COUNTER_GET(rx_int_wake_up));
	seq_printf(seq, "- RX int queue  \t\t: %d\n", SDPSTATS_COUNTER_GET(rx_int_queue));
	seq_printf(seq, "- RX int no op  \t\t: %d\n", SDPSTATS_COUNTER_GET(rx_int_no_op));
	seq_printf(seq, "- RX cq modified\t\t: %d\n", SDPSTATS_COUNTER_GET(rx_cq_modified));

	seq_printf(seq, "- TX irq armed\t\t: %d\n", SDPSTATS_COUNTER_GET(tx_int_arm));
	seq_printf(seq, "- TX interrupts\t\t: %d\n", SDPSTATS_COUNTER_GET(tx_int_count));

	seq_printf(seq, "ZCopy stats:\n");
	seq_printf(seq, "- TX timeout\t\t: %d\n", SDPSTATS_COUNTER_GET(zcopy_tx_timeout));
	seq_printf(seq, "- TX cross send\t\t: %d\n", SDPSTATS_COUNTER_GET(zcopy_cross_send));
	seq_printf(seq, "- TX aborted by peer\t: %d\n", SDPSTATS_COUNTER_GET(zcopy_tx_aborted));
	seq_printf(seq, "- TX error\t\t: %d\n", SDPSTATS_COUNTER_GET(zcopy_tx_error));
	seq_printf(seq, "- FMR alloc error\t: %d\n", SDPSTATS_COUNTER_GET(fmr_alloc_error));

	__sdpstats_seq_hist_pcpu(seq, "CPU sendmsg", sendmsg);
	__sdpstats_seq_hist_pcpu(seq, "CPU recvmsg", recvmsg);
	__sdpstats_seq_hist_pcpu(seq, "CPU rx_irq", rx_int_count);
	__sdpstats_seq_hist_pcpu(seq, "CPU rx_wq", rx_wq);
	__sdpstats_seq_hist_pcpu(seq, "CPU tx_irq", tx_int_count);

	vfree(h);

	return 0;
}

static ssize_t sdpstats_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *offs)
{
	int i;

	for_each_possible_cpu(i)
		memset(&per_cpu(sdpstats, i), 0, sizeof(struct sdpstats));
	printk(KERN_WARNING "Cleared sdp statistics\n");

	return count;
}

static int sdpstats_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdpstats_seq_show, NULL);
}

static struct file_operations sdpstats_fops = {
	.owner		= THIS_MODULE,
	.open		= sdpstats_seq_open,
	.read		= seq_read,
	.write		= sdpstats_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#endif

#ifdef SDP_PROFILING
struct sdpprf_log sdpprf_log[SDPPRF_LOG_SIZE];
atomic_t sdpprf_log_count;

static cycles_t start_t;

static inline void remove_newline(char *s)
{
	while (*s) {
		if (*s == '\n')
			*s = '\0';
		++s;
	}
}

static int sdpprf_show(struct seq_file *m, void *v)
{
	struct sdpprf_log *l = v;
	unsigned long usec_rem, t;

	if (!atomic_read(&sdpprf_log_count)) {
		seq_printf(m, "No performance logs\n");
		goto out;
	}

	t = sdp_cycles_to_usecs(l->time - start_t);
	usec_rem = do_div(t, USEC_PER_SEC);
	remove_newline(l->msg);
	seq_printf(m, "%-6d: [%5lu.%06lu] %-50s - [%d{%d} %d:%d] "
			"skb: %p %s:%d\n",
			l->idx, t, usec_rem,
			l->msg, l->pid, l->cpu, l->sk_num, l->sk_dport,
			l->skb, l->func, l->line);
out:
	return 0;
}

static void *sdpprf_start(struct seq_file *p, loff_t *pos)
{
	int idx = *pos;

	if (!*pos) {
		if (!atomic_read(&sdpprf_log_count))
			return SEQ_START_TOKEN;
	}

	if (*pos >= MIN(atomic_read(&sdpprf_log_count), SDPPRF_LOG_SIZE - 1))
		return NULL;

	if (atomic_read(&sdpprf_log_count) >= SDPPRF_LOG_SIZE - 1) {
		int off = atomic_read(&sdpprf_log_count) & (SDPPRF_LOG_SIZE - 1);
		idx = (idx + off) & (SDPPRF_LOG_SIZE - 1);

	}

	if (!start_t)
		start_t = sdpprf_log[idx].time;
	return &sdpprf_log[idx];
}

static void *sdpprf_next(struct seq_file *p, void *v, loff_t *pos)
{
	struct sdpprf_log *l = v;

	if (++*pos >= MIN(atomic_read(&sdpprf_log_count), SDPPRF_LOG_SIZE - 1))
		return NULL;

	++l;
	if (l - &sdpprf_log[0] >= SDPPRF_LOG_SIZE - 1)
		return &sdpprf_log[0];

	return l;
}

static void sdpprf_stop(struct seq_file *p, void *v)
{
}

static struct seq_operations sdpprf_ops = {
	.start = sdpprf_start,
	.stop = sdpprf_stop,
	.next = sdpprf_next,
	.show = sdpprf_show,
};

static int sdpprf_open(struct inode *inode, struct file *file)
{
	int res;

	res = seq_open(file, &sdpprf_ops);

	return res;
}

static ssize_t sdpprf_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *offs)
{
	atomic_set(&sdpprf_log_count,  0);
	printk(KERN_INFO "Cleared sdpprf statistics\n");

	return count;
}

static struct file_operations sdpprf_fops = {
	.open           = sdpprf_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release,
	.write		= sdpprf_write,
};
#endif /* SDP_PROFILING */

#ifdef SDP_SOCK_HISTORY

void sdp_print_history(struct sock *sk)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	unsigned i;
	unsigned long flags;

	spin_lock_irqsave(&ssk->hst_lock, flags);

	sdp_warn(sk, "############## %p %s %lu/%zu ##############\n",
			sk, sdp_state_str(sk->sk_state),
			ssk->hst_idx, ARRAY_SIZE(ssk->hst));

	for (i = 0; i < ssk->hst_idx; ++i) {
		struct sdp_sock_hist *hst = &ssk->hst[i];
		char *ref_str = reftype2str(hst->ref_type);

		if (hst->ref_type == NOT_REF)
			ref_str = "";

		if (hst->cnt != 1) {
			sdp_warn(sk, "[%s:%d pid: %d] %s %s : %d\n",
					hst->func, hst->line, hst->pid,
					ref_str, hst->str, hst->cnt);
		} else {
			sdp_warn(sk, "[%s:%d pid: %d] %s %s\n",
					hst->func, hst->line, hst->pid,
					ref_str, hst->str);
		}
	}

	spin_unlock_irqrestore(&ssk->hst_lock, flags);
}

void _sdp_add_to_history(struct sock *sk, const char *str,
		const char *func, int line, int ref_type, int ref_enum)
{
	struct sdp_sock *ssk = sdp_sk(sk);
	unsigned i;
	unsigned long flags;
	struct sdp_sock_hist *hst;

 	spin_lock_irqsave(&ssk->hst_lock, flags);

	i = ssk->hst_idx;

	if (i >= ARRAY_SIZE(ssk->hst)) {
		//sdp_warn(sk, "overflow, drop: %s\n", s);
		++ssk->hst_idx;
		goto out;
	}

	if (ssk->hst[i].str)
		sdp_warn(sk, "overwriting %s\n", ssk->hst[i].str);

	switch (ref_type) {
		case NOT_REF:
		case HOLD_REF:
simple_add:
			hst = &ssk->hst[i];
			hst->str = (char *)str;
			hst->func = (char *)func;
			hst->line = line;
			hst->ref_type = ref_type;
			hst->ref_enum = ref_enum;
			hst->cnt = 1;
			hst->pid = current->pid;
			++ssk->hst_idx;
			break;
		case PUT_REF:
		case __PUT_REF:
			/* Try to shrink history by attaching HOLD+PUT
			 * together */
			hst = i > 0 ? &ssk->hst[i - 1] : NULL;
			if (hst && hst->ref_type == HOLD_REF &&
					hst->ref_enum == ref_enum) {
				hst->ref_type = BOTH_REF;
				hst->func = (char *)func;
				hst->line = line;
				hst->pid = current->pid;

				/* try to shrink some more - by summing up */
				--i;
				hst = i > 0 ? &ssk->hst[i - 1] : NULL;
				if (hst && hst->ref_type == BOTH_REF &&
						hst->ref_enum == ref_enum) {
					++hst->cnt;
					hst->func = (char *)func;
					hst->line = line;
					hst->pid = current->pid;
					ssk->hst[i].str = NULL;

					--ssk->hst_idx;
				}
			} else
				goto simple_add;
			break;
		default:
			sdp_warn(sk, "error\n");
	}
out:
	spin_unlock_irqrestore(&ssk->hst_lock, flags);
}
static int sdp_ssk_hist_seq_show(struct seq_file *seq, void *v)
{
	struct sock *sk = seq->private;
	struct sdp_sock *ssk = sdp_sk(sk);
	unsigned i;
	unsigned long flags;

	spin_lock_irqsave(&ssk->hst_lock, flags);

	seq_printf(seq, "############## %p %s %lu/%zu ##############\n",
			sk, sdp_state_str(sk->sk_state),
			ssk->hst_idx, ARRAY_SIZE(ssk->hst));

	for (i = 0; i < ssk->hst_idx; ++i) {
		struct sdp_sock_hist *hst = &ssk->hst[i];
		char *ref_str = reftype2str(hst->ref_type);

		if (hst->ref_type == NOT_REF)
			ref_str = "";

		if (hst->cnt != 1) {
			seq_printf(seq, "[%30s:%-5d pid: %-6d] %s %s : %d\n",
					hst->func, hst->line, hst->pid,
					ref_str, hst->str, hst->cnt);
		} else {
			seq_printf(seq, "[%30s:%-5d pid: %-6d] %s %s\n",
					hst->func, hst->line, hst->pid,
					ref_str, hst->str);
		}
	}

	spin_unlock_irqrestore(&ssk->hst_lock, flags);
	return 0;
}

static int sdp_ssk_hist_seq_open(struct inode *inode, struct file *file)
{
	struct sock *sk = inode->i_private;

	return single_open(file, sdp_ssk_hist_seq_show, sk);
}

static struct file_operations ssk_hist_fops = {
	.owner	 = THIS_MODULE,
	.open	 = sdp_ssk_hist_seq_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

static void sdp_ssk_hist_name(char *sk_name, int len, struct sock *sk)
{
	int lport = inet_sk(sk)->num;
	int rport = ntohs(inet_sk(sk)->dport);

	snprintf(sk_name, len, "%05x_%d:%d",
			sdp_sk(sk)->sk_id, lport, rport);
}

int sdp_ssk_hist_open(struct sock *sk)
{
	int ret = 0;
	char sk_name[256];
	struct sdp_sock *ssk = sdp_sk(sk);

	if (!sdp_dbgfs_base) {
		return 0;
	}

	sdp_ssk_hist_name(sk_name, sizeof(sk_name), sk);

	ssk->hst_dentr = debugfs_create_file(sk_name, S_IRUGO | S_IWUGO, 
			sdp_dbgfs_base, sk, &ssk_hist_fops);
	if (IS_ERR(ssk->hst_dentr)) {
		ret = PTR_ERR(ssk->hst_dentr);
		ssk->hst_dentr = NULL;
	}

	return ret;
}

int sdp_ssk_hist_close(struct sock *sk)
{
	if (sk && sdp_sk(sk)->hst_dentr)
		debugfs_remove(sdp_sk(sk)->hst_dentr);
	return 0;
}

int sdp_ssk_hist_rename(struct sock *sk)
{
	char sk_name[256];
	struct dentry *d;

	if (!sk || !sdp_sk(sk)->hst_dentr)
		return 0;

	sdp_ssk_hist_name(sk_name, sizeof(sk_name), sk);

	d = debugfs_rename(sdp_dbgfs_base, sdp_sk(sk)->hst_dentr, sdp_dbgfs_base, sk_name);
	if (IS_ERR(d))
		return PTR_ERR(d);

	return 0;
}
#endif

int __init sdp_proc_init(void)
{
	struct proc_dir_entry *p = NULL;
#ifdef SDPSTATS_ON
	struct proc_dir_entry *stats = NULL;
#endif

	sdp_seq_afinfo.seq_fops->owner         = sdp_seq_afinfo.owner;
	sdp_seq_afinfo.seq_fops->open          = sdp_seq_open;
	sdp_seq_afinfo.seq_fops->read          = seq_read;
	sdp_seq_afinfo.seq_fops->llseek        = seq_lseek;
	sdp_seq_afinfo.seq_fops->release       = seq_release_private;

#if defined(SDP_PROFILING) || defined(SDP_SOCK_HISTORY)
	sdp_dbgfs_base = debugfs_create_dir(DEBUGFS_SDP_BASE, NULL);
	if (!sdp_dbgfs_base || IS_ERR(sdp_dbgfs_base)) {
		if (PTR_ERR(sdp_dbgfs_base) == -ENODEV)
			printk(KERN_WARNING "sdp: debugfs is not supported.\n");
		else {
			printk(KERN_ERR "sdp: error creating debugfs information %ld\n",
					PTR_ERR(sdp_dbgfs_base));
			return -EINVAL;
		}
	}
#endif

	p = proc_net_fops_create(&init_net, sdp_seq_afinfo.name, S_IRUGO,
				 sdp_seq_afinfo.seq_fops);
	if (p)
		p->data = &sdp_seq_afinfo;
	else
		goto no_mem;

#ifdef SDPSTATS_ON

	stats = proc_net_fops_create(&init_net, PROC_SDP_STATS,
			S_IRUGO | S_IWUGO, &sdpstats_fops);
	if (!stats)
		goto no_mem_stats;

#endif

#ifdef SDP_PROFILING
	sdp_prof_file = debugfs_create_file(PROC_SDP_PERF, S_IRUGO | S_IWUGO, 
			sdp_dbgfs_base, NULL, &sdpprf_fops);
	if (!sdp_prof_file)
		goto no_mem_prof;
#endif

	return 0;

#ifdef SDP_PROFILING
no_mem_prof:
#endif

#ifdef SDPSTATS_ON
	proc_net_remove(&init_net, PROC_SDP_STATS);

no_mem_stats:
#endif
	proc_net_remove(&init_net, sdp_seq_afinfo.name);

no_mem:
	return -ENOMEM;
}

void sdp_proc_unregister(void)
{
	proc_net_remove(&init_net, sdp_seq_afinfo.name);
	memset(sdp_seq_afinfo.seq_fops, 0, sizeof(*sdp_seq_afinfo.seq_fops));

#ifdef SDPSTATS_ON
	proc_net_remove(&init_net, PROC_SDP_STATS);
#endif
#ifdef SDP_PROFILING
	debugfs_remove(sdp_prof_file);
#endif
#if defined(SDP_PROFILING) || defined(SDP_SOCK_HISTORY)
	debugfs_remove(sdp_dbgfs_base);
#endif
}

#else /* CONFIG_PROC_FS */

int __init sdp_proc_init(void)
{
	return 0;
}

void sdp_proc_unregister(void)
{

}
#endif /* CONFIG_PROC_FS */
