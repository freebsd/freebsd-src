#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	dump_personality \
	pc_path_var \
	pc_system_libdirs_var \
	pc_system_includedirs_var

dump_personality_body()
{
	atf_check \
		-o match:"Triplet: i386-linux-gnu" \
		-o match:"DefaultSearchPaths: /usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig" \
		-o match:"SystemIncludePaths: /usr/lib/i386-linux-gnu/include" \
		-o match:"SystemLibraryPaths: /usr/lib/i386-linux-gnu/lib" \
		pkgconf --personality=${selfdir}/personality-data/i386-linux-gnu.personality --dump-personality
}

pc_path_var_body()
{
	atf_check \
		-o inline:"/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig\n" \
		pkgconf --personality=${selfdir}/personality-data/i386-linux-gnu.personality --variable=pc_path pkg-config
}

pc_system_libdirs_var_body()
{
	atf_check \
		-o inline:"/usr/lib/i386-linux-gnu/lib\n" \
		pkgconf --personality=${selfdir}/personality-data/i386-linux-gnu.personality --variable=pc_system_libdirs pkg-config
}

pc_system_includedirs_var_body()
{
	atf_check \
		-o inline:"/usr/lib/i386-linux-gnu/include\n" \
		pkgconf --personality=${selfdir}/personality-data/i386-linux-gnu.personality --variable=pc_system_includedirs pkg-config
}
