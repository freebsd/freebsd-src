#!/bin/sh
# Copyright (c) April 1997 Wolfram Schneider <wosch@FreeBSD.org>, Berlin.
#
# portsinfo - Generate list of new ports for last two weeks.
#
# $Id: portsinfo,v 1.3 1997/05/28 19:51:20 wosch Exp wosch $

PATH=/bin:/usr/bin:/usr/local/bin:$PATH; export PATH

url=http://www.de.freebsd.org/de/cgi/ports.cgi
time='?type=new&time=2+week+ago&sektion=all'

lynx -nolist -dump -reload -nostatus -underscore "$url$time" | 
    grep -v "Description :" |
perl -ne 'print if (/Main/ .. /XX%MXX/)' |
perl -ne 'if (/Main Category/) { 
		print; for(1..50) {print "="}; print "\n";
          } else { print}'

echo ""
echo "This information was produced by $url"
