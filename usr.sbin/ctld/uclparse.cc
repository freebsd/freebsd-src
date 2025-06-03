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

#include "conf.h"
#include "ctld.h"

static bool uclparse_toplevel(const ucl_object_t *);
static bool uclparse_chap(const char *, const ucl_object_t *);
static bool uclparse_chap_mutual(const char *, const ucl_object_t *);
static bool uclparse_lun(const char *, const ucl_object_t *);
static bool uclparse_lun_entries(const char *, const ucl_object_t *);
static bool uclparse_auth_group(const char *, const ucl_object_t *);
static bool uclparse_portal_group(const char *, const ucl_object_t *);
static bool uclparse_target(const char *, const ucl_object_t *);
static bool uclparse_target_portal_group(const char *, const ucl_object_t *);
static bool uclparse_target_lun(const char *, const ucl_object_t *);

static bool
uclparse_chap(const char *ag_name, const ucl_object_t *obj)
{
	const ucl_object_t *user, *secret;

	user = ucl_object_find_key(obj, "user");
	if (!user || user->type != UCL_STRING) {
		log_warnx("chap section in auth-group \"%s\" is missing "
		    "\"user\" string key", ag_name);
		return (false);
	}

	secret = ucl_object_find_key(obj, "secret");
	if (!secret || secret->type != UCL_STRING) {
		log_warnx("chap section in auth-group \"%s\" is missing "
		    "\"secret\" string key", ag_name);
		return (false);
	}

	return (auth_group_add_chap(
	    ucl_object_tostring(user),
	    ucl_object_tostring(secret)));
}

static bool
uclparse_chap_mutual(const char *ag_name, const ucl_object_t *obj)
{
	const ucl_object_t *user, *secret, *mutual_user;
	const ucl_object_t *mutual_secret;

	user = ucl_object_find_key(obj, "user");
	if (!user || user->type != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"user\" string key", ag_name);
		return (false);
	}

	secret = ucl_object_find_key(obj, "secret");
	if (!secret || secret->type != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"secret\" string key", ag_name);
		return (false);
	}

	mutual_user = ucl_object_find_key(obj, "mutual-user");
	if (!mutual_user || mutual_user->type != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"mutual-user\" string key", ag_name);
		return (false);
	}

	mutual_secret = ucl_object_find_key(obj, "mutual-secret");
	if (!mutual_secret || mutual_secret->type != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"mutual-secret\" string key", ag_name);
		return (false);
	}

	return (auth_group_add_chap_mutual(
	    ucl_object_tostring(user),
	    ucl_object_tostring(secret),
	    ucl_object_tostring(mutual_user),
	    ucl_object_tostring(mutual_secret)));
}

static bool
uclparse_target_chap(const char *t_name, const ucl_object_t *obj)
{
	const ucl_object_t *user, *secret;

	user = ucl_object_find_key(obj, "user");
	if (!user || user->type != UCL_STRING) {
		log_warnx("chap section in target \"%s\" is missing "
		    "\"user\" string key", t_name);
		return (false);
	}

	secret = ucl_object_find_key(obj, "secret");
	if (!secret || secret->type != UCL_STRING) {
		log_warnx("chap section in target \"%s\" is missing "
		    "\"secret\" string key", t_name);
		return (false);
	}

	return (target_add_chap(
	    ucl_object_tostring(user),
	    ucl_object_tostring(secret)));
}

static bool
uclparse_target_chap_mutual(const char *t_name, const ucl_object_t *obj)
{
	const ucl_object_t *user, *secret, *mutual_user;
	const ucl_object_t *mutual_secret;

	user = ucl_object_find_key(obj, "user");
	if (!user || user->type != UCL_STRING) {
		log_warnx("chap-mutual section in target \"%s\" is missing "
		    "\"user\" string key", t_name);
		return (false);
	}

	secret = ucl_object_find_key(obj, "secret");
	if (!secret || secret->type != UCL_STRING) {
		log_warnx("chap-mutual section in target \"%s\" is missing "
		    "\"secret\" string key", t_name);
		return (false);
	}

	mutual_user = ucl_object_find_key(obj, "mutual-user");
	if (!mutual_user || mutual_user->type != UCL_STRING) {
		log_warnx("chap-mutual section in target \"%s\" is missing "
		    "\"mutual-user\" string key", t_name);
		return (false);
	}

	mutual_secret = ucl_object_find_key(obj, "mutual-secret");
	if (!mutual_secret || mutual_secret->type != UCL_STRING) {
		log_warnx("chap-mutual section in target \"%s\" is missing "
		    "\"mutual-secret\" string key", t_name);
		return (false);
	}

	return (target_add_chap_mutual(
	    ucl_object_tostring(user),
	    ucl_object_tostring(secret),
	    ucl_object_tostring(mutual_user),
	    ucl_object_tostring(mutual_secret)));
}

