#!/bin/sh

#-
# Copyright 2004-2006 Colin Percival
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
usage () {
	cat <<EOF
usage: `basename $0` [options] command ... [path]

Options:
  -b basedir   -- Operate on a system mounted at basedir
                  (default: /)
  -d workdir   -- Store working files in workdir
                  (default: /var/db/freebsd-update/)
  -f conffile  -- Read configuration options from conffile
                  (default: /etc/freebsd-update.conf)
  -k KEY       -- Trust an RSA key with SHA256 hash of KEY
  -s server    -- Server from which to fetch updates
                  (default: update.FreeBSD.org)
  -t address   -- Mail output of cron command, if any, to address
                  (default: root)
Commands:
  fetch        -- Fetch updates from server
  cron         -- Sleep rand(3600) seconds, fetch updates, and send an
                  email if updates were found
  install      -- Install downloaded updates
  rollback     -- Uninstall most recently installed updates
EOF
	exit 0
}

#### Configuration processing functions

#-
# Configuration options are set in the following order of priority:
# 1. Command line options
# 2. Configuration file options
# 3. Default options
# In addition, certain options (e.g., IgnorePaths) can be specified multiple
# times and (as long as these are all in the same place, e.g., inside the
# configuration file) they will accumulate.  Finally, because the path to the
# configuration file can be specified at the command line, the entire command
# line must be processed before we start reading the configuration file.
#
# Sound like a mess?  It is.  Here's how we handle this:
# 1. Initialize CONFFILE and all the options to "".
# 2. Process the command line.  Throw an error if a non-accumulating option
#    is specified twice.
# 3. If CONFFILE is "", set CONFFILE to /etc/freebsd-update.conf .
# 4. For all the configuration options X, set X_saved to X.
# 5. Initialize all the options to "".
# 6. Read CONFFILE line by line, parsing options.
# 7. For each configuration option X, set X to X_saved iff X_saved is not "".
# 8. Repeat steps 4-7, except setting options to their default values at (6).

CONFIGOPTIONS="KEYPRINT WORKDIR SERVERNAME MAILTO ALLOWADD ALLOWDELETE
    KEEPMODIFIEDMETADATA COMPONENTS IGNOREPATHS UPDATEIFUNMODIFIED
    BASEDIR VERBOSELEVEL"

# Set all the configuration options to "".
nullconfig () {
	for X in ${CONFIGOPTIONS}; do
		eval ${X}=""
	done
}

# For each configuration option X, set X_saved to X.
saveconfig () {
	for X in ${CONFIGOPTIONS}; do
		eval ${X}_saved=\$${X}
	done
}

# For each configuration option X, set X to X_saved if X_saved is not "".
mergeconfig () {
	for X in ${CONFIGOPTIONS}; do
		eval _=\$${X}_saved
		if ! [ -z "${_}" ]; then
			eval ${X}=\$${X}_saved
		fi
	done
}

# Set the trusted keyprint.
config_KeyPrint () {
	if [ -z ${KEYPRINT} ]; then
		KEYPRINT=$1
	else
		return 1
	fi
}

# Set the working directory.
config_WorkDir () {
	if [ -z ${WORKDIR} ]; then
		WORKDIR=$1
	else
		return 1
	fi
}

# Set the name of the server (pool) from which to fetch updates
config_ServerName () {
	if [ -z ${SERVERNAME} ]; then
		SERVERNAME=$1
	else
		return 1
	fi
}

# Set the address to which 'cron' output will be mailed.
config_MailTo () {
	if [ -z ${MAILTO} ]; then
		MAILTO=$1
	else
		return 1
	fi
}

# Set whether FreeBSD Update is allowed to add files (or directories, or
# symlinks) which did not previously exist.
config_AllowAdd () {
	if [ -z ${ALLOWADD} ]; then
		case $1 in
		[Yy][Ee][Ss])
			ALLOWADD=yes
			;;
		[Nn][Oo])
			ALLOWADD=no
			;;
		*)
			return 1
			;;
		esac
	else
		return 1
	fi
}

# Set whether FreeBSD Update is allowed to remove files/directories/symlinks.
config_AllowDelete () {
	if [ -z ${ALLOWDELETE} ]; then
		case $1 in
		[Yy][Ee][Ss])
			ALLOWDELETE=yes
			;;
		[Nn][Oo])
			ALLOWDELETE=no
			;;
		*)
			return 1
			;;
		esac
	else
		return 1
	fi
}

# Set whether FreeBSD Update should keep existing inode ownership,
# permissions, and flags, in the event that they have been modified locally
# after the release.
config_KeepModifiedMetadata () {
	if [ -z ${KEEPMODIFIEDMETADATA} ]; then
		case $1 in
		[Yy][Ee][Ss])
			KEEPMODIFIEDMETADATA=yes
			;;
		[Nn][Oo])
			KEEPMODIFIEDMETADATA=no
			;;
		*)
			return 1
			;;
		esac
	else
		return 1
	fi
}

# Add to the list of components which should be kept updated.
config_Components () {
	for C in $@; do
		COMPONENTS="${COMPONENTS} ${C}"
	done
}

# Add to the list of paths under which updates will be ignored.
config_IgnorePaths () {
	for C in $@; do
		IGNOREPATHS="${IGNOREPATHS} ${C}"
	done
}

# Add to the list of paths within which updates will be performed only if the
# file on disk has not been modified locally.
config_UpdateIfUnmodified () {
	for C in $@; do
		UPDATEIFUNMODIFIED="${UPDATEIFUNMODIFIED} ${C}"
	done
}

# Work on a FreeBSD installation mounted under $1
config_BaseDir () {
	if [ -z ${BASEDIR} ]; then
		BASEDIR=$1
	else
		return 1
	fi
}

# Define what happens to output of utilities
config_VerboseLevel () {
	if [ -z ${VERBOSELEVEL} ]; then
		case $1 in
		[Dd][Ee][Bb][Uu][Gg])
			VERBOSELEVEL=debug
			;;
		[Nn][Oo][Ss][Tt][Aa][Tt][Ss])
			VERBOSELEVEL=nostats
			;;
		[Ss][Tt][Aa][Tt][Ss])
			VERBOSELEVEL=stats
			;;
		*)
			return 1
			;;
		esac
	else
		return 1
	fi
}

# Handle one line of configuration
configline () {
	if [ $# -eq 0 ]; then
		return
	fi

	OPT=$1
	shift
	config_${OPT} $@
}

#### Parameter handling functions.

# Initialize parameters to null, just in case they're
# set in the environment.
init_params () {
	# Configration settings
	nullconfig

	# No configuration file set yet
	CONFFILE=""

	# No commands specified yet
	COMMANDS=""
}

# Parse the command line
parse_cmdline () {
	while [ $# -gt 0 ]; do
		case "$1" in
		# Location of configuration file
		-f)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${CONFFILE}" ]; then usage; fi
			shift; CONFFILE="$1"
			;;

		# Configuration file equivalents
		-b)
			if [ $# -eq 1 ]; then usage; fi; shift
			config_BaseDir $1 || usage
			;;
		-d)
			if [ $# -eq 1 ]; then usage; fi; shift
			config_WorkDir $1 || usage
			;;
		-k)
			if [ $# -eq 1 ]; then usage; fi; shift
			config_KeyPrint $1 || usage
			;;
		-s)
			if [ $# -eq 1 ]; then usage; fi; shift
			config_ServerName $1 || usage
			;;
		-t)
			if [ $# -eq 1 ]; then usage; fi; shift
			config_MailTo $1 || usage
			;;
		-v)
			if [ $# -eq 1 ]; then usage; fi; shift
			config_VerboseLevel $1 || usage
			;;

		# Aliases for "-v debug" and "-v nostats"
		--debug)
			config_VerboseLevel debug || usage
			;;
		--no-stats)
			config_VerboseLevel nostats || usage
			;;

		# Commands
		cron | fetch | install | rollback)
			COMMANDS="${COMMANDS} $1"
			;;

		# Anything else is an error
		*)
			usage
			;;
		esac
		shift
	done

	# Make sure we have at least one command
	if [ -z "${COMMANDS}" ]; then
		usage
	fi
}

