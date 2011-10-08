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
 */

/*
 * DIFFUSE Naive Bayes classifier.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#else
#include <sys/types.h>
#include <stdio.h>
#endif /* _KERNEL */
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <netinet/ipfw/diffuse_common.h>
#include <netinet/ipfw/diffuse_classifier.h>
#include <netinet/ipfw/diffuse_classifier_nbayes.h>
#ifdef _KERNEL
#include <netinet/ipfw/diffuse_classifier_module.h>
#include <netinet/ipfw/diffuse_private.h>
#endif

#ifdef _KERNEL

/* Computes the buffer size needed to store classifier. */
static int
get_size(struct di_classifier_nbayes_config *c)
{

	return (sizeof(struct di_classifier_nbayes_config) + c->fdist_len);
}

/* Copy, assumes target memory is allocated. */
static void
cpy_conf(struct di_classifier_nbayes_config *f,
    struct di_classifier_nbayes_config *t)
{

	t->oid = f->oid;
	strcpy(t->model_name, f->model_name);
	t->feature_cnt = f->feature_cnt;
	t->class_cnt = f->class_cnt;
	t->multi = f->multi;
	t->fdist_len = f->fdist_len;
	memcpy(t->fdist, f->fdist, f->fdist_len);
}

static int
nbayes_init_instance(struct di_cdata *cdata, struct di_oid *params)
{
	struct di_classifier_nbayes_config *c, *conf;

	c = (struct di_classifier_nbayes_config *)params;

	DID("class cnt %d", c->class_cnt);
	DID("attr cnt %d", c->feature_cnt);
	DID("multi %d", (1 << c->multi));
	DID("fdist_len %d", c->fdist_len);
	DID("want size %d", get_size(c));

	cdata->conf = malloc(get_size(c), M_DIFFUSE, M_NOWAIT | M_ZERO);
	if (cdata->conf == NULL)
		return (ENOMEM);

	conf = (struct di_classifier_nbayes_config *)cdata->conf;
	cpy_conf(c, conf);

	return (0);
}

static int
nbayes_destroy_instance(struct di_cdata *cdata)
{

	free(cdata->conf, M_DIFFUSE);

	return (0);
}

static int
nbayes_get_conf(struct di_cdata *cdata, struct di_oid *cbuf, int size_only)
{
	int len;

	len = get_size((struct di_classifier_nbayes_config *)cdata->conf);

	if (!size_only)
		cpy_conf((struct di_classifier_nbayes_config *)cdata->conf,
		    (struct di_classifier_nbayes_config *)cbuf);

	return (len);
}

#endif /* _KERNEL */

#define	LINEAR_SEARCH_THRESHOLD 5

static inline struct di_nbayes_attr_disc_val *
find_val(struct di_nbayes_attr_disc_val *first, int val_cnt, int class_cnt,
    int32_t fval)
{
	struct di_nbayes_attr_disc_val *val;
	int h, l, j, m;

	val = NULL;

	if (val_cnt <= LINEAR_SEARCH_THRESHOLD) {
		/* Linear search. */
		for (j = 0; j < val_cnt; j++) {
			val = (struct di_nbayes_attr_disc_val *)
			    (((char *)first) +
			    (sizeof(struct di_nbayes_attr_disc_val) +
			    class_cnt * sizeof(uint32_t)) * j);
			if (fval <= val->high_val)
				break;
		}

	} else {
		/* Binary search. */
		l = 0;
		h = val_cnt - 1;
		while (l < h) {
			m = l + ((h - l) / 2);
			val = (struct di_nbayes_attr_disc_val *)
			    (((char *)first) +
			    (sizeof(struct di_nbayes_attr_disc_val) +
			    class_cnt * sizeof(uint32_t)) * m);

			if (val->high_val < fval)
				l = m + 1;
			else
				h = m;
		}

		val = (struct di_nbayes_attr_disc_val *)(((char *)first) +
		    (sizeof(struct di_nbayes_attr_disc_val) + class_cnt *
		    sizeof(uint32_t)) * h);
	}

	return (val);
}

