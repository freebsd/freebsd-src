/*
 * Copyright (c) 2006-2007 Voltaire, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Implementation of opensm partition management configuration
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <iba/ib_types.h>
#include <opensm/osm_base.h>
#include <opensm/osm_partition.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>

#include <complib/cl_byteswap.h>

struct part_conf {
	osm_log_t *p_log;
	osm_subn_t *p_subn;
	osm_prtn_t *p_prtn;
	unsigned is_ipoib, mtu, rate, sl, scope_mask;
	boolean_t full;
};

extern osm_prtn_t *osm_prtn_make_new(osm_log_t * p_log, osm_subn_t * p_subn,
				     const char *name, uint16_t pkey);
extern ib_api_status_t osm_prtn_add_all(osm_log_t * p_log,
					osm_subn_t * p_subn,
					osm_prtn_t * p, boolean_t full);
extern ib_api_status_t osm_prtn_add_port(osm_log_t * p_log,
					 osm_subn_t * p_subn, osm_prtn_t * p,
					 ib_net64_t guid, boolean_t full);
extern ib_api_status_t osm_prtn_add_mcgroup(osm_log_t * p_log,
					    osm_subn_t * p_subn, osm_prtn_t * p,
					    uint8_t rate,
					    uint8_t mtu, uint8_t scope);

static int partition_create(unsigned lineno, struct part_conf *conf,
			    char *name, char *id, char *flag, char *flag_val)
{
	uint16_t pkey;
	unsigned int scope;

	if (!id && name && isdigit(*name)) {
		id = name;
		name = NULL;
	}

	if (id) {
		char *end;

		pkey = (uint16_t) strtoul(id, &end, 0);
		if (end == id || *end)
			return -1;
	} else
		pkey = 0;

	conf->p_prtn = osm_prtn_make_new(conf->p_log, conf->p_subn,
					 name, cl_hton16(pkey));
	if (!conf->p_prtn)
		return -1;

	if (!conf->p_subn->opt.qos && conf->sl != OSM_DEFAULT_SL) {
		OSM_LOG(conf->p_log, OSM_LOG_DEBUG, "Overriding SL %d"
			" to default SL %d on partition %s"
			" as QoS is not enabled.\n",
			conf->sl, OSM_DEFAULT_SL, name);
		conf->sl = OSM_DEFAULT_SL;
	}
	conf->p_prtn->sl = (uint8_t) conf->sl;

	if (!conf->is_ipoib)
		return 0;

	if (!conf->scope_mask) {
		osm_prtn_add_mcgroup(conf->p_log, conf->p_subn, conf->p_prtn,
				     (uint8_t) conf->rate,
				     (uint8_t) conf->mtu,
				     0);
		return 0;
	}

	for (scope = 0; scope < 16; scope++) {
		if (((1<<scope) & conf->scope_mask) == 0)
			continue;

		osm_prtn_add_mcgroup(conf->p_log, conf->p_subn, conf->p_prtn,
				     (uint8_t) conf->rate,
				     (uint8_t) conf->mtu,
				     (uint8_t) scope);
	}
	return 0;
}

static int partition_add_flag(unsigned lineno, struct part_conf *conf,
			      char *flag, char *val)
{
	int len = strlen(flag);
	if (!strncmp(flag, "ipoib", len)) {
		conf->is_ipoib = 1;
	} else if (!strncmp(flag, "mtu", len)) {
		if (!val || (conf->mtu = strtoul(val, NULL, 0)) == 0)
			OSM_LOG(conf->p_log, OSM_LOG_VERBOSE,
				"PARSE WARN: line %d: "
				"flag \'mtu\' requires valid value"
				" - skipped\n", lineno);
	} else if (!strncmp(flag, "rate", len)) {
		if (!val || (conf->rate = strtoul(val, NULL, 0)) == 0)
			OSM_LOG(conf->p_log, OSM_LOG_VERBOSE,
				"PARSE WARN: line %d: "
				"flag \'rate\' requires valid value"
				" - skipped\n", lineno);
	} else if (!strncmp(flag, "scope", len)) {
		unsigned int scope;
		if (!val || (scope = strtoul(val, NULL, 0)) == 0 || scope > 0xF)
			OSM_LOG(conf->p_log, OSM_LOG_VERBOSE,
				"PARSE WARN: line %d: "
				"flag \'scope\' requires valid value"
				" - skipped\n", lineno);
		else
			conf->scope_mask |= (1<<scope);
	} else if (!strncmp(flag, "sl", len)) {
		unsigned sl;
		char *end;

		if (!val || !*val || (sl = strtoul(val, &end, 0)) > 15 ||
		    (*end && !isspace(*end)))
			OSM_LOG(conf->p_log, OSM_LOG_VERBOSE,
				"PARSE WARN: line %d: "
				"flag \'sl\' requires valid value"
				" - skipped\n", lineno);
		else
			conf->sl = sl;
	} else if (!strncmp(flag, "defmember", len)) {
		if (!val || (strncmp(val, "limited", strlen(val))
			     && strncmp(val, "full", strlen(val))))
			OSM_LOG(conf->p_log, OSM_LOG_VERBOSE,
				"PARSE WARN: line %d: "
				"flag \'defmember\' requires valid value (limited or full)"
				" - skipped\n", lineno);
		else
			conf->full = strncmp(val, "full", strlen(val)) == 0;
	} else {
		OSM_LOG(conf->p_log, OSM_LOG_VERBOSE,
			"PARSE WARN: line %d: "
			"unrecognized partition flag \'%s\'"
			" - ignored\n", lineno, flag);
	}
	return 0;
}

static int partition_add_port(unsigned lineno, struct part_conf *conf,
			      char *name, char *flag)
{
	osm_prtn_t *p = conf->p_prtn;
	ib_net64_t guid;
	boolean_t full = conf->full;

	if (!name || !*name || !strncmp(name, "NONE", strlen(name)))
		return 0;

	if (flag) {
		/* reset default membership to limited */
		full = FALSE;
		if (!strncmp(flag, "full", strlen(flag)))
			full = TRUE;
		else if (strncmp(flag, "limited", strlen(flag))) {
			OSM_LOG(conf->p_log, OSM_LOG_VERBOSE,
				"PARSE WARN: line %d: "
				"unrecognized port flag \'%s\'."
				" Assume \'limited\'\n", lineno, flag);
		}
	}

	if (!strncmp(name, "ALL", strlen(name))) {
		return osm_prtn_add_all(conf->p_log, conf->p_subn, p,
					full) == IB_SUCCESS ? 0 : -1;
	} else if (!strncmp(name, "SELF", strlen(name))) {
		guid = cl_ntoh64(conf->p_subn->sm_port_guid);
	} else {
		char *end;
		guid = strtoull(name, &end, 0);
		if (!guid || *end)
			return -1;
	}

	if (osm_prtn_add_port(conf->p_log, conf->p_subn, p,
			      cl_hton64(guid), full) != IB_SUCCESS)
		return -1;

	return 0;
}

