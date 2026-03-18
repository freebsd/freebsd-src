#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	libs \
	libs_cflags \
	libs_static \
	libs_static_pure \
	cflags_libs_private \
	static_cflags \
	foo_metapackage_3 \
	libs_static2

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

static_cflags_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-fPIC -I/test/include/foo -DFOO_STATIC\n" \
		pkgconf --static --cflags baz
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

cflags_libs_private_body()
{
	atf_check \
		-o inline:"\n" \
		pkgconf --with-path="${selfdir}/lib1" --libs cflags-libs-private-a

	atf_check \
		-o inline:"-lc\n" \
		pkgconf --with-path="${selfdir}/lib1" --static --libs cflags-libs-private-a
}
