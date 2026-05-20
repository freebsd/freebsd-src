#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	modversion \
	variable \
	define_variable \
	global_variable

modversion_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"1.0.1 \n" \
		pkgconf --modversion pkg-config
}

variable_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"/test \n" \
		pkgconf --variable=prefix foo
}

define_variable_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"/test2 \n" \
		pkgconf --define-variable=prefix=/test2 --variable=prefix foo
}

global_variable_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"${selfdir}/lib1 \n"
		pkgconf --exists -foo
}

argv_parse_3_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-llib-1 -pthread /test/lib/lib2.so \n" \
		pkgconf --libs argv-parse-3
}

tilde_quoting_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L~ -ltilde \n" \
		pkgconf --libs tilde-quoting
	atf_check \
		-o inline:"-I~ \n" \
		pkgconf --cflags tilde-quoting
}

paren_quoting_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L\$(libdir) -ltilde \n" \
		pkgconf --libs paren-quoting
}
