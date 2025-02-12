/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 iXsystems Inc.
 * All rights reserved.
 *
 * This software was developed by Jakub Klama <jceel@FreeBSD.org>
 * under sponsorship from iXsystems Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/nv.h>
#include <sys/queue.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ucl.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "ctld.h"

static struct conf *conf = NULL;

static bool uclparse_toplevel(const ucl_object_t *);
static bool uclparse_chap(struct auth_group *, const ucl_object_t *);
static bool uclparse_chap_mutual(struct auth_group *, const ucl_object_t *);
static bool uclparse_lun(const char *, const ucl_object_t *);
static bool uclparse_auth_group(const char *, const ucl_object_t *);
static bool uclparse_portal_group(const char *, const ucl_object_t *);
static bool uclparse_target(const char *, const ucl_object_t *);
static bool uclparse_target_portal_group(struct target *, const ucl_object_t *);
static bool uclparse_target_lun(struct target *, const ucl_object_t *);

static bool
uclparse_chap(struct auth_group *auth_group, const ucl_object_t *obj)
{
	const struct auth *ca;
	const ucl_object_t *user, *secret;

	assert(auth_group != NULL);
	user = ucl_object_find_key(obj, "user");
	if (!user || user->type != UCL_STRING) {
		log_warnx("chap section in auth-group \"%s\" is missing "
		    "\"user\" string key", auth_group->ag_name);
		return (false);
	}

	secret = ucl_object_find_key(obj, "secret");
	if (!secret || secret->type != UCL_STRING) {
		log_warnx("chap section in auth-group \"%s\" is missing "
		    "\"secret\" string key", auth_group->ag_name);
	}

	ca = auth_new_chap(auth_group,
	    ucl_object_tostring(user),
	    ucl_object_tostring(secret));

	if (ca == NULL)
		return (false);

	return (true);
}

static bool
uclparse_chap_mutual(struct auth_group *auth_group, const ucl_object_t *obj)
{
	const struct auth *ca;
	const ucl_object_t *user, *secret, *mutual_user;
	const ucl_object_t *mutual_secret;

	assert(auth_group != NULL);
	user = ucl_object_find_key(obj, "user");
	if (!user || user->type != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"user\" string key", auth_group->ag_name);
		return (false);
	}

	secret = ucl_object_find_key(obj, "secret");
	if (!secret || secret->type != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"secret\" string key", auth_group->ag_name);
		return (false);
	}

	mutual_user = ucl_object_find_key(obj, "mutual-user");
	if (!user || user->type != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"mutual-user\" string key", auth_group->ag_name);
		return (false);
	}

	mutual_secret = ucl_object_find_key(obj, "mutual-secret");
	if (!secret || secret->type != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"mutual-secret\" string key", auth_group->ag_name);
		return (false);
	}

	ca = auth_new_chap_mutual(auth_group,
	    ucl_object_tostring(user),
	    ucl_object_tostring(secret),
	    ucl_object_tostring(mutual_user),
	    ucl_object_tostring(mutual_secret));

	if (ca == NULL)
		return (false);

	return (true);
}

static bool
uclparse_target_portal_group(struct target *target, const ucl_object_t *obj)
{
	struct portal_group *tpg;
	struct auth_group *tag = NULL;
	struct port *tp;
	const ucl_object_t *portal_group, *auth_group;

	portal_group = ucl_object_find_key(obj, "name");
	if (!portal_group || portal_group->type != UCL_STRING) {
		log_warnx("portal-group section in target \"%s\" is missing "
		    "\"name\" string key", target->t_name);
		return (false);
	}

	auth_group = ucl_object_find_key(obj, "auth-group-name");
	if (auth_group && auth_group->type != UCL_STRING) {
		log_warnx("portal-group section in target \"%s\" is missing "
		    "\"auth-group-name\" string key", target->t_name);
		return (false);
	}


	tpg = portal_group_find(conf, ucl_object_tostring(portal_group));
	if (tpg == NULL) {
		log_warnx("unknown portal-group \"%s\" for target "
		    "\"%s\"", ucl_object_tostring(portal_group), target->t_name);
		return (false);
	}

	if (auth_group) {
		tag = auth_group_find(conf, ucl_object_tostring(auth_group));
		if (tag == NULL) {
			log_warnx("unknown auth-group \"%s\" for target "
			    "\"%s\"", ucl_object_tostring(auth_group),
			    target->t_name);
			return (false);
		}
	}

	tp = port_new(conf, target, tpg);
	if (tp == NULL) {
		log_warnx("can't link portal-group \"%s\" to target "
		    "\"%s\"", ucl_object_tostring(portal_group), target->t_name);
		return (false);
	}
	tp->p_auth_group = tag;

	return (true);
}

