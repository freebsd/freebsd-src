#!/bin/csh
#
# Script to summarize ipeerstats, loopstats and clockstats files
#
# This script can be run from a cron job once per day, week or month. It
# runs the file-specific summary script and appends the summary data to 
# designated files, which must be created first.
#
if ( -e peer_summary ) then
	peer.sh >>peer_summary
endif
if ( -e loop_summary ) then
	loop.sh >>loop_summary
endif
if ( -e clock_summary ) then
	clock.sh >>clock_summary
endif
