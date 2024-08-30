#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 The FreeBSD Foundation
#
# This software was developed by Christos Margiolis <christos@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
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

mixer_exists()
{
	if ! mixer >/dev/null 2>&1; then
		atf_skip "no mixer available"
	fi
}

save_conf()
{
	atf_check -o save:test_mixer_conf mixer -o
}

restore_conf()
{
	mixer_exists
	test -r "test_mixer_conf" && mixer $(cat test_mixer_conf)
}

load_dummy()
{
	if ! kldload -n snd_dummy; then
		atf_skip "cannot load snd_dummy.ko"
	fi
}

set_default()
{
	deflt_unit="$(mixer | grep ^pcm | cut -f1 -d:)"
	dummy_unit="$(mixer -a | grep "Dummy Audio Device" | cut -f1 -d:)"

	atf_check -o save:test_mixer_deflt_unit echo ${deflt_unit}
	atf_check -o save:test_mixer_dummy_unit echo ${dummy_unit}

	# Set the dummy as the default
	mixer -d ${dummy_unit}
}

restore_default()
{
	test -r "test_mixer_deflt_unit" &&
	mixer -d $(cat test_mixer_deflt_unit) || true
}

atf_test_case o_flag cleanup
o_flag_head()
{
	atf_set "descr" "Verify that the output of the -o flag can be used " \
		"as valid input"
}
o_flag_body()
{
	load_dummy
	mixer_exists
	set_default

	atf_check -o ignore -e empty mixer $(mixer -o)
}
o_flag_cleanup()
{
	restore_default
}

atf_test_case d_flag cleanup
d_flag_head()
{
	atf_set "descr" "Test default unit setting"
}
d_flag_body()
{
	load_dummy
	mixer_exists
	set_default

	dev="${dummy_unit}"
	unit=$(echo ${dev} | sed 's/pcm//')

	atf_check -o ignore -e empty mixer -d ${dev}
	atf_check -o ignore -e empty mixer -d ${unit}
}
d_flag_cleanup()
{
	restore_default
}

atf_test_case volume cleanup
volume_head()
{
	atf_set "descr" "Test volume setting"
}
volume_body()
{
	load_dummy
	mixer_exists
	set_default
	save_conf

	# Test lower bound
	mixer vol.volume=0
	atf_check -o match:"0.00:0.00" mixer vol.volume

	mixer vol.volume=-2
	atf_check -o match:"0.00:0.00" mixer vol.volume

	mixer vol.volume=-1:-2
	atf_check -o match:"0.00:0.00" mixer vol.volume

	mixer vol.volume=-110%
	atf_check -o match:"0.00:0.00" mixer vol.volume

	# Test higher bound
	mixer vol.volume=1
	atf_check -o match:"1.00:1.00" mixer vol.volume

	mixer vol.volume=+1.01
	atf_check -o match:"1.00:1.00" mixer vol.volume

	mixer vol.volume=2
	atf_check -o match:"1.00:1.00" mixer vol.volume

	mixer vol.volume=+1:+1
	atf_check -o match:"1.00:1.00" mixer vol.volume

	mixer vol.volume=2:2
	atf_check -o match:"1.00:1.00" mixer vol.volume

	mixer vol.volume=100%
	atf_check -o match:"1.00:1.00" mixer vol.volume

	mixer vol.volume=110%
	atf_check -o match:"1.00:1.00" mixer vol.volume

	mixer vol.volume=+110%
	atf_check -o match:"1.00:1.00" mixer vol.volume

	# Test percentages
	mixer vol.volume=1

	mixer vol.volume=-10%
	atf_check -o match:"0.90:0.90" mixer vol.volume

	mixer vol.volume=+5%
	atf_check -o match:"0.95:0.95" mixer vol.volume

	mixer vol.volume=80%
	atf_check -o match:"0.80:0.80" mixer vol.volume

	# Test left:right assignment
	mixer vol.volume=0.80:0.70
	atf_check -o match:"0.80:0.70" mixer vol.volume

	mixer vol.volume=+5%:+10%
	atf_check -o match:"0.85:0.80" mixer vol.volume

	mixer vol.volume=-5%:-10%
	atf_check -o match:"0.80:0.70" mixer vol.volume

	mixer vol.volume=+10%:-15%
	atf_check -o match:"0.90:0.55" mixer vol.volume

	# Test wrong values
	atf_check -o ignore -e not-empty mixer vol.volume=foobar
	atf_check -o ignore -e not-empty mixer vol.volume=2oo:b4r
	atf_check -o ignore -e not-empty mixer vol.volume=+f0o:1
}
volume_cleanup()
{
	restore_conf
	restore_default
}

atf_test_case mute cleanup
mute_head()
{
	atf_set "descr" "Test muting"
}
mute_body()
{
	load_dummy
	mixer_exists
	set_default
	save_conf

	# Check that the mute control exists
	atf_check -o not-empty mixer vol.mute

	atf_check -o ignore -e empty mixer vol.mute=off
	atf_check -o match:"=off" mixer vol.mute

	atf_check -o ignore -e empty mixer vol.mute=on
	atf_check -o match:"=on" mixer vol.mute

	atf_check -o ignore -e empty mixer vol.mute=toggle
	atf_check -o match:"=off" mixer vol.mute

	# Test deprecated interface
	atf_check -o ignore -e empty mixer vol.mute=0
	atf_check -o match:"=off" mixer vol.mute

	atf_check -o ignore -e empty mixer vol.mute=1
	atf_check -o match:"=on" mixer vol.mute

	atf_check -o ignore -e empty mixer vol.mute=^
	atf_check -o match:"=off" mixer vol.mute

	# Test wrong values
	atf_check -o ignore -e not-empty mixer vol.mute=foobar
	atf_check -o ignore -e not-empty mixer vol.mute=10
}
mute_cleanup()
{
	restore_conf
	restore_default
}

atf_test_case recsrc cleanup
recsrc_head()
{
	atf_set "descr" "Test recording source handling"
}
recsrc_body()
{
	load_dummy
	mixer_exists
	set_default
	save_conf
	test -n "$(mixer -s)" || atf_skip "no recording source found"

	recsrc=$(mixer -s | awk '{print $2}')
	atf_check -o ignore -e empty mixer ${recsrc}.recsrc=add
	atf_check -o ignore -e empty mixer ${recsrc}.recsrc=remove
	atf_check -o ignore -e empty mixer ${recsrc}.recsrc=set
	atf_check -o ignore -e empty mixer ${recsrc}.recsrc=toggle

	# Test deprecated interface
	atf_check -o ignore -e empty mixer ${recsrc}.recsrc=+
	atf_check -o ignore -e empty mixer ${recsrc}.recsrc=-
	atf_check -o ignore -e empty mixer ${recsrc}.recsrc==
	atf_check -o ignore -e empty mixer ${recsrc}.recsrc=^

	# Test wrong values
	atf_check -o ignore -e not-empty mixer ${recsrc}.recsrc=foobar
	atf_check -o ignore -e not-empty mixer ${recsrc}.recsrc=10
}
recsrc_cleanup()
{
	restore_conf
	restore_default
}

atf_init_test_cases()
{
	atf_add_test_case o_flag
	atf_add_test_case d_flag
	atf_add_test_case volume
	atf_add_test_case mute
	atf_add_test_case recsrc
}
