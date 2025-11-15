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
#include <ucl++.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <libutil++.hh>
#include <memory>

#include "conf.h"
#include "ctld.hh"

struct scope_exit {
	using callback = void();
	scope_exit(callback *fn) : fn(fn) {}

	~scope_exit() { fn(); }

private:
	callback *fn;
};

static bool uclparse_toplevel(const ucl::Ucl &);
static bool uclparse_chap(const char *, const ucl::Ucl &);
static bool uclparse_chap_mutual(const char *, const ucl::Ucl &);
static bool uclparse_lun(const char *, const ucl::Ucl &);
static bool uclparse_lun_entries(const char *, const ucl::Ucl &);
static bool uclparse_auth_group(const char *, const ucl::Ucl &);
static bool uclparse_portal_group(const char *, const ucl::Ucl &);
static bool uclparse_transport_group(const char *, const ucl::Ucl &);
static bool uclparse_controller(const char *, const ucl::Ucl &);
static bool uclparse_controller_transport_group(const char *, const ucl::Ucl &);
static bool uclparse_controller_namespace(const char *, const ucl::Ucl &);
static bool uclparse_target(const char *, const ucl::Ucl &);
static bool uclparse_target_portal_group(const char *, const ucl::Ucl &);
static bool uclparse_target_lun(const char *, const ucl::Ucl &);

static bool
uclparse_chap(const char *ag_name, const ucl::Ucl &obj)
{
	auto user = obj["user"];
	if (!user || user.type() != UCL_STRING) {
		log_warnx("chap section in auth-group \"%s\" is missing "
		    "\"user\" string key", ag_name);
		return (false);
	}

	auto secret = obj["secret"];
	if (!secret || secret.type() != UCL_STRING) {
		log_warnx("chap section in auth-group \"%s\" is missing "
		    "\"secret\" string key", ag_name);
		return (false);
	}

	return (auth_group_add_chap(
	    user.string_value().c_str(),
	    secret.string_value().c_str()));
}

static bool
uclparse_chap_mutual(const char *ag_name, const ucl::Ucl &obj)
{
	auto user = obj["user"];
	if (!user || user.type() != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"user\" string key", ag_name);
		return (false);
	}

	auto secret = obj["secret"];
	if (!secret || secret.type() != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"secret\" string key", ag_name);
		return (false);
	}

	auto mutual_user = obj["mutual-user"];
	if (!mutual_user || mutual_user.type() != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"mutual-user\" string key", ag_name);
		return (false);
	}

	auto mutual_secret = obj["mutual-secret"];
	if (!mutual_secret || mutual_secret.type() != UCL_STRING) {
		log_warnx("chap-mutual section in auth-group \"%s\" is missing "
		    "\"mutual-secret\" string key", ag_name);
		return (false);
	}

	return (auth_group_add_chap_mutual(
	    user.string_value().c_str(),
	    secret.string_value().c_str(),
	    mutual_user.string_value().c_str(),
	    mutual_secret.string_value().c_str()));
}

static bool
uclparse_target_chap(const char *t_name, const ucl::Ucl &obj)
{
	auto user = obj["user"];
	if (!user || user.type() != UCL_STRING) {
		log_warnx("chap section in target \"%s\" is missing "
		    "\"user\" string key", t_name);
		return (false);
	}

	auto secret = obj["secret"];
	if (!secret || secret.type() != UCL_STRING) {
		log_warnx("chap section in target \"%s\" is missing "
		    "\"secret\" string key", t_name);
		return (false);
	}

	return (target_add_chap(
	    user.string_value().c_str(),
	    secret.string_value().c_str()));
}