static bool
uclparse_target_lun(struct target *target, const ucl_object_t *obj)
{
	struct lun *lun;
	uint64_t tmp;

	if (obj->type == UCL_INT) {
		char *name;

		tmp = ucl_object_toint(obj);
		if (tmp >= MAX_LUNS) {
			log_warnx("LU number %ju in target \"%s\" is too big",
			    tmp, target->t_name);
			return (false);
		}

		asprintf(&name, "%s,lun,%ju", target->t_name, tmp);
		lun = lun_new(conf, name);
		if (lun == NULL)
			return (false);

		lun_set_scsiname(lun, name);
		target->t_luns[tmp] = lun;
		return (true);
	}

	if (obj->type == UCL_OBJECT) {
		const ucl_object_t *num = ucl_object_find_key(obj, "number");
		const ucl_object_t *name = ucl_object_find_key(obj, "name");

		if (num == NULL || num->type != UCL_INT) {
			log_warnx("lun section in target \"%s\" is missing "
			    "\"number\" integer property", target->t_name);
			return (false);
		}
		tmp = ucl_object_toint(num);
		if (tmp >= MAX_LUNS) {
			log_warnx("LU number %ju in target \"%s\" is too big",
			    tmp, target->t_name);
			return (false);
		}

		if (name == NULL || name->type != UCL_STRING) {
			log_warnx("lun section in target \"%s\" is missing "
			    "\"name\" string property", target->t_name);
			return (false);
		}

		lun = lun_find(conf, ucl_object_tostring(name));
		if (lun == NULL)
			return (false);

		target->t_luns[tmp] = lun;
	}

	return (true);
}

