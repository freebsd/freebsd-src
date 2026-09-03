#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The FreeBSD Foundation
#

# Tests for the 32 bit BIOS Parameter Block and FSInfo fields decoded by
# readboot() in sbin/fsck_msdosfs/boot.c.  Each case sets one field to a
# value whose most significant byte has its high bit set, which is the
# range that a byte-by-byte "b[3] << 24" decode has to shift into the
# sign bit of an int.
#
# Each case makes two assertions:
#
# 1. On stdout, that fsck_msdosfs(8) reports the full unsigned 32 bit
#    value back.  This covers the decoding itself in an ordinary build.
#
# 2. On stderr, that nothing reports a runtime error.  A byte-by-byte
#    decode of these values is undefined behavior, but every compiler we
#    use wraps it into the same bit pattern, so it cannot be caught by
#    the value alone.  In a WITH_UBSAN build it is caught here, because
#    bsd.sanitizer.mk builds with -fsanitize=undefined and
#    -fsanitize-recover=undefined, so the shift is reported on stderr and
#    execution continues.  In a build without the sanitizer these
#    assertions are trivially true.
#
# Note that assertion 2 also fails on unrelated undefined behavior that
# these images reach anywhere in fsck_msdosfs(8), which is intentional.

IMG=fat32.img

# The high bit of the most significant byte is set in all of these.
HIGH=2147483648		# 0x80000000
HIGH1=2147483649	# 0x80000001

# A UBSan report, as produced by a WITH_UBSAN build.  It never appears
# in a build without the sanitizer.
UB='runtime error'

# Read an unsigned little-endian integer of $3 bytes at offset $2 of $1.
bpb_read()
{
	od -An -v -tu1 -j "$2" -N "$3" "$1" | awk '
	    { for (i = 1; i <= NF; i++) b[n++] = $i }
	    END { v = 0; for (i = n - 1; i >= 0; i--) v = v * 256 + b[i]
		  print v }'
}