/* conf file parser */

#define STRIP_HEAD_SPACES(p) while (*(p) == ' ' || *(p) == '\t' || \
		*(p) == '\n') { (p)++; }
#define STRIP_TAIL_SPACES(p) { char *q = (p) + strlen(p); \
				while ( q != (p) && ( *q == '\0' || \
					*q == ' ' || *q == '\t' || \
					*q == '\n')) { *q-- = '\0'; }; }

static int parse_name_token(char *str, char **name, char **val)
{
	int len = 0;
	char *p, *q;

	*name = *val = NULL;

	p = str;

	while (*p == ' ' || *p == '\t' || *p == '\n')
		p++;

	q = strchr(p, '=');
	if (q)
		*q++ = '\0';

	len = strlen(str) + 1;
	str = q;

	q = p + strlen(p);
	while (q != p && (*q == '\0' || *q == ' ' || *q == '\t' || *q == '\n'))
		*q-- = '\0';

	*name = p;

	p = str;
	if (!p)
		return len;

	while (*p == ' ' || *p == '\t' || *p == '\n')
		p++;

	q = p + strlen(p);
	len += (int)(q - str) + 1;
	while (q != p && (*q == '\0' || *q == ' ' || *q == '\t' || *q == '\n'))
		*q-- = '\0';
	*val = p;

	return len;
}

static struct part_conf *new_part_conf(osm_log_t * p_log, osm_subn_t * p_subn)
{
	static struct part_conf part;
	struct part_conf *conf = &part;

	memset(conf, 0, sizeof(*conf));
	conf->p_log = p_log;
	conf->p_subn = p_subn;
	conf->p_prtn = NULL;
	conf->is_ipoib = 0;
	conf->sl = OSM_DEFAULT_SL;
	conf->full = FALSE;
	return conf;
}

