# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 The FreeBSD Foundation
#
# This software was developed1 by Yan-Hao Wang <bses30074@gmail.com>
# under sponsorship from the FreeBSD Foundation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

. $(atf_get_srcdir)/conf.sh

atf_test_case create cleanup
create_head()
{
    atf_set "descr" "Test gunion create and destroy"
    atf_set "require.user" "root"
}
create_body()
{
    gunion_test_setup

    attach_md upperdev -s 1m
    attach_md lowerdev -s 1m
    newfs -U "/dev/${lowerdev}"

    atf_check gunion create "$upperdev" "$lowerdev"
    guniondev="${upperdev}-${lowerdev}.union"
    atf_check -o inline:"/dev/${guniondev}\n" ls "/dev/${guniondev}"
    atf_check -o ignore fsck -p -f "/dev/${guniondev}"

    atf_check gunion destroy "$guniondev"
    atf_check -s not-exit:0 -o ignore -e ignore ls "/dev/${guniondev}"
}
create_cleanup()
{
    gunion_test_cleanup
}

atf_test_case basic cleanup
basic_head()
{
    atf_set "descr" "Check gunion doesn't affect lowerdev status and lowerdev can't be mounted when being in a gunion"
    atf_set "require.user" "root"
}
basic_body()
{
    gunion_test_setup

    attach_md upperdev -s 1m
    attach_md lowerdev -s 1m
    newfs -U "/dev/${lowerdev}"
    mkdir lowermnt
    mkdir gunionmnt

    mount "/dev/${lowerdev}" lowermnt
    echo "lower file" > lower_file
    cp lower_file lowermnt/lower_file
    sync
    umount lowermnt

    gunion create "$upperdev" "$lowerdev"
    guniondev="${upperdev}-${lowerdev}.union"
    atf_check -s not-exit:0 -o ignore -e ignore mount "/dev/${lowerdev}" lowermnt

    mount "/dev/${guniondev}" gunionmnt
    echo "update lower file" >> gunionmnt/lower_file
    echo "gunion file" > gunion_file
    cp gunion_file gunionmnt/gunion_file
    sync
    umount gunionmnt

    gunion destroy "$guniondev"
    mount "/dev/${lowerdev}" lowermnt
    checksum lowermnt/lower_file lower_file
    atf_check -s not-exit:0  -o ignore -e ignore ls lowermnt/gunion_file
}
basic_cleanup()
{
    gunion_test_cleanup
}

atf_test_case commit cleanup
commit_head()
{
    atf_set "descr" "Test basic gunion commit without option"
    atf_set "require.user" "root"
}
commit_body()
{
    gunion_test_setup

    attach_md upperdev -s 1m
    attach_md lowerdev -s 1m
    newfs -U "/dev/${lowerdev}"
    mkdir lowermnt
    mkdir gunionmnt

    mount "/dev/${lowerdev}" lowermnt
    echo "lower file" > lower_file
    cp lower_file lowermnt/lower_file
    sync
    umount lowermnt

    gunion create "$upperdev" "$lowerdev"
    guniondev="${upperdev}-${lowerdev}.union"
    mount "/dev/${guniondev}" gunionmnt
    checksum gunionmnt/lower_file lower_file

    echo "update lower file" >> lower_file
    cp -f lower_file gunionmnt/lower_file
    echo "gunion file" > gunion_file
    cp gunion_file gunionmnt/gunion_file
    sync
    umount gunionmnt
    atf_check gunion commit "$guniondev"
    gunion destroy "$guniondev"

    atf_check -o ignore fsck -p -f "/dev/${lowerdev}"
    mount "/dev/${lowerdev}" lowermnt
    checksum lowermnt/lower_file lower_file
    checksum lowermnt/gunion_file gunion_file
}
commit_cleanup()
{
    gunion_test_cleanup
}

atf_test_case offset cleanup
offset_head()
{
    atf_set "descr" "Test gunion create with -o offset option"
    atf_set "require.user" "root"
}
offset_body()
{
    gunion_test_setup

    attach_md upperdev -s 1m
    attach_md lowerdev -s 1m
    gpart create -s GPT "/dev/${lowerdev}"
    gpart add -t freebsd-ufs "$lowerdev"
    newfs "/dev/${lowerdev}p1"
    gpt_entry_1=$(gpart show "/dev/${lowerdev}")
    mkdir gunionmnt

    secsize="$(diskinfo "/dev/${lowerdev}" | awk '{print $2}')"
    p1_start_sector="$(gpart show -p "/dev/${lowerdev}" | grep ${lowerdev}p1 | awk '{print $1}')"
    offset_size="$((secsize * p1_start_sector))"

    gunion create -o "$offset_size" "$upperdev" "$lowerdev"
    guniondev="${upperdev}-${lowerdev}.union"

    atf_check -o ignore fsck -p -f "/dev/${guniondev}"
    atf_check mount "/dev/${guniondev}" gunionmnt
    umount gunionmnt
    gunion destroy "$guniondev"

    gpt_entry_2=$(gpart show "/dev/${lowerdev}")
    atf_check_equal "$gpt_entry_1" "$gpt_entry_2"
}
offset_cleanup()
{
    gunion_test_cleanup
}

