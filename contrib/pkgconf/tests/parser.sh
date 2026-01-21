#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	fragment_escaping_1 \
	fragment_escaping_2 \
	fragment_escaping_3 \
	fragment_quoting \
	fragment_quoting_2 \
	fragment_quoting_3 \
	fragment_quoting_5 \
	fragment_quoting_7 \
	msvc_fragment_quoting \
	msvc_fragment_render_cflags \
	version_with_whitespace \
	version_with_whitespace_2 \
	version_with_whitespace_diagnostic \
	fragment_tree

fragment_quoting_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-fPIC -I/test/include/foo -DQUOTED=\\\"/test/share/doc\\\"\n" \
		pkgconf --cflags fragment-quoting
}

fragment_quoting_2_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-fPIC -I/test/include/foo -DQUOTED=/test/share/doc\n" \
		pkgconf --cflags fragment-quoting-2
}

fragment_quoting_3_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-fPIC -I/test/include/foo -DQUOTED=\\\"/test/share/doc\\\"\n" \
		pkgconf --cflags fragment-quoting-3
}

fragment_quoting_5_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-fPIC -I/test/include/foo -DQUOTED=/test/share/doc\n" \
		pkgconf --cflags fragment-quoting-5
}

fragment_quoting_7_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-Dhello=10 -Dworld=+32 -DDEFINED_FROM_PKG_CONFIG=hello\\ world\n" \
		pkgconf --cflags fragment-quoting-7
}

fragment_escaping_1_body()
{
	atf_check \
		-o inline:"-IC:\\\\\\\\D\\ E\n" \
		pkgconf --with-path="${selfdir}/lib1" --cflags fragment-escaping-1
}

fragment_escaping_2_body()
{
	atf_check \
		-o inline:"-IC:\\\\\\\\D\\ E\n" \
		pkgconf --with-path="${selfdir}/lib1" --cflags fragment-escaping-2
}

fragment_escaping_3_body()
{
	atf_check \
		-o inline:"-IC:\\\\\\\\D\\ E\n" \
		pkgconf --with-path="${selfdir}/lib1" --cflags fragment-escaping-3
}

fragment_quoting_7a_body()
{
	set -x

	test_cflags=$(pkgconf --with-path=${selfdir}/lib1 --cflags fragment-quoting-7)
	echo $test_cflags
#	test_cflags='-Dhello=10 -Dworld=+32 -DDEFINED_FROM_PKG_CONFIG=hello\\ world'

	cat > test.c <<- __TESTCASE_END__
		int main(int argc, char *argv[]) { return DEFINED_FROM_PKG_CONFIG; }
	__TESTCASE_END__
	cc -o test-fragment-quoting-7 ${test_cflags} ./test.c
	atf_check -e 42 ./test-fragment-quoting-7
	rm -f test.c test-fragment-quoting-7

	set +x
}

msvc_fragment_quoting_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:'/libpath:"C:\D E" E.lib \n' \
		pkgconf --libs --msvc-syntax fragment-escaping-1
}

msvc_fragment_render_cflags_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:'/I/test/include/foo /DFOO_STATIC \n' \
		pkgconf --cflags --static --msvc-syntax foo
}

version_with_whitespace_body()
{
	atf_check \
		-o inline:'3.922\n' \
		pkgconf --with-path="${selfdir}/lib1" --modversion malformed-version
}

version_with_whitespace_2_body()
{
	atf_check \
		-o inline:'malformed-version = 3.922\n' \
		pkgconf --with-path="${selfdir}/lib1" --print-provides malformed-version
}

version_with_whitespace_diagnostic_body()
{
	atf_check \
		-o match:warning \
		pkgconf --with-path="${selfdir}/lib1" --validate malformed-version
}

fragment_tree_body()
{
	atf_check \
		-o inline:"'-Wl,--start-group' [untyped]
  '-la' [type l]
  '-lb' [type l]
  '-Wl,--end-group' [untyped]

'-nodefaultlibs' [untyped]
'-Wl,--start-group' [untyped]
  '-la' [type l]
  '-lgcc' [type l]
  '-Wl,--end-group' [untyped]

'-Wl,--gc-sections' [untyped]

" \
		pkgconf --with-path="${selfdir}/lib1" --fragment-tree fragment-groups-2
}

fragment_whitespace_body()
{
	atf_check \
		-o inline:"'-I/includedir' [type I]\n\n" \
			pkgconf --with-path="${selfdir}/lib1" --fragment-tree flag-whitespace-2

	atf_check \
		-o inline:"'-I/includedir' [type I]\n\n" \
			pkgconf --with-path="${selfdir}/lib1" --fragment-tree flag-whitespace
}