static bool
uclparse_toplevel(const ucl_object_t *top)
{
	ucl_object_iter_t it = NULL, iter = NULL;
	const ucl_object_t *obj = NULL, *child = NULL;

	/* Pass 1 - everything except targets */
	while ((obj = ucl_iterate_object(top, &it, true))) {
		const char *key = ucl_object_key(obj);

		if (!strcmp(key, "debug")) {
			if (obj->type == UCL_INT)
				conf->conf_debug = ucl_object_toint(obj);
			else {
				log_warnx("\"debug\" property value is not integer");
				return (false);
			}
		}

		if (!strcmp(key, "timeout")) {
			if (obj->type == UCL_INT)
				conf->conf_timeout = ucl_object_toint(obj);
			else {
				log_warnx("\"timeout\" property value is not integer");
				return (false);
			}
		}

		if (!strcmp(key, "maxproc")) {
			if (obj->type == UCL_INT)
				conf->conf_maxproc = ucl_object_toint(obj);
			else {
				log_warnx("\"maxproc\" property value is not integer");
				return (false);
			}
		}

		if (!strcmp(key, "pidfile")) {
			if (obj->type == UCL_STRING)
				conf->conf_pidfile_path = strdup(
				    ucl_object_tostring(obj));
			else {
				log_warnx("\"pidfile\" property value is not string");
				return (false);
			}
		}

		if (!strcmp(key, "isns-server")) {
			if (obj->type == UCL_ARRAY) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter,
				    true))) {
					if (child->type != UCL_STRING)
						return (false);

					if (!isns_new(conf,
					    ucl_object_tostring(child)))
						return (false);
				}
			} else {
				log_warnx("\"isns-server\" property value is "
				    "not an array");
				return (false);
			}
		}

		if (!strcmp(key, "isns-period")) {
			if (obj->type == UCL_INT)
				conf->conf_timeout = ucl_object_toint(obj);
			else {
				log_warnx("\"isns-period\" property value is not integer");
				return (false);
			}
		}

		if (!strcmp(key, "isns-timeout")) {
			if (obj->type == UCL_INT)
				conf->conf_timeout = ucl_object_toint(obj);
			else {
				log_warnx("\"isns-timeout\" property value is not integer");
				return (false);
			}
		}

		if (!strcmp(key, "auth-group")) {
			if (obj->type == UCL_OBJECT) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter, true))) {
					uclparse_auth_group(ucl_object_key(child), child);
				}
			} else {
				log_warnx("\"auth-group\" section is not an object");
				return (false);
			}
		}

		if (!strcmp(key, "portal-group")) {
			if (obj->type == UCL_OBJECT) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter, true))) {
					uclparse_portal_group(ucl_object_key(child), child);
				}
			} else {
				log_warnx("\"portal-group\" section is not an object");
				return (false);
			}
		}

		if (!strcmp(key, "lun")) {
			if (obj->type == UCL_OBJECT) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter, true))) {
					uclparse_lun(ucl_object_key(child), child);
				}
			} else {
				log_warnx("\"lun\" section is not an object");
				return (false);
			}
		}
	}

	/* Pass 2 - targets */
	it = NULL;
	while ((obj = ucl_iterate_object(top, &it, true))) {
		const char *key = ucl_object_key(obj);

		if (!strcmp(key, "target")) {
			if (obj->type == UCL_OBJECT) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter,
				    true))) {
					uclparse_target(ucl_object_key(child),
					    child);
				}
			} else {
				log_warnx("\"target\" section is not an object");
				return (false);
			}
		}
	}

	return (true);
}

static bool
uclparse_auth_group(const char *name, const ucl_object_t *top)
{
	struct auth_group *auth_group;
	const struct auth_name *an;
	const struct auth_portal *ap;
	ucl_object_iter_t it = NULL, it2 = NULL;
	const ucl_object_t *obj = NULL, *tmp = NULL;
	const char *key;

	if (!strcmp(name, "default") &&
	    conf->conf_default_ag_defined == false) {
		auth_group = auth_group_find(conf, name);
		conf->conf_default_ag_defined = true;
	} else {
		auth_group = auth_group_new(conf, name);
	}

	if (auth_group == NULL)
		return (false);

	while ((obj = ucl_iterate_object(top, &it, true))) {
		key = ucl_object_key(obj);

		if (!strcmp(key, "auth-type")) {
			const char *value = ucl_object_tostring(obj);

			if (!auth_group_set_type(auth_group, value))
				return (false);
		}

		if (!strcmp(key, "chap")) {
			if (obj->type != UCL_ARRAY) {
				log_warnx("\"chap\" property of "
				    "auth-group \"%s\" is not an array",
				    name);
				return (false);
			}

			it2 = NULL;
			while ((tmp = ucl_iterate_object(obj, &it2, true))) {
				if (!uclparse_chap(auth_group, tmp))
					return (false);
			}
		}

		if (!strcmp(key, "chap-mutual")) {
			if (obj->type != UCL_ARRAY) {
				log_warnx("\"chap-mutual\" property of "
				    "auth-group \"%s\" is not an array",
				    name);
				return (false);
			}

			it2 = NULL;
			while ((tmp = ucl_iterate_object(obj, &it2, true))) {
				if (!uclparse_chap_mutual(auth_group, tmp))
					return (false);
			}
		}

		if (!strcmp(key, "initiator-name")) {
			if (obj->type != UCL_ARRAY) {
				log_warnx("\"initiator-name\" property of "
				    "auth-group \"%s\" is not an array",
				    name);
				return (false);
			}

			it2 = NULL;
			while ((tmp = ucl_iterate_object(obj, &it2, true))) {
				const char *value = ucl_object_tostring(tmp);

				an = auth_name_new(auth_group, value);
				if (an == NULL)
					return (false);
			}
		}

		if (!strcmp(key, "initiator-portal")) {
			if (obj->type != UCL_ARRAY) {
				log_warnx("\"initiator-portal\" property of "
				    "auth-group \"%s\" is not an array",
				    name);
				return (false);
			}

			it2 = NULL;
			while ((tmp = ucl_iterate_object(obj, &it2, true))) {
				const char *value = ucl_object_tostring(tmp);

				ap = auth_portal_new(auth_group, value);
				if (ap == NULL)
					return (false);
			}
		}
	}

	return (true);
}

