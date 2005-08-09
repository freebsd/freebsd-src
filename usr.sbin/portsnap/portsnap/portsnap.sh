#!/bin/sh

#-
# Copyright 2004-2005 Colin Percival
# All rights reserved
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

# $FreeBSD$

#### Usage function -- called from command-line handling code.

# Usage instructions.  Options not listed:
# --debug	-- don't filter output from utilities
# --no-stats	-- don't show progress statistics while fetching files
usage() {
	cat <<EOF
usage: `basename $0` [options] command [path]

Options:
  -d workdir   -- Store working files in workdir
                  (default: /var/db/portsnap/)
  -f conffile  -- Read configuration options from conffile
                  (default: /etc/portsnap.conf)
  -I           -- Update INDEX only. (update command only)
  -k KEY       -- Trust an RSA key with SHA256 hash of KEY
  -p portsdir  -- Location of uncompressed ports tree
                  (default: /usr/ports/)
  -s server    -- Server from which to fetch updates.
                  (default: portsnap.FreeBSD.org)
  path         -- Extract only parts of the tree starting with the given
                  string.  (extract command only)
Commands:
  fetch        -- Fetch a compressed snapshot of the ports tree,
                  or update an existing snapshot.
  cron         -- Sleep rand(3600) seconds, and then fetch updates.
  extract      -- Extract snapshot of ports tree, replacing existing
                  files and directories.
  update       -- Update ports tree to match current snapshot, replacing
                  files and directories which have changed.
EOF
	exit 0
}

#### Parameter handling functions.

# Initialize parameters to null, just in case they're
# set in the environment.
init_params() {
	KEYPRINT=""
	EXTRACTPATH=""
	WORKDIR=""
	PORTSDIR=""
	CONFFILE=""
	COMMAND=""
	QUIETREDIR=""
	QUIETFLAG=""
	STATSREDIR=""
	XARGST=""
	NDEBUG=""
	DDSTATS=""
	INDEXONLY=""
	SERVERNAME=""
}

# Parse the command line
parse_cmdline() {
	while [ $# -gt 0 ]; do
		case "$1" in
		-d)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${WORKDIR}" ]; then usage; fi
			shift; WORKDIR="$1"
			;;
		--debug)
			QUIETREDIR="/dev/stderr"
			STATSREDIR="/dev/stderr"
			QUIETFLAG=" "
			NDEBUG=" "
			XARGST="-t"
			DDSTATS=".."
			;;
		-f)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${CONFFILE}" ]; then usage; fi
			shift; CONFFILE="$1"
			;;
		-h | --help | help)
			usage
			;;
		-I)
			INDEXONLY="YES"
			;;
		-k)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${KEYPRINT}" ]; then usage; fi
			shift; KEYPRINT="$1"
			;;
		--no-stats)
			if [ -z "${STATSREDIR}" ]; then
				STATSREDIR="/dev/null"
				DDSTATS=".. "
			fi
			;;
		-p)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${PORTSDIR}" ]; then usage; fi
			shift; PORTSDIR="$1"
			;;
		-s)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${SERVERNAME}" ]; then usage; fi
			shift; SERVERNAME="$1"
			;;
		cron | extract | fetch | update)
			if [ ! -z "${COMMAND}" ]; then usage; fi
			COMMAND="$1"
			;;
		*)
			if [ $# -gt 1 ]; then usage; fi
			if [ "${COMMAND}" = "extract" ]; then usage; fi
			EXTRACTPATH="$1"
			;;
		esac
		shift
	done

	if [ -z "${COMMAND}" ]; then
		usage
	fi
}

# If CONFFILE was specified at the command-line, make
# sure that it exists and is readable.
sanity_conffile() {
	if [ ! -z "${CONFFILE}" ] && [ ! -r "${CONFFILE}" ]; then
		echo -n "File does not exist "
		echo -n "or is not readable: "
		echo ${CONFFILE}
		exit 1
	fi
}

# If a configuration file hasn't been specified, use
# the default value (/etc/portsnap.conf)
default_conffile() {
	if [ -z "${CONFFILE}" ]; then
		CONFFILE="/etc/portsnap.conf"
	fi
}