# Parse the configuration file
parse_conffile () {
	# If a configuration file was specified on the command line, check
	# that it exists and is readable.
	if [ ! -z "${CONFFILE}" ] && [ ! -r "${CONFFILE}" ]; then
		echo -n "File does not exist "
		echo -n "or is not readable: "
		echo ${CONFFILE}
		exit 1
	fi

	# If a configuration file was not specified on the command line,
	# use the default configuration file path.  If that default does
	# not exist, give up looking for any configuration.
	if [ -z "${CONFFILE}" ]; then
		CONFFILE="/etc/freebsd-update.conf"
		if [ ! -r "${CONFFILE}" ]; then
			return
		fi
	fi

	# Save the configuration options specified on the command line, and
	# clear all the options in preparation for reading the config file.
	saveconfig
	nullconfig

	# Read the configuration file.  Anything after the first '#' is
	# ignored, and any blank lines are ignored.
	L=0
	while read LINE; do
		L=$(($L + 1))
		LINEX=`echo "${LINE}" | cut -f 1 -d '#'`
		if ! configline ${LINEX}; then
			echo "Error processing configuration file, line $L:"
			echo "==> ${LINE}"
			exit 1
		fi
	done < ${CONFFILE}

	# Merge the settings read from the configuration file with those
	# provided at the command line.
	mergeconfig
}

# Provide some default parameters
default_params () {
	# Save any parameters already configured, and clear the slate
	saveconfig
	nullconfig

	# Default configurations
	config_WorkDir /var/db/freebsd-update
	config_MailTo root
	config_AllowAdd yes
	config_AllowDelete yes
	config_KeepModifiedMetadata yes
	config_BaseDir /
	config_VerboseLevel stats

	# Merge these defaults into the earlier-configured settings
	mergeconfig
}

# Set utility output filtering options, based on ${VERBOSELEVEL}
fetch_setup_verboselevel () {
	case ${VERBOSELEVEL} in
	debug)
		QUIETREDIR="/dev/stderr"
		QUIETFLAG=" "
		STATSREDIR="/dev/stderr"
		DDSTATS=".."
		XARGST="-t"
		NDEBUG=" "
		;;
	nostats)
		QUIETREDIR=""
		QUIETFLAG=""
		STATSREDIR="/dev/null"
		DDSTATS=".."
		XARGST=""
		NDEBUG=""
		;;
	stats)
		QUIETREDIR="/dev/null"
		QUIETFLAG="-q"
		STATSREDIR="/dev/stdout"
		DDSTATS=""
		XARGST=""
		NDEBUG="-n"
		;;
	esac
}

# Perform sanity checks and set some final parameters
# in preparation for fetching files.  Figure out which
# set of updates should be downloaded: If the user is
# running *-p[0-9]+, strip off the last part; if the
# user is running -SECURITY, call it -RELEASE.  Chdir
# into the working directory.
fetch_check_params () {
	export HTTP_USER_AGENT="freebsd-update (${COMMAND}, `uname -r`)"

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

	# Generate release number.  The s/SECURITY/RELEASE/ bit exists
	# to provide an upgrade path for FreeBSD Update 1.x users, since
	# the kernels provided by FreeBSD Update 1.x are always labelled
	# as X.Y-SECURITY.
	RELNUM=`uname -r |
	    sed -E 's,-p[0-9]+,,' |
	    sed -E 's,-SECURITY,-RELEASE,'`
	ARCH=`uname -m`
	FETCHDIR=${RELNUM}/${ARCH}

	# Figure out what directory contains the running kernel
	BOOTFILE=`sysctl -n kern.bootfile`
	KERNELDIR=${BOOTFILE%/kernel}
	if ! [ -d ${KERNELDIR} ]; then
		echo "Cannot identify running kernel"
		exit 1
	fi

	# Define some paths
	BSPATCH=/usr/bin/bspatch
	SHA256=/sbin/sha256
	PHTTPGET=/usr/libexec/phttpget

	# Set up variables relating to VERBOSELEVEL
	fetch_setup_verboselevel

	# Construct a unique name from ${BASEDIR}
	BDHASH=`echo ${BASEDIR} | sha256 -q`
}

# Perform sanity checks and set some final parameters in
# preparation for installing updates.
install_check_params () {
	# Check that we are root.  All sorts of things won't work otherwise.
	if [ `id -u` != 0 ]; then
		echo "You must be root to run this."
		exit 1
	fi

	# Check that we have a working directory
	_WORKDIR_bad="Directory does not exist or is not writable: "
	if ! [ -d "${WORKDIR}" -a -w "${WORKDIR}" ]; then
		echo -n "`basename $0`: "
		echo -n "${_WORKDIR_bad}"
		echo ${WORKDIR}
		exit 1
	fi
	cd ${WORKDIR} || exit 1

	# Construct a unique name from ${BASEDIR}
	BDHASH=`echo ${BASEDIR} | sha256 -q`

	# Check that we have updates ready to install
	if ! [ -L ${BDHASH}-install ]; then
		echo "No updates are available to install."
		echo "Run '$0 fetch' first."
		exit 1
	fi
	if ! [ -f ${BDHASH}-install/INDEX-OLD ] ||
	    ! [ -f ${BDHASH}-install/INDEX-NEW ]; then
		echo "Update manifest is corrupt -- this should never happen."
		echo "Re-run '$0 fetch'."
		exit 1
	fi
}

