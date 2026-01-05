#
# Copyright 2015 EMC Corp.
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

KB=1024
: ${TMPDIR=/tmp}
DEFAULT_MTREE_KEYWORDS="type,mode,gid,uid,size,link,time"
MTREE="mtree -k $DEFAULT_MTREE_KEYWORDS"
TEST_IMAGE="$TMPDIR/test.img"
TEST_INPUTS_DIR="$TMPDIR/inputs"
TEST_MD_DEVICE_FILE="$TMPDIR/md.output"
TEST_MOUNT_DIR="$TMPDIR/mnt"
TEST_SPEC_FILE="$TMPDIR/mtree.spec"

check_image_contents()
{
	local directories=$TEST_INPUTS_DIR
	local excludes mtree_excludes_arg mtree_file
	local mtree_keywords="$DEFAULT_MTREE_KEYWORDS"

	while getopts "d:f:m:X:" flag; do
		case "$flag" in
		d)
			directories="$directories $OPTARG"
			;;
		f)
			mtree_file=$OPTARG
			;;
		m)
			mtree_keywords=$OPTARG
			;;
		X)
			excludes="$excludes $OPTARG"
			;;
		*)
			echo "usage: check_image_contents [-d directory ...] [-f mtree-file] [-m mtree-keywords] [-X exclude]"
			atf_fail "unhandled option: $flag"
			;;
		esac
	done

	if [ -n "$excludes" ]; then
		echo "$excludes" | tr ' ' '\n' > excludes.txt
		mtree_excludes_arg="-X excludes.txt"
	fi

	if [ -z "$mtree_file" ]; then
		mtree_file=input_spec.mtree
		for directory in $directories; do
			mtree -c -k $mtree_keywords -p $directory $mtree_excludes_arg
		done > $mtree_file
	fi

	echo "<---- Input spec BEGIN ---->"
	cat $mtree_file
	echo "<---- Input spec END ---->"
	atf_check mtree -UW -f $mtree_file \
	    -p $TEST_MOUNT_DIR $mtree_excludes_arg
}

create_manifest_file()
{
	# Prefer a specification format that is easier to parse (-C)
	mtree -k "$DEFAULT_MTREE_KEYWORDS" -cp "$TEST_INPUTS_DIR" | \
	    mtree -C > "$TEST_SPEC_FILE"
	sed -i "" "s|^\.|.${TEST_INPUTS_DIR##$TMPDIR}|g" "$TEST_SPEC_FILE"
}

create_test_dirs()
{
	atf_check mkdir -m 0777 -p $TEST_MOUNT_DIR
	atf_check mkdir -m 0777 -p $TEST_INPUTS_DIR
}

create_test_inputs()
{
	create_test_dirs

	cd $TEST_INPUTS_DIR

	atf_check mkdir -m 0755 -p a/b/1
	atf_check ln -s a/b c
	atf_check touch d
	atf_check ln d e
	atf_check touch .f
	atf_check mkdir .g
	# XXX: fifos on the filesystem don't match fifos created by makefs for
	# some odd reason.
	#atf_check mkfifo h
	atf_check -e ignore dd if=/dev/zero of=i count=1000 bs=1
	atf_check touch klmn
	atf_check touch opqr
	atf_check touch stuv
	atf_check install -m 0755 /dev/null wxyz
	atf_check touch 0b00000001
	atf_check touch 0b00000010
	atf_check touch 0b00000011
	atf_check touch 0b00000100
	atf_check touch 0b00000101
	atf_check touch 0b00000110
	atf_check touch 0b00000111
	atf_check touch 0b00001000
	atf_check touch 0b00001001
	atf_check touch 0b00001010
	atf_check touch 0b00001011
	atf_check touch 0b00001100
	atf_check touch 0b00001101
	atf_check touch 0b00001110

	for filesize in 1 512 $(( 2 * $KB )) $(( 10 * $KB )) $(( 512 * $KB )); \
	do
		atf_check -e ignore dd if=/dev/zero of=${filesize}.file bs=1 \
		    count=${filesize} conv=sparse
		files="${files} ${filesize}.file"
	done

	cd -
}

mount_image()
{
	atf_check -o save:$TEST_MD_DEVICE_FILE mdconfig -a -f $TEST_IMAGE
	atf_check $MOUNT ${1} /dev/$(cat $TEST_MD_DEVICE_FILE) $TEST_MOUNT_DIR
}

change_mtree_timestamp()
{
	filename="$1"
	timestamp="$2"

	sed -i "" "s/time=.*$/time=${timestamp}.0/g" "$filename"
}
