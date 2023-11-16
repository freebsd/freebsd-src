#!/bin/sh

class="union"
base=$(atf_get ident)

attach_md()
{
	local test_md

	test_md=$(mdconfig -a "$@") || atf_fail "failed to allocate md(4)"
	echo $test_md >> $TEST_MDS_FILE || exit
	echo $test_md
}

gunion_test_cleanup()
{
    if mount | grep -q "/gunionmnt"; then
        umount gunionmnt
    fi
    if mount | grep -q "/uppermnt"; then
        umount uppermnt
    fi
    if mount | grep -q "/lowermnt"; then
        umount lowermnt
    fi

    if [ -e "guniondev" ]; then
        gunion destroy "$(cat guniondev)"
    fi

    geom_test_cleanup
}

gunion_test_setup()
{
	geom_atf_test_setup
}

ATF_TEST=true
. `dirname $0`/../geom_subr.sh