# Perform sanity checks and set some final parameters in
# preparation for UNinstalling updates.
rollback_check_params () {
	# Check that we are root.  All sorts of things won't work otherwise.
	if [ `id -u` != 0 ]; then
		echo "You must be root to run this."
		exit 1
	fi

	# Check that we have a working directory
	_WORKDIR_bad="Directory does not exist or is not writable: "
	if ! [ -d "${WORKDIR}" -a -w "${WORKDIR}" ]; then
		echo -n "`basename $0`: "
		echo -n "${_WORKDIR_bad}"
		echo ${WORKDIR}
		exit 1
	fi
	cd ${WORKDIR} || exit 1

	# Construct a unique name from ${BASEDIR}
	BDHASH=`echo ${BASEDIR} | sha256 -q`

	# Check that we have updates ready to rollback
	if ! [ -L ${BDHASH}-rollback ]; then
		echo "No rollback directory found."
		exit 1
	fi
	if ! [ -f ${BDHASH}-rollback/INDEX-OLD ] ||
	    ! [ -f ${BDHASH}-rollback/INDEX-NEW ]; then
		echo "Update manifest is corrupt -- this should never happen."
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
# We ignore the Port field, since we are always going to use port 80.

# Fetch the mirror list, but do not pick a mirror yet.  Returns 1 if
# no mirrors are available for any reason.
fetch_pick_server_init () {
	: > serverlist_tried

# Check that host(1) exists (i.e., that the system wasn't built with the
# WITHOUT_BIND set) and don't try to find a mirror if it doesn't exist.
	if ! which -s host; then
		: > serverlist_full
		return 1
	fi

	echo -n "Looking up ${SERVERNAME} mirrors... "

# Issue the SRV query and pull out the Priority, Weight, and Target fields.
# BIND 9 prints "$name has SRV record ..." while BIND 8 prints
# "$name server selection ..."; we allow either format.
	MLIST="_http._tcp.${SERVERNAME}"
	host -t srv "${MLIST}" |
	    sed -nE "s/${MLIST} (has SRV record|server selection) //p" |
	    cut -f 1,2,4 -d ' ' |
	    sed -e 's/\.$//' |
	    sort > serverlist_full

# If no records, give up -- we'll just use the server name we were given.
	if [ `wc -l < serverlist_full` -eq 0 ]; then
		echo "none found."
		return 1
	fi

# Report how many mirrors we found.
	echo `wc -l < serverlist_full` "mirrors found."

# Generate a random seed for use in picking mirrors.  If HTTP_PROXY
# is set, this will be used to generate the seed; otherwise, the seed
# will be random.
	if [ -n "${HTTP_PROXY}${http_proxy}" ]; then
		RANDVALUE=`sha256 -qs "${HTTP_PROXY}${http_proxy}" |
		    tr -d 'a-f' |
		    cut -c 1-9`
	else
		RANDVALUE=`jot -r 1 0 999999999`
	fi
}

# Pick a mirror.  Returns 1 if we have run out of mirrors to try.
fetch_pick_server () {
# Generate a list of not-yet-tried mirrors
	sort serverlist_tried |
	    comm -23 serverlist_full - > serverlist

# Have we run out of mirrors?
	if [ `wc -l < serverlist` -eq 0 ]; then
		echo "No mirrors remaining, giving up."
		return 1
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

# Pick a value between 0 and the sum of the weights - 1
	SRV_RND=`expr ${RANDVALUE} % ${SRV_WSUM}`

# Read through the list of mirrors and set SERVERNAME.  Write the line
# corresponding to the mirror we selected into serverlist_tried so that
# we won't try it again.
	while read X; do
		case "$X" in
		${SRV_PRIORITY}\ *)
			SRV_W=`echo $X | cut -f 2 -d ' '`
			SRV_W=$(($SRV_W + $SRV_W_ADD))
			if [ $SRV_RND -lt $SRV_W ]; then
				SERVERNAME=`echo $X | cut -f 3 -d ' '`
				echo "$X" >> serverlist_tried
				break
			else
				SRV_RND=$(($SRV_RND - $SRV_W))
			fi
			;;
		esac
	done < serverlist
}

# Take a list of ${oldhash}|${newhash} and output a list of needed patches,
# i.e., those for which we have ${oldhash} and don't have ${newhash}.
fetch_make_patchlist () {
	grep -vE "^([0-9a-f]{64})\|\1$" |
	    tr '|' ' ' |
		while read X Y; do
			if [ -f "files/${Y}.gz" ] ||
			    [ ! -f "files/${X}.gz" ]; then
				continue
			fi
			echo "${X}|${Y}"
		done | uniq
}

# Print user-friendly progress statistics
fetch_progress () {
	LNC=0
	while read x; do
		LNC=$(($LNC + 1))
		if [ $(($LNC % 10)) = 0 ]; then
			echo -n $LNC
		elif [ $(($LNC % 2)) = 0 ]; then
			echo -n .
		fi
	done
	echo -n " "
}

# Initialize the working directory
workdir_init () {
	mkdir -p files
	touch tINDEX.present
}

