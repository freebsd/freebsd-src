#!/bin/sh
# Copyright (c) 2019 Axcient
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

. $(atf_get_srcdir)/conf.sh

atf_test_case add cleanup
add_head()
{
	atf_set "descr" "Add a new path"
	atf_set "require.user" "root"
}
add_body()
{
	load_gmultipath
	load_dtrace

	md0=$(alloc_md)
	md1=$(alloc_md)
	md2=$(alloc_md)
	name=$(mkname)
	atf_check -s exit:0 gmultipath create "$name" ${md0} ${md1}
	check_multipath_state ${md0} "OPTIMAL" "ACTIVE" "PASSIVE" 

	# Add a new path
	atf_check -s exit:0 gmultipath add "$name" ${md2}
	check_multipath_state ${md0} "OPTIMAL" "ACTIVE" "PASSIVE" "PASSIVE"
}
add_cleanup()
{
	common_cleanup
}

atf_test_case create_A cleanup
create_A_head()
{
	atf_set "descr" "Create an Active/Active multipath device"
	atf_set "require.user" "root"
}
create_A_body()
{
	load_gmultipath
	load_dtrace

	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	atf_check -s exit:0 gmultipath create -A "$name" ${md0} ${md1}
	check_multipath_state "${md1} ${md0}" "OPTIMAL" "ACTIVE" "ACTIVE" 
}
create_A_cleanup()
{
	common_cleanup
}

atf_test_case create_R cleanup
create_R_head()
{
	atf_set "descr" "Create an Active/Read multipath device"
	atf_set "require.user" "root"
}
create_R_body()
{
	load_gmultipath
	load_dtrace

	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	atf_check -s exit:0 gmultipath create -R "$name" ${md0} ${md1}
	check_multipath_state ${md0} "OPTIMAL" "ACTIVE" "READ" 
}
create_R_cleanup()
{
	common_cleanup
}

atf_test_case depart_and_arrive cleanup
depart_and_arrive_head()
{
	atf_set "descr" "gmultipath should remove devices that disappear, and automatically reattach labeled providers that reappear"
	atf_set "require.user" "root"
}
depart_and_arrive_body()
{
	load_gnop
	load_gmultipath
	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	# We need a non-zero offset to gmultipath won't see the label when it
	# tastes the md device.  We only want the label to be visible on the
	# gnop device.
	offset=131072
	atf_check gnop create -o $offset /dev/${md0}
	atf_check gnop create -o $offset /dev/${md1}
	atf_check -s exit:0 gmultipath label "$name" ${md0}.nop
	# gmultipath is too smart to let us create a gmultipath device by label
	# when the two providers aren't actually related.  So we create a
	# device by label with one provider, and then manually add the second.
	atf_check -s exit:0 gmultipath add "$name" ${md1}.nop
	NDEVS=`gmultipath list "$name" | grep -c 'md[0-9]*\.nop'`
	atf_check_equal 2 $NDEVS

	# Now fail the labeled provider
	atf_check -s exit:0 gnop destroy -f ${md0}.nop
	# It should be automatically removed from the multipath device
	NDEVS=`gmultipath list "$name" | grep -c 'md[0-9]*\.nop'`
	atf_check_equal 1 $NDEVS

	# Now return the labeled provider
	atf_check gnop create -o $offset /dev/${md0}
	# It should be automatically restored to the multipath device.  We
	# don't really care which path is active.
	NDEVS=`gmultipath list "$name" | grep -c 'md[0-9]*\.nop'`
	atf_check_equal 2 $NDEVS
	STATE=`gmultipath list "$name" | awk '/^State:/ {print $2}'`
	atf_check_equal "OPTIMAL" $STATE
}
depart_and_arrive_cleanup()
{
	common_cleanup
}


atf_test_case fail cleanup
fail_head()
{
	atf_set "descr" "Manually fail a path"
	atf_set "require.user" "root"
}
fail_body()
{
	load_gmultipath
	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	atf_check -s exit:0 gmultipath create "$name" ${md0} ${md1}
	check_multipath_state ${md0} "OPTIMAL" "ACTIVE" "PASSIVE" 
	# Manually fail the active path
	atf_check -s exit:0 gmultipath fail "$name" ${md0}
	check_multipath_state ${md1} "DEGRADED" "FAIL" "ACTIVE" 
}
fail_cleanup()
{
	common_cleanup
}

atf_test_case fail_on_error cleanup
fail_on_error_head()
{
	atf_set "descr" "An error in the provider will cause gmultipath to mark it as FAIL"
	atf_set "require.user" "root"
}
fail_on_error_body()
{
	load_gnop
	load_gmultipath
	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	atf_check gnop create /dev/${md0}
	atf_check gnop create /dev/${md1}
	atf_check -s exit:0 gmultipath create "$name" ${md0}.nop ${md1}.nop
	# The first I/O to the first path should fail, causing gmultipath to
	# fail over to the second path.
	atf_check gnop configure -r 100 -w 100 ${md0}.nop
	atf_check -s exit:0 -o ignore -e ignore dd if=/dev/zero of=/dev/multipath/"$name" bs=4096 count=1
	check_multipath_state ${md1}.nop "DEGRADED" "FAIL" "ACTIVE" 
}
fail_on_error_cleanup()
{
	common_cleanup
}

