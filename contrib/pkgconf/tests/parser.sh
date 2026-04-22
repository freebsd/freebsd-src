#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	comments \
	comments_in_fields \
	dos \
	no_trailing_newline \
	argv_parse \
	bad_option \
	argv_parse_3 \
	tilde_quoting \
	paren_quoting \
	multiline_field \
	multiline_bogus_header \
	escaped_backslash \
	flag_order_1 \
	flag_order_2 \
	flag_order_3 \
	flag_order_4 \
	quoted \
	variable_whitespace \
	fragment_escaping_1 \
	fragment_escaping_2 \
	fragment_escaping_3 \
	fragment_quoting \
	fragment_quoting_2 \
	fragment_quoting_3 \
	fragment_quoting_5 \
	fragment_quoting_7 \
	fragment_comment \
	msvc_fragment_quoting \
	msvc_fragment_render_cflags \
	tuple_dequote \
	version_with_whitespace \
	version_with_whitespace_2 \
	version_with_whitespace_diagnostic \
	fragment_groups \
	fragment_groups_composite \
	fragment_tree \
	truncated \
	c_comment

comments_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-lfoo\n" \
		pkgconf --libs comments
}

comments_in_fields_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-lfoo\n" \
		pkgconf --libs comments-in-fields
}

dos_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/lib/dos-lineendings -ldos-lineendings\n" \
		pkgconf --libs dos-lineendings
}

no_trailing_newline_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-I/test/include/no-trailing-newline\n" \
		pkgconf --cflags no-trailing-newline
}

argv_parse_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-llib-3 -llib-1 -llib-2 -lpthread\n" \
		pkgconf --libs argv-parse
}

bad_option_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-e ignore \
		-s eq:1 \
		pkgconf --exists -foo
}

argv_parse_3_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-llib-1 -pthread /test/lib/lib2.so\n" \
		pkgconf --libs argv-parse-3
}

tilde_quoting_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L~ -ltilde\n" \
		pkgconf --libs tilde-quoting
	atf_check \
		-o inline:"-I~\n" \
		pkgconf --cflags tilde-quoting
}

paren_quoting_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L\$(libdir) -ltilde\n" \
		pkgconf --libs paren-quoting
}

multiline_field_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-e ignore \
		-o match:"multiline description" \
		pkgconf --list-all
}

multiline_bogus_header_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-s eq:0 \
		pkgconf --exists multiline-bogus
}

escaped_backslash_body()
{
	atf_check \
		-e ignore \
		-o inline:"-IC:\\\\\\\\A\n" \
		pkgconf --with-path=${selfdir}/lib1 --cflags escaped-backslash
}

quoted_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-DQUOTED=\\\"bla\\\" -DA=\\\"escaped\\ string\\\'\\ literal\\\" -DB=\\\\\\1\$ -DC=bla\n" \
		pkgconf --cflags quotes
}

flag_order_1_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/lib -Bdynamic -lfoo -Bstatic -lbar\n" \
		pkgconf --libs flag-order-1
}

flag_order_2_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/lib -Bdynamic -lfoo -Bstatic -lbar -lfoo\n" \
		pkgconf --libs flag-order-1 foo
}

flag_order_3_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/lib -Wl,--start-group -lfoo -lbar -Wl,--end-group\n" \
		pkgconf --libs flag-order-3
}

flag_order_4_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/lib -Wl,--start-group -lfoo -lbar -Wl,--end-group -lfoo\n" \
		pkgconf --libs flag-order-3 foo
}

variable_whitespace_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-I/test/include\n" \
		pkgconf --cflags variable-whitespace
}

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


fragment_comment_body()
{
	atf_check \
		-o inline:'kuku=\#ttt\n' \
		pkgconf --with-path="${selfdir}/lib1" --cflags fragment-comment
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

tuple_dequote_body()
{
	atf_check \
		-o inline:'-L/test/lib -lfoo\n' \
		pkgconf --with-path="${selfdir}/lib1" --libs tuple-quoting
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

fragment_groups_body()
{
	atf_check \
		-o inline:'-Wl,--start-group -la -lb -Wl,--end-group -nodefaultlibs -Wl,--start-group -la -lgcc -Wl,--end-group -Wl,--gc-sections\n' \
		pkgconf --with-path="${selfdir}/lib1" --libs fragment-groups
}

fragment_groups_composite_body()
{
	atf_check \
		-o inline:'-Wl,--start-group -la -lb -Wl,--end-group -nodefaultlibs -Wl,--start-group -la -lgcc -Wl,--end-group -Wl,--gc-sections\n' \
		pkgconf --with-path="${selfdir}/lib1" --libs fragment-groups-2
}

truncated_body()
{
	atf_check \
		-o match:warning -s exit:1 \
		pkgconf --with-path="${selfdir}/lib1" --validate truncated
}

c_comment_body()
{
	atf_check \
		-o match:warning \
		pkgconf --with-path="${selfdir}/lib1" --validate c-comment
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

