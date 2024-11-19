#
# Copyright (c) 2007 Diomidis Spinellis
# Copyright (c) 2023 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

mv_setup() {
	atf_check mkdir fs
	atf_check mount -t tmpfs -o size=1m tmp fs
}

mv_cleanup() {
	umount fs || true
}

# Make a file that can later be verified
mv_makefile() {
	local cn="${1##*/}"
	echo "$cn-$cn" >"$1"
}

# Verify that the file specified is correct
mv_checkfile() {
	atf_check -o inline:"$1-$1\n" cat "$2"
}

# Make a fifo that can later be verified
mv_makepipe() {
	atf_check mkfifo $1
}

# Verify that the file specified is correct
mv_checkpipe() {
	atf_check test -p "$2"
}

# Make a directory that can later be verified
mv_makedir() {
	local cn="${1##*/}"
	atf_check mkdir -p "$1/$cn-$cn"
}

# Verify that the directory specified is correct
mv_checkdir() {
	atf_check test -d "$2/$1-$1"
}

# Verify that the specified file does not exist
# (is not there)
mv_checkabsent() {
	atf_check -s exit:1 test -r "$1"
}

atf_test_case rename_file cleanup
rename_file_head() {
	atf_set "descr" "Rename file"
	atf_set "require.user" "root"
}
rename_file_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makefile fa
		atf_check mv fa ${FS}fb
		mv_checkfile fa ${FS}fb
		mv_checkabsent fa
	done
}
rename_file_cleanup() {
	mv_cleanup
}

atf_test_case file_into_dir cleanup
file_into_dir_head() {
	atf_set "descr" "Move files into directory"
	atf_set "require.user" "root"
}
file_into_dir_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makefile fa
		mv_makefile fb
		atf_check mkdir -p ${FS}1/2/3
		atf_check mv fa fb ${FS}1/2/3
		mv_checkfile fa ${FS}1/2/3/fa
		mv_checkfile fb ${FS}1/2/3/fb
		mv_checkabsent fa
		mv_checkabsent fb
	done
}
file_into_dir_cleanup() {
	mv_cleanup
}

atf_test_case file_from_dir cleanup
file_from_dir_head() {
	atf_set "descr" "Move file from directory to file"
	atf_set "require.user" "root"
}
file_from_dir_body() {
	mv_setup
	atf_check mkdir -p 1/2/3
	for FS in "" "fs/" ; do
		mv_makefile 1/2/3/fa
		atf_check mv 1/2/3/fa ${FS}fb
		mv_checkfile fa ${FS}fb
		mv_checkabsent 1/2/3/fa
	done
}
file_from_dir_cleanup() {
	mv_cleanup
}

atf_test_case file_from_dir_replace cleanup
file_from_dir_replace_head() {
	atf_set "descr" "Move file from directory to existing file"
	atf_set "require.user" "root"
}
file_from_dir_replace_body() {
	mv_setup
	atf_check mkdir -p 1/2/3
	for FS in "" "fs/" ; do
		mv_makefile 1/2/3/fa
		:> ${FS}fb
		atf_check mv 1/2/3/fa ${FS}fb
		mv_checkfile fa ${FS}fb
		mv_checkabsent 1/2/3/fa
	done
}
file_from_dir_replace_cleanup() {
	mv_cleanup
}

atf_test_case file_to_dir cleanup
file_to_dir_head() {
	atf_set "descr" "Move file from directory to existing directory"
	atf_set "require.user" "root"
}
file_to_dir_body() {
	mv_setup
	atf_check mkdir -p 1/2/3
	for FS in "" "fs/" ; do
		mv_makefile 1/2/3/fa
		atf_check mkdir -p ${FS}db/fa
		# Should fail per POSIX step 3a:
		# Destination path is a file of type directory and
		# source_file is not a file of type directory
		atf_check -s not-exit:0 -e match:"Is a directory" \
		      mv 1/2/3/fa ${FS}db
		mv_checkfile fa 1/2/3/fa
	done
}
file_to_dir_cleanup() {
	mv_cleanup
}

atf_test_case file_from_rename_dir cleanup
file_from_rename_dir_head() {
	atf_set "descr" "Move file from directory to directory"
	atf_set "require.user" "root"
}
file_from_rename_dir_body() {
	mv_setup
	atf_check mkdir -p da1/da2/da3
	for FS in "" "fs/" ; do
		atf_check mkdir -p ${FS}db1/db2/db3
		mv_makefile da1/da2/da3/fa
		atf_check mv da1/da2/da3/fa ${FS}db1/db2/db3/fb
		mv_checkfile fa ${FS}db1/db2/db3/fb
		mv_checkabsent da1/da2/da3/fa
	done
}
file_from_rename_dir_cleanup() {
	mv_cleanup
}