atf_test_case size cleanup
size_head()
{
    atf_set "descr" "Test gunion create with -s size option"
    atf_set "require.user" "root"
}
size_body()
{
    gunion_test_setup

    attach_md upperdev -s 2m
    attach_md lowerdev -s 1m
    newfs -U "/dev/${lowerdev}"

    gunion create -s 2m "$upperdev" "$lowerdev"
    guniondev="${upperdev}-${lowerdev}.union"
    echo "$guniondev" > guniondev

    size="$(diskinfo "/dev/$guniondev" | awk '{print $3}')"
    atf_check_equal "2097152" "$size" # 2 MB = 2097152 bytes
}
size_cleanup()
{
    gunion_test_cleanup
}

atf_test_case secsize cleanup
secsize_head()
{
    atf_set "descr" "Test gunion create with -S secsize option"
    atf_set "require.user" "root"
}
secsize_body()
{
    gunion_test_setup

    attach_md upperdev -s 1m
    attach_md lowerdev -s 1m
    newfs -S 512 -U "/dev/${lowerdev}"
    lower_secsize="$(diskinfo "/dev/${lowerdev}" | awk '{print $2}')"
    atf_check_equal "512" "$lower_secsize"

    gunion create -S 1024 "$upperdev" "$lowerdev"
    guniondev="${upperdev}-${lowerdev}.union"
    echo "$guniondev" > guniondev

    secsize="$(diskinfo "/dev/${guniondev}" | awk '{print $2}')"
    atf_check_equal "1024" "$secsize"
}
secsize_cleanup()
{
    gunion_test_cleanup
}

atf_test_case gunionname cleanup
gunionname_head()
{
    atf_set "descr" "Test gunion create with -Z gunionname option"
    atf_set "require.user" "root"
}
gunionname_body()
{
    gunion_test_setup

    attach_md upperdev -s 1m
    attach_md lowerdev -s 1m
    newfs -U "/dev/${lowerdev}"

    gunion create -Z gunion1 "$upperdev" "$lowerdev"
    echo "gunion1.union" > guniondev
    atf_check -o inline:"/dev/gunion1.union\n" ls /dev/gunion1.union
}
gunionname_cleanup()
{
    gunion_test_cleanup
}

atf_test_case revert cleanup
revert_head()
{
    atf_set "descr" "Test gunion revert"
    atf_set "require.user" "root"
}
revert_body()
{
    gunion_test_setup

    attach_md upperdev -s 1m
    attach_md lowerdev -s 1m
    newfs -U "/dev/${lowerdev}"
    mkdir lowermnt
    mkdir gunionmnt

    mount "/dev/${lowerdev}" lowermnt
    echo "lower file" > lower_file
    cp lower_file lowermnt/lower_file
    sync
    umount lowermnt

    atf_check gunion create "$upperdev" "$lowerdev"
    guniondev="${upperdev}-${lowerdev}.union"
    mount "/dev/${guniondev}" gunionmnt

    echo "update lower file" >> gunionmnt/lower_file
    echo "gunion file" > gunion_file
    cp gunion_file gunionmnt/gunion_file
    sync
    umount gunionmnt
    atf_check gunion revert "$guniondev"

    mount "/dev/${guniondev}" gunionmnt
    checksum gunionmnt/lower_file lower_file
    atf_check -s not-exit:0 -o ignore -e ignore ls gunionmnt/gunion_file

    umount gunionmnt
    gunion destroy "$guniondev"
}
revert_cleanup()
{
    gunion_test_cleanup
}

atf_init_test_cases()
{
    atf_add_test_case create
    atf_add_test_case basic
    atf_add_test_case commit
    atf_add_test_case offset
    atf_add_test_case size
    atf_add_test_case secsize
    atf_add_test_case gunionname
    atf_add_test_case revert
}

checksum()
{
    src=$1
    work=$2

    if [ ! -e "$src" ]; then
        atf_fail "file not exist"
    fi
    if [ ! -e "$work" ]; then
        atf_fail "file not exist"
    fi

    src_checksum=$(md5 -q "$src")
    work_checksum=$(md5 -q "$work")

    if [ "$work_checksum" != "$src_checksum" ]; then
        atf_fail "md5 checksum didn't match with ${src} and ${work}"
    fi
}