int
nbayes_classify(struct di_cdata *cdata, int32_t *features, int fcnt)
{
	struct di_classifier_nbayes_config *conf =
	    (struct di_classifier_nbayes_config *)cdata->conf;
	struct di_nbayes_attr_prior *ap;
	struct di_nbayes_attr_disc *ad;
	struct di_nbayes_attr_disc_val *val;
	struct di_nbayes_attr_id *attr;
	char *d;
	uint64_t probs[conf->class_cnt];
	uint64_t max_prob;
	int divs[conf->class_cnt];
	int best_class, divs_max, f, i, l, len;

	max_prob = 0;
	best_class = divs_max = 0;

#ifdef DIFFUSE_DEBUG2
	printf("DIFFUSE: %-10s features ", __func__);
	for (i = 0; i < fcnt; i++)
		printf("%u ", features[i]);
	printf("\n");
#endif

	for (i = 0; i < conf->class_cnt; i++) {
		divs[i] = 0;

		for (l = conf->fdist_len, d = (char *)conf->fdist, f = -1;
		    l > 0; l -= len, d += len, f++) {
			attr = (struct di_nbayes_attr_id *)d;
			len = attr->len;

			DID("type %d(%d)", attr->type, attr->len);

			if (f >= fcnt)
				return (-1); /* Should never happen. */

			if (attr->type == DI_NBAYES_ATTR_PRIOR) {
				ap = (struct di_nbayes_attr_prior *)attr;
				probs[i] = ap->prior_p[i];
			} else if (attr->type == DI_NBAYES_ATTR_DISC) {
				ad = (struct di_nbayes_attr_disc *)attr;

				val = find_val(ad->val, ad->val_cnt,
				    conf->class_cnt, features[f]);
				probs[i] *= val->cond_p[i];

				if (probs[i] >
				    ((uint64_t)1 << (64 - conf->multi))) {
					/*
					 * Divide by 2^(conf->multi - 1) to avoid
					 * overflow.
					 */
					probs[i] = fixp_div(probs[i], conf->multi);
					divs[i]++;
					if (divs[i] > divs_max)
						divs_max = divs[i];
				}
			} else if (attr->type == DI_NBAYES_ATTR_NORM) {
				return (-1); /* XXX: Not supported yet. */
			}
		}
	}

	/* Make sure the divisor is the same for all probs and find max prob. */
	for (i = 0; i < conf->class_cnt; i++) {
		probs[i] = fixp_div(probs[i], conf->multi *
		    (divs_max - divs[i]) - (divs_max - divs[i] - 1));

		DID2("class %u prob %llu %i", i, probs[i], divs[i]);

		if (probs[i] > max_prob) {
			max_prob = probs[i];
			best_class = i;
		}
	}

	return (best_class);
}

#ifdef _KERNEL

static int
nbayes_get_feature_cnt(struct di_cdata *cdata)
{

	return (((struct di_classifier_nbayes_config *)
	    cdata->conf)->feature_cnt);
}

static int
nbayes_get_class_cnt(struct di_cdata *cdata)
{

	return (((struct di_classifier_nbayes_config *)cdata->conf)->class_cnt);
}

static struct di_classifier_alg di_nbayes_desc = {
	_FI( .name = )			"nbayes",
	_FI( .ref_count = )		0,

	_FI( .init_instance = )		nbayes_init_instance,
	_FI( .destroy_instance = )	nbayes_destroy_instance,
	_FI( .get_conf = )		nbayes_get_conf,
	_FI( .classify = )		nbayes_classify,
	_FI( .get_feature_cnt = )	nbayes_get_feature_cnt,
	_FI( .get_class_cnt = )		nbayes_get_class_cnt,
};

DECLARE_DIFFUSE_CLASSIFIER_MODULE(nbayes, &di_nbayes_desc);

#endif
