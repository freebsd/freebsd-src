#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Build a committed source snapshot using the upstream release Makefile.
set -eu

fail()
{
	printf 'ameos: %s\n' "$*" >&2
	exit 1
}

usage()
{
	cat <<'EOF'
Usage: sh ameos/scripts/build-release.sh [--plan|--check] [--profile NAME] [--jobs N] --workdir /new/path

  --plan       Show inputs and stages without creating files (any POSIX host).
  --check      Check native FreeBSD tools and resources without building.
  --profile    base-amd64 (default), compact-amd64 or v0.2-b-amd64.
  --jobs N     Parallel build jobs; default: 2.
  --workdir    New, absolute directory outside the source checkout.
  --help       Show this help.

Builds on native FreeBSD/amd64 as the current user, using NO_ROOT staging.
The checkout must be clean for a build. No network fetches are performed.
EOF
}

plan=no
check=no
profile=base-amd64
jobs=2
workdir=
while [ "$#" -gt 0 ]; do
	case "$1" in
	--plan) plan=yes; shift ;;
	--check) check=yes; shift ;;
	--jobs|--workdir|--profile)
		[ "$#" -ge 2 ] || fail "missing value for $1"
		case "$1" in
		--jobs) jobs=$2 ;;
		--workdir) workdir=$2 ;;
		--profile) profile=$2 ;;
		esac
		shift 2
		;;
	--help) usage; exit 0 ;;
	*) fail "unknown argument: $1" ;;
	esac
done
[ "$plan:$check" != yes:yes ] || fail '--plan and --check are mutually exclusive'
case "$profile" in base-amd64|compact-amd64|v0.2-b-amd64) ;; *) fail 'unknown build profile' ;; esac

case "$jobs" in ''|*[!0-9]*|0|0*) fail '--jobs must be a positive integer' ;; esac
case "$workdir" in
/*) ;;
*) fail '--workdir must be an absolute path' ;;
esac
# Upstream release recipes contain unquoted paths. Restrict inputs accordingly.
case "$workdir" in
*[!a-zA-Z0-9_./-]*|*/../*|*/./*|*/..|*/.|*/|*//*)
	fail '--workdir must use simple path components without a trailing slash' ;;
esac
[ ! -e "$workdir" ] && [ ! -L "$workdir" ] || fail 'workdir already exists'
parent=$(dirname "$workdir")
[ -d "$parent" ] || fail 'workdir parent must already exist'
parent=$(cd "$parent" && pwd -P)
workdir="$parent/$(basename "$workdir")"
case "$workdir" in
*[!a-zA-Z0-9_./-]*) fail 'resolved workdir contains unsupported characters' ;;
esac

scriptdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
root=$(CDPATH= cd -- "$scriptdir/../.." && pwd -P)
case "$root" in *[!a-zA-Z0-9_./-]*) fail 'source path must not contain spaces or shell metacharacters' ;; esac
case "$workdir/" in "$root/"*) fail 'workdir must be outside the source checkout' ;; esac
command -v git >/dev/null 2>&1 || fail 'git is required'
[ "$(git -C "$root" rev-parse --show-prefix)" = '' ] || fail 'expected a repository root'

base=$(cat "$root/ameos/base-revision")
case "$base" in ''|*[!0-9a-f]*) fail 'invalid base revision' ;; esac
[ "${#base}" -eq 40 ] || fail 'base revision must be a full Git SHA-1'
commit=$(git -C "$root" rev-parse --verify 'HEAD^{commit}')
git -C "$root" merge-base --is-ancestor "$base" "$commit" || fail 'pinned base is not an ancestor of HEAD'
epoch=$(git -C "$root" show -s --format=%ct "$commit")

# Profiles are trusted source files reviewed with the rest of this repository.
. "$root/ameos/profiles/$profile.conf"
[ "$TARGET:$TARGET_ARCH:$KERNCONF" = 'amd64:amd64:GENERIC' ] || fail 'unsupported target profile'
case "$AMEOS_VERSION" in ''|*[!a-zA-Z0-9.-]*) fail 'invalid AmeOS version' ;; esac
version_file="$root/sys/conf/newvers.sh"
freebsd_type=$(sed -n 's/^TYPE="\([^"]*\)"$/\1/p' "$version_file")
freebsd_revision=$(sed -n 's/^REVISION="\([^"]*\)"$/\1/p' "$version_file")
freebsd_branch=$(sed -n 's/^BRANCH="\([^"]*\)"$/\1/p' "$version_file")
[ "$freebsd_type" = FreeBSD ] || fail 'unexpected base system type'
case "$freebsd_revision" in ''|*[!0-9.]*) fail 'invalid FreeBSD revision' ;; esac
case "$freebsd_branch" in ''|*[!a-zA-Z0-9._-]*) fail 'invalid FreeBSD branch' ;; esac
release="AmeOS-${AMEOS_VERSION}-${TARGET_ARCH}"
dirty=$(git -C "$root" status --porcelain --untracked-files=normal)

cat <<EOF
Profile: $profile (console baseline; AmeDE is not included)
Source repository: https://github.com/ntych/AmeOS
Pinned FreeBSD base: $base
Source snapshot: $commit
Source date epoch: $epoch
Target/kernel: $TARGET/$TARGET_ARCH/$KERNCONF
Base system version: $freebsd_type $freebsd_revision-$freebsd_branch
Release filename prefix: $release
Work directory: $workdir
Artifacts: $workdir/artifacts
Jobs: $jobs
Stages: snapshot -> buildworld -> buildkernel -> release -> install artifacts
External packages/ports/pkgbase: disabled
Host installation: none; release install uses DESTDIR
EOF
if [ -n "$dirty" ]; then
	if [ "$plan" = yes ]; then
		printf '%s\n' 'NOTE: checkout has uncommitted files; commit them before building.'
	else
		fail 'checkout is dirty; commit reviewed changes before building'
	fi
