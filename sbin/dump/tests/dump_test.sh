#
# Copyright (c) 2026 Mitsutaka Fujita
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case ufs2_suj_live cleanup

ufs2_suj_live_head()
{
        atf_set descr "Verify dump -L works on a live UFS2 filesystem with SUJ"
        atf_set require.user root
}

ufs2_suj_live_body()
{
        atf_check -o save:md mdconfig -a -t swap -s 1g
        md=/dev/$(cat md)

        atf_check -o ignore -e ignore \
                newfs -t -O 2 -U -j "${md}"

        atf_check mkdir mnt
        atf_check mkdir restore
        atf_check mount "${md}" mnt

        echo "Hello FreeBSD dump test" > mnt/testfile

        atf_check -s exit:0 -o ignore -e ignore \
                dump -0Laf dumpfile "${md}"

        cd restore

        atf_check -s exit:0 -o ignore -e ignore \
                restore -rf ../dumpfile

        atf_check cmp testfile ../mnt/testfile
}

ufs2_suj_live_cleanup()
{
        if [ -d ./mnt ]; then
                umount ./mnt
        fi

        if [ -d restore ]; then
                # restore(8) recreates .sujournal with flags that prevent its removal.
                chflags -R noschg,nosunlink restore 2>/dev/null || true
        fi

        if [ -s md ]; then
                mdconfig -d -u "$(cat md)"
        fi
}

atf_init_test_cases()
{
        atf_add_test_case ufs2_suj_live
}