static bool
uclparse_target_portal_group(const char *t_name, const ucl_object_t *obj)
{
	const ucl_object_t *portal_group, *auth_group;
	const char *ag_name;

	/*
	 * If the value is a single string, assume it is a
	 * portal-group name.
	 */
	if (obj->type == UCL_STRING)
		return (target_add_portal_group(ucl_object_tostring(obj),
		    NULL));

	if (obj->type != UCL_OBJECT) {
		log_warnx("portal-group section in target \"%s\" must be "
		    "an object or string", t_name);
		return (false);
	}

	portal_group = ucl_object_find_key(obj, "name");
	if (!portal_group || portal_group->type != UCL_STRING) {
		log_warnx("portal-group section in target \"%s\" is missing "
		    "\"name\" string key", t_name);
		return (false);
	}

	auth_group = ucl_object_find_key(obj, "auth-group-name");
	if (auth_group != NULL) {
		if (auth_group->type != UCL_STRING) {
			log_warnx("\"auth-group-name\" property in "
			    "portal-group section for target \"%s\" is not "
			    "a string", t_name);
			return (false);
		}
		ag_name = ucl_object_tostring(auth_group);
	} else
		ag_name = NULL;

	return (target_add_portal_group(ucl_object_tostring(portal_group),
	    ag_name));
}

static bool
uclparse_target_lun(const char *t_name, const ucl_object_t *obj)
{
	const ucl_object_t *num;
	const ucl_object_t *name;
	const char *key;
	char *end, *lun_name;
	u_int id;
	bool ok;

	key = ucl_object_key(obj);
	if (key != NULL) {
		id = strtoul(key, &end, 0);
		if (*end != '\0') {
			log_warnx("lun key \"%s\" in target \"%s\" is invalid",
			    key, t_name);
			return (false);
		}

		if (obj->type == UCL_STRING)
			return (target_add_lun(id, ucl_object_tostring(obj)));
	}

	if (obj->type != UCL_OBJECT) {
		log_warnx("lun section entries in target \"%s\" must be objects",
		    t_name);
		return (false);
	}

	if (key == NULL) {
		num = ucl_object_find_key(obj, "number");
		if (num == NULL || num->type != UCL_INT) {
			log_warnx("lun section in target \"%s\" is missing "
			    "\"number\" integer property", t_name);
			return (false);
		}
		id = ucl_object_toint(num);
	}

	name = ucl_object_find_key(obj, "name");
	if (name == NULL) {
		if (!target_start_lun(id))
			return (false);

		asprintf(&lun_name, "lun %u for target \"%s\"", id, t_name);
		ok = uclparse_lun_entries(lun_name, obj);
		free(lun_name);
		return (ok);
	}

	if (name->type != UCL_STRING) {
		log_warnx("\"name\" property for lun %u for target "
		    "\"%s\" is not a string", id, t_name);
		return (false);
	}

	return (target_add_lun(id, ucl_object_tostring(name)));
}

