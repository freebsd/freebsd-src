#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022-2023 The FreeBSD Foundation
#
# This software was developed by Mark Johnston under sponsorship from
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

MAKEFS="makefs -t zfs -o nowarn=true"
ZFS_POOL_NAME="makefstest$$"
TEST_ZFS_POOL_NAME="$TMPDIR/poolname"

. "$(dirname "$0")/makefs_tests_common.sh"

common_cleanup()
{
	local pool md

	# Try to force a TXG, this can help catch bugs by triggering a panic.
	sync

	pool=$(cat $TEST_ZFS_POOL_NAME)
	if zpool list "$pool" >/dev/null; then
		zpool destroy "$pool"
	fi

	md=$(cat $TEST_MD_DEVICE_FILE)
	if [ -c /dev/"$md" ]; then
		mdconfig -d -u "$md"
	fi
}

import_image()
{
	atf_check -e empty -o save:$TEST_MD_DEVICE_FILE -s exit:0 \
	    mdconfig -a -f $TEST_IMAGE
	atf_check -o ignore -e empty -s exit:0 \
	    zdb -e -p /dev/$(cat $TEST_MD_DEVICE_FILE) -mmm -ddddd $ZFS_POOL_NAME
	atf_check zpool import -R $TEST_MOUNT_DIR $ZFS_POOL_NAME
	echo "$ZFS_POOL_NAME" > $TEST_ZFS_POOL_NAME
}

#
# Test autoexpansion of the vdev.
#
# The pool is initially 10GB, so we get 10GB minus one metaslab's worth of
# usable space for data.  Then the pool is expanded to 50GB, and the amount of
# usable space is 50GB minus one metaslab.
#
atf_test_case autoexpand cleanup
autoexpand_body()
{
	local mssize poolsize poolsize1 newpoolsize

	create_test_inputs

	mssize=$((128 * 1024 * 1024))
	poolsize=$((10 * 1024 * 1024 * 1024))
	atf_check $MAKEFS -s $poolsize -o mssize=$mssize -o rootpath=/ \
	    -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	newpoolsize=$((50 * 1024 * 1024 * 1024))
	truncate -s $newpoolsize $TEST_IMAGE

	import_image

	check_image_contents

	poolsize1=$(zpool list -Hp -o size $ZFS_POOL_NAME)
	atf_check [ $((poolsize1 + $mssize)) -eq $poolsize ]

	atf_check zpool online -e $ZFS_POOL_NAME /dev/$(cat $TEST_MD_DEVICE_FILE)

	check_image_contents

	poolsize1=$(zpool list -Hp -o size $ZFS_POOL_NAME)
	atf_check [ $((poolsize1 + $mssize)) -eq $newpoolsize ]
}
autoexpand_cleanup()
{
	common_cleanup
}

#
# Test with some default layout defined by the common code.
#
atf_test_case basic cleanup
basic_body()
{
	create_test_inputs

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents
}
basic_cleanup()
{
	common_cleanup
}

atf_test_case dataset_removal cleanup
dataset_removal_body()
{
	create_test_dirs

	cd $TEST_INPUTS_DIR
	mkdir dir
	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    -o fs=${ZFS_POOL_NAME}/dir \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents

	atf_check zfs destroy ${ZFS_POOL_NAME}/dir
}
dataset_removal_cleanup()
{
	common_cleanup
}

#
# Make sure that we can create and remove an empty directory.
#
atf_test_case empty_dir cleanup
empty_dir_body()
{
	create_test_dirs

	cd $TEST_INPUTS_DIR
	mkdir dir
	cd -

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents

	atf_check rmdir ${TEST_MOUNT_DIR}/dir
}
empty_dir_cleanup()
{
	common_cleanup
}

atf_test_case empty_fs cleanup
empty_fs_body()
{
	create_test_dirs

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents
}
empty_fs_cleanup()
{
	common_cleanup
}

atf_test_case file_extend cleanup
file_extend_body()
{
	local i start

	create_test_dirs

	# Create a file slightly longer than the maximum block size.
	start=132
	dd if=/dev/random of=${TEST_INPUTS_DIR}/foo bs=1k count=$start
	md5 -q ${TEST_INPUTS_DIR}/foo > foo.md5

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents

	i=0
	while [ $i -lt 1000 ]; do
		dd if=/dev/random of=${TEST_MOUNT_DIR}/foo bs=1k count=1 \
		    seek=$(($i + $start)) conv=notrunc
		# Make sure that the first $start blocks are unmodified.
		dd if=${TEST_MOUNT_DIR}/foo bs=1k count=$start of=foo.copy
		atf_check -o file:foo.md5 md5 -q foo.copy
		i=$(($i + 1))
	done
}
file_extend_cleanup()
{
	common_cleanup
}

