#!/bin/csh
#
# Script to summarize clockstats files
#
set x = `ls clockstats.*`
foreach dayfile ( $x )
	if ($dayfile == $x[$#x]) continue
	echo " "
	echo $dayfile
	awk -f clock.awk $dayfile
	awk -f itf.awk $dayfile >>itf
	awk -f etf.awk $dayfile >>etf
	awk -f ensemble.awk $dayfile >>ensemble
	awk -f tdata.awk $dayfile >>tdata
	rm -f $dayfile
end

