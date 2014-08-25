#! /usr/local/bin/ksh93 -p
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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)inherit_001_pos.ksh	1.5	09/08/06 SMI"
#

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/inheritance/inherit.kshlib

###############################################################################
#
# __stc_assertion_start
#
# ID: inherit_001_pos
#
# DESCRIPTION:
# Test that properties are correctly inherited using 'zfs set',
# 'zfs inherit' and 'zfs inherit -r'.
#
# STRATEGY:
# 1) Read a configX.cfg file and create the specified datasets
# 2) Read a stateX.cfg file and execute the commands within it
# and verify that the properties have the correct values
# 3) Repeat steps 1-2 for each configX and stateX files found.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2005-07-04)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

log_assert "Test properties are inherited correctly"

#
# Simple function to create specified datasets.
#
function create_dataset { #name type disks
        typeset dataset=$1
        typeset type=$2
        typeset disks=$3

        if [[ $type == "POOL" ]]; then
                create_pool "$dataset" "$disks"
        elif [[ $type == "CTR" ]]; then
                log_must $ZFS create $dataset
		log_must $ZFS set canmount=off $dataset
        elif [[ $type == "FS" ]]; then
                log_must $ZFS create $dataset
        else
                log_fail "Unrecognised type $type"
        fi

        list="$list $dataset"
}

