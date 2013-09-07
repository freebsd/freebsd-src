#!/bin/sh
# $FreeBSD$
#
# This sample downloads the package INDEX file from FTP to /tmp (if it doesn't
# already exist) and then displays the package configuration/management screen
# using the local INDEX file (results in faster browsing of packages from-start
# since the INDEX can be loaded from local media).
#
# NOTE: Packages cannot be installed unless staged to /tmp/packages/All
#
. /usr/share/bsdconfig/script.subr
nonInteractive=1
TMPDIR=/tmp
if [ ! -e "$TMPDIR/packages/INDEX" ]; then
	[ -d "$TMPDIR/packages" ] || mkdir -p "$TMPDIR/packages" || exit 1
	_ftpPath=ftp://ftp.freebsd.org
	# For older releases, use ftp://ftp-archive.freebsd.org
	mediaSetFTP
	mediaOpen
	f_show_info "Downloading packages/INDEX from\n %s" "$_ftpPath" 
	f_device_get media packages/INDEX > $TMPDIR/packages/INDEX
fi
_directoryPath=$TMPDIR
mediaSetDirectory
configPackages
