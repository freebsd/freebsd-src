#!/bin/sh
#
# Author:	Jordan Hubbard
# Date:		Mon Jul 10 01:18:20 2000
# Version:	$FreeBSD: src/release/scripts/print-cdrom-packages.sh,v 1.5.2.1 2000/07/22 23:46:07 obrien Exp $
#
# This script prints out the list of "minimum required packages" for
# a given CDROM number, that numer currently referring to the 4 CD
# "official set" published by BSDi.  If there is no minimum package
# set for the given CDROM, or none is known, the script will exit
# with a error code of 1.  At some point, this script should be extended
# to at least cope with other official CD distributions, like non-US ones.
#
# usage: print-cdrom-packages.sh cdrom-number
#
# example: ./print-cdrom-packages.sh 1
# will print the minimal package set for the first cdrom (what's generally
# referred to as the installation boot CD).
#
# This information is codified in script form so that some definitive
# reference for the package set info exists rather than having it
# be left up to everybody's best guess.  It's currently hard-coded directly
# into the script but may, at some point, switch to a more sophisticated
# data-extraction technique from the ports collection.  For now, add your
# packages to the appropriate CDROM_SET_<n> variable as /usr/ports/<your-entry>
# so that the package name and dependency list for each can be at least be
# obtained in an automated fashion.

# usage: extract-names cd#
extract-names()
{
	_FOO=`eval echo \\${CDROM_SET_$1}`
	if [ "${_FOO}" ]; then
		TMPNAME="/tmp/_extract_names$$"
		rm -f ${TMPNAME}
		for i in ${_FOO}; do
			( cd /usr/ports/$i && make package-name package-depends ) >> ${TMPNAME};
		done
		if [ -s "${TMPNAME}" ]; then
			sort ${TMPNAME} | uniq
		fi
	else
		exit 1
	fi
}


## Start of set for CDROM #1
# This is the set required by sysinstall.
CDROM_SET_1=""
if [ "X`uname -m`" = "Xalpha" ]; then
CDROM_SET_1="${CDROM_SET_1} emulators/osf1_base"
else
CDROM_SET_1="${CDROM_SET_1} emulators/linux_base"
fi
CDROM_SET_1="${CDROM_SET_1} x11/kde11"
CDROM_SET_1="${CDROM_SET_1} x11/gnome"
CDROM_SET_1="${CDROM_SET_1} x11-wm/afterstep"
CDROM_SET_1="${CDROM_SET_1} x11-wm/enlightenment"
CDROM_SET_1="${CDROM_SET_1} x11-wm/fvwm2"
CDROM_SET_1="${CDROM_SET_1} net/pcnfsd"

# This is the set of "people really want these" packages.  Please add to
# this list.
if [ "X`uname -m`" = "Xi386" ]; then
CDROM_SET_1="${CDROM_SET_1} shells/ksh93"
fi
CDROM_SET_1="${CDROM_SET_1} shells/bash2"
CDROM_SET_1="${CDROM_SET_1} shells/pdksh"
CDROM_SET_1="${CDROM_SET_1} editors/emacs20"
CDROM_SET_1="${CDROM_SET_1} editors/vim5"
CDROM_SET_1="${CDROM_SET_1} editors/vim-lite"
CDROM_SET_1="${CDROM_SET_1} www/netscape-wrapper"
CDROM_SET_1="${CDROM_SET_1} www/netscape-remote"
CDROM_SET_1="${CDROM_SET_1} www/netscape47-communicator"
CDROM_SET_1="${CDROM_SET_1} www/netscape47-navigator"
CDROM_SET_1="${CDROM_SET_1} print/acroread"

# VERY common build dependancies
CDROM_SET_1="${CDROM_SET_1} devel/gmake"
CDROM_SET_1="${CDROM_SET_1} archivers/bzip2"

## End of set for CDROM #1

## Start of set for CDROM #2
## End of set for CDROM #2

## Start of set for CDROM #3
## End of set for CDROM #3

## Start of set for CDROM #4
## End of set for CDROM #4

# Start of actual script.
if [ $# -lt 1 ]; then
	echo "usage: $0 cdrom-number"
	exit 2
fi
extract-names $1
exit 0