static bool
uclparse_target_chap_mutual(const char *t_name, const ucl::Ucl &obj)
{
	auto user = obj["user"];
	if (!user || user.type() != UCL_STRING) {
		log_warnx("chap-mutual section in target \"%s\" is missing "
		    "\"user\" string key", t_name);
		return (false);
	}

	auto secret = obj["secret"];
	if (!secret || secret.type() != UCL_STRING) {
		log_warnx("chap-mutual section in target \"%s\" is missing "
		    "\"secret\" string key", t_name);
		return (false);
	}

	auto mutual_user = obj["mutual-user"];
	if (!mutual_user || mutual_user.type() != UCL_STRING) {
		log_warnx("chap-mutual section in target \"%s\" is missing "
		    "\"mutual-user\" string key", t_name);
		return (false);
	}

	auto mutual_secret = obj["mutual-secret"];
	if (!mutual_secret || mutual_secret.type() != UCL_STRING) {
		log_warnx("chap-mutual section in target \"%s\" is missing "
		    "\"mutual-secret\" string key", t_name);
		return (false);
	}

	return (target_add_chap_mutual(
	    user.string_value().c_str(),
	    secret.string_value().c_str(),
	    mutual_user.string_value().c_str(),
	    mutual_secret.string_value().c_str()));
}

static bool
uclparse_target_portal_group(const char *t_name, const ucl::Ucl &obj)
{
	/*
	 * If the value is a single string, assume it is a
	 * portal-group name.
	 */
	if (obj.type() == UCL_STRING)
		return (target_add_portal_group(obj.string_value().c_str(),
		    NULL));

	if (obj.type() != UCL_OBJECT) {
		log_warnx("portal-group section in target \"%s\" must be "
		    "an object or string", t_name);
		return (false);
	}

	auto portal_group = obj["name"];
	if (!portal_group || portal_group.type() != UCL_STRING) {
		log_warnx("portal-group section in target \"%s\" is missing "
		    "\"name\" string key", t_name);
		return (false);
	}

	auto auth_group = obj["auth-group-name"];
	if (auth_group) {
		if (auth_group.type() != UCL_STRING) {
			log_warnx("\"auth-group-name\" property in "
			    "portal-group section for target \"%s\" is not "
			    "a string", t_name);
			return (false);
		}
		return (target_add_portal_group(
		    portal_group.string_value().c_str(),
		    auth_group.string_value().c_str()));
	}

	return (target_add_portal_group(portal_group.string_value().c_str(),
	    NULL));
}

static bool
uclparse_controller_transport_group(const char *t_name, const ucl::Ucl &obj)
{
	/*
	 * If the value is a single string, assume it is a
	 * transport-group name.
	 */
	if (obj.type() == UCL_STRING)
		return target_add_portal_group(obj.string_value().c_str(),
		    nullptr);

	if (obj.type() != UCL_OBJECT) {
		log_warnx("transport-group section in controller \"%s\" must "
		    "be an object or string", t_name);
		return false;
	}

	auto portal_group = obj["name"];
	if (!portal_group || portal_group.type() != UCL_STRING) {
		log_warnx("transport-group section in controller \"%s\" is "
		    "missing \"name\" string key", t_name);
		return false;
	}

	auto auth_group = obj["auth-group-name"];
	if (auth_group) {
		if (auth_group.type() != UCL_STRING) {
			log_warnx("\"auth-group-name\" property in "
			    "transport-group section for controller \"%s\" is "
			    "not a string", t_name);
			return false;
		}
		return target_add_portal_group(
		    portal_group.string_value().c_str(),
		    auth_group.string_value().c_str());
	}

	return target_add_portal_group(portal_group.string_value().c_str(),
	    nullptr);
}

static bool
uclparse_target_lun(const char *t_name, const ucl::Ucl &obj)
{
	char *end;
	u_int id;

	std::string key = obj.key();
	if (!key.empty()) {
		id = strtoul(key.c_str(), &end, 0);
		if (*end != '\0') {
			log_warnx("lun key \"%s\" in target \"%s\" is invalid",
			    key.c_str(), t_name);
			return (false);
		}

		if (obj.type() == UCL_STRING)
			return (target_add_lun(id, obj.string_value().c_str()));
	}

	if (obj.type() != UCL_OBJECT) {
		log_warnx("lun section entries in target \"%s\" must be objects",
		    t_name);
		return (false);
	}

	if (key.empty()) {
		auto num = obj["number"];
		if (!num || num.type() != UCL_INT) {
			log_warnx("lun section in target \"%s\" is missing "
			    "\"number\" integer property", t_name);
			return (false);
		}
		id = num.int_value();
	}

	auto name = obj["name"];
	if (!name) {
		if (!target_start_lun(id))
			return (false);

		scope_exit finisher(lun_finish);
		std::string lun_name =
		    freebsd::stringf("lun %u for target \"%s\"", id, t_name);
		return (uclparse_lun_entries(lun_name.c_str(), obj));
	}

	if (name.type() != UCL_STRING) {
		log_warnx("\"name\" property for lun %u for target "
		    "\"%s\" is not a string", id, t_name);
		return (false);
	}

	return (target_add_lun(id, name.string_value().c_str()));
}

