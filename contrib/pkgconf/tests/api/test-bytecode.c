/*
 * test-bytecode.c
 * Tests for the public libpkgconf bytecode API:
 * pkgconf_bytecode_compile and pkgconf_bytecode_eval_str.
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2025 pkgconf authors (see AUTHORS).
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
#include "test-api.h"

/*
 * Build a variable list with the given key/value pairs. Caller frees
 * with pkgconf_variable_list_free().
 *
 * The key/value pairs are stored by compiling `value` as bytecode and
 * stashing the result on a pkgconf_variable_t inside the list, which
 * is how the parser builds variable scopes for real .pc files.
 */
static void
seed_variable(pkgconf_list_t *vars, const char *key, const char *value)
{
	pkgconf_variable_t *v = pkgconf_variable_get_or_create(vars, key);
	TEST_ASSERT_NONNULL(v);

	pkgconf_buffer_reset(&v->bcbuf);
	pkgconf_bytecode_compile(&v->bcbuf, value);
	pkgconf_bytecode_from_buffer(&v->bc, &v->bcbuf);
}

static void
test_emit_text_and_eval(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	pkgconf_buffer_t bcbuf = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_t bc;
	bool saw_sysroot = false;

	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_text(&bcbuf, "plain", 5));
	pkgconf_bytecode_from_buffer(&bc, &bcbuf);

	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;
	TEST_ASSERT_TRUE(pkgconf_bytecode_eval(client, &vars, &bc, &out, &saw_sysroot));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str_or_empty(&out), "plain");

	pkgconf_buffer_finalize(&out);
	pkgconf_buffer_finalize(&bcbuf);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_emit_guards(void)
{
	pkgconf_buffer_t bcbuf = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_text(&bcbuf, NULL, 5));
	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_text(&bcbuf, "x", 0));
	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_var(&bcbuf, NULL, 3));
	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_var(&bcbuf, "x", 0));

	TEST_ASSERT_EQ(pkgconf_buffer_len(&bcbuf), 0);

	pkgconf_buffer_finalize(&bcbuf);
}

