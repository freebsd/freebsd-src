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
# $Id$

if [ "$_MISCFUNCS_SH_LOADED_" = "yes" ]; then
	return 0
else
	_MISCFUNCS_SH_LOADED_=yes
fi

PATH=/usr/bin:/usr/sbin:/bin:/sbin:/stand
export PATH
DISTNAME=2.0-ALPHA

interrupt() {
	if dialog --clear --title "User Interrupt Requested" \
	  --yesno "Do you wish to abort the installation?" 5 70; then
		exit 0;
	fi
}

# Handle the return value from a dialog, doing some pre-processing
# so that each client doesn't have to.
handle_rval() {
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

# A simple user-confirmation dialog.
confirm() {
	dialog --title "User Confirmation" --msgbox "$*" 8 72
}

# A simple message box dialog.
message() {
	dialog --title "Progress" --infobox "$*" 5 72
}

# A simple error dialog.
error() {
	dialog --title "Error!" --msgbox "$*" 10 72
}

# Something isn't supported yet! :-(
not_supported() {
	dialog --title "Sorry!" \
	--msgbox "This feature is not supported in the current version of the \
installation tools.  Barring some sort of fatal accident, we do \
expect it to be in the release.  Please press RETURN to go on." 10 60
}

# Get a string from the user
input()
{
	dialog --title "$title" $clear \
	--inputbox "$*" 17 72 "$default_value" 2> ${TMP}/inputbox.tmp.$$
	if ! handle_rval $?; then rm -f ${TMP}/inputbox.tmp.$$; return 1; fi
	answer=`cat ${TMP}/inputbox.tmp.$$`
	rm -f ${TMP}/inputbox.tmp.$$
}

# Ask a networking question
network_dialog()
{
	title="Network Configuration"
	if ! input "$*"; then return 1; fi
}
