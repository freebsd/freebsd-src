#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	libs \
	libs_cflags \
	libs_static \
	libs_static_pure \
	cflags_libs_private \
	argv_parse2 \
	static_cflags \
	private_duplication \
	private_duplication_digraph \
	foo_bar \
	bar_foo \
	foo_metapackage_3 \
	libs_static2 \
	missing \
	requires_internal \
	requires_internal_missing \
	requires_internal_collision \
	orphaned_requires_private

libs_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/lib -lbar -lfoo\n" \
		pkgconf --libs bar
}

libs_cflags_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-fPIC -I/test/include/foo -L/test/lib -lbaz\n" \
		pkgconf --libs --cflags baz
}

libs_static_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/lib -lbaz -L/test/lib -lzee -lfoo\n" \
		pkgconf --static --libs baz
}

libs_static_pure_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/lib -lbaz -lfoo\n" \
		pkgconf --static --pure --libs baz
}

argv_parse2_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-llib-1 -pthread /test/lib/lib2.so\n" \
		pkgconf --static --libs argv-parse-2
}

static_cflags_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-fPIC -I/test/include/foo -DFOO_STATIC\n" \
		pkgconf --static --cflags baz
}

private_duplication_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-lprivate -lbaz -lzee -lbar -lfoo\n" \
		pkgconf --static --libs-only-l private-libs-duplication
}

private_duplication_digraph_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o 'match:"user:request" -> "private-libs-duplication"' \
		-o 'match:"private-libs-duplication" -> "bar"' \
		-o 'match:"private-libs-duplication" -> "baz"' \
		-o 'match:"bar" -> "foo"' \
		-o 'match:"baz" -> "foo"' \
		pkgconf --static --libs-only-l private-libs-duplication --digraph
}

bar_foo_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-lbar -lfoo\n" \
		pkgconf --static --libs-only-l bar foo
}

foo_bar_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-lbar -lfoo\n" \
		pkgconf --static --libs-only-l foo bar
}

foo_metapackage_3_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-lbar -lfoo\n" \
		pkgconf --static --libs-only-l foo metapackage-3
}

libs_static2_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-lbar -lbar-private -L/test/lib -lfoo\n" \
		pkgconf --static --libs static-libs
}

missing_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --cflags missing-require
}

requires_internal_body()
{
	atf_check \
		-o inline:"-lbar -lbar-private -L/test/lib -lfoo\n" \
		pkgconf --with-path="${selfdir}/lib1" --static --libs requires-internal
}

requires_internal_missing_body()
{
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --with-path="${selfdir}/lib1" --static --libs requires-internal-missing
}

requires_internal_collision_body()
{
	atf_check \
		-o inline:"-I/test/local/include/foo\n" \
		pkgconf --with-path="${selfdir}/lib1" --cflags requires-internal-collision
}

orphaned_requires_private_body()
{
	atf_check \
		-s exit:1 \
		-e ignore \
		-o ignore \
		pkgconf --with-path="${selfdir}/lib1" --cflags --libs orphaned-requires-private
}

cflags_libs_private_body()
{
	atf_check \
		-o inline:"\n" \
		pkgconf --with-path="${selfdir}/lib1" --libs cflags-libs-private-a

	atf_check \
		-o inline:"-lc\n" \
		pkgconf --with-path="${selfdir}/lib1" --static --libs cflags-libs-private-a
}
