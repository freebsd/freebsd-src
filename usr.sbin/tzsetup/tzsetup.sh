#!/bin/sh
# $Id$
#
# Copyright 1994, Garrett A. Wollman.  All rights reserved.
# This script is subject to the terms and conditions listed at the
# end.
#

ask() {
	while true; do
		echo -n "$1" "(${2-no default}) "
		read ans
		if [ -z "$ans" ]; then
			ans="$2"
		fi
		if [ -z "$ans" ]; then
			echo 'An empty response is not valid.'
			continue
		fi
		return 0
	done
}

askyn() {
	while : ; do
		ask "$1 [yn]?" $2
		case $ans in
			[Yy]*) return 0;;
			[Nn]*) return 1;;
		esac
	done
}

select() {
	where=$1
	while true; do
		echo "Please select a location from the following list which"
		echo "has the same legal time as your location:"
		echo ""
		ls -C /usr/share/zoneinfo/$where
		echo ""
		echo "Type \`\`exit'' to return to the main menu."
		ask "Your choice:" exit
		if [ "$ans" = "exit" ]; then return 0; fi
		city="$ans"
		if [ -r /usr/share/zoneinfo/$where/$city ]; then
			echo -n "I think the time in `echo $city | tr _ ' '` is: "
			TZ=$where/$city date
			if askyn "Is this what you wanted" y; then
				cp /usr/share/zoneinfo/$where/$city \
				   /etc/localtime && echo "Timezone changed."
				return 0
			fi
		fi
	done
}

setadjkerntz() {
	cat << EOF

You can configure your system to assume that the battery-backed CMOS
clock is set to your local legal time rather than Universal or
Greenwich time.  When the system boots, the \`adjkerntz' program will
examine your current time, reverse-apply the timezone correction, and
then set the system's UTC time to the corrected value.  This approach
is NOT guaranteed to always work; in particular, if you reboot your
system during the transition from or to summer time, the calculation
is sometimes impossible, and wierd things may result.

For this reason, we recommend that, unless you absolutely positively
must leave your CMOS clock on local time, you set your CMOS clock to GMT.

EOF
	if [ -f /etc/wall_cmos_clock ]; then
		curr=y
		echo "This system is currently set up for local CMOS time."
	else
		curr=n
		echo "This system is currently set up for GMT CMOS time."
	fi
	if askyn "Do you want a local CMOS clock" $curr; then
		touch /etc/wall_cmos_clock
		if [ $curr = "n" ] && askyn "Start now" y; then
			adjkerntz -i
		fi
	else
		rm -f /etc/wall_cmos_clock
	fi
	echo "Done."
}

mainmenu() {
	set `TZ= date`
	if askyn "Is $4, $1 $2 $3 $6 the correct current date and time" n; then
		cat << EOF

Unless your local time is GMT, this probably means that your CMOS
clock is on local time.  In this case, the local times that you will
be shown will be incorrect.  You should either set your CMOS clock to
GMT, or select option 98 from the main menu after selecting a
timezone.

EOF
		echo -n "Press return to continue ==>"
		read junk
	fi

	while true; do
		cat <<EOH


	Pick the item that best describes your location:

	 1) Africa
	 2) North, South, Central America (includes Greenland)
	 3) Atlantic Ocean islands
	 4) Asia and the Middle East
	 5) Australia
	 6) Europe
	 7) Indian Ocean islands
	 8) Pacific islands (includes Hawaii)

	98) Select CMOS time status (UTC or local)
	99) Exit this menu

EOH
		ask 'Your choice?' 99
		case $ans in
			 1) select Africa;;
			 2) select America;;
			 3) select Atlantic;;
			 4) select Asia;;
			 5) select Australia;;
			 6) select Europe;;
			 7) select Indian;;
			 8) select Pacific;;
			 9) select Etc;;
			10) select SystemV;;
			98) setadjkerntz;;
			99) return 0;;
		esac
	done
}

cat <<EOH
Welcome to `sysctl -n kern.ostype` `sysctl -n kern.osrelease`!

This program will help you select a default timezone for your users and
for system processes.

EOH
echo -n 'Press return to continue ==>'
read return

mainmenu
exit 0
#
# Copyright (c) 1994 Garrett A. Wollman.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
