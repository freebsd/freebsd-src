#!/bin/csh
#
# Script to summarize loopstats files
#
set x = `ls loopstats.*`
foreach dayfile ( $x )
        if ($dayfile == $x[$#x]) continue
        echo " "
        echo $dayfile
	awk -f loop.awk $dayfile
        rm -f $dayfile
end

