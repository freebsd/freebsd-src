#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	simple \
	foo \
	bar \
	baz \
	quux \
	moo \
	meow \
	indirect_dependency_node

simple_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
OUTPUT="provides-test-foo = 1.0.0
provides-test-bar > 1.1.0
provides-test-baz >= 1.1.0
provides-test-quux < 1.2.0
provides-test-moo <= 1.2.0
provides-test-meow != 1.3.0
provides = 1.2.3
"
	atf_check \
		-o inline:"${OUTPUT}" \
		pkgconf --print-provides provides
	atf_check \
		-o inline:"-lfoo\n" \
		pkgconf --libs provides-request-simple
	atf_check \
		-e ignore \
		-s exit:1 \
		pkgconf --no-provides --libs provides-request-simple
}

foo_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o ignore \
		pkgconf --libs provides-test-foo
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-foo = 1.0.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-foo >= 1.0.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-foo <= 1.0.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-foo != 1.0.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-foo > 1.0.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-foo < 1.0.0'
}

bar_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o ignore \
		pkgconf --libs provides-test-bar
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-bar = 1.1.1'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-bar >= 1.1.1'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-bar <= 1.1.1'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-bar != 1.1.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-bar != 1.1.1'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-bar > 1.1.1'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-bar <= 1.1.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-bar <= 1.2.0'
}

baz_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o ignore \
		pkgconf --libs provides-test-baz
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-baz = 1.1.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-baz >= 1.1.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-baz <= 1.1.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-baz != 1.1.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-baz != 1.0.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-baz > 1.1.1'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-baz > 1.1.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-baz < 1.1.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-baz < 1.2.0'
}

quux_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o ignore \
		pkgconf --libs provides-test-quux
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-quux = 1.1.9'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-quux >= 1.1.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-quux >= 1.1.9'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-quux >= 1.2.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-quux <= 1.2.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-quux <= 1.1.9'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-quux != 1.2.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-quux != 1.1.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-quux != 1.0.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-quux > 1.1.9'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-quux > 1.2.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-quux < 1.1.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-quux > 1.2.0'
}

moo_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o ignore \
		pkgconf --libs provides-test-moo
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-moo = 1.2.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-moo >= 1.1.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-moo >= 1.2.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-moo >= 1.2.1'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-moo <= 1.2.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-moo != 1.1.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-moo != 1.0.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-moo > 1.1.9'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-moo > 1.2.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-moo < 1.1.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-moo < 1.2.0'
}

meow_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o ignore \
		pkgconf --libs provides-test-meow
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-meow = 1.3.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-meow != 1.3.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-meow > 1.2.9'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-meow < 1.3.1'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-meow < 1.3.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-meow > 1.3.0'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-meow >= 1.3.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-meow >= 1.3.1'
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --libs 'provides-test-meow <= 1.3.0'
	atf_check \
		-o ignore \
		pkgconf --libs 'provides-test-meow < 1.2.9'
}

indirect_dependency_node_body()
{
	atf_check \
		-o inline:'1.2.3\n' \
		pkgconf --with-path="${selfdir}/lib1" --modversion 'provides-test-meow'
	atf_check \
		-s exit:1 \
		-e ignore \
		pkgconf --with-path="${selfdir}/lib1" --modversion 'provides-test-meow = 1.3.0'
}
