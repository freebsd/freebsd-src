#
# Regression tests for partition offsets in mkimg(1) partition specifications.
#
# mkimg(1) documents an optional offset for a partition specification:
#
#       <t>[/<l>]::<size>[:[+]<offset>]  - SIZE form
#       <t>[/<l>]:=<file>[:[+]<offset>]  - FILE form
#
# An offset without a "+" prefix is absolute from the start of the image; with
# a "+" prefix it is relative to the end of the preceding partition.  Both
# forms must honour both kinds of offset.
#
# These tests assert the resulting start LBA rather than comparing a hexdump
# baseline, so a failure reports the address that was actually produced.
#

# Start LBA of partition $2 (1-based) in GPT image $1, for an image whose
# logical sector size is $3 (default 512).
#
# The GPT entry array starts at LBA 2, so its byte offset scales with the
# sector size.  Entries are 128 bytes and the starting LBA is a little-endian
# 64-bit value at offset 32 within an entry; both are fixed by the spec.  Only
# the low 32 bits are read, which is ample for these images and keeps the
# arithmetic inside what expr(1) handles portably.
gpt_start_lba()
{
    local image index secsz off byte shift value

    image=$1
    index=$2
    secsz=${3:-512}

    off=$(((2 * secsz) + (index - 1) * 128 + 32))

    value=0
    shift=1
    # Little-endian: least significant byte first.
    for byte in $(od -A n -t u1 -j $off -N 4 "$image"); do
        value=$((value + byte * shift))
        shift=$((shift * 256))
    done
    echo $value
}

# Build a 512-byte-sector GPT image whose second partition is requested at an
# offset, then check where that partition actually landed.
#
# $1 partition specification for partition 2
# $2 expected start LBA of partition 2
check_second_partition()
{
    local spec want got

    spec=$1
    want=$2

    # Partition 1 is 25 blocks, so without offset handling partition 2
    # would be packed at LBA 34 + 25 = 59.  The expected offsets below are
    # all beyond that, which is what makes the failure observable.
    atf_check -s exit:0 -o empty -e empty \
        mkimg -s gpt -S 512 -P 512 --capacity 16m \
        -p freebsd-boot::12800 \
        -p "$spec" \
        -o image.raw

    got=$(gpt_start_lba image.raw 2)
    if test "$got" != "$want"; then
        atf_fail "partition 2 starts at LBA $got, expected $want"
    fi
}

# As check_second_partition(), but for a 4096-byte logical sector image.
#
# Note that -S sets the logical sector size that determines the GPT layout;
# -P only sets the physical block size used for alignment.  Partition 1 is 2m,
# which at this sector size spans LBA 6 through 517, so the offsets used below
# have to reach past that to avoid an overlap.
#
# $1 partition specification for partition 2
# $2 expected start LBA of partition 2
check_second_partition_4k()
{
    local spec want got

    spec=$1
    want=$2

    atf_check -s exit:0 -o empty -e empty \
        mkimg -s gpt -S 4096 -P 4096 --capacity 64m \
        -p freebsd-boot::2m \
        -p "$spec" \
        -o image4k.raw

    got=$(gpt_start_lba image4k.raw 2 4096)
    if test "$got" != "$want"; then
        atf_fail "partition 2 starts at LBA $got, expected $want"
    fi
}

atf_test_case size_absolute_offset
size_absolute_offset_head()
{
    atf_set "descr" "SIZE form honours an absolute offset"
}
size_absolute_offset_body()
{
    # 40960 bytes / 512 = LBA 80.  This is the specification form and the
    # offset used by release/amd64/mkisoimages.sh for the ESP.
    check_second_partition "efi::2m:40960" 80
}

atf_test_case size_relative_offset
size_relative_offset_head()
{
    atf_set "descr" "SIZE form honours a relative offset"
}
size_relative_offset_body()
{
    # Partition 1 ends at LBA 58, so +10752 bytes (21 blocks) is LBA 80.
    check_second_partition "efi::2m:+10752" 80
}

atf_test_case file_absolute_offset
file_absolute_offset_head()
{
    atf_set "descr" "FILE form honours an absolute offset"
}
file_absolute_offset_body()
{
    check_second_partition \
        "efi:=$(atf_get_srcdir)/partition_data_4M.bin:40960" 80
}

atf_test_case file_relative_offset
file_relative_offset_head()
{
    atf_set "descr" "FILE form honours a relative offset"
}
file_relative_offset_body()
{
    check_second_partition \
        "efi:=$(atf_get_srcdir)/partition_data_4M.bin:+10752" 80
}

atf_test_case size_no_offset
size_no_offset_head()
{
    atf_set "descr" "SIZE form without an offset packs after the previous partition"
}
size_no_offset_body()
{
    # Guards against a fix that treats a missing offset as an absolute 0.
    check_second_partition "efi::2m" 59
}

atf_test_case file_colon_in_name
file_colon_in_name_head()
{
    atf_set "descr" "FILE form accepts a filename containing a colon"
}
file_colon_in_name_body()
{
    # An existing file wins over the offset interpretation, so a colon in
    # the name must not be parsed as an offset separator.  This is the
    # behaviour the parser rework was written to provide; keep it covered
    # so restoring offsets to the SIZE form cannot regress it.
    atf_check -s exit:0 -o empty -e empty \
        cp "$(atf_get_srcdir)/partition_data_4M.bin" "odd:name.bin"
    check_second_partition "efi:=odd:name.bin" 59
}

atf_test_case size_absolute_offset_4k
size_absolute_offset_4k_head()
{
    atf_set "descr" "SIZE form honours an absolute offset with 4K sectors"
}
size_absolute_offset_4k_body()
{
    # 2621440 bytes / 4096 = LBA 640.
    check_second_partition_4k "efi::2m:2621440" 640
}

atf_test_case size_relative_offset_4k
size_relative_offset_4k_head()
{
    atf_set "descr" "SIZE form honours a relative offset with 4K sectors"
}
size_relative_offset_4k_body()
{
    # Partition 1 ends at LBA 517, so +512000 bytes (125 blocks) is LBA 643.
    check_second_partition_4k "efi::2m:+512000" 643
}

atf_test_case file_absolute_offset_4k
file_absolute_offset_4k_head()
{
    atf_set "descr" "FILE form honours an absolute offset with 4K sectors"
}
file_absolute_offset_4k_body()
{
    check_second_partition_4k \
        "efi:=$(atf_get_srcdir)/partition_data_4M.bin:2621440" 640
}

atf_test_case size_no_offset_4k
size_no_offset_4k_head()
{
    atf_set "descr" "SIZE form without an offset packs after the previous partition with 4K sectors"
}
size_no_offset_4k_body()
{
    check_second_partition_4k "efi::2m" 518
}

atf_init_test_cases()
{
    atf_add_test_case size_absolute_offset
    atf_add_test_case size_relative_offset
    atf_add_test_case file_absolute_offset
    atf_add_test_case file_relative_offset
    atf_add_test_case size_no_offset
    atf_add_test_case file_colon_in_name
    atf_add_test_case size_absolute_offset_4k
    atf_add_test_case size_relative_offset_4k
    atf_add_test_case file_absolute_offset_4k
    atf_add_test_case size_no_offset_4k
}
