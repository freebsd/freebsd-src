#! xCSH_PATHx -f
#
# Sccs2rcs is a script to convert an existing SCCS
# history into an RCS history without losing any of
# the information contained therein.
# It has been tested under the following OS's:
#     SunOS 3.5, 4.0.3, 4.1
#     Ultrix-32 2.0, 3.1
#
# Things to note:
#   + It will NOT delete or alter your ./SCCS history under any circumstances.
#
#   + Run in a directory where ./SCCS exists and where you can
#       create ./RCS
#
#   + /usr/local/bin is put in front of the default path.
#     (SCCS under Ultrix is set-uid sccs, bad bad bad, so
#     /usr/local/bin/sccs here fixes that)
#
#   + Date, time, author, comments, branches, are all preserved.
#
#   + If a command fails somewhere in the middle, it bombs with
#     a message -- remove what it's done so far and try again.
#         "rm -rf RCS; sccs unedit `sccs tell`; sccs clean"
#     There is no recovery and exit is far from graceful.
#     If a particular module is hanging you up, consider
#     doing it separately; move it from the current area so that
#     the next run will have a better chance or working.
#     Also (for the brave only) you might consider hacking
#     the s-file for simpler problems:  I've successfully changed
#     the date of a delta to be in sync, then run "sccs admin -z"
#     on the thing.
#
#   + After everything finishes, ./SCCS will be moved to ./old-SCCS.
#
# This file may be copied, processed, hacked, mutilated, and
# even destroyed as long as you don't tell anyone you wrote it.
#
# Ken Cox
# Viewlogic Systems, Inc.
# kenstir@viewlogic.com
# ...!harvard!cg-atla!viewlog!kenstir
#
# Various hacks made by Brian Berliner before inclusion in CVS contrib area.
# $FreeBSD$


#we'll assume the user set up the path correctly
# for the Pmax, /usr/ucb/sccs is suid sccs, what a pain
#   /usr/local/bin/sccs should override /usr/ucb/sccs there
set path = (/usr/local/bin $path)


############################################################
# Error checking
#
if (! -w .) then
    echo "Error: ./ not writeable by you."
    exit 1
endif
if (! -d SCCS) then
    echo "Error: ./SCCS directory not found."
    exit 1