atf_test_case physpath cleanup
physpath_head()
{
	atf_set "descr" "gmultipath should append /mp to the underlying providers' physical path"
	atf_set "require.user" "root"
}
physpath_body()
{
	load_gnop
	load_gmultipath
	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	physpath="some/physical/path"
	# Create two providers with the same physical paths, mimicing how
	# multipathed SAS drives appear.  This is the normal way to use
	# gmultipath.  If the underlying providers' physical paths differ,
	# then you're probably using gmultipath wrong.
	atf_check gnop create -z $physpath /dev/${md0}
	atf_check gnop create -z $physpath /dev/${md1}
	atf_check -s exit:0 gmultipath create "$name" ${md0}.nop ${md1}.nop
	gmultipath_physpath=$(diskinfo -p multipath/"$name") 
	atf_check_equal "$physpath/mp" "$gmultipath_physpath"
}
physpath_cleanup()
{
	common_cleanup
}

atf_test_case prefer cleanup
prefer_head()
{
	atf_set "descr" "Manually select the preferred path"
	atf_set "require.user" "root"
}
prefer_body()
{
	load_gmultipath
	load_dtrace

	md0=$(alloc_md)
	md1=$(alloc_md)
	md2=$(alloc_md)
	name=$(mkname)
	atf_check -s exit:0 gmultipath create "$name" ${md0} ${md1} ${md2}
	check_multipath_state ${md0} "OPTIMAL" "ACTIVE" "PASSIVE" "PASSIVE"

	# Explicitly prefer the final path
	atf_check -s exit:0 gmultipath prefer "$name" ${md2}
	check_multipath_state ${md2} "OPTIMAL" "PASSIVE" "PASSIVE" "ACTIVE"
}
prefer_cleanup()
{
	common_cleanup
}

atf_test_case restore cleanup
restore_head()
{
	atf_set "descr" "Manually restore a failed path"
	atf_set "require.user" "root"
}
restore_body()
{
	load_gmultipath
	load_dtrace

	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	atf_check -s exit:0 gmultipath create "$name" ${md0} ${md1}

	# Explicitly fail the first path
	atf_check -s exit:0 gmultipath fail "$name" ${md0}
	check_multipath_state ${md1} "DEGRADED" "FAIL" "ACTIVE" 

	# Explicitly restore it
	atf_check -s exit:0 gmultipath restore "$name" ${md0}
	check_multipath_state ${md1} "OPTIMAL" "PASSIVE" "ACTIVE" 
}
restore_cleanup()
{
	common_cleanup
}

atf_test_case restore_on_error cleanup
restore_on_error_head()
{
	atf_set "descr" "A failed path should be restored if an I/O error is encountered on all other active paths"
	atf_set "require.user" "root"
}
restore_on_error_body()
{
	load_gnop
	load_gmultipath
	load_dtrace

	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	atf_check gnop create /dev/${md0}
	atf_check gnop create /dev/${md1}
	atf_check -s exit:0 gmultipath create "$name" ${md0}.nop ${md1}.nop
	# Explicitly fail the first path
	atf_check -s exit:0 gmultipath fail "$name" ${md0}.nop

	# Setup the second path to fail on the next I/O
	atf_check gnop configure -r 100 -w 100  ${md1}.nop
	atf_check -s exit:0 -o ignore -e ignore \
	    dd if=/dev/zero of=/dev/multipath/"$name" bs=4096 count=1

	# Now the first path should be active, and the second should be failed
	check_multipath_state ${md0}.nop "DEGRADED" "ACTIVE" "FAIL" 
}
restore_on_error_cleanup()
{
	common_cleanup
}

atf_test_case rotate cleanup
rotate_head()
{
	atf_set "descr" "Manually rotate the active path"
	atf_set "require.user" "root"
}
rotate_body()
{
	load_gmultipath
	load_dtrace

	md0=$(alloc_md)
	md1=$(alloc_md)
	md2=$(alloc_md)
	name=$(mkname)
	atf_check -s exit:0 gmultipath create "$name" ${md0} ${md1} ${md2}
	check_multipath_state ${md0} "OPTIMAL" "ACTIVE" "PASSIVE" "PASSIVE"

	# Explicitly rotate the paths
	atf_check -s exit:0 gmultipath rotate "$name"
	check_multipath_state ${md2} "OPTIMAL" "PASSIVE" "PASSIVE" "ACTIVE"
	# Again
	atf_check -s exit:0 gmultipath rotate "$name"
	check_multipath_state ${md1} "OPTIMAL" "PASSIVE" "ACTIVE" "PASSIVE"
	# Final rotation should restore original configuration
	atf_check -s exit:0 gmultipath rotate "$name"
	check_multipath_state ${md0} "OPTIMAL" "ACTIVE" "PASSIVE" "PASSIVE"
}
rotate_cleanup()
{
	common_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case add
	atf_add_test_case create_A
	atf_add_test_case create_R
	atf_add_test_case depart_and_arrive
	atf_add_test_case fail
	atf_add_test_case fail_on_error
	atf_add_test_case physpath
	atf_add_test_case prefer
	atf_add_test_case restore
	atf_add_test_case restore_on_error
	atf_add_test_case rotate
}