atf_test_case file_sizes cleanup
file_sizes_body()
{
	local i

	create_test_dirs
	cd $TEST_INPUTS_DIR

	i=1
	while [ $i -lt $((1 << 20)) ]; do
		truncate -s $i ${i}.1
		truncate -s $(($i - 1)) ${i}.2
		truncate -s $(($i + 1)) ${i}.3
		i=$(($i << 1))
	done

	cd -

	# XXXMJ this creates sparse files, make sure makefs doesn't
	#       preserve the sparseness.
	# XXXMJ need to test with larger files (at least 128MB for L2 indirs)
	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents
}
file_sizes_cleanup()
{
	common_cleanup
}

atf_test_case hard_links cleanup
hard_links_body()
{
	local f

	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir
	echo "hello" > 1
	ln 1 2
	ln 1 dir/1

	echo "goodbye" > dir/a
	ln dir/a dir/b
	ln dir/a a

	cd -

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents

	stat -f '%i' ${TEST_MOUNT_DIR}/1 > ./ino
	stat -f '%l' ${TEST_MOUNT_DIR}/1 > ./nlink
	for f in 1 2 dir/1; do
		atf_check -o file:./nlink -e empty -s exit:0 \
		    stat -f '%l' ${TEST_MOUNT_DIR}/${f}
		atf_check -o file:./ino -e empty -s exit:0 \
		    stat -f '%i' ${TEST_MOUNT_DIR}/${f}
		atf_check cmp -s ${TEST_INPUTS_DIR}/1 ${TEST_MOUNT_DIR}/${f}
	done

	stat -f '%i' ${TEST_MOUNT_DIR}/dir/a > ./ino
	stat -f '%l' ${TEST_MOUNT_DIR}/dir/a > ./nlink
	for f in dir/a dir/b a; do
		atf_check -o file:./nlink -e empty -s exit:0 \
		    stat -f '%l' ${TEST_MOUNT_DIR}/${f}
		atf_check -o file:./ino -e empty -s exit:0 \
		    stat -f '%i' ${TEST_MOUNT_DIR}/${f}
		atf_check cmp -s ${TEST_INPUTS_DIR}/dir/a ${TEST_MOUNT_DIR}/${f}
	done
}
hard_links_cleanup()
{
	common_cleanup
}

# Allocate enough dnodes from an object set that the meta dnode needs to use
# indirect blocks.
atf_test_case indirect_dnode_array cleanup
indirect_dnode_array_body()
{
	local count i

	# How many dnodes do we need to allocate?  Well, the data block size
	# for meta dnodes is always 16KB, so with a dnode size of 512B we get
	# 32 dnodes per direct block.  The maximum indirect block size is 128KB
	# and that can fit 1024 block pointers, so we need at least 32 * 1024
	# files to force the use of two levels of indirection.
	#
	# Unfortunately that number of files makes the test run quite slowly,
	# so we settle for a single indirect block for now...
	count=$(jot -r 1 32 1024)

	create_test_dirs
	cd $TEST_INPUTS_DIR
	for i in $(seq 1 $count); do
		touch $i
	done
	cd -

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents
}
indirect_dnode_array_cleanup()
{
	common_cleanup
}

#
# Create some files with long names, so as to test fat ZAP handling.
#
atf_test_case long_file_name cleanup
long_file_name_body()
{
	local dir i

	create_test_dirs
	cd $TEST_INPUTS_DIR

	# micro ZAP keys can be at most 50 bytes.
	for i in $(seq 1 60); do
		touch $(jot -s '' $i 1 1)
	done
	dir=$(jot -s '' 61 1 1)
	mkdir $dir
	for i in $(seq 1 60); do
		touch ${dir}/$(jot -s '' $i 1 1)
	done

	cd -

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents

	# Add a directory entry in the hope that OpenZFS might catch a bug
	# in makefs' fat ZAP encoding.
	touch ${TEST_MOUNT_DIR}/foo
}
long_file_name_cleanup()
{
	common_cleanup
}

