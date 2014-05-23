#  Copyright (c) 2013 Spectra Logic Corporation
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions, and the following disclaimer,
#     without modification.
#  2. Redistributions in binary form must reproduce at minimum a disclaimer
#     substantially similar to the "NO WARRANTY" disclaimer below
#     ("Disclaimer") and any redistribution must be conditioned upon
#     including a substantially similar Disclaimer requirement for further
#     binary redistribution.
#
#  NO WARRANTY
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
#  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
#  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGES.
#
#  Authors: Alan Somers         (Spectra Logic Corporation)
#
# $FreeBSD$

#
# Test Case: zfsd_unittest
# TODO: get coverage in cleanup
#
atf_test_case zfsd_unittest
zfsd_unittest_head()
{
	atf_set "descr" "Run zfsd unit tests"
}


zfsd_unittest_body()
{
	TESTPROG=$(atf_get_srcdir)/zfsd_unittest
	if atf_config_has coverage_dir; then
		# If coverage_dir is defined, then we want to save the .gcda
		# and .gcno files for future analysis.  Put them in a directory
		# tree that resembles /usr/src, but is anchored at
		# coverage_dir.
		export GCOV_PREFIX=`atf_config_get coverage_dir`
		# Examine zfsd_unittest to calculate the GCOV_PREFIX_STRIP
		# The outer echo command is needed to strip off whitespace
		# printed by wc
		OLDGCDADIR=`strings $TESTPROG | grep 'zfsd.gcda'`
		export GCOV_PREFIX_STRIP=$( echo $( echo $OLDGCDADIR | \
			sed -e 's:/cddl/sbin/zfsd.*::' -e 's:/: :g' | \
			wc -w ) )
		NEWGCDADIR=$GCOV_PREFIX/`dirname $OLDGCDADIR | \
			sed -e 's:.*\(cddl/sbin/zfsd\):\1:'`
		mkdir -p $NEWGCDADIR
		cp $(atf_get_srcdir)/*.gcno $NEWGCDADIR
	fi
	atf_check -s exit:0 -o ignore -e ignore $TESTPROG
}

atf_init_test_cases()
{
	atf_add_test_case zfsd_unittest
}
