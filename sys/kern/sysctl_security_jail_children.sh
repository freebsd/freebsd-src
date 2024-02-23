#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Igor Ostapenko <pm@igoro.pro>
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
# Even being is_exclusive="true" this test does not expect a host to spawn
# other jails during the test execution.
#
atf_test_case "max_cur" "cleanup"
max_cur_head()
{
	atf_set descr 'Test maximum and current number of child jails'
	atf_set require.user root
	if ! which -s jail; then
		atf_skip "This test requires jail"
	fi
}
max_cur_body()
{
	origin_max=$(sysctl -n security.jail.children.max)
	origin_cur=$(sysctl -n security.jail.children.cur)

	# Magic numbers reasoning:
	# 3 stands for:
	#   - the test creates three jails: childfree, maxallowed, maxallowed.family
	# 6 stands for:
	#   - maxallowed.family wants to set children.max=4
	#   - it means that its parent (maxallowed) should have at least children.max=5
	#   - it makes the origin (parent of maxallowed) provide children.max=6 minimum
	#
	test $origin_cur -le $origin_max || atf_fail "Abnormal cur=$origin_cur > max=$origin_max."
	test $((origin_max - origin_cur)) -ge 3 || atf_skip "Not enough child jails are allowed for the test."
	test $origin_max -ge 6 || atf_skip "Not high enough children.max limit for the test."

	jail -c name=childfree persist
	atf_check_equal "$((origin_cur + 1))" "$(sysctl -n security.jail.children.cur)"
	atf_check_equal "0" "$(jexec childfree sysctl -n security.jail.children.max)"
	atf_check_equal "0" "$(jexec childfree sysctl -n security.jail.children.cur)"

	jail -c name=maxallowed children.max=$((origin_max - 1)) persist
	atf_check_equal "$((origin_cur + 2))" "$(sysctl -n security.jail.children.cur)"
	atf_check_equal "$((origin_max - 1))" "$(jexec maxallowed sysctl -n security.jail.children.max)"
	atf_check_equal "0" "$(jexec maxallowed sysctl -n security.jail.children.cur)"

	jexec maxallowed jail -c name=family children.max=4 persist
	atf_check_equal "$((origin_cur + 3))" "$(sysctl -n security.jail.children.cur)"
	atf_check_equal "1" "$(jexec maxallowed sysctl -n security.jail.children.cur)"
	atf_check_equal "4" "$(jexec maxallowed.family sysctl -n security.jail.children.max)"
	atf_check_equal "0" "$(jexec maxallowed.family sysctl -n security.jail.children.cur)"
}
max_cur_cleanup()
{
	jail -r maxallowed
	jail -r childfree
	return 0
}

atf_init_test_cases()
{
	atf_add_test_case "max_cur"
}