# Check that we have a public key with an appropriate hash, or
# fetch the key if it doesn't exist.  Returns 1 if the key has
# not yet been fetched.
fetch_key () {
	if [ -r pub.ssl ] && [ `${SHA256} -q pub.ssl` = ${KEYPRINT} ]; then
		return 0
	fi

	echo -n "Fetching public key from ${SERVERNAME}... "
	rm -f pub.ssl
	fetch ${QUIETFLAG} http://${SERVERNAME}/${FETCHDIR}/pub.ssl \
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

# Fetch metadata signature, aka "tag".
fetch_tag () {
	echo ${NDEBUG} "Fetching metadata signature from ${SERVERNAME}... "
	rm -f latest.ssl
	fetch ${QUIETFLAG} http://${SERVERNAME}/${FETCHDIR}/latest.ssl	\
	    2>${QUIETREDIR} || true
	if ! [ -r latest.ssl ]; then
		echo "failed."
		return 1
	fi

	openssl rsautl -pubin -inkey pub.ssl -verify		\
	    < latest.ssl > tag.new 2>${QUIETREDIR} || true
	rm latest.ssl

	if ! [ `wc -l < tag.new` = 1 ] ||
	    ! grep -qE	\
    "^freebsd-update\|${ARCH}\|${RELNUM}\|[0-9]+\|[0-9a-f]{64}\|[0-9]{10}" \
		tag.new; then
		echo "invalid signature."
		return 1
	fi

	echo "done."

	RELPATCHNUM=`cut -f 4 -d '|' < tag.new`
	TINDEXHASH=`cut -f 5 -d '|' < tag.new`
	EOLTIME=`cut -f 6 -d '|' < tag.new`
}

# Sanity-check the patch number in a tag, to make sure that we're not
# going to "update" backwards and to prevent replay attacks.
fetch_tagsanity () {
	# Check that we're not going to move from -pX to -pY with Y < X.
	RELPX=`uname -r | sed -E 's,.*-,,'`
	if echo ${RELPX} | grep -qE '^p[0-9]+$'; then
		RELPX=`echo ${RELPX} | cut -c 2-`
	else
		RELPX=0
	fi
	if [ "${RELPATCHNUM}" -lt "${RELPX}" ]; then
		echo
		echo -n "Files on mirror (${RELNUM}-p${RELPATCHNUM})"
		echo " appear older than what"
		echo "we are currently running (`uname -r`)!"
		echo "Cowardly refusing to proceed any further."
		return 1
	fi

	# If "tag" exists and corresponds to ${RELNUM}, make sure that
	# it contains a patch number <= RELPATCHNUM, in order to protect
	# against rollback (replay) attacks.
	if [ -f tag ] &&
	    grep -qE	\
    "^freebsd-update\|${ARCH}\|${RELNUM}\|[0-9]+\|[0-9a-f]{64}\|[0-9]{10}" \
		tag; then
		LASTRELPATCHNUM=`cut -f 4 -d '|' < tag`

		if [ "${RELPATCHNUM}" -lt "${LASTRELPATCHNUM}" ]; then
			echo
			echo -n "Files on mirror (${RELNUM}-p${RELPATCHNUM})"
			echo " are older than the"
			echo -n "most recently seen updates"
			echo " (${RELNUM}-p${LASTRELPATCHNUM})."
			echo "Cowardly refusing to proceed any further."
			return 1
		fi
	fi
}

# Fetch metadata index file
fetch_metadata_index () {
	echo ${NDEBUG} "Fetching metadata index... "
	rm -f ${TINDEXHASH}
	fetch ${QUIETFLAG} http://${SERVERNAME}/${FETCHDIR}/t/${TINDEXHASH}
	    2>${QUIETREDIR}
	if ! [ -f ${TINDEXHASH} ]; then
		echo "failed."
		return 1
	fi
	if [ `${SHA256} -q ${TINDEXHASH}` != ${TINDEXHASH} ]; then
		echo "update metadata index corrupt."
		return 1
	fi
	echo "done."
}

# Print an error message about signed metadata being bogus.
fetch_metadata_bogus () {
	echo
	echo "The update metadata$1 is correctly signed, but"
	echo "failed an integrity check."
	echo "Cowardly refusing to proceed any further."
	return 1
}

# Construct tINDEX.new by merging the lines named in $1 from ${TINDEXHASH}
# with the lines not named in $@ from tINDEX.present (if that file exists).
fetch_metadata_index_merge () {
	for METAFILE in $@; do
		if [ `grep -E "^${METAFILE}\|" ${TINDEXHASH} | wc -l`	\
		    -ne 1 ]; then
			fetch_metadata_bogus " index"
			return 1
		fi

		grep -E "${METAFILE}\|" ${TINDEXHASH}
	done |
	    sort > tINDEX.wanted

	if [ -f tINDEX.present ]; then
		join -t '|' -v 2 tINDEX.wanted tINDEX.present |
		    sort -m - tINDEX.wanted > tINDEX.new
		rm tINDEX.wanted
	else
		mv tINDEX.wanted tINDEX.new
	fi
}

# Sanity check all the lines of tINDEX.new.  Even if more metadata lines
# are added by future versions of the server, this won't cause problems,
# since the only lines which appear in tINDEX.new are the ones which we
# specifically grepped out of ${TINDEXHASH}.
fetch_metadata_index_sanity () {
	if grep -qvE '^[0-9A-Z.-]+\|[0-9a-f]{64}$' tINDEX.new; then
		fetch_metadata_bogus " index"
		return 1
	fi
}

# Sanity check the metadata file $1.
fetch_metadata_sanity () {
	# Some aliases to save space later: ${P} is a character which can
	# appear in a path; ${M} is the four numeric metadata fields; and
	# ${H} is a sha256 hash.
	P="[-+./:=_[[:alnum:]]"
	M="[0-9]+\|[0-9]+\|[0-9]+\|[0-9]+"
	H="[0-9a-f]{64}"

	# Check that the first four fields make sense.
	if gunzip -c < files/$1.gz |
	    grep -qvE "^[a-z]+\|[0-9a-z]+\|${P}+\|[fdL-]\|"; then
		fetch_metadata_bogus ""
		return 1
	fi

	# Remove the first three fields.
	gunzip -c < files/$1.gz |
	    cut -f 4- -d '|' > sanitycheck.tmp

	# Sanity check entries with type 'f'
	if grep -E '^f' sanitycheck.tmp |
	    grep -qvE "^f\|${M}\|${H}\|${P}*\$"; then
		fetch_metadata_bogus ""
		return 1
	fi

	# Sanity check entries with type 'd'
	if grep -E '^d' sanitycheck.tmp |
	    grep -qvE "^d\|${M}\|\|\$"; then
		fetch_metadata_bogus ""
		return 1
	fi

	# Sanity check entries with type 'L'
	if grep -E '^L' sanitycheck.tmp |
	    grep -qvE "^L\|${M}\|${P}*\|\$"; then
		fetch_metadata_bogus ""
		return 1
	fi

	# Sanity check entries with type '-'
	if grep -E '^-' sanitycheck.tmp |
	    grep -qvE "^-\|\|\|\|\|\|"; then
		fetch_metadata_bogus ""
		return 1
	fi

	# Clean up
	rm sanitycheck.tmp
}

# Fetch the metadata index and metadata files listed in $@,
# taking advantage of metadata patches where possible.
fetch_metadata () {
	fetch_metadata_index || return 1
	fetch_metadata_index_merge $@ || return 1
	fetch_metadata_index_sanity || return 1

	# Generate a list of wanted metadata patches
	join -t '|' -o 1.2,2.2 tINDEX.present tINDEX.new |
	    fetch_make_patchlist > patchlist

	if [ -s patchlist ]; then
		# Attempt to fetch metadata patches
		echo -n "Fetching `wc -l < patchlist | tr -d ' '` "
		echo ${NDEBUG} "metadata patches.${DDSTATS}"
		tr '|' '-' < patchlist |
		    lam -s "${FETCHDIR}/tp/" - -s ".gz" |
		    xargs ${XARGST} ${PHTTPGET} ${SERVERNAME}	\
			2>${STATSREDIR} | fetch_progress
		echo "done."

		# Attempt to apply metadata patches
		echo -n "Applying metadata patches... "
		tr '|' ' ' < patchlist |
		    while read X Y; do
			if [ ! -f "${X}-${Y}.gz" ]; then continue; fi
			gunzip -c < ${X}-${Y}.gz > diff
			gunzip -c < files/${X}.gz > diff-OLD

			# Figure out which lines are being added and removed
			grep -E '^-' diff |
			    cut -c 2- |
			    while read PREFIX; do
				look "${PREFIX}" diff-OLD
			    done |
			    sort > diff-rm
			grep -E '^\+' diff |
			    cut -c 2- > diff-add

			# Generate the new file
			comm -23 diff-OLD diff-rm |
			    sort - diff-add > diff-NEW

			if [ `${SHA256} -q diff-NEW` = ${Y} ]; then
				mv diff-NEW files/${Y}
				gzip -n files/${Y}
			else
				mv diff-NEW ${Y}.bad
			fi
			rm -f ${X}-${Y}.gz diff
			rm -f diff-OLD diff-NEW diff-add diff-rm
		done 2>${QUIETREDIR}
		echo "done."
	fi

	# Update metadata without patches
	cut -f 2 -d '|' < tINDEX.new |
	    while read Y; do
		if [ ! -f "files/${Y}.gz" ]; then
			echo ${Y};
		fi
	    done |
	    sort -u > filelist

	if [ -s filelist ]; then
		echo -n "Fetching `wc -l < filelist | tr -d ' '` "
		echo ${NDEBUG} "metadata files... "
		lam -s "${FETCHDIR}/m/" - -s ".gz" < filelist |
		    xargs ${XARGST} ${PHTTPGET} ${SERVERNAME}	\
		    2>${QUIETREDIR}

		while read Y; do
			if ! [ -f ${Y}.gz ]; then
				echo "failed."
				return 1
			fi
			if [ `gunzip -c < ${Y}.gz |
			    ${SHA256} -q` = ${Y} ]; then
				mv ${Y}.gz files/${Y}.gz
			else
				echo "metadata is corrupt."
				return 1
			fi
		done < filelist
		echo "done."
	fi

# Sanity-check the metadata files.
	cut -f 2 -d '|' tINDEX.new > filelist
	while read X; do
		fetch_metadata_sanity ${X} || return 1
	done < filelist

# Remove files which are no longer needed
	cut -f 2 -d '|' tINDEX.present |
	    sort > oldfiles
	cut -f 2 -d '|' tINDEX.new |
	    sort |
	    comm -13 - oldfiles |
	    lam -s "files/" - -s ".gz" |
	    xargs rm -f
	rm patchlist filelist oldfiles
	rm ${TINDEXHASH}

# We're done!
	mv tINDEX.new tINDEX.present
	mv tag.new tag

	return 0
}

# Generate a filtered version of the metadata file $1 from the downloaded
# file, by fishing out the lines corresponding to components we're trying
# to keep updated, and then removing lines corresponding to paths we want
# to ignore.
fetch_filter_metadata () {
	METAHASH=`look "$1|" tINDEX.present | cut -f 2 -d '|'`
	gunzip -c < files/${METAHASH}.gz > $1.all

	# Fish out the lines belonging to components we care about.
	# Canonicalize directory names by removing any trailing / in
	# order to avoid listing directories multiple times if they
	# belong to multiple components.  Turning "/" into "" doesn't
	# matter, since we add a leading "/" when we use paths later.
	for C in ${COMPONENTS}; do
		look "`echo ${C} | tr '/' '|'`|" $1.all
	done |
	    cut -f 3- -d '|' |
	    sed -e 's,/|d|,|d|,' |
	    sort -u > $1.tmp

	# Figure out which lines to ignore and remove them.
	for X in ${IGNOREPATHS}; do
		grep -E "^${X}" $1.tmp
	done |
	    sort -u |
	    comm -13 - $1.tmp > $1

	# Remove temporary files.
	rm $1.all $1.tmp
}

# Filter the metadata file $1 by adding lines with "/boot/`uname -i`"
# replaced by ${KERNELDIR} (which is `sysctl -n kern.bootfile` minus the
# trailing "/kernel"); and if "/boot/`uname -i`" does not exist, remove
# the original lines which start with that.
# Put another way: Deal with the fact that the FOO kernel is sometimes
# installed in /boot/FOO/ and is sometimes installed elsewhere.
fetch_filter_kernel_names () {
	KERNCONF=`uname -i`

	grep ^/boot/${KERNCONF} $1 |
	    sed -e "s,/boot/${KERNCONF},${KERNELDIR},g" |
	    sort - $1 > $1.tmp
	mv $1.tmp $1

	if ! [ -d /boot/${KERNCONF} ]; then
		grep -v ^/boot/${KERNCONF} $1 > $1.tmp
		mv $1.tmp $1
	fi
}

# For all paths appearing in $1 or $3, inspect the system
# and generate $2 describing what is currently installed.
fetch_inspect_system () {
	# No errors yet...
	rm -f .err

	# Tell the user why his disk is suddenly making lots of noise
	echo -n "Inspecting system... "

	# Generate list of files to inspect
	cat $1 $3 |
	    cut -f 1 -d '|' |
	    sort -u > filelist

	# Examine each file and output lines of the form
	# /path/to/file|type|device-inum|user|group|perm|flags|value
	# sorted by device and inode number.
	while read F; do
		# If the symlink/file/directory does not exist, record this.
		if ! [ -e ${BASEDIR}/${F} ]; then
			echo "${F}|-||||||"
			continue
		fi
		if ! [ -r ${BASEDIR}/${F} ]; then
			echo "Cannot read file: ${BASEDIR}/${F}"	\
			    >/dev/stderr
			touch .err
			return 1
		fi

		# Otherwise, output an index line.
		if [ -L ${BASEDIR}/${F} ]; then
			echo -n "${F}|L|"
			stat -n -f '%d-%i|%u|%g|%Mp%Lp|%Of|' ${BASEDIR}/${F};
			readlink ${BASEDIR}/${F};
		elif [ -f ${BASEDIR}/${F} ]; then
			echo -n "${F}|f|"
			stat -n -f '%d-%i|%u|%g|%Mp%Lp|%Of|' ${BASEDIR}/${F};
			sha256 -q ${BASEDIR}/${F};
		elif [ -d ${BASEDIR}/${F} ]; then
			echo -n "${F}|d|"
			stat -f '%d-%i|%u|%g|%Mp%Lp|%Of|' ${BASEDIR}/${F};
		else
			echo "Unknown file type: ${BASEDIR}/${F}"	\
			    >/dev/stderr
			touch .err
			return 1
		fi
	done < filelist |
	    sort -k 3,3 -t '|' > $2.tmp
	rm filelist

	# Check if an error occured during system inspection
	if [ -f .err ]; then
		return 1
	fi

	# Convert to the form
	# /path/to/file|type|user|group|perm|flags|value|hlink
	# by resolving identical device and inode numbers into hard links.
	cut -f 1,3 -d '|' $2.tmp |
	    sort -k 1,1 -t '|' |
	    sort -s -u -k 2,2 -t '|' |
	    join -1 2 -2 3 -t '|' - $2.tmp |
	    awk -F \| -v OFS=\|		\
		'{
		    if (($2 == $3) || ($4 == "-"))
			print $3,$4,$5,$6,$7,$8,$9,""
		    else
			print $3,$4,$5,$6,$7,$8,$9,$2
		}' |
	    sort > $2
	rm $2.tmp

	# We're finished looking around
	echo "done."
}