# Read {KEYPRINT, SERVERNAME, WORKDIR, PORTSDIR} from the configuration
# file if they haven't already been set.  If the configuration
# file doesn't exist, do nothing.
parse_conffile() {
	if [ -r "${CONFFILE}" ]; then
		for X in KEYPRINT WORKDIR PORTSDIR SERVERNAME; do
			eval _=\$${X}
			if [ -z "${_}" ]; then
				eval ${X}=`grep "^${X}=" "${CONFFILE}" |
				    cut -f 2- -d '=' | tail -1`
			fi
		done
	fi
}

# If parameters have not been set, use default values
default_params() {
	_QUIETREDIR="/dev/null"
	_QUIETFLAG="-q"
	_STATSREDIR="/dev/stdout"
	_WORKDIR="/var/db/portsnap"
	_PORTSDIR="/usr/ports"
	_NDEBUG="-n"
	for X in QUIETREDIR QUIETFLAG STATSREDIR WORKDIR PORTSDIR NDEBUG; do
		eval _=\$${X}
		eval __=\$_${X}
		if [ -z "${_}" ]; then
			eval ${X}=${__}
		fi
	done
}

# Perform sanity checks and set some final parameters
# in preparation for fetching files.  Also chdir into
# the working directory.
fetch_check_params() {
	export HTTP_USER_AGENT="portsnap (${COMMAND}, `uname -r`)"

	_SERVERNAME_z=\
"SERVERNAME must be given via command line or configuration file."
	_KEYPRINT_z="Key must be given via -k option or configuration file."
	_KEYPRINT_bad="Invalid key fingerprint: "
	_WORKDIR_bad="Directory does not exist or is not writable: "

	if [ -z "${SERVERNAME}" ]; then
		echo -n "`basename $0`: "
		echo "${_SERVERNAME_z}"
		exit 1
	fi
	if [ -z "${KEYPRINT}" ]; then
		echo -n "`basename $0`: "
		echo "${_KEYPRINT_z}"
		exit 1
	fi
	if ! echo "${KEYPRINT}" | grep -qE "^[0-9a-f]{64}$"; then
		echo -n "`basename $0`: "
		echo -n "${_KEYPRINT_bad}"
		echo ${KEYPRINT}
		exit 1
	fi
	if ! [ -d "${WORKDIR}" -a -w "${WORKDIR}" ]; then
		echo -n "`basename $0`: "
		echo -n "${_WORKDIR_bad}"
		echo ${WORKDIR}
		exit 1
	fi
	cd ${WORKDIR} || exit 1

	BSPATCH=/usr/bin/bspatch
	SHA256=/sbin/sha256
	PHTTPGET=/usr/libexec/phttpget
}

# Perform sanity checks and set some final parameters
# in preparation for extracting or updating ${PORTSDIR}
extract_check_params() {
	_WORKDIR_bad="Directory does not exist: "
	_PORTSDIR_bad="Directory does not exist or is not writable: "

	if ! [ -d "${WORKDIR}" ]; then
		echo -n "`basename $0`: "
		echo -n "${_WORKDIR_bad}"
		echo ${WORKDIR}
		exit 1
	fi
	if ! [ -d "${PORTSDIR}" -a -w "${PORTSDIR}" ]; then
		echo -n "`basename $0`: "
		echo -n "${_PORTSDIR_bad}"
		echo ${PORTSDIR}
		exit 1
	fi

	if ! [ -d "${WORKDIR}/files" -a -r "${WORKDIR}/tag"	\
	    -a -r "${WORKDIR}/INDEX" -a -r "${WORKDIR}/tINDEX" ]; then
		echo "No snapshot available.  Try running"
		echo "# `basename $0` fetch"
		exit 1
	fi

	MKINDEX=/usr/libexec/make_index
}

# Perform sanity checks and set some final parameters
# in preparation for updating ${PORTSDIR}
update_check_params() {
	extract_check_params

	if ! [ -r ${PORTSDIR}/.portsnap.INDEX ]; then
		echo "${PORTSDIR} was not created by portsnap."
		echo -n "You must run '`basename $0` extract' before "
		echo "running '`basename $0` update'."
		exit 1
	fi

}

#### Core functionality -- the actual work gets done here

