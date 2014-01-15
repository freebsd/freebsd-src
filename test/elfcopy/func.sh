# $Id: func.sh 2082 2011-10-27 04:38:32Z jkoshy $
#
# `init' initializes test engine global data.
#
init() {
    THISDIR=`/bin/pwd`
    TOPDIR=${THISDIR}/../..
    ELFCOPY=${TOPDIR}/elfcopy/elfcopy
    STRIP=${TOPDIR}/elfcopy/strip
    MCS=${TOPDIR}/elfcopy/mcs

    # keep a record of total tests and number of tests passed.
    TOTALCT=/tmp/elfcopy-test-total
    PASSEDCT=/tmp/elfcopy-test-passed
    echo 0 > ${TOTALCT}
    echo 0 > ${PASSEDCT}
}

# `inittest' initializes individual test process. (set up temp dirs,
# make copies of files used in the test if necessary, etc.)
#
inittest() {
    if [ $# -ne 2 ]; then
	echo "usage: inittest tcname tcdir"
	exit 1
    fi

    TC=$1
    TCDIR=$2
    TESTDIR=/tmp/${TC}
    OUTDIR=/tmp/${TC}-out
    RLTDIR=/tmp/${TC}-rlt
    rm -rf ${TESTDIR}
    rm -rf ${OUTDIR}
    rm -rf ${RLTDIR}
    mkdir -p ${TESTDIR} || exit 1
    mkdir -p ${OUTDIR} || exit 1
    mkdir -p ${RLTDIR} || exit 1

    if [ -d "${TCDIR}/in" ]; then
	cp -R ${TCDIR}/in/* ${TESTDIR} || exit 1
    fi

    if [ -d "${TCDIR}/out" ]; then
	cp -R ${TCDIR}/out/* ${RLTDIR} || exit 1
    fi
}

# `extshar' extracts shar file in the specific dir,
# then uudecode the resulting file(s).
#
extshar() {
    if [ $# -ne 1 ]; then
	echo "usage: extshar dir"
	exit 1
    fi

    cd $1 || exit 1
    for f in *.shar; do
	sh $f > /dev/null 2>&1 || exit 1
	rm -rf $f
    done

    udecode $1
}

# `udecode' calls uudecode to decode files encoded by
# uuencode in the specific dir.
#
udecode() {
    if [ $# -ne 1 ]; then
	echo "usage: uudecode dir"
	exit 1
    fi

    cd $1 || exit 1
    for f in *.uu; do
	uudecode $f || exit 1
	rm -rf $f
    done
}

# `runcmd' runs `cmd' on the work/result dir.
#
# cmd: command to execute
# loc: work/result
# rec: true (keep a record of the stdout and stderr)
#      false (do not record)
#
runcmd() {
    if [ $# -ne 3 ]; then
	echo "usage: dotest cmd loc rec"
	exit 1
    fi

    # prefix executable with abolute pathname.
    executable=`echo $1 | cut -f 1 -d ' '`
    relapath=`dirname ${executable}`
    cd ${THISDIR}
    absolpath=`cd ${relapath} && /bin/pwd`
    newcmd=${absolpath}/`basename ${executable}`" "`echo $1 | cut -f 2- -d ' '`

    if [ "$2" = work ]; then
	cd ${TESTDIR} || exit 1
    elif [ "$2" = result ]; then
	cd ${RLTDIR} || exit 1
    else
	echo "loc must be work or result."
	exit 1
    fi

    if [ "$3" = true ]; then
	${newcmd} > ${OUTDIR}/${TC}.out 2> ${OUTDIR}/${TC}.err
	echo $? > ${OUTDIR}/${TC}.eval
    elif [ "$3" = false ]; then
	${newcmd}
    else
	echo "rec must be true of false."
	exit 1
    fi

    cd ${THISDIR}
}

# `rundiff' performs standard diff to compare exit value,
# stdout output, stderr output and resulting files with
# "standard answers".
#
rundiff() {
    # $1 indicates whether we should compare resulting files.
    if [ $# -ne 1 ]; then
	echo "usage: rundiff [true|false]"
	exit 1
    fi
    cd ${THISDIR} || exit 1
    if [ -f ${TCDIR}/${TC}.eval ]; then
	incct ${TOTALCT}
	diff -urN ${TCDIR}/${TC}.eval ${OUTDIR}/${TC}.eval
	if [ $? -eq 0 ]; then
	    echo "${TC} exit value - ok"
	    incct ${PASSEDCT}
	else
	    echo "${TC} exit value - not ok"
	fi
    fi

    if [ -f ${TCDIR}/${TC}.out ]; then
	incct ${TOTALCT}
	diff -urN ${TCDIR}/${TC}.out ${OUTDIR}/${TC}.out
	if [ $? -eq 0 ]; then
	    echo "${TC} stdout - ok"
	    incct ${PASSEDCT}
	else
	    echo "${TC} stdout - not ok"
	fi
    fi

    if [ -f ${TCDIR}/${TC}.err ]; then
	incct ${TOTALCT}
	diff -urN ${TCDIR}/${TC}.err ${OUTDIR}/${TC}.err
	if [ $? -eq 0 ]; then
	    echo "${TC} stderr - ok"
	    incct ${PASSEDCT}
	else
	    echo "${TC} stderr - not ok"
	fi
    fi

    if [ "$1" = true ]; then
	incct ${TOTALCT}
	diff -urN ${RLTDIR} ${TESTDIR}
	if [ $? -eq 0 ]; then
	    echo "${TC} resulting files - ok"
	    incct ${PASSEDCT}
	else
	    echo "${TC} resulting files - not ok"
	fi
    fi
}

# `innct' increase specified counter by 1.
incct() {
    if [ $# -ne 1 ]; then
	echo "usage: incct counterfile"
	exit 1
    fi
    if [ -f $1 ]; then
	exec 3< $1
	read val <&3
	exec 3<&-
	newval=`expr ${val} + 1`
	echo ${newval} > $1
    else
	echo "$1 not exist"
	exit 1
    fi
}

# `statistic' shows number of test passed.
#
statistic() {
    exec 3< ${TOTALCT}
    read tval <&3
    exec 3<&-
    exec 3< ${PASSEDCT}
    read pval <&3
    exec 3<&-

    echo "${pval} out of ${tval} passed."
}
