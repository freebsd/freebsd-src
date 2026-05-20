#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	case_sensitivity \
	depgraph_break_1 \
	depgraph_break_2 \
	depgraph_break_3 \
	define_variable \
	define_variable_override \
	variable \
	keep_system_libs \
	libs \
	libs_only \
	libs_never_mergeback \
	cflags_only \
	cflags_never_mergeback \
	incomplete_libs \
	incomplete_cflags \
	isystem_munge_order \
	isystem_munge_sysroot \
	idirafter_munge_order \
	idirafter_munge_sysroot \
	idirafter_ordering \
	modversion_common_prefix \
	modversion_fullpath \
	modversion_provides \
	modversion_uninstalled \
	modversion_one_word_expression \
	modversion_two_word_expression \
	modversion_three_word_expression \
	modversion_one_word_expression_no_space \
	modversion_one_word_expression_no_space_zero \
	pcpath \
	virtual_variable \
	fragment_collision \
	malformed_1 \
	malformed_quoting \
	explicit_sysroot \
	empty_tuple \
	solver_requires_private_debounce \
	billion_laughs \
	define_prefix_child_prefix_1 \
	define_prefix_child_prefix_1_env

#	sysroot_munge \

case_sensitivity_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"3\n" \
		pkgconf --variable=foo case-sensitivity
	atf_check \
		-o inline:"4\n" \
		pkgconf --variable=Foo case-sensitivity
}

depgraph_break_1_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check -s exit:1 -e ignore \
		pkgconf --exists --print-errors 'foo > 0.6.0 foo < 0.8.0'
}

depgraph_break_2_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check -s exit:1 -e ignore \
		pkgconf --exists --print-errors 'nonexisting foo <= 3'
}

depgraph_break_3_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check -s exit:1 -e ignore \
		pkgconf --exists --print-errors 'depgraph-break'
}

define_variable_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check -o inline:"\\\${libdir}/typelibdir\n" \
		pkgconf --variable=typelibdir --define-variable='libdir=\${libdir}' typelibdir
}

define_variable_override_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check -o inline:"/test\n" \
		pkgconf --variable=prefix --define-variable='prefix=/test' typelibdir
}

variable_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"/test/include\n" \
		pkgconf --variable=includedir foo
}

keep_system_libs_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	eval export "$LIBRARY_PATH_ENV"="/test/local/lib"
	atf_check \
		-o inline:"\n" \
		pkgconf --libs-only-L cflags-libs-only

	atf_check \
		-o inline:"-L/test/local/lib\n" \
		pkgconf --libs-only-L --keep-system-libs cflags-libs-only
}

libs_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/local/lib -lfoo\n" \
		pkgconf --libs cflags-libs-only
}

libs_only_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/local/lib -lfoo\n" \
		pkgconf --libs-only-L --libs-only-l cflags-libs-only
}

libs_never_mergeback_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-L/test/bar/lib -lfoo1\n" \
		pkgconf --libs prefix-foo1
	atf_check \
		-o inline:"-L/test/bar/lib -lfoo1 -lfoo2\n" \
		pkgconf --libs prefix-foo1 prefix-foo2
}

cflags_only_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-I/test/local/include/foo\n" \
		pkgconf --cflags-only-I --cflags-only-other cflags-libs-only
}

cflags_never_mergeback_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-I/test/bar/include/foo -DBAR -fPIC -DFOO\n" \
		pkgconf --cflags prefix-foo1 prefix-foo2
}

incomplete_libs_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"\n" \
		pkgconf --libs incomplete
}

incomplete_cflags_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"\n" \
		pkgconf --cflags incomplete
}

isystem_munge_order_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-isystem /opt/bad/include -isystem /opt/bad2/include\n" \
		pkgconf --cflags isystem
}

isystem_munge_sysroot_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1" PKG_CONFIG_SYSROOT_DIR="${selfdir}"
	atf_check \
		-o match:"-isystem ${selfdir}/opt/bad/include" \
		pkgconf --cflags isystem
}

idirafter_munge_order_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-idirafter /opt/bad/include -idirafter /opt/bad2/include\n" \
		pkgconf --cflags idirafter
}

idirafter_munge_sysroot_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1" PKG_CONFIG_SYSROOT_DIR="${selfdir}"
	atf_check \
		-o match:"-idirafter ${selfdir}/opt/bad/include" \
		pkgconf --cflags idirafter
}

idirafter_ordering_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1"
	atf_check \
		-o inline:"-I/opt/bad/include1 -idirafter -I/opt/bad/include2 -I/opt/bad/include3\n" \
		pkgconf --cflags idirafter-ordering
}

pcpath_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib2"
	atf_check \
		-o inline:"-fPIC -I/test/include/foo\n" \
		pkgconf --cflags ${selfdir}/lib3/bar.pc
}