# Use an SRV query to pick a server.  If the SRV query doesn't provide
# a useful answer, use the server name specified by the user.
# Put another way... look up _http._tcp.${SERVERNAME} and pick a server
# from that; or if no servers are returned, use ${SERVERNAME}.
# This allows a user to specify "portsnap.freebsd.org" (in which case
# portsnap will select one of the mirrors) or "portsnap5.tld.freebsd.org"
# (in which case portsnap will use that particular server, since there
# won't be an SRV entry for that name).
#
# We don't implement the recommendations from RFC 2782 completely, since
# we are only looking to pick a single server -- the recommendations are
# targetted at applications which obtain a list of servers and then try
# each in turn, but we are instead just going to pick one server and let
# the user re-run portsnap if a broken server was selected.
#
# We also ignore the Port field, since we are always going to use port 80.
fetch_pick_server() {
	echo -n "Looking up ${SERVERNAME} mirrors..."

# Issue the SRV query and pull out the Priority, Weight, and Target fields.
	host -t srv "_http._tcp.${SERVERNAME}" |
	    grep -E "^_http._tcp.${SERVERNAME} has SRV record" |
	    cut -f 5,6,8 -d ' ' > serverlist

# If no records, give up -- we'll just use the server name we were given.
	if [ `wc -l < serverlist` -eq 0 ]; then
		echo " none found."
		return
	fi

# Find the highest priority level (lowest numeric value).
	SRV_PRIORITY=`cut -f 1 -d ' ' serverlist | sort -n | head -1`

# Add up the weights of the response lines at that priority level.
	SRV_WSUM=0;
	while read X; do
		case "$X" in
		${SRV_PRIORITY}\ *)
			SRV_W=`echo $X | cut -f 2 -d ' '`
			SRV_WSUM=$(($SRV_WSUM + $SRV_W))
			;;
		esac
	done < serverlist

# If all the weights are 0, pretend that they are all 1 instead.
	if [ ${SRV_WSUM} -eq 0 ]; then
		SRV_WSUM=`grep -E "^${SRV_PRIORITY} " serverlist | wc -l`
		SRV_W_ADD=1
	else
		SRV_W_ADD=0
	fi

# Pick a random value between 1 and the sum of the weights
	SRV_RND=`jot -r 1 1 ${SRV_WSUM}`

# Read through the list of mirrors and set SERVERNAME
	while read X; do
		case "$X" in
		${SRV_PRIORITY}\ *)
			SRV_W=`echo $X | cut -f 2 -d ' '`
			SRV_W=$(($SRV_W + $SRV_W_ADD))
			if [ $SRV_RND -le $SRV_W ]; then
				SERVERNAME=`echo $X | cut -f 3 -d ' '`
				break
			else
				SRV_RND=$(($SRV_RND - $SRV_W))
			fi
			;;
		esac
	done < serverlist

	echo " using ${SERVERNAME}"
}

# Check that we have a public key with an appropriate hash, or
# fetch the key if it doesn't exist.
fetch_key() {
	if [ -r pub.ssl ] && [ `${SHA256} -q pub.ssl` = ${KEYPRINT} ]; then
		return
	fi

	echo -n "Fetching public key... "
	rm -f pub.ssl
	fetch ${QUIETFLAG} http://${SERVERNAME}/pub.ssl \
	    2>${QUIETREDIR} || true
	if ! [ -r pub.ssl ]; then
		echo "failed."
		return 1
	fi
	if ! [ `${SHA256} -q pub.ssl` = ${KEYPRINT} ]; then
		echo "key has incorrect hash."
		rm -f pub.ssl
		return 1
	fi
	echo "done."
}

# Fetch a snapshot tag
fetch_tag() {
	rm -f snapshot.ssl tag.new

	echo ${NDEBUG} "Fetching snapshot tag... "
	fetch ${QUIETFLAG} http://${SERVERNAME}/$1.ssl
	    2>${QUIETREDIR} || true
	if ! [ -r $1.ssl ]; then
		echo "failed."
		return 1
	fi

	openssl rsautl -pubin -inkey pub.ssl -verify		\
	    < $1.ssl > tag.new 2>${QUIETREDIR} || true
	rm $1.ssl

	if ! [ `wc -l < tag.new` = 1 ] ||
	    ! grep -qE "^portsnap\|[0-9]{10}\|[0-9a-f]{64}" tag.new; then
		echo "invalid snapshot tag."
		return 1
	fi

	echo "done."

	SNAPSHOTDATE=`cut -f 2 -d '|' < tag.new`
	SNAPSHOTHASH=`cut -f 3 -d '|' < tag.new`
}

