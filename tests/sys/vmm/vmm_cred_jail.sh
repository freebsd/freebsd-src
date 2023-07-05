#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 The FreeBSD Foundation
#
# This software was developed by Cyril Zhang under sponsorship from
# the FreeBSD Foundation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the distribution.
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

. $(atf_get_srcdir)/utils.subr

atf_test_case vmm_cred_jail_host cleanup
vmm_cred_jail_host_head()
{
	atf_set "descr" "Tests deleting the host's VM from within a jail"
	atf_set "require.user" "root"
}
vmm_cred_jail_host_body()
{
	if ! kldstat -qn vmm; then
		atf_skip "vmm is not loaded"
	fi
	bhyvectl --vm=testvm --create
	vmm_mkjail myjail
	atf_check -s exit:1 -e ignore jexec myjail bhyvectl --vm=testvm --destroy
}
vmm_cred_jail_host_cleanup()
{
	bhyvectl --vm=testvm --destroy
	vmm_cleanup
}

atf_test_case vmm_cred_jail_other cleanup
vmm_cred_jail_other_head()
{
	atf_set "descr" "Tests deleting a jail's VM from within another jail"
	atf_set "require.user" "root"
}
vmm_cred_jail_other_body()
{
	if ! kldstat -qn vmm; then
		atf_skip "vmm is not loaded"
	fi
	vmm_mkjail myjail1
	vmm_mkjail myjail2
	atf_check -s exit:0 jexec myjail1 bhyvectl --vm=testvm --create
	atf_check -s exit:1 -e ignore jexec myjail2 bhyvectl --vm=testvm --destroy
}
vmm_cred_jail_other_cleanup()
{
	bhyvectl --vm=testvm --destroy
	vmm_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case vmm_cred_jail_host
	atf_add_test_case vmm_cred_jail_other
}
