#
# Copyright (c) 2026 Dag-Erling Smørgrav <des@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Create a small file system to experiment on
tunefs_setup()
{
	atf_check -o save:dev mdconfig -t malloc -s 16M
	tunefs_dev=/dev/$(cat dev)
	atf_check -o ignore newfs "$@" $tunefs_dev
}

# Verify that the changes we ask tunefs to perform are applied to the
# test file system
tunefs_test()
{
	local opt=$1		# tunefs option
	local name=$2		# what tunefs(8) calls it
	local descr=${3:-$name}	# what file(1) calls it, if different

	# Verify that the optin is not enabled
	atf_check -o not-match:"$name" \
	    file -s $tunefs_dev

	# Enable the option and verify that it is enabled
	atf_check -e match:"$name set" -o ignore \
	    tunefs $opt enable $tunefs_dev
	atf_check -o match:"$descr" \
	    file -s $tunefs_dev

	# Enable it again and verify that it is still enabled
	atf_check -e match:"$name remains unchanged as enabled" \
	    tunefs $opt enable $tunefs_dev
	atf_check -o match:"$descr" \
	    file -s $tunefs_dev

	# Disable the option and verify that it is disabled
	atf_check -e match:"$name cleared" -o ignore \
	    tunefs $opt disable $tunefs_dev
	atf_check -o not-match:"$descr" \
	    file -s $tunefs_dev

	# Disable it again and verify that it is still disabled
	atf_check -e match:"$name remains unchanged as disabled" \
	    tunefs $opt disable $tunefs_dev
	atf_check -o not-match:"$descr" \
	    file -s $tunefs_dev
}

# Clean up after ourselves
tunefs_cleanup()
{
	# Destroy the test file system
	if [ -f dev ]; then
		mdconfig -d -u $(cat dev) || true
	fi
}

# POSIX.1e ACLs
atf_test_case posixacl cleanup
posixacl_head()
{
	atf_set descr "Turn POSIX.1e ACLs on and off"
	atf_set require.user "root"
}
posixacl_body()
{
	tunefs_setup
	tunefs_test -a "POSIX.1e ACLs"
}
posixacl_cleanup()
{
	tunefs_cleanup
}

# NFSv4 ACLs
atf_test_case nfs4acl cleanup
nfs4acl_head()
{
	atf_set descr "Turn NFSv4 ACLs on and off"
	atf_set require.user "root"
}
nfs4acl_body()
{
	tunefs_setup
	tunefs_test -N "NFSv4 ACLs"
}
nfs4acl_cleanup()
{
	tunefs_cleanup
}

# Soft Updates (no journaling)
atf_test_case sunoj cleanup
sunoj_head()
{
	atf_set descr "Turn Soft Updates on and off"
	atf_set require.user "root"
}
sunoj_body()
{
	tunefs_setup -u
	tunefs_test -n "soft updates"
}
sunoj_cleanup()
{
	tunefs_cleanup
}

# Soft Updates journaling
atf_test_case suj cleanup
suj_head()
{
	atf_set descr "Turn Soft Updates journaling on and off"
	atf_set require.user "root"
}
suj_body()
{
	tunefs_setup
	tunefs_test -j "soft updates journaling"
}
suj_cleanup()
{
	tunefs_cleanup
}

# GEOM journaling
atf_test_case gjournal cleanup
gjournal_head()
{
	atf_set descr "Turn GEOM journaling on and off"
	atf_set require.user "root"
}
gjournal_body()
{
	tunefs_setup -u
	tunefs_test -J "gjournal" "GEOM journaling"
}
gjournal_cleanup()
{
	tunefs_cleanup
}

# Try combining Soft Updates with GEOM journaling
atf_test_case conflict cleanup
conflict_head()
{
	atf_set descr "Soft Updates and GEOM journaling are mutually exclusive"
	atf_set require.user "root"
}
conflict_body()
{
	tunefs_setup -U

	# Verify that SU is enabled
	atf_check -o match:"soft updates" \
	    file -s $tunefs_dev
	# Verify that enabling gjournal fails
	atf_check -e match:"cannot be enabled" \
	    tunefs -J enable $tunefs_dev
	# Now turn SU off
	atf_check -e match:"soft updates cleared" \
	    tunefs -n disable $tunefs_dev
	# Enable gjournal
	atf_check -e match:"gjournal set" \
	    tunefs -J enable $tunefs_dev
	# Verify that enabling SU+J fails
	atf_check -e match:"cannot be enabled" \
	    tunefs -j enable $tunefs_dev
	# Verify that enabling SU alone fails
	atf_check -e match:"cannot be enabled" \
	    tunefs -n enable $tunefs_dev
}
conflict_cleanup()
{
	tunefs_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case posixacl
	atf_add_test_case nfs4acl
	atf_add_test_case sunoj
	atf_add_test_case suj
	atf_add_test_case gjournal
	atf_add_test_case conflict
}
