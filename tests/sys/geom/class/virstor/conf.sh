#!/bin/sh

class="virstor"
base=$(atf_get ident)
TEST_VIRSTOR_DEVS_FILE="${TMPDIR}/test_virstor_devs.$(basename $0)"

gvirstor_dev_setup()
{
	# Pick a random name and record it for cleanup.
	local vdevbase="$(mktemp -u virstor.XXXXXX)" || aft_fail "mktemp"
	echo "$vdevbase" >> "$TEST_VIRSTOR_DEVS_FILE"
	eval "${1}='${vdevbase}'"
}

gvirstor_test_cleanup()
{
	local vdevbase
	if [ -f "$TEST_VIRSTOR_DEVS_FILE" ]; then
		while read vdevbase; do
			if [ -c "/dev/$class/$vdevbase" ]; then
				echo "# Destroying test virstor device:" \
				    "$vdevbase"
				gvirstor destroy "$vdevbase"
			fi
		done < "$TEST_VIRSTOR_DEVS_FILE"
	fi
	geom_test_cleanup
}

ATF_TEST=true
. `dirname $0`/../geom_subr.sh
