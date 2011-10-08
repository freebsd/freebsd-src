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

#ifndef _NETINET_IP_DIFFUSE_H_
#define _NETINET_IP_DIFFUSE_H_

/*
 * Definition of the kernel-userland API for DIFFUSE.
 * Use the same type of interface as dummynet.
 *
 * Setsockopt() and getsockopt() pass a batch of objects, each of them starting
 * with a "struct di_oid" which should fully identify the object and its
 * relation with others in the sequence.
 * The first object in each request should have:
 *	 type = DI_CMD_*, id = DI_API_VERSION.
 * For other objects, type and subtype specify the object, len indicates the
 * total length including the header, and 'id' identifies the specific object.
 */

/* Max feature/classifier name string length (including terminator). */
#define	DI_MAX_NAME_STR_LEN	8

#define	DI_MAX_MODEL_STR_LEN	256

/* Max action parameter string length (including terminator). */
#define	DI_MAX_PARAM_STR_LEN	16

/* Max number of features in list. */
#define	DI_MAX_FEATURES		12

/* Max number of statistics/features used by classifier. */
#define	DI_MAX_FEATURE_STATS	64

/* Max number of classes/tags used by classifier. */
#define	DI_MAX_CLASSES		25

/* Char to indicate class numbers. */
#define	DI_CLASS_NO_CHAR	'#'

#define	DI_API_VERSION		1
#define	DI_MAX_ID		0x10000

#define	DI_UNIDIRECTIONAL	0x00
#define	DI_BIDIRECTIONAL	0x01

#define	DI_ACTION_TYPE_UNIDIRECTIONAL	DI_UNIDIRECTIONAL
#define	DI_ACTION_TYPE_BIDIRECTIONAL	DI_BIDIRECTIONAL
#define	DI_FEATURE_ALG_UNIDIRECTIONAL	DI_UNIDIRECTIONAL
#define	DI_FEATURE_ALG_BIDIRECTIONAL	DI_BIDIRECTIONAL
#define	DI_FLOW_TYPE_UNIDIRECTIONAL	DI_UNIDIRECTIONAL
#define	DI_FLOW_TYPE_BIDIRECTIONAL	DI_BIDIRECTIONAL

/* Enable debugging output. */
/* #define	DIFFUSE_DEBUG 1 */

/* Enable more debugging output. */
/* #define	DIFFUSE_DEBUG2 1 */

/* Debugging support. */
#ifdef DIFFUSE_DEBUG
#define	DID(fmt, ...) printf("diffuse: %-10s: " fmt "\n", __func__, \
    ## __VA_ARGS__)
#else
#define	DID(fmt, ...) do {} while (0)
#endif

#ifdef DIFFUSE_DEBUG2
#define	DID2(fmt, ...) printf("diffuse: %-10s: " fmt "\n", __func__, \
    ## __VA_ARGS__)
#else
#define	DID2(fmt, ...) do {} while (0)
#endif

struct di_oid {
	uint32_t	len;	/* Total obj len including this header
				 * (16 bit too small for flowtable show). */
	uint32_t	id;	/* Generic id. */
	uint16_t	flags;	/* Data we can pass in the oid. */
	uint8_t		type;	/* Type, e.g. delete or show. */
	uint8_t		subtype; /* Object, e.g. feature, classifier. */
};

/*
 * These values are in the type field of struct di_oid. To preserve the ABI,
 * never rearrange the list or delete entries with the exception of DI_LAST.
 */
enum {
	DI_NONE = 0,

	DI_CMD_CONFIG,		/* Objects follow. */
	DI_CMD_DELETE,		/* Subtype + list of entries. */
	DI_CMD_GET,		/* Subtype + list of entries. */
	DI_CMD_FLUSH,
	DI_CMD_ZERO,

	DI_FEATURE,
	DI_FEATURE_CONFIG,
	DI_CLASSIFIER,
	DI_CLASSIFIER_CONFIG,
	DI_EXPORT,
	DI_FLOW_TABLE,

