#!/bin/sh
#
# Author:	Jordan Hubbard
# Date:		Mon Jul 10 01:18:20 2000
# Version:	$FreeBSD$
#
# MAINTAINER:	jkh
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

# The following are required if you obtained your packages from one of the
# package building clusters or otherwise had these defined when the packages
# were built.
export BATCH=t
export PACKAGE_BUILDING=t

# Don't pick up installed packages from the host
export LOCALBASE=/nonexistentlocal
export X11BASE=/nonexistentx
export PKG_DBDIR=/nonexistentdb

# usage: extract-names cd# [portsdir]
extract-names()
{
	portsdir=${2-/usr/ports}
	_FOO=`eval echo \\${CDROM_SET_$1}`
	if [ "${_FOO}" ]; then
		TMPNAME="/tmp/_extract_names$$"
		rm -f ${TMPNAME}
		for i in ${_FOO}; do
			( cd $portsdir/$i && PORTSDIR=$portsdir make package-name package-depends ) >> ${TMPNAME};
		done
		if [ -s "${TMPNAME}" ]; then
			sed -e 's/:.*$//' < ${TMPNAME} | sort -u
		fi
		rm -f ${TMPNAME}
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
CDROM_SET_1="${CDROM_SET_1} net/pcnfsd"
CDROM_SET_1="${CDROM_SET_1} net/rsync"
CDROM_SET_1="${CDROM_SET_1} x11-fonts/XFree86-4-font100dpi"
CDROM_SET_1="${CDROM_SET_1} x11-fonts/XFree86-4-font75dpi"
CDROM_SET_1="${CDROM_SET_1} x11-fonts/XFree86-4-fontCyrillic"
CDROM_SET_1="${CDROM_SET_1} x11-fonts/XFree86-4-fontDefaultBitmaps"
CDROM_SET_1="${CDROM_SET_1} x11-fonts/XFree86-4-fontEncodings"
CDROM_SET_1="${CDROM_SET_1} x11-fonts/XFree86-4-fontScalable"
CDROM_SET_1="${CDROM_SET_1} x11-servers/XFree86-4-FontServer"
CDROM_SET_1="${CDROM_SET_1} x11-servers/XFree86-4-NestServer"
CDROM_SET_1="${CDROM_SET_1} x11-servers/XFree86-4-PrintServer"
CDROM_SET_1="${CDROM_SET_1} x11-servers/XFree86-4-Server"
CDROM_SET_1="${CDROM_SET_1} x11-servers/XFree86-4-VirtualFramebufferServer"
CDROM_SET_1="${CDROM_SET_1} x11-wm/afterstep"
CDROM_SET_1="${CDROM_SET_1} x11-wm/enlightenment"
CDROM_SET_1="${CDROM_SET_1} x11-wm/fvwm2"
CDROM_SET_1="${CDROM_SET_1} x11-wm/sawfish"
CDROM_SET_1="${CDROM_SET_1} x11-wm/windowmaker"
CDROM_SET_1="${CDROM_SET_1} x11/XFree86-4"
CDROM_SET_1="${CDROM_SET_1} x11/XFree86-4-clients"
CDROM_SET_1="${CDROM_SET_1} x11/XFree86-4-documents"
CDROM_SET_1="${CDROM_SET_1} x11/XFree86-4-libraries"
CDROM_SET_1="${CDROM_SET_1} x11/XFree86-4-manuals"
CDROM_SET_1="${CDROM_SET_1} x11/gnome2"
CDROM_SET_1="${CDROM_SET_1} x11/kde3"
CDROM_SET_1="${CDROM_SET_1} www/links"

# This is the set of "people really want these" packages.  Please add to
# this list.
CDROM_SET_1="${CDROM_SET_1} astro/xearth"
CDROM_SET_1="${CDROM_SET_1} editors/emacs21"
CDROM_SET_1="${CDROM_SET_1} editors/vim-lite"
CDROM_SET_1="${CDROM_SET_1} editors/vim"
CDROM_SET_1="${CDROM_SET_1} emulators/mtools"
CDROM_SET_1="${CDROM_SET_1} ftp/ncftp"
CDROM_SET_1="${CDROM_SET_1} graphics/gimp1"
CDROM_SET_1="${CDROM_SET_1} graphics/xpdf"
CDROM_SET_1="${CDROM_SET_1} graphics/xv"
CDROM_SET_1="${CDROM_SET_1} irc/xchat"
CDROM_SET_1="${CDROM_SET_1} lang/perl5"
CDROM_SET_1="${CDROM_SET_1} mail/exim"
CDROM_SET_1="${CDROM_SET_1} mail/fetchmail"
CDROM_SET_1="${CDROM_SET_1} mail/mutt"
CDROM_SET_1="${CDROM_SET_1} mail/pine4"
CDROM_SET_1="${CDROM_SET_1} mail/popd"
CDROM_SET_1="${CDROM_SET_1} mail/xfmail"
CDROM_SET_1="${CDROM_SET_1} misc/screen"
CDROM_SET_1="${CDROM_SET_1} net/cvsup"
CDROM_SET_1="${CDROM_SET_1} net/samba"
CDROM_SET_1="${CDROM_SET_1} news/slrn"
CDROM_SET_1="${CDROM_SET_1} news/tin"
CDROM_SET_1="${CDROM_SET_1} print/a2ps-letter"
if [ "X`uname -m`" = "Xalpha" ]; then
CDROM_SET_1="${CDROM_SET_1} print/acroread4"
fi
if [ "X`uname -m`" = "Xi386" ]; then
CDROM_SET_1="${CDROM_SET_1} print/acroread5"
fi
CDROM_SET_1="${CDROM_SET_1} print/apsfilter"
CDROM_SET_1="${CDROM_SET_1} print/ghostscript-gnu-nox11"
CDROM_SET_1="${CDROM_SET_1} print/ghostview"
CDROM_SET_1="${CDROM_SET_1} print/gv"
CDROM_SET_1="${CDROM_SET_1} print/psutils-letter"
CDROM_SET_1="${CDROM_SET_1} security/sudo"
CDROM_SET_1="${CDROM_SET_1} shells/bash2"
CDROM_SET_1="${CDROM_SET_1} shells/pdksh"
CDROM_SET_1="${CDROM_SET_1} shells/zsh"
CDROM_SET_1="${CDROM_SET_1} sysutils/portupgrade"
CDROM_SET_1="${CDROM_SET_1} www/lynx"
CDROM_SET_1="${CDROM_SET_1} www/netscape-remote"
CDROM_SET_1="${CDROM_SET_1} www/netscape-wrapper"
CDROM_SET_1="${CDROM_SET_1} www/netscape48-communicator"
CDROM_SET_1="${CDROM_SET_1} www/netscape48-navigator"
CDROM_SET_1="${CDROM_SET_1} www/opera"
CDROM_SET_1="${CDROM_SET_1} x11/rxvt"

# VERY common build dependencies
CDROM_SET_1="${CDROM_SET_1} archivers/unzip"
CDROM_SET_1="${CDROM_SET_1} devel/gmake"
CDROM_SET_1="${CDROM_SET_1} graphics/png"
CDROM_SET_1="${CDROM_SET_1} misc/compat22"
CDROM_SET_1="${CDROM_SET_1} misc/compat3x"
CDROM_SET_1="${CDROM_SET_1} misc/compat4x"

## End of set for CDROM #1

## Start of set for CDROM #2
## Live file system, CVS repositories, and commerical software demos
## typically live on this disc.  Users do not expect to find packages
## here.
## End of set for CDROM #2

## Start of set for CDROM #3
CDROM_SET_3="${CDROM_SET_3} editors/xemacs21"
CDROM_SET_3="${CDROM_SET_3} lang/gnat"
CDROM_SET_3="${CDROM_SET_3} net/cvsup-without-gui"
CDROM_SET_3="${CDROM_SET_3} print/teTeX"
CDROM_SET_3="${CDROM_SET_3} textproc/docproj-jadetex"

## End of set for CDROM #3

## Start of set for CDROM #4
## End of set for CDROM #4


## Start of set that should not be included on any CDROM.
## This should not contain packages that are already marked BROKEN or
## RESTRICTED, it is only for packages that sysinstall(8) has trouble
## with.
NO_CDROM_SET=""
NO_CDROM_SET="${NO_CDROM_SET} net/cvsupit"

# Start of actual script.
if [ $# -lt 1 ]; then
	echo "usage: $0 cdrom-number [portsdir]"
	exit 2
fi
if [ ${1} = 0 ]; then
	echo $NO_CDROM_SET
else
	extract-names $*
fi
exit 0
