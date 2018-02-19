#!/bin/sh
# $FreeBSD$

# Will image the test directory (default /tmp/loadertest) if it doesn't exist

die() {
    echo $*
    exit 1
}

dir=$1
scriptdir=$(dirname $(realpath $0))
cd $(make -V SRCTOP)/stand
obj=$(make -V .OBJDIR)
t=$obj/userboot/test/test
u=$obj/userboot/userboot/userboot.so

[ -n "$dir" ] || dir=/tmp/loadertest
[ -d "$dir" ] || ${scriptdir}/lua-img.sh ${dir}
[ -f "$dir/boot/lua/loader.lua" ] || die "No boot/lua/loader.lua found"
[ -f "$dir/boot/kernel/kernel" ] || die "No kernel to load"
[ -x "$t" ] || die "no userboot test jig found ($t)"
[ -x "$u" ] || die "no userboot.so ($u) found"

$t -h $dir -b $u
