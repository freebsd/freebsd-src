#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Mark Johnston <markj@FreeBSD.org>
#

atf_test_case livedump_kldstat
livedump_kldstat_head()
{
	atf_set "descr" "Test livedump integrity"
	atf_set "require.progs" kgdb
	atf_set "require.user" root
}
livedump_kldstat_body()
{
	atf_check -e match:"savecore .*- livedump" savecore -L .

	kernel=$(sysctl -n kern.bootfile)

	if ! [ -f /usr/lib/debug/${kernel}.debug ]; then
		atf_skip "No debug symbols for the running kernel"
	fi

	# Implement kldstat using gdb script.
	cat >./kldstat.gdb <<'__EOF__'
printf "Id Refs Address                Size Name\n"
set $_lf = linker_files.tqh_first
while ($_lf)
    printf "%2d %4d %p %8x %s\n", $_lf->id, $_lf->refs, $_lf->address, $_lf->size, $_lf->filename
    set $_lf = $_lf->link.tqe_next
end
__EOF__

	# Ignore stderr since kgdb prints some warnings about inaccessible
	# source files.
	#
	# Use a script to source the main gdb script, otherwise kgdb prints
	# a bunch of line noise that is annoying to filter out.
	echo "source ./kldstat.gdb" > ./script.gdb
	atf_check -o save:out -e ignore \
	    kgdb -q ${kernel} ./livecore.0 < ./script.gdb

	# Get rid of gunk printed by kgdb.
	sed -i '' -n -e 's/^(kgdb) //' -e '/^Id Refs /,$p' out

	# The output of kgdb should match the output of kldstat.
	atf_check -o save:kldstat kldstat
	atf_check diff kldstat out
}

atf_init_test_cases()
{
	atf_add_test_case livedump_kldstat
}
