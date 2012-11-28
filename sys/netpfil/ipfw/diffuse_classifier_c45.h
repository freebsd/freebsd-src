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
 * DIFFUSE C4.5 classifier.
 */

#ifndef _NETINET_IPFW_DIFFUSE_CLASSIFIER_C45_H_
#define _NETINET_IPFW_DIFFUSE_CLASSIFIER_C45_H_

/* Bit flags for struct di_c45_node_real gt_type/le_type fields. */
#define	DI_C45_CLASS	0x01
#define	DI_C45_NODE	0x02
#define	DI_C45_FEAT	0x04

/* Values for struct di_c45_node type field. */
#define	DI_C45_BNOM	1
#define	DI_C45_NOM	2
#define	DI_C45_REAL	3

/* NOTE: Number of classes, features limited to 256. */

struct di_c45_node {
	/* Node type (split node vs leaf), value type (nominal vs real). */
	uint16_t	type;
	/* Feature number. */
	uint8_t		feature;
	/* Class if feature missing. */
	uint8_t		missing_class;
};

struct di_c45_node_real {
	struct di_c45_node	nid;

	/* Split value. */
	int64_t			val;
	/*
	 * le = less-equal class (class or node depending on type).
	 * ge = greater-than class (class or node depending on type).
	 */
	uint16_t		le_id;
	uint16_t		gt_id;
	uint8_t			le_type;
	uint8_t			gt_type;
};

struct di_c45_node_bin_nominal {
	struct di_c45_node	nid;

	int64_t			val;
	/* eq = equal class (class or node depending on type). */
	uint16_t		eq_id;
	/* ne = non-equal class (class or node depending on type). */
	uint16_t		ne_id;
	uint8_t			eq_type;
	uint8_t			ne_type;
};

/* XXX: No support for non-binary nominal yet. */

struct di_classifier_c45_config
{
	struct di_oid		oid;

	char			model_name[DI_MAX_MODEL_STR_LEN];
	/* Number of features. */
	uint16_t		feature_cnt;
	/* Number of classes. */
	uint16_t		class_cnt;
	/* Precsion, multipler for double->int. */
	uint16_t		multi;
	/* Length of nodes. */
	uint16_t		tree_len;
	/* Tree. */
	struct di_c45_node	nodes[];
};

struct di_classifier_module * c45_module(void);

struct di_cdata;

int c45_classify(struct di_cdata *cdata, int32_t *features, int fcnt);

#endif /* _NETINET_IPFW_DIFFUSE_CLASSIFIER_C45_H_ */
