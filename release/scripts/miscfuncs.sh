#!/stand/sh
#
# miscfuncs - miscellaneous functions for the other distribution scripts.
#
# Written:  November 15th, 1994
# Copyright (C) 1994 by Jordan K. Hubbard
#
# Permission to copy or use this software for any purpose is granted
# provided that this message stay intact, and at this location (e.g. no
# putting your name on top after doing something trivial like reindenting
# it, just to make it look like you wrote it!).
#
# $Id: miscfuncs.sh,v 1.4 1995/01/29 01:32:32 jkh Exp $

if [ "${_MISCFUNCS_SH_LOADED_}" = "yes" ]; then
	return 0
else
	_MISCFUNCS_SH_LOADED_=yes
fi

PATH=/usr/bin:/usr/sbin:/bin:/sbin:/stand
export PATH

# Keep this current with the distribution!
DISTNAME="2.0-950201-SNAP"

# Express or Custom install?
INSTALL_TYPE=""

# Flagrant guesses for now.  These need to be hand-edited or, much better yet,
# automatically done as part of the release process.  When that's the case,
# the hardwired constants will be replaced with tokens that get sed'd for
# the real sizes.
#
BINSIZE="52MB"
GAMESIZE="12MB"
MANSIZE="8MB"
INFOSIZE="4MB"
PROFSIZE="4MB"
DICTSIZE="6MB"
SRCSIZE="112MB"
SECRSIZE="2MB"
COMPATSIZE="5MB"
X11SIZE="50MB"

# Paths
ETC="/etc"
MNT="/mnt"
HOME=/; export HOME
TMP=/tmp

# Commands and flags
FT_CMD="ft"
TAR_CMD="tar"
TAR_FLAGS="--unlink -xvf"
IFCONFIG_CMD="ifconfig"
ROUTE_CMD="route"
ROUTE_FLAGS="add default"
HOSTNAME_CMD="hostname"
SLATTACH_CMD="slattach"
SLATTACH_FLAGS="-l -a -s"
PPPD_CMD="pppd"
PPPD_FLAGS="crtscts defaultroute -ip -mn netmask $netmask"

interrupt()
{
	dialog --clear --title "User Interrupt Requested" \
	  --msgbox "\n ** Aborting the installation ** \n" -1 -1
	exit 0;
}

# Handle the return value from a dialog, doing some pre-processing
# so that each client doesn't have to.
handle_rval()
{
	case $1 in
	0)
		return 0
	;;
	255)
		PS1="subshell# " /stand/sh
	;;
	*)
		return 1
	;;
	esac
}

# stick a progress message out on the other vty
progress()
{
	echo "Progress <$*>" > /dev/ttyv1
}

# A simple user-confirmation dialog.
confirm()
{
	dialog --title "User Confirmation" --msgbox "$*" -1 -1
}

# A simple message box dialog.
message()
{
	progress $*
	dialog --title "Progress" --infobox "$*" -1 -1
}

# A simple error dialog.
error()
{
	echo "ERROR <$*>" > /dev/ttyv1
	dialog --title "Error!" --msgbox "$*" -1 -1
}

# Something isn't supported yet! :-(
not_supported()
{
	echo "<Feature not supported>" > /dev/ttyv1
	dialog --title "Sorry!" --msgbox \
"This feature is not supported in the current version of the
installation tools.  Barring some sort of fatal accident, we do
expect it to be in a later release.  Please press RETURN to go on." -1 -1
}

# Get a string from the user
input()
{
	TITLE=${TITLE-"User Input Required"}
	dialog --title "${TITLE}" \
	  --inputbox "$*" -1 -1 "${DEFAULT_VALUE}" 2> ${TMP}/inputbox.tmp.$$
	if ! handle_rval $?; then rm -f ${TMP}/inputbox.tmp.$$; return 1; fi
	ANSWER=`cat ${TMP}/inputbox.tmp.$$`
	rm -f ${TMP}/inputbox.tmp.$$
}

# Ask a networking question
network_dialog()
{
	TITLE="Network Configuration"
	if ! input "$*"; then return 1; fi
}