static bool
uclparse_dscp(const char *group_type, struct portal_group *portal_group,
    const ucl_object_t *obj)
{
	const char *key;

	if ((obj->type != UCL_STRING) && (obj->type != UCL_INT)) {
		log_warnx("\"dscp\" property of %s group \"%s\" is not a "
		    "string or integer", group_type, portal_group->pg_name);
		return (false);
	}
	if (obj->type == UCL_INT)
		portal_group->pg_dscp = ucl_object_toint(obj);
	else {
		key = ucl_object_tostring(obj);
		if (strcmp(key, "0x") == 0)
			portal_group->pg_dscp = strtol(key + 2, NULL, 16);
		else if (strcmp(key, "be") || strcmp(key, "cs0"))
			portal_group->pg_dscp = IPTOS_DSCP_CS0 >> 2;
		else if (strcmp(key, "ef"))
			portal_group->pg_dscp = IPTOS_DSCP_EF >> 2;
		else if (strcmp(key, "cs0"))
			portal_group->pg_dscp = IPTOS_DSCP_CS0 >> 2;
		else if (strcmp(key, "cs1"))
			portal_group->pg_dscp = IPTOS_DSCP_CS1 >> 2;
		else if (strcmp(key, "cs2"))
			portal_group->pg_dscp = IPTOS_DSCP_CS2 >> 2;
		else if (strcmp(key, "cs3"))
			portal_group->pg_dscp = IPTOS_DSCP_CS3 >> 2;
		else if (strcmp(key, "cs4"))
			portal_group->pg_dscp = IPTOS_DSCP_CS4 >> 2;
		else if (strcmp(key, "cs5"))
			portal_group->pg_dscp = IPTOS_DSCP_CS5 >> 2;
		else if (strcmp(key, "cs6"))
			portal_group->pg_dscp = IPTOS_DSCP_CS6 >> 2;
		else if (strcmp(key, "cs7"))
			portal_group->pg_dscp = IPTOS_DSCP_CS7 >> 2;
		else if (strcmp(key, "af11"))
			portal_group->pg_dscp = IPTOS_DSCP_AF11 >> 2;
		else if (strcmp(key, "af12"))
			portal_group->pg_dscp = IPTOS_DSCP_AF12 >> 2;
		else if (strcmp(key, "af13"))
			portal_group->pg_dscp = IPTOS_DSCP_AF13 >> 2;
		else if (strcmp(key, "af21"))
			portal_group->pg_dscp = IPTOS_DSCP_AF21 >> 2;
		else if (strcmp(key, "af22"))
			portal_group->pg_dscp = IPTOS_DSCP_AF22 >> 2;
		else if (strcmp(key, "af23"))
			portal_group->pg_dscp = IPTOS_DSCP_AF23 >> 2;
		else if (strcmp(key, "af31"))
			portal_group->pg_dscp = IPTOS_DSCP_AF31 >> 2;
		else if (strcmp(key, "af32"))
			portal_group->pg_dscp = IPTOS_DSCP_AF32 >> 2;
		else if (strcmp(key, "af33"))
			portal_group->pg_dscp = IPTOS_DSCP_AF33 >> 2;
		else if (strcmp(key, "af41"))
			portal_group->pg_dscp = IPTOS_DSCP_AF41 >> 2;
		else if (strcmp(key, "af42"))
			portal_group->pg_dscp = IPTOS_DSCP_AF42 >> 2;
		else if (strcmp(key, "af43"))
			portal_group->pg_dscp = IPTOS_DSCP_AF43 >> 2;
		else {
			log_warnx("\"dscp\" property value is not a supported textual value");
			return (false);
		}
	}
	return (true);
}

