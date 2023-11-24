#
#  Copyright (c) 2016 Dell EMC Isilon
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

atf_test_case coredump_phnum cleanup
coredump_phnum_head()
{
	atf_set "descr" "More than 65534 segments"
	atf_set "require.config" "allow_sysctl_side_effects"
	atf_set "require.progs" "readelf procstat"
	atf_set "require.user" "root"
}
coredump_phnum_body()
{
	# Set up core dumping
	atf_check -o save:coredump_phnum_restore_state sysctl -e \
	    kern.coredump kern.corefile

	ulimit -c unlimited
	atf_check -o ignore sysctl kern.coredump=1
	atf_check -o ignore sysctl kern.corefile=coredump_phnum_helper.core
	atf_check -o save:cuc sysctl -n kern.compress_user_cores
	read cuc < cuc

	atf_check -s signal:sigabrt "$(atf_get_srcdir)/coredump_phnum_helper"

	case "$cuc" in
	0)
		unzip_status=0
		;;
	1)
		gunzip coredump_phnum_helper.core.gz 2>unzip_stderr
		unzip_status=$?
		;;
	2)
		zstd -qd coredump_phnum_helper.core.zst 2>unzip_stderr
		unzip_status=$?
		;;
	*)
		atf_skip "unsupported kern.compress_user_cores=$cuc"
		;;
	esac

	if [ $unzip_status -ne 0 ]; then
		if grep -q 'No space left on device' unzip_stderr; then
			atf_skip "file system full: $(df $PWD | tail -n 1)"
		fi
		atf_fail "unzip failed; status ${unzip_status}; " \
			"stderr: $(cat unzip_stderr)"
	fi

	# Check that core looks good
	if [ ! -f coredump_phnum_helper.core ]; then
		atf_fail "Helper program did not dump core"
	fi

	# These magic numbers don't have any real significance.  They are just
	# the result of running the helper program and dumping core.  The only
	# important bit is that they're larger than 65535 (UINT16_MAX).
	atf_check -o "match:65535 \(66[0-9]{3}\)" \
	    -x 'readelf -h coredump_phnum_helper.core | grep "Number of program headers:"'
	atf_check -o "match:There are 66[0-9]{3} program headers" \
	    -x 'readelf -l coredump_phnum_helper.core | grep -1 "program headers"'
	atf_check -o "match: 00000(0000000000)?1 .* 66[0-9]{3} " \
	    -x 'readelf -S coredump_phnum_helper.core | grep -A1 "^  \[ 0\] "'

	atf_check -o "match:66[0-9]{3}" \
	    -x 'procstat -v coredump_phnum_helper.core | wc -l'
}
coredump_phnum_cleanup()
{
	rm -f coredump_phnum_helper.core
	if [ -f coredump_phnum_restore_state ]; then
		sysctl -f coredump_phnum_restore_state
		rm -f coredump_phnum_restore_state
	fi
	rm -f cuc
}

atf_init_test_cases()
{
	atf_add_test_case coredump_phnum
}
