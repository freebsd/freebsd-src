#!/stand/sh
#
# bininst - perform the last stage of installation by somehow getting
# a bindist onto the user's disk and unpacking it.  The name bininst
# is actually something of a misnomer, since this utility will install
# more than just the bindist set.
#
# Written:  November 11th, 1994
# Copyright (C) 1994 by Jordan K. Hubbard
#
# Permission to copy or use this software for any purpose is granted
# provided that this message stay intact, and at this location (e.g. no
# putting your name on top after doing something trivial like reindenting
# it, just to make it look like you wrote it!).
#
# $Id: bininst.sh,v 1.1 1995/01/14 10:28:20 jkh Exp $

if [ "${_SETUP_LOADED_}" = "yes" ]; then
        error "Error, $0 loaded more than once!"
        return 1
else
        _SETUP_LOADED_=yes
fi

# Grab the miscellaneous functions.
. /stand/miscfuncs.sh

setup()
{
	DONE=""
	while [ "${DONE}" = "" ]; do
	dialog --title "Configuration Menu" --menu \
"Configure your system for basic single user, network or\n\
development workstation usage.  Please select one of the
following options.  When you are finished setting up your\n\
system, select \"done\".  To invoke this configuration tool \n\
again, type \`/stand/setup.sh\'." -1 -1 5 \
"help" "Help!  What does all this mean?" \
"tzsetup" "Configure your system's time zone" \
"network" "Configure basic networking parameters" \
"user" "Add a user name for yourself to the system" \
"guest" "Add a default user \"guest\" \
"packages" "Install additional optional software on your system." \
"ports" "Enable use of the ports collection from CD or fileserver." \
"done" "Exit from setup." 2> ${TMP}/menu.tmp.$$
	RETVAL=$?
	CHOICE=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval ${RETVAL}; then exit 0; fi

	case ${CHOICE} in
	tzsetup)
		dialog --clear
		sh /stand/tzsetup
		dialog --clear
	;;

	network)
		network_setup
	;;

	user)
		sh /stand/adduser.sh -i
	;;

	guest)
		sh /stand/adduser.sh
	;;

	done)
		DONE="yes"
	;;

	*)
		not_supported	
	esac
	done
}