#
# Exercise handling of multiple datasets.
#
atf_test_case multi_dataset_1 cleanup
multi_dataset_1_body()
{
	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir1
	echo a > dir1/a
	mkdir dir2
	echo b > dir2/b

	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    -o fs=${ZFS_POOL_NAME}/dir1 -o fs=${ZFS_POOL_NAME}/dir2 \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents

	# Make sure that we have three datasets with the expected mount points.
	atf_check -o inline:${ZFS_POOL_NAME}\\n -e empty -s exit:0 \
	    zfs list -H -o name ${ZFS_POOL_NAME}
	atf_check -o inline:${TEST_MOUNT_DIR}\\n -e empty -s exit:0 \
	    zfs list -H -o mountpoint ${ZFS_POOL_NAME}

	atf_check -o inline:${ZFS_POOL_NAME}/dir1\\n -e empty -s exit:0 \
	    zfs list -H -o name ${ZFS_POOL_NAME}/dir1
	atf_check -o inline:${TEST_MOUNT_DIR}/dir1\\n -e empty -s exit:0 \
	    zfs list -H -o mountpoint ${ZFS_POOL_NAME}/dir1

	atf_check -o inline:${ZFS_POOL_NAME}/dir2\\n -e empty -s exit:0 \
	    zfs list -H -o name ${ZFS_POOL_NAME}/dir2
	atf_check -o inline:${TEST_MOUNT_DIR}/dir2\\n -e empty -s exit:0 \
	    zfs list -H -o mountpoint ${ZFS_POOL_NAME}/dir2
}
multi_dataset_1_cleanup()
{
	common_cleanup
}

#
# Create a pool with two datasets, where the root dataset is mounted below
# the child dataset.
#
atf_test_case multi_dataset_2 cleanup
multi_dataset_2_body()
{
	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir1
	echo a > dir1/a
	mkdir dir2
	echo b > dir2/b

	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    -o fs=${ZFS_POOL_NAME}/dir1\;mountpoint=/ \
	    -o fs=${ZFS_POOL_NAME}\;mountpoint=/dir1 \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents
}
multi_dataset_2_cleanup()
{
	common_cleanup
}

#
# Create a dataset with a non-existent mount point.
#
atf_test_case multi_dataset_3 cleanup
multi_dataset_3_body()
{
	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir1
	echo a > dir1/a

	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    -o fs=${ZFS_POOL_NAME}/dir1 \
	    -o fs=${ZFS_POOL_NAME}/dir2 \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	atf_check -o inline:${TEST_MOUNT_DIR}/dir2\\n -e empty -s exit:0 \
	    zfs list -H -o mountpoint ${ZFS_POOL_NAME}/dir2

	# Mounting dir2 should have created a directory called dir2.  Go
	# back and create it in the staging tree before comparing.
	atf_check mkdir ${TEST_INPUTS_DIR}/dir2

	check_image_contents
}
multi_dataset_3_cleanup()
{
	common_cleanup
}

#
# Create an unmounted dataset.
#
atf_test_case multi_dataset_4 cleanup
multi_dataset_4_body()
{
	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir1
	echo a > dir1/a

	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    -o fs=${ZFS_POOL_NAME}/dir1\;canmount=noauto\;mountpoint=none \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	atf_check -o inline:none\\n -e empty -s exit:0 \
	    zfs list -H -o mountpoint ${ZFS_POOL_NAME}/dir1

	check_image_contents

	atf_check zfs set mountpoint=/dir1 ${ZFS_POOL_NAME}/dir1
	atf_check zfs mount ${ZFS_POOL_NAME}/dir1
	atf_check -o inline:${TEST_MOUNT_DIR}/dir1\\n -e empty -s exit:0 \
	    zfs list -H -o mountpoint ${ZFS_POOL_NAME}/dir1

	# dir1/a should be part of the root dataset, not dir1.
	atf_check -s not-exit:0 -e not-empty stat ${TEST_MOUNT_DIR}dir1/a
}
multi_dataset_4_cleanup()
{
	common_cleanup
}

#
# Validate handling of multiple staging directories.
#
atf_test_case multi_staging_1 cleanup
multi_staging_1_body()
{
	local tmpdir

	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir1
	echo a > a
	echo a > dir1/a
	echo z > z

	cd -

	tmpdir=$(mktemp -d)
	cd $tmpdir

	mkdir dir2 dir2/dir3
	echo b > dir2/b
	echo c > dir2/dir3/c
	ln -s dir2/dir3c s

	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE ${TEST_INPUTS_DIR} $tmpdir

	import_image

	check_image_contents -d $tmpdir
}
multi_staging_1_cleanup()
{
	common_cleanup
}

