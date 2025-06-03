# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 David A. Bright
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

#
# A basic test for nvmecontrol. This isn't a thorough or complete test
# of nvmecontrol functionality; it is more of a sanity check that
# nvmecontrol basically works.
#

DANGEROUS=false # Set to true to run "dangerous" tests
# Select a nvme device to use for testing. If none exist, use nvme0.
TEST_DEV=$(cd /dev/; ls -1 nvme[0-9]* 2> /dev/null | grep -E 'nvme[0-9][0-9]*$' | head -n 1)
TEST_DEV=${TEST_DEV:-nvme0}
TEST_DEV_PATH=/dev/${TEST_DEV}
INV_OPT="-z"
INV_OPT_MSG="invalid option -- z"


atf_test_case fake_lib cleanup
fake_lib_head()
{
	atf_set "descr" "check loading of a library from /lib"
	atf_set "require.user" "root"
}
fake_lib_body()
{
	local libdir="/lib/nvmecontrol"
	local fakelib="${libdir}/fake.so"
	if [ -d ${libdir} ] ; then
		touch ${fakelib}
		atf_check -s not-exit:0 -o ignore -e match:"Can't load ${fakelib}" nvmecontrol
		rm -f ${fakelib}
	else
		atf_skip "Skipping; directory ${libdir} does not exist"
	fi
}
fake_lib_cleanup()
{
	rm -f /lib/nvmecontrol/fake.so
}

atf_test_case fake_local_lib cleanup
fake_local_lib_head()
{
	atf_set "descr" "check loading of a library from /usr/local/lib"
	atf_set "require.user" "root"
}
fake_local_lib_body()
{
	local libdir="/usr/local/lib/nvmecontrol"
	local fakelib="${libdir}/fake.so"
	if [ -d ${libdir} ] ; then
		touch ${fakelib}
		atf_check -s not-exit:0 -o ignore -e match:"Can't load ${fakelib}" nvmecontrol
		rm -f ${fakelib}
	else
		atf_skip "Skipping; directory ${libdir} does not exist"
	fi
}
fake_local_lib_cleanup()
{
	rm -f /usr/local/lib/nvmecontrol/fake.so
}

atf_test_case admin_passthru
admin_passthru_head()
{
	atf_set "descr" "check the admin-passthru command"
	atf_set "require.user" "root"
}
admin_passthru_body()
{
	if [ -c "${TEST_DEV_PATH}" ] ; then
		atf_check -o not-empty -e empty nvmecontrol admin-passthru --opcode=06 --data-len=4096 --cdw10=1 -r ${TEST_DEV}
	else
		atf_check -s not-exit:0 -o empty -e not-empty nvmecontrol admin-passthru --opcode=06 --data-len=4096 --cdw10=1 -r ${TEST_DEV}
	fi
	atf_check -s not-exit:0 -o ignore -e match:"${INV_OPT_MSG}" nvmecontrol admin-passthru ${INV_OPT} --opcode=06 --data-len=4096 --cdw10=1 -r ${TEST_DEV}
}

atf_test_case devlist
devlist_head()
{
	atf_set "descr" "check the devlist command"
	atf_set "require.user" "root"
}
devlist_body()
{
	if [ -c "${TEST_DEV_PATH}" ] ; then
		atf_check -o not-empty -e empty nvmecontrol devlist
	else
		atf_check -s not-exit:0 -o ignore -e ignore nvmecontrol devlist
	fi
	atf_check -s not-exit:0 -o ignore -e match:"${INV_OPT_MSG}" nvmecontrol devlist ${INV_OPT}
}

atf_test_case identify
identify_head()
{
	atf_set "descr" "check the identify command"
	atf_set "require.user" "root"
}
identify_body()
{
	if [ -c "${TEST_DEV_PATH}" ] ; then
		atf_check -o not-empty -e empty nvmecontrol identify ${TEST_DEV}
	else
		atf_check -s not-exit:0 -o empty -e not-empty nvmecontrol identify ${TEST_DEV}
	fi
	atf_check -s not-exit:0 -o ignore -e match:"${INV_OPT_MSG}" nvmecontrol identify ${INV_OPT} ${TEST_DEV}
}