# For any paths matching ${UPDATEIFUNMODIFIED}, remove lines from $[123]
# which correspond to lines in $2 with hashes not matching $1 or $3.  For
# entries in $2 marked "not present" (aka. type -), remove lines from $[123]
# unless there is a corresponding entry in $1.
fetch_filter_unmodified_notpresent () {
	# Figure out which lines of $1 and $3 correspond to bits which
	# should only be updated if they haven't changed, and fish out
	# the (path, type, value) tuples.
	# NOTE: We don't consider a file to be "modified" if it matches
	# the hash from $3.
	for X in ${UPDATEIFUNMODIFIED}; do
		grep -E "^${X}" $1
		grep -E "^${X}" $3
	done |
	    cut -f 1,2,7 -d '|' |
	    sort > $1-values

	# Do the same for $2.
	for X in ${UPDATEIFUNMODIFIED}; do
		grep -E "^${X}" $2
	done |
	    cut -f 1,2,7 -d '|' |
	    sort > $2-values

	# Any entry in $2-values which is not in $1-values corresponds to
	# a path which we need to remove from $1, $2, and $3.
	comm -13 $1-values $2-values > mlines
	rm $1-values $2-values

	# Any lines in $2 which are not in $1 AND are "not present" lines
	# also belong in mlines.
	comm -13 $1 $2 |
	    cut -f 1,2,7 -d '|' |
	    fgrep '|-|' >> mlines

	# Remove lines from $1, $2, and $3
	for X in $1 $2 $3; do
		sort -t '|' -k 1,1 ${X} > ${X}.tmp
		cut -f 1 -d '|' < mlines |
		    sort |
		    join -v 2 -t '|' - ${X}.tmp |
		    sort > ${X}
		rm ${X}.tmp
	done

	# Store a list of the modified files, for future reference
	fgrep -v '|-|' mlines |
	    cut -f 1 -d '|' > modifiedfiles
	rm mlines
}

# For each entry in $1 of type -, remove any corresponding
# entry from $2 if ${ALLOWADD} != "yes".  Remove all entries
# of type - from $1.
fetch_filter_allowadd () {
	cut -f 1,2 -d '|' < $1 |
	    fgrep '|-' |
	    cut -f 1 -d '|' > filesnotpresent

	if [ ${ALLOWADD} != "yes" ]; then
		sort < $2 |
		    join -v 1 -t '|' - filesnotpresent |
		    sort > $2.tmp
		mv $2.tmp $2
	fi

	sort < $1 |
	    join -v 1 -t '|' - filesnotpresent |
	    sort > $1.tmp
	mv $1.tmp $1
	rm filesnotpresent
}