#
# Function to walk through all the properties in a
# dataset, setting them to a 'local' value if required.
#
function init_props { #dataset init_code
        typeset dataset=$1
        typeset init_code=$2
	typeset new_val
	typeset -i i=0

	#
	# Though the effect of '-' and 'default' is the same we
	# call them out via a log_note to aid in debugging the
	# config files 
	#
        if [[ $init_code == "-" ]]; then
                log_note "Leaving properties for $dataset unchanged."
		[[ $def_recordsize == 0 ]] && \
			update_recordsize $dataset $init_code
                return;
        elif [[ $init_code == "default" ]]; then
                log_note "Leaving properties for $dataset at default values."
		[[ $def_recordsize == 0 ]] && \
			update_recordsize $dataset $init_code
                return;
        elif [[ $init_code == "local" ]]; then
                log_note "Setting properties for $dataset to local values."
                while (( i <  ${#prop[*]} )); do
			if [[ ${prop[i]} == "recordsize" ]]; then
				update_recordsize $dataset $init_code
                	else
				set_n_verify_prop ${prop[i]} \
					${local_val[((i/2))]} $dataset
                	fi

                        (( i = i + 2 ))
                done
        else
		log_fail "Unrecognised init code $init_code"
	fi
}

#
# We enter this function either to update the recordsize value
# in the default array, or to update the local value array.
#
function update_recordsize { #dataset init_code 
	typeset dataset=$1
	typeset init_code=$2
	typeset idx=0
	typeset record_val
	
	#
	# First need to find where the recordsize property is
	# located in the arrays
	#
	while (( idx <  ${#prop[*]} )); do
		[[ ${prop[idx]} == "recordsize" ]] && \
			break
		
		(( idx = idx + 2))
	done

	(( idx = idx / 2 ))
	record_val=`get_prop recordsize $dataset`
	if [[ $init_code == "-" || \
		$init_code == "default" ]]; then

		def_val[idx]=$record_val
		def_recordsize=1

	elif [[ $init_code == "local" ]]; then

		log_must $ZFS set recordsize=$record_val $dataset

		local_val[idx]=$record_val
	fi
}

#
# The mountpoint property is slightly different from other properties and 
# so is handled here. For all other properties if they are set to a specific 
# value at a higher level in the data hierarchy (i.e. checksum=on) then that 
# value propogates down the hierarchy unchanged, with the source field being 
# set to 'inherited from <higher dataset>'.
#
# The mountpoint property is different in that while the value propogates
# down the hierarchy, the value at each level is determined by a combination 
# of the top-level value and the current level in the hierarchy. 
#
# For example consider the case where we have a pool (called pool1), containing 
# a dataset (ctr) which in turn contains a filesystem (fs). If we set the 
# mountpoint of the pool to '/mnt2' then the mountpoints for the dataset and 
# filesystem are '/mnt2/ctr' and /mnt2/ctr/fs' respectively, with the 'source' 
# field being set to 'inherited from pool1'. 
#
# So at the filesystem level to calculate what our mountpoint property should 
# be set to we walk back up the hierarchy sampling the mountpoint property at 
# each level and forming up the expected mountpoint value piece by piece until 
# we reach the level specified in the 'source' field, which in this example is 
# the top-level pool.
#
function get_mntpt_val #dataset src index
{
        typeset dataset=$1
        typeset src=$2
        typeset idx=$3
        typeset new_path=""
        typeset dset
	typeset mntpt=""

	if [[ $src == "local" ]]; then
		mntpt=${local_val[idx]}
	elif [[ $src == "default" ]]; then
		mntpt="$ZFSROOT/"$dataset
	else
		# Walk back up the hierarchy building up the 
		# expected mountpoint property value.
		obj_name=${dataset##*/}

		while [[ $src != $dataset ]]; do
			dset=${dataset%/*}

			mnt_val=`get_prop mountpoint $dset`

			mod_prop_val=${mnt_val##*/}
			new_path="/"$mod_prop_val$new_path
			dataset=$dset
		done

		mntpt=$new_path"/"$obj_name
	fi
	print $mntpt
}

#
# Simple function to verify that a property has the
# expected value.
#
function verify_prop_val #property dataset src index
{
	typeset prop=$1
	typeset dataset=$2
	typeset src=$3
	typeset idx=$4
	typeset new_path=""
	typeset dset
	typeset exp_val
	typeset prop_val

	prop_val=`get_prop $prop $dataset`

	# mountpoint property is handled as a special case
	if [[ $prop == "mountpoint" ]]; then
		exp_val=`get_mntpt_val $dataset $src $idx`
	else
		if [[ $src == "local" ]]; then
			exp_val=${local_val[idx]}
		elif [[ $src == "default" ]]; then
			exp_val=${def_val[idx]}
		else
			#
			# We are inheriting the value from somewhere
			# up the hierarchy.
			#
			exp_val=`get_prop $prop $src`
		fi
	fi

	if [[ $prop_val != $exp_val ]]; then
		# After putback PSARC/2008/231 Apr,09,2008, 
		# the default value of aclinherit has changed to be
		# 'restricted' instead of 'secure',
		# but the old interface of 'secure' still exist

		if [[ $prop != "aclinherit" || \
			$exp_val != "secure" || \
			$prop_val != "restricted" ]]; then

			log_fail "$prop of $dataset is [$prop_val] rather "\
				"than [$exp_val]"
		fi
	fi
}

#
# Function to read the configX.cfg files and create the specified 
# dataset hierarchy
#
function scan_config { #config-file
	typeset config_file=$1

	DISK=${DISKS%% *}

	list=""

        grep "^[^#]" $config_file | {
                while read name type init ; do
                        create_dataset $name $type $DISK
                        init_props $name $init
                done
        }
}

#
# Function to check an exit flag, calling log_fail if that exit flag
# is non-zero. Can be used from code that runs in a tight loop, which
# would otherwise result in a lot of journal output.
#
function check_failure { # int status, error message to use
	
	typeset -i exit_flag=$1
	error_message=$2

	if [ $exit_flag -ne 0 ]
	then
		log_fail "$error_message"
	fi
}


#
# Main function. Executes the commands specified in the stateX.cfg
# files and then verifies that all the properties have the correct
# values and 'source' fields.
#
function scan_state { #state-file
	typeset state_file=$1
	typeset -i i=0
	typeset -i j=0

	log_note "Reading state from $state_file"

	while (( i <  ${#prop[*]} )); do
		grep "^[^#]" $state_file | {
			while IFS=: read target op; do
				#
				# The user can if they wish specify that no 
				# operation be performed (by specifying '-' 
				# rather than a command). This is not as 
				# useless as it sounds as it allows us to 
				# verify that the dataset hierarchy has been 
				# set up correctly as specified in the 
				# configX.cfg file (which includes 'set'ting 
				# properties at a higher level and checking 
				# that they propogate down to the lower levels.
				#
				# Note in a few places here, we use
				# check_failure, rather than log_must - this
				# substantially reduces journal output.
				#
				if [[ $op == "-" ]]; then
					log_note "No operation specified"
				else
					# Unmount the test datasets if they
					# are still mounted.  Most often, they
					# won't be, so discard the output
					unmount_all_safe > /dev/null 2>&1

					for p in ${prop[i]} ${prop[((i+1))]}; do
						$ZFS $op $p $target
						ret=$?
						check_failure $ret "$ZFS $op $p \
						$target"
					done
				fi
				for check_obj in $list; do
					read init_src final_src

					for p in ${prop[i]} ${prop[((i+1))]}; do
						# check_failure to keep journal
						# small
						verify_prop_src $check_obj $p \
							$final_src
						ret=$?
						check_failure $ret "verify_prop_src \
							$check_obj $p \
							$final_src"
						
					# Again, to keep journal size down.
						verify_prop_val $p $check_obj \
							$final_src $j
						ret=$?
						check_failure $ret "verify_prop_val \
							$check_obj $p \
							$final_src"
					done
                                done
                        done
                }
                (( i = i + 2 ))
                (( j = j + 1 ))
        done
}


set -A prop "checksum" "" \
	"compression" "compress" \
	"atime" "" \
	"exec" "" \
        "setuid" "" \
	"sharenfs" "" \
	"recordsize" "recsize" \
	"mountpoint" "" \
	"snapdir" "" \
	"aclmode" "" \
	"aclinherit" "" \
	"readonly" "rdonly"

# 
# Note except for the mountpoint default value (which is handled in
# the routine itself), each property specified in the 'prop' array
# above must have a corresponding entry in the two arrays below.
# 
set -A def_val "on" "off" "on"  "on" \
	"on" "off" "" \
	"" "hidden" "discard" "secure" \
	"off"

set -A local_val "off" "on" "off"  "off" \
	"off" "on" "" \
	"$TESTDIR" "visible" "groupmask" "discard" \
	"off"

# Append the "shareiscsi" property if it is supported
$ZFS get shareiscsi > /dev/null 2>&1
if [[ $? -eq 0 ]]; then
	typeset -i i=${#prop[*]}
	prop[i]="shareiscsi"
	prop[((i+1))]=""
	def_val[((i/2))]="off"
	local_val[((i/2))]="on"
fi

# Append the "devices" property if it is settable
log_must $ZPOOL create $TESTPOOL ${DISKS%% *}
$ZFS set devices=off $TESTPOOL
if [[ $? -eq 0 ]]; then
	typeset -i i=${#prop[*]}
	prop[i]="devices"
	prop[((i+1))]=""
	def_val[((i/2))]="on"
	local_val[((i/2))]="off"
else
	log_note "Setting devices=off is not supported on this system"
fi
log_must $ZPOOL destroy $TESTPOOL

#
# Global flag indicating whether the default record size had been
# read.
#
typeset def_recordsize=0

set -A config_files $(ls $STF_SUITE/tests/inheritance/config*[1-9]*.cfg)
set -A state_files $(ls $STF_SUITE/tests/inheritance/state*.cfg)

#
# Global list of datasets created.
#
list=""

typeset -i k=0

if [[ ${#config_files[*]} != ${#state_files[*]} ]]; then
	log_fail "Must have the same number of config files "\
		" (${#config_files[*]}) and state files ${#state_files[*]}"
fi

while (( k < ${#config_files[*]} )); do
	default_cleanup_noexit
	def_recordsize=0

	log_note "Testing configuration ${config_files[k]}"

	scan_config ${config_files[k]}
	scan_state ${state_files[k]}

	(( k = k + 1 ))
done

log_pass "Properties correctly inherited as expected"
