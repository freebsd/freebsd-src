#!/bin/csh
#
# Script to summarize ipeerstats, loopstats and clockstats files
#
# This script can be run from a cron job once per day, week or month. It
# runs the file-specific summary script and appends the summary data to 
# designated files.
#
peer.sh >>peer_summary
loop.sh >>loop_summary
clock.sh >>clock_summary

