#!/bin/sh
#-
# SPDX-License-Identifier: BSD-2-Clause
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

set -u

############################################################ CONFIGURATION

: ${DESTDIR:=}
: ${DISTBASE:=}

############################################################ GLOBALS

SCRIPTNAME="${0##*/}"
ERRORS=0
NOOP=false
UNPRIV=false
VERBOSE=false

############################################################ FUNCTIONS

info()
{
	echo "${0##*/}: $@" >&2
}

verbose()
{
	if "${VERBOSE}" ; then
		info "$@"
	fi
}

perform()
{
	if ! "${NOOP}" ; then
		"$@"
	fi
}

cert_files_in()
{
	find -L "$@" -type f \( \
	     -name '*.pem' -or \
	     -name '*.crt' -or \
	     -name '*.cer' -or \
	     -name '*.crl' \
	\) 2>/dev/null
}

do_hash()
{
	local hash

	if hash=$(openssl x509 -noout -subject_hash -in "$1") ; then
		echo "$hash"
		return 0
	else
		info "Error: $1"
		ERRORS=$((ERRORS + 1))
		return 1
	fi
}

get_decimal()
{
	local checkdir hash decimal

	checkdir=$1
	hash=$2
	decimal=0

	while [ -e "$checkdir/$hash.$decimal" ] ; do
		decimal=$((decimal + 1))
	done

	echo ${decimal}
	return 0
}

create_trusted()
{
	local hash certhash otherfile otherhash
	local suffix
	local link=${2:+-lm}

	hash=$(do_hash "$1") || return
	certhash=$(openssl x509 -sha1 -in "$1" -noout -fingerprint)
	for otherfile in $(find $UNTRUSTDESTDIR -name "$hash.*") ; do
		otherhash=$(openssl x509 -sha1 -in "$otherfile" -noout -fingerprint)
		if [ "$certhash" = "$otherhash" ] ; then
			info "Skipping untrusted certificate $hash ($otherfile)"
			return 1
		fi
	done
	for otherfile in $(find $CERTDESTDIR -name "$hash.*") ; do
		otherhash=$(openssl x509 -sha1 -in "$otherfile" -noout -fingerprint)
		if [ "$certhash" = "$otherhash" ] ; then
			verbose "Skipping duplicate entry for certificate $hash"
			return 0
		fi
	done
	suffix=$(get_decimal "$CERTDESTDIR" "$hash")
	verbose "Adding $hash.$suffix to trust store"
	perform install ${INSTALLFLAGS} -m 0444 ${link} \
		"$(realpath "$1")" "$CERTDESTDIR/$hash.$suffix"
}

# Accepts either dot-hash form from `certctl list` or a path to a valid cert.
resolve_certname()
{
	local hash srcfile filename
	local suffix

	# If it exists as a file, we'll try that; otherwise, we'll scan
	if [ -e "$1" ] ; then
		hash=$(do_hash "$1") || return
		srcfile=$(realpath "$1")
		suffix=$(get_decimal "$UNTRUSTDESTDIR" "$hash")
		filename="$hash.$suffix"
		echo "$srcfile" "$hash.$suffix"
	elif [ -e "${CERTDESTDIR}/$1" ] ;  then
		srcfile=$(realpath "${CERTDESTDIR}/$1")
		hash=$(echo "$1" | sed -Ee 's/\.([0-9])+$//')
		suffix=$(get_decimal "$UNTRUSTDESTDIR" "$hash")
		filename="$hash.$suffix"
		echo "$srcfile" "$hash.$suffix"
	fi
}

create_untrusted()
{
	local srcfile filename
	local link=${2:+-lm}

	set -- $(resolve_certname "$1")
	srcfile=$1
	filename=$2

	if [ -z "$srcfile" -o -z "$filename" ] ; then
		return
	fi

	verbose "Adding $filename to untrusted list"
	perform install ${INSTALLFLAGS} -m 0444 ${link} \
		"$srcfile" "$UNTRUSTDESTDIR/$filename"
}

do_scan()
{
	local CFUNC CSEARCH CPATH CFILE CERT SPLITDIR
	local oldIFS="$IFS"
	CFUNC="$1"
	CSEARCH="$2"

	IFS=:
	set -- $CSEARCH
	IFS="$oldIFS"
	for CFILE in $(cert_files_in "$@") ; do
		verbose "Reading $CFILE"
		case $(grep -c '^Certificate:$' "$CFILE") in
		0)
			;;
		1)
			"$CFUNC" "$CFILE" link
			;;
		*)
			verbose "Multiple certificates found, splitting..."
			SPLITDIR=$(mktemp -d)
			egrep '^[^#]' "$CFILE" | \
				split -p '^Certificate:$' - "$SPLITDIR/x"
			for CERT in $(find "$SPLITDIR" -type f) ; do
				"$CFUNC" "$CERT"
			done
			rm -rf "$SPLITDIR"
			;;
		esac
	done
}

do_list()
{
	local CFILE subject

	for CFILE in $(find "$@" \( -type f -or -type l \) -name '*.[0-9]') ; do
		if [ ! -s "$CFILE" ] ; then
			info "Unable to read $CFILE"
			ERRORS=$((ERRORS + 1))
			continue
		fi
		subject=
		if ! "$VERBOSE" ; then
			subject=$(openssl x509 -noout -subject -nameopt multiline -in "$CFILE" | sed -n '/commonName/s/.*= //p')
		fi
		if [ -z "$subject" ] ; then
			subject=$(openssl x509 -noout -subject -in "$CFILE")
		fi
		printf "%s\t%s\n" "${CFILE##*/}" "$subject"
	done
}