static bool
uclparse_toplevel(const ucl_object_t *top)
{
	ucl_object_iter_t it = NULL, iter = NULL;
	const ucl_object_t *obj = NULL, *child = NULL;

	/* Pass 1 - everything except targets */
	while ((obj = ucl_iterate_object(top, &it, true))) {
		const char *key = ucl_object_key(obj);

		if (strcmp(key, "debug") == 0) {
			if (obj->type == UCL_INT)
				conf_set_debug(ucl_object_toint(obj));
			else {
				log_warnx("\"debug\" property value is not integer");
				return (false);
			}
		}

		if (strcmp(key, "timeout") == 0) {
			if (obj->type == UCL_INT)
				conf_set_timeout(ucl_object_toint(obj));
			else {
				log_warnx("\"timeout\" property value is not integer");
				return (false);
			}
		}

		if (strcmp(key, "maxproc") == 0) {
			if (obj->type == UCL_INT)
				conf_set_maxproc(ucl_object_toint(obj));
			else {
				log_warnx("\"maxproc\" property value is not integer");
				return (false);
			}
		}

		if (strcmp(key, "pidfile") == 0) {
			if (obj->type == UCL_STRING) {
				if (!conf_set_pidfile_path(
				    ucl_object_tostring(obj)))
					return (false);
			} else {
				log_warnx("\"pidfile\" property value is not string");
				return (false);
			}
		}

		if (strcmp(key, "isns-server") == 0) {
			if (obj->type == UCL_ARRAY) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter,
				    true))) {
					if (child->type != UCL_STRING)
						return (false);

					if (!isns_add_server(
					    ucl_object_tostring(child)))
						return (false);
				}
			} else {
				log_warnx("\"isns-server\" property value is "
				    "not an array");
				return (false);
			}
		}

		if (strcmp(key, "isns-period") == 0) {
			if (obj->type == UCL_INT)
				conf_set_isns_period(ucl_object_toint(obj));
			else {
				log_warnx("\"isns-period\" property value is not integer");
				return (false);
			}
		}

		if (strcmp(key, "isns-timeout") == 0) {
			if (obj->type == UCL_INT)
				conf_set_isns_timeout(ucl_object_toint(obj));
			else {
				log_warnx("\"isns-timeout\" property value is not integer");
				return (false);
			}
		}

		if (strcmp(key, "auth-group") == 0) {
			if (obj->type == UCL_OBJECT) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter, true))) {
					if (!uclparse_auth_group(
					    ucl_object_key(child), child))
						return (false);
				}
			} else {
				log_warnx("\"auth-group\" section is not an object");
				return (false);
			}
		}

		if (strcmp(key, "portal-group") == 0) {
			if (obj->type == UCL_OBJECT) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter, true))) {
					if (!uclparse_portal_group(
					    ucl_object_key(child), child))
						return (false);
				}
			} else {
				log_warnx("\"portal-group\" section is not an object");
				return (false);
			}
		}

		if (strcmp(key, "lun") == 0) {
			if (obj->type == UCL_OBJECT) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter, true))) {
					if (!uclparse_lun(ucl_object_key(child),
					    child))
						return (false);
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

		if (strcmp(key, "target") == 0) {
			if (obj->type == UCL_OBJECT) {
				iter = NULL;
				while ((child = ucl_iterate_object(obj, &iter,
				    true))) {
					if (!uclparse_target(
					    ucl_object_key(child), child))
						return (false);
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
	ucl_object_iter_t it = NULL, it2 = NULL;
	const ucl_object_t *obj = NULL, *tmp = NULL;
	const char *key;

	if (!auth_group_start(name))
		return (false);

	while ((obj = ucl_iterate_object(top, &it, true))) {
		key = ucl_object_key(obj);

		if (strcmp(key, "auth-type") == 0) {
			const char *value = ucl_object_tostring(obj);

			if (!auth_group_set_type(value))
				goto fail;
		}

		if (strcmp(key, "chap") == 0) {
			if (obj->type == UCL_OBJECT) {
				if (!uclparse_chap(name, obj))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				it2 = NULL;
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!uclparse_chap(name, tmp))
						goto fail;
				}
			} else {
				log_warnx("\"chap\" property of auth-group "
				    "\"%s\" is not an array or object",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "chap-mutual") == 0) {
			if (obj->type == UCL_OBJECT) {
				if (!uclparse_chap_mutual(name, obj))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				it2 = NULL;
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!uclparse_chap_mutual(name, tmp))
						goto fail;
				}
			} else {
				log_warnx("\"chap-mutual\" property of "
				    "auth-group \"%s\" is not an array or object",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "initiator-name") == 0) {
			if (obj->type == UCL_STRING) {
				const char *value = ucl_object_tostring(obj);

				if (!auth_group_add_initiator_name(value))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				it2 = NULL;
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					const char *value =
					    ucl_object_tostring(tmp);

					if (!auth_group_add_initiator_name(
					    value))
						goto fail;
				}
			} else {
				log_warnx("\"initiator-name\" property of "
				    "auth-group \"%s\" is not an array or string",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "initiator-portal") == 0) {
			if (obj->type == UCL_STRING) {
				const char *value = ucl_object_tostring(obj);

				if (!auth_group_add_initiator_portal(value))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				it2 = NULL;
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					const char *value =
					    ucl_object_tostring(tmp);

					if (!auth_group_add_initiator_portal(
					    value))
						goto fail;
				}
			} else {
				log_warnx("\"initiator-portal\" property of "
				    "auth-group \"%s\" is not an array or string",
				    name);
				goto fail;
			}
		}
	}

	auth_group_finish();
	return (true);
fail:
	auth_group_finish();
	return (false);
}