static bool
uclparse_controller_namespace(const char *t_name, const ucl::Ucl &obj)
{
	char *end;
	u_int id;

	std::string key = obj.key();
	if (!key.empty()) {
		id = strtoul(key.c_str(), &end, 0);
		if (*end != '\0') {
			log_warnx("namespace key \"%s\" in controller \"%s\""
			    " is invalid", key.c_str(), t_name);
			return false;
		}

		if (obj.type() == UCL_STRING)
			return controller_add_namespace(id,
			    obj.string_value().c_str());
	}

	if (obj.type() != UCL_OBJECT) {
		log_warnx("namespace section entries in controller \"%s\""
		    " must be objects", t_name);
		return false;
	}

	if (key.empty()) {
		auto num = obj["number"];
		if (!num || num.type() != UCL_INT) {
			log_warnx("namespace section in controller \"%s\" is "
			    "missing \"id\" integer property", t_name);
			return (false);
		}
		id = num.int_value();
	}

	auto name = obj["name"];
	if (!name) {
		if (!controller_start_namespace(id))
			return false;

		std::string lun_name =
		    freebsd::stringf("namespace %u for controller \"%s\"", id,
			t_name);
		return uclparse_lun_entries(lun_name.c_str(), obj);
	}

	if (name.type() != UCL_STRING) {
		log_warnx("\"name\" property for namespace %u for "
		    "controller \"%s\" is not a string", id, t_name);
		return (false);
	}

	return controller_add_namespace(id, name.string_value().c_str());
}