# Write the unsigned 32 bit little-endian value $3 at offset $2 of $1.
poke32()
{
	printf "$(printf '\\%03o\\%03o\\%03o\\%03o' $(($3 & 255)) \
	    $((($3 >> 8) & 255)) $((($3 >> 16) & 255)) \
	    $((($3 >> 24) & 255)))" |
	    dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

# Byte offset of the FSInfo sector of $IMG.
fsinfo_off()
{
	echo $(($(bpb_read ${IMG} 48 2) * $(bpb_read ${IMG} 11 2)))
}

# Create a 40 MiB FAT32 file system in $IMG.  One sector per cluster
# keeps it comfortably above the 65525 cluster FAT32 minimum.
make_image()
{
	atf_check -s exit:0 -o ignore -e ignore \
	    newfs_msdos -C 40m -F 32 -c 1 -S 512 ./${IMG}
	# A freshly created file system must be clean, and must not have
	# tripped the sanitizer on its way through readboot().
	atf_check -s exit:0 -o ignore -e not-match:"${UB}" \
	    fsck_msdosfs -y ./${IMG}
}

atf_test_case hidden_secs_high_bit
hidden_secs_high_bit_head()
{
	atf_set "descr" "Hidden sector count with the high bit set"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
hidden_secs_high_bit_body()
{
	make_image
	poke32 ${IMG} 28 ${HIGH}

	# readboot() decodes bpbHiddenSecs but nothing uses it, so the
	# only thing to check is that the file system still comes out
	# clean and that decoding it was well defined.
	atf_check -s exit:0 -o not-match:'Invalid' -e not-match:"${UB}" \
	    fsck_msdosfs -n ./${IMG}
}

atf_test_case fsinfo_free_high_bit
fsinfo_free_high_bit_head()
{
	atf_set "descr" "FSInfo free cluster count with the high bit set"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
fsinfo_free_high_bit_body()
{
	make_image
	poke32 ${IMG} $(($(fsinfo_off) + 0x1e8)) ${HIGH}

	# The count is far larger than the number of clusters, so it has
	# to be reported as wrong, with the decoded value spelled out.
	atf_check -s exit:0 \
	    -o match:"Free space in FSInfo block \(${HIGH}\) not correct" \
	    -e not-match:"${UB}" fsck_msdosfs -n ./${IMG}

	# Once repaired the bogus count must be gone for good.
	atf_check -s exit:0 -o ignore -e not-match:"${UB}" \
	    fsck_msdosfs -y ./${IMG}
	atf_check -s exit:0 -o not-match:'Free space in FSInfo block' \
	    -e not-match:"${UB}" fsck_msdosfs -n ./${IMG}
}

atf_test_case fsinfo_next_high_bit
fsinfo_next_high_bit_head()
{
	atf_set "descr" "FSInfo next free cluster with the high bit set"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
fsinfo_next_high_bit_body()
{
	make_image
	poke32 ${IMG} $(($(fsinfo_off) + 0x1ec)) ${HIGH1}

	atf_check -s exit:0 \
	    -o match:"Next free cluster in FSInfo block \(${HIGH1}\) invalid" \
	    -e not-match:"${UB}" fsck_msdosfs -n ./${IMG}

	atf_check -s exit:0 -o ignore -e not-match:"${UB}" \
	    fsck_msdosfs -y ./${IMG}
	atf_check -s exit:0 -o not-match:'Next free cluster in FSInfo block' \
	    -e not-match:"${UB}" fsck_msdosfs -n ./${IMG}
}

atf_test_case root_cluster_high_bit
root_cluster_high_bit_head()
{
	atf_set "descr" "FAT32 root directory cluster with the high bit set"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
root_cluster_high_bit_body()
{
	make_image
	poke32 ${IMG} 44 ${HIGH}

	# Out of range, so readboot() gives up before any phase runs.
	atf_check -s exit:8 \
	    -o match:"Root directory starts with cluster out of range\(${HIGH}\)" \
	    -e not-match:"${UB}" fsck_msdosfs -n ./${IMG}
}

atf_test_case fatsecs_high_bit
fatsecs_high_bit_head()
{
	atf_set "descr" "FAT32 sectors per FAT with the high bit set"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
fatsecs_high_bit_body()
{
	local nfats

	make_image
	nfats=$(bpb_read ${IMG} 16 1)
	poke32 ${IMG} 36 ${HIGH}

	# ${HIGH} FAT sectors times ${nfats} FATs overflows 32 bits.
	atf_check -s exit:8 \
	    -o match:"Invalid FATs\(${nfats}\) with FATsecs\(${HIGH}\)" \
	    -e not-match:"${UB}" fsck_msdosfs -n ./${IMG}
}

atf_test_case huge_sectors_high_bit
huge_sectors_high_bit_head()
{
	atf_set "descr" "32 bit total sector count with the high bit set"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
huge_sectors_high_bit_body()
{
	local bps spc rsvd nfats fatsz first clusters

	make_image
	bps=$(bpb_read ${IMG} 11 2)
	spc=$(bpb_read ${IMG} 13 1)
	rsvd=$(bpb_read ${IMG} 14 2)
	nfats=$(bpb_read ${IMG} 16 1)
	fatsz=$(bpb_read ${IMG} 36 4)
	poke32 ${IMG} 32 ${HIGH}

	# The root directory has no fixed entries on FAT32, so the data
	# area starts right after the reserved sectors and the FATs.
	first=$((rsvd + nfats * fatsz))
	clusters=$(((HIGH - first) / spc))

	# Too many clusters for FAT32; the count in the message is
	# derived from the decoded sector count.
	atf_check -s exit:8 \
	    -o match:"Filesystem too big \(${clusters} clusters\) for FAT32" \
	    -e not-match:"${UB}" fsck_msdosfs -n ./${IMG}
}

atf_init_test_cases()
{
	atf_add_test_case hidden_secs_high_bit
	atf_add_test_case fsinfo_free_high_bit
	atf_add_test_case fsinfo_next_high_bit
	atf_add_test_case root_cluster_high_bit
	atf_add_test_case fatsecs_high_bit
	atf_add_test_case huge_sectors_high_bit
}
