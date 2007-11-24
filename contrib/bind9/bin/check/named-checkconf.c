/*
 * Copyright (C) 2004-2006  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: named-checkconf.c,v 1.12.12.11 2006/03/02 00:37:20 marka Exp $ */

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
#include <dns/result.h>

#include "check-tool.h"

isc_log_t *logc = NULL;

#define CHECK(r)\
	do { \
		result = (r); \
		if (result != ISC_R_SUCCESS) \
			goto cleanup; \
	} while (0)

static void
usage(void) {
        fprintf(stderr, "usage: named-checkconf [-j] [-v] [-z] [-t directory] "
		"[named.conf]\n");
        exit(1);
}

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

static isc_result_t
configure_zone(const char *vclass, const char *view,
	       const cfg_obj_t *zconfig, isc_mem_t *mctx)
{
	isc_result_t result;
	const char *zclass;
	const char *zname;
	const char *zfile;
	const cfg_obj_t *zoptions = NULL;
	const cfg_obj_t *classobj = NULL;
	const cfg_obj_t *typeobj = NULL;
	const cfg_obj_t *fileobj = NULL;
	const cfg_obj_t *dbobj = NULL;

	zname = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));
	classobj = cfg_tuple_get(zconfig, "class");
        if (!cfg_obj_isstring(classobj))
                zclass = vclass;
        else
		zclass = cfg_obj_asstring(classobj);
	zoptions = cfg_tuple_get(zconfig, "options");
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
	result = load_zone(mctx, zname, zfile, zclass, NULL);
	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "%s/%s/%s: %s\n", view, zname, zclass,
			dns_result_totext(result));
	return(result);
}

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
		tresult = configure_zone(vclass, view, zconfig, mctx);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
	}
	return (result);
}


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
	
	while ((c = isc_commandline_parse(argc, argv, "djt:vz")) != EOF) {
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
			result = isc_dir_chdir("/");
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "isc_dir_chdir: %s\n",
					isc_result_totext(result));
				exit(1);
			}
			break;

		case 'v':
			printf(VERSION "\n");
			exit(0);

		case 'z':
			load_zones = ISC_TRUE;
			break;

		default:
			usage();
		}
	}

	if (argv[isc_commandline_index] != NULL)
		conffile = argv[isc_commandline_index];
	if (conffile == NULL || conffile[0] == '\0')
		conffile = NAMED_CONFFILE;

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	RUNTIME_CHECK(setup_logging(mctx, &logc) == ISC_R_SUCCESS);

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
		dns_log_init(logc);
                dns_log_setcontext(logc);
		result = load_zones_fromconfig(config, mctx);
		if (result != ISC_R_SUCCESS)
			exit_status = 1;
	}

	cfg_obj_destroy(parser, &config);

	cfg_parser_destroy(&parser);

	isc_log_destroy(&logc);

	isc_hash_destroy();
	isc_entropy_detach(&ectx);

	isc_mem_destroy(&mctx);

	return (exit_status);
}