static void
test_emit_var_and_eval(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	pkgconf_buffer_t bcbuf = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_t bc;
	bool saw_sysroot = false;

	seed_variable(&vars, "name", "world");

	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_text(&bcbuf, "hello ", 6));
	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_var(&bcbuf, "name", 4));
	pkgconf_bytecode_from_buffer(&bc, &bcbuf);

	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;
	TEST_ASSERT_TRUE(pkgconf_bytecode_eval(client, &vars, &bc, &out, &saw_sysroot));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str_or_empty(&out), "hello world");

	pkgconf_buffer_finalize(&out);
	pkgconf_buffer_finalize(&bcbuf);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_emit_sysroot_and_eval(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	pkgconf_buffer_t bcbuf = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_t bc;
	bool saw_sysroot = false;

	pkgconf_client_set_sysroot_dir(client, "/sysroot");

	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_sysroot(&bcbuf));
	TEST_ASSERT_TRUE(pkgconf_bytecode_emit_text(&bcbuf, "/usr/include", 12));
	pkgconf_bytecode_from_buffer(&bc, &bcbuf);

	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;
	TEST_ASSERT_TRUE(pkgconf_bytecode_eval(client, &vars, &bc, &out, &saw_sysroot));
	TEST_ASSERT_TRUE(saw_sysroot);
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str_or_empty(&out), "/sysroot/usr/include");

	pkgconf_buffer_finalize(&out);
	pkgconf_buffer_finalize(&bcbuf);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_eval_null_args(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_t bc = { NULL, 0 };

	// NULL client, bc, or out all return false.
	TEST_ASSERT_FALSE(pkgconf_bytecode_eval(NULL, &vars, &bc, &out, NULL));
	TEST_ASSERT_FALSE(pkgconf_bytecode_eval(client, &vars, NULL, &out, NULL));
	TEST_ASSERT_FALSE(pkgconf_bytecode_eval(client, &vars, &bc, NULL, NULL));

	pkgconf_buffer_finalize(&out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_compile_null_args(void)
{
	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_FALSE(pkgconf_bytecode_compile(NULL, "x"));
	TEST_ASSERT_FALSE(pkgconf_bytecode_compile(&out, NULL));

	pkgconf_buffer_finalize(&out);
}

static void
test_dollar_escape(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// $$ collapses to a literal $; the following text is NOT treated as a variable reference
	char *out = pkgconf_bytecode_eval_str(client, &vars, "price: $$5 and $${notavar}", &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "price: $5 and ${notavar}");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_malformed_unclosed(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// A ${ with no closing } is emitted as literal text
	char *out = pkgconf_bytecode_eval_str(client, &vars, "broken ${unclosed", &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "broken ${unclosed");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_empty_braces(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// ${} has a zero-length name; it's emitted as literal text rather than treated as a variable
	char *out = pkgconf_bytecode_eval_str(client, &vars, "empty ${} here", &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "empty ${} here");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_compile_time_sysroot(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	pkgconf_client_set_sysroot_dir(client, "/sysroot");

	// ${pc_sysrootdir} is special-cased at compile time into an OP_SYSROOT op rather than a variable lookup
	char *out = pkgconf_bytecode_eval_str(client, &vars, "${pc_sysrootdir}/usr/lib", &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_TRUE(saw_sysroot);
	TEST_ASSERT_STRCMP_EQ(out, "/sysroot/usr/lib");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_sysroot_dot_disables(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// A sysroot of "." disables sysroot rewriting: ${pc_sysrootdir} expands to empty
	pkgconf_client_set_sysroot_dir(client, ".");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "${pc_sysrootdir}/usr/lib", &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "/usr/lib");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_sysroot_root_disables(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// A sysroot of "/" (the root dir) disables rewriting.
	pkgconf_client_set_sysroot_dir(client, "/");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "${pc_sysrootdir}/usr/lib", &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "/usr/lib");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_sysroot_trailing_slash_trimmed(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// Trailing slashes on the sysroot are normalized away, so the result doesn't get a doubled slash
	pkgconf_client_set_sysroot_dir(client, "/sysroot///");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "${pc_sysrootdir}/usr/lib", &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_TRUE(saw_sysroot);
	TEST_ASSERT_STRCMP_EQ(out, "/sysroot/usr/lib");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_sysroot_normalizes_to_root_disables(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	/* A sysroot of "//" survives the bare-"/" early check but normalizes down to "/" after
	 * trailing-slash trimming, which then disables rewriting (empty sysroot) */
	pkgconf_client_set_sysroot_dir(client, "//");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "${pc_sysrootdir}/usr/lib", &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "/usr/lib");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_circular_reference(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// A variable that references itself must not infinite-loop: the `expanding` guard breaks the cycle
	seed_variable(&vars, "loop", "${loop}");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "${loop}", &saw_sysroot);

	/* The self-reference is broken; behavior is "expands to nothing further"
	 * We mostly care that it returns rather than hanging or crashing */
	if (out != NULL)
		free(out);

	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_references_var(void)
{
	pkgconf_buffer_t bcbuf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_bytecode_compile(&bcbuf, "-I${includedir} -L${libdir}");

	TEST_ASSERT_TRUE(pkgconf_bytecode_references_var(&bcbuf, "includedir"));
	TEST_ASSERT_TRUE(pkgconf_bytecode_references_var(&bcbuf, "libdir"));
	TEST_ASSERT_FALSE(pkgconf_bytecode_references_var(&bcbuf, "prefix"));

	TEST_ASSERT_FALSE(pkgconf_bytecode_references_var(NULL, "x"));
	TEST_ASSERT_FALSE(pkgconf_bytecode_references_var(&bcbuf, NULL));

	pkgconf_buffer_finalize(&bcbuf);
}

static void
test_references_var_text_only(void)
{
	pkgconf_buffer_t bcbuf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_bytecode_compile(&bcbuf, "just plain text");

	TEST_ASSERT_FALSE(pkgconf_bytecode_references_var(&bcbuf, "anything"));

	pkgconf_buffer_finalize(&bcbuf);
}

static void
test_rewrite_selfrefs(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	pkgconf_buffer_t prev = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_compile(&prev, "old");

	pkgconf_buffer_t rhs = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_compile(&rhs, "${foo} new");

	pkgconf_buffer_t rewritten = PKGCONF_BUFFER_INITIALIZER;
	TEST_ASSERT_TRUE(pkgconf_bytecode_rewrite_selfrefs(&rewritten, &rhs, "foo", &prev));

	pkgconf_bytecode_t bc;
	pkgconf_bytecode_from_buffer(&bc, &rewritten);

	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;
	TEST_ASSERT_TRUE(pkgconf_bytecode_eval(client, &vars, &bc, &out, &saw_sysroot));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str_or_empty(&out), "old new");

	pkgconf_buffer_finalize(&out);
	pkgconf_buffer_finalize(&rewritten);
	pkgconf_buffer_finalize(&rhs);
	pkgconf_buffer_finalize(&prev);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_rewrite_selfrefs_no_match(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	pkgconf_buffer_t prev = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_compile(&prev, "PREV");

	pkgconf_buffer_t rhs = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_compile(&rhs, "${other} tail");

	pkgconf_buffer_t rewritten = PKGCONF_BUFFER_INITIALIZER;
	TEST_ASSERT_TRUE(pkgconf_bytecode_rewrite_selfrefs(&rewritten, &rhs, "foo", &prev));

	// ${other} survives; seed it so eval can resolve it
	seed_variable(&vars, "other", "OTHER");

	pkgconf_bytecode_t bc;
	pkgconf_bytecode_from_buffer(&bc, &rewritten);

	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;
	TEST_ASSERT_TRUE(pkgconf_bytecode_eval(client, &vars, &bc, &out, &saw_sysroot));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str_or_empty(&out), "OTHER tail");

	pkgconf_buffer_finalize(&out);
	pkgconf_buffer_finalize(&rewritten);
	pkgconf_buffer_finalize(&rhs);
	pkgconf_buffer_finalize(&prev);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_eval_plain_text(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// A string with no variable references should round-trip unchanged.
	char *out = pkgconf_bytecode_eval_str(client, &vars, "plain text value", &saw_sysroot);

	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "plain text value");
	TEST_ASSERT_FALSE(saw_sysroot);

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_eval_variable_substitution(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	seed_variable(&vars, "prefix", "/opt/foo");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "${prefix}/lib", &saw_sysroot);

	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "/opt/foo/lib");
	TEST_ASSERT_FALSE(saw_sysroot);

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_eval_nested_variables(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// Recursive expansion: libdir references prefix.
	seed_variable(&vars, "prefix", "/usr/local");
	seed_variable(&vars, "libdir", "${prefix}/lib");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "-L${libdir}", &saw_sysroot);

	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "-L/usr/local/lib");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_eval_undefined_variable(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// Referencing an undefined variable should produce empty substitution, not failure
	char *out = pkgconf_bytecode_eval_str(client, &vars, "prefix=${nonexistent}/end", &saw_sysroot);

	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "prefix=/end");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_eval_multiple_variables(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	seed_variable(&vars, "prefix", "/usr");
	seed_variable(&vars, "exec_prefix", "/usr/local");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "-I${prefix}/include -L${exec_prefix}/lib", &saw_sysroot);

	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "-I/usr/include -L/usr/local/lib");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_eval_empty_input(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	char *out = pkgconf_bytecode_eval_str(client, &vars, "", &saw_sysroot);

	// An empty input may evaluate to either NULL or an empty string
	if (out != NULL)
	{
		TEST_ASSERT_STRCMP_EQ(out, "");
		free(out);
	}

	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_eval_sysroot_detection(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	pkgconf_client_set_sysroot_dir(client, "/sysroot");

	seed_variable(&vars, "pc_sysrootdir", "/sysroot");
	seed_variable(&vars, "includedir", "${pc_sysrootdir}/usr/include");

	char *out = pkgconf_bytecode_eval_str(client, &vars, "${includedir}", &saw_sysroot);

	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_TRUE(saw_sysroot);

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_compile_produces_nonempty_buffer(void)
{
	pkgconf_buffer_t bc = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_bytecode_compile(&bc, "hello ${world}");

	// The compiled bytecode buffer should be non-empty
	TEST_ASSERT_NE(pkgconf_buffer_len(&bc), 0);

	pkgconf_buffer_finalize(&bc);
}

static void
test_compile_eval_roundtrip(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// Compile directly, then evaluate via the bc field on a variable
	seed_variable(&vars, "name", "world");

	pkgconf_variable_t *v = pkgconf_variable_get_or_create(&vars, "greeting");
	TEST_ASSERT_NONNULL(v);
	pkgconf_buffer_reset(&v->bcbuf);
	pkgconf_bytecode_compile(&v->bcbuf, "hello ${name}");
	pkgconf_bytecode_from_buffer(&v->bc, &v->bcbuf);

	char *out = pkgconf_variable_eval_str(client, &vars, v, &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "hello world");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_eval_plain_text);
	TEST_RUN(basename, test_eval_empty_input);
	TEST_RUN(basename, test_eval_variable_substitution);
	TEST_RUN(basename, test_eval_nested_variables);
	TEST_RUN(basename, test_eval_multiple_variables);
	TEST_RUN(basename, test_eval_undefined_variable);
	TEST_RUN(basename, test_eval_sysroot_detection);
	TEST_RUN(basename, test_eval_null_args);

	TEST_RUN(basename, test_emit_guards);
	TEST_RUN(basename, test_emit_text_and_eval);
	TEST_RUN(basename, test_emit_var_and_eval);
	TEST_RUN(basename, test_emit_sysroot_and_eval);

	TEST_RUN(basename, test_dollar_escape);
	TEST_RUN(basename, test_malformed_unclosed);
	TEST_RUN(basename, test_empty_braces);

	TEST_RUN(basename, test_circular_reference);
	TEST_RUN(basename, test_references_var);
	TEST_RUN(basename, test_references_var_text_only);

	TEST_RUN(basename, test_compile_time_sysroot);
	TEST_RUN(basename, test_sysroot_dot_disables);
	TEST_RUN(basename, test_sysroot_root_disables);
	TEST_RUN(basename, test_sysroot_trailing_slash_trimmed);
	TEST_RUN(basename, test_sysroot_normalizes_to_root_disables);

	TEST_RUN(basename, test_rewrite_selfrefs);
	TEST_RUN(basename, test_rewrite_selfrefs_no_match);

	TEST_RUN(basename, test_compile_eval_roundtrip);
	TEST_RUN(basename, test_compile_produces_nonempty_buffer);
	TEST_RUN(basename, test_compile_null_args);

	return EXIT_SUCCESS;
}
