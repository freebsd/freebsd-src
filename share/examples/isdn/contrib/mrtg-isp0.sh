#!/bin/sh
#---------------------------------------------------------------------------
#
#	collect callout and callback statistics for the isp0 i/f
#
#	enable budget callout and callback restrictions and file
#	rotation in the isdnd.rc file.
#
#	last edit-date: [Fri May 25 15:22:45 2001]
#
# $FreeBSD$
#
#---------------------------------------------------------------------------
#
#---------------------------------------------------------------------------
# this is the entry for mrtg in mrtg.cfg
#---------------------------------------------------------------------------
# Target[ernie.cstat.isp0]: `/usr/local/etc/mrtg/util/mrtg-isp0.sh`
# MaxBytes[ernie.cstat.isp0]: 10
# AbsMax[ernie.cstat.isp0]: 200
# Title[ernie.cstat.isp0]: isp0: callouts / callbacks
# PageTop[ernie.cstat.isp0]: <H1> isp0: callouts /callbacks </H1>
# Options[ernie.cstat.isp0]: gauge, nopercent, integer
# YLegend[ernie.cstat.isp0]: co / cb
# ShortLegend[ernie.cstat.isp0]: n
# Legend1[ernie.cstat.isp0]: callouts
# Legend2[ernie.cstat.isp0]: callbacks
# LegendI[ernie.cstat.isp0]: callouts:
# LegendO[ernie.cstat.isp0]: callbacks:
# WithPeak[ernie.cstat.isp0]: ymwd
#
#---------------------------------------------------------------------------
#	this is the shell script run by mrtg
#---------------------------------------------------------------------------
if [ -r /var/log/isdn/callouts.isp0 ]
then
        cat /var/log/isdn/callouts.isp0 | awk '{print $3}'
else
        echo 0
fi

if [ -r /var/log/isdn/callbacks.isp0 ]
then
        cat /var/log/isdn/callbacks.isp0 | awk '{print $3}'
else
        echo 0
fi

uptime | cut -c 12-18
uname -nsr

exit 0