static bool
uclparse_toplevel(const ucl::Ucl &top)
{
	/* Pass 1 - everything except targets */
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key == "debug") {
			if (obj.type() == UCL_INT)
				conf_set_debug(obj.int_value());
			else {
				log_warnx("\"debug\" property value is not integer");
				return (false);
			}
		}

		if (key == "timeout") {
			if (obj.type() == UCL_INT)
				conf_set_timeout(obj.int_value());
			else {
				log_warnx("\"timeout\" property value is not integer");
				return (false);
			}
		}

		if (key == "maxproc") {
			if (obj.type() == UCL_INT)
				conf_set_maxproc(obj.int_value());
			else {
				log_warnx("\"maxproc\" property value is not integer");
				return (false);
			}
		}

		if (key == "pidfile") {
			if (obj.type() == UCL_STRING) {
				if (!conf_set_pidfile_path(
				    obj.string_value().c_str()))
					return (false);
			} else {
				log_warnx("\"pidfile\" property value is not string");
				return (false);
			}
		}

		if (key == "isns-server") {
			if (obj.type() == UCL_ARRAY) {
				for (const auto &child : obj) {
					if (child.type() != UCL_STRING)
						return (false);

					if (!isns_add_server(
					    child.string_value().c_str()))
						return (false);
				}
			} else {
				log_warnx("\"isns-server\" property value is "
				    "not an array");
				return (false);
			}
		}

		if (key == "isns-period") {
			if (obj.type() == UCL_INT)
				conf_set_isns_period(obj.int_value());
			else {
				log_warnx("\"isns-period\" property value is not integer");
				return (false);
			}
		}

		if (key == "isns-timeout") {
			if (obj.type() == UCL_INT)
				conf_set_isns_timeout(obj.int_value());
			else {
				log_warnx("\"isns-timeout\" property value is not integer");
				return (false);
			}
		}

		if (key == "auth-group") {
			if (obj.type() == UCL_OBJECT) {
				for (const auto &child : obj) {
					if (!uclparse_auth_group(
					    child.key().c_str(), child))
						return (false);
				}
			} else {
				log_warnx("\"auth-group\" section is not an object");
				return (false);
			}
		}

		if (key == "portal-group") {
			if (obj.type() == UCL_OBJECT) {
				for (const auto &child : obj) {
					if (!uclparse_portal_group(
					    child.key().c_str(), child))
						return (false);
				}
			} else {
				log_warnx("\"portal-group\" section is not an object");
				return (false);
			}
		}

		if (key == "transport-group") {
			if (obj.type() == UCL_OBJECT) {
				for (const auto &child : obj) {
					if (!uclparse_transport_group(
					    child.key().c_str(), child))
						return false;
				}
			} else {
				log_warnx("\"transport-group\" section is not an object");
				return false;
			}
		}

		if (key == "lun") {
			if (obj.type() == UCL_OBJECT) {
				for (const auto &child : obj) {
					if (!uclparse_lun(child.key().c_str(),
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
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key == "controller") {
			if (obj.type() == UCL_OBJECT) {
				for (const auto &child : obj) {
					if (!uclparse_controller(
					    child.key().c_str(), child))
						return false;
				}
			} else {
				log_warnx("\"controller\" section is not an object");
				return false;
			}
		}

		if (key == "target") {
			if (obj.type() == UCL_OBJECT) {
				for (const auto &child : obj) {
					if (!uclparse_target(
					    child.key().c_str(), child))
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
uclparse_auth_group(const char *name, const ucl::Ucl &top)
{
	if (!auth_group_start(name))
		return (false);

	scope_exit finisher(auth_group_finish);
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key == "auth-type") {
			if (!auth_group_set_type(obj.string_value().c_str()))
				return false;
		}

		if (key == "chap") {
			if (obj.type() == UCL_OBJECT) {
				if (!uclparse_chap(name, obj))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!uclparse_chap(name, tmp))
						return false;
				}
			} else {
				log_warnx("\"chap\" property of auth-group "
				    "\"%s\" is not an array or object",
				    name);
				return false;
			}
		}

		if (key == "chap-mutual") {
			if (obj.type() == UCL_OBJECT) {
				if (!uclparse_chap_mutual(name, obj))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!uclparse_chap_mutual(name, tmp))
						return false;
				}
			} else {
				log_warnx("\"chap-mutual\" property of "
				    "auth-group \"%s\" is not an array or object",
				    name);
				return false;
			}
		}

		if (key == "host-address") {
			if (obj.type() == UCL_STRING) {
				if (!auth_group_add_host_address(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!auth_group_add_host_address(
					    tmp.string_value().c_str()))
						return false;
				}
			} else {
				log_warnx("\"host-address\" property of "
				    "auth-group \"%s\" is not an array or string",
				    name);
				return false;
			}
		}

		if (key == "host-nqn") {
			if (obj.type() == UCL_STRING) {
				if (!auth_group_add_host_nqn(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!auth_group_add_host_nqn(
					    tmp.string_value().c_str()))
						return false;
				}
			} else {
				log_warnx("\"host-nqn\" property of "
				    "auth-group \"%s\" is not an array or string",
				    name);
				return false;
			}
		}

		if (key == "initiator-name") {
			if (obj.type() == UCL_STRING) {
				if (!auth_group_add_initiator_name(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!auth_group_add_initiator_name(
					    tmp.string_value().c_str()))
						return false;
				}
			} else {
				log_warnx("\"initiator-name\" property of "
				    "auth-group \"%s\" is not an array or string",
				    name);
				return false;
			}
		}

		if (key == "initiator-portal") {
			if (obj.type() == UCL_STRING) {
				if (!auth_group_add_initiator_portal(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!auth_group_add_initiator_portal(
					    tmp.string_value().c_str()))
						return false;
				}
			} else {
				log_warnx("\"initiator-portal\" property of "
				    "auth-group \"%s\" is not an array or string",
				    name);
				return false;
			}
		}
	}

	return (true);
}

static bool
uclparse_dscp(const char *group_type, const char *pg_name,
    const ucl::Ucl &obj)
{
	if ((obj.type() != UCL_STRING) && (obj.type() != UCL_INT)) {
		log_warnx("\"dscp\" property of %s group \"%s\" is not a "
		    "string or integer", group_type, pg_name);
		return (false);
	}
	if (obj.type() == UCL_INT)
		return (portal_group_set_dscp(obj.int_value()));

	std::string key = obj.key();
	if (key == "be" || key == "cs0")
		portal_group_set_dscp(IPTOS_DSCP_CS0 >> 2);
	else if (key == "ef")
		portal_group_set_dscp(IPTOS_DSCP_EF >> 2);
	else if (key == "cs0")
		portal_group_set_dscp(IPTOS_DSCP_CS0 >> 2);
	else if (key == "cs1")
		portal_group_set_dscp(IPTOS_DSCP_CS1 >> 2);
	else if (key == "cs2")
		portal_group_set_dscp(IPTOS_DSCP_CS2 >> 2);
	else if (key == "cs3")
		portal_group_set_dscp(IPTOS_DSCP_CS3 >> 2);
	else if (key == "cs4")
		portal_group_set_dscp(IPTOS_DSCP_CS4 >> 2);
	else if (key == "cs5")
		portal_group_set_dscp(IPTOS_DSCP_CS5 >> 2);
	else if (key == "cs6")
		portal_group_set_dscp(IPTOS_DSCP_CS6 >> 2);
	else if (key == "cs7")
		portal_group_set_dscp(IPTOS_DSCP_CS7 >> 2);
	else if (key == "af11")
		portal_group_set_dscp(IPTOS_DSCP_AF11 >> 2);
	else if (key == "af12")
		portal_group_set_dscp(IPTOS_DSCP_AF12 >> 2);
	else if (key == "af13")
		portal_group_set_dscp(IPTOS_DSCP_AF13 >> 2);
	else if (key == "af21")
		portal_group_set_dscp(IPTOS_DSCP_AF21 >> 2);
	else if (key == "af22")
		portal_group_set_dscp(IPTOS_DSCP_AF22 >> 2);
	else if (key == "af23")
		portal_group_set_dscp(IPTOS_DSCP_AF23 >> 2);
	else if (key == "af31")
		portal_group_set_dscp(IPTOS_DSCP_AF31 >> 2);
	else if (key == "af32")
		portal_group_set_dscp(IPTOS_DSCP_AF32 >> 2);
	else if (key == "af33")
		portal_group_set_dscp(IPTOS_DSCP_AF33 >> 2);
	else if (key == "af41")
		portal_group_set_dscp(IPTOS_DSCP_AF41 >> 2);
	else if (key == "af42")
		portal_group_set_dscp(IPTOS_DSCP_AF42 >> 2);
	else if (key == "af43")
		portal_group_set_dscp(IPTOS_DSCP_AF43 >> 2);
	else {
		log_warnx("\"dscp\" property value is not a supported textual value");
		return (false);
	}
	return (true);
}

static bool
uclparse_pcp(const char *group_type, const char *pg_name,
    const ucl::Ucl &obj)
{
	if (obj.type() != UCL_INT) {
		log_warnx("\"pcp\" property of %s group \"%s\" is not an "
		    "integer", group_type, pg_name);
		return (false);
	}
	return (portal_group_set_pcp(obj.int_value()));
}

static bool
uclparse_portal_group(const char *name, const ucl::Ucl &top)
{
	if (!portal_group_start(name))
		return (false);

	scope_exit finisher(portal_group_finish);
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key == "discovery-auth-group") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"discovery-auth-group\" property "
				    "of portal-group \"%s\" is not a string",
				    name);
				return false;
			}

			if (!portal_group_set_discovery_auth_group(
			    obj.string_value().c_str()))
				return false;
		}

		if (key == "discovery-filter") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"discovery-filter\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				return false;
			}

			if (!portal_group_set_filter(
			    obj.string_value().c_str()))
				return false;
		}

		if (key == "foreign") {
			portal_group_set_foreign();
		}

		if (key == "listen") {
			if (obj.type() == UCL_STRING) {
				if (!portal_group_add_listen(
				    obj.string_value().c_str(), false))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!portal_group_add_listen(
					    tmp.string_value().c_str(),
					    false))
						return false;
				}
			} else {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				return false;
			}
		}

		if (key == "listen-iser") {
			if (obj.type() == UCL_STRING) {
				if (!portal_group_add_listen(
				    obj.string_value().c_str(), true))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!portal_group_add_listen(
					    tmp.string_value().c_str(),
					    true))
						return false;
				}
			} else {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				return false;
			}
		}

		if (key == "offload") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"offload\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				return false;
			}

			if (!portal_group_set_offload(
			    obj.string_value().c_str()))
				return false;
		}

		if (key == "redirect") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"listen\" property of "
				    "portal-group \"%s\" is not a string",
				    name);
				return false;
			}

			if (!portal_group_set_redirection(
			    obj.string_value().c_str()))
				return false;
		}

		if (key == "options") {
			if (obj.type() != UCL_OBJECT) {
				log_warnx("\"options\" property of portal group "
				    "\"%s\" is not an object", name);
				return false;
			}

			for (const auto &tmp : obj) {
				if (!portal_group_add_option(
				    tmp.key().c_str(),
				    tmp.forced_string_value().c_str()))
					return false;
			}
		}

		if (key == "tag") {
			if (obj.type() != UCL_INT) {
				log_warnx("\"tag\" property of portal group "
				    "\"%s\" is not an integer",
				    name);
				return false;
			}

			portal_group_set_tag(obj.int_value());
		}

		if (key == "dscp") {
			if (!uclparse_dscp("portal", name, obj))
				return false;
		}

		if (key == "pcp") {
			if (!uclparse_pcp("portal", name, obj))
				return false;
		}
	}

	return (true);
}