static bool
uclparse_dscp(const char *group_type, const char *pg_name,
    const ucl_object_t *obj)
{
	const char *key;

	if ((obj->type != UCL_STRING) && (obj->type != UCL_INT)) {
		log_warnx("\"dscp\" property of %s group \"%s\" is not a "
		    "string or integer", group_type, pg_name);
		return (false);
	}
	if (obj->type == UCL_INT)
		return (portal_group_set_dscp(ucl_object_toint(obj)));

	key = ucl_object_tostring(obj);
	if (strcmp(key, "be") == 0 || strcmp(key, "cs0") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS0 >> 2);
	else if (strcmp(key, "ef") == 0)
		portal_group_set_dscp(IPTOS_DSCP_EF >> 2);
	else if (strcmp(key, "cs0") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS0 >> 2);
	else if (strcmp(key, "cs1") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS1 >> 2);
	else if (strcmp(key, "cs2") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS2 >> 2);
	else if (strcmp(key, "cs3") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS3 >> 2);
	else if (strcmp(key, "cs4") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS4 >> 2);
	else if (strcmp(key, "cs5") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS5 >> 2);
	else if (strcmp(key, "cs6") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS6 >> 2);
	else if (strcmp(key, "cs7") == 0)
		portal_group_set_dscp(IPTOS_DSCP_CS7 >> 2);
	else if (strcmp(key, "af11") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF11 >> 2);
	else if (strcmp(key, "af12") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF12 >> 2);
	else if (strcmp(key, "af13") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF13 >> 2);
	else if (strcmp(key, "af21") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF21 >> 2);
	else if (strcmp(key, "af22") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF22 >> 2);
	else if (strcmp(key, "af23") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF23 >> 2);
	else if (strcmp(key, "af31") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF31 >> 2);
	else if (strcmp(key, "af32") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF32 >> 2);
	else if (strcmp(key, "af33") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF33 >> 2);
	else if (strcmp(key, "af41") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF41 >> 2);
	else if (strcmp(key, "af42") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF42 >> 2);
	else if (strcmp(key, "af43") == 0)
		portal_group_set_dscp(IPTOS_DSCP_AF43 >> 2);
	else {
		log_warnx("\"dscp\" property value is not a supported textual value");
		return (false);
	}
	return (true);
}

static bool
uclparse_pcp(const char *group_type, const char *pg_name,
    const ucl_object_t *obj)
{
	if (obj->type != UCL_INT) {
		log_warnx("\"pcp\" property of %s group \"%s\" is not an "
		    "integer", group_type, pg_name);
		return (false);
	}
	return (portal_group_set_pcp(ucl_object_toint(obj)));
}

