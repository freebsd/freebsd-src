#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The FreeBSD Foundation
#

# Tests for fsck_msdosfs(8) on FAT32 volumes larger than 4 GiB, where a
# cluster's byte offset no longer fits in 32 bits.  reconnect() used to
# compute the offset of the LOST.DIR cluster in 32 bit arithmetic, so it
# read, modified and wrote back the cluster 4 GiB below the intended one,
# silently corrupting whatever user data lived there while leaving the lost
# chain unreferenced.

IMG=fat32.img

# Read an unsigned little-endian integer of $3 bytes at offset $2 of $1.
bpb_read()
{
	od -An -v -tu1 -j "$2" -N "$3" "$1" | awk '
	    { for (i = 1; i <= NF; i++) b[n++] = $i }
	    END { v = 0; for (i = n - 1; i >= 0; i--) v = v * 256 + b[i]
		  print v }'
}

# Write the unsigned 8 bit value $3 at offset $2 of $1.
poke8()
{
	printf "$(printf '\\%03o' $(($3 & 255)))" |
	    dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

# Write the unsigned 16 bit little-endian value $3 at offset $2 of $1.
poke16()
{
	printf "$(printf '\\%03o\\%03o' $(($3 & 255)) $((($3 >> 8) & 255)))" |
	    dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

# Write the unsigned 32 bit little-endian value $3 at offset $2 of $1.
poke32()
{
	printf "$(printf '\\%03o\\%03o\\%03o\\%03o' $(($3 & 255)) \
	    $((($3 >> 8) & 255)) $((($3 >> 16) & 255)) \
	    $((($3 >> 24) & 255)))" |
	    dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

# Write the ASCII string $3 at offset $2 of $1.
poke_str()
{
	printf '%s' "$3" | dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

# Read the file system geometry of $IMG out of its BPB and derive the two
# cluster numbers the tests below use.  Everything is taken from the image
# rather than assumed, so newfs_msdos(8) remains free to pick a different
# layout than the one requested.
#
# victimcl is an ordinary data cluster near the start of the volume and
# lostcl is exactly 4 GiB further into the volume, so that truncating the
# offset of lostcl to 32 bits yields the offset of victimcl.
fat32_geom()
{
	local totsec

	bps=$(bpb_read ${IMG} 11 2)
	spc=$(bpb_read ${IMG} 13 1)
	rsvd=$(bpb_read ${IMG} 14 2)
	nfats=$(bpb_read ${IMG} 16 1)
	totsec=$(bpb_read ${IMG} 32 4)
	fatsz=$(bpb_read ${IMG} 36 4)
	rootcl=$(bpb_read ${IMG} 44 4)

	clsz=$((spc * bps))
	fatoff=$((rsvd * bps))
	dataoff=$(((rsvd + nfats * fatsz) * bps))
	numclust=$(((totsec - rsvd - nfats * fatsz) / spc))

	victimcl=64
	lostcl=$((victimcl + 4294967296 / clsz))

	if [ "${lostcl}" -ge "${numclust}" ]; then
		atf_fail "image holds ${numclust} clusters, need ${lostcl}"
	fi
}

# Print the byte offset of cluster $1.
cloff()
{
	echo $((dataoff + ($1 - 2) * clsz))
}

# Set the FAT32 entry for cluster $2 to $3 in every copy of the FAT of $1.
fat_set()
{
	local i

	i=0
	while [ "${i}" -lt "${nfats}" ]; do
		poke32 "$1" $((fatoff + i * fatsz * bps + $2 * 4)) "$3"
		i=$((i + 1))
	done
}

# Write the 8.3 directory entry $2 at offset $1 of $IMG, with attribute $3,
# start cluster $4 and size $5.  Everything not written here is already zero
# in a freshly created file system, which is what the remaining fields need
# to be.
dirent()
{
	poke_str ${IMG} "$1" "$2"
	poke8 ${IMG} $(($1 + 11)) "$3"
	poke16 ${IMG} $(($1 + 20)) $(($4 >> 16))
	poke16 ${IMG} $(($1 + 26)) $(($4 & 65535))
	poke32 ${IMG} $(($1 + 28)) "$5"
}

# Create a 4.5 GiB FAT32 file system in $IMG.  newfs_msdos(8) -C only calls
# ftruncate(2), and nothing outside the reserved area, the FATs and a
# handful of clusters is ever written, so the image stays sparse.
#
# The volume has to be large enough that a cluster can sit a full 4 GiB
# beyond an ordinary data cluster, which 4.5 GiB satisfies for every cluster
# size newfs_msdos(8) may choose here.
make_image()
{
	atf_check -s exit:0 -o ignore -e ignore \
	    newfs_msdos -C 4608m -F 32 -c 64 -S 512 ./${IMG}
	fat32_geom
	# A freshly created file system must be clean.
	atf_check -s exit:0 -o ignore -e ignore fsck_msdosfs -y ./${IMG}
}

# Create an empty LOST.DIR in the root directory of $IMG, with cluster
# $lostcl holding its contents, so that reconnect() has somewhere to link a
# lost chain to.  That cluster begins more than 4 GiB into the volume.
create_lost_dir()
{
	local root dir

	fat_set ${IMG} ${lostcl} 268435455

	root=$(cloff ${rootcl})
	dir=$(cloff ${lostcl})

	# The entry in the root directory.  16 is ATTR_DIRECTORY.
	dirent ${root} 'LOST    DIR' 16 ${lostcl} 0

	# Its "." and ".." entries.  The remainder of the cluster stays
	# zero, which reads as SLOT_EMPTY, so reconnect() has free slots.
	dirent ${dir} '.          ' 16 ${lostcl} 0
	dirent $((dir + 32)) '..         ' 16 0 0
}

# Create PAYLOAD.BIN in the root directory of $IMG, occupying the single
# cluster $victimcl, which is exactly 4 GiB below the LOST.DIR cluster.
#
# The first 32 bytes are left zero on purpose: a truncated offset makes
# reconnect() search this cluster for a free directory slot, and a leading
# NUL reads as SLOT_EMPTY, so the bogus entry lands at a known place.  The
# rest carries a marker so the region is recognisable in a corrupted image.
create_payload()
{
	local data

	fat_set ${IMG} ${victimcl} 268435455

	data=$(cloff ${victimcl})
	poke_str ${IMG} $((data + 32)) 'PAYLOAD.BIN DATA - MUST NOT BE TOUCHED'

	# 32 is ATTR_ARCHIVE.
	dirent $(($(cloff ${rootcl}) + 32)) 'PAYLOAD BIN' 32 ${victimcl} ${clsz}
}

# Mark clusters 300, 301 and 302 of $IMG as an allocated chain in every copy
# of the FAT.  No directory entry refers to them, so fsck_msdosfs(8) has to
# find them as a lost chain in phase 3 and reconnect them into LOST.DIR.
inject_lost_chain()
{
	fat_set ${IMG} 300 301
	fat_set ${IMG} 301 302
	fat_set ${IMG} 302 268435455
}

# Copy the cluster $1 of $IMG into the file $2.  The size is checked so that
# a short read cannot turn the comparison below into a vacuous success.
save_cluster()
{
	dd if=${IMG} of="$2" bs=${bps} skip=$(($(cloff "$1") / bps)) \
	    count=${spc} status=none
	if [ "$(stat -f %z "$2")" -ne "${clsz}" ]; then
		atf_fail "could not read cluster $1 of ${IMG}"
	fi
}

atf_test_case reconnect_above_4g
reconnect_above_4g_head()
{
	atf_set "descr" "Reconnecting into a LOST.DIR past 4 GiB does not corrupt user data"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
reconnect_above_4g_body()
{
	make_image
	create_lost_dir
	create_payload

	# Adding the entries by hand must not have damaged anything.  This
	# also brings the free cluster count in the FSInfo block back in
	# line with the FAT, so the run below does not have to fix it.
	atf_check -s exit:0 -o ignore -e ignore fsck_msdosfs -y ./${IMG}

	inject_lost_chain
	save_cluster ${victimcl} victim.before

	# reconnect() has to link the lost chain into the LOST.DIR cluster
	# more than 4 GiB into the volume.  Computing that offset in 32 bit
	# arithmetic instead lands on PAYLOAD.BIN's cluster.
	atf_check -s exit:0 \
	    -o match:'Lost cluster chain at cluster 300' \
	    -o match:'3 Cluster\(s\) lost' \
	    -o match:'Reconnect\? yes' \
	    -e ignore \
	    fsck_msdosfs -y ./${IMG}

	# PAYLOAD.BIN is 4 GiB below LOST.DIR, so a truncated offset
	# rewrites its cluster with a directory entry in the first slot.
	save_cluster ${victimcl} victim.after
	atf_check cmp victim.before victim.after

	# The reconnect has to be durable: the chain is only referenced if
	# the entry reached the real LOST.DIR, so a second pass that still
	# reports it means the first one wrote somewhere else.
	atf_check -s exit:0 -o not-match:'Lost cluster chain' -e ignore \
	    fsck_msdosfs -y ./${IMG}
}

atf_init_test_cases()
{
	atf_add_test_case reconnect_above_4g
}
