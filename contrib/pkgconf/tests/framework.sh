#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	libs

libs_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-F/test/lib -framework framework-1\n" \
		pkgconf --libs framework-1
	atf_check \
		-o inline:"-F/test/lib -framework framework-2 -framework framework-1\n" \
		pkgconf --libs framework-2
	atf_check \
		-o inline:"-F/test/lib -framework framework-2 -framework framework-1\n" \
		pkgconf --libs framework-1 framework-2
}