	DI_LAST
};

/* Flow table export. */
#define	DI_FT_GET_NONE		0x00
#define	DI_FT_GET_EXPIRED	0x01

struct di_ft_flow_class {
	char		cname[DI_MAX_NAME_STR_LEN];
	uint16_t	class;
};

/* This is the data sent to userspace for a show command. */
struct di_ft_export_entry {
	uint16_t	ruleno;
	uint16_t	setno;

	struct ipfw_flow_id id;	/* (masked) flow id */
	uint64_t	pcnt;	/* Packet match counter. */
	uint64_t	bcnt;	/* Byte match counter. */
	uint32_t	expire;	/* Expire time. */
	uint32_t	bucket;	/* Which bucket in hash table. */
	uint32_t	state;	/* State of this rule (typically a
				 * combination of TCP flags). */
	uint8_t		fcnt;	/* Number of features. */
	uint8_t		tcnt;	/* Number of class tags. */
	uint8_t		final;	/* Equals 1 if final entry. */
	uint8_t		ftype;	/* Bidirectional vs unidirectional. */
#if 0
	/*
	 * The variable length data component which will appear after the above
	 * fixed size header is structured as follows:
	 */
	uint8_t		fidx[fcnt];	/* Index for each feature in feature
					 * list. */
	uint8_t		scnt[fcnt];	/* Number of stats per feature. */
	uint32_t	fwd_svals[fcnt][scnt];	/* Forward statistics. */
	uint32_t	bck_svals[fcnt][scnt];	/* Backward statistics. */
	di_ft_flow_class_t class_tags[tcnt];	/* Class tags. */
#endif
};

/* Feature related types. */
struct di_ctl_feature
{
	struct di_oid	oid;
	char		name[DI_MAX_NAME_STR_LEN];	/* Feature name. */
	char		mod_name[DI_MAX_NAME_STR_LEN];	/* Algorithm name. */
};

struct di_feature_stat {
	uint8_t	fdir;				/* Flow direction. */
	char	sname[DI_MAX_NAME_STR_LEN];	/* Stat name. */
	char	fname[DI_MAX_NAME_STR_LEN];	/* Feature name. */
};

/* Classifier related types. */
struct di_ctl_classifier
{
	struct di_oid	oid;
	char		name[DI_MAX_NAME_STR_LEN]; /* Classifier name. */
	char		mod_name[DI_MAX_NAME_STR_LEN]; /* Algorithm name. */
	uint16_t	confirm;	/* Confirm threshold for
					 * classification. */
	uint8_t		ccnt;		/* Number of class names. */
	uint8_t		fscnt;		/* Number of feature stats. */
	struct di_feature_stat fstats[]; /* Features. */
};

/* Exporter related types. */
struct di_export_config {
	uint8_t		proto;		/* Fixed to UDP for now. */
	uint8_t		addr_type;	/* 4=ip4, 6=ip6 */
	uint16_t	port;		/* Port exporter is listening. */
	struct in_addr	ip;		/* IPv4 address of exporter. */
	struct in6_addr	ip6;		/* IPv6 address of exporter. */

	uint16_t	confirm;	/* Need N consistent consecutive
					 * classifications. */
	uint16_t	min_batch;
	uint16_t	max_batch;
	uint32_t	max_delay;	/* Max ms delay for exporting. */
	uint32_t	flags;		/* e.g. retransmit. */
	uint8_t		atype;		/* Uni vs bidirectional action. */
	char	action[DI_MAX_NAME_STR_LEN]; /* Opaque action for action node. */
	char	action_param[DI_MAX_PARAM_STR_LEN]; /* Opaque action params. */
};

struct di_ctl_export
{
	struct di_oid		oid;
	char			name[DI_MAX_NAME_STR_LEN];
	struct di_export_config	conf;
};

/* Classification policy defines. */
#define	DI_MATCH_ONCE		1
#define	DI_MATCH_SAMPLE_REG	2
#define	DI_MATCH_SAMPLE_RAND	3
#define	DI_MATCH_ONCE_CLASS	4
#define	DI_MATCH_ONCE_EXP	5