endif
set edits = (`sccs tell`)
if ($#edits) then
    echo "Error: $#edits file(s) out for edit...clean up before converting."
    exit 1
endif
if (-d RCS) then
    echo "Warning: RCS directory exists"
    if (`ls -a RCS | wc -l` > 2) then
        echo "Error: RCS directory not empty"
        exit 1
    endif
else
    mkdir RCS
endif

sccs clean

set logfile = /tmp/sccs2rcs_$$_log
rm -f $logfile
set tmpfile = /tmp/sccs2rcs_$$_tmp
rm -f $tmpfile
set emptyfile = /tmp/sccs2rcs_$$_empty
echo -n "" > $emptyfile
set initialfile = /tmp/sccs2rcs_$$_init
echo "Initial revision" > $initialfile
set sedfile = /tmp/sccs2rcs_$$_sed
rm -f $sedfile
set revfile = /tmp/sccs2rcs_$$_rev
rm -f $revfile

# the quotes surround the dollar signs to fool RCS when I check in this script
set sccs_keywords = (\
    '%W%[ 	]*%G%'\
    '%W%[ 	]*%E%'\
    '%W%'\
    '%Z%%M%[ 	]*%I%[ 	]*%G%'\
    '%Z%%M%[ 	]*%I%[ 	]*%E%'\
    '%M%[ 	]*%I%[ 	]*%G%'\
    '%M%[ 	]*%I%[ 	]*%E%'\
    '%M%'\
    '%I%'\
    '%G%'\
    '%E%'\
    '%U%')
set rcs_keywords = (\
    '$'Id'$'\
    '$'Id'$'\
    '$'Id'$'\
    '$'SunId'$'\
    '$'SunId'$'\
    '$'Id'$'\
    '$'Id'$'\
    '$'RCSfile'$'\
    '$'Revision'$'\
    '$'Date'$'\
    '$'Date'$'\
    '')


############################################################
# Get some answers from user
#
echo ""
echo "Do you want to be prompted for a description of each"
echo "file as it is checked in to RCS initially?"
echo -n "(y=prompt for description, n=null description) [y] ?"
set ans = $<
if ((_$ans == _) || (_$ans == _y) || (_$ans == _Y)) then
    set nodesc = 0
else
    set nodesc = 1
endif
echo ""
echo "The default keyword substitutions are as follows and are"
echo "applied in the order specified:"
set i = 1
while ($i <= $#sccs_keywords)
#    echo '	'\"$sccs_keywords[$i]\"'	==>	'\"$rcs_keywords[$i]\"
    echo "	$sccs_keywords[$i]	==>	$rcs_keywords[$i]"
    @ i = $i + 1
end
echo ""
echo -n "Do you want to change them [n] ?"
set ans = $<
if ((_$ans != _) && (_$ans != _n) && (_$ans != _N)) then
    echo "You can't always get what you want."
    echo "Edit this script file and change the variables:"
    echo '    $sccs_keywords'
    echo '    $rcs_keywords'
else
    echo "good idea."
endif

# create the sed script
set i = 1
while ($i <= $#sccs_keywords)
    echo "s,$sccs_keywords[$i],$rcs_keywords[$i],g" >> $sedfile
    @ i = $i + 1
end

onintr ERROR

############################################################
# Loop over every s-file in SCCS dir
#
foreach sfile (SCCS/s.*)
    # get rid of the "s." at the beginning of the name
    set file = `echo $sfile:t | sed -e "s/^..//"`

    # work on each rev of that file in ascending order
    set firsttime = 1
    sccs prs $file | grep "^D " | awk '{print $2}' | sed -e 's/\./ /g' | sort -n -u +0 +1 +2 +3 +4 +5 +6 +7 +8 | sed -e 's/ /./g' > $revfile
    foreach rev (`cat $revfile`)
        if ($status != 0) goto ERROR

        # get file into current dir and get stats
        set date = `sccs prs -r$rev $file | grep "^D " | awk '{y=$3; if($3 < 100) {y+=1900};printf("%s %s", y, $4);exit;}'`
        set author = `sccs prs -r$rev $file | grep "^D " | awk '{print $5; exit}'`
        echo ""
        echo "==> file $file, rev=$rev, date=$date, author=$author"
        sccs edit -r$rev $file >>& $logfile
        if ($status != 0) goto ERROR
        echo checked out of SCCS

        # add RCS keywords in place of SCCS keywords
        sed -f $sedfile $file > $tmpfile
        if ($status != 0) goto ERROR
        echo performed keyword substitutions
        cp $tmpfile $file

        # check file into RCS
        if ($firsttime) then
            set firsttime = 0
            if ($nodesc) then
		echo about to do ci
                echo ci -f -r$rev -d"$date" -w$author -t$emptyfile $file 
                ci -f -r$rev -d"$date" -w$author -t$emptyfile $file < $initialfile >>& $logfile
                if ($status != 0) goto ERROR
                echo initial rev checked into RCS without description
            else
                echo ""
                echo Enter a brief description of the file $file \(end w/ Ctrl-D\):
                cat > $tmpfile
                ci -f -r$rev -d"$date" -w$author -t$tmpfile $file < $initialfile >>& $logfile
                if ($status != 0) goto ERROR
                echo initial rev checked into RCS
            endif
        else
            # get RCS lock
	    set lckrev = `echo $rev | sed -e 's/\.[0-9]*$//'`
	    if ("$lckrev" =~ [0-9]*.*) then
		# need to lock the brach -- it is OK if the lock fails
		rcs -l$lckrev $file >>& $logfile
	    else
		# need to lock the trunk -- must succeed
                rcs -l $file >>& $logfile
                if ($status != 0) goto ERROR
	    endif
            echo got lock
            sccs prs -r$rev $file | grep "." > $tmpfile
            # it's OK if grep fails here and gives status == 1
            # put the delta message in $tmpfile
            ed $tmpfile >>& $logfile <<EOF
/COMMENTS
1,.d
w
q
EOF
            ci -f -r$rev -d"$date" -w$author $file < $tmpfile >>& $logfile
            if ($status != 0) goto ERROR
            echo checked into RCS
        endif
        sccs unedit $file >>& $logfile
        if ($status != 0) goto ERROR
    end
    rm -f $file
end


############################################################
# Clean up
#
echo cleaning up...
mv SCCS old-SCCS
rm -f $tmpfile $emptyfile $initialfile $sedfile
echo ===================================================
echo "       Conversion Completed Successfully"
echo ""
echo "         SCCS history now in old-SCCS/"
echo ===================================================
set exitval = 0
goto cleanup

ERROR:
foreach f (`sccs tell`)
    sccs unedit $f
end
echo ""
echo ""
echo Danger\!  Danger\!
echo Some command exited with a non-zero exit status.
echo Log file exists in $logfile.
echo ""
echo Incomplete history in ./RCS -- remove it
echo Original unchanged history in ./SCCS
set exitval = 1

cleanup:
# leave log file
rm -f $tmpfile $emptyfile $initialfile $sedfile $revfile

exit $exitval