# Sanity-check the date on a snapshot tag
fetch_snapshot_tagsanity() {
	if [ `date "+%s"` -gt `expr ${SNAPSHOTDATE} + 31536000` ]; then
		echo "Snapshot appears to be more than a year old!"
		echo "(Is the system clock correct?)"
		echo "Cowarly refusing to proceed any further."
		return 1
	fi
	if [ `date "+%s"` -lt `expr ${SNAPSHOTDATE} - 86400` ]; then
		echo -n "Snapshot appears to have been created more than "
		echo "one day into the future!"
		echo "(Is the system clock correct?)"
		echo "Cowardly refusing to proceed any further."
		return 1
	fi
}

# Sanity-check the date on a snapshot update tag
fetch_update_tagsanity() {
	fetch_snapshot_tagsanity || return 1

	if [ ${OLDSNAPSHOTDATE} -gt ${SNAPSHOTDATE} ]; then
		echo -n "Latest snapshot on server is "
		echo "older than what we already have!"
		echo -n "Cowardly refusing to downgrade from "
		date -r ${OLDSNAPSHOTDATE}
		echo "to `date -r ${SNAPSHOTDATE}`."
		return 1
	fi
}

# Compare old and new tags; return 1 if update is unnecessary
fetch_update_neededp() {
	if [ ${OLDSNAPSHOTDATE} -eq ${SNAPSHOTDATE} ]; then
		echo -n "Latest snapshot on server matches "
		echo "what we already have."
		echo "No updates needed."
		rm tag.new
		return 1
	fi
	if [ ${OLDSNAPSHOTHASH} = ${SNAPSHOTHASH} ]; then
		echo -n "Ports tree hasn't changed since "
		echo "last snapshot."
		echo "No updates needed."
		rm tag.new
		return 1
	fi

	return 0
}

# Fetch snapshot metadata file
fetch_metadata() {
	rm -f ${SNAPSHOTHASH} tINDEX.new

	echo ${NDEBUG} "Fetching snapshot metadata... "
	fetch ${QUIETFLAG} http://${SERVERNAME}/t/${SNAPSHOTHASH}
	    2>${QUIETREDIR} || return
	if [ `${SHA256} -q ${SNAPSHOTHASH}` != ${SNAPSHOTHASH} ]; then
		echo "snapshot metadata corrupt."
		return 1
	fi
	mv ${SNAPSHOTHASH} tINDEX.new
	echo "done."
}

# Warn user about bogus metadata
fetch_metadata_freakout() {
	echo
	echo "Portsnap metadata is correctly signed, but contains"
	echo "at least one line which appears bogus."
	echo "Cowardly refusing to proceed any further."
}

# Sanity-check a snapshot metadata file
fetch_metadata_sanity() {
	if grep -qvE "^[0-9A-Z.]+\|[0-9a-f]{64}$" tINDEX.new; then
		fetch_metadata_freakout
		return 1
	fi
	if [ `look INDEX tINDEX.new | wc -l` != 1 ]; then
		echo
		echo "Portsnap metadata appears bogus."
		echo "Cowardly refusing to proceed any further."
		return 1
	fi
}

# Take a list of ${oldhash}|${newhash} and output a list of needed patches
fetch_make_patchlist() {
	grep -vE "^([0-9a-f]{64})\|\1$" | 
		while read LINE; do
			X=`echo ${LINE} | cut -f 1 -d '|'`
			Y=`echo ${LINE} | cut -f 2 -d '|'`
			if [ -f "files/${Y}.gz" ]; then continue; fi
			if [ ! -f "files/${X}.gz" ]; then continue; fi
			echo "${LINE}"
		done
}

# Print user-friendly progress statistics
fetch_progress() {
	LNC=0
	while read x; do
		LNC=$(($LNC + 1))
		if [ $(($LNC % 10)) = 0 ]; then
			echo -n $FN2
		elif [ $(($LNC % 2)) = 0 ]; then
			echo -n .
		fi
	done
	echo -n " "
}

# Sanity-check an index file
fetch_index_sanity() {
	if grep -qvE "^[-_+./@0-9A-Za-z]+\|[0-9a-f]{64}$" INDEX.new ||
	    fgrep -q "./" INDEX.new; then
		fetch_metadata_freakout
		return 1
	fi
}

# Verify a list of files
fetch_snapshot_verify() {
	while read F; do
		if [ `gunzip -c snap/${F} | ${SHA256} -q` != ${F} ]; then
			echo "snapshot corrupt."
			return 1
		fi
	done
	return 0
}

