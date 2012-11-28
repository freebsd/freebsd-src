/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Internal stuff for DIFFUSE.
 */

#ifndef _NETINET_IPFW_DIFFUSE_PRIVATE_H_
#define _NETINET_IPFW_DIFFUSE_PRIVATE_H_

/*
 * For platforms that do not have SYSCTL support, we wrap the SYSCTL_* into a
 * function (one per file) to collect the values into an array at module
 * initialization. The wrapping macros, SYSBEGIN() and SYSEND, are empty in the
 * default case.
 */
#ifndef SYSBEGIN
#define	SYSBEGIN(x)
#endif
#ifndef SYSEND
#define	SYSEND
#endif

MALLOC_DECLARE(M_DIFFUSE);

#define	DI_LOCK_INIT()		rw_init(&di_config.mtx, "diffuse main lock")
#define	DI_LOCK_DESTROY()	rw_destroy(&di_config.mtx)
#define	DI_RLOCK()		rw_rlock(&di_config.mtx)
#define	DI_WLOCK()		rw_wlock(&di_config.mtx)
#define	DI_UNLOCK()		rw_unlock(&di_config.mtx)
#define	DI_RLOCK_ASSERT()	rw_assert(&di_config.mtx, RA_RLOCKED)
#define	DI_WLOCK_ASSERT()	rw_assert(&di_config.mtx, RA_WLOCKED)
#define	DI_LOCK_ASSERT()	rw_assert(&di_config.mtx, RA_LOCKED)

SLIST_HEAD(di_feature_alg_head, di_feature_alg);
LIST_HEAD(di_features_head, di_feature);
SLIST_HEAD(di_classifier_alg_head, di_classifier_alg);
LIST_HEAD(di_classifier_head, di_classifier);
LIST_HEAD(di_export_head, di_export);
/* One (big) list of export records (fifo) - double linked for fast removals. */
TAILQ_HEAD(di_export_rec_head, di_export_rec);

/*
 * Configuration and global data for DIFFUSE.
 *
 * When a configuration is modified from userland, 'id' is incremented
 * so we can use the value to check for stale pointers.
 */
struct di_parms {
	uint32_t	id;			/* Configuration version. */

	int		debug;
	int		init_done;

	int		an_rule_removal;	/* Explicit remove messages or timeouts. */

	/* Counters of objects -- used for reporting space. */
	int		feature_count;		/* Number of feature instances. */
	int		classifier_count;	/* Number of classifier instances. */
	int		export_count;		/* Number of exports. */
	int		export_rec_count;	/* Number of exports recs. */

	/* List of feature algorithms. */
	struct di_feature_alg_head	feature_list;
	/* List of feature instances. */
	struct di_features_head		feature_inst_list;
	
	/* List of classifier algorithms. */
	struct di_classifier_alg_head	classifier_list;
	/* List of classifier instances. */
	struct di_classifier_head	classifier_inst_list;

	/* List of export instances. */
	struct di_export_head		export_list;

	/* List of export records. */
	struct di_export_rec_head	export_rec_list;

#ifdef _KERNEL
	/*
	 * This file is normally used in the kernel, unless we do some userland
	 * tests, in which case we do not need a mtx.
	 */
#if defined( __linux__ ) || defined( _WIN32 )
	spinlock_t mtx;
#else
	struct rwlock mtx;
#endif
#endif /* _KERNEL */
};

/* List of flow table timeouts. */
struct di_to_entry {
	struct di_ft_entry	*flow;
	LIST_ENTRY(di_to_entry)	next;
};

typedef struct di_export_rec *(*di_to_handler_fn_t)
    (struct di_ft_entry *q, struct di_export *, int);

/* List of flow classes. */
struct di_flow_class {
	char		cname[DI_MAX_NAME_STR_LEN];	/* Classifier name. */
	uint16_t	class;				/* Class ID. */
	int16_t		prev_class; /* Prev class id, -1 if no previous class. */
	uint16_t	confirm; /* How many identical consecutive classifications. */
	SLIST_ENTRY(di_flow_class) next;
};

