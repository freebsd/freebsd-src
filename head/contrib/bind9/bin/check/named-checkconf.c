/*
 * Copyright (C) 2004-2007, 2009-2011  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: named-checkconf.c,v 1.54.62.2 2011-03-12 04:59:13 tbox Exp $ */

/*! \file */

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <isc/commandline.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/util.h>

#include <isccfg/namedconf.h>

#include <bind9/check.h>

#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/result.h>
#include <dns/zone.h>

#include "check-tool.h"

static const char *program = "named-checkconf";

isc_log_t *logc = NULL;

#define CHECK(r)\
	do { \
		result = (r); \
		if (result != ISC_R_SUCCESS) \
			goto cleanup; \
	} while (0)

/*% usage */
ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "usage: %s [-h] [-j] [-p] [-v] [-z] [-t directory] "
		"[named.conf]\n", program);
	exit(1);
}

/*% directory callback */
static isc_result_t
directory_callback(const char *clausename, const cfg_obj_t *obj, void *arg) {
	isc_result_t result;
	const char *directory;

	REQUIRE(strcasecmp("directory", clausename) == 0);

	UNUSED(arg);
	UNUSED(clausename);

	/*
	 * Change directory.
	 */
	directory = cfg_obj_asstring(obj);
	result = isc_dir_chdir(directory);
	if (result != ISC_R_SUCCESS) {
		cfg_obj_log(obj, logc, ISC_LOG_ERROR,
			    "change directory to '%s' failed: %s\n",
			    directory, isc_result_totext(result));
		return (result);
	}

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
get_maps(const cfg_obj_t **maps, const char *name, const cfg_obj_t **obj) {
	int i;
	for (i = 0;; i++) {
		if (maps[i] == NULL)
			return (ISC_FALSE);
		if (cfg_map_get(maps[i], name, obj) == ISC_R_SUCCESS)
			return (ISC_TRUE);
	}
}

static isc_boolean_t
get_checknames(const cfg_obj_t **maps, const cfg_obj_t **obj) {
	const cfg_listelt_t *element;
	const cfg_obj_t *checknames;
	const cfg_obj_t *type;
	const cfg_obj_t *value;
	isc_result_t result;
	int i;

	for (i = 0;; i++) {
		if (maps[i] == NULL)
			return (ISC_FALSE);
		checknames = NULL;
		result = cfg_map_get(maps[i], "check-names", &checknames);
		if (result != ISC_R_SUCCESS)
			continue;
		if (checknames != NULL && !cfg_obj_islist(checknames)) {
			*obj = checknames;
			return (ISC_TRUE);
		}
		for (element = cfg_list_first(checknames);
		     element != NULL;
		     element = cfg_list_next(element)) {
			value = cfg_listelt_value(element);
			type = cfg_tuple_get(value, "type");
			if (strcasecmp(cfg_obj_asstring(type), "master") != 0)
				continue;
			*obj = cfg_tuple_get(value, "mode");
			return (ISC_TRUE);
		}
	}
}

static isc_result_t
config_get(const cfg_obj_t **maps, const char *name, const cfg_obj_t **obj) {
	int i;

	for (i = 0;; i++) {
		if (maps[i] == NULL)
			return (ISC_R_NOTFOUND);
		if (cfg_map_get(maps[i], name, obj) == ISC_R_SUCCESS)
			return (ISC_R_SUCCESS);
	}
}

/*% configure the zone */
static isc_result_t
configure_zone(const char *vclass, const char *view,
	       const cfg_obj_t *zconfig, const cfg_obj_t *vconfig,
	       const cfg_obj_t *config, isc_mem_t *mctx)
{
	int i = 0;
	isc_result_t result;
	const char *zclass;
	const char *zname;
	const char *zfile;
	const cfg_obj_t *maps[4];
	const cfg_obj_t *zoptions = NULL;
	const cfg_obj_t *classobj = NULL;
	const cfg_obj_t *typeobj = NULL;
	const cfg_obj_t *fileobj = NULL;
	const cfg_obj_t *dbobj = NULL;
	const cfg_obj_t *obj = NULL;
	const cfg_obj_t *fmtobj = NULL;
	dns_masterformat_t masterformat;

	zone_options = DNS_ZONEOPT_CHECKNS | DNS_ZONEOPT_MANYERRORS;

	zname = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));
	classobj = cfg_tuple_get(zconfig, "class");
	if (!cfg_obj_isstring(classobj))
		zclass = vclass;
	else
		zclass = cfg_obj_asstring(classobj);

	zoptions = cfg_tuple_get(zconfig, "options");
	maps[i++] = zoptions;
	if (vconfig != NULL)
		maps[i++] = cfg_tuple_get(vconfig, "options");
	if (config != NULL) {
		cfg_map_get(config, "options", &obj);
		if (obj != NULL)
			maps[i++] = obj;
	}
	maps[i] = NULL;

	cfg_map_get(zoptions, "type", &typeobj);
	if (typeobj == NULL)
		return (ISC_R_FAILURE);
	if (strcasecmp(cfg_obj_asstring(typeobj), "master") != 0)
		return (ISC_R_SUCCESS);
	cfg_map_get(zoptions, "database", &dbobj);
	if (dbobj != NULL)
		return (ISC_R_SUCCESS);
	cfg_map_get(zoptions, "file", &fileobj);
	if (fileobj == NULL)
		return (ISC_R_FAILURE);
	zfile = cfg_obj_asstring(fileobj);

	obj = NULL;
	if (get_maps(maps, "check-dup-records", &obj)) {
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			zone_options |= DNS_ZONEOPT_CHECKDUPRR;
			zone_options &= ~DNS_ZONEOPT_CHECKDUPRRFAIL;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			zone_options |= DNS_ZONEOPT_CHECKDUPRR;
			zone_options |= DNS_ZONEOPT_CHECKDUPRRFAIL;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			zone_options &= ~DNS_ZONEOPT_CHECKDUPRR;
			zone_options &= ~DNS_ZONEOPT_CHECKDUPRRFAIL;
		} else
			INSIST(0);
	} else {
		zone_options |= DNS_ZONEOPT_CHECKDUPRR;
		zone_options &= ~DNS_ZONEOPT_CHECKDUPRRFAIL;
	}

	obj = NULL;
	if (get_maps(maps, "check-mx", &obj)) {
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			zone_options |= DNS_ZONEOPT_CHECKMX;
			zone_options &= ~DNS_ZONEOPT_CHECKMXFAIL;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			zone_options |= DNS_ZONEOPT_CHECKMX;
			zone_options |= DNS_ZONEOPT_CHECKMXFAIL;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			zone_options &= ~DNS_ZONEOPT_CHECKMX;
			zone_options &= ~DNS_ZONEOPT_CHECKMXFAIL;
		} else
			INSIST(0);
	} else {
		zone_options |= DNS_ZONEOPT_CHECKMX;
		zone_options &= ~DNS_ZONEOPT_CHECKMXFAIL;
	}

	obj = NULL;
	if (get_maps(maps, "check-integrity", &obj)) {
		if (cfg_obj_asboolean(obj))
			zone_options |= DNS_ZONEOPT_CHECKINTEGRITY;
		else
			zone_options &= ~DNS_ZONEOPT_CHECKINTEGRITY;
	} else
		zone_options |= DNS_ZONEOPT_CHECKINTEGRITY;

	obj = NULL;
	if (get_maps(maps, "check-mx-cname", &obj)) {
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			zone_options |= DNS_ZONEOPT_WARNMXCNAME;
			zone_options &= ~DNS_ZONEOPT_IGNOREMXCNAME;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			zone_options &= ~DNS_ZONEOPT_WARNMXCNAME;
			zone_options &= ~DNS_ZONEOPT_IGNOREMXCNAME;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			zone_options |= DNS_ZONEOPT_WARNMXCNAME;
			zone_options |= DNS_ZONEOPT_IGNOREMXCNAME;
		} else
			INSIST(0);
	} else {
		zone_options |= DNS_ZONEOPT_WARNMXCNAME;
		zone_options &= ~DNS_ZONEOPT_IGNOREMXCNAME;
	}

	obj = NULL;
	if (get_maps(maps, "check-srv-cname", &obj)) {
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			zone_options |= DNS_ZONEOPT_WARNSRVCNAME;
			zone_options &= ~DNS_ZONEOPT_IGNORESRVCNAME;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			zone_options &= ~DNS_ZONEOPT_WARNSRVCNAME;
			zone_options &= ~DNS_ZONEOPT_IGNORESRVCNAME;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			zone_options |= DNS_ZONEOPT_WARNSRVCNAME;
			zone_options |= DNS_ZONEOPT_IGNORESRVCNAME;
		} else
			INSIST(0);
	} else {
		zone_options |= DNS_ZONEOPT_WARNSRVCNAME;
		zone_options &= ~DNS_ZONEOPT_IGNORESRVCNAME;
	}

	obj = NULL;
	if (get_maps(maps, "check-sibling", &obj)) {
		if (cfg_obj_asboolean(obj))
			zone_options |= DNS_ZONEOPT_CHECKSIBLING;
		else
			zone_options &= ~DNS_ZONEOPT_CHECKSIBLING;
	}

	obj = NULL;
	if (get_checknames(maps, &obj)) {
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			zone_options |= DNS_ZONEOPT_CHECKNAMES;
			zone_options &= ~DNS_ZONEOPT_CHECKNAMESFAIL;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			zone_options |= DNS_ZONEOPT_CHECKNAMES;
			zone_options |= DNS_ZONEOPT_CHECKNAMESFAIL;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			zone_options &= ~DNS_ZONEOPT_CHECKNAMES;
			zone_options &= ~DNS_ZONEOPT_CHECKNAMESFAIL;
		} else
			INSIST(0);
	} else {
	       zone_options |= DNS_ZONEOPT_CHECKNAMES;
	       zone_options |= DNS_ZONEOPT_CHECKNAMESFAIL;
	}

	masterformat = dns_masterformat_text;
	fmtobj = NULL;
	result = config_get(maps, "masterfile-format", &fmtobj);
	if (result == ISC_R_SUCCESS) {
		const char *masterformatstr = cfg_obj_asstring(fmtobj);
		if (strcasecmp(masterformatstr, "text") == 0)
			masterformat = dns_masterformat_text;
		else if (strcasecmp(masterformatstr, "raw") == 0)
			masterformat = dns_masterformat_raw;
		else
			INSIST(0);
	}

	result = load_zone(mctx, zname, zfile, masterformat, zclass, NULL);
	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "%s/%s/%s: %s\n", view, zname, zclass,
			dns_result_totext(result));
	return(result);
}