static bool
uclparse_pcp(const char *group_type, struct portal_group *portal_group,
    const ucl_object_t *obj)
{
	if (obj->type != UCL_INT) {
		log_warnx("\"pcp\" property of %s group \"%s\" is not an "
		    "integer", group_type, portal_group->pg_name);
		return (false);
	}
	portal_group->pg_pcp = ucl_object_toint(obj);
	if (!((portal_group->pg_pcp >= 0) && (portal_group->pg_pcp <= 7))) {
		log_warnx("invalid \"pcp\" value %d, using default",
		    portal_group->pg_pcp);
		portal_group->pg_pcp = -1;
	}
	return (true);
}

static bool
uclparse_portal_group(const char *name, const ucl_object_t *top)
{
	struct portal_group *portal_group;
	ucl_object_iter_t it = NULL, it2 = NULL;
	const ucl_object_t *obj = NULL, *tmp = NULL;
	const char *key;

	if (strcmp(name, "default") == 0 &&
	    conf->conf_default_pg_defined == false) {
		portal_group = portal_group_find(conf, name);
		conf->conf_default_pg_defined = true;
	} else {
		portal_group = portal_group_new(conf, name);
	}

	if (portal_group == NULL)
		return (false);

	while ((obj = ucl_iterate_object(top, &it, true))) {
		key = ucl_object_key(obj);

		if (!strcmp(key, "discovery-auth-group")) {
			portal_group->pg_discovery_auth_group =
			    auth_group_find(conf, ucl_object_tostring(obj));
			if (portal_group->pg_discovery_auth_group == NULL) {
				log_warnx("unknown discovery-auth-group \"%s\" "
				    "for portal-group \"%s\"",
				    ucl_object_tostring(obj),
				    portal_group->pg_name);
				return (false);
			}
		}

		if (!strcmp(key, "discovery-filter")) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"discovery-filter\" property of "
				    "portal-group \"%s\" is not a string",
				    portal_group->pg_name);
				return (false);
			}

			if (!portal_group_set_filter(portal_group,
			    ucl_object_tostring(obj)))
				return (false);
		}

		if (!strcmp(key, "listen")) {
			if (obj->type == UCL_STRING) {
				if (!portal_group_add_listen(portal_group,
				    ucl_object_tostring(obj), false))
					return (false);
			} else if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!portal_group_add_listen(
					    portal_group,
					    ucl_object_tostring(tmp),
					    false))
						return (false);
				}
			} else {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    portal_group->pg_name);
				return (false);
			}
		}

		if (!strcmp(key, "listen-iser")) {
			if (obj->type == UCL_STRING) {
				if (!portal_group_add_listen(portal_group,
				    ucl_object_tostring(obj), true))
					return (false);
			} else if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!portal_group_add_listen(
					    portal_group,
					    ucl_object_tostring(tmp),
					    true))
						return (false);
				}
			} else {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    portal_group->pg_name);
				return (false);
			}
		}

		if (!strcmp(key, "redirect")) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    portal_group->pg_name);
				return (false);
			}

			if (!portal_group_set_redirection(portal_group,
			    ucl_object_tostring(obj)))
				return (false);
		}

		if (!strcmp(key, "options")) {
			if (obj->type != UCL_OBJECT) {
				log_warnx("\"options\" property of portal group "
				    "\"%s\" is not an object", portal_group->pg_name);
				return (false);
			}

			while ((tmp = ucl_iterate_object(obj, &it2,
			    true))) {
				if (!option_new(portal_group->pg_options,
				    ucl_object_key(tmp),
				    ucl_object_tostring_forced(tmp)))
					return (false);
			}
		}

		if (!strcmp(key, "dscp")) {
			if (!uclparse_dscp("portal", portal_group, obj))
				return (false);
		}

		if (!strcmp(key, "pcp")) {
			if (!uclparse_pcp("portal", portal_group, obj))
				return (false);
		}
	}

	return (true);
}

