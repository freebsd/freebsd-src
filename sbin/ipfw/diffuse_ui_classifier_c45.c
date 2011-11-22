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
 * C4.5 classifier.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>

#include <netinet/ipfw/diffuse_classifier_c45.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "diffuse_ui.h"
#include "ipfw2.h"

enum classifier_c45_tokens {
	TOK_DI_MODEL_FILE = TOK_DI_FEATURE_MOD_START,
};

static struct di_option classifier_c45_params[] = {
	{ "model",	DI_OPTION_ARG_STR,	0,	0,
	    TOK_DI_MODEL_FILE },
	{ NULL, 0, 0 }  /* Terminator. */
};

struct c45_id {
	uint16_t val;
	uint8_t type;
};

int
c45_get_conf_size(void)
{

	return (sizeof(struct di_classifier_c45_config));
}

int
c45_get_opts(struct di_option **opts)
{
	*opts = classifier_c45_params;

	return (sizeof(classifier_c45_params));
}

int
c45_parse_opts(int token, char *arg_val, struct di_oid *buf)
{
	static struct di_classifier_c45_config *conf = NULL;

	if (conf == NULL) {
		conf = (struct di_classifier_c45_config *)buf;
		conf->model_name[0] = '\0';
		conf->class_cnt = 0;
		conf->feature_cnt = 0;
	}

	switch(token) {
	case TOK_DI_OPTS_INIT:
		break;

	case TOK_DI_MODEL_FILE:
		strncpy(conf->model_name, arg_val, DI_MAX_MODEL_STR_LEN - 1);
		conf->model_name[DI_MAX_MODEL_STR_LEN - 1] = '\0';
		break;

	default:
		/* This should never happen. */
		errx(EX_DATAERR, "invalid option, fix source");
	}

	return (0);
}

char *
print_id(int id, int type)
{
	static char buf[16];

	if (type & DI_C45_CLASS)
		sprintf(buf, "c_%u", id);
	else if (type & DI_C45_NODE)
		sprintf(buf, "n_%u", id);
	else
		sprintf(buf, "a_%u", id);

	return (buf);
}

void
c45_print_model(struct di_oid *opts, char *mod_data)
{
	struct di_classifier_c45_config *conf;
	struct di_c45_node_real *nodes;
	int i;

	conf = (struct di_classifier_c45_config *)opts;
	
	if (mod_data != NULL)
		nodes = (struct di_c45_node_real *)mod_data;
	else
	 	nodes = (struct di_c45_node_real *)conf->nodes;

	for(i = 0; i < conf->tree_len / sizeof(struct di_c45_node_real); i++) {
		printf("    n_%d a_%u %s c_%u ", i, nodes[i].nid.feature,
		    (nodes[i].nid.type & DI_C45_REAL) ? "r" : "n",
		    nodes[i].nid.missing_class);

		if (nodes[i].nid.type == DI_C45_BNOM) {
			/* XXX: No support for non-binary nominal yet. */
		} else {
			printf("%.2f ", (double)nodes[i].val / (1 << conf->multi));
			printf("%s ", print_id(nodes[i].le_id, nodes[i].le_type));
			printf("%s\n", print_id(nodes[i].gt_id, nodes[i].gt_type));
		}
	}
}

void
c45_print_opts(struct di_oid *opts)
{
	struct di_classifier_c45_config *conf;

	conf = (struct di_classifier_c45_config *)opts;

	printf("  model name: %s\n", conf->model_name);
	printf("  model:\n");
	c45_print_model(opts, NULL);
}

void
c45_print_usage()
{

	printf("algorithm c45 model <file-name>\n");
}

struct c45_id parse_id(unsigned int line_no, char *s, int type)
{
	char *p, *endptr;
	struct c45_id ret = {0, 0};
	uint16_t max_id;

	p = NULL;

	if (type & DI_C45_CLASS) {
		p = strstr(s, "c_");
		ret.type = DI_C45_CLASS;
	}
	if (p == NULL && (type & DI_C45_NODE)) {
		p = strstr(s, "n_");
		ret.type = DI_C45_NODE;
	}
	if (p == NULL && (type & DI_C45_FEAT)) {
		p = strstr(s, "a_");
		ret.type = DI_C45_FEAT;
	}

	if (p == NULL) {
		errx(EX_DATAERR, "model line %u: invalid class/node/feature "
		    "id specification %s", line_no, s);
	}

	if (ret.type == DI_C45_NODE)
		max_id = 4095;
	else
		max_id = 255;

	p += 2;
	ret.val = strtonum(p, 0, max_id, (const char **)&endptr);
	if (endptr != NULL) {
		errx(EX_DATAERR, "model line %u: invalid or out of range "
		    "(0--%d) class/node id %s", line_no, max_id, p);
	}

	return (ret);
}

