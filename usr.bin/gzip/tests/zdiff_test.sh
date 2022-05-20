#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Kyle Evans <kevans@FreeBSD.org>
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

specials="foo'bar foo\"bar foo\$bar"

prepare_files()
{
	compressfunc="$1"
	compresssuffix="$2"

	echo "foo" > foo

	for f in $specials foo; do
		[ "$f" == "foo" ] || cp foo "$f"
		atf_check "$compressfunc" -k "$f"
		atf_check tar -ckf "$f.tar" "$f"
		atf_check -o save:"$f.$compresssuffix" \
		    "$compressfunc" -c "$f.tar"

		# Regenerate $f.tar to create a diff from the .$compresssuffix
		# file, too.
		echo "bar" >> "$f"
		atf_check tar -ckf "$f.tar" "$f"
	done
}

atf_test_case gzip
gzip_body()
{
	prepare_files gzip tgz
	cp foo.gz foo.Z

	for f in foo $specials; do
		atf_check -s exit:1 -o file:"$(atf_get_srcdir)"/foo.diff \
		    zdiff "$f.gz"
	done

	atf_check -s exit:1 -o file:"$(atf_get_srcdir)"/foo.diff zdiff foo.Z

	for f in foo $specials; do
		rm "$f"
		atf_check -s exit:1 -o match:"Binary files" zdiff "$f.tgz"
	done
}

atf_test_case bzip
bzip_body()
{
	prepare_files bzip2 tbz2
	cp foo.bz2 foo.bz

	for f in foo $specials; do
		atf_check -s exit:1 -o file:"$(atf_get_srcdir)"/foo.diff \
		    zdiff "$f.bz2"
	done

	atf_check -s exit:1 -o file:"$(atf_get_srcdir)"/foo.diff zdiff foo.bz

	for f in foo $specials; do
		rm "$f"
		atf_check -s exit:1 -o match:"Binary files" zdiff "$f.tbz2"
	done
}

atf_test_case xzip
xzip_body()
{
	prepare_files xz txz
	cp foo.xz foo.lzma

	for f in foo $specials; do
		atf_check -s exit:1 -o file:"$(atf_get_srcdir)"/foo.diff \
		    zdiff "$f.xz"
	done

	atf_check -s exit:1 -o file:"$(atf_get_srcdir)"/foo.diff zdiff foo.lzma

	for f in foo $specials; do
		rm "$f"
		atf_check -s exit:1 -o match:"Binary files" zdiff "$f.txz"
	done
}

atf_test_case unknown
unknown_body()
{
	prepare_files xz fxz

	for f in foo $specials; do
		atf_check -s exit:1 -e match:"unknown suffix$" zdiff "$f.fxz"
	done
}

atf_init_test_cases()
{

	atf_add_test_case gzip
	atf_add_test_case bzip
	atf_add_test_case xzip
	atf_add_test_case unknown
}