# Fetch a snapshot tarball, extract, and verify.
fetch_snapshot() {
	fetch_tag snapshot || return 1
	fetch_snapshot_tagsanity || return 1
	fetch_metadata || return 1
	fetch_metadata_sanity || return 1

	rm -f ${SNAPSHOTHASH}.tgz
	rm -rf snap/

# Don't ask fetch(1) to be quiet -- downloading a snapshot of ~ 35MB will
# probably take a while, so the progrees reports that fetch(1) generates
# will be useful for keeping the users' attention from drifting.
	echo "Fetching snapshot generated at `date -r ${SNAPSHOTDATE}`:"
	fetch http://${SERVERNAME}/s/${SNAPSHOTHASH}.tgz || return 1

	echo -n "Extracting snapshot... "
	tar -xzf ${SNAPSHOTHASH}.tgz snap/ || return 1
	rm ${SNAPSHOTHASH}.tgz
	echo "done."

	echo -n "Verifying snapshot integrity... "
# Verify the metadata files
	cut -f 2 -d '|' tINDEX.new | fetch_snapshot_verify || return 1
# Extract the index
	rm -f INDEX.new
	gunzip -c snap/`look INDEX tINDEX.new |
	    cut -f 2 -d '|'`.gz > INDEX.new
	fetch_index_sanity || return 1
# Verify the snapshot contents
	cut -f 2 -d '|' INDEX.new | fetch_snapshot_verify || return 1
	echo "done."

# Move files into their proper locations
	rm -f tag INDEX tINDEX
	rm -rf files
	mv tag.new tag
	mv tINDEX.new tINDEX
	mv INDEX.new INDEX
	mv snap/ files/

	return 0
}

