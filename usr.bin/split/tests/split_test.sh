#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Klara Systems
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

# sys/param.h
: ${MAXBSIZE:=65536}

atf_test_case bytes
bytes_body()
{
	printf "aaaa" > foo-aa
	printf "bb\nc" > foo-ab
	printf "ccc\n" > foo-ac

	cat foo-* > foo
	atf_check split -b 4 foo split-
	atf_check -o file:foo-aa cat split-aa
	atf_check -o file:foo-ab cat split-ab
	atf_check -o file:foo-ac cat split-ac

	# MAXBSIZE is the default buffer size, so we'll split at just a little
	# bit past the buffer size to make sure that it still properly splits
	# even when it needs to read again to hit the limit.
	bsize=$((MAXBSIZE + 12))
	rm foo-* foo
	jot -ns "" -b "a" ${bsize} > foo-aa
	jot -ns "" -b "b" ${bsize} > foo-ab
	jot -ns "" -b "c" 12 > foo-ac

	cat foo-* > foo
	atf_check split -b ${bsize} foo split-
	atf_check -o file:foo-aa cat split-aa
	atf_check -o file:foo-ab cat split-ab
	atf_check -o file:foo-ac cat split-ac
}

atf_test_case chunks
chunks_body()
{
	jot -ns "" -b "a" 4096 > foo
	jot -ns "" -b "b" 4096 >> foo
	jot -ns "" -b "c" 4104 >> foo

	chunks=3
	jot -ns "" -b "a" 4096 > foo-aa
	jot -ns "" -b "b" 2 >> foo-aa
	jot -ns "" -b "b" 4094 > foo-ab
	jot -ns "" -b "c" 4 >> foo-ab
	jot -ns "" -b "c" 4100 > foo-ac

	atf_check split -n ${chunks} foo split-
	atf_check -o file:foo-aa cat split-aa
	atf_check -o file:foo-ab cat split-ab
	atf_check -o file:foo-ac cat split-ac
}

atf_test_case sensible_lines
sensible_lines_body()
{
	echo "The quick brown fox" > foo-aa
	echo "jumps over" > foo-ab
	echo "the lazy dog" > foo-ac

	cat foo-* > foo
	atf_check split -l 1 foo split-
	atf_check -o file:foo-aa cat split-aa
	atf_check -o file:foo-ab cat split-ab
	atf_check -o file:foo-ac cat split-ac

	# Try again, make sure that `-` uses stdin as documented.
	atf_check rm split-*
	atf_check -x 'split -l 1 - split- < foo'
	atf_check -o file:foo-aa cat split-aa
	atf_check -o file:foo-ab cat split-ab
	atf_check -o file:foo-ac cat split-ac

	# Finally, try with -l == 2; we should see a 2/1 split instead of the
	# previous 1/1/1.
	cat foo-aa foo-ab > foo-aa-ng
	cat foo-ac > foo-ab-ng

	atf_check rm split-*
	atf_check split -l 2 foo split-

	atf_check -o file:foo-aa-ng cat split-aa
	atf_check -o file:foo-ab-ng cat split-ab
}

atf_test_case long_lines
long_lines_body()
{

	# Test file lines will be:
	# a x MAXBSIZE
	# b x MAXBSIZE + c x MAXBSIZE
	# d x 1024
	#
	# The historical split(1) implementation wouldn't grow its internal
	# buffer, so we'd end up with 2/3 split- files being wrong with -l 1.
	# Notably, split-aa would include most of the first two lines, split-ab
	# a tiny fraction of the second line, and split-ac the third line.
	#
	# Recent split(1) instead grows the buffer until we can either fit the
	# line or we run out of memory.
	jot -s "" -b "a" ${MAXBSIZE} > foo-aa
	jot -ns "" -b "b" ${MAXBSIZE} > foo-ab
	jot -s "" -b "c" ${MAXBSIZE} >> foo-ab
	jot -s "" -b "d" 1024 > foo-ac

	cat foo-* > foo
	atf_check split -l 1 foo split-

	atf_check -o file:foo-aa cat split-aa
	atf_check -o file:foo-ab cat split-ab
	atf_check -o file:foo-ac cat split-ac
}

atf_test_case numeric_suffix
numeric_suffix_body()
{
	echo "The quick brown fox" > foo-00
	echo "jumps over" > foo-01
	echo "the lazy dog" > foo-02

	cat foo-* > foo
	atf_check split -d -l 1 foo split-

	atf_check -o file:foo-00 cat split-00
	atf_check -o file:foo-01 cat split-01
	atf_check -o file:foo-02 cat split-02
}

atf_test_case larger_suffix_length
larger_suffix_length_body()
{
	:> foo

	# Generate foo-000 through foo-009, then foo-010 and foo-011
	for i in $(seq -w 0 11); do
		len=$((${i##0} + 1))
		file="foo-0${i}"
		jot -s "" -b "a" ${len} > ${file}
		cat ${file} >> foo
	done

	atf_check split -a 3 -d -l 1 foo split-
	for i in $(seq -w 0 11); do
		srcfile="foo-0${i}"
		splitfile="split-0${i}"
		atf_check -o file:"${srcfile}" cat "${splitfile}"
	done
}

atf_test_case pattern
pattern_body()
{

	# Some fake yaml gives us a good realistic use-case for -p, as we can
	# split on top-level stanzas.
	cat <<EOF > foo-aa
cat:
  aa: true
  ab: true
  ac: true
EOF
	cat <<EOF > foo-ab
dog:
  ba: true
  bb: true
  bc: true
EOF

	cat foo-* > foo

	atf_check split -p "^[^[:space:]]+:" foo split-
	atf_check -o file:foo-aa cat split-aa
	atf_check -o file:foo-ab cat split-ab
}

atf_init_test_cases()
{
	atf_add_test_case bytes
	atf_add_test_case chunks
	atf_add_test_case sensible_lines
	atf_add_test_case long_lines
	atf_add_test_case numeric_suffix
	atf_add_test_case larger_suffix_length
	atf_add_test_case pattern
}
