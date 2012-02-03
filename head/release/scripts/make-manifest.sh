#!/bin/sh

# make-manifest.sh: create checksums and package descriptions for the installer
#
#  Usage: make-manifest.sh foo1.txz foo2.txz ...
#
# The output file looks like this (tab-delimited):
#  foo1.txz SHA256-checksu Number-of-files foo1 Description Install-by-default
#
# $FreeBSD$

desc_base="Base system (MANDATORY)"
desc_kernel="Kernel (MANDATORY)"
desc_doc="Additional documentation"
doc_default=off
desc_games="Games (fortune, etc.)"
desc_lib32="32-bit compatibility libraries"
desc_ports="Ports tree"
desc_src="System source code"
src_default=off

for i in $*; do
	echo "`basename $i`	`sha256 -q $i`	`tar tvf $i | wc -l | tr -d ' '`	`basename $i .txz`	\"`eval echo \\\$desc_$(basename $i .txz)`\"	`eval echo \\\${$(basename $i .txz)_default:-on}`"
done