fi
[ "$plan" = no ] || exit 0

[ "$(uname -s)" = FreeBSD ] || fail 'full release builds require native FreeBSD'
[ "$(uname -m)" = amd64 ] || fail 'this profile requires an amd64 host'
for tool in make tar sha256 sha512 makefs mkimg etdump df awk sysctl swapinfo; do
	command -v "$tool" >/dev/null 2>&1 || fail "missing host tool: $tool"
done

check_space()
{
	available_mib=$(df -Pk "$parent" | awk 'END { print int($4 / 1024) }')
	case "$available_mib" in ''|*[!0-9]*) fail 'cannot determine free disk space' ;; esac
	[ "$available_mib" -ge 2048 ] || fail 'less than 2048 MiB free; refusing to start the next stage'
}
check_space
memory_mib=$(sysctl -n hw.physmem | awk '{ print int($1 / 1048576) }')
swap_mib=$(swapinfo -k | awk 'NR > 1 { total += $2 } END { print int(total / 1024) }')
printf 'Host resources: %s MiB disk free, %s MiB RAM, %s MiB swap\n' "$available_mib" "$memory_mib" "$swap_mib"
if [ "$memory_mib" -lt 4096 ] && [ "$swap_mib" -eq 0 ]; then
	printf '%s\n' 'WARNING: little RAM and no swap; compilation can fail from memory exhaustion.' >&2
fi
printf '%s\n' 'Resource checks are prerequisites, not a guarantee that a full build fits.'
[ "$check" = no ] || exit 0
# Do not inherit make overrides from a developer shell.
unset MAKEFLAGS MFLAGS MAKEOBJDIRPREFIX MAKEOBJDIR DESTDIR SRCCONF __MAKE_CONF BRANCH_OVERRIDE
export LC_ALL=C TZ=UTC
SOURCE_DATE_EPOCH=$epoch
export SOURCE_DATE_EPOCH
umask 022

# mkdir is deliberately non-recursive: an existing build is never reused.
mkdir "$workdir"
mkdir "$workdir/src" "$workdir/obj" "$workdir/artifacts"
git -C "$root" archive --format=tar --output="$workdir/source.tar" "$commit"
tar -xf "$workdir/source.tar" -C "$workdir/src"
# The original committed checkout remains available for recreating this archive.
rm -f "$workdir/source.tar"
src="$workdir/src"
export MAKEOBJDIRPREFIX="$workdir/obj"
cat > "$workdir/artifacts/BUILD-INFO.txt" <<EOF
profile=$profile
ameos_version=$AMEOS_VERSION
source_commit=$commit
freebsd_base=$base
freebsd_version=$freebsd_revision-$freebsd_branch
source_date_epoch=$epoch
host=$(uname -srmp)
jobs=$jobs
EOF
cp "$src/ameos/profiles/$profile.conf" "$workdir/artifacts/"
cp "$src/ameos/profiles/$profile.src.conf" "$workdir/artifacts/"

build_make()
{
	make -C "$src" -j "$jobs" __MAKE_CONF=/dev/null \
	    SRCCONF="$src/ameos/profiles/$profile.src.conf" \
	    TARGET="$TARGET" TARGET_ARCH="$TARGET_ARCH" KERNCONF="$KERNCONF" \
	    SOURCE_DATE_EPOCH="$epoch" "$@"
}
release_make()
{
	make -C "$src/release" __MAKE_CONF=/dev/null \
	    SRCCONF="$src/ameos/profiles/$profile.src.conf" \
	    TARGET="$TARGET" TARGET_ARCH="$TARGET_ARCH" KERNCONF="$KERNCONF" \
	    SOURCE_DATE_EPOCH="$epoch" PKG_TIMESTAMP="$epoch" \
	    NOPORTS=yes NOSRC=yes NOPKG=yes NOPKGBASE=yes \
	    WITH_DVD= WITH_VMIMAGES= WITH_CLOUDWARE= WITH_OCIIMAGES= \
	    WITH_COMPRESSED_IMAGES= WITH_COMPRESSED_VMIMAGES= \
	    TYPE="$freebsd_type" REVISION="$freebsd_revision" BRANCH="$freebsd_branch" \
	    NO_ROOT=yes WITHOUT_QEMU=yes OSRELEASE="$release" "$@"
}
# Redirect each stage directly: failures cannot be hidden by a tee pipeline.
printf 'Building world; log: %s/buildworld.log\n' "$workdir"
check_space
build_make buildworld > "$workdir/buildworld.log" 2>&1
printf 'Building kernel; log: %s/buildkernel.log\n' "$workdir"
check_space
build_make buildkernel > "$workdir/buildkernel.log" 2>&1
printf 'Creating release media; log: %s/release.log\n' "$workdir"
check_space
release_make release > "$workdir/release.log" 2>&1
check_space
release_make install DESTDIR="$workdir/artifacts" > "$workdir/install.log" 2>&1
# A successful command sequence must also produce the expected offline media.
[ -s "$workdir/artifacts/$release-disc1.iso" ] || fail 'missing disc1 ISO'
[ -s "$workdir/artifacts/$release-memstick.img" ] || fail 'missing memstick image'
(
	cd "$workdir/artifacts"
	sha256 "$release"-* > CHECKSUM.SHA256
	sha512 "$release"-* > CHECKSUM.SHA512
)
printf 'Release media created in %s/artifacts; boot testing is still required.\n' "$workdir"
