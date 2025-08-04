#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Yoshihiro Ota <ota@j.email.ne.jp>
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

sysctl_name="kern.ostype"
sysctl_value="FreeBSD"
sysctl_type="string"
sysctl_description="Operating system type"

atf_test_case sysctl_aflag
sysctl_aflag_head()
{
	atf_set "descr" "Exercise all sysctl handlers"
}
sysctl_aflag_body()
{
	# Avoid using atf_check here since sysctl -ao generates tons of
	# output and it would all otherwise be saved.
	sysctl -ao >/dev/null 2>stderr
	if [ $? -ne 0 ]; then
		cat stderr
		atf_fail "sysctl -ao failed"
	elif [ -s stderr ]; then
		cat stderr
		atf_fail "sysctl -ao printed to stderr"
	fi
}


atf_test_case sysctl_aflag_jail
sysctl_aflag_jail_head()
{
	atf_set "descr" "Exercise all sysctl handlers in a jail"
	atf_set "require.user" "root"
}
sysctl_aflag_jail_body()
{
	local jail

	jail=sysctl_test_aflag_jail

	# Avoid using atf_check here since sysctl -ao generates tons of
	# output and it would all otherwise be saved.
	jail -c name=$jail command=sysctl -ao >/dev/null 2>stderr
	if [ $? -ne 0 ]; then
		atf_fail "sysctl -ao failed"
	elif [ -s stderr ]; then
		cat stderr
		atf_fail "sysctl -ao printed to stderr"
	fi

	jail -c name=$jail vnet command=sysctl -ao >/dev/null 2>stderr
	if [ $? -ne 0 ]; then
		atf_fail "sysctl -ao failed"
	elif [ -s stderr ]; then
		cat stderr
		atf_fail "sysctl -ao printed to stderr"
	fi
}


atf_test_case sysctl_by_name
sysctl_by_name_head()
{
	atf_set "descr" "Verify name without any arguments"
}
sysctl_by_name_body()
{
	atf_check -o "inline:${sysctl_name}: ${sysctl_value}\n" sysctl ${sysctl_name}
}


atf_test_case sysctl_nflag
sysctl_nflag_head()
{
	atf_set "descr" "Verify -n argument"
}
sysctl_nflag_body()
{
	atf_check -o "inline:${sysctl_value}\n" sysctl -n ${sysctl_name}
}


atf_test_case sysctl_eflag
sysctl_eflag_head()
{
	atf_set "descr" "Verify -e argument"
}
sysctl_eflag_body()
{
	atf_check -o "inline:${sysctl_name}=${sysctl_value}\n" sysctl -e ${sysctl_name}
}


atf_test_case sysctl_tflag
sysctl_tflag_head()
{
	atf_set "descr" "Verify -t argument"
}
sysctl_tflag_body()
{
	atf_check -o "inline:${sysctl_name}: ${sysctl_type}\n" sysctl -t ${sysctl_name}
}


atf_test_case sysctl_dflag
sysctl_dflag_head()
{
	atf_set "descr" "Verify -d argument"
}
sysctl_dflag_body()
{
	atf_check -o "inline:${sysctl_name}: ${sysctl_description}\n" sysctl -d ${sysctl_name}
}


atf_test_case sysctl_tflag_dflag
sysctl_tflag_dflag_head()
{
	atf_set "descr" "Verify -t -d arguments"
}
sysctl_tflag_dflag_body()
{
	atf_check -o "inline:${sysctl_name}: ${sysctl_type}: ${sysctl_description}\n" sysctl -t -d ${sysctl_name}
	atf_check -o "inline:${sysctl_name}: ${sysctl_type}: ${sysctl_description}\n" sysctl -d -t ${sysctl_name}
}


atf_test_case sysctl_nflag_tflag_dflag
sysctl_nflag_tflag_dflag_head()
{
	atf_set "descr" "Verify -n -t -d arguments"
}
sysctl_nflag_tflag_dflag_body()
{
	atf_check -o "inline:${sysctl_type}: ${sysctl_description}\n" sysctl -n -t -d ${sysctl_name}
}


atf_init_test_cases()
{
	atf_add_test_case sysctl_aflag
	atf_add_test_case sysctl_aflag_jail
	atf_add_test_case sysctl_by_name
	atf_add_test_case sysctl_nflag
	atf_add_test_case sysctl_eflag
	atf_add_test_case sysctl_tflag
	atf_add_test_case sysctl_dflag
	atf_add_test_case sysctl_tflag_dflag
	atf_add_test_case sysctl_nflag_tflag_dflag
}