# Update a compressed snapshot
fetch_update() {
	rm -f patchlist diff OLD NEW filelist INDEX.new

	OLDSNAPSHOTDATE=`cut -f 2 -d '|' < tag`
	OLDSNAPSHOTHASH=`cut -f 3 -d '|' < tag`

	fetch_tag latest || return 1
	fetch_update_tagsanity || return 1
	fetch_update_neededp || return 0
	fetch_metadata || return 1
	fetch_metadata_sanity || return 1

	echo -n "Updating from `date -r ${OLDSNAPSHOTDATE}` "
	echo "to `date -r ${SNAPSHOTDATE}`."

# Generate a list of wanted metadata patches
	join -t '|' -o 1.2,2.2 tINDEX tINDEX.new |
	    fetch_make_patchlist > patchlist

# Attempt to fetch metadata patches
	echo -n "Fetching `wc -l < patchlist | tr -d ' '` "
	echo ${NDEBUG} "metadata patches.${DDSTATS}"
	tr '|' '-' < patchlist |
	    lam -s "tp/" - -s ".gz" |
	    xargs ${XARGST} ${PHTTPGET} ${SERVERNAME}	\
	    2>${STATSREDIR} | fetch_progress
	echo "done."

# Attempt to apply metadata patches
	echo -n "Applying metadata patches... "
	while read LINE; do
		X=`echo ${LINE} | cut -f 1 -d '|'`
		Y=`echo ${LINE} | cut -f 2 -d '|'`
		if [ ! -f "${X}-${Y}.gz" ]; then continue; fi
		gunzip -c < ${X}-${Y}.gz > diff
		gunzip -c < files/${X}.gz > OLD
		cut -c 2- diff | join -t '|' -v 2 - OLD > ptmp
		grep '^\+' diff | cut -c 2- |
		    sort -k 1,1 -t '|' -m - ptmp > NEW
		if [ `${SHA256} -q NEW` = ${Y} ]; then
			mv NEW files/${Y}
			gzip -n files/${Y}
		fi
		rm -f diff OLD NEW ${X}-${Y}.gz ptmp
	done < patchlist 2>${QUIETREDIR}
	echo "done."

# Update metadata without patches
	join -t '|' -v 2 tINDEX tINDEX.new |
	    cut -f 2 -d '|' /dev/stdin patchlist |
		while read Y; do
			if [ ! -f "files/${Y}.gz" ]; then
				echo ${Y};
			fi
		done > filelist
	echo -n "Fetching `wc -l < filelist | tr -d ' '` "
	echo ${NDEBUG} "metadata files... "
	lam -s "f/" - -s ".gz" < filelist |
	    xargs ${XARGST} ${PHTTPGET} ${SERVERNAME}	\
	    2>${QUIETREDIR}

	while read Y; do
		if [ `gunzip -c < ${Y}.gz | ${SHA256} -q` = ${Y} ]; then
			mv ${Y}.gz files/${Y}.gz
		else
			echo "metadata is corrupt."
			return 1
		fi
	done < filelist
	echo "done."

# Extract the index
	gunzip -c files/`look INDEX tINDEX.new |
	    cut -f 2 -d '|'`.gz > INDEX.new
	fetch_index_sanity || return 1

# Generate a list of wanted ports patches
	join -t '|' -o 1.2,2.2 INDEX INDEX.new |
	    fetch_make_patchlist > patchlist

# Attempt to fetch ports patches
	echo -n "Fetching `wc -l < patchlist | tr -d ' '` "
	echo ${NDEBUG} "patches.${DDSTATS}"
	tr '|' '-' < patchlist | lam -s "bp/" - |
	    xargs ${XARGST} ${PHTTPGET} ${SERVERNAME}	\
	    2>${STATSREDIR} | fetch_progress
	echo "done."

# Attempt to apply ports patches
	echo -n "Applying patches... "
	while read LINE; do
		X=`echo ${LINE} | cut -f 1 -d '|'`
		Y=`echo ${LINE} | cut -f 2 -d '|'`
		if [ ! -f "${X}-${Y}" ]; then continue; fi
		gunzip -c < files/${X}.gz > OLD
		${BSPATCH} OLD NEW ${X}-${Y}
		if [ `${SHA256} -q NEW` = ${Y} ]; then
			mv NEW files/${Y}
			gzip -n files/${Y}
		fi
		rm -f diff OLD NEW ${X}-${Y}
	done < patchlist 2>${QUIETREDIR}
	echo "done."

# Update ports without patches
	join -t '|' -v 2 INDEX INDEX.new |
	    cut -f 2 -d '|' /dev/stdin patchlist |
		while read Y; do
			if [ ! -f "files/${Y}.gz" ]; then
				echo ${Y};
			fi
		done > filelist
	echo -n "Fetching `wc -l < filelist | tr -d ' '` "
	echo ${NDEBUG} "new ports or files... "
	lam -s "f/" - -s ".gz" < filelist |
	    xargs ${XARGST} ${PHTTPGET} ${SERVERNAME}	\
	    2>${QUIETREDIR}

	while read Y; do
		if [ `gunzip -c < ${Y}.gz | ${SHA256} -q` = ${Y} ]; then
			mv ${Y}.gz files/${Y}.gz
		else
			echo "snapshot is corrupt."
			return 1
		fi
	done < filelist
	echo "done."

# Remove files which are no longer needed
	cut -f 2 -d '|' tINDEX INDEX | sort > oldfiles
	cut -f 2 -d '|' tINDEX.new INDEX.new | sort | comm -13 - oldfiles |
	    lam -s "files/" - -s ".gz" | xargs rm -f
	rm patchlist filelist oldfiles

# We're done!
	mv INDEX.new INDEX
	mv tINDEX.new tINDEX
	mv tag.new tag

	return 0
}

# Do the actual work involved in "fetch" / "cron".
fetch_run() {
	fetch_pick_server

	fetch_key || return 1

	if ! [ -d files -a -r tag -a -r INDEX -a -r tINDEX ]; then
		fetch_snapshot || return 1
	fi
	fetch_update || return 1
}

