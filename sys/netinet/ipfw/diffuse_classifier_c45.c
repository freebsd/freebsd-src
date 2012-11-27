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
 * DIFFUSE C4.5 classifier.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#endif /* _KERNEL */
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <netinet/ipfw/diffuse_common.h>
#include <netinet/ipfw/diffuse_classifier.h>
#include <netinet/ipfw/diffuse_classifier_c45.h>
#ifdef _KERNEL
#include <netinet/ipfw/diffuse_classifier_module.h>
#include <netinet/ipfw/diffuse_private.h>
#endif

#ifdef _KERNEL

/* Computes the buffer size needed to store classifier. */
static int
get_size(struct di_classifier_c45_config *c)
{

	return (sizeof(struct di_classifier_c45_config) + c->tree_len);
}

/* Assumes target memory is allocated. */
static void
cpy_conf(struct di_classifier_c45_config *f, struct di_classifier_c45_config *t)
{

	t->oid = f->oid;
	strcpy(t->model_name, f->model_name);
	t->feature_cnt = f->feature_cnt;
	t->class_cnt = f->class_cnt;
	t->multi = f->multi;
	t->tree_len = f->tree_len;
	memcpy(t->nodes, f->nodes, f->tree_len);
}

static int
c45_init_instance(struct di_cdata *cdata, struct di_oid *params)
{
	struct di_classifier_c45_config *c, *conf;

	c = (struct di_classifier_c45_config *)params;

	DID("class cnt %d", c->class_cnt);
	DID("attr cnt %d", c->feature_cnt);
	DID("multi %d", (1 << c->multi));
	DID("tree_len %d", c->tree_len);
	DID("want size %d", get_size(c));

	cdata->conf = malloc(get_size(c), M_DIFFUSE, M_NOWAIT | M_ZERO);
	if (cdata->conf == NULL)
		return (ENOMEM);

	conf = (struct di_classifier_c45_config *)cdata->conf;
	cpy_conf(c, conf);

	return (0);
}

static int
c45_destroy_instance(struct di_cdata *cdata)
{

	free(cdata->conf, M_DIFFUSE);

	return (0);
}

static int
c45_get_conf(struct di_cdata *cdata, struct di_oid *cbuf, int size_only)
{
	int len;

	len = get_size((struct di_classifier_c45_config*)cdata->conf);

	if (!size_only)
		cpy_conf((struct di_classifier_c45_config *)cdata->conf,
		    (struct di_classifier_c45_config *)cbuf);

	return (len);
}

#endif

int
c45_classify(struct di_cdata *cdata, int32_t *features, int fcnt)
{
	struct di_classifier_c45_config *conf;
	struct di_c45_node_real *nodes;
	uint64_t fval;
	int n;

	conf = (struct di_classifier_c45_config *)cdata->conf;
	nodes = (struct di_c45_node_real *)conf->nodes;
	n = 0;

#ifdef DIFFUSE_DEBUG2
	printf("DIFFUSE: %-10s features ", __func__);
	for (int i = 0; i < fcnt; i++)
		printf("%u ", features[i]);
	printf("\n");
#endif

	while (n < conf->tree_len / sizeof(struct di_c45_node_real)) {
		if (nodes[n].nid.feature > fcnt - 1)
			return (-1); /* Should never happen. */

		fval = features[nodes[n].nid.feature] * (1 << conf->multi);

		switch (nodes[n].nid.type) {
		case DI_C45_REAL:
			if (fval <= nodes[n].val) {
				if (nodes[n].le_type & DI_C45_CLASS)
					return (nodes[n].le_id);
				else
					n = nodes[n].le_id;
			} else {
				if (nodes[n].gt_type & DI_C45_CLASS)
					return (nodes[n].gt_id);
				else
					n = nodes[n].gt_id;
			}
			break;

		case DI_C45_BNOM:
			if (fval == nodes[n].val) {
				if (nodes[n].le_type & DI_C45_CLASS)
					return (nodes[n].le_id);
				else
					n = nodes[n].le_id;
			} else {
				if (nodes[n].gt_type & DI_C45_CLASS)
					return (nodes[n].gt_id);
				else
					n = nodes[n].gt_id;
			}
			break;

		case DI_C45_NOM:
			/* XXX: Not supported yet. */
			return (-1);
			break;

		default:
			break;
		}
	}

	return (-1);
}

#ifdef _KERNEL

static int
c45_get_feature_cnt(struct di_cdata *cdata)
{

	return (((struct di_classifier_c45_config *)cdata->conf)->feature_cnt);
}

static int
c45_get_class_cnt(struct di_cdata *cdata)
{

	return (((struct di_classifier_c45_config *)cdata->conf)->class_cnt);
}

static struct di_classifier_alg di_c45_desc = {
	_FI( .name = )			"c4.5",
	_FI( .ref_count = )		0,

	_FI( .init_instance = )		c45_init_instance,
	_FI( .destroy_instance = )	c45_destroy_instance,
	_FI( .get_conf = )		c45_get_conf,
	_FI( .classify = )		c45_classify,
	_FI( .get_feature_cnt = )	c45_get_feature_cnt,
	_FI( .get_class_cnt = )		c45_get_class_cnt,
};

DECLARE_DIFFUSE_CLASSIFIER_MODULE(c45, &di_c45_desc);

#endif