sysroot_munge_body()
{
	sed "s|/sysroot/|${selfdir}/|g" ${selfdir}/lib1/sysroot-dir.pc > ${selfdir}/lib1/sysroot-dir-selfdir.pc
	export PKG_CONFIG_PATH="${selfdir}/lib1" PKG_CONFIG_SYSROOT_DIR="${selfdir}"
	atf_check \
		-o inline:"-L${selfdir}/lib -lfoo\n" \
		pkgconf --libs sysroot-dir-selfdir
}

virtual_variable_body()
{
	atf_check -s exit:0 \
		pkgconf --exists pkg-config
	atf_check -s exit:0 \
		pkgconf --exists pkgconf

	atf_check -o inline:"${pcpath}\n" \
		pkgconf --variable=pc_path pkg-config
	atf_check -o inline:"${pcpath}\n" \
		pkgconf --variable=pc_path pkgconf
}

fragment_collision_body()
{
	atf_check -o inline:"-D_BAZ -D_BAR -D_FOO -D_THREAD_SAFE -pthread\n" \
		pkgconf --with-path="${selfdir}/lib1" --cflags fragment-collision
}

malformed_1_body()
{
	atf_check -s exit:1 -o ignore \
		pkgconf --validate --with-path="${selfdir}/lib1" malformed-1
}

malformed_quoting_body()
{
	atf_check -s exit:0 -o ignore \
		pkgconf --validate --with-path="${selfdir}/lib1" malformed-quoting
}

explicit_sysroot_body()
{
	export PKG_CONFIG_SYSROOT_DIR=${selfdir}
	atf_check -o inline:"${selfdir}/usr/share/test\n" \
		pkgconf --with-path="${selfdir}/lib1" --variable=pkgdatadir explicit-sysroot
}

empty_tuple_body()
{
	atf_check -o inline:"\n" \
		pkgconf --with-path="${selfdir}/lib1" --cflags empty-tuple
}

solver_requires_private_debounce_body()
{
	atf_check -o inline:"-I/metapackage-1 -I/metapackage-2 -lmetapackage-1 -lmetapackage-2\n" \
		pkgconf --with-path="${selfdir}/lib1" --cflags --libs metapackage
}

billion_laughs_body()
{
	atf_check -o inline:"warning: truncating very long variable to 64KB\nwarning: truncating very long variable to 64KB\nwarning: truncating very long variable to 64KB\nwarning: truncating very long variable to 64KB\nwarning: truncating very long variable to 64KB\n" \
		pkgconf --with-path="${selfdir}/lib1" --validate billion-laughs
}

modversion_common_prefix_body()
{
	atf_check -o inline:"foo: 1.2.3\nfoobar: 3.2.1\n" \
		pkgconf --with-path="${selfdir}/lib1" --modversion --verbose foo foobar
}

modversion_fullpath_body()
{
	atf_check -o inline:"1.2.3\n" \
		pkgconf --modversion "${selfdir}/lib1/foo.pc"
}

modversion_provides_body()
{
	atf_check -o inline:"1.2.3\n" \
		pkgconf --with-path="${selfdir}/lib1" --modversion unavailable
}

modversion_uninstalled_body()
{
	atf_check -o inline:"1.2.3\n" \
		pkgconf --with-path="${selfdir}/lib1" --modversion omg
}

modversion_one_word_expression_body()
{
	atf_check -o inline:"1.2.3\n" \
		pkgconf --with-path="${selfdir}/lib1" --modversion "foo > 1.0"
}

modversion_two_word_expression_body()
{
	atf_check -o inline:"1.2.3\n" \
		pkgconf --with-path="${selfdir}/lib1" --modversion foo "> 1.0"
}

modversion_three_word_expression_body()
{
	atf_check -o inline:"1.2.3\n" \
		pkgconf --with-path="${selfdir}/lib1" --modversion foo ">" 1.0
}

modversion_one_word_expression_no_space_body()
{
	atf_check -o inline:"1.2.3\n" \
		pkgconf --with-path="${selfdir}/lib1" --modversion "foo >1.0"
}

modversion_one_word_expression_no_space_zero_body()
{
	atf_check -o inline:"1.2.3\n" \
		pkgconf --with-path="${selfdir}/lib1" --modversion "foo >0.5"
}

define_prefix_child_prefix_1_body()
{
	atf_check -o inline:"-I${selfdir}/lib1/include/child-prefix-1 -L${selfdir}/lib1/lib64 -lchild-prefix-1\n" \
		pkgconf --with-path="${selfdir}/lib1/child-prefix/pkgconfig" --define-prefix --cflags --libs child-prefix-1
}

define_prefix_child_prefix_1_env_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib1/child-prefix/pkgconfig"
	export PKG_CONFIG_RELOCATE_PATHS=1
	atf_check -o inline:"-I${selfdir}/lib1/include/child-prefix-1 -L${selfdir}/lib1/lib64 -lchild-prefix-1\n" \
		pkgconf --cflags --libs child-prefix-1
}
