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
# $Id: bininst.sh,v 1.8 1995/02/02 23:39:44 jkh Exp $

# Grab the miscellaneous functions.
. /stand/scripts/miscfuncs.sh

# Grab the installation routines
. /stand/scripts/instdist.sh

# Grab the network setup routines
. /stand/scripts/netinst.sh

# Grab the setup script
. /stand/scripts/setup.sh

# Deal with trigger-happy users.
trap interrupt 1 2 15

# set initial defaults
set_defaults()
{
	network_set_defaults
	media_set_defaults
	mkdir -p ${TMP}
	cp /stand/etc/* /etc
}

# Print welcome banner.
welcome()
{
}

goodbye()
{
	dialog --title "Auf Wiedersehen!" --msgbox \
"Don't forget that the login name \"root\" has no password.
If you didn't create any users with adduser, you can at least log in
as this user.  Also be aware that root is the _superuser_, which means
that you can easily wipe out your system with it if you're not careful!

Further information may be obtained by sending mail to
questions@freebsd.org (though please read the docs first,
we get LOTS of questions! :-) or browsing through our
WEB site:  http://www.freebsd.org/

If you encounter a bug and can send/receive Internet email, please
use the \`send-pr\' command to submit a report - this will ensure
that the bug is noted and tracked to some sort of resolution.

Enjoy FreeBSD 2.0!

		The FreeBSD Project Team" -1 -1
}

welcome
set_defaults

if media_select_distribution; then
	if media_chose_method; then
		for xx in ${MEDIA_DISTRIBUTIONS}; do
			MEDIA_DISTRIBUTION=`eval echo \`echo $xx\``
			media_install_set
		done
	fi
	final_configuration
	goodbye
fi

echo; echo "Spawning shell.  Exit shell to continue with new system."
echo "Progress <installation completed>" > /dev/ttyv1
/stand/sh
exit 20