/*% configure a view */
static isc_result_t
configure_view(const char *vclass, const char *view, const cfg_obj_t *config,
	       const cfg_obj_t *vconfig, isc_mem_t *mctx)
{
	const cfg_listelt_t *element;
	const cfg_obj_t *voptions;
	const cfg_obj_t *zonelist;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;

	voptions = NULL;
	if (vconfig != NULL)
		voptions = cfg_tuple_get(vconfig, "options");

	zonelist = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "zone", &zonelist);
	else
		(void)cfg_map_get(config, "zone", &zonelist);

	for (element = cfg_list_first(zonelist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *zconfig = cfg_listelt_value(element);
		tresult = configure_zone(vclass, view, zconfig, vconfig,
					 config, mctx);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
	}
	return (result);
}


/*% load zones from the configuration */
static isc_result_t
load_zones_fromconfig(const cfg_obj_t *config, isc_mem_t *mctx) {
	const cfg_listelt_t *element;
	const cfg_obj_t *classobj;
	const cfg_obj_t *views;
	const cfg_obj_t *vconfig;
	const char *vclass;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;

	views = NULL;

	(void)cfg_map_get(config, "view", &views);
	for (element = cfg_list_first(views);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const char *vname;

		vclass = "IN";
		vconfig = cfg_listelt_value(element);
		if (vconfig != NULL) {
			classobj = cfg_tuple_get(vconfig, "class");
			if (cfg_obj_isstring(classobj))
				vclass = cfg_obj_asstring(classobj);
		}
		vname = cfg_obj_asstring(cfg_tuple_get(vconfig, "name"));
		tresult = configure_view(vclass, vname, config, vconfig, mctx);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
	}

	if (views == NULL) {
		tresult = configure_view("IN", "_default", config, NULL, mctx);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
	}
	return (result);
}

