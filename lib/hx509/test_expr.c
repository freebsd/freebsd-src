
#include "hx_locl.h"
#include <err.h>

struct foo {
    int val;
    char *str;
} foo[] = {
    { 0, "FALSE" },
    { 1, "TRUE" },
    { 0, "!TRUE" },
    { 0, "! TRUE" },
    { 0, "!\tTRUE" },
    { 0, "( FALSE AND FALSE )" },
    { 0, "( TRUE AND FALSE )" },
    { 1, "( TRUE AND TRUE )" },
    { 1, "( TRUE OR TRUE )" },
    { 1, "( TRUE OR FALSE )" },
    { 0, "( FALSE OR FALSE )" },
    { 1, "! ( FALSE OR FALSE )" },

    { 1, "\"foo\" TAILMATCH \"foo\"" },
    { 1, "\"foobar\" TAILMATCH \"bar\"" },
    { 0, "\"foobar\" TAILMATCH \"foo\"" },

    { 1, "\"foo\" == \"foo\"" },
    { 0, "\"foo\" == \"bar\"" },
    { 0, "\"foo\" != \"foo\"" },
    { 1, "\"foo\" != \"bar\"" },
    { 1, "%{variable} == \"foo\"" },
    { 0, "%{variable} == \"bar\"" },
    { 1, "%{context.variable} == \"foo\"" },
    { 0, "%{context.variable} == \"bar\"" },
    { 1, "\"foo\" IN ( \"bar\", \"foo\")" },
    { 0, "\"foo\" IN ( \"bar\", \"baz\")" },
    { 0, "\"bar\" IN %{context}" },
    { 1, "\"foo\" IN %{context}" },
    { 1, "\"variable\" IN %{context}" },

    { 1, "\"foo\" IN %{context} AND %{context.variable} == \"foo\"" }
};

int
main(int argc, char **argv)
{
    struct hx_expr *expr;
    hx509_context context;
    hx509_env env = NULL, env2 = NULL;
    int val, i, ret;

#if 0
    extern int yydebug;
    yydebug = 1;
#endif

    ret = hx509_context_init(&context);
    if (ret)
	errx(1, "hx509_context_init failed with %d", ret);

    hx509_env_add(context, &env, "variable", "foo");
    hx509_env_add(context, &env2, "variable", "foo");
    hx509_env_add_binding(context, &env, "context", env2);

    for (i = 0; i < sizeof(foo)/sizeof(foo[0]); i++) {

	expr = _hx509_expr_parse(foo[i].str);
	if (expr == NULL)
	    errx(1, "_hx509_expr_parse failed for %d: %s", i, foo[i].str);

	val = _hx509_expr_eval(context, env, expr);
	if (foo[i].val) {
	    if (val == 0)
		errx(1, "_hx509_expr_eval not true when it should: %d: %s",
		     i, foo[i].str);
	} else {
	    if (val)
		errx(1, "_hx509_expr_eval true when it should not: %d: %s",
		     i, foo[i].str);
	}

	_hx509_expr_free(expr);
    }

    hx509_env_free(&env);

    return 0;
}