static bool
uclparse_transport_listen_obj(const char *pg_name, const ucl::Ucl &top)
{
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key.empty()) {
			log_warnx("missing protocol for \"listen\" "
			    "property of transport-group \"%s\"", pg_name);
			return false;
		}

		if (key == "tcp") {
			if (obj.type() == UCL_STRING) {
				if (!transport_group_add_listen_tcp(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!transport_group_add_listen_tcp(
					    tmp.string_value().c_str()))
						return false;
				}
			}
		} else if (key == "discovery-tcp") {
			if (obj.type() == UCL_STRING) {
				if (!transport_group_add_listen_discovery_tcp(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!transport_group_add_listen_discovery_tcp(
					    tmp.string_value().c_str()))
						return false;
				}
			}
		} else {
			log_warnx("invalid listen protocol \"%s\" for "
			    "transport-group \"%s\"", key.c_str(), pg_name);
			return false;
		}
	}
	return true;
}

static bool
uclparse_transport_group(const char *name, const ucl::Ucl &top)
{
	if (!transport_group_start(name))
		return false;

	scope_exit finisher(portal_group_finish);
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key == "discovery-auth-group") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"discovery-auth-group\" property "
				    "of transport-group \"%s\" is not a string",
				    name);
				return false;
			}

			if (!portal_group_set_discovery_auth_group(
			    obj.string_value().c_str()))
				return false;
		}

		if (key == "discovery-filter") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"discovery-filter\" property of "
				    "transport-group \"%s\" is not a string",
				    name);
				return false;
			}

			if (!portal_group_set_filter(
			    obj.string_value().c_str()))
				return false;
		}

		if (key == "listen") {
			if (obj.type() != UCL_OBJECT) {
				log_warnx("\"listen\" property of "
				    "transport-group \"%s\" is not an object",
				    name);
				return false;
			}
			if (!uclparse_transport_listen_obj(name, obj))
				return false;
		}

		if (key == "options") {
			if (obj.type() != UCL_OBJECT) {
				log_warnx("\"options\" property of transport group "
				    "\"%s\" is not an object", name);
				return false;
			}

			for (const auto &tmp : obj) {
				if (!portal_group_add_option(
				    tmp.key().c_str(),
				    tmp.forced_string_value().c_str()))
					return false;
			}
		}

		if (key == "dscp") {
			if (!uclparse_dscp("transport", name, obj))
				return false;
		}

		if (key == "pcp") {
			if (!uclparse_pcp("transport", name, obj))
				return false;
		}
	}

	return true;
}