static bool
uclparse_portal_group(const char *name, const ucl_object_t *top)
{
	ucl_object_iter_t it = NULL, it2 = NULL;
	const ucl_object_t *obj = NULL, *tmp = NULL;
	const char *key;

	if (!portal_group_start(name))
		return (false);

	while ((obj = ucl_iterate_object(top, &it, true))) {
		key = ucl_object_key(obj);

		if (strcmp(key, "discovery-auth-group") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"discovery-auth-group\" property "
				    "of portal-group \"%s\" is not a string",
				    name);
				goto fail;
			}

			if (!portal_group_set_discovery_auth_group(
			    ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "discovery-filter") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"discovery-filter\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				goto fail;
			}

			if (!portal_group_set_filter(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "foreign") == 0) {
			portal_group_set_foreign();
		}

		if (strcmp(key, "listen") == 0) {
			if (obj->type == UCL_STRING) {
				if (!portal_group_add_listen(
				    ucl_object_tostring(obj), false))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!portal_group_add_listen(
					    ucl_object_tostring(tmp),
					    false))
						goto fail;
				}
			} else {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "listen-iser") == 0) {
			if (obj->type == UCL_STRING) {
				if (!portal_group_add_listen(
				    ucl_object_tostring(obj), true))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!portal_group_add_listen(
					    ucl_object_tostring(tmp),
					    true))
						goto fail;
				}
			} else {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "offload") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"offload\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				goto fail;
			}

			if (!portal_group_set_offload(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "redirect") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				goto fail;
			}

			if (!portal_group_set_redirection(
			    ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "options") == 0) {
			if (obj->type != UCL_OBJECT) {
				log_warnx("\"options\" property of portal group "
				    "\"%s\" is not an object", name);
				goto fail;
			}

			while ((tmp = ucl_iterate_object(obj, &it2,
			    true))) {
				if (!portal_group_add_option(
				    ucl_object_key(tmp),
				    ucl_object_tostring_forced(tmp)))
					goto fail;
			}
		}

		if (strcmp(key, "tag") == 0) {
			if (obj->type != UCL_INT) {
				log_warnx("\"tag\" property of portal group "
				    "\"%s\" is not an integer",
				    name);
				goto fail;
			}

			portal_group_set_tag(ucl_object_toint(obj));
		}

		if (strcmp(key, "dscp") == 0) {
			if (!uclparse_dscp("portal", name, obj))
				goto fail;
		}

		if (strcmp(key, "pcp") == 0) {
			if (!uclparse_pcp("portal", name, obj))
				goto fail;
		}
	}

	portal_group_finish();
	return (true);
fail:
	portal_group_finish();
	return (false);
}

static bool
uclparse_target(const char *name, const ucl_object_t *top)
{
	ucl_object_iter_t it = NULL, it2 = NULL;
	const ucl_object_t *obj = NULL, *tmp = NULL;
	const char *key;

	if (!target_start(name))
		return (false);

	while ((obj = ucl_iterate_object(top, &it, true))) {
		key = ucl_object_key(obj);

		if (strcmp(key, "alias") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"alias\" property of target "
				    "\"%s\" is not a string", name);
				goto fail;
			}

			if (!target_set_alias(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "auth-group") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"auth-group\" property of target "
				    "\"%s\" is not a string", name);
				goto fail;
			}

			if (!target_set_auth_group(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "auth-type") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"auth-type\" property of target "
				    "\"%s\" is not a string", name);
				goto fail;
			}

			if (!target_set_auth_type(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "chap") == 0) {
			if (obj->type == UCL_OBJECT) {
				if (!uclparse_target_chap(name, obj))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!uclparse_target_chap(name, tmp))
						goto fail;
				}
			} else {
				log_warnx("\"chap\" property of target "
				    "\"%s\" is not an array or object",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "chap-mutual") == 0) {
			if (obj->type == UCL_OBJECT) {
				if (!uclparse_target_chap_mutual(name, obj))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!uclparse_target_chap_mutual(name,
					    tmp))
						goto fail;
				}
			} else {
				log_warnx("\"chap-mutual\" property of target "
				    "\"%s\" is not an array or object",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "initiator-name") == 0) {
			if (obj->type == UCL_STRING) {
				if (!target_add_initiator_name(
				    ucl_object_tostring(obj)))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!target_add_initiator_name(
					    ucl_object_tostring(tmp)))
						goto fail;
				}
			} else {
				log_warnx("\"initiator-name\" property of "
				    "target \"%s\" is not an array or string",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "initiator-portal") == 0) {
			if (obj->type == UCL_STRING) {
				if (!target_add_initiator_portal(
				    ucl_object_tostring(obj)))
					goto fail;
			} else if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!target_add_initiator_portal(
					    ucl_object_tostring(tmp)))
						goto fail;
				}
			} else {
				log_warnx("\"initiator-portal\" property of "
				    "target \"%s\" is not an array or string",
				    name);
				goto fail;
			}
		}

		if (strcmp(key, "portal-group") == 0) {
			if (obj->type == UCL_ARRAY) {
				while ((tmp = ucl_iterate_object(obj, &it2,
				    true))) {
					if (!uclparse_target_portal_group(name,
					    tmp))
						goto fail;
				}
			} else {
				if (!uclparse_target_portal_group(name, obj))
					goto fail;
			}
		}

		if (strcmp(key, "port") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"port\" property of target "
				    "\"%s\" is not a string", name);
				goto fail;
			}

			if (!target_set_physical_port(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "redirect") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"redirect\" property of target "
				    "\"%s\" is not a string", name);
				goto fail;
			}

			if (!target_set_redirection(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "lun") == 0) {
			while ((tmp = ucl_iterate_object(obj, &it2, true))) {
				if (!uclparse_target_lun(name, tmp))
					goto fail;
			}
		}
	}

	target_finish();
	return (true);
