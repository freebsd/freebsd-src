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

#ifndef _NETINET_IPFW_DIFFUSE_CLASSIFIER_H_
#define _NETINET_IPFW_DIFFUSE_CLASSIFIER_H_

struct di_cdata;
struct di_fdata;

/*
 * Descriptor for a classifier. Contains all function pointers for a given
 * classifier. This is typically created when a module is loaded, and stored in
 * a global list of classifiers.
 */
struct di_classifier_alg {
	const char *name;	/* Classifier name. */
	volatile int ref_count;	/* Number of instances in the system. */

	/*
	 * Init instance.
	 * param1: pointer to instance config
	 * param2: config from userspace
	 * return: non-zero on error
	 */
	int (*init_instance)(struct di_cdata *, struct di_oid *);

	/*
	 * Destroy instance.
	 * param1: pointer to instance config
	 * return: non-zero on error
	 */
	int (*destroy_instance)(struct di_cdata *);

	/*
	 * Classify packet (sub flow).
	 * param1: pointer to instance config
	 * param2: pointer to features
	 * param3: number of features
	 * return: class
	 */
	int (*classify)(struct di_cdata *, int32_t *, int);

	/*
	 * Get configuration data.
	 * param1: pointer to instance config
	 * param2: pointer to configuration
	 * param3: only compute size (if 1)
	 * return: number of stats
	 */
	int (*get_conf)(struct di_cdata *, struct di_oid *, int);

	/*
	 * Get number of features needed.
	 * param1: pointer to instance config
	 * return: number of features
	 */
	int (*get_feature_cnt)(struct di_cdata *);

	/*
	 * Get number of classes.
	 * param1: pointer to instance config
	 * return: number of classes
	 */
	int (*get_class_cnt)(struct di_cdata *);

	SLIST_ENTRY(di_classifier_alg) next; /* Next in the list. */
};

#endif /* _NETINET_IPFW_DIFFUSE_CLASSIFIER_H_ */