# If ${ALLOWDELETE} != "yes", then remove any entries from $1
# which don't correspond to entries in $2.
fetch_filter_allowdelete () {
	# Produce a lists ${PATH}|${TYPE}
	for X in $1 $2; do
		cut -f 1-2 -d '|' < ${X} |
		    sort -u > ${X}.nodes
	done

	# Figure out which lines need to be removed from $1.
	if [ ${ALLOWDELETE} != "yes" ]; then
		comm -23 $1.nodes $2.nodes > $1.badnodes
	else
		: > $1.badnodes
	fi

	# Remove the relevant lines from $1
	while read X; do
		look "${X}|" $1
	done < $1.badnodes |
	    comm -13 - $1 > $1.tmp
	mv $1.tmp $1

	rm $1.badnodes $1.nodes $2.nodes
}

# If ${KEEPMODIFIEDMETADATA} == "yes", then for each entry in $2
# with metadata not matching any entry in $1, replace the corresponding
# line of $3 with one having the same metadata as the entry in $2.
fetch_filter_modified_metadata () {
	# Fish out the metadata from $1 and $2
	for X in $1 $2; do
		cut -f 1-6 -d '|' < ${X} > ${X}.metadata
	done

	# Find the metadata we need to keep
	if [ ${KEEPMODIFIEDMETADATA} = "yes" ]; then
		comm -13 $1.metadata $2.metadata > keepmeta
	else
		: > keepmeta
	fi

	# Extract the lines which we need to remove from $3, and
	# construct the lines which we need to add to $3.
	: > $3.remove
	: > $3.add
	while read LINE; do
		NODE=`echo "${LINE}" | cut -f 1-2 -d '|'`
		look "${NODE}|" $3 >> $3.remove
		look "${NODE}|" $3 |
		    cut -f 7- -d '|' |
		    lam -s "${LINE}|" - >> $3.add
	done < keepmeta

	# Remove the specified lines and add the new lines.
	sort $3.remove |
	    comm -13 - $3 |
	    sort -u - $3.add > $3.tmp
	mv $3.tmp $3

	rm keepmeta $1.metadata $2.metadata $3.add $3.remove
}

# Remove lines from $1 and $2 which are identical;
# no need to update a file if it isn't changing.
fetch_filter_uptodate () {
	comm -23 $1 $2 > $1.tmp
	comm -13 $1 $2 > $2.tmp

	mv $1.tmp $1
	mv $2.tmp $2
}

# Prepare to fetch files: Generate a list of the files we need,
# copy the unmodified files we have into /files/, and generate
# a list of patches to download.
fetch_files_prepare () {
	# Tell the user why his disk is suddenly making lots of noise
	echo -n "Preparing to download files... "

	# Reduce indices to ${PATH}|${HASH} pairs
	for X in $1 $2 $3; do
		cut -f 1,2,7 -d '|' < ${X} |
		    fgrep '|f|' |
		    cut -f 1,3 -d '|' |
		    sort > ${X}.hashes
	done

	# List of files wanted
	cut -f 2 -d '|' < $3.hashes |
	    sort -u > files.wanted

	# Generate a list of unmodified files
	comm -12 $1.hashes $2.hashes |
	    sort -k 1,1 -t '|' > unmodified.files

	# Copy all files into /files/.  We only need the unmodified files
	# for use in patching; but we'll want all of them if the user asks
	# to rollback the updates later.
	cut -f 1 -d '|' < $2.hashes |
	    while read F; do
		cp "${BASEDIR}/${F}" tmpfile
		gzip -c < tmpfile > files/`sha256 -q tmpfile`.gz
		rm tmpfile
	    done

	# Produce a list of patches to download
	sort -k 1,1 -t '|' $3.hashes |
	    join -t '|' -o 2.2,1.2 - unmodified.files |
	    fetch_make_patchlist > patchlist

	# Garbage collect
	rm unmodified.files $1.hashes $2.hashes $3.hashes

	# We don't need the list of possible old files any more.
	rm $1

	# We're finished making noise
	echo "done."
}

# Fetch files.
fetch_files () {
	# Attempt to fetch patches
	if [ -s patchlist ]; then
		echo -n "Fetching `wc -l < patchlist | tr -d ' '` "
		echo ${NDEBUG} "patches.${DDSTATS}"
		tr '|' '-' < patchlist |
		    lam -s "${FETCHDIR}/bp/" - |
		    xargs ${XARGST} ${PHTTPGET} ${SERVERNAME}	\
			2>${STATSREDIR} | fetch_progress
		echo "done."

		# Attempt to apply patches
		echo -n "Applying patches... "
		tr '|' ' ' < patchlist |
		    while read X Y; do
			if [ ! -f "${X}-${Y}" ]; then continue; fi
			gunzip -c < files/${X}.gz > OLD

			bspatch OLD NEW ${X}-${Y}

			if [ `${SHA256} -q NEW` = ${Y} ]; then
				mv NEW files/${Y}
				gzip -n files/${Y}
			fi
			rm -f diff OLD NEW ${X}-${Y}
		done 2>${QUIETREDIR}
		echo "done."
	fi

	# Download files which couldn't be generate via patching
	while read Y; do
		if [ ! -f "files/${Y}.gz" ]; then
			echo ${Y};
		fi
	done < files.wanted > filelist

	if [ -s filelist ]; then
		echo -n "Fetching `wc -l < filelist | tr -d ' '` "
		echo ${NDEBUG} "files... "
		lam -s "${FETCHDIR}/f/" - -s ".gz" < filelist |
		    xargs ${XARGST} ${PHTTPGET} ${SERVERNAME}	\
		    2>${QUIETREDIR}

		while read Y; do
			if ! [ -f ${Y}.gz ]; then
				echo "failed."
				return 1
			fi
			if [ `gunzip -c < ${Y}.gz |
			    ${SHA256} -q` = ${Y} ]; then
				mv ${Y}.gz files/${Y}.gz
			else
				echo "${Y} has incorrect hash."
				return 1
			fi
		done < filelist
		echo "done."
	fi

	# Clean up
	rm files.wanted filelist patchlist
}

# Create and populate install manifest directory; and report what updates
# are available.
fetch_create_manifest () {
	# If we have an existing install manifest, nuke it.
	if [ -L "${BDHASH}-install" ]; then
		rm -r ${BDHASH}-install/
		rm ${BDHASH}-install
	fi

	# Report to the user if any updates were avoided due to local changes
	if [ -s modifiedfiles ]; then
		echo
		echo -n "The following files are affected by updates, "
		echo "but no changes have"
		echo -n "been downloaded because the files have been "
		echo "modified locally:"
		cat modifiedfiles
	fi
	rm modifiedfiles

	# If no files will be updated, tell the user and exit
	if ! [ -s INDEX-PRESENT ] &&
	    ! [ -s INDEX-NEW ]; then
		rm INDEX-PRESENT INDEX-NEW
		echo
		echo -n "No updates needed to update system to "
		echo "${RELNUM}-p${RELPATCHNUM}."
		return
	fi

	# Divide files into (a) removed files, (b) added files, and
	# (c) updated files.
	cut -f 1 -d '|' < INDEX-PRESENT |
	    sort > INDEX-PRESENT.flist
	cut -f 1 -d '|' < INDEX-NEW |
	    sort > INDEX-NEW.flist
	comm -23 INDEX-PRESENT.flist INDEX-NEW.flist > files.removed
	comm -13 INDEX-PRESENT.flist INDEX-NEW.flist > files.added
	comm -12 INDEX-PRESENT.flist INDEX-NEW.flist > files.updated
	rm INDEX-PRESENT.flist INDEX-NEW.flist

	# Report removed files, if any
	if [ -s files.removed ]; then
		echo
		echo -n "The following files will be removed "
		echo "as part of updating to ${RELNUM}-p${RELPATCHNUM}:"
		cat files.removed
	fi
	rm files.removed

	# Report added files, if any
	if [ -s files.added ]; then
		echo
		echo -n "The following files will be added "
		echo "as part of updating to ${RELNUM}-p${RELPATCHNUM}:"
		cat files.added
	fi
	rm files.added

	# Report updated files, if any
	if [ -s files.updated ]; then
		echo
		echo -n "The following files will be updated "
		echo "as part of updating to ${RELNUM}-p${RELPATCHNUM}:"

		cat files.updated
	fi
	rm files.updated

	# Create a directory for the install manifest.
	MDIR=`mktemp -d install.XXXXXX` || return 1

	# Populate it
	mv INDEX-PRESENT ${MDIR}/INDEX-OLD
	mv INDEX-NEW ${MDIR}/INDEX-NEW

	# Link it into place
	ln -s ${MDIR} ${BDHASH}-install
}

