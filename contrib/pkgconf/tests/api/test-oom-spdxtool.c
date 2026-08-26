/*
 * test-oom.c
 * Allocation-failure (OOM) tests for spdxtool, using the oom.h injection
 * harness.  For each target, a failure is injected at every successive
 * allocation; the target must report the error gracefully (NULL) and, under
 * ASAN, leak nothing on the error path.
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "oom.h"
#include "core.h"
#include "software.h"
#include "simplelicensing.h"
#include "serialize.h"
#include "util.h"

static pkgconf_client_t *g_client;

static void
oom_setup(void)
{
	g_client = test_client_new();
	// Mirror cli/spdxtool/main.c: the constructors read these globals
	spdxtool_util_set_uri_root(g_client, "https://example.com/test");
	spdxtool_util_set_uri_separator_colon(g_client, false);
	spdxtool_util_set_spdx_version(g_client, "3.0.1");
	spdxtool_util_set_spdx_license(g_client, "CC0-1.0");
}

/*
 * ==============================================
 * core / software / simplelicensing constructors
 * ==============================================
 */

static void
test_oom_agent_new(void)
{
	spdxtool_core_agent_t *a;
	OOM_TEST_PTR(a, spdxtool_core_agent_new(g_client, "_:c1", "Agent Name"), spdxtool_core_agent_free(a));
}

static void
test_oom_creation_info_new(void)
{
	spdxtool_core_creation_info_t *c;
	// NULL time exercises the get_current_iso8601_time() strdup path too
	OOM_TEST_PTR(c, spdxtool_core_creation_info_new(g_client, "agentid", "_:c1", NULL),
		spdxtool_core_creation_info_free(c));
}

static void
test_oom_spdx_document_new(void)
{
	spdxtool_core_spdx_document_t *d;
	OOM_TEST_PTR(d, spdxtool_core_spdx_document_new(g_client, "docid", "_:c1", "agentid"),
		spdxtool_core_spdx_document_free(d));
}

static void
test_oom_sbom_new(void)
{
	spdxtool_software_sbom_t *s;
	OOM_TEST_PTR(s, spdxtool_software_sbom_new(g_client, "sbomid", "_:c1", "build"), spdxtool_software_sbom_free(s));
}

static void
test_oom_license_expression_new(void)
{
	spdxtool_simplelicensing_license_expression_t *e;
	OOM_TEST_PTR(e, spdxtool_simplelicensing_licenseExpression_new(g_client, "MIT"),
		spdxtool_simplelicensing_licenseExpression_free(e));
}

/*
 * ======================
 * serializer value model
 * ======================
 */

static void
test_oom_serialize_constructors(void)
{
	spdxtool_serialize_object_list_t *o;
	OOM_TEST_PTR(o, spdxtool_serialize_object_list_new(), spdxtool_serialize_object_list_free(o));

	spdxtool_serialize_array_t *a;
	OOM_TEST_PTR(a, spdxtool_serialize_array_new(), spdxtool_serialize_array_free(a));

	spdxtool_serialize_value_t *v;
	OOM_TEST_PTR(v, spdxtool_serialize_value_string("hello"), spdxtool_serialize_value_free(v));
	OOM_TEST_PTR(v, spdxtool_serialize_value_int(7), spdxtool_serialize_value_free(v));
	OOM_TEST_PTR(v, spdxtool_serialize_value_bool(true), spdxtool_serialize_value_free(v));
	OOM_TEST_PTR(v, spdxtool_serialize_value_null(), spdxtool_serialize_value_free(v));
}

/*
 * ============================================================
 * *_to_object serializers that depend only on their own struct
 * ============================================================
 */

static void
test_oom_to_object(void)
{
	spdxtool_serialize_value_t *v;

	spdxtool_core_agent_t *agent = spdxtool_core_agent_new(g_client, "_:c1", "Agent");
	TEST_ASSERT_NONNULL(agent);
	OOM_TEST_PTR(v, spdxtool_core_agent_to_object(g_client, agent),
		spdxtool_serialize_value_free(v));
	spdxtool_core_agent_free(agent);

	spdxtool_core_creation_info_t *ci =
		spdxtool_core_creation_info_new(g_client, "agentid", "_:c1", "2020-01-01T00:00:00Z");
	TEST_ASSERT_NONNULL(ci);
	OOM_TEST_PTR(v, spdxtool_core_creation_info_to_object(g_client, ci),
		spdxtool_serialize_value_free(v));
	spdxtool_core_creation_info_free(ci);

	spdxtool_simplelicensing_license_expression_t *le =
		spdxtool_simplelicensing_licenseExpression_new(g_client, "MIT");
	TEST_ASSERT_NONNULL(le);
	OOM_TEST_PTR(v, spdxtool_simplelicensing_licenseExpression_to_object(g_client, "_:c1", le),
		spdxtool_serialize_value_free(v));
	spdxtool_simplelicensing_licenseExpression_free(le);
}

int
main(int argc, const char **argv)
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	oom_setup();

	TEST_RUN(basename, test_oom_agent_new);
	TEST_RUN(basename, test_oom_creation_info_new);
	TEST_RUN(basename, test_oom_spdx_document_new);
	TEST_RUN(basename, test_oom_sbom_new);
	TEST_RUN(basename, test_oom_license_expression_new);
	TEST_RUN(basename, test_oom_serialize_constructors);
	TEST_RUN(basename, test_oom_to_object);

	pkgconf_client_free(g_client);
	return EXIT_SUCCESS;
}
