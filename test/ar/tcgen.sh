#!/bin/sh
# $Id: tcgen.sh 2080 2011-10-27 04:23:24Z jkoshy $

# A script to generate test cases.

usage() {
    echo "Usage: tcgen.sh [-nsu] [-i path] [-o path] [-g gcmd] [-c rcmd] tcname"
    echo "Options:"
    echo "-n Generated test case do not use standard diff to compare"
    echo "   resulting files"
    echo "-s shar the output files. (This implies -u)"
    echo "-u uuencode the output files."
    echo "-i Specify the directory where input files locate."
    echo "   If not specified, I assume there are no input files."
    echo "-o Specify the directory where I should put the output files."
    echo "   If not specified, I will put output files on a subdirectory"
    echo "   of current working dir with the name 'tcname'."
    echo "-g Specify the cmd to execute when generating the test case."
    echo "   If omitted, I assume that it is the same as rcmd."
    echo "-c Specify the cmd to execute when running the test case."
    echo "tcname Specify the name of the test case."
}

# record the input/output state, i.e., record input/output files,
# encode and/or shar them if required.
# argument:
# 	$1 = in/out
recstate() {
    entries=`ls ${WORKDIR}`
    if [ X"${entries}" != X ]; then

	# uuencode if required.
	if [ "${USE_UUENCODE}" = yes ]; then
	    cp -R ${WORKDIR}/* ${WORKDIR}.uu.${1}
	    cd ${WORKDIR}.uu.${1} || exit 1
	    find . -type f | xargs -I % uuencode -o %.uu % %
	    find . -type f ! -name '*.uu' -delete
	fi

	# Pack them up using shar if required, or just copy.
	mkdir -p ${OPATH}/${1} || exit 1
	if [ "${USE_SHAR}" = yes ]; then
	    cd ${WORKDIR}.uu.${1} || exit 1
	    shar `find . -print` > ${OPATH}/${1}/$TC.${1}.shar
	elif [ "${USE_UUENCODE}" = yes ]; then
	    cp -R ${WORKDIR}.uu.${1}/* ${OPATH}/${1}
	else
	    cp -R ${WORKDIR}/* ${OPATH}/${1}
	fi
    fi
}

THISDIR=`/bin/pwd`

# Check the command line options.
#
while getopts "nsui:o:c:g:" COMMAND_LINE_ARGUMENT ; do
    case "${COMMAND_LINE_ARGUMENT}" in
	n)
	    NODIFFRLT=yes;
	    ;;
	s)
	    USE_SHAR=yes;
	    USE_UUENCODE=yes;
	    ;;
	u)
	    USE_UUENCODE=yes;
	    ;;
	i)
	    IPATH=${OPTARG}
	    ;;
	o)
	    OPATH=${OPTARG}
	    ;;
	g)
	    GCMD=${OPTARG}
	    ;;
	c)
	    RCMD=${OPTARG}
	    ;;
	*)
	    usage
	    exit 1
	    ;;
    esac
done

if [ $# -ne $OPTIND ]; then
    usage
    exit 1
fi
eval TC=$"{${OPTIND}}"

if [ -z "${OPATH}" ]; then
    OPATH=${TC};
fi
mkdir -p ${OPATH} || exit 1

if [ -z "${RCMD}" ]; then
    RCMD=":"
fi

if [ -z "${GCMD}" ]; then
    GCMD=${RCMD}
fi

# Convert to absolute pathnames.
#
if [ -n "${IPATH}" ]; then
    IPATH=`cd ${IPATH} 2>/dev/null && /bin/pwd \
	|| echo "can't locate ${IPATH}" && exit 1`
fi

ROPATH=${OPATH}			# backup relative opath for later use.
OPATH=`cd ${OPATH} 2>/dev/null && /bin/pwd \
    || echo "can't locate ${OPATH}" && exit 1`

# Prefix $GCMD with absolute pathnames.
#
executable=`echo ${GCMD} | cut -f 1 -d ' '`
relapath=`dirname ${executable}`
cd ${THISDIR}
absolpath=`cd ${relapath} && /bin/pwd`
GCMD=${absolpath}/`basename ${executable}`" "`echo ${GCMD} | cut -f 2- -d ' '`

# Set up temporary directories.
#
WORKDIR=/tmp/bsdar-tcgen-work
rm -rf ${WORKDIR}
rm -rf ${WORKDIR}.uu.in
rm -rf ${WORKDIR}.uu.out
mkdir -p ${WORKDIR} || exit 1
mkdir -p ${WORKDIR}.uu.in || exit 1 # Keep encoded input files
mkdir -p ${WORKDIR}.uu.out || exit 1 # Keep encoded output files

if [ -n "${IPATH}" ]; then
    cp -R ${IPATH}/* ${WORKDIR} 2>/dev/null
fi

# Keep a record of input state.
#
recstate "in"

# Execute the cmd, record stdout, stderr and exit value.
#
redirin=`echo ${GCMD} | cut -f 2- -d '<'`
if [ "${redirin}" != "${GCMD}" ]; then
    GCMD=`echo ${GCMD} | cut -f 1 -d '<'`
    redirin=`echo ${redirin} | sed 's/^ *\(.*\) *$/\1/'`
fi

cd ${WORKDIR} || exit 1
if [ "${redirin}" != "${GCMD}" ]; then
    ${GCMD} < ${redirin} > ${OPATH}/$TC.out 2> ${OPATH}/$TC.err
else
    ${GCMD} > ${OPATH}/$TC.out 2> ${OPATH}/$TC.err
fi
echo $? > ${OPATH}/$TC.eval

# Keep a record of output state.
#
recstate "out"

# Generate test script.
#
echo "inittest ${TC} ${ROPATH}" > ${OPATH}/${TC}.sh
if [ "${USE_SHAR}" = yes ]; then
    echo 'extshar ${TESTDIR}' >> ${OPATH}/${TC}.sh
    echo 'extshar ${RLTDIR}' >> ${OPATH}/${TC}.sh
elif [ "${USE_UUENCODE}" = yes ]; then
    echo 'udecode ${TESTDIR}' >> ${OPATH}/${TC}.sh
    echo 'udecode ${RLTDIR}' >> ${OPATH}/${TC}.sh
fi
echo "runcmd \"${RCMD}\" work true" >> ${OPATH}/${TC}.sh
if [ "${NODIFFRLT}" = yes ]; then
    echo "rundiff false" >> ${OPATH}/${TC}.sh
else
    echo "rundiff true" >> ${OPATH}/${TC}.sh
fi

cd ${THISDIR} || exit 1
echo "done."
