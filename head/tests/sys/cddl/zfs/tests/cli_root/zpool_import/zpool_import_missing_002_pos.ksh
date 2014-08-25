#!/usr/local/bin/ksh93 -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)zpool_import_missing_002_pos.ksh	1.4	07/10/09 SMI"
#
. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/cli_root/zfs_mount/zfs_mount.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_import_missing_002_pos
#
# DESCRIPTION:
# 	Once a pool has been exported, and one or more devices are 
#	move to other place, import should handle this kind of situation
#	as described:
#		- Regular, report error while any number of devices failing.
#		- Mirror could withstand (N-1) devices failing 
#		  before data integrity is compromised 
#		- Raidz could withstand one devices failing 
#		  before data integrity is compromised 
# 	Verify that is true.
#
# STRATEGY:
#	1. Create test pool upon device files using the various combinations.
#		- Regular pool
#		- Mirror
#		- Raidz
#	2. Create necessary filesystem and test files.
#	3. Export the test pool.
#	4. Move one or more device files to other directory 
#	5. Verify 'zpool import -d' with the new directory 
#	   will handle moved files successfullly.
#	   Using the various combinations.
#		- Regular import
#		- Alternate Root Specified
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2005-08-01)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

set -A vdevs "" "mirror" "raidz"
set -A options "" "-R $ALTER_ROOT"

function cleanup
{
	cd $DEVICE_DIR || log_fail "Unable change directory to $DEVICE_DIR"
	[[ -e $DEVICE_DIR/$DEVICE_ARCHIVE ]] && \
		log_must $TAR xf $DEVICE_DIR/$DEVICE_ARCHIVE
		
	poolexists $TESTPOOL1 || \
		log_must $ZPOOL import -d $DEVICE_DIR $TESTPOOL1

	cleanup_filesystem $TESTPOOL1 $TESTFS

	destroy_pool $TESTPOOL1
}

function cleanup_all
{
	cleanup

	# recover dev files 
	typeset i=0
	while (( i < $MAX_NUM )); do
		typeset dev_file=${DEVICE_DIR}/${DEVICE_FILE}$i
		if [[ ! -e ${dev_file} ]]; then
			log_must $MKFILE $FILE_SIZE ${dev_file}
		fi
		((i += 1))
	done

	log_must $RM -f $DEVICE_DIR/$DEVICE_ARCHIVE
	cd $CWD || log_fail "Unable change directory to $CWD"

	[[ -d $ALTER_ROOT ]] && \
		log_must $RM -rf $ALTER_ROOT

	[[ -d $BACKUP_DEVICE_DIR ]] && \
		log_must $RM -rf $BACKUP_DEVICE_DIR
}

log_onexit cleanup_all

log_assert "Verify that import could handle moving device."

CWD=$PWD

[[ ! -d $BACKUP_DEVICE_DIR ]] && 
	log_must $MKDIR -p $BACKUP_DEVICE_DIR

cd $DEVICE_DIR || log_fail "Unable change directory to $DEVICE_DIR"

typeset -i i=0
typeset -i j=0
typeset -i count=0
typeset basedir backup
typeset action

while (( i < ${#vdevs[*]} )); do

	(( i != 0 )) && \
		log_must $TAR xf $DEVICE_DIR/$DEVICE_ARCHIVE
		
	setup_filesystem "$DEVICE_FILES" \
		$TESTPOOL1 $TESTFS $TESTDIR1 \
		"" ${vdevs[i]}

	guid=$(get_config $TESTPOOL1 pool_guid)
	backup=""

	log_must $CP $MYTESTFILE $TESTDIR1/$TESTFILE0

	log_must $ZFS umount $TESTDIR1

	j=0
	while (( j <  ${#options[*]} )); do

		count=0

		#
		# Restore all device files.
		#
		[[ -n $backup ]] && \
			log_must $TAR xf $DEVICE_DIR/$DEVICE_ARCHIVE

		log_must $RM -f $BACKUP_DEVICE_DIR/*

		for device in $DEVICE_FILES ; do

			poolexists $TESTPOOL1 && \
				log_must $ZPOOL export $TESTPOOL1

			#
			# Backup all device files while filesystem prepared.
			#
			if [[ -z $backup ]] ; then
				log_must $TAR cf $DEVICE_DIR/$DEVICE_ARCHIVE ${DEVICE_FILE}*
				backup="true"
			fi

			log_must $MV $device $BACKUP_DEVICE_DIR

			(( count = count + 1 ))

			action=log_mustnot
			case "${vdevs[i]}" in
				'mirror') (( count < $GROUP_NUM )) && \
					action=log_must
					;;
				'raidz')  (( count == 1 )) && \
					action=log_must
					;;
 			esac

			typeset target=$TESTPOOL1
			if (( RANDOM % 2 == 0 )) ; then
				target=$guid
				log_note "Import by guid."
			fi
			$action $ZPOOL import \
				-d $DEVICE_DIR ${options[j]} $target

		done

		((j = j + 1))
	done

	cleanup

	((i = i + 1))
done

log_pass "Import could handle moving device."
