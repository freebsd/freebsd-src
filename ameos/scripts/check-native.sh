#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Real native build/image-tool smoke check, not a full world or boot test.
set -eu

fail()
{
	printf 'ameos-native: %s\n' "$*" >&2
	exit 1
}

[ "$#" -eq 2 ] && [ "$1" = --workdir ] || fail 'usage: sh ameos/scripts/check-native.sh --workdir /new/path'
workdir=$2
scriptdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
root=$(CDPATH= cd -- "$scriptdir/../.." && pwd -P)
# Reuse path, clean-source, host and resource validation. No output is created.
sh "$scriptdir/build-release.sh" --check --profile compact-amd64 --jobs 1 --workdir "$workdir"
for tool in cc file; do
	command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done
parent=$(dirname "$workdir")
parent=$(cd "$parent" && pwd -P)
workdir="$parent/$(basename "$workdir")"
unset MAKEFLAGS MFLAGS MAKEOBJDIRPREFIX MAKEOBJDIR DESTDIR SRCCONF __MAKE_CONF
export LC_ALL=C TZ=UTC
umask 022
mkdir "$workdir"
mkdir "$workdir/obj"
export MAKEOBJDIRPREFIX="$workdir/obj"
native_make()
{
	make -C "$root/bin/echo" __MAKE_CONF=/dev/null SRCCONF=/dev/null "$@"
}
printf 'Building bin/echo from source; log: %s/native-build.log\n' "$workdir"
native_make obj > "$workdir/native-build.log" 2>&1
native_make all >> "$workdir/native-build.log" 2>&1
objdir=$(native_make -V .OBJDIR)
case "$objdir/" in "$workdir/obj/"*) ;; *) fail 'unexpected object directory' ;; esac
output=$("$objdir/echo" 'AmeOS native build smoke')
[ "$output" = 'AmeOS native build smoke' ] || fail 'built echo returned unexpected output'
file "$objdir/echo" > "$workdir/binary-info.txt"

mkdir -p "$workdir/image-root/etc"
printf 'AmeOS native image-tool probe\n' > "$workdir/image-root/etc/ameos-probe"
makefs -t ffs -s 8m -o version=2 "$workdir/probe.ufs" "$workdir/image-root" > "$workdir/image-tools.log" 2>&1
mkimg -s gpt -p "freebsd-ufs:=$workdir/probe.ufs" -o "$workdir/probe.img" >> "$workdir/image-tools.log" 2>&1
[ -s "$workdir/probe.img" ] || fail 'image was not created'
(
	cd "$workdir"
	sha256 probe.ufs probe.img > CHECKSUM.SHA256
)
cat > "$workdir/PROBE-RESULTS.txt" <<EOF
result=PASS
source_commit=$(git -C "$root" rev-parse HEAD)
host=$(uname -srmp)
uid=$(id -u)
native_echo_build=PASS
native_echo_execution=PASS
ufs_image_creation=PASS
gpt_image_creation=PASS
full_world_build=NOT_RUN
boot_test=NOT_RUN
EOF
printf 'Native smoke check passed: %s/PROBE-RESULTS.txt\n' "$workdir"
printf '%s\n' 'The probe image contains test data only and is not bootable.'