SLIST_HEAD(di_flow_class_head, di_flow_class);

/* List of exporters. */
struct di_exp {
	struct di_export *ex;
	SLIST_ENTRY(di_exp) next;
};

SLIST_HEAD(di_exp_list_head, di_exp);

/* Flow table entry. */
struct di_ft_entry {
	struct di_ft_entry	*next;	/* Linked list of rules. */
	struct ip_fw		*rule;	/* Used to pass up the rule number. */
	struct ipfw_flow_id	id;	/* (masked) flow id. */
	uint64_t		pcnt;	/* Packet match counter. */
	uint64_t		bcnt;	/* Byte match counter. */
	uint32_t		expire;	/* Expire time. */
	uint32_t		bucket;	/* Which bucket in hash table. */
	uint32_t		state;	/* State of this rule (typically a
					 * combination of TCP flags). */
	uint8_t			ftype;	/* Bidir vs unidir, match limiting. */
	uint8_t			fcnt;	/* Number of features. */
	uint8_t			tcnt;	/* Number of class tags. */
	uint16_t		sample_int;	/* Sample interval. */
	uint32_t		sample_prob;	/* Sample probability. */
	uint16_t		pkts_after_last; /* Match limiting: packets n */
	struct di_feature	*features[DI_MAX_FEATURES]; /* Feature ptrs. */
	struct di_fdata		fwd_data[DI_MAX_FEATURES];
	struct di_fdata		bck_data[DI_MAX_FEATURES];
	struct timeval		ex_time;	/* Time last exported. */
	struct di_flow_class_head	flow_classes;	/* List of class tags. */
	struct di_to_entry	*to;		/* Timeout list entry ptr. */
	struct di_exp_list_head	ex_list;	/* Ptrs to exporters. */
};

/* Export data record. */
struct di_export_rec {
	char ename[DI_MAX_NAME_STR_LEN]; /* Generating export instance. */
	struct ipfw_flow_id id;	/* IPs, ports. */
	struct timeval	time;	/* Generation time. */
	struct timeval	no_earlier; /* Don't send before. */
	/* XXX: Flow label, TOS missing */
	uint8_t		mtype;		/* Message type. */
	uint8_t		fcnt;		/* Number of features. */
	uint8_t		tcnt; 		/* Number of tags. */
	uint8_t		ftype;		/* Bidir vs unidir. */
	uint8_t		match_dir;
	uint8_t		action_dir;	/* Bidir vs unidir. */
	uint8_t		ttype;		/* Timeout type. */
	uint16_t	tval;		/* Timeout value in seconds. */
	uint32_t	pcnt;		/* Flow packet counter. */
	uint64_t	bcnt;		/* Flow byte counter. */
	struct di_ft_flow_class class_tags[DI_MAX_CLASSES];
	char action[DI_MAX_NAME_STR_LEN];
	char act_params[DI_MAX_PARAM_STR_LEN];
	struct di_ft_entry *ft_rec; /* Flow entry ptr. */
	TAILQ_ENTRY(di_export_rec) next;
};

/* Feature. */
struct di_feature {
	char		*name; /* Instance name. */
	volatile int	ref_count; /* Num rules referencing feature. */
	struct di_cdata	conf;
	struct di_feature_alg *alg; /* Feature algorithm ptr. */
	LIST_ENTRY(di_feature) next; /* Next in list. */
};

/* Classifier. */
struct di_feature_stat_ptr {
	struct di_feature	*fptr;	/* Pointer to feature. */
	uint8_t			sidx;	/* Statistic index. */
};

