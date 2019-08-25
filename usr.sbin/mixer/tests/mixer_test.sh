#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2019 Mateusz Piotrowski <0mp@FreeBSD.org>
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

# $FreeBSD$

mixer_unavailable()
{
	! { mixer && mixer vol; } >/dev/null 2>&1
}

save_mixer_vol()
{
	atf_check -o match:'^[0-9]{1,3}:[0-9]{1,3}$' -o save:saved_vol \
		-x "mixer vol | awk '{print \$7}'"
}

set_mixer_vol()
{
	atf_check \
		-o match:'^Setting the mixer vol from [0-9]{1,3}:[0-9]{1,3} to 0:0\.$' \
		mixer vol 0
}

restore_mixer_vol()
{
	if [ -r "saved_vol" ]; then
		mixer vol "$(cat saved_vol)"
	fi
}

atf_test_case s_flag cleanup
s_flag_head()
{
	atf_set	"descr" "Verify that the output of the -s flag could be " \
		"reused as command-line arguments to the mixer command"
}
s_flag_body()
{
	if mixer_unavailable; then
		atf_skip "This test requires mixer support"
	fi
	save_mixer_vol
	set_mixer_vol
	atf_check -o inline:"vol 0:0" -o save:values mixer -s vol
	atf_check -o inline:"Setting the mixer vol from 0:0 to 0:0.\n" \
		mixer $(cat values)
}
s_flag_cleanup()
{
	restore_mixer_vol
}

atf_test_case S_flag cleanup
S_flag_head()
{
	atf_set	"descr" "Verify that the output of the -S flag is " \
		"matching the documented behavior"
}
S_flag_body()
{
	if mixer_unavailable; then
		atf_skip "This test requires mixer support"
	fi
	save_mixer_vol
	set_mixer_vol
	atf_check -o inline:"vol:0:0" mixer -S vol
}
S_flag_cleanup()
{
	restore_mixer_vol
}

atf_test_case set_empty_value
set_empty_value_head()
{
	atf_set	"descr" "Verify that mixer returns when the provided " \
		"value to set is an empty string instead of a number"
	atf_set "timeout" "1"
}
set_empty_value_body()
{
	if mixer_unavailable; then
		atf_skip "This test requires mixer support"
	fi
	save_mixer_vol
	atf_check -s exit:1 -e inline:"mixer: invalid value: \n" \
		-o match:"^usage:" mixer vol ""
}
set_empty_value_cleanup()
{
	restore_mixer_vol
}


atf_init_test_cases()
{
	atf_add_test_case s_flag
	atf_add_test_case S_flag
	atf_add_test_case set_empty_value
}
