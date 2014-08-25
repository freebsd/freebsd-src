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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)r_verify_recv.ksh	1.4	09/05/19 SMI"
#

. ${STF_SUITE}/include/libtest.kshlib

RTEST_ROOT=$1
prog=`whence -p $0`
progpath=${prog%/*}
progdirname=${progpath##*/} # test case directory name
commlibpath=${progpath%/*}
PKGNAME=$(get_package_name)
PKGDIR=`print $(/usr/bin/pkginfo -l $PKGNAME | \
		/usr/bin/grep BASEDIR: | cut -d: -f2)`

. $RTEST_ROOT/stf_config.vars
. $RTEST_ROOT/$progdirname/cross_endian.cfg
. $PKGDIR/commands.cfg
. $commlibpath/remote_common.kshlib

#
# Verify the data integrity after wire sending 
#
# $1 data file path
#

function verify_data # <data file path>
{
	typeset filepath=$1
	typeset snappath=$1/$(get_snapdir_name)/$TESTSNAP
	typeset cksumfile="$filepath/file_cksum.list"
	typeset orig_val
	typeset dest_val
	typeset snapfile
	typeset fname
	typeset orig_sum
	typeset dest_sum
	typeset orig_size
	typeset dest_size
	typeset -i ret

	[[ ! -e $cksumfile ]] && exit 1

	for file in `$FIND $filepath -type f`; do
		fname=${file##$filepath}
        	fname=${fname#/}
        	snapfile=$snappath/$fname

        	[[ ! -e $snapfile ]] && _err_exit 1 \
			"$snapfile doesn't exist."

        	$DIFF $file $snapfile >/dev/null 2>&1
		ret=$?
		(( $ret != 0 )) && _err_exit $ret \
			"The original $file differs from $snapfile."

		if [[ $file != $cksumfile ]]; then
			for data in $file $snapfile; do
        			cksum_val="`$CKSUM $data`"
				orig_val="`$GREP "$fname" $cksumfile`"  
				orig_sum="`$ECHO $orig_val | $AWK '{print $1}'"
				dest_sum="`$ECHO $cksum_val | $AWK '{print $1}'"
				orig_size="`$ECHO $orig_val | $AWK '{print $2}'"
				dest_size="`$ECHO $cksum_val | $AWK '{print $2}'"
				if [[ "$orig_sum" != "$dest_sum" ]] || \
					[[ "$orig_size" != "$dest_size" ]]; then
					_err_exit 1 "Checksum changed after wire sending."
				fi
			done
		fi
	done
}

for ds in $RTESTPOOL/$TESTFS $RTESTPOOL/$TESTFS@$TESTSNAP; do
	$ZFS list -H "$ds" >/dev/null 2>&1
	ret=$?
	(( $ret != 0 )) && _err_exit $ret \
		"The dataset $ds doesn't exist."
done
	
verify_data /$RTESTPOOL/$TESTFS

exit 0
