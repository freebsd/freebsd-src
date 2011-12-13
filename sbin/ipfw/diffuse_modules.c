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
 * Description:
 * Functions to manage classifier and feature UI modules.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>

#include <netinet/ipfw/diffuse_feature.h>
#include <netinet/ipfw/diffuse_classifier_nbayes.h>
#include <netinet/ipfw/diffuse_classifier_c45.h>
#include <netinet/ipfw/diffuse_feature_iat.h>
#include <netinet/ipfw/diffuse_feature_iatbd.h>
#include <netinet/ipfw/diffuse_feature_pcnt.h>
#include <netinet/ipfw/diffuse_feature_plen_common.h>
#include <netinet/ipfw/diffuse_feature_plen.h>
#include <netinet/ipfw/diffuse_feature_plenbd.h>
#include <netinet/ipfw/diffuse_feature_skype.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffuse_ui.h"

SLIST_HEAD(di_featuremods_head, di_feature_module);
SLIST_HEAD(di_classifiermods_head, di_classifier_module);

/* List of feature modules. */
static struct di_featuremods_head featuremods =
    SLIST_HEAD_INITIALIZER(featuremods);

/* List of classifier modules. */
static struct di_classifiermods_head classifiermods =
    SLIST_HEAD_INITIALIZER(classifiermods);

struct di_classifier_module *
find_classifier_module(const char *name)
{
	struct di_classifier_module *tmp;

	tmp = NULL;

	SLIST_FOREACH(tmp, &classifiermods, next) {
		if (strcmp(tmp->name, name) == 0)
			break;
	}

	return (tmp);
}

void
print_classifier_modules()
{
	struct di_classifier_module *tmp;

	SLIST_FOREACH(tmp, &classifiermods, next) {
		printf("%s ", tmp->name);
	}
	printf("\n");
}

struct di_feature_module *
find_feature_module(const char *name)
{
	struct di_feature_module *tmp;

	tmp = NULL;

	SLIST_FOREACH(tmp, &featuremods, next) {
		if (strcmp(tmp->name, name) == 0)
			break;
	}

	return (tmp);
}

void
print_feature_modules()
{
	struct di_feature_module *tmp;

	SLIST_FOREACH(tmp, &featuremods, next) {
		printf("%s ", tmp->name);
	}
	printf("\n");
}

void
diffuse_classifier_modules_init()
{

	SLIST_INIT(&classifiermods);
	SLIST_INSERT_HEAD(&classifiermods, c45_module(), next);
	SLIST_INSERT_HEAD(&classifiermods, nbayes_module(), next);
}

void
diffuse_feature_modules_init()
{

	SLIST_INIT(&featuremods);
	SLIST_INSERT_HEAD(&featuremods, iat_module(), next);
	SLIST_INSERT_HEAD(&featuremods, iatbd_module(), next);
	SLIST_INSERT_HEAD(&featuremods, pcnt_module(), next);
	SLIST_INSERT_HEAD(&featuremods, plen_module(), next);
	SLIST_INSERT_HEAD(&featuremods, plenbd_module(), next);
	SLIST_INSERT_HEAD(&featuremods, skype_module(), next);
}

void
diffuse_modules_init()
{

	diffuse_feature_modules_init();
	diffuse_classifier_modules_init();
#ifdef DIFFFUSE_DEBUG
	printf("Known features: ");
	print_feature_modules();
	printf("Known classifiers: ");
	print_classifier_modules();
#endif
}