# Warn about any upcoming EoL
fetch_warn_eol () {
	# What's the current time?
	NOWTIME=`date "+%s"`

	# When did we last warn about the EoL date?
	if [ -f lasteolwarn ]; then
		LASTWARN=`cat lasteolwarn`
	else
		LASTWARN=`expr ${NOWTIME} - 63072000`
	fi

	# If the EoL time is past, warn.
	if [ ${EOLTIME} -lt ${NOWTIME} ]; then
		echo
		cat <<-EOF
		WARNING: `uname -sr` HAS PASSED ITS END-OF-LIFE DATE.
		Any security issues discovered after `date -r ${EOLTIME}`
		will not have been corrected.
		EOF
		return 1
	fi

	# Figure out how long it has been since we last warned about the
	# upcoming EoL, and how much longer we have left.
	SINCEWARN=`expr ${NOWTIME} - ${LASTWARN}`
	TIMELEFT=`expr ${EOLTIME} - ${NOWTIME}`

	# Don't warn if the EoL is more than 6 months away
	if [ ${TIMELEFT} -gt 15768000 ]; then
		return 0
	fi

	# Don't warn if the time remaining is more than 3 times the time
	# since the last warning.
	if [ ${TIMELEFT} -gt `expr ${SINCEWARN} \* 3` ]; then
		return 0
	fi

	# Figure out what time units to use.
	if [ ${TIMELEFT} -lt 604800 ]; then
		UNIT="day"
		SIZE=86400
	elif [ ${TIMELEFT} -lt 2678400 ]; then
		UNIT="week"
		SIZE=604800
	else
		UNIT="month"
		SIZE=2678400
	fi

	# Compute the right number of units
	NUM=`expr ${TIMELEFT} / ${SIZE}`
	if [ ${NUM} != 1 ]; then
		UNIT="${UNIT}s"
	fi

	# Print the warning
	echo
	cat <<-EOF
		WARNING: `uname -sr` is approaching its End-of-Life date.
		It is strongly recommended that you upgrade to a newer
		release within the next ${NUM} ${UNIT}.
	EOF

	# Update the stored time of last warning
	echo ${NOWTIME} > lasteolwarn
}

# Do the actual work involved in "fetch" / "cron".
fetch_run () {
	workdir_init || return 1

	# Prepare the mirror list.
	fetch_pick_server_init && fetch_pick_server

	# Try to fetch the public key until we run out of servers.
	while ! fetch_key; do
		fetch_pick_server || return 1
	done

	# Try to fetch the metadata index signature ("tag") until we run
	# out of available servers; and sanity check the downloaded tag.
	while ! fetch_tag; do
		fetch_pick_server || return 1
	done
	fetch_tagsanity || return 1

	# Fetch the latest INDEX-NEW and INDEX-OLD files.
	fetch_metadata INDEX-NEW INDEX-OLD || return 1

	# Generate filtered INDEX-NEW and INDEX-OLD files containing only
	# the lines which (a) belong to components we care about, and (b)
	# don't correspond to paths we're explicitly ignoring.
	fetch_filter_metadata INDEX-NEW || return 1
	fetch_filter_metadata INDEX-OLD || return 1

	# Translate /boot/`uname -i` into ${KERNELDIR}
	fetch_filter_kernel_names INDEX-NEW
	fetch_filter_kernel_names INDEX-OLD

	# For all paths appearing in INDEX-OLD or INDEX-NEW, inspect the
	# system and generate an INDEX-PRESENT file.
	fetch_inspect_system INDEX-OLD INDEX-PRESENT INDEX-NEW || return 1

	# Based on ${UPDATEIFUNMODIFIED}, remove lines from INDEX-* which
	# correspond to lines in INDEX-PRESENT with hashes not appearing
	# in INDEX-OLD or INDEX-NEW.  Also remove lines where the entry in
	# INDEX-PRESENT has type - and there isn't a corresponding entry in
	# INDEX-OLD with type -.
	fetch_filter_unmodified_notpresent INDEX-OLD INDEX-PRESENT INDEX-NEW

	# For each entry in INDEX-PRESENT of type -, remove any corresponding
	# entry from INDEX-NEW if ${ALLOWADD} != "yes".  Remove all entries
	# of type - from INDEX-PRESENT.
	fetch_filter_allowadd INDEX-PRESENT INDEX-NEW

	# If ${ALLOWDELETE} != "yes", then remove any entries from
	# INDEX-PRESENT which don't correspond to entries in INDEX-NEW.
	fetch_filter_allowdelete INDEX-PRESENT INDEX-NEW

	# If ${KEEPMODIFIEDMETADATA} == "yes", then for each entry in
	# INDEX-PRESENT with metadata not matching any entry in INDEX-OLD,
	# replace the corresponding line of INDEX-NEW with one having the
	# same metadata as the entry in INDEX-PRESENT.
	fetch_filter_modified_metadata INDEX-OLD INDEX-PRESENT INDEX-NEW

	# Remove lines from INDEX-PRESENT and INDEX-NEW which are identical;
	# no need to update a file if it isn't changing.
	fetch_filter_uptodate INDEX-PRESENT INDEX-NEW

	# Prepare to fetch files: Generate a list of the files we need,
	# copy the unmodified files we have into /files/, and generate
	# a list of patches to download.
	fetch_files_prepare INDEX-OLD INDEX-PRESENT INDEX-NEW

	# Fetch files.
	fetch_files || return 1

	# Create and populate install manifest directory; and report what
	# updates are available.
	fetch_create_manifest || return 1

	# Warn about any upcoming EoL
	fetch_warn_eol || return 1
}

# Make sure that all the file hashes mentioned in $@ have corresponding
# gzipped files stored in /files/.
install_verify () {
	# Generate a list of hashes
	cat $@ |
	    cut -f 2,7 -d '|' |
	    grep -E '^f' |
	    cut -f 2 -d '|' |
	    sort -u > filelist

	# Make sure all the hashes exist
	while read HASH; do
		if ! [ -f files/${HASH}.gz ]; then
			echo -n "Update files missing -- "
			echo "this should never happen."
			echo "Re-run '$0 fetch'."
			return 1
		fi
	done < filelist

	# Clean up
	rm filelist
}

