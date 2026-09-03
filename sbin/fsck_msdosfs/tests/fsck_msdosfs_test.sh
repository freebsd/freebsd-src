#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The FreeBSD Foundation
#

# Tests for fsck_msdosfs(8) phase 3 ("Checking for Lost Files") repair
# accounting: a lost cluster chain that fsck_msdosfs(8) has repaired must
# not be reported as an unrecovered error, and one that it has left alone
# must be.

IMG=fat16.img

# Read an unsigned little-endian integer of $3 bytes at offset $2 of $1.
bpb_read()
{
	od -An -v -tu1 -j "$2" -N "$3" "$1" | awk '
	    { for (i = 1; i <= NF; i++) b[n++] = $i }
	    END { v = 0; for (i = n - 1; i >= 0; i--) v = v * 256 + b[i]
		  print v }'
}

# Write the unsigned 16 bit little-endian value $3 at offset $2 of $1.
poke16()
{
	printf "$(printf '\\%03o\\%03o' $(($3 & 255)) $((($3 >> 8) & 255)))" |
	    dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

# Write the unsigned 8 bit value $3 at offset $2 of $1.
poke8()
{
	printf "$(printf '\\%03o' $(($3 & 255)))" |
	    dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

# Write the ASCII string $3 at offset $2 of $1.
poke_str()
{
	printf '%s' "$3" | dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

# Create a 4 MiB FAT16 file system in $IMG.  One sector per cluster keeps
# the cluster numbers used below well inside the data area.
make_image()
{
	atf_check -s exit:0 -o ignore -e ignore \
	    newfs_msdos -C 4m -F 16 -c 1 -S 512 ./${IMG}
	# A freshly created file system must be clean.
	atf_check -s exit:0 -o ignore -e ignore fsck_msdosfs -y ./${IMG}
}

# Mark clusters 300, 301 and 302 of $IMG as an allocated chain in every
# copy of the FAT.  No directory entry refers to them, so fsck_msdosfs(8)
# has to find them as a lost chain in phase 3.  These cluster numbers are
# used because neither byte of their little-endian FAT16 encoding is NUL.
inject_lost_chain()
{
	local bps rsvd nfats fatsz i base

	bps=$(bpb_read ${IMG} 11 2)
	rsvd=$(bpb_read ${IMG} 14 2)
	nfats=$(bpb_read ${IMG} 16 1)
	fatsz=$(bpb_read ${IMG} 22 2)

	i=0
	while [ "${i}" -lt "${nfats}" ]; do
		base=$((rsvd * bps + i * fatsz * bps))
		poke16 ${IMG} $((base + 300 * 2)) 301
		poke16 ${IMG} $((base + 301 * 2)) 302
		poke16 ${IMG} $((base + 302 * 2)) 65535
		i=$((i + 1))
	done
}

# Mark clusters 300 and 301 of $IMG as an allocated lost chain where 300 points
# to 301 and 301 points to CLUST_FREE (0).  No directory entry refers to them, so
# checklost() finds cluster 300 as a lost chain head, but checkchain() fails
# because the chain ends unexpectedly with a free cluster.
inject_corrupted_lost_chain()
{
	local bps rsvd nfats fatsz i base

	bps=$(bpb_read ${IMG} 11 2)
	rsvd=$(bpb_read ${IMG} 14 2)
	nfats=$(bpb_read ${IMG} 16 1)
	fatsz=$(bpb_read ${IMG} 22 2)

	i=0
	while [ "${i}" -lt "${nfats}" ]; do
		base=$((rsvd * bps + i * fatsz * bps))
		poke16 ${IMG} $((base + 300 * 2)) 301
		poke16 ${IMG} $((base + 301 * 2)) 0
		i=$((i + 1))
	done
}

# Create an empty LOST.DIR in the root directory of $IMG, with cluster 400
# holding its contents, so that reconnect() has somewhere to link a lost
# chain to.  Everything not written here is already zero in a freshly
# created file system, which is what these fields need to be.
create_lost_dir()
{
	local bps spc rsvd nfats rootent fatsz i base rootoff dataoff dir

	bps=$(bpb_read ${IMG} 11 2)
	spc=$(bpb_read ${IMG} 13 1)
	rsvd=$(bpb_read ${IMG} 14 2)
	nfats=$(bpb_read ${IMG} 16 1)
	rootent=$(bpb_read ${IMG} 17 2)
	fatsz=$(bpb_read ${IMG} 22 2)

	i=0
	while [ "${i}" -lt "${nfats}" ]; do
		base=$((rsvd * bps + i * fatsz * bps))
		poke16 ${IMG} $((base + 400 * 2)) 65535
		i=$((i + 1))
	done

	rootoff=$(((rsvd + nfats * fatsz) * bps))
	dataoff=$((rootoff + rootent * 32))
	dir=$((dataoff + (400 - 2) * spc * bps))

	# The entry in the root directory.  16 is ATTR_DIRECTORY.
	poke_str ${IMG} ${rootoff} 'LOST    DIR'
	poke8 ${IMG} $((rootoff + 11)) 16
	poke16 ${IMG} $((rootoff + 26)) 400

	# Its "." and ".." entries.  The remainder of the cluster stays
	# zero, which reads as SLOT_EMPTY, so reconnect() has free slots.
	poke_str ${IMG} ${dir} '.          '
	poke8 ${IMG} $((dir + 11)) 16
	poke16 ${IMG} $((dir + 26)) 400
	poke_str ${IMG} $((dir + 32)) '..         '
	poke8 ${IMG} $((dir + 43)) 16
}

atf_test_case lost_chain_cleared
lost_chain_cleared_head()
{
	atf_set "descr" "A lost chain that was cleared is not an error"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
lost_chain_cleared_body()
{
	make_image
	inject_lost_chain

	# There is no LOST.DIR, so reconnect() fails and fsck_msdosfs(8)
	# falls back to clearing the chain.  That repairs the file system,
	# so the exit status must be 0 and not 8 (unrecovered error).
	atf_check -s exit:0 \
	    -o match:'Lost cluster chain at cluster 300' \
	    -o match:'3 Cluster\(s\) lost' \
	    -o match:'No LOST.DIR directory' \
	    -o match:'Clear\? yes' \
	    -e ignore \
	    fsck_msdosfs -y ./${IMG}

	# The repair has to be durable: a second pass must find nothing.
	atf_check -s exit:0 -o not-match:'Lost cluster chain' -e ignore \
	    fsck_msdosfs -y ./${IMG}
}

atf_test_case lost_chain_left_alone
lost_chain_left_alone_head()
{
	atf_set "descr" "A lost chain that was not repaired is an error"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
lost_chain_left_alone_body()
{
	make_image
	inject_lost_chain
	cp ${IMG} ${IMG}.save

	# In -n mode nothing is repaired, so the lost chain must still be
	# reported as an unrecovered error and the image must not change.
	atf_check -s exit:8 -o match:'Lost cluster chain at cluster 300' \
	    -e ignore fsck_msdosfs -n ./${IMG}
	atf_check cmp ${IMG}.save ${IMG}
}

atf_test_case lost_chain_preen
lost_chain_preen_head()
{
	atf_set "descr" "Preen mode reports a lost chain it cannot reconnect"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
lost_chain_preen_body()
{
	make_image
	inject_lost_chain

	# Preen mode attempts the reconnect but never clears, so with no
	# LOST.DIR the chain stays lost and has to be reported.
	atf_check -s exit:8 -o match:'Lost cluster chain at cluster 300' \
	    -o match:'No LOST.DIR directory' -e ignore \
	    fsck_msdosfs -p -f ./${IMG}
}

atf_test_case corrupted_lost_chain_left_alone
corrupted_lost_chain_left_alone_head()
{
	atf_set "descr" "A corrupted lost chain that checkchain fails on is an error if left alone"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
corrupted_lost_chain_left_alone_body()
{
	make_image
	inject_corrupted_lost_chain
	cp ${IMG} ${IMG}.save

	# In -n mode nothing is repaired, so checkchain() returns FSERROR when
	# the chain ends unexpectedly with CLUST_FREE.  checklost() must not
	# swallow this FSERROR, so fsck_msdosfs must exit 8 and leave the image unchanged.
	atf_check -s exit:8 \
	    -o match:'Cluster chain starting at 300 ends with cluster marked free' \
	    -e ignore fsck_msdosfs -n ./${IMG}
	atf_check cmp ${IMG}.save ${IMG}
}

atf_test_case corrupted_lost_chain_reconnected
corrupted_lost_chain_reconnected_head()
{
	atf_set "descr" "Truncating a lost chain before reconnecting it is written out"
	atf_set "require.progs" "newfs_msdos fsck_msdosfs"
}
corrupted_lost_chain_reconnected_body()
{
	make_image
	create_lost_dir
	# Adding LOST.DIR by hand must not have damaged anything.
	atf_check -s exit:0 -o ignore -e ignore fsck_msdosfs -y ./${IMG}
	inject_corrupted_lost_chain

	# checkchain() truncates the chain (FSFATMOD) and reconnect() then
	# links it into LOST.DIR (FSDIRMOD).  Both results have to reach
	# mod: without the FSFATMOD, checkfilesys() never writes the FATs
	# back and the truncation is silently discarded.
	atf_check -s exit:0 \
	    -o match:'Cluster chain starting at 300 ends with cluster marked free' \
	    -o match:'Truncate\? yes' \
	    -o match:'Lost cluster chain at cluster 300' \
	    -o match:'Update FATs\? yes' \
	    -e ignore \
	    fsck_msdosfs -y ./${IMG}

	# The truncation has to be durable: a second pass must find nothing.
	atf_check -s exit:0 -o not-match:'ends with cluster marked free' \
	    -e ignore fsck_msdosfs -y ./${IMG}
}

atf_init_test_cases()
{
	atf_add_test_case lost_chain_cleared
	atf_add_test_case lost_chain_left_alone
	atf_add_test_case lost_chain_preen
	atf_add_test_case corrupted_lost_chain_left_alone
	atf_add_test_case corrupted_lost_chain_reconnected
}