atf_test_case multi_staging_2 cleanup
multi_staging_2_body()
{
	local tmpdir

	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir
	echo a > dir/foo
	echo b > dir/bar

	cd -

	tmpdir=$(mktemp -d)
	cd $tmpdir

	mkdir dir
	echo c > dir/baz

	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE ${TEST_INPUTS_DIR} $tmpdir

	import_image

	# check_image_contents can't easily handle merged directories, so
	# just check that the merged directory contains the files we expect.
	atf_check -o not-empty stat ${TEST_MOUNT_DIR}/dir/foo
	atf_check -o not-empty stat ${TEST_MOUNT_DIR}/dir/bar
	atf_check -o not-empty stat ${TEST_MOUNT_DIR}/dir/baz

	if [ "$(ls ${TEST_MOUNT_DIR}/dir | wc -l)" -ne 3 ]; then
		atf_fail "Expected 3 files in ${TEST_MOUNT_DIR}/dir"
	fi
}
multi_staging_2_cleanup()
{
	common_cleanup
}

#
# Rudimentary test to verify that two ZFS images created using the same
# parameters and input hierarchy are byte-identical.  In particular, makefs(1)
# does not preserve file access times.
#
atf_test_case reproducible cleanup
reproducible_body()
{
	create_test_inputs

	atf_check $MAKEFS -s 512m -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    ${TEST_IMAGE}.1 $TEST_INPUTS_DIR

	atf_check $MAKEFS -s 512m -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    ${TEST_IMAGE}.2 $TEST_INPUTS_DIR

	# XXX-MJ cmp(1) is really slow
	atf_check cmp ${TEST_IMAGE}.1 ${TEST_IMAGE}.2
}
reproducible_cleanup()
{
}

#
# Verify that we can take a snapshot of a generated dataset.
#
atf_test_case snapshot cleanup
snapshot_body()
{
	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir
	echo "hello" > dir/hello
	echo "goodbye" > goodbye

	cd -

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	atf_check zfs snapshot ${ZFS_POOL_NAME}@1
}
snapshot_cleanup()
{
	common_cleanup
}

#
# Check handling of symbolic links.
#
atf_test_case soft_links cleanup
soft_links_body()
{
	create_test_dirs
	cd $TEST_INPUTS_DIR

	mkdir dir
	ln -s a a
	ln -s dir/../a a
	ln -s dir/b b
	echo 'c' > dir
	ln -s dir/c c
	# XXX-MJ overflows bonus buffer ln -s $(jot -s '' 320 1 1) 1

	cd -

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents
}
soft_links_cleanup()
{
	common_cleanup
}

#
# Verify that we can set properties on the root dataset.
#
atf_test_case root_props cleanup
root_props_body()
{
	create_test_inputs

	atf_check $MAKEFS -s 10g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    -o fs=${ZFS_POOL_NAME}\;atime=off\;setuid=off \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents

	atf_check -o inline:off\\n -e empty -s exit:0 \
	    zfs get -H -o value atime $ZFS_POOL_NAME
	atf_check -o inline:local\\n -e empty -s exit:0 \
	    zfs get -H -o source atime $ZFS_POOL_NAME
	atf_check -o inline:off\\n -e empty -s exit:0 \
	    zfs get -H -o value setuid $ZFS_POOL_NAME
	atf_check -o inline:local\\n -e empty -s exit:0 \
	    zfs get -H -o source setuid $ZFS_POOL_NAME
}
root_props_cleanup()
{
	common_cleanup
}

