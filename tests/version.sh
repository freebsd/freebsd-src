#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	atleast \
	exact \
	max

atleast_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		pkgconf --atleast-version 1.0 foo
	atf_check \
		-s exit:1 \
		pkgconf --atleast-version 2.0 foo
}

exact_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-s exit:1 \
		pkgconf --exact-version 1.0 foo
	atf_check \
		pkgconf --exact-version 1.2.3 foo
}

max_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-s exit:1 \
		pkgconf --max-version 1.0 foo
	atf_check \
		pkgconf --max-version 2.0 foo
}