fail:
	target_finish();
	return (false);
}

static bool
uclparse_lun(const char *name, const ucl_object_t *top)
{
	char *lun_name;
	bool ok;

	if (!lun_start(name))
		return (false);
	asprintf(&lun_name, "lun \"%s\"", name);
	ok = uclparse_lun_entries(lun_name, top);
	free(lun_name);
	return (ok);
}

static bool
uclparse_lun_entries(const char *name, const ucl_object_t *top)
{
	ucl_object_iter_t it = NULL, child_it = NULL;
	const ucl_object_t *obj = NULL, *child = NULL;
	const char *key;

	while ((obj = ucl_iterate_object(top, &it, true))) {
		key = ucl_object_key(obj);

		if (strcmp(key, "backend") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"backend\" property of %s "
				    "is not a string", name);
				goto fail;
			}

			if (!lun_set_backend(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "blocksize") == 0) {
			if (obj->type != UCL_INT) {
				log_warnx("\"blocksize\" property of %s "
				    "is not an integer", name);
				goto fail;
			}

			if (!lun_set_blocksize(ucl_object_toint(obj)))
				goto fail;
		}

		if (strcmp(key, "device-id") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"device-id\" property of %s "
				    "is not an integer", name);
				goto fail;
			}

			if (!lun_set_device_id(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "device-type") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"device-type\" property of %s "
				    "is not an integer", name);
				goto fail;
			}

			if (!lun_set_device_type(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "ctl-lun") == 0) {
			if (obj->type != UCL_INT) {
				log_warnx("\"ctl-lun\" property of %s "
				    "is not an integer", name);
				goto fail;
			}

			if (!lun_set_ctl_lun(ucl_object_toint(obj)))
				goto fail;
		}

		if (strcmp(key, "options") == 0) {
			if (obj->type != UCL_OBJECT) {
				log_warnx("\"options\" property of %s "
				    "is not an object", name);
				goto fail;
			}

			while ((child = ucl_iterate_object(obj, &child_it,
			    true))) {
				if (!lun_add_option(ucl_object_key(child),
				    ucl_object_tostring_forced(child)))
					goto fail;
			}
		}

		if (strcmp(key, "path") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"path\" property of %s "
				    "is not a string", name);
				goto fail;
			}

			if (!lun_set_path(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "serial") == 0) {
			if (obj->type != UCL_STRING) {
				log_warnx("\"serial\" property of %s "
				    "is not a string", name);
				goto fail;
			}

			if (!lun_set_serial(ucl_object_tostring(obj)))
				goto fail;
		}

		if (strcmp(key, "size") == 0) {
			if (obj->type != UCL_INT) {
				log_warnx("\"size\" property of %s "
				    "is not an integer", name);
				goto fail;
			}

			if (!lun_set_size(ucl_object_toint(obj)))
				goto fail;
		}
	}

	lun_finish();
	return (true);
fail:
	lun_finish();
	return (false);
}

bool
uclparse_conf(const char *path)
{
	struct ucl_parser *parser;
	ucl_object_t *top;
	bool parsed;

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