static bool
uclparse_controller(const char *name, const ucl::Ucl &top)
{
	if (!controller_start(name))
		return false;

	scope_exit finisher(target_finish);
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key == "auth-group") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"auth-group\" property of "
				    "controller \"%s\" is not a string", name);
				return false;
			}

			if (!target_set_auth_group(obj.string_value().c_str()))
				return false;
		}

		if (key == "auth-type") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"auth-type\" property of "
				    "controller \"%s\" is not a string", name);
				return false;
			}

			if (!target_set_auth_type(obj.string_value().c_str()))
				return false;
		}

		if (key == "host-address") {
			if (obj.type() == UCL_STRING) {
				if (!controller_add_host_address(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!controller_add_host_address(
					    tmp.string_value().c_str()))
						return false;
				}
			} else {
				log_warnx("\"host-address\" property of "
				    "controller \"%s\" is not an array or "
				    "string", name);
				return false;
			}
		}

		if (key == "host-nqn") {
			if (obj.type() == UCL_STRING) {
				if (!controller_add_host_nqn(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!controller_add_host_nqn(
					    tmp.string_value().c_str()))
						return false;
				}
			} else {
				log_warnx("\"host-nqn\" property of "
				    "controller \"%s\" is not an array or "
				    "string", name);
				return false;
			}
		}

		if (key == "transport-group") {
			if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!uclparse_controller_transport_group(name,
					    tmp))
						return false;
				}
			} else {
				if (!uclparse_controller_transport_group(name,
				    obj))
					return false;
			}
		}

		if (key == "namespace") {
			for (const auto &tmp : obj) {
				if (!uclparse_controller_namespace(name, tmp))
					return false;
			}
		}
	}

	return true;
}