atf_test_case rename_dir cleanup
rename_dir_head() {
	atf_set "descr" "Rename directory"
	atf_set "require.user" "root"
}
rename_dir_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makedir da
		atf_check mv da ${FS}db
		mv_checkdir da ${FS}db
		mv_checkabsent da
	done
}
rename_dir_cleanup() {
	mv_cleanup
}

atf_test_case dir_to_dir cleanup
dir_to_dir_head() {
	atf_set "descr" "Move directory to directory name"
	atf_set "require.user" "root"
}
dir_to_dir_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makedir da1/da2/da3/da
		atf_check mkdir -p ${FS}db1/db2/db3
		atf_check mv da1/da2/da3/da ${FS}db1/db2/db3/db
		mv_checkdir da ${FS}db1/db2/db3/db
		mv_checkabsent da1/da2/da3/da
	done
}
dir_to_dir_cleanup() {
	mv_cleanup
}

atf_test_case dir_into_dir cleanup
dir_into_dir_head() {
	atf_set "descr" "Move directory to directory"
	atf_set "require.user" "root"
}
dir_into_dir_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makedir da1/da2/da3/da
		atf_check mkdir -p ${FS}db1/db2/db3
		atf_check mv da1/da2/da3/da ${FS}db1/db2/db3
		mv_checkdir da ${FS}db1/db2/db3/da
		mv_checkabsent da1/da2/da3/da
	done
}
dir_into_dir_cleanup() {
	mv_cleanup
}

atf_test_case dir_to_empty_dir cleanup
dir_to_empty_dir_head() {
	atf_set "descr" "Move directory to existing empty directory"
	atf_set "require.user" "root"
}
dir_to_empty_dir_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makedir da1/da2/da3/da
		atf_check mkdir -p ${FS}db1/db2/db3/da
		atf_check mv da1/da2/da3/da ${FS}db1/db2/db3
		mv_checkdir da ${FS}db1/db2/db3/da
		mv_checkabsent da1/da2/da3/da
	done
}
dir_to_empty_dir_cleanup() {
	mv_cleanup
}

atf_test_case dir_to_nonempty_dir cleanup
dir_to_nonempty_dir_head() {
	atf_set "descr" "Move directory to existing non-empty directory"
	atf_set "require.user" "root"
}
dir_to_nonempty_dir_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makedir da1/da2/da3/da
		atf_check mkdir -p ${FS}db1/db2/db3/da/full
		# Should fail (per the semantics of rename(2))
		atf_check -s not-exit:0 -e match:"Directory not empty" \
		      mv da1/da2/da3/da ${FS}db1/db2/db3
		mv_checkdir da da1/da2/da3/da
	done
}
dir_to_nonempty_dir_cleanup() {
	mv_cleanup
}

atf_test_case dir_to_file cleanup
dir_to_file_head() {
	atf_set "descr" "Move directory to existing file"
	atf_set "require.user" "root"
}
dir_to_file_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makedir da1/da2/da3/da
		atf_check mkdir -p ${FS}db1/db2/db3
		:> ${FS}db1/db2/db3/da
		# Should fail per POSIX step 3b:
		# Destination path is a file not of type directory
		# and source_file is a file of type directory
		atf_check -s not-exit:0 -e match:"Not a directory" \
		      mv da1/da2/da3/da ${FS}db1/db2/db3/da
		mv_checkdir da da1/da2/da3/da
	done
}
dir_to_file_cleanup() {
	mv_cleanup
}

atf_test_case rename_fifo cleanup
rename_fifo_head() {
	atf_set "descr" "Rename fifo"
	atf_set "require.user" "root"
}
rename_fifo_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makepipe fa
		atf_check mv fa ${FS}fb
		mv_checkpipe fa ${FS}fb
		mv_checkabsent fa
	done
}
rename_fifo_cleanup() {
	mv_cleanup
}

atf_test_case fifo_into_dir cleanup
fifo_into_dir_head() {
	atf_set "descr" "Move fifos into directory"
	atf_set "require.user" "root"
}
fifo_into_dir_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makepipe fa
		mv_makepipe fb
		atf_check mkdir -p ${FS}1/2/3
		atf_check mv fa fb ${FS}1/2/3
		mv_checkpipe fa ${FS}1/2/3/fa
		mv_checkpipe fb ${FS}1/2/3/fb
		mv_checkabsent fa
		mv_checkabsent fb
	done
}
fifo_into_dir_cleanup() {
	mv_cleanup
}

atf_test_case fifo_from_dir cleanup
fifo_from_dir_head() {
	atf_set "descr" "Move fifo from directory to fifo"
	atf_set "require.user" "root"
}
fifo_from_dir_body() {
	mv_setup
	atf_check mkdir -p 1/2/3
	for FS in "" "fs/" ; do
		mv_makepipe 1/2/3/fa
		atf_check mv 1/2/3/fa ${FS}fb
		mv_checkpipe fa ${FS}fb
		mv_checkabsent 1/2/3/fa
	done
}
fifo_from_dir_cleanup() {
	mv_cleanup
}