# Build a ports INDEX file
extract_make_index() {
	gunzip -c "${WORKDIR}/files/`look $1 ${WORKDIR}/tINDEX |
	    cut -f 2 -d '|'`.gz" | ${MKINDEX} /dev/stdin > ${PORTSDIR}/$2
}

# Create INDEX, INDEX-5, INDEX-6
extract_indices() {
	echo -n "Building new INDEX files... "
	extract_make_index DESCRIBE.4 INDEX || return 1
	extract_make_index DESCRIBE.5 INDEX-5 || return 1
	extract_make_index DESCRIBE.6 INDEX-6 || return 1
	echo "done."
}

# Create .portsnap.INDEX
extract_metadata() {
	sort ${WORKDIR}/INDEX > ${PORTSDIR}/.portsnap.INDEX
}

# Do the actual work involved in "extract"
extract_run() {
	grep "^${EXTRACTPATH}" ${WORKDIR}/INDEX | while read LINE; do
		FILE=`echo ${LINE} | cut -f 1 -d '|'`
		HASH=`echo ${LINE} | cut -f 2 -d '|'`
		echo ${PORTSDIR}/${FILE}
		if ! [ -r "${WORKDIR}/files/${HASH}.gz" ]; then
			echo "files/${HASH}.gz not found -- snapshot corrupt."
			return 1
		fi
		case ${FILE} in
		*/)
			rm -rf ${PORTSDIR}/${FILE}
			mkdir -p ${PORTSDIR}/${FILE}
			tar -xzf ${WORKDIR}/files/${HASH}.gz	\
			    -C ${PORTSDIR}/${FILE}
			;;
		*)
			rm -f ${PORTSDIR}/${FILE}
			tar -xzf ${WORKDIR}/files/${HASH}.gz	\
			    -C ${PORTSDIR} ${FILE}
			;;
		esac
	done
	if [ ! -z "${EXTRACTPATH}" ]; then
		return 0;
	fi

	extract_metadata
	extract_indices
}

# Do the actual work involved in "update"
update_run() {
	if ! [ -z "${INDEXONLY}" ]; then
		extract_indices >/dev/null || return 1
		return 0
	fi

	echo -n "Removing old files and directories... "
	sort ${WORKDIR}/INDEX | comm -23 ${PORTSDIR}/.portsnap.INDEX - |
	    cut -f 1 -d '|' | lam -s "${PORTSDIR}/" - | xargs rm -rf
	echo "done."

# Install new files
	echo "Extracting new files:"
	sort ${WORKDIR}/INDEX | comm -13 ${PORTSDIR}/.portsnap.INDEX - |
	    while read LINE; do
		FILE=`echo ${LINE} | cut -f 1 -d '|'`
		HASH=`echo ${LINE} | cut -f 2 -d '|'`
		echo ${PORTSDIR}/${FILE}
		if ! [ -r "${WORKDIR}/files/${HASH}.gz" ]; then
			echo "files/${HASH}.gz not found -- snapshot corrupt."
			return 1
		fi
		case ${FILE} in
		*/)
			mkdir -p ${PORTSDIR}/${FILE}
			tar -xzf ${WORKDIR}/files/${HASH}.gz	\
			    -C ${PORTSDIR}/${FILE}
			;;
		*)
			tar -xzf ${WORKDIR}/files/${HASH}.gz	\
			    -C ${PORTSDIR} ${FILE}
			;;
		esac
	done

	extract_metadata
	extract_indices
}

#### Main functions -- call parameter-handling and core functions

# Using the command line, configuration file, and defaults,
# set all the parameters which are needed later.
get_params() {
	init_params
	parse_cmdline $@
	sanity_conffile
	default_conffile
	parse_conffile
	default_params
}

# Fetch command.  Make sure that we're being called
# interactively, then run fetch_check_params and fetch_run
cmd_fetch() {
	if [ ! -t 0 ]; then
		echo -n "`basename $0` fetch should not "
		echo "be run non-interactively."
		echo "Run `basename $0` cron instead."
		exit 1
	fi
	fetch_check_params
	fetch_run || exit 1
}

# Cron command.  Make sure the parameters are sensible; wait
# rand(3600) seconds; then fetch updates.  While fetching updates,
# send output to a temporary file; only print that file if the
# fetching failed.
cmd_cron() {
	fetch_check_params
	sleep `jot -r 1 0 3600`

	TMPFILE=`mktemp /tmp/portsnap.XXXXXX` || exit 1
	if ! fetch_run >> ${TMPFILE}; then
		cat ${TMPFILE}
		rm ${TMPFILE}
		exit 1
	fi

	rm ${TMPFILE}
}

# Extract command.  Make sure the parameters are sensible,
# then extract the ports tree (or part thereof).
cmd_extract() {
	extract_check_params
	extract_run || exit 1
}

# Update command.  Make sure the parameters are sensible,
# then update the ports tree.
cmd_update() {
	update_check_params
	update_run || exit 1
}

#### Entry point

# Make sure we find utilities from the base system
export PATH=/sbin:/bin:/usr/sbin:/usr/bin:${PATH}

get_params $@
cmd_${COMMAND}
