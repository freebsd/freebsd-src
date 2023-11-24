#!/bin/sh -
#
#

# If there is a global system configuration file, suck it in.
#
if [ -r /etc/defaults/periodic.conf ]
then
    . /etc/defaults/periodic.conf
    source_periodic_confs
fi

oldmask=$(umask)
umask 066
case "$monthly_accounting_enable" in
    [Yy][Ee][Ss])
	W=/var/log/utx.log
	rc=0
	remove=NO
	filetoread=$W.0
	if [ ! -f $W.0 ]
	then
	    if [ -f $W.0.gz ] || [ -f $W.0.bz2 ] || [ -f $W.0.xz ] || [ -f $W.0.zst ]
	    then
	        TMP=`mktemp -t accounting`
		remove=YES
		filetoread=$TMP
		if [ -f $W.0.gz ]
		then
		    zcat $W.0.gz > $TMP || rc=1
		elif [ -f $W.0.bz2 ]
		then
		    bzcat $W.0.bz2 > $TMP || rc=1
		elif [ -f $W.0.xz ]
		then
		    xzcat $W.0.xz > $TMP || rc=1
		elif [ -f $W.0.zst ]
		then
		    zstdcat $W.0.zst > $TMP || rc=1
		else
		# shouldn't get here, unless something disappeared under us.
		    rc=2
		fi
	    else
		echo '$monthly_accounting_enable is set but' \
		    "$W.0 doesn't exist"
		rc=2
	    fi
	fi
	if [ $rc -eq 0 ]
	then
	    echo ""
	    echo "Doing login accounting:"

	    rc=$(ac -p -w $filetoread | sort -nr -k 2 | tee /dev/stderr | wc -l)
	    [ $rc -gt 0 ] && rc=1
	fi
	[ $remove = YES ] && rm -f $TMP;;

    *)  rc=0;;
esac

umask $oldmask
exit $rc
