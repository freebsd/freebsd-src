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
 * DIFFUSE Naive Bayes classifier.
 */

#ifndef _NETINET_IPFW_DIFFUSE_CLASSIFIER_NBAYES_H_
#define _NETINET_IPFW_DIFFUSE_CLASSIFIER_NBAYES_H_

#define	DI_NBAYES_ATTR_PRIOR	0
#define	DI_NBAYES_ATTR_DISC	1
#define	DI_NBAYES_ATTR_NORM	2

struct di_nbayes_attr_id
{
	uint16_t	type;
	uint16_t	len;
};

struct di_nbayes_attr_disc_val
{
	int32_t		high_val;
	uint32_t	cond_p[];
};

struct di_nbayes_attr_disc
{
	struct di_nbayes_attr_id	id;
	/* Number of values/intervals. */
	uint32_t			val_cnt;
	/* Interval values and conditional probs. */
	struct di_nbayes_attr_disc_val	val[];
};

struct di_nbayes_attr_norm_class
{
	int32_t		mean;
	uint32_t	stddev;
	int32_t		wsum;
	uint32_t	prec;
};

/* One per real attribute. */
struct di_nbayes_attr_norm
{
	struct di_nbayes_attr_id		id;
	/* class_cnt structs. */
	struct di_nbayes_attr_norm_class	class[];
};

/* Priors, not really an attribute. */
struct di_nbayes_attr_prior
{
	struct di_nbayes_attr_id	id;
	/* class_cnt prior probs. */
	uint32_t			prior_p[];
};

struct di_classifier_nbayes_config
{
	struct di_oid			oid;

	/* Model name. */
	char				model_name[DI_MAX_MODEL_STR_LEN];
	/* Number of features. */
	uint16_t			feature_cnt;
	/* Number of classes. */
	uint16_t			class_cnt;
	/* Precision, multiplier for double->int. */
	uint16_t			multi;
	/* Length of fdists. */
	uint16_t			fdist_len;
	/* feature_cnt feature distributions. */
	struct di_nbayes_attr_id	fdist[];
};

struct di_classifier_module * nbayes_module(void);

struct di_cdata;

int nbayes_classify(struct di_cdata *cdata, int32_t *features, int fcnt);

#endif /* _NETINET_IPFW_DIFFUSE_CLASSIFIER_NBAYES_H_ */