# Remove the system immutable flag from files
install_unschg () {
	# Generate file list
	cat $@ |
	    cut -f 1 -d '|' > filelist

	# Remove flags
	while read F; do
		if ! [ -e ${F} ]; then
			continue
		fi

		chflags noschg ${F} || return 1
	done < filelist

	# Clean up
	rm filelist
}

# Install new files
install_from_index () {
	# First pass: Do everything apart from setting file flags.  We
	# can't set flags yet, because schg inhibits hard linking.
	sort -k 1,1 -t '|' $1 |
	    tr '|' ' ' |
	    while read FPATH TYPE OWNER GROUP PERM FLAGS HASH LINK; do
		case ${TYPE} in
		d)
			# Create a directory
			install -d -o ${OWNER} -g ${GROUP}		\
			    -m ${PERM} ${BASEDIR}/${FPATH}
			;;
		f)
			if [ -z "${LINK}" ]; then
				# Create a file, without setting flags.
				gunzip < files/${HASH}.gz > ${HASH}
				install -S -o ${OWNER} -g ${GROUP}	\
				    -m ${PERM} ${HASH} ${BASEDIR}/${FPATH}
				rm ${HASH}
			else
				# Create a hard link.
				ln -f ${LINK} ${BASEDIR}/${FPATH}
			fi
			;;
		L)
			# Create a symlink
			ln -sfh ${HASH} ${BASEDIR}/${FPATH}
			;;
		esac
	    done

	# Perform a second pass, adding file flags.
	tr '|' ' ' < $1 |
	    while read FPATH TYPE OWNER GROUP PERM FLAGS HASH LINK; do
		if [ ${TYPE} = "f" ] &&
		    ! [ ${FLAGS} = "0" ]; then
			chflags ${FLAGS} ${BASEDIR}/${FPATH}
		fi
	    done
}

# Remove files which we want to delete
install_delete () {
	# Generate list of new files
	cut -f 1 -d '|' < $2 |
	    sort > newfiles

	# Generate subindex of old files we want to nuke
	sort -k 1,1 -t '|' $1 |
	    join -t '|' -v 1 - newfiles |
	    sort -r -k 1,1 -t '|' |
	    cut -f 1,2 -d '|' |
	    tr '|' ' ' > killfiles

	# Remove the offending bits
	while read FPATH TYPE; do
		case ${TYPE} in
		d)
			rmdir ${BASEDIR}/${FPATH}
			;;
		f)
			rm ${BASEDIR}/${FPATH}
			;;
		L)
			rm ${BASEDIR}/${FPATH}
			;;
		esac
	done < killfiles

	# Clean up
	rm newfiles killfiles
}

# Update linker.hints if anything in /boot/ was touched
install_kldxref () {
	if cat $@ |
	    grep -qE '^/boot/'; then
		kldxref -R /boot/
	fi
}

# Rearrange bits to allow the installed updates to be rolled back
install_setup_rollback () {
	if [ -L ${BDHASH}-rollback ]; then
		mv ${BDHASH}-rollback ${BDHASH}-install/rollback
	fi

	mv ${BDHASH}-install ${BDHASH}-rollback
}

# Actually install updates
install_run () {
	echo -n "Installing updates..."

	# Make sure we have all the files we should have
	install_verify ${BDHASH}-install/INDEX-OLD	\
	    ${BDHASH}-install/INDEX-NEW || return 1

	# Remove system immutable flag from files
	install_unschg ${BDHASH}-install/INDEX-OLD	\
	    ${BDHASH}-install/INDEX-NEW || return 1

	# Install new files
	install_from_index				\
	    ${BDHASH}-install/INDEX-NEW || return 1

	# Remove files which we want to delete
	install_delete ${BDHASH}-install/INDEX-OLD	\
	    ${BDHASH}-install/INDEX-NEW || return 1

	# Update linker.hints if anything in /boot/ was touched
	install_kldxref ${BDHASH}-install/INDEX-OLD	\
	    ${BDHASH}-install/INDEX-NEW

	# Rearrange bits to allow the installed updates to be rolled back
	install_setup_rollback

	echo " done."
}

# Rearrange bits to allow the previous set of updates to be rolled back next.
rollback_setup_rollback () {
	if [ -L ${BDHASH}-rollback/rollback ]; then
		mv ${BDHASH}-rollback/rollback rollback-tmp
		rm -r ${BDHASH}-rollback/
		rm ${BDHASH}-rollback
		mv rollback-tmp ${BDHASH}-rollback
	else
		rm -r ${BDHASH}-rollback/
		rm ${BDHASH}-rollback
	fi
}

# Actually rollback updates
rollback_run () {
	echo -n "Uninstalling updates..."

	# If there are updates waiting to be installed, remove them; we
	# want the user to re-run 'fetch' after rolling back updates.
	if [ -L ${BDHASH}-install ]; then
		rm -r ${BDHASH}-install/
		rm ${BDHASH}-install
	fi

	# Make sure we have all the files we should have
	install_verify ${BDHASH}-rollback/INDEX-NEW	\
	    ${BDHASH}-rollback/INDEX-OLD || return 1

	# Remove system immutable flag from files
	install_unschg ${BDHASH}-rollback/INDEX-NEW	\
	    ${BDHASH}-rollback/INDEX-OLD || return 1

	# Install new files
	install_from_index				\
	    ${BDHASH}-rollback/INDEX-OLD || return 1

	# Remove files which we want to delete
	install_delete ${BDHASH}-rollback/INDEX-NEW	\
	    ${BDHASH}-rollback/INDEX-OLD || return 1

	# Update linker.hints if anything in /boot/ was touched
	install_kldxref ${BDHASH}-rollback/INDEX-NEW	\
	    ${BDHASH}-rollback/INDEX-OLD

	# Remove the rollback directory and the symlink pointing to it; and
	# rearrange bits to allow the previous set of updates to be rolled
	# back next.
	rollback_setup_rollback

	echo " done."
}

#### Main functions -- call parameter-handling and core functions

# Using the command line, configuration file, and defaults,
# set all the parameters which are needed later.
get_params () {
	init_params
	parse_cmdline $@
	parse_conffile
	default_params
}

# Fetch command.  Make sure that we're being called
# interactively, then run fetch_check_params and fetch_run
cmd_fetch () {
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
cmd_cron () {
	fetch_check_params
	sleep `jot -r 1 0 3600`

	TMPFILE=`mktemp /tmp/freebsd-update.XXXXXX` || exit 1
	if ! fetch_run >> ${TMPFILE} ||
	    ! grep -q "No updates needed" ${TMPFILE} ||
	    [ ${VERBOSELEVEL} = "debug" ]; then
		mail -s "`hostname` security updates" ${MAILTO} < ${TMPFILE}
	fi

	rm ${TMPFILE}
}

# Install downloaded updates.
cmd_install () {
	install_check_params
	install_run || exit 1
}

# Rollback most recently installed updates.
cmd_rollback () {
	rollback_check_params
	rollback_run || exit 1
}

#### Entry point

# Make sure we find utilities from the base system
export PATH=/sbin:/bin:/usr/sbin:/usr/bin:${PATH}

# Set LC_ALL in order to avoid problems with character ranges like [A-Z].
export LC_ALL=C

get_params $@
for COMMAND in ${COMMANDS}; do
	cmd_${COMMAND}
done