static bool
uclparse_target(const char *name, const ucl::Ucl &top)
{
	if (!target_start(name))
		return (false);

	scope_exit finisher(target_finish);
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key == "alias") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"alias\" property of target "
				    "\"%s\" is not a string", name);
				return false;
			}

			if (!target_set_alias(obj.string_value().c_str()))
				return false;
		}

		if (key == "auth-group") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"auth-group\" property of target "
				    "\"%s\" is not a string", name);
				return false;
			}

			if (!target_set_auth_group(obj.string_value().c_str()))
				return false;
		}

		if (key == "auth-type") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"auth-type\" property of target "
				    "\"%s\" is not a string", name);
				return false;
			}

			if (!target_set_auth_type(obj.string_value().c_str()))
				return false;
		}

		if (key == "chap") {
			if (obj.type() == UCL_OBJECT) {
				if (!uclparse_target_chap(name, obj))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!uclparse_target_chap(name, tmp))
						return false;
				}
			} else {
				log_warnx("\"chap\" property of target "
				    "\"%s\" is not an array or object",
				    name);
				return false;
			}
		}

		if (key == "chap-mutual") {
			if (obj.type() == UCL_OBJECT) {
				if (!uclparse_target_chap_mutual(name, obj))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!uclparse_target_chap_mutual(name,
					    tmp))
						return false;
				}
			} else {
				log_warnx("\"chap-mutual\" property of target "
				    "\"%s\" is not an array or object",
				    name);
				return false;
			}
		}

		if (key == "initiator-name") {
			if (obj.type() == UCL_STRING) {
				if (!target_add_initiator_name(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!target_add_initiator_name(
					    tmp.string_value().c_str()))
						return false;
				}
			} else {
				log_warnx("\"initiator-name\" property of "
				    "target \"%s\" is not an array or string",
				    name);
				return false;
			}
		}

		if (key == "initiator-portal") {
			if (obj.type() == UCL_STRING) {
				if (!target_add_initiator_portal(
				    obj.string_value().c_str()))
					return false;
			} else if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!target_add_initiator_portal(
					    tmp.string_value().c_str()))
						return false;
				}
			} else {
				log_warnx("\"initiator-portal\" property of "
				    "target \"%s\" is not an array or string",
				    name);
				return false;
			}
		}

		if (key == "portal-group") {
			if (obj.type() == UCL_ARRAY) {
				for (const auto &tmp : obj) {
					if (!uclparse_target_portal_group(name,
					    tmp))
						return false;
				}
			} else {
				if (!uclparse_target_portal_group(name, obj))
					return false;
			}
		}

		if (key == "port") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"port\" property of target "
				    "\"%s\" is not a string", name);
				return false;
			}

			if (!target_set_physical_port(obj.string_value().c_str()))
				return false;
		}

		if (key == "redirect") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"redirect\" property of target "
				    "\"%s\" is not a string", name);
				return false;
			}

			if (!target_set_redirection(obj.string_value().c_str()))
				return false;
		}

		if (key == "lun") {
			for (const auto &tmp : obj) {
				if (!uclparse_target_lun(name, tmp))
					return false;
			}
		}
	}

	return (true);
}