cmd_rehash()
{

	if [ -e "$CERTDESTDIR" ] ; then
		perform find "$CERTDESTDIR" \( -type f -or -type l \) -delete
	else
		perform install -d -m 0755 "$CERTDESTDIR"
	fi
	if [ -e "$UNTRUSTDESTDIR" ] ; then
		perform find "$UNTRUSTDESTDIR" \( -type f -or -type l \) -delete
	else
		perform install -d -m 0755 "$UNTRUSTDESTDIR"
	fi

	do_scan create_untrusted "$UNTRUSTPATH"
	do_scan create_trusted "$TRUSTPATH"
}

cmd_list()
{
	info "Listing Trusted Certificates:"
	do_list "$CERTDESTDIR"
}

cmd_untrust()
{
	local UTFILE

	shift # verb
	perform install -d -m 0755 "$UNTRUSTDESTDIR"
	for UTFILE in "$@"; do
		info "Adding $UTFILE to untrusted list"
		create_untrusted "$UTFILE"
	done
}

cmd_trust()
{
	local UTFILE untrustedhash certhash hash

	shift # verb
	for UTFILE in "$@"; do
		if [ -s "$UTFILE" ] ; then
			hash=$(do_hash "$UTFILE")
			certhash=$(openssl x509 -sha1 -in "$UTFILE" -noout -fingerprint)
			for UNTRUSTEDFILE in $(find $UNTRUSTDESTDIR -name "$hash.*") ; do
				untrustedhash=$(openssl x509 -sha1 -in "$UNTRUSTEDFILE" -noout -fingerprint)
				if [ "$certhash" = "$untrustedhash" ] ; then
					info "Removing $(basename "$UNTRUSTEDFILE") from untrusted list"
					perform rm -f $UNTRUSTEDFILE
				fi
			done
		elif [ -e "$UNTRUSTDESTDIR/$UTFILE" ] ; then
			info "Removing $UTFILE from untrusted list"
			perform rm -f "$UNTRUSTDESTDIR/$UTFILE"
		else
			info "Cannot find $UTFILE"
			ERRORS=$((ERRORS + 1))
		fi
	done
}

cmd_untrusted()
{
	info "Listing Untrusted Certificates:"
	do_list "$UNTRUSTDESTDIR"
}

usage()
{
	exec >&2
	echo "Manage the TLS trusted certificates on the system"
	echo "	$SCRIPTNAME [-v] list"
	echo "		List trusted certificates"
	echo "	$SCRIPTNAME [-v] untrusted"
	echo "		List untrusted certificates"
	echo "	$SCRIPTNAME [-nUv] [-D <destdir>] [-d <distbase>] [-M <metalog>] rehash"
	echo "		Generate hash links for all certificates"
	echo "	$SCRIPTNAME [-nv] untrust <file>"
	echo "		Add <file> to the list of untrusted certificates"
	echo "	$SCRIPTNAME [-nv] trust <file>"
	echo "		Remove <file> from the list of untrusted certificates"
	exit 64
}

############################################################ MAIN

while getopts D:d:M:nUv flag; do
	case "$flag" in
	D) DESTDIR=${OPTARG} ;;
	d) DISTBASE=${OPTARG} ;;
	M) METALOG=${OPTARG} ;;
	n) NOOP=true ;;
	U) UNPRIV=true ;;
	v) VERBOSE=true ;;
	esac
done
shift $((OPTIND - 1))

DESTDIR=${DESTDIR%/}

if ! [ -z "${CERTCTL_VERBOSE:-}" ] ; then
	VERBOSE=true
fi
: ${METALOG:=${DESTDIR}/METALOG}
INSTALLFLAGS=
if "$UNPRIV" ; then
	INSTALLFLAGS="-U -M ${METALOG} -D ${DESTDIR}"
fi
: ${LOCALBASE:=$(sysctl -n user.localbase)}
: ${TRUSTPATH:=${DESTDIR}${DISTBASE}/usr/share/certs/trusted:${DESTDIR}${LOCALBASE}/share/certs:${DESTDIR}${LOCALBASE}/etc/ssl/certs}
: ${UNTRUSTPATH:=${DESTDIR}${DISTBASE}/usr/share/certs/untrusted:${DESTDIR}${LOCALBASE}/etc/ssl/untrusted:${DESTDIR}${LOCALBASE}/etc/ssl/blacklisted}
: ${CERTDESTDIR:=${DESTDIR}${DISTBASE}/etc/ssl/certs}
: ${UNTRUSTDESTDIR:=${DESTDIR}${DISTBASE}/etc/ssl/untrusted}

[ $# -gt 0 ] || usage
case "$1" in
list)		cmd_list ;;
rehash)		cmd_rehash ;;
blacklist)	cmd_untrust "$@" ;;
untrust)	cmd_untrust "$@" ;;
trust)		cmd_trust "$@" ;;
unblacklist)	cmd_trust "$@" ;;
untrusted)	cmd_untrusted ;;
blacklisted)	cmd_untrusted ;;
*)		usage # NOTREACHED
esac

retval=$?
if [ $ERRORS -gt 0 ] ; then
	info "Encountered $ERRORS errors"
fi
exit $retval

################################################################################
# END
################################################################################
