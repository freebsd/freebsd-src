#!/bin/csh
#
# Script to summarize peerstats files
#
set x = `ls peerstats.*`
foreach dayfile ( $x )
        if ($dayfile == $x[$#x]) continue
        echo " "
        echo $dayfile
	awk -f peer.awk $dayfile
        rm -f $dayfile
end