struct di_classifier {
	char		*name;	/* Instance name. */
	volatile int	ref_count; /* Num rules referencing classifier. */
	struct di_classifier_alg *alg; /* Classifier algorithm ptr. */
	struct di_cdata	conf;
	int32_t		id;	/* Unique id used for tag cookie. */
	uint16_t	*tags;
	uint16_t	confirm; /* How many identical consecutive
				  * classifications. */
	uint8_t		fscnt;	/* Number of features. */
	uint8_t		ccnt;	/* Number of classes. */
	uint8_t		tcnt;	/* Tag count (ipfw tags). */
	struct di_feature_stat	*fstats; /* Features + classes. */
	struct di_feature_stat_ptr *fstats_ptr; /* Feature indices. */
	LIST_ENTRY(di_classifier) next;  /* Next in list. */
};

/* Export. */
struct di_export {
	char			*name;	/* Instance name. */
	volatile int		ref_count; /* Num rules referencing exporter. */
	struct di_export_config	conf;	/* Config from userspace. */
	struct socket		*sock;	/* Protocol socket. */
	uint32_t		seq_no;	/* Sequence number. */
	struct timeval		last_pkt_time; /* Most recently sent packet. */
	struct mbuf		*mh;	/* First mbuf of export packet chain. */
	struct mbuf		*mt;	/* Last mbuf of export packet chain. */
	LIST_ENTRY(di_export)	next;	/* Next in list. */
};

/* For tagging mbufs (packets) with classes. */
#define	MTAG_DIFFUSE_CLASS 1243750889

struct di_class_tag {
	struct m_tag	tag;
	int		class;
	int		prev_class;
	int		confirm;
};

/* Global configuration. */
extern struct di_parms di_config;

/* Function prototypes. */

/* ip_diffuse.c  */
int diffuse_get_feature_idx(const char *name);

/* diffuse_flowtable.c */
void diffuse_ft_attach(void);
void diffuse_ft_detach(void);
void diffuse_ft_init(void);
void diffuse_ft_uninit(void);
int diffuse_ft_len(int expired);
struct di_ft_entry * diffuse_ft_install_state(struct ip_fw *rule,
    ipfw_insn_features *cmd, struct ip_fw_args *args, void *ulp, int pktlen);
struct di_ft_entry * diffuse_ft_lookup_entry(struct ipfw_flow_id *pkt,
    struct ip_fw_args *args, void *ulp, int pktlen, int *match_direction);
void diffuse_get_ft(char **pbp, const char *ep, int expired);
int diffuse_ft_entries(void);
void diffuse_ft_remove_entries(struct ip_fw *rule);
void diffuse_ft_add_class(struct di_ft_entry *e, char *cname, int class,
    int *prev_class, int *confirm);
int diffuse_ft_get_class(struct di_ft_entry *e, char *cname, int *prev_class,
    int *confirm);
void diffuse_ft_remove_class(struct di_ft_entry *e, char *cname);
void diffuse_ft_unlock(void);
void diffuse_ft_check_timeouts(di_to_handler_fn_t f);
void diffuse_ft_add_export(struct di_ft_entry *e,
    struct di_export_rec *ex_rec, struct di_export *nex);
void diffuse_ft_flush(int reset_counters_only);
int diffuse_ft_get_stat(struct di_ft_entry *q, int fdir,
    struct di_feature *fptr, int sidx, int32_t *val);
int diffuse_ft_get_stats(struct di_ft_entry *q, int fscnt,
    struct di_feature_stat *fstats, struct di_feature_stat_ptr *fstats_ptr,
    int32_t *fvec);
int diffuse_ft_do_export(struct di_ft_entry *q, uint16_t confirm);
int diffuse_ft_update_features(struct di_ft_entry *q,
    ipfw_insn_features *cmd, struct ip_fw_args *args, void *ulp);

/* diffuse_export.c */
int diffuse_export_remove_recs(char *ename);
struct di_export_rec * diffuse_export_add_rec(struct di_ft_entry *q,
    struct di_export *ex, int add_command);
struct socket *diffuse_export_open(struct di_export_config *conf);
int diffuse_export_send(struct di_export *ex);
void diffuse_export_close(struct socket *s);
void diffuse_export_init(void);
void diffuse_export_uninit(void);
void diffuse_export_prune_recs(void);

#endif /* _NETINET_IPFW_DIFFUSE_PRIVATE_H_ */