#
# Verify that usedds and usedchild props are set properly.
#
atf_test_case used_space_props cleanup
used_space_props_body()
{
	local used usedds usedchild
	local rootmb childmb totalmb fudge
	local status

	create_test_dirs
	cd $TEST_INPUTS_DIR
	mkdir dir

	rootmb=17
	childmb=39
	totalmb=$(($rootmb + $childmb))
	fudge=$((2 * 1024 * 1024))

	atf_check -e ignore dd if=/dev/random of=foo bs=1M count=$rootmb
	atf_check -e ignore dd if=/dev/random of=dir/bar bs=1M count=$childmb

	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    -o fs=${ZFS_POOL_NAME}/dir \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	# Make sure that each dataset's space usage is no more than 2MB larger
	# than their files.  This number is magic and might need to change
	# someday.
	usedds=$(zfs list -o usedds -Hp ${ZFS_POOL_NAME})
	atf_check test $usedds -gt $(($rootmb * 1024 * 1024)) -a \
	    $usedds -le $(($rootmb * 1024 * 1024 + $fudge))
	usedds=$(zfs list -o usedds -Hp ${ZFS_POOL_NAME}/dir)
	atf_check test $usedds -gt $(($childmb * 1024 * 1024)) -a \
	    $usedds -le $(($childmb * 1024 * 1024 + $fudge))

	# Make sure that the usedchild property value makes sense: the parent's
	# value corresponds to the size of the child, and the child has no
	# children.
	usedchild=$(zfs list -o usedchild -Hp ${ZFS_POOL_NAME})
	atf_check test $usedchild -gt $(($childmb * 1024 * 1024)) -a \
	    $usedchild -le $(($childmb * 1024 * 1024 + $fudge))
	atf_check -o inline:'0\n' \
	    zfs list -Hp -o usedchild ${ZFS_POOL_NAME}/dir

	# Make sure that the used property value makes sense: the parent's
	# value is the sum of the two sizes, and the child's value is the
	# same as its usedds value, which has already been checked.
	used=$(zfs list -o used -Hp ${ZFS_POOL_NAME})
	atf_check test $used -gt $(($totalmb * 1024 * 1024)) -a \
	    $used -le $(($totalmb * 1024 * 1024 + 2 * $fudge))
	used=$(zfs list -o used -Hp ${ZFS_POOL_NAME}/dir)
	atf_check -o inline:$used'\n' \
	    zfs list -Hp -o usedds ${ZFS_POOL_NAME}/dir

	# Both datasets do not have snapshots.
	atf_check -o inline:'0\n' zfs list -Hp -o usedsnap ${ZFS_POOL_NAME}
	atf_check -o inline:'0\n' zfs list -Hp -o usedsnap ${ZFS_POOL_NAME}/dir
}
used_space_props_cleanup()
{
	common_cleanup
}

# Verify that file permissions are set properly.  Make sure that non-executable
# files can't be executed.
atf_test_case perms cleanup
perms_body()
{
	local mode

	create_test_dirs
	cd $TEST_INPUTS_DIR

	for mode in $(seq 0 511); do
		mode=$(printf "%04o\n" $mode)
		echo 'echo a' > $mode
		atf_check chmod $mode $mode
	done

	cd -

	atf_check $MAKEFS -s 1g -o rootpath=/ -o poolname=$ZFS_POOL_NAME \
	    $TEST_IMAGE $TEST_INPUTS_DIR

	import_image

	check_image_contents

	for mode in $(seq 0 511); do
		mode=$(printf "%04o\n" $mode)
		if [ $(($mode & 0111)) -eq 0 ]; then
			atf_check -s not-exit:0 -e match:"Permission denied" \
			    ${TEST_INPUTS_DIR}/$mode
		fi
		if [ $(($mode & 0001)) -eq 0 ]; then
			atf_check -s not-exit:0 -e match:"Permission denied" \
			    su -m tests -c ${TEST_INPUTS_DIR}/$mode
		fi
	done

}
perms_cleanup()
{
	common_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case autoexpand
	atf_add_test_case basic
	atf_add_test_case dataset_removal
	atf_add_test_case empty_dir
	atf_add_test_case empty_fs
	atf_add_test_case file_extend
	atf_add_test_case file_sizes
	atf_add_test_case hard_links
	atf_add_test_case indirect_dnode_array
	atf_add_test_case long_file_name
	atf_add_test_case multi_dataset_1
	atf_add_test_case multi_dataset_2
	atf_add_test_case multi_dataset_3
	atf_add_test_case multi_dataset_4
	atf_add_test_case multi_staging_1
	atf_add_test_case multi_staging_2
	atf_add_test_case reproducible
	atf_add_test_case snapshot
	atf_add_test_case soft_links
	atf_add_test_case root_props
	atf_add_test_case used_space_props
	atf_add_test_case perms

	# XXXMJ tests:
	# - test with different ashifts (at least, 9 and 12), different image sizes
	# - create datasets in imported pool
}