static void
output(void *closure, const char *text, int textlen) {
	UNUSED(closure);
	if (fwrite(text, 1, textlen, stdout) != (size_t)textlen) {
		perror("fwrite");
		exit(1);
	}
}

/*% The main processing routine */
int
main(int argc, char **argv) {
	int c;
	cfg_parser_t *parser = NULL;
	cfg_obj_t *config = NULL;
	const char *conffile = NULL;
	isc_mem_t *mctx = NULL;
	isc_result_t result;
	int exit_status = 0;
	isc_entropy_t *ectx = NULL;
	isc_boolean_t load_zones = ISC_FALSE;
	isc_boolean_t print = ISC_FALSE;

	isc_commandline_errprint = ISC_FALSE;

	while ((c = isc_commandline_parse(argc, argv, "dhjt:pvz")) != EOF) {
		switch (c) {
		case 'd':
			debug++;
			break;

		case 'j':
			nomerge = ISC_FALSE;
			break;

		case 't':
			result = isc_dir_chroot(isc_commandline_argument);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "isc_dir_chroot: %s\n",
					isc_result_totext(result));
				exit(1);
			}
			break;

		case 'p':
			print = ISC_TRUE;
			break;

		case 'v':
			printf(VERSION "\n");
			exit(0);

		case 'z':
			load_zones = ISC_TRUE;
			docheckmx = ISC_FALSE;
			docheckns = ISC_FALSE;
			dochecksrv = ISC_FALSE;
			break;

		case '?':
			if (isc_commandline_option != '?')
				fprintf(stderr, "%s: invalid argument -%c\n",
					program, isc_commandline_option);
		case 'h':
			usage();

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	if (isc_commandline_index + 1 < argc)
		usage();
	if (argv[isc_commandline_index] != NULL)
		conffile = argv[isc_commandline_index];
	if (conffile == NULL || conffile[0] == '\0')
		conffile = NAMED_CONFFILE;

#ifdef _WIN32
	InitSockets();
#endif

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	RUNTIME_CHECK(setup_logging(mctx, stdout, &logc) == ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_entropy_create(mctx, &ectx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE)
		      == ISC_R_SUCCESS);

	dns_result_register();

	RUNTIME_CHECK(cfg_parser_create(mctx, logc, &parser) == ISC_R_SUCCESS);

	cfg_parser_setcallback(parser, directory_callback, NULL);

	if (cfg_parse_file(parser, conffile, &cfg_type_namedconf, &config) !=
	    ISC_R_SUCCESS)
		exit(1);

	result = bind9_check_namedconf(config, logc, mctx);
	if (result != ISC_R_SUCCESS)
		exit_status = 1;

	if (result == ISC_R_SUCCESS && load_zones) {
		result = load_zones_fromconfig(config, mctx);
		if (result != ISC_R_SUCCESS)
			exit_status = 1;
	}

	if (print && exit_status == 0)
		cfg_print(config, output, NULL);
	cfg_obj_destroy(parser, &config);

	cfg_parser_destroy(&parser);

	dns_name_destroy();

	isc_log_destroy(&logc);

	isc_hash_destroy();
	isc_entropy_detach(&ectx);

	isc_mem_destroy(&mctx);

#ifdef _WIN32
	DestroySockets();
#endif

	return (exit_status);
}