/*
 * Instruction definitions.
 */

typedef struct _ipfw_insn_features {
	ipfw_insn	o;
	uint8_t		ftype;		/* Bidirectional, unidirectional, ... */
	uint8_t		fcnt;		/* Number of features. */
	uint16_t	sample_int;	/* Regular sampling interval. */
	uint32_t	sample_prob;	/* Random sampling. */
	char		fnames[DI_MAX_FEATURES][DI_MAX_NAME_STR_LEN];
	struct di_feature *fptrs[DI_MAX_FEATURES]; /* Feature ptrs. */
} ipfw_insn_features;

/* Feature match instruction. */
#define	DI_MATCH_DIR_NONE	0x00
#define	DI_MATCH_DIR_FWD	0x01
#define	DI_MATCH_DIR_BCK	0x02

enum di_comp_types {
	DI_COMP_LT = 0,
	DI_COMP_LE,
	DI_COMP_EQ,
	DI_COMP_GE,
	DI_COMP_GT
};

typedef struct _ipfw_insn_feature_match {
	ipfw_insn		o;
	struct di_feature	*fptr;	/* Feature ptr. */
	int32_t			thresh; /* Value we compare against. */
	uint8_t			sidx;	/* Stat index. */
	uint8_t			fdir;	/* Feature direction. */
	uint8_t			comp;	/* Comparison type. */
	char	sname[DI_MAX_NAME_STR_LEN];	/* Feature statistic. */
	char	fname[DI_MAX_NAME_STR_LEN];	/* Feature name. */
} ipfw_insn_feature_match;

/* Match if class instruction. */

/* Max number of classes in match-if. */
#define	DI_MAX_MATCH_CLASSES 1

typedef struct _ipfw_insn_match_if_class {
	ipfw_insn	o;
	uint8_t		mcnt; /* Number of classes that match. */
	uint8_t	match_classes[DI_MAX_MATCH_CLASSES]; /* Class number of matching
						      * classes. */
	struct di_classifier *clptr;		/* Classifier ptr. */
	char	cname[DI_MAX_NAME_STR_LEN];	/* Classifier name. */
	char	clnames[][DI_MAX_NAME_STR_LEN];	/* Class names. */
} ipfw_insn_match_if_class;

/* Tag using ipfw tags. */
typedef struct _ipfw_insn_class_tags {
	ipfw_insn		o;
	char			cname[DI_MAX_NAME_STR_LEN]; /* Classifier
							     * name. */
	struct di_classifier	*clptr; /* Classifier ptr. */
	uint8_t			tcnt;	/* Number of tags
					 * (<= number of classes). */
	uint16_t		tags[];	/* One tag per class. */
} ipfw_insn_class_tags;

/* Classifier action instruction. */
typedef struct _ipfw_insn_ml_classify {
	ipfw_insn		o;
	char			cname[DI_MAX_NAME_STR_LEN]; /* Classifier
							     * name. */
	struct di_classifier	*clptr; /* Classifier ptr. */
	struct _ipfw_insn_class_tags *tcmd; /* Link to optional tag command. */
} ipfw_insn_ml_classify;

/* Export action instruction. */
typedef struct _ipfw_insn_export {
	ipfw_insn		o;
	char			ename[DI_MAX_NAME_STR_LEN]; /* Export name. */
	struct di_export	*eptr; /* Export instance ptr. */
} ipfw_insn_export;

/*
 * Stores all the persistent data required across multiple calls to
 * diffuse_chk_pkt().
 */
struct di_chk_pkt_args {
	struct di_ft_entry	*q;
	ipfw_insn_class_tags	*tcmd;
	int			no_class;
};

/*
 * Stores all the persistent data required (currently none) across multiple
 * calls to diffuse_chk_rule_cmd().
 */
struct di_chk_rule_cmd_args {
};

#endif /* _NETINET_IP_DIFFUSE_H_ */