atf_test_case io_passthru
io_passthru_head()
{
	atf_set "descr" "check the io-passthru command"
	atf_set "require.user" "root"
}
io_passthru_body()
{
	if [ -c "${TEST_DEV_PATH}" ] ; then
		atf_check -o not-empty -e empty nvmecontrol io-passthru --opcode=02 --data-len=4096 --cdw10=0 --cdw11=0 --cdw12=0x70000 -r nvme0 ${TEST_DEV}
	else
		atf_check -s not-exit:0 -o empty -e not-empty nvmecontrol io-passthru --opcode=02 --data-len=4096 --cdw10=0 --cdw11=0 --cdw12=0x70000 -r nvme0 ${TEST_DEV}
	fi
	atf_check -s not-exit:0 -o ignore -e match:"${INV_OPT_MSG}" nvmecontrol io-passthru ${INV_OPT} --opcode=02 --data-len=4096 --cdw10=0 --cdw11=0 --cdw12=0x70000 -r nvme0 ${TEST_DEV}
}

atf_test_case logpage
logpage_head()
{
	atf_set "descr" "check the logpage command"
	atf_set "require.user" "root"
}
logpage_body()
{
	if [ -c "${TEST_DEV_PATH}" ] ; then
		atf_check -o not-empty -e empty nvmecontrol logpage -p 1 ${TEST_DEV}
	else
		atf_check -s not-exit:0 -o empty -e not-empty nvmecontrol logpage -p 1 ${TEST_DEV}
	fi
	atf_check -s not-exit:0 -o ignore -e match:"${INV_OPT_MSG}" nvmecontrol logpage -p 1 ${INV_OPT} ${TEST_DEV}
}

atf_test_case nsid
nsid_head()
{
	atf_set "descr" "check the nsid command"
	atf_set "require.user" "root"
}
nsid_body()
{
	if [ -c "${TEST_DEV_PATH}" ] ; then
		atf_check -o not-empty -e empty nvmecontrol nsid ${TEST_DEV}
	else
		atf_check -s not-exit:0 -o empty -e not-empty nvmecontrol nsid ${TEST_DEV}
	fi
	atf_check -s not-exit:0 -o ignore -e match:"${INV_OPT_MSG}" nvmecontrol nsid ${INV_OPT} ${TEST_DEV}
}

atf_test_case power
power_head()
{
	atf_set "descr" "check the power command"
	atf_set "require.user" "root"
}
power_body()
{
	if [ -c "${TEST_DEV_PATH}" ] ; then
		atf_check -o not-empty -e empty nvmecontrol power ${TEST_DEV}
	else
		atf_check -s not-exit:0 -o empty -e not-empty nvmecontrol power ${TEST_DEV}
	fi
	atf_check -s not-exit:0 -o ignore -e match:"${INV_OPT_MSG}" nvmecontrol power ${INV_OPT} ${TEST_DEV}
}

atf_test_case reset
reset_head()
{
	atf_set "descr" "check the reset command"
	atf_set "require.user" "root"
}
reset_body()
{
	atf_check -s not-exit:0 -o ignore -e match:"${INV_OPT_MSG}" nvmecontrol reset ${INV_OPT} ${TEST_DEV}
	if [ -c "${TEST_DEV_PATH}" ] ; then
		# Reset of an active device seems a little dangerous,
		# therefore, this is normally disabled.
	    	if ${DANGEROUS} ; then
			atf_check -o not-empty -e empty nvmecontrol reset ${TEST_DEV}
		else
			atf_skip "Skipping reset test"
		fi
	else
		atf_check -s not-exit:0 -o empty -e not-empty nvmecontrol reset ${TEST_DEV}
	fi
}


atf_init_test_cases()
{
	atf_add_test_case fake_lib
	atf_add_test_case fake_local_lib
	atf_add_test_case admin_passthru
	atf_add_test_case devlist
	atf_add_test_case identify
	atf_add_test_case io_passthru
	atf_add_test_case logpage
	atf_add_test_case nsid
	atf_add_test_case power
	atf_add_test_case reset
}