static bool
uclparse_target(const char *name, const ucl_object_t *top)
{
	struct target *target;
	ucl_object_iter_t it = NULL, it2 = NULL;
	const ucl_object_t *obj = NULL, *tmp = NULL;
	const char *key;

	target = target_new(conf, name);
	if (target == NULL)
		return (false);

	while ((obj = ucl_iterate_object(top, &it, true))) {
		key = ucl_object_key(obj);

		if (!strcmp(key, "alias")) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"alias\" property of target "
				    "\"%s\" is not a string", target->t_name);
				return (false);
			}

			target->t_alias = strdup(ucl_object_tostring(obj));
		}

		if (!strcmp(key, "auth-group")) {
			const char *ag;

			if (target->t_auth_group != NULL) {
				if (target->t_auth_group->ag_name != NULL)
					log_warnx("auth-group for target \"%s\" "
					    "specified more than once",
					    target->t_name);
				else
					log_warnx("cannot use both auth-group "
					    "and explicit authorisations for "
					    "target \"%s\"", target->t_name);
				return (false);
			}
			ag = ucl_object_tostring(obj);
			if (!ag) {
				log_warnx("auth-group must be a string");
				return (false);
			}
			target->t_auth_group = auth_group_find(conf, ag);
			if (target->t_auth_group == NULL) {
				log_warnx("unknown auth-group \"%s\" for target "
				    "\"%s\"", ucl_object_tostring(obj),
				    target->t_name);
				return (false);
			}
		}

		if (!strcmp(key, "auth-type")) {
			if (target->t_auth_group != NULL) {
				if (target->t_auth_group->ag_name != NULL) {
					log_warnx("cannot use both auth-group and "
					    "auth-type for target \"%s\"",
					    target->t_name);
					return (false);
				}
			} else {
				target->t_auth_group = auth_group_new(conf, NULL);
				if (target->t_auth_group == NULL)
					return (false);

				target->t_auth_group->ag_target = target;
			}
			if (!auth_group_set_type(target->t_auth_group,
			    ucl_object_tostring(obj)))
				return (false);
		}

		if (!strcmp(key, "chap")) {
			if (target->t_auth_group != NULL) {
				if (target->t_auth_group->ag_name != NULL) {
					log_warnx("cannot use both auth-group "
					    "and chap for target \"%s\"",
					    target->t_name);
					return (false);
				}
			} else {
				target->t_auth_group = auth_group_new(conf, NULL);
				if (target->t_auth_group == NULL) {
					return (false);
				}
				target->t_auth_group->ag_target = target;
			}
			if (!uclparse_chap(target->t_auth_group, obj))
				return (false);
		}

		if (!strcmp(key, "chap-mutual")) {
			if (!uclparse_chap_mutual(target->t_auth_group, obj))
				return (false);
		}

		if (!strcmp(key, "initiator-name")) {
			const struct auth_name *an;

			if (target->t_auth_group != NULL) {
				if (target->t_auth_group->ag_name != NULL) {
					log_warnx("cannot use both auth-group and "
					    "initiator-name for target \"%s\"",
					    target->t_name);
					return (false);
				}
			} else {
				target->t_auth_group = auth_group_new(conf, NULL);
				if (target->t_auth_group == NULL)
					return (false);

				target->t_auth_group->ag_target = target;
			}
			an = auth_name_new(target->t_auth_group,
			    ucl_object_tostring(obj));
			if (an == NULL)
				return (false);
		}

		if (!strcmp(key, "initiator-portal")) {
			const struct auth_portal *ap;

			if (target->t_auth_group != NULL) {
				if (target->t_auth_group->ag_name != NULL) {
					log_warnx("cannot use both auth-group and "
					    "initiator-portal for target \"%s\"",
					    target->t_name);
					return (false);
				}
			} else {
				target->t_auth_group = auth_group_new(conf, NULL);
				if (target->t_auth_group == NULL)
					return (false);

				target->t_auth_group->ag_target = target;
			}
			ap = auth_portal_new(target->t_auth_group,
			    ucl_object_tostring(obj));
			if (ap == NULL)
				return (false);
		}

		if (!strcmp(key, "portal-group")) {
			if (obj->type == UCL_OBJECT) {
				if (!uclparse_target_portal_group(target, obj))
					return (false);
			}

			if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!uclparse_target_portal_group(target,
					    tmp))
						return (false);
				}
			}
		}

		if (!strcmp(key, "port")) {
			const char *value;

			value = ucl_object_tostring(obj);
			target->t_pport = strdup(value);
		}

		if (!strcmp(key, "redirect")) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"redirect\" property of target "
				    "\"%s\" is not a string", target->t_name);
				return (false);
			}

			if (!target_set_redirection(target,
			    ucl_object_tostring(obj)))
				return (false);
		}

		if (!strcmp(key, "lun")) {
			while ((tmp = ucl_iterate_object(obj, &it2, true))) {
				if (!uclparse_target_lun(target, tmp))
					return (false);
			}
		}
	}

	return (true);
}