static bool
uclparse_lun(const char *name, const ucl::Ucl &top)
{
	if (!lun_start(name))
		return (false);

	scope_exit finisher(lun_finish);
	std::string lun_name = freebsd::stringf("lun \"%s\"", name);
	return (uclparse_lun_entries(lun_name.c_str(), top));
}

static bool
uclparse_lun_entries(const char *name, const ucl::Ucl &top)
{
	for (const auto &obj : top) {
		std::string key = obj.key();

		if (key == "backend") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"backend\" property of %s "
				    "is not a string", name);
				return false;
			}

			if (!lun_set_backend(obj.string_value().c_str()))
				return false;
		}

		if (key == "blocksize") {
			if (obj.type() != UCL_INT) {
				log_warnx("\"blocksize\" property of %s "
				    "is not an integer", name);
				return false;
			}

			if (!lun_set_blocksize(obj.int_value()))
				return false;
		}

		if (key == "device-id") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"device-id\" property of %s "
				    "is not an integer", name);
				return false;
			}

			if (!lun_set_device_id(obj.string_value().c_str()))
				return false;
		}

		if (key == "device-type") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"device-type\" property of %s "
				    "is not an integer", name);
				return false;
			}

			if (!lun_set_device_type(obj.string_value().c_str()))
				return false;
		}

		if (key == "ctl-lun") {
			if (obj.type() != UCL_INT) {
				log_warnx("\"ctl-lun\" property of %s "
				    "is not an integer", name);
				return false;
			}

			if (!lun_set_ctl_lun(obj.int_value()))
				return false;
		}

		if (key == "options") {
			if (obj.type() != UCL_OBJECT) {
				log_warnx("\"options\" property of %s "
				    "is not an object", name);
				return false;
			}

			for (const auto &child : obj) {
				if (!lun_add_option(child.key().c_str(),
				    child.forced_string_value().c_str()))
					return false;
			}
		}

		if (key == "path") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"path\" property of %s "
				    "is not a string", name);
				return false;
			}

			if (!lun_set_path(obj.string_value().c_str()))
				return false;
		}

		if (key == "serial") {
			if (obj.type() != UCL_STRING) {
				log_warnx("\"serial\" property of %s "
				    "is not a string", name);
				return false;
			}

			if (!lun_set_serial(obj.string_value().c_str()))
				return false;
		}

		if (key == "size") {
			if (obj.type() != UCL_INT) {
				log_warnx("\"size\" property of %s "
				    "is not an integer", name);
				return false;
			}

			if (!lun_set_size(obj.int_value()))
				return false;
		}
	}

	return (true);
}

bool
uclparse_conf(const char *path)
{
	std::string err;
	ucl::Ucl top = ucl::Ucl::parse_from_file(path, err);
	if (!top) {
		log_warnx("unable to parse configuration file %s: %s", path,
		    err.c_str());
		return (false);
	}

	bool parsed;
	try {
		parsed = uclparse_toplevel(top);
	} catch (std::bad_alloc &) {
		log_warnx("failed to allocate memory parsing %s", path);
		parsed = false;
	} catch (...) {
		log_warnx("unknown exception parsing %s", path);
		parsed = false;
	}

	return (parsed);
}