int
c45_load_model(struct di_oid *buf, char **model, char **feature_str,
    char **class_str)
{
	struct di_classifier_c45_config *conf;
	struct di_c45_node_real *node, *nodes;
	struct c45_id x;
	/* Allocate these statically for now. */
	static char mod_data[65535], fstr[1024], clstr[1024];
	FILE *f;
	char *sep = " \t";
	char *endptr, *p, *word;
	char line[1024], tmp[1024];
	double v;
	unsigned int line_no;
	uint16_t max_nodes, multi;

	conf = (struct di_classifier_c45_config *)buf;
	line_no = 0;
	multi = 16; /* 2^multi */
	max_nodes = sizeof(mod_data) / sizeof(struct di_c45_node_real);

	if (conf->model_name == NULL)
		errx(EX_DATAERR, "no classifier model specified");

	fstr[0] = '\0';
	clstr[0] = '\0';

	/* The precision. */
	conf->multi = multi;

	/* Pointer to list of dists. */
	conf->tree_len = 0;
	nodes = (struct di_c45_node_real *)mod_data;

	if ((f = fopen(conf->model_name, "r")) != NULL) {
		while (fgets(line, sizeof(line), f) != NULL) {
			line_no++;

			/*
			 * Trim leading ws and check that we actually have at
			 * least 1 char.
			 */
			p = line;
			while (isspace(*p))
				p++;

			if (!strlen(p) || !sscanf(p, "%[^\r\n]\n", tmp))
				continue;

			word = strtok(tmp, sep);

			if (!strncmp(word, "#", 1)) {
				/* Ignore comments. */
				continue;
			} else if (!strcmp(word, "classes")) {
				word = strtok(NULL, sep);
				for (; word; word = strtok(NULL, sep)) {
					conf->class_cnt++;
					strcat(clstr, word);
					strcat(clstr, ",");
				}
				/* Get rid of last comma. */
				clstr[strlen(clstr) - 1] = '\0';
				continue;
			} else if (!strcmp(word, "attributes")) {
				word = strtok(NULL, sep);
				for (; word; word = strtok(NULL, sep)) {
					conf->feature_cnt++;
					strcat(fstr, word);
					strcat(fstr, ",");
				}
				/* Get rid of last comma. */
				fstr[strlen(fstr) - 1] = '\0';
				continue;
			} else {
				/* Tree node description. */
				endptr = NULL;
				node = NULL;

				x = parse_id(line_no, word, DI_C45_NODE);
				if (x.val > max_nodes) {
					errx(EX_DATAERR, "classifier model too "
					    "large (max %u nodes)\n",
					    max_nodes);
				}
				node = (struct di_c45_node_real *)&nodes[x.val];
				conf->tree_len = (x.val + 1) *
				    sizeof(struct di_c45_node_real);

				word = strtok(NULL, sep);
				if (!word)
					goto bad;

				x = parse_id(line_no, word, DI_C45_FEAT);
				node->nid.feature = x.val;
				word = strtok(NULL, sep);
				if (!word)
					goto bad;

				if (!strcmp(word, "r")) {
					node->nid.type = DI_C45_REAL;
				} else if (!strcmp(word, "n")) {
					node->nid.type = DI_C45_BNOM;
					/* XXX: Check if non-binary. */
				} else {
					errx(EX_DATAERR, "model line %u: "
					    "unknown node type %s", line_no,
					    word);
				}

				word=strtok(NULL, sep);
				if (!word)
					goto bad;

				x = parse_id(line_no, word, DI_C45_CLASS);
				node->nid.missing_class = x.val;

				/* Parse split values and classes. */
				word = strtok(NULL, sep);
				if (!word)
					goto bad;

				if (node->nid.type == DI_C45_NOM) {
					/*
					 * XXX: Non-binary nominal not supported
					 * yet.
					 */
					errx(EX_DATAERR, "model line %u: "
					    "non-binary nominal splits not "
					    "supported yet", line_no);
				} else {
					/*
					 * Binary nominal has the same
					 * structures as real.
					 */
					v = strtod(word, &endptr);
					if (endptr == NULL) {
						errx(EX_DATAERR, "model line "
						    "%u: split value not a "
						    "number %s", line_no, word);
					}
					node->val = (int64_t)round(v *
					    (1 << conf->multi));

					word = strtok(NULL, sep);
					if (!word)
						goto bad;

					x = parse_id(line_no, word,
					    DI_C45_NODE | DI_C45_CLASS);
					node->le_id = x.val;
					node->le_type = x.type;

					word = strtok(NULL, sep);
					if (!word)
						goto bad;

					x = parse_id(line_no, word,
					    DI_C45_NODE | DI_C45_CLASS);
					node->gt_id = x.val;
					node->gt_type = x.type;
				}
			}
bad:
			if (!word) {
				errx(EX_DATAERR,
				    "model line %u: missing value(s)", line_no);
			}
		}
		fclose(f);
	} else {
		errx(EX_DATAERR, "could not open classifier model %s",
		    conf->model_name);
	}

	if (conf->tree_len == 0)
		errx(EX_DATAERR, "empty classifier model %s", conf->model_name);

#ifdef DIFFUSE_DEBUG2
	printf("model file: %s\n", conf->model_name);
	printf("classes: %d\n", conf->class_cnt);
	printf("attributes: %d\n", conf->feature_cnt);
	printf("multi: %d\n", (1 << conf->multi));
	printf("tree_len: %d\n", conf->tree_len);
	c45_print_model(buf, mod_data);
#endif

	*model = mod_data;
	*feature_str = fstr;
	*class_str = clstr;

	return (conf->tree_len);
}

static struct di_classifier_module c45_classifier_module = {
	.name =			"c4.5",
	.get_conf_size =	c45_get_conf_size,
	.get_opts =		c45_get_opts,
	.parse_opts =		c45_parse_opts,
	.print_opts =		c45_print_opts,
	.print_usage =		c45_print_usage,
	.load_model =		c45_load_model
};

struct di_classifier_module *
c45_module(void)
{

	return (&c45_classifier_module);
}