static bool
uclparse_lun(const char *name, const ucl_object_t *top)
{
	struct lun *lun;
	ucl_object_iter_t it = NULL, child_it = NULL;
	const ucl_object_t *obj = NULL, *child = NULL;
	const char *key;

	lun = lun_new(conf, name);
	if (lun == NULL)
		return (false);

	while ((obj = ucl_iterate_object(top, &it, true))) {
		key = ucl_object_key(obj);

		if (!strcmp(key, "backend")) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"backend\" property of lun "
				    "\"%s\" is not a string",
				    lun->l_name);
				return (false);
			}

			lun_set_backend(lun, ucl_object_tostring(obj));
		}

		if (!strcmp(key, "blocksize")) {
			if (obj->type != UCL_INT) {
				log_warnx("\"blocksize\" property of lun "
				    "\"%s\" is not an integer", lun->l_name);
				return (false);
			}

			lun_set_blocksize(lun, ucl_object_toint(obj));
		}

		if (!strcmp(key, "device-id")) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"device-id\" property of lun "
				    "\"%s\" is not an integer", lun->l_name);
				return (false);
			}

			lun_set_device_id(lun, ucl_object_tostring(obj));
		}

		if (!strcmp(key, "options")) {
			if (obj->type != UCL_OBJECT) {
				log_warnx("\"options\" property of lun "
				    "\"%s\" is not an object", lun->l_name);
				return (false);
			}

			while ((child = ucl_iterate_object(obj, &child_it,
			    true))) {
				if (!option_new(lun->l_options,
				    ucl_object_key(child),
				    ucl_object_tostring_forced(child)))
					return (false);
			}
		}

		if (!strcmp(key, "path")) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"path\" property of lun "
				    "\"%s\" is not a string", lun->l_name);
				return (false);
			}

			lun_set_path(lun, ucl_object_tostring(obj));
		}

		if (!strcmp(key, "serial")) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"serial\" property of lun "
				    "\"%s\" is not a string", lun->l_name);
				return (false);
			}

			lun_set_serial(lun, ucl_object_tostring(obj));
		}

		if (!strcmp(key, "size")) {
			if (obj->type != UCL_INT) {
				log_warnx("\"size\" property of lun "
				    "\"%s\" is not an integer", lun->l_name);
				return (false);
			}

			lun_set_size(lun, ucl_object_toint(obj));
		}
	}

	return (true);
}

bool
uclparse_conf(struct conf *newconf, const char *path)
{
	struct ucl_parser *parser;
	ucl_object_t *top;
	bool parsed;

	conf = newconf;
	parser = ucl_parser_new(0);

	if (!ucl_parser_add_file(parser, path)) {
		log_warn("unable to parse configuration file %s: %s", path,
		    ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		return (false);
	}

	top = ucl_parser_get_object(parser);
	parsed = uclparse_toplevel(top);
	ucl_object_unref(top);
	ucl_parser_free(parser);

	return (parsed);
}