static int flush_part_conf(struct part_conf *conf)
{
	memset(conf, 0, sizeof(*conf));
	return 0;
}

static int parse_part_conf(struct part_conf *conf, char *str, int lineno)
{
	int ret, len = 0;
	char *name, *id, *flag, *flval;
	char *q, *p;

	p = str;
	if (*p == '\t' || *p == '\0' || *p == '\n')
		p++;

	len += (int)(p - str);
	str = p;

	if (conf->p_prtn)
		goto skip_header;

	q = strchr(p, ':');
	if (!q) {
		OSM_LOG(conf->p_log, OSM_LOG_ERROR, "PARSE ERROR: line %d: "
			"no partition definition found\n", lineno);
		fprintf(stderr, "\nPARSE ERROR: line %d: "
			"no partition definition found\n", lineno);
		return -1;
	}

	*q++ = '\0';
	str = q;

	name = id = flag = flval = NULL;

	q = strchr(p, ',');
	if (q)
		*q = '\0';

	ret = parse_name_token(p, &name, &id);
	p += ret;
	len += ret;

	while (q) {
		flag = flval = NULL;
		q = strchr(p, ',');
		if (q)
			*q++ = '\0';
		ret = parse_name_token(p, &flag, &flval);
		if (!flag) {
			OSM_LOG(conf->p_log, OSM_LOG_ERROR,
				"PARSE ERROR: line %d: "
				"bad partition flags\n", lineno);
			fprintf(stderr, "\nPARSE ERROR: line %d: "
				"bad partition flags\n", lineno);
			return -1;
		}
		p += ret;
		len += ret;
		partition_add_flag(lineno, conf, flag, flval);
	}

	if (p != str || (partition_create(lineno, conf,
					  name, id, flag, flval) < 0)) {
		OSM_LOG(conf->p_log, OSM_LOG_ERROR, "PARSE ERROR: line %d: "
			"bad partition definition\n", lineno);
		fprintf(stderr, "\nPARSE ERROR: line %d: "
			"bad partition definition\n", lineno);
		return -1;
	}

skip_header:
	do {
		name = flag = NULL;
		q = strchr(p, ',');
		if (q)
			*q++ = '\0';
		ret = parse_name_token(p, &name, &flag);
		if (partition_add_port(lineno, conf, name, flag) < 0) {
			OSM_LOG(conf->p_log, OSM_LOG_ERROR,
				"PARSE ERROR: line %d: "
				"bad PortGUID\n", lineno);
			fprintf(stderr, "PARSE ERROR: line %d: "
				"bad PortGUID\n", lineno);
			return -1;
		}
		p += ret;
		len += ret;
	} while (q);

	return len;
}

int osm_prtn_config_parse_file(osm_log_t * p_log, osm_subn_t * p_subn,
			       const char *file_name)
{
	char line[1024];
	struct part_conf *conf = NULL;
	FILE *file;
	int lineno;

	file = fopen(file_name, "r");
	if (!file) {
		OSM_LOG(p_log, OSM_LOG_VERBOSE,
			"Cannot open config file \'%s\': %s\n",
			file_name, strerror(errno));
		return -1;
	}

	lineno = 0;

	while (fgets(line, sizeof(line) - 1, file) != NULL) {
		char *q, *p = line;

		lineno++;

		p = line;

		q = strchr(p, '#');
		if (q)
			*q = '\0';

		do {
			int len;
			while (*p == ' ' || *p == '\t' || *p == '\n')
				p++;
			if (*p == '\0')
				break;

			if (!conf && !(conf = new_part_conf(p_log, p_subn))) {
				OSM_LOG(conf->p_log, OSM_LOG_ERROR,
					"PARSE ERROR: line %d: "
					"internal: cannot create config\n",
					lineno);
				fprintf(stderr,
					"PARSE ERROR: line %d: "
					"internal: cannot create config\n",
					lineno);
				break;
			}

			q = strchr(p, ';');
			if (q)
				*q = '\0';

			len = parse_part_conf(conf, p, lineno);
			if (len < 0) {
				break;
			}

			p += len;

			if (q) {
				flush_part_conf(conf);
				conf = NULL;
			}
		} while (q);
	}

	fclose(file);

	return 0;
}
