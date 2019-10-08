#!/bin/sh
#-
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright 2018 Allan Jude <allanjude@freebsd.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted providing that the following conditions 
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$

############################################################ CONFIGURATION

: ${DESTDIR:=}
: ${TRUSTPATH:=${DESTDIR}/usr/share/certs/trusted:${DESTDIR}/usr/local/share/certs:${DESTDIR}/usr/local/etc/ssl/certs}
: ${BLACKLISTPATH:=${DESTDIR}/usr/share/certs/blacklisted:${DESTDIR}/usr/local/etc/ssl/blacklisted}
: ${CERTDESTDIR:=${DESTDIR}/etc/ssl/certs}
: ${BLACKLISTDESTDIR:=${DESTDIR}/etc/ssl/blacklisted}
: ${EXTENSIONS:="*.pem *.crt *.cer *.crl *.0"}
: ${VERBOSE:=0}

############################################################ GLOBALS

SCRIPTNAME="${0##*/}"
ERRORS=0
NOOP=0

############################################################ FUNCTIONS

do_hash()
{
	local hash

	if hash=$( openssl x509 -noout -subject_hash -in "$1" ); then
		echo "$hash"
		return 0
	else
		echo "Error: $1" >&2
		ERRORS=$(( $ERRORS + 1 ))
		return 1
	fi
}

create_trusted_link()
{
	local hash

	hash=$( do_hash "$1" ) || return
	if [ -e "$BLACKLISTDESTDIR/$hash.0" ]; then
		echo "Skipping blacklisted certificate $1 ($BLACKLISTDESTDIR/$hash.0)"
		return 1
	fi
	[ $VERBOSE -gt 0 ] && echo "Adding $hash.0 to trust store"
	[ $NOOP -eq 0 ] && ln -fs $(realpath "$1") "$CERTDESTDIR/$hash.0"
}

create_blacklisted()
{
	local hash srcfile filename

	# If it exists as a file, we'll try that; otherwise, we'll scan
	if [ -e "$1" ]; then
		hash=$( do_hash "$1" ) || return
		srcfile=$(realpath "$1")
		filename="$hash.0"
	elif [ -e "${CERTDESTDIR}/$1" ];  then
		srcfile=$(realpath "${CERTDESTDIR}/$1")
		filename="$1"
	else
		return
	fi
	[ $VERBOSE -gt 0 ] && echo "Adding $filename to blacklist"
	[ $NOOP -eq 0 ] && ln -fs "$srcfile" "$BLACKLISTDESTDIR/$filename"
}

do_scan()
{
	local CFUNC CSEARCH CPATH CFILE
	local oldIFS="$IFS"
	CFUNC="$1"
	CSEARCH="$2"

	IFS=:
	set -- $CSEARCH
	IFS="$oldIFS"
	for CPATH in "$@"; do
		[ -d "$CPATH" ] || continue
		echo "Scanning $CPATH for certificates..."
		cd "$CPATH"
		for CFILE in $EXTENSIONS; do
			[ -e "$CFILE" ] || continue
			[ $VERBOSE -gt 0 ] && echo "Reading $CFILE"
			"$CFUNC" "$CPATH/$CFILE"
		done
		cd -
	done
}

do_list()
{
	local CFILE subject

	if [ -e "$1" ]; then
		cd "$1"
		for CFILE in *.0; do
			if [ ! -s "$CFILE" ]; then
				echo "Unable to read $CFILE" >&2
				ERRORS=$(( $ERRORS + 1 ))
				continue
			fi
			subject=
			if [ $VERBOSE -eq 0 ]; then
				subject=$( openssl x509 -noout -subject -nameopt multiline -in "$CFILE" |
				    sed -n '/commonName/s/.*= //p' )
			fi
			[ "$subject" ] ||
			    subject=$( openssl x509 -noout -subject -in "$CFILE" )
			printf "%s\t%s\n" "$CFILE" "$subject"
		done
		cd -
	fi
}

cmd_rehash()
{

	[ $NOOP -eq 0 ] && rm -rf "$CERTDESTDIR"
	[ $NOOP -eq 0 ] && mkdir -p "$CERTDESTDIR"
	[ $NOOP -eq 0 ] && mkdir -p "$BLACKLISTDESTDIR"

	do_scan create_blacklisted "$BLACKLISTPATH"
	do_scan create_trusted_link "$TRUSTPATH"
}

cmd_list()
{
	echo "Listing Trusted Certificates:"
	do_list "$CERTDESTDIR"
}

cmd_blacklist()
{
	local BPATH

	shift # verb
	[ $NOOP -eq 0 ] && mkdir -p "$BLACKLISTDESTDIR"
	for BFILE in "$@"; do
		echo "Adding $BFILE to blacklist"
		create_blacklisted "$BFILE"
	done
}

cmd_unblacklist()
{
	local BFILE hash

	shift # verb
	for BFILE in "$@"; do
		if [ -s "$BFILE" ]; then
			hash=$( do_hash "$BFILE" )
			echo "Removing $hash.0 from blacklist"
			[ $NOOP -eq 0 ] && rm -f "$BLACKLISTDESTDIR/$hash.0"
		elif [ -e "$BLACKLISTDESTDIR/$BFILE" ]; then
			echo "Removing $BFILE from blacklist"
			[ $NOOP -eq 0 ] && rm -f "$BLACKLISTDESTDIR/$BFILE"
		else
			echo "Cannot find $BFILE" >&2
			ERRORS=$(( $ERRORS + 1 ))
		fi
	done
}

cmd_blacklisted()
{
	echo "Listing Blacklisted Certificates:"
	do_list "$BLACKLISTDESTDIR"
}

usage()
{
	exec >&2
	echo "Manage the TLS trusted certificates on the system"
	echo "	$SCRIPTNAME [-v] list"
	echo "		List trusted certificates"
	echo "	$SCRIPTNAME [-v] blacklisted"
	echo "		List blacklisted certificates"
	echo "	$SCRIPTNAME [-nv] rehash"
	echo "		Generate hash links for all certificates"
	echo "	$SCRIPTNAME [-nv] blacklist <file>"
	echo "		Add <file> to the list of blacklisted certificates"
	echo "	$SCRIPTNAME [-nv] unblacklist <file>"
	echo "		Remove <file> from the list of blacklisted certificates"
	exit 64
}

############################################################ MAIN

while getopts nv flag; do
	case "$flag" in
	n) NOOP=1 ;;
	v) VERBOSE=$(( $VERBOSE + 1 )) ;;
	esac
done
shift $(( $OPTIND - 1 ))

[ $# -gt 0 ] || usage
case "$1" in
list)		cmd_list ;;
rehash)		cmd_rehash ;;
blacklist)	cmd_blacklist "$@" ;;
unblacklist)	cmd_unblacklist "$@" ;;
blacklisted)	cmd_blacklisted ;;
*)		usage # NOTREACHED
esac

retval=$?
[ $ERRORS -gt 0 ] && echo "Encountered $ERRORS errors" >&2
exit $retval

################################################################################
# END
################################################################################
