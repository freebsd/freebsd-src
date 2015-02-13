#!/bin/sh

# make-manifest.sh: create checksums and package descriptions for the installer
#
#  Usage: make-manifest.sh foo1.txz foo2.txz ...
#
# The output file looks like this (tab-delimited):
#  foo1.txz SHA256-checksum Number-of-files foo1 Description Install-by-default
#
# $FreeBSD$

base="Base system"
doc="Additional Documentation"
games="Games (fortune, etc.)"
kernel="Kernel"
ports="Ports tree"
src="System source tree"
lib32="32-bit compatibility libraries"
tests="Test suite"

desc_base="${base} (MANDATORY)"
desc_base_dbg="${base} (Debugging)"
desc_doc="${doc}"
desc_games="${games}"
desc_games_dbg="${games} (Debugging)"
desc_kernel="${kernel} (MANDATORY)"
desc_kernel_symbols="${kernel} (Debugging symbols)"
desc_kernel_alt="Alternate ${kernel}"
desc_lib32="${lib32}"
desc_lib32_dbg="${lib32} (Debugging)"
desc_ports="${ports}"
desc_src="${src}"
desc_tests="${tests}"

default_doc=off
default_src=off
default_tests=off
default_base_dbg=off
default_games_dbg=off
default_lib32_dbg=off
default_kernel_alt=off
default_kernel_symbols=on

for i in ${*}; do
	dist="${i}"
	distname="${i%%.txz}"
	distname="$(echo ${distname} | sed -E 's/-dbg/_dbg/')"
	distname="$(echo ${distname} | sed -E 's/kernel\..*/kernel_alt/')"
	hash="$(sha256 -q ${i})"
	nfiles="$(tar tvf ${i} | wc -l | tr -d ' ')"
	default="$(eval echo \${default_${distname}:-on})"
	desc="$(eval echo \"\${desc_${distname}}\")"

	case ${i} in
		kernel.*.*)
			desc="${desc} \($(echo ${i%%.txz} | cut -f 2 -d '.')\)"
			;;
		*)
			;;
	esac

	printf "${dist}\t${hash}\t${nfiles}\t${distname}\t\"${desc}\"\t${default}\n"
done

