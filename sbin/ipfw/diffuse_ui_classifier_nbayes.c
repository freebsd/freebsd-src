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
 * Naive bayes classifier.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>

#include <netinet/ipfw/diffuse_classifier_nbayes.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <math.h>

#include "diffuse_ui.h"
#include "ipfw2.h"

enum classifier_nbayes_tokens {
	TOK_DI_MODEL_FILE = TOK_DI_FEATURE_MOD_START,
};

static struct di_option classifier_nbayes_params[] = {
	{ "model",	DI_OPTION_ARG_STR,	0,	0,
	    TOK_DI_MODEL_FILE },
	{ NULL, 0, 0 }  /* Terminator. */
};

int
nbayes_get_conf_size(void)
{

	return (sizeof(struct di_classifier_nbayes_config));
}

int
nbayes_get_opts(struct di_option **opts)
{

	*opts = classifier_nbayes_params;

	return (sizeof(classifier_nbayes_params));
}

int
nbayes_parse_opts(int token, char *arg_val, struct di_oid *buf)
{
	static struct di_classifier_nbayes_config *conf = NULL;

	if (conf == NULL) {
		conf = (struct di_classifier_nbayes_config *)buf;
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

void
nbayes_print_model(struct di_oid *opts, char *mod_data)
{
	struct di_classifier_nbayes_config *conf;
	struct di_nbayes_attr_disc *disc;
	struct di_nbayes_attr_disc_val *val;
	struct di_nbayes_attr_id *id;
	struct di_nbayes_attr_norm *norm;
	struct di_nbayes_attr_prior *prior;
	char *dst;
	int fcnt, i, j, l, len;

	conf = (struct di_classifier_nbayes_config *)opts;
	fcnt = 0;

	if (mod_data != NULL)
		dst = mod_data;
	else
		dst = (char *)conf->fdist;

	for (l = conf->fdist_len; l > 0; dst += len, l -= len) {
		id = (struct di_nbayes_attr_id *)dst;
		len = id->len;
#ifdef DIFFUSE_DEBUG2
		printf("    type %d(%d)\n", id->type, id->len);
#endif
		switch(id->type) {
		case DI_NBAYES_ATTR_PRIOR:
			prior = (struct di_nbayes_attr_prior *)dst;
			if (id->len < sizeof(struct di_nbayes_attr_prior)) {
				errx(EX_DATAERR, "invalid classifier model");
				goto done;
			}
			printf("    prior ");
			for (i = 0; i < conf->class_cnt; i++)
				printf("%d ", prior->prior_p[i]);
			printf("\n");
			break;

		case DI_NBAYES_ATTR_DISC:
			disc = (struct di_nbayes_attr_disc *)dst;
			if (id->len < sizeof(struct di_nbayes_attr_disc)) {
				errx(EX_DATAERR, "invalid classifier model");
				goto done;
			}
			for (i = 0; i < disc->val_cnt; i++) {
				val = (struct di_nbayes_attr_disc_val *)
				    (((char *)disc->val) +
				    (sizeof(struct di_nbayes_attr_disc_val) +
				    conf->class_cnt * sizeof(uint32_t)) * i);

				printf("    a_%d %d ", fcnt, val->high_val);
				for (j = 0; j < conf->class_cnt; j++)
					printf("%u ", val->cond_p[j]);
				printf("\n");
			}
			fcnt++;
			break;

		case DI_NBAYES_ATTR_NORM:
			norm = (struct di_nbayes_attr_norm *)dst;
			if (id->len < sizeof(struct di_nbayes_attr_norm)) {
				errx(EX_DATAERR, "invalid classifier model");
				goto done;
			}
			printf("    a_%d mean ", fcnt);
			for (i = 0; i < conf->class_cnt; i++)
				printf("%d ", norm->class[i].mean);

			printf("\n");
			printf("    a_%d stddev ", fcnt);
			for (i = 0; i < conf->class_cnt; i++)
				printf("%u ", norm->class[i].stddev);

			printf("\n");
			/* Weight, precision missing. */
			fcnt++;
			break;

		default:
			errx(EX_DATAERR, "unknown feature dist type");
			break;
		}
	}
done:
	return;
}

void
nbayes_print_opts(struct di_oid *opts)
{
	struct di_classifier_nbayes_config *conf;

	conf = (struct di_classifier_nbayes_config *)opts;

	printf("  model name: %s\n", conf->model_name);
	printf("  model:\n");
	nbayes_print_model(opts, NULL);
}

void
nbayes_print_usage()
{

	printf("algorithm nbayes model <file-name>\n");
}

int
nbayes_load_model(struct di_oid *buf, char **model, char **feature_str,
    char **class_str)
{
	struct di_classifier_nbayes_config *conf;
	struct di_nbayes_attr_disc *disc;
	struct di_nbayes_attr_disc_val *val;
	struct di_nbayes_attr_id *id;
	struct di_nbayes_attr_norm *norm;
	struct di_nbayes_attr_prior *prior;
	/* Allocate these statically for now. */
	static char mod_data[65535], fstr[1024], clstr[1024];
	FILE *f;
	char *sep = " \t";
	char *dst, *endptr, *first, *p, *second, *word;
	char line[1024], tmp[1024], last_attr[128];
	double v;
	int i;
	unsigned int line_no;
	uint16_t multi;

	conf = (struct di_classifier_nbayes_config *)buf;
	line_no = 0;
	multi = 16; /* 2^multi. */

	if (conf->model_name == NULL)
		errx(EX_DATAERR, "no classifier model specified");

	fstr[0] = '\0';
	clstr[0] = '\0';

	/* The precision. */
	conf->multi = multi;

	/* Pointer to list of dists. */
	conf->fdist_len = 0;
	dst = (char *)mod_data;

	if ((f = fopen(conf->model_name, "r")) != NULL) {
		last_attr[0] = '\0';
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

			first = strtok(tmp, sep);
			if (!strncmp(first, "#", 1)) {
				/* Ignore comments. */
				continue;
			} else if (!strcmp(first, "classes")) {
				word = strtok(NULL, sep);
				for (; word; word = strtok(NULL, sep)) {
					conf->class_cnt++;
					strcat(clstr, word);
					strcat(clstr, ",");
				}
				/* Get rid of last comma. */
				clstr[strlen(clstr)-1] = '\0';
			} else if (!strcmp(first, "attributes")) {
				word = strtok(NULL, sep);
				for (; word; word = strtok(NULL, sep)) {
					conf->feature_cnt++;
					strcat(fstr, word);
					strcat(fstr, ",");
				}
				/* Get rid of last comma. */
				fstr[strlen(fstr) - 1] = '\0';
			} else if (!strcmp(first, "prior")) {
				prior = (struct di_nbayes_attr_prior *)dst;
				prior->id.type = DI_NBAYES_ATTR_PRIOR;
				prior->id.len =
				    sizeof(struct di_nbayes_attr_id) +
				    conf->class_cnt * sizeof(uint32_t);

				for (word = strtok(NULL, sep), i = 0; word;
				    word = strtok(NULL, sep), i++) {
					endptr = NULL;
					v = strtod(word, &endptr);
					if (endptr == NULL) {
						errx(EX_DATAERR, "model line "
						    "%u: prior probability not "
						    "a number %s", line_no,
						    word);
					}
					prior->prior_p[i] = (uint32_t)round(v *
					    (1 << multi));
				}
				conf->fdist_len += prior->id.len;
				dst += prior->id.len;
				if (dst > mod_data + sizeof(mod_data)) {
					errx(EX_DATAERR, "classifier model too "
					    "large (max 64kB)\n");
				}
			} else {
				/* Must be an attribute line. */
				if (strcmp(first, last_attr)) {
					/* Attribute complete. */
					id = (struct di_nbayes_attr_id *)dst;
					conf->fdist_len += id->len;
					dst += id->len;
					if (dst > mod_data + sizeof(mod_data)) {
						errx(EX_DATAERR,
						    "classifier model too "
						    "large (max 64kB)\n");
					}
				}
				second = word = strtok(NULL, sep);

				if (!strcmp(word, "mean") ||
				    !strcmp(word, "stddev") ||
				    !strcmp(word, "weightsum") ||
				    !strcmp(word, "precision")) {
					/* Normal. */
					norm = (struct di_nbayes_attr_norm *)dst;
					if (strcmp(first, last_attr)) {
						norm->id.type =
						    DI_NBAYES_ATTR_NORM;
						norm->id.len = sizeof(
						    struct di_nbayes_attr_norm);
					}
					for (word = strtok(NULL, sep), i = 0;
					    word;
					    word = strtok(NULL, sep), i++) {
						v = strtod(word, &endptr);
						if (endptr == NULL) {
							errx(EX_DATAERR,
							    "model line %u: %s "
							    "value not a "
							    "number %s",
							    line_no, second,
							    word);
						}

						if (!strcmp(second, "mean")) {
							norm->class[i].mean =
							    (int32_t)round(v *
							    (1 << multi));
						} else if (!strcmp(second,
						    "stddev")) {
							norm->class[i].stddev =
							    (uint32_t)round(v *
							    (1 << multi));
						} else if (!strcmp(second,
						    "weightsum")) {
							norm->class[i].wsum =
							    (int32_t)round(v *
							    (1 << multi));
						} else {
							norm->class[i].prec =
							    (uint32_t)round(v *
							    (1 << multi));
						}
					}

					if (i != conf->class_cnt) {
						errx(EX_DATAERR,
						    "model line %u: %s %s has "
						    "less values than classes",
						    line_no, first, second);
					}
					norm->id.len += conf->class_cnt *
					    sizeof(uint32_t);

				} else {
					/* Discrete. */
					disc = (struct di_nbayes_attr_disc *)dst;
					if (strcmp(first, last_attr)) {
						disc->id.type =
						    DI_NBAYES_ATTR_DISC;
						disc->id.len = sizeof(
						    struct di_nbayes_attr_disc);
						disc->val_cnt = 1;
					} else {
						disc->val_cnt++;
					}

					val = (struct di_nbayes_attr_disc_val *)
					    (((char *)disc->val) + (sizeof(
					    struct di_nbayes_attr_disc_val) +
					    conf->class_cnt *
					    sizeof(uint32_t)) *
					    (disc->val_cnt - 1));

					/*
					 * Parse interval value.
					 * Interval: "low-high" || "All",
					 * values can be negative/positive
					 * numbers or "-inf" or "inf".
					 *
					 * XXX: Implement single nominal value?
					 */

					/*
					 * Find "-" after first "(-)inf" or
					 * number.
					 */
					p = strchr(&word[1], '-');
					if (p != NULL)
						p++;

					/* Else assume single nominal value. */
					if (!p || !strncmp(p, "inf", 3)) {
						/*
						 * !p catches the case when
						 * interval = "All".
						 */
						val->high_val = 0x7FFFFFFF;
					} else {
						v = strtod(p, &endptr);
						if (endptr == NULL) {
							errx(EX_DATAERR,
							    "model line %u: "
							    "interval high val "
							    "not a number %s",
							    line_no, p);
						}
						/*
						 * We test if high_val >=
						 * feature and with integer
						 * features, intervals will be
						 * on .5, e.g. if the value is
						 * 40 we have an interval of
						 * 39.5-40.5. So, for >= 0 use
						 * floor and for <0 use ceil.
						 */
						if (v >= 0) {
							val->high_val =
							    (int32_t)floor(v);
						} else {
							val->high_val =
							    (int32_t)ceil(v);
						}
					}

					/* Parse probabilities. */
					for (word = strtok(NULL, sep), i = 0;
					    word;
					    word = strtok(NULL, sep), i++) {
						v = strtod(word, &endptr);
						if (endptr == NULL) {
							errx(EX_DATAERR,
							    "model line %u: "
							    "conditional "
							    "probability not a "
							    "number %s",
							    line_no, word);
						}
						val->cond_p[i] =
						    (uint32_t)round(v *
						    (1 << multi));
					}
					if (i != conf->class_cnt) {
						errx(EX_DATAERR,
						    "model line %u: %s %s has "
						    "less values than classes",
						    line_no, first, second);
					}

					disc->id.len += sizeof(int32_t) +
					    conf->class_cnt * sizeof(uint32_t);
				}

				strncpy(last_attr, first,
				    sizeof(last_attr) - 1);
				last_attr[sizeof(last_attr) - 1] = '\0';
			}
		}
		id = (struct di_nbayes_attr_id *)dst;
		conf->fdist_len += id->len;

		fclose(f);
	} else {
		errx(EX_DATAERR, "could not open classifier model %s",
		    conf->model_name);
	}

	if (conf->fdist_len == 0)
		errx(EX_DATAERR, "empty classifier model %s", conf->model_name);

#ifdef DIFFUSE_DEBUG2
	printf("model: %s\n", conf->model_name);
	printf("classes: %d\n", conf->class_cnt);
	printf("attributes: %d\n", conf->feature_cnt);
	printf("multi: %d\n", (1 << conf->multi));
	printf("fdist_len: %d\n", conf->fdist_len);
	nbayes_print_model(buf, mod_data);
#endif

	*model = mod_data;
	*feature_str = fstr;
	*class_str = clstr;

	return (conf->fdist_len);
}

static struct di_classifier_module nbayes_classifier_module = {
	.name =			"nbayes",
	.get_conf_size =	nbayes_get_conf_size,
	.get_opts =		nbayes_get_opts,
	.parse_opts =		nbayes_parse_opts,
	.print_opts =		nbayes_print_opts,
	.print_usage =		nbayes_print_usage,
	.load_model =		nbayes_load_model
};

struct di_classifier_module *
nbayes_module(void)
{

	return (&nbayes_classifier_module);
}
