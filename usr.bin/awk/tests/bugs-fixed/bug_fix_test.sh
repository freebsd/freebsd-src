#
# Copyright 2014 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

export LANG=C.UTF-8
SRCDIR=$(atf_get_srcdir)

check()
{
	local tc=${1%.awk}; shift
	local in_flag out_flag err_flag

	awk=awk

	local out_file="${SRCDIR}/${tc}.ok"
	[ -f "${out_file}" ] && out_flag="-o file:${out_file}"
	local err_file="${SRCDIR}/${tc}.err"
	[ -f "${err_file}" ] && err_flag="-e file:${err_file} -s exit:2"
	local in_file="${SRCDIR}/${tc}.in"
	[ -f "${in_file}" ] && in_flag="${in_file}"

	(cd ${SRCDIR} ; atf_check ${out_flag} ${err_flag} ${awk} -f "${tc}.awk" ${in_flag})
}

add_testcase()
{
	local tc=${1%.awk}
	local tc_escaped word

	case "${tc%.*}" in
	*-*)
		local IFS="-"
		for word in ${tc}; do
			tc_escaped="${tc_escaped:+${tc_escaped}_}${word}"
		done
		;;
	*)
		tc_escaped=${tc}
		;;
	esac

	atf_test_case ${tc_escaped}
	eval "${tc_escaped}_body() { check ${tc}; }"
	atf_add_test_case ${tc_escaped}
}

atf_init_test_cases()
{
	for path in $(find -s "${SRCDIR}" -name '*.awk'); do
		add_testcase ${path##*/}
	done
}
