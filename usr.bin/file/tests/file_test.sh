#!/usr/libexec/atf-sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Eric van Gyzen
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

atf_test_case contrib_file_tests cleanup
contrib_file_tests_body() {
	srcdir="$(atf_get_srcdir)"

	for testfile in "${srcdir}"/*.testfile; do
		test_name="${testfile%.testfile}"
		result_file="${test_name}.result"
		file_args=
		magic_files=
		for magic_file in ${test_name}*.magic; do
			if [ -f "${magic_file}" ]; then
				if [ -z "${magic_files}" ]; then
					magic_files="${magic_file}"
				else
					magic_files="${magic_files}:${magic_file}"
				fi
			fi
		done
		if [ -z "${magic_files}" ]; then
			magic_files=/usr/share/misc/magic
		fi
		if [ -f "${test_name}.flags" ]; then
			file_args="${file_args} -$(cat "${test_name}.flags")"
		fi
		# The result files were created in UTC.
		atf_check -o save:actual_output -e ignore env TZ=Z MAGIC="${magic_files}" \
			file ${file_args} --brief "$testfile"
		atf_check cmp actual_output "$result_file"
	done
}

contrib_file_tests_cleanup() {
	rm -f actual_output trimmed_output
}

atf_init_test_cases() {
	atf_add_test_case contrib_file_tests
}
