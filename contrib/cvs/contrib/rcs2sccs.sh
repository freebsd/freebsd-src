#! /bin/sh
#
#

############################################################
# Error checking
#
if [ ! -d SCCS ] ; then
    mkdir SCCS
fi

logfile=/tmp/rcs2sccs_$$_log
rm -f $logfile
tmpfile=/tmp/rcs2sccs_$$_tmp
rm -f $tmpfile
emptyfile=/tmp/rcs2sccs_$$_empty
echo -n "" > $emptyfile
initialfile=/tmp/rcs2sccs_$$_init
echo "Initial revision" > $initialfile
sedfile=/tmp/rcs2sccs_$$_sed
rm -f $sedfile
revfile=/tmp/rcs2sccs_$$_rev
rm -f $revfile
commentfile=/tmp/rcs2sccs_$$_comment
rm -f $commentfile

# create the sed script
cat > $sedfile << EOF
s,;Id;,%Z%%M% %I% %E%,g
s,;SunId;,%Z%%M% %I% %E%,g
s,;RCSfile;,%M%,g
s,;Revision;,%I%,g
s,;Date;,%E%,g
s,;Id:.*;,%Z%%M% %I% %E%,g
s,;SunId:.*;,%Z%%M% %I% %E%,g
s,;RCSfile:.*;,%M%,g
s,;Revision:.*;,%I%,g
s,;Date:.*;,%E%,g
EOF
sed -e 's/;/\\$/g' $sedfile > $tmpfile
cp $tmpfile $sedfile
############################################################
# Loop over every RCS file in RCS dir
#
for vfile in *,v; do
    # get rid of the ",v" at the end of the name
    file=`echo $vfile | sed -e 's/,v$//'`

    # work on each rev of that file in ascending order
    firsttime=1
    rlog $file | grep "^revision [0-9][0-9]*\." | awk '{print $2}' | sed -e 's/\./ /g' | sort -n -u +0 +1 +2 +3 +4 +5 +6 +7 +8 | sed -e 's/ /./g' > $revfile
    for rev in `cat $revfile`; do
        if [ $? != 0 ]; then
		echo ERROR - revision
		exit
	fi
        # get file into current dir and get stats
        date=`rlog -r$rev $file | grep "^date: " | awk '{print $2; exit}' | sed -e 's/^19\|^20//'`
        time=`rlog -r$rev $file | grep "^date: " | awk '{print $3; exit}' | sed -e 's/;//'`
        author=`rlog -r$rev $file | grep "^date: " | awk '{print $5; exit}' | sed -e 's/;//'`
	date="$date $time"
        echo ""
	rlog -r$rev $file | sed -e '/^branches: /d' -e '1,/^date: /d' -e '/^===========/d' -e 's/$/\\/' | awk '{if ((total += length($0) + 1) < 510) print $0}' > $commentfile
        echo "==> file $file, rev=$rev, date=$date, author=$author"
	rm -f $file
        co -r$rev $file >> $logfile  2>&1
        if [ $? != 0 ]; then
		echo ERROR - co
		exit
	fi
        echo checked out of RCS

        # add SCCS keywords in place of RCS keywords
        sed -f $sedfile $file > $tmpfile
        if [ $? != 0 ]; then
		echo ERROR - sed
		exit
	fi
        echo performed keyword substitutions
	rm -f $file
        cp $tmpfile $file

        # check file into SCCS
        if [ "$firsttime" = "1" ]; then
            firsttime=0
	    echo about to do sccs admin
            echo sccs admin -n -i$file $file < $commentfile
            sccs admin -n -i$file $file < $commentfile >> $logfile 2>&1
            if [ $? != 0 ]; then
		    echo ERROR - sccs admin
		    exit
	    fi
            echo initial rev checked into SCCS
        else
	    case $rev in
	    *.*.*.*)
		brev=`echo $rev | sed -e 's/\.[0-9]*$//'`
		sccs admin -fb $file 2>>$logfile
		echo sccs get -e -p -r$brev $file
		sccs get -e -p -r$brev $file >/dev/null 2>>$logfile
		;;
	    *)
		echo sccs get -e -p $file
		sccs get -e -p $file >/dev/null 2>> $logfile
		;;
	    esac
	    if [ $? != 0 ]; then
		    echo ERROR - sccs get
		    exit
	    fi
	    sccs delta $file < $commentfile >> $logfile 2>&1
            if [ $? != 0 ]; then
		    echo ERROR - sccs delta -r$rev $file
		    exit
	    fi
            echo checked into SCCS
	fi
	sed -e "s;^d D $rev ../../.. ..:..:.. [^ ][^ ]*;d D $rev $date $author;" SCCS/s.$file > $tmpfile
	rm -f SCCS/s.$file
	cp $tmpfile SCCS/s.$file
	chmod 444 SCCS/s.$file
	sccs admin -z $file
        if [ $? != 0 ]; then
		echo ERROR - sccs admin -z
		exit
	fi
    done
    rm -f $file
done


############################################################
# Clean up
#
echo cleaning up...
rm -f $tmpfile $emptyfile $initialfile $sedfile $commentfile
echo ===================================================
echo "       Conversion Completed Successfully"
echo ===================================================

rm -f *,v
