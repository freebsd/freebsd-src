#!/bin/sh
# Copyright (c) April 1997 Wolfram Schneider <wosch@FreeBSD.org>, Berlin.
#
# portsinfo - Generate list of new ports for last two weeks.
#
# $Id$

url=http://www.de.freebsd.org/de/cgi/ports.cgi
time='?type=new&time=2+week+ago&sektion=all'

lynx -nolist -dump "$url$time" | grep -v "Description _:_" |
perl -ne 's/_$//; s/:_ /: /; s/^(\s+)_/$1/; print if (/Main/ .. /____/)' |
perl -ne 'if (/Main Category/) { 
		print; for(1..50) {print "="}; print "\n";
          } else { print}'

echo "This information was produced at `date -u +'%Y/%m/%d %H:%M UTC'` by"
echo "$url"