atf_test_case fifo_from_dir_into_dir cleanup
fifo_from_dir_into_dir_head() {
	atf_set "descr" "Move fifo from directory to directory"
	atf_set "require.user" "root"
}
fifo_from_dir_into_dir_body() {
	mv_setup
	atf_check mkdir -p da1/da2/da3
	for FS in "" "fs/" ; do
		atf_check mkdir -p ${FS}db1/db2/db3
		mv_makepipe da1/da2/da3/fa
		atf_check mv da1/da2/da3/fa ${FS}db1/db2/db3/fb
		mv_checkpipe fa ${FS}db1/db2/db3/fb
		mv_checkabsent da1/da2/da3/fa
	done
}
fifo_from_dir_into_dir_cleanup() {
	mv_cleanup
}

atf_test_case mv_f cleanup
mv_f_head() {
	atf_set "descr" "Force replacement"
	atf_set "require.user" "root"
}
mv_f_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makefile fa
		atf_check mv fa ${FS}fb
		mv_checkfile fa ${FS}fb
		mv_checkabsent fa
		mv_makefile fa
		:> ${FS}fb
		atf_check mv -i -n -f fa ${FS}fb
		mv_checkfile fa ${FS}fb
		mv_checkabsent fa
	done
}
mv_f_cleanup() {
	mv_cleanup
}

atf_test_case mv_h cleanup
mv_h_head() {
	atf_set "descr" "Replace symbolic link"
	atf_set "require.user" "root"
}
mv_h_body() {
	mv_setup
	for FS in "" "fs/" ; do
		atf_check mkdir ${FS}da
		atf_check ln -s da ${FS}db
		# First test without -h, file goes into symlink target
		mv_makefile fa
		atf_check mv fa ${FS}db
		mv_checkfile fa ${FS}da/fa
		# Second test with -h, file replaces symlink
		mv_makefile fa
		atf_check mv -h fa ${FS}db
		mv_checkfile fa ${FS}db
	done
}
mv_h_cleanup() {
	mv_cleanup
}

atf_test_case mv_i cleanup
mv_i_head() {
	atf_set "descr" "Confirm replacement"
	atf_set "require.user" "root"
}
mv_i_body() {
	mv_setup
	echo n >n
	echo y >y
	for FS in "" "fs/" ; do
		mv_makefile fa
		mv_makefile ${FS}fb
		# First test, answer no, file is not replaced
		atf_check -e match:"^overwrite ${FS}fb\\?" \
		      mv -i fa ${FS}fb <n
		mv_checkfile fa fa
		mv_checkfile fb ${FS}fb
		# Second test, answer yes, file is replaced
		atf_check -e match:"^overwrite ${FS}fb\\?" \
		      mv -i fa ${FS}fb <y
		mv_checkabsent fa
		mv_checkfile fa ${FS}fb
	done
}
mv_i_cleanup() {
	mv_cleanup
}

atf_test_case mv_n cleanup
mv_n_head() {
	atf_set "descr" "Decline replacement"
	atf_set "require.user" "root"
}
mv_n_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makefile fa
		mv_makefile ${FS}fb
		atf_check mv -n fa ${FS}fb
		mv_checkfile fa fa
		mv_checkfile fb ${FS}fb
	done
}
mv_n_cleanup() {
	mv_cleanup
}

atf_test_case mv_v cleanup
mv_v_head() {
	atf_set "descr" "Verbose mode"
	atf_set "require.user" "root"
}
mv_v_body() {
	mv_setup
	for FS in "" "fs/" ; do
		mv_makefile fa
		atf_check mkdir ${FS}da
		atf_check -o inline:"fa -> ${FS}da/fa\n" \
		      mv -v fa ${FS}da
	done
}
mv_v_cleanup() {
	mv_cleanup
}

atf_init_test_cases() {
	atf_add_test_case rename_file
	atf_add_test_case file_into_dir
	atf_add_test_case file_from_dir
	atf_add_test_case file_from_dir_replace
	atf_add_test_case file_to_dir
	atf_add_test_case file_from_rename_dir
	atf_add_test_case rename_dir
	atf_add_test_case dir_to_dir
	atf_add_test_case dir_into_dir
	atf_add_test_case dir_to_empty_dir
	atf_add_test_case dir_to_nonempty_dir
	atf_add_test_case dir_to_file
	atf_add_test_case rename_fifo
	atf_add_test_case fifo_into_dir
	atf_add_test_case fifo_from_dir
	atf_add_test_case fifo_from_dir_into_dir
	atf_add_test_case mv_f
	atf_add_test_case mv_h
	atf_add_test_case mv_i
	atf_add_test_case mv_n
	atf_add_test_case mv_v
}
