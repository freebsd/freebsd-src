#!/bin/sh

scriptdir=$(dirname $(realpath "$0"))

. ${scriptdir}/common.sh

# Ensure emul_swp is enabled just for this test; we'll turn it back off if
# it wasn't enabled before the test.
emul_swpval=$(sysctl -n compat.arm.emul_swp)
sysctl compat.arm.emul_swp=1 >/dev/null
${scriptdir}/swp_test_impl
if [ "$emul_swpval" -ne 1 ]; then
	sysctl compat.arm.emul_swp="$emul_swpval" >/dev/null
fi
